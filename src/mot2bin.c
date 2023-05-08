/*
 * mot2bin converts a Motorola hex file to binary.
 *
 * Copyright (C) 2015,  Jacques Pelletier
 * checksum extensions Copyright (C) 2004 Rockwell Automation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 20040617 Alf Lacis: Added pad byte (may not always want FF).
 *          Added initialisation to checksum to remove GNU
 *          compiler warning about possible uninitialised usage
 *          Added 2x'break;' to remove GNU compiler warning about label at
 *          end of compound statement
 *          Added PROGRAM & VERSION strings.
 *
 * 20071005 PG: Improvements on options parsing
 * 20091212 JP: Corrected crash on 0 byte length data records
 * 20100402 JP: ADDRESS_MASK is now calculated from MEMORY_SIZE
 *
 * 20120125 Danny Schneider:
 *          Added code for filling a binary file to a given max_length relative to
 *          Starting address if Max-address is larger than Highest-address
 * 20120509 Yoshimasa Nakane:
 *          modified error checking (also for output file, JP)
 * 20141005 JP: added support for byte swapped hex files
 *          corrected bug caused by extra LF at end or within file
 * 20141121 Slucx: added line for removing extra CR when entering file name at run time.
 * 20150116 Richard Genoud (Paratronic): correct buffer overflows/wrong results with the -l flag
 * 20150122 JP: added support for different check methods
 * 20150221 JP: rewrite of the checksum write/force value
 * 20150804 JP: added batch file option
 */
#include <string.h>
#include "common.h"
#include "checksum.h"

#define PROGRAM "mot2bin"
#define VERSION "2.5"

const char *program_name = PROGRAM;

static void get_highest_and_lowest_addresses(char *line)
{
    uint32_t i;
    FILE *fileIn = NULL;
    uint16_t recordNb = 0;
    uint32_t nb_bytes = 0;
    int result;
    uint32_t temp;
    uint32_t type;
    uint32_t first_word;

    /* get highest and lowest addresses so that we can allocate the right size */
    do {
        /* Read a line from input file. */
        fileIn = GetInFile();
        GetLine(line, fileIn);
        recordNb++;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(line);

        if (--i != 0) {
            if (line[i] == '\n') {
                line[i] = '\0';
            }

            switch (line[1]) {
                case '0':
                    nb_bytes = 1; /* This is to fix the g_highest_address set to -1 when nb_bytes = 0 */
                    break;

                /* 16 bits address */
                case '1':
                    result = sscanf(line, "S%1x%2x%4x", &type, &nb_bytes, &first_word);
                    if (result != 3)
                        fprintf(fp, "Error in line %d of hex file\n", recordNb);

                    /* Adjust nb_bytes for the number of data bytes */
                    nb_bytes = nb_bytes - 3;
                    break;

                /* 24 bits address */
                case '2':
                    result = sscanf(line, "S%1x%2x%6x", &type, &nb_bytes, &first_word);
                    if (result != 3)
                        fprintf(fp, "Error in line %d of hex file\n", recordNb);

                    /* Adjust nb_bytes for the number of data bytes */
                    nb_bytes = nb_bytes - 4;
                    break;

                /* 32 bits address */
                case '3':
                    result = sscanf(line, "S%1x%2x%8x", &type, &nb_bytes, &first_word);
                    if (result != 3)
                        fprintf(fp, "Error in line %d of hex file\n", recordNb);

                    /* Adjust nb_bytes for the number of data bytes */
                    nb_bytes = nb_bytes - 5;
                    break;
            }

            g_phys_addr = first_word;

            /* Set the lowest address as base pointer. */
            if (g_phys_addr < g_lowest_address) {
                g_lowest_address = g_phys_addr;
            }

            /* Same for the top address. */
            temp = g_phys_addr + nb_bytes - 1;

            if (temp > g_highest_address) {
                g_highest_address = temp;
            }
        }
    } while (!feof(fileIn));
}

static void verify_checksum(uint32_t record_checksum, uint8_t cs, uint16_t record_nb)
{
    /* Verify checksum value. */
    if (((record_checksum + cs) != 0xFF) && GetEnableChecksumError()) {
        fprintf(fp, "checksum error in record %d: should be %02X\n", record_nb, 255 - cs);
        SetStatusChecksumError(true);
    }
}

static void read_file_process_lines(uint8_t *memory_block, char *line)
{
    int i;
    FILE *fileIn = NULL;
    uint16_t recordNb = 0;
    uint32_t nb_bytes = 0;
    int result;

    uint32_t exec_address;
    uint32_t record_count;
    uint32_t record_checksum;
    uint32_t type;
    uint32_t address;
    uint8_t checksum = 0;

    uint8_t data_str[MAX_LINE_SIZE];
    char *p;

    /* Read the file & process the lines. */
    do { /* repeat until EOF(fileIn) */
        /* Read a line from input file. */
        fileIn = GetInFile();
        GetLine(line, fileIn);
        recordNb++;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(line);

        if (--i == 0) {
            continue;
        }
        if (line[i] == '\n') {
            line[i] = '\0';
        }

        /* Scan starting address and nb of bytes. */
        /* Look at the record type after the 'S' */
        type = 0;
        p = (char *)data_str;

        switch (line[1]) {
            case '0':
                result = sscanf(line, "S0%2x0000484452%2x", &nb_bytes, &record_checksum);
                if (result != 2)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + 0x48 + 0x44 + 0x52;

                /* Adjust nb_bytes for the number of data bytes */
                nb_bytes = 0;
                break;
            /* 16 bits address */
            case '1':
                result = sscanf(line, "S%1x%2x%4x%s", &type, &nb_bytes, &address, p);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (address >> 8) + (address & 0xFF);

                /* Adjust nb_bytes for the number of data bytes */
                nb_bytes = nb_bytes - 3;
                break;
            /* 24 bits address */
            case '2':
                result = sscanf(line, "S%1x%2x%6x%s", &type, &nb_bytes, &address, p);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (address >> 16) + (address >> 8) + (address & 0xFF);

                /* Adjust nb_bytes for the number of data bytes */
                nb_bytes = nb_bytes - 4;
                break;
            /* 32 bits address */
            case '3':
                result = sscanf(line, "S%1x%2x%8x%s", &type, &nb_bytes, &address, p);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (address >> 24) + (address >> 16) + (address >> 8) + (address & 0xFF);

                /* Adjust nb_bytes for the number of data bytes */
                nb_bytes = nb_bytes - 5;
                break;
            case '5':
                result = sscanf(line, "S%1x%2x%4x%2x", &type, &nb_bytes, &record_count, &record_checksum);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (record_count >> 8) + (record_count & 0xFF);

                /* Adjust nb_bytes for the number of data bytes */
                nb_bytes = 0;
                break;
            case '7':
                result = sscanf(line, "S%1x%2x%8x%2x", &type, &nb_bytes, &exec_address, &record_checksum);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (exec_address >> 24) + (exec_address >> 16) + (exec_address >> 8) +
                    (exec_address & 0xFF);
                nb_bytes = 0;
                break;
            case '8':
                result = sscanf(line, "S%1x%2x%6x%2x", &type, &nb_bytes, &exec_address, &record_checksum);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (exec_address >> 16) + (exec_address >> 8) + (exec_address & 0xFF);
                nb_bytes = 0;
                break;
            case '9':
                result = sscanf(line, "S%1x%2x%4x%2x", &type, &nb_bytes, &exec_address, &record_checksum);
                if (result != 4)
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                checksum = nb_bytes + (exec_address >> 8) + (exec_address & 0xFF);
                nb_bytes = 0;
                break;
        }

        /* If we're reading the last record, ignore it. */
        switch (type) {
            /* Data record */
            case 1:
            case 2:
            case 3:
                if (nb_bytes == 0) {
                    fprintf(fp, "0 byte length Data record ignored\n");
                    break;
                }
                g_phys_addr = address;

                p = ReadDataBytes(p, memory_block, &checksum, recordNb, nb_bytes);

                /* Read the checksum value. */
                result = sscanf(p, "%2x", &record_checksum);
                if (result != 1) {
                    fprintf(fp, "Error in line %d of hex file\n", recordNb);
                }
                break;

            case 5:
                fprintf(fp, "Record total: %d\n", record_count);
                break;

            case 7:
                fprintf(fp, "Execution address (unused): %08X\n", exec_address);
                break;

            case 8:
                fprintf(fp, "Execution address (unused): %06X\n", exec_address);
                break;

            case 9:
                fprintf(fp, "Execution address (unused): %04X\n", exec_address);
                break;

            /* Ignore all other records */
            default:;
        }

        record_checksum &= 0xFF;

        /* Verify checksum value. */
        verify_checksum(record_checksum, checksum, recordNb);
    } while (!feof(fileIn));
}

int main(int argc, char *argv[])
{
    /* line inputted from file */
    char line[MAX_LINE_SIZE];

    char extension[MAX_EXTENSION_SIZE];
    char file_name[MAX_FILE_NAME_SIZE];
    uint32_t records_start;
    uint8_t *memory_block = NULL;

    fp = fopen("log.txt", "w");
    if (fp == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    fprintf(fp, "software name: %s version: %s build_time: %s, %s\n\n", PROGRAM, VERSION, __TIME__, __DATE__);

    if (argc == 1) {
        usage(__func__, __LINE__);
    }

    strcpy(extension, "bin"); /* default is for binary file extension */

    ParseOptions(argc, argv);

    /* when user enters input file name */
    /* Assume last parameter is filename */
    GetFilename(file_name, argv[argc - 1]);

    /* Just a normal file name */
    if (NoFailOpenInputFile(file_name) == false) {
        return 1;
    }
    PutExtension(file_name, extension);
    NoFailOpenOutputFile(file_name);

    /*
     * When the hex file is opened, the program will read it in 2 passes.
     * The first pass gets the highest and lowest addresses so that we can allocate the right size.
     * The second pass processes the hex data.
     *
     * To begin, assume the lowest address is at the end of the memory.
     * While reading each records, subsequent addresses will lower this number.
     * At the end of the input file, this value will be the lowest address.
     * A similar assumption is made for highest address. It starts at the
     * beginning of memory. While reading each records, subsequent addresses will raise this number.
     * At the end of the input file, this value will be the highest address.
     */
    g_lowest_address = (uint32_t)-1;
    g_highest_address = 0;

    get_highest_and_lowest_addresses(line);
    records_start = g_lowest_address;
    Allocate_Memory_And_Rewind(&memory_block);
    read_file_process_lines(memory_block, line);

    fprintf(fp, "Binary file start = 0x%08X\n", g_lowest_address);
    fprintf(fp, "Records start     = 0x%08X\n", records_start);
    fprintf(fp, "Highest address   = 0x%08X\n", g_highest_address);
    fprintf(fp, "Pad Byte          = 0x%X\n\n", GetPadByte());

    WriteMemory(memory_block);
    WriteOutFile(&memory_block);

#ifdef USE_FILE_BUFFERS
    free(FilinBuf);
    free(FiloutBuf);
#endif

    NoFailCloseInputFile(NULL);
    NoFailCloseOutputFile(NULL);
    fclose(fp);

    if (GetStatusChecksumError() && GetEnableChecksumError()) {
        fprintf(fp, "checksum error detected.\n");
        return 1;
    }

    return 0;
}
