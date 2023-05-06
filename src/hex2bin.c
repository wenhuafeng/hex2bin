/*
  hex2bin converts an Intel hex file to binary.

  Copyright (C) 2015,  Jacques Pelletier
  checksum extensions Copyright (C) 2004 Rockwell Automation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
  Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  20040617 Alf Lacis: Added pad byte (may not always want FF).
  Added 'break;' to remove GNU compiler warning about label at
  end of compound statement
  Added PROGRAM & VERSION strings.

  20071005 PG: Improvements on options parsing
  20091212 JP: Corrected crash on 0 byte length data records
  20100402 JP: Corrected bug on physical address calculation for extended
  linear address record.
  ADDRESS_MASK is now calculated from MEMORY_SIZE

  20120125 Danny Schneider:
  Added code for filling a binary file to a given max_length relative to
  Starting address if Max-address is larger than Highest-address
  20120509 Yoshimasa Nakane:
  modified error checking (also for output file, JP)
  20141005 JP: added support for byte swapped hex files
           corrected bug caused by extra LF at end or within file
  20141008 JP: removed junk code
  20141121 Slucx: added line for removing extra CR when entering file name at run time.
  20141122 Simone Fratini: small feature added
  20150116 Richard Genoud (Paratronic): correct buffer overflows/wrong results with the -l flag
  20150122 JP: added support for different check methods
  20150221 JP: rewrite of the checksum write/force value
  20150804 JP: added batch file option
  20160923 JP: added code for checking filename length
  20170418 Simone Fratini: added option -t and -T to obtain shorter binary files
*/
#include <string.h>
#include "common.h"
#include "checksum.h"

#define PROGRAM "hex2bin"
#define VERSION "3.0"

#define NO_ADDRESS_TYPE_SELECTED 0
#define LINEAR_ADDRESS 1
#define SEGMENTED_ADDRESS 2

const char *Pgm_Name = PROGRAM;
unsigned int Seg_Lin_Select = NO_ADDRESS_TYPE_SELECTED;

static void address_zero(unsigned int nb_bytes, unsigned int first_Word, unsigned int segment, unsigned int upper_address)
{
    unsigned int address;
    unsigned int temp;

    if (nb_bytes == 0) {
        return;
    }

    address = first_Word;

    if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
        Phys_Addr = (segment << 4) + address;
    } else {
        /* LINEAR_ADDRESS or NO_ADDRESS_TYPE_SELECTED
            upper_address = 0 as specified in the Intel spec. until an extended address
            record is read. */
        Phys_Addr = ((upper_address << 16) + address);
    }

    if (Verbose_Flag) {
        fprintf(fp, "Physical address: %08X\n", Phys_Addr);
    }

    /* Floor address */
    if (floor_address() == false) {
        return;
    }

    /* Set the lowest address as base pointer. */
    if (Phys_Addr < Lowest_Address) {
        Lowest_Address = Phys_Addr;
    }

    /* Same for the top address. */
    temp = Phys_Addr + nb_bytes - 1;

    /* Ceiling address */
    if (ceiling_address(temp) == false) {
        return;
    }
    if (temp > Highest_Address) {
        Highest_Address = temp;
    }
    if (Verbose_Flag) {
        fprintf(fp, "Highest_Address: %08X\n", Highest_Address);
    }
}

static void address_two(char *p, unsigned int *segment, uint16_t record_nb)
{
    int result;
    unsigned int temp2;

    /* First_Word contains the offset. It's supposed to be 0000 so
        we ignore it. */

    /* First extended segment address record ? */
    if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED) {
        Seg_Lin_Select = SEGMENTED_ADDRESS;
    }

    /* Then ignore subsequent extended linear address records */
    if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
        result = sscanf(p, "%4x%2x", segment, &temp2);
        if (result != 2) {
            fprintf(fp, "Error in line %d of hex file\n", record_nb);
        }

        if (Verbose_Flag) {
            fprintf(fp, "Extended Segment address record: %04X\n", *segment);
        }

        /* Update the current address. */
        Phys_Addr = (*segment << 4);
    } else {
        fprintf(fp, "Ignored extended linear address record %d\n", record_nb);
    }
}

static void address_four(char *p, unsigned int *upper_address, uint16_t record_nb)
{
    int result;
    unsigned int temp2;

    /* First_Word contains the offset. It's supposed to be 0000 sowe ignore it. */
    /* First extended linear address record ? */
    if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED) {
        Seg_Lin_Select = LINEAR_ADDRESS;
    }

    /* Then ignore subsequent extended segment address records */
    if (Seg_Lin_Select == LINEAR_ADDRESS) {
        result = sscanf(p, "%4x%2x", upper_address, &temp2);
        if (result != 2) {
            fprintf(fp, "Error in line %d of hex file\n", record_nb);
        }
        if (Verbose_Flag) {
            fprintf(fp, "Extended Linear address record: %04X\n", *upper_address);
        }

        /* Update the current address. */
        Phys_Addr = (*upper_address << 16);

        if (Verbose_Flag) {
            fprintf(fp, "Physical address: %08X\n", Phys_Addr);
        }
    } else {
        fprintf(fp, "Ignored extended segment address record %d\n", record_nb);
    }
}

static void get_highest_and_lowest_addresses(char *line)
{
    unsigned int i;
    FILE *fileIn = NULL;
    int result;
    unsigned int First_Word;
    unsigned int Type;
    uint8_t Data_Str[MAX_LINE_SIZE];
    char *p;

    unsigned int Segment = 0x00;
    unsigned int Upper_Address = 0x00;
    uint16_t Record_Nb = 0;
    unsigned int Nb_Bytes = 0;

    /* get highest and lowest addresses so that we can allocate the rintervallo incoerenteight size */
    do {
        /* Read a line from input file. */
        fileIn = GetInFile();
        GetLine(line, fileIn);
        Record_Nb++;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(line);

        if (--i == 0) {
            continue;
        }

        if (line[i] == '\n') {
            line[i] = '\0';
        }

        /* Scan the first two bytes and nb of bytes.
            The two bytes are read in First_Word since its use depend on the
            record type: if it's an extended address record or a data record.
            */
        result = sscanf(line, ":%2x%4x%2x%s", &Nb_Bytes, &First_Word, &Type, Data_Str);
        if (result != 4) {
            fprintf(fp, "Error in line %d of hex file\n", Record_Nb);
        }

        p = (char *)Data_Str;

        /* If we're reading the last record, ignore it. */
        switch (Type) {
            /* Data record */
            case 0:
                address_zero(Nb_Bytes, First_Word, Segment, Upper_Address);
                break;
            case 1:
                if (Verbose_Flag) {
                    fprintf(fp, "End of File record\n");
                }
                break;
            case 2:
                address_two(p, &Segment, Record_Nb);
                break;
            case 3:
                if (Verbose_Flag) {
                    fprintf(fp, "Start Segment address record: ignored\n");
                }
                break;
            case 4:
                address_four(p, &Upper_Address, Record_Nb);
                break;
            case 5:
                if (Verbose_Flag) {
                    fprintf(fp, "Start Linear address record: ignored\n");
                }
                break;
            default:
                if (Verbose_Flag) {
                    fprintf(fp, "Unknown record type: %d at %d\n", Type, Record_Nb);
                }
                break;
        }
    } while (!feof(fileIn));
}

static void VerifyChecksumValue(uint8_t cs, uint16_t record_nb)
{
    if ((cs != 0) && GetEnableChecksumError()) {
        fprintf(fp, "checksum error in record %d: should be %02X\n", record_nb, (256 - cs) & 0xFF);
        SetStatusChecksumError(true);
    }
}

static void lines_zero(char *p, uint8_t *memory_block, uint8_t *cs, unsigned int first_Word, unsigned int nb_bytes,
    unsigned int upper_address, unsigned int segment, unsigned int offset, uint16_t record_nb)
{
    int result;
    unsigned int address;
    unsigned int temp2;

    if (nb_bytes == 0) {
        fprintf(fp, "0 byte length Data record ignored\n");
        return;
    }

    address = first_Word;

    if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
        Phys_Addr = (segment << 4) + address;
    } else {
        /* LINEAR_ADDRESS or NO_ADDRESS_TYPE_SELECTED
            Upper_Address = 0 as specified in the Intel spec. until an extended address
            record is read. */
        if (GetAddressAlignmentWord()) {
            Phys_Addr = ((upper_address << 16) + (address << 1)) + offset;
        } else {
            Phys_Addr = ((upper_address << 16) + address);
        }
    }

    /* Check that the physical address stays in the buffer's range. */
    if ((Phys_Addr >= Lowest_Address) && (Phys_Addr <= Highest_Address)) {
        /* The memory block begins at Lowest_Address */
        Phys_Addr -= Lowest_Address;

        p = ReadDataBytes(p, memory_block, cs, record_nb, nb_bytes);

        /* Read the checksum value. */
        result = sscanf(p, "%2x", &temp2);
        if (result != 1) {
            fprintf(fp, "Error in line %d of hex file\n", record_nb);
        }

        /* Verify checksum value. */
        *cs = (*cs + temp2) & 0xFF;
        VerifyChecksumValue(*cs, record_nb);
    } else {
        if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
            fprintf(fp, "Data record skipped at %4X:%4X\n", segment, address);
        } else {
            fprintf(fp, "Data record skipped at %8X\n", Phys_Addr);
        }
    }
}

static void lines_two(char *p, unsigned int *segment, uint8_t *cs, uint16_t record_nb)
{
    int result;
    unsigned int temp2;

    /* First_Word contains the offset. It's supposed to be 0000 so we ignore it. */
    /* First extended segment address record ? */
    if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED) {
        Seg_Lin_Select = SEGMENTED_ADDRESS;
    }

    /* Then ignore subsequent extended linear address records */
    if (Seg_Lin_Select == SEGMENTED_ADDRESS) {
        result = sscanf(p, "%4x%2x", segment, &temp2);
        if (result != 2) {
            fprintf(fp, "Error in line %d of hex file\n", record_nb);
        }

        /* Update the current address. */
        Phys_Addr = (*segment << 4);

        /* Verify checksum value. */
        *cs = (*cs + (*segment >> 8) + (*segment & 0xFF) + temp2) & 0xFF;
        VerifyChecksumValue(*cs, record_nb);
    }
}

static void lines_four(char *p, uint8_t *cs, unsigned int *upper_address, unsigned int *offset, uint16_t record_nb)
{
    int result;
    unsigned int temp2;

    /* First_Word contains the offset. It's supposed to be 0000 so we ignore it. */
    if (GetAddressAlignmentWord()) {
        sscanf(p, "%4x", offset);
        *offset = *offset << 16;
        *offset -= Lowest_Address;
    }
    /* First extended linear address record ? */
    if (Seg_Lin_Select == NO_ADDRESS_TYPE_SELECTED)
        Seg_Lin_Select = LINEAR_ADDRESS;

    /* Then ignore subsequent extended segment address records */
    if (Seg_Lin_Select == LINEAR_ADDRESS) {
        result = sscanf(p, "%4x%2x", upper_address, &temp2);
        if (result != 2) {
            fprintf(fp, "Error in line %d of hex file\n", record_nb);
        }

        /* Update the current address. */
        Phys_Addr = (*upper_address << 16);

        /* Verify checksum value. */
        *cs = (*cs + (*upper_address >> 8) + (*upper_address & 0xFF) + temp2) & 0xFF;
        VerifyChecksumValue(*cs, record_nb);
    }
}

static void read_file_process_lines(uint8_t *memory_block, char *line)
{
    unsigned int i;
    FILE *fileIn = NULL;
    int result;
    unsigned int First_Word;
    unsigned int Type;
    uint8_t Data_Str[MAX_LINE_SIZE];
    char *p;

    unsigned int Segment = 0x00;
    unsigned int Upper_Address = 0x00;

    unsigned int Offset = 0x00;
    uint8_t Checksum = 0;
    uint16_t Record_Nb = 0;
    unsigned int Nb_Bytes = 0;

    /* Read the file & process the lines. */
    do { /* repeat until EOF(fileIn) */
        /* Read a line from input file. */
        fileIn = GetInFile();
        GetLine(line, fileIn);
        Record_Nb++;

        /* Remove carriage return/line feed at the end of line. */
        i = strlen(line);

        // fprintf(fp,"Record: %d; length: %d\n", Record_Nb, i);

        if (--i == 0) {
            continue;
        }
        if (line[i] == '\n') {
            line[i] = '\0';
        }

        /* Scan the first two bytes and nb of bytes.
            The two bytes are read in First_Word since its use depend on the
            record type: if it's an extended address record or a data record.
        */
        result = sscanf(line, ":%2x%4x%2x%s", &Nb_Bytes, &First_Word, &Type, Data_Str);
        if (result != 4) {
            fprintf(fp, "Error in line %d of hex file\n", Record_Nb);
        }

        Checksum = Nb_Bytes + (First_Word >> 8) + (First_Word & 0xFF) + Type;

        p = (char *)Data_Str;

        /* If we're reading the last record, ignore it. */
        switch (Type) {
            /* Data record */
            case 0:
                lines_zero(p, memory_block, &Checksum, First_Word, Nb_Bytes, Upper_Address, Segment, Offset, Record_Nb);
                break;
            /* End of file record */
            case 1:
                /* Simply ignore checksum errors in this line. */
                break;
            /* Extended segment address record */
            case 2:
                lines_two(p, &Segment, &Checksum, Record_Nb);
                break;
            /* Start segment address record */
            case 3:
                /* Nothing to be done since it's for specifying the starting address for
                    execution of the binary code */
                break;
            /* Extended linear address record */
            case 4:
                lines_four(p, &Checksum, &Upper_Address, &Offset, Record_Nb);
                break;
            /* Start linear address record */
            case 5:
                /* Nothing to be done since it's for specifying the starting address for
                    execution of the binary code */
                break;
            default:
                fprintf(fp, "Unknown record type\n");
                break;
        }
    } while (!feof(fileIn));
}

int main(int argc, char *argv[])
{
    /* line inputted from file */
    char Line[MAX_LINE_SIZE];

    char Extension[MAX_EXTENSION_SIZE];
    char Filename[MAX_FILE_NAME_SIZE];
    unsigned int Records_Start;
    uint8_t *Memory_Block = NULL;

    fp = fopen("log.txt", "w"); // 打开文件，以追加模式写入
    if (fp == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    fprintf(fp, "software name: %s version: %s build_time: %s, %s\n\n", PROGRAM, VERSION, __TIME__, __DATE__);

    if (argc == 1) {
        usage(__func__, __LINE__);
    }

    strcpy(Extension, "bin"); /* default is for binary file extension */

    ParseOptions(argc, argv);

    /* when user enters input file name */

    /* Assume last parameter is filename */
    GetFilename(Filename, argv[argc - 1]);

    /* Just a normal file name */
    if (NoFailOpenInputFile(Filename) == false) {
        return 1;
    }
    PutExtension(Filename, Extension);
    NoFailOpenOutputFile(Filename);
    //Fileread = true;

    /* When the hex file is opened, the program will read it in 2 passes.
    The first pass gets the highest and lowest addresses so that we can allocate
    the right size.
    The second pass processes the hex data. */

    /* To begin, assume the lowest address is at the end of the memory.
     While reading each records, subsequent addresses will lower this number.
     At the end of the input file, this value will be the lowest address.

     A similar assumption is made for highest address. It starts at the
     beginning of memory. While reading each records, subsequent addresses will raise this number.
     At the end of the input file, this value will be the highest address. */
    Lowest_Address = (unsigned int)-1;
    Highest_Address = 0;

    /* Check if are set Floor and Ceiling address and range is coherent */
    VerifyRangeFloorCeil();

    get_highest_and_lowest_addresses(Line);

    if (GetAddressAlignmentWord()) {
        Highest_Address += (Highest_Address - Lowest_Address) + 1;
    }

    Records_Start = Lowest_Address;
    Allocate_Memory_And_Rewind(&Memory_Block);
    read_file_process_lines(Memory_Block, Line);

    fprintf(fp, "Binary file start = 0x%08X\n", Lowest_Address);
    fprintf(fp, "Records start     = 0x%08X\n", Records_Start);
    fprintf(fp, "Highest address   = 0x%08X\n", Highest_Address);
    fprintf(fp, "Pad Byte          = 0x%X\n\n", GetPadByte());

    WriteMemory(Memory_Block);
    WriteOutFile(&Memory_Block);

#ifdef USE_FILE_BUFFERS
    free(FilinBuf);
    free(FiloutBuf);
#endif

    NoFailCloseInputFile(NULL);
    NoFailCloseOutputFile(NULL);
    fclose(fp); // 关闭文件

    if (GetStatusChecksumError() && GetEnableChecksumError()) {
        fprintf(fp, "checksum error detected.\n");
        return 1;
    }

    return 0;
}
