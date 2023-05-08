/*
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

  20151124 Donna Whisnant: Bug fix for range checking of WriteMemory()
           and Allocate_Memory_And_Rewind() report of addresses in hexadecimal
  20160930 JP: corrected the wrong error report "Force/Check"
  20170304 JP: added the 16-bit checksum 8-bit wide
*/

#include "common.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "binary.h"
#include "libcrc.h"
#include "checksum.h"

/* We use buffer to speed disk access. */
#ifdef USE_FILE_BUFFERS
#define BUFFSZ 4096
#endif

/* option character */
#if defined(MSDOS) || defined(__DOS__) || defined(__MSDOS__) || defined(_MSDOS)
#define _IS_OPTION_(x) (((x) == '-') || ((x) == '/'))
#else
/* Assume unix and similar */
/* We don't accept an option beginning with a '/' because it could be a file name. */
#define _IS_OPTION_(x) ((x) == '-')
#endif

// static char extension[MAX_EXTENSION_SIZE]; /* filename extension for output files */

static FILE *file_in;  /* input files */
static FILE *file_out; /* output files */

#ifdef USE_FILE_BUFFERS
char *FilinBuf;  /* text buffer for file input */
char *FiloutBuf; /* text buffer for file output */
#endif

static int pad_byte = 0xFF;

static uint32_t starting_address;
static uint32_t max_length = 0;
static uint32_t minimum_block_size = 0x1000; // 4096 byte
static uint32_t floor_address = 0x00;
static uint32_t ceiling_address = 0xFFFFFFFF;
static bool minimum_block_size_setted = false;
static bool starting_address_setted = false;
static bool floor_address_setted = false;
static bool ceiling_address_setted = false;
static bool max_length_setted = false;
static bool swap_wordwise = false;
static bool address_alignment_word = false;
static bool batch_mode = false;

static bool enable_checksum_error = false;
static bool status_checksum_error = false;

bool verbose_flag = false;

/* This will hold binary codes translated from hex file. */
uint32_t g_lowest_address;
uint32_t g_highest_address;
uint32_t g_phys_addr;
FILE *fp = NULL;

/* procedure USAGE */
void usage(const char *func, uint32_t line)
{
    fprintf(fp,
        "\n"
        "usage: %s [OPTIONS] filename\n"
        "func: %s\n"
        "line: %d\n"
        "Options:\n"
        "  -a            address Alignment Word (hex2bin only)\n"
        "  -b            Batch mode: exits if specified file doesn't exist\n"
        "  -c            Enable record checksum verification\n"
        "  -C [Poly][Init][RefIn][RefOut][XorOut]\n                CRC parameters\n"
        "  -e [ext]      Output filename extension (without the dot)\n"
        "  -E [0|1]      Endian for checksum/CRC, 0: little, 1: big\n"
        "  -f [address]  address of check result to write\n"
        "  -F [address] [value]\n                address and value to force\n"
        "  -k [0-6]      Select check method (checksum or CRC) and size\n"
        "  -d            display list of check methods/value size\n"
        "  -l [length]   Maximal Length (Starting address + Length -1 is Max address)\n"
        "                File will be filled with Pattern until Max address is reached\n"
        "  -m [size]     Minimum Block Size\n"
        "                File Size Dimension will be a multiple of Minimum block size\n"
        "                File will be filled with Pattern\n"
        "                Length must be a power of 2 in hexadecimal [see -l option]\n"
        "                Attention this option is STRONGER than Maximal Length  \n"
        "  -p [value]    Pad-byte value in hex (default: %x)\n"
        "  -r [start] [end]\n"
        "                Range to compute checksum over (default is min and max addresses)\n"
        "  -s [address]  Starting address in hex for binary file (default: 0)\n"
        "                ex.: if the first record is :nn010000ddddd...\n"
        "                the data supposed to be stored at 0100 will start at 0000\n"
        "                in the binary file.\n"
        "                Specifying this starting address will put pad bytes in the\n"
        "                binary file so that the data supposed to be stored at 0100\n"
        "                will start at the same address in the binary file.\n"
        "  -t [address]  Floor address in hex (hex2bin only)\n"
        "  -T [address]  Ceiling address in hex (hex2bin only)\n"
        "  -v            Verbose messages for debugging purposes\n"
        "  -w            Swap wordwise (low <-> high)\n\n",
        program_name, func, line, pad_byte);
    exit(1);
}

static void DisplayCheckMethods(void)
{
    fprintf(fp, "Check methods/value size:\n"
        "0:  checksum  8-bit\n"
        "1:  checksum 16-bit (adds 16-bit words into a 16-bit sum, data and result BE or LE)\n"
        "2:  CRC8\n"
        "3:  CRC16\n"
        "4:  CRC32\n"
        "5:  checksum 16-bit (adds bytes into a 16-bit sum, result BE or LE)\n");
    exit(1);
}

/* Open the input file, with error checking */
bool NoFailOpenInputFile(char *file_name)
{
    file_in = fopen(file_name, "r");
    if (file_in == NULL) {
        if (batch_mode) {
            fprintf(fp, "Input file %s cannot be opened.\n", file_name);
            exit(1);
        } else {
            fprintf(fp, "Input file %s cannot be opened. Enter new filename: ", file_name);
            if (file_name[strlen(file_name) - 1] == '\n') {
                file_name[strlen(file_name) - 1] = '\0';
            }
        }
        return false;
    }

#ifdef USE_FILE_BUFFERS
    FilinBuf = (char *)NoFailMalloc(BUFFSZ);
    setvbuf(file_in, FilinBuf, _IOFBF, BUFFSZ);
#endif

    return true;
}

void NoFailCloseInputFile(char *file_name)
{
    fclose(file_in);
}

/* Open the output file, with error checking */
void NoFailOpenOutputFile(char *file_name)
{
    while ((file_out = fopen(file_name, "wb")) == NULL) {
        if (batch_mode) {
            fprintf(fp, "Output file %s cannot be opened.\n", file_name);
            exit(1);
        } else {
            /* Failure to open the output file may be
             simply due to an insufficient permission setting. */
            fprintf(fp, "Output file %s cannot be opened. Enter new file name: ", file_name);
            if (file_name[strlen(file_name) - 1] == '\n') {
                file_name[strlen(file_name) - 1] = '\0';
            }
        }
    }

#ifdef USE_FILE_BUFFERS
    FiloutBuf = (char *)NoFailMalloc(BUFFSZ);
    setvbuf(file_out, FiloutBuf, _IOFBF, BUFFSZ);
#endif
} /* procedure OPENFILOUT */

void NoFailCloseOutputFile(char *file_name)
{
    //fclose(fileOut);
}

void GetLine(char *str, FILE *in)
{
    char *result;

    result = fgets(str, MAX_LINE_SIZE, in);
    if ((result == NULL) && !feof(in)) {
        fprintf(fp, "Error occurred while reading from file\n");
    }
}

#if 0
static int GetDec(const char *str)
{
    int result;
    uint32_t value;

    result = sscanf(str, "%u", &value);

    if (result == 1) {
        return value;
    } else {
        fprintf(fp, "GetDec: some error occurred when parsing options.\n");
        exit(1);
    }
}
#endif

void GetFilename(char *dest, char *src)
{
    if (strlen(src) < MAX_FILE_NAME_SIZE) {
        strcpy(dest, src);
    } else {
        fprintf(fp, "filename length exceeds %d characters.\n", MAX_FILE_NAME_SIZE);
        exit(1);
    }
}

static void GetExtension(const char *str, char *ext)
{
    if (strlen(str) > MAX_EXTENSION_SIZE) {
        usage(__func__, __LINE__);
    }

    strcpy(ext, str);
}

/* Adds an extension to a file name */
void PutExtension(char *file_name, char *extension)
{
    char *period; /* location of period in file name */

    /* This assumes DOS like file names */
    /* Don't use strchr(): consider the following filename:
     ../my.dir/file.hex
    */
    if ((period = strrchr(file_name, '.')) != NULL) {
        *(period) = '\0';
        if (strcmp(extension, period + 1) == 0) {
            fprintf(fp, "Input and output filenames (%s) are the same.\n", file_name);
            exit(1);
        }
    }
    strcat(file_name, ".");
    strcat(file_name, extension);
}

/* Check if are set Floor and Ceiling address and range is coherent */
void VerifyRangeFloorCeil(void)
{
    if (floor_address_setted && ceiling_address_setted && (floor_address >= ceiling_address)) {
        fprintf(fp, "Floor address %08X higher than Ceiling address %08X\n", floor_address, ceiling_address);
        exit(1);
    }
}

void Allocate_Memory_And_Rewind(uint8_t **memory_block)
{
    if (starting_address_setted == true) {
        g_lowest_address = starting_address;
    } else {
        starting_address = g_lowest_address;
    }

    if (max_length_setted == false) {
        max_length = g_highest_address - g_lowest_address + 1;
    } else {
        g_highest_address = g_lowest_address + max_length - 1;
    }

    fprintf(fp, "Allocate_Memory_and_Rewind:\n");
    fprintf(fp, "Lowest address:   = 0x%08X\n", g_lowest_address);
    fprintf(fp, "Highest address:  = 0x%08X\n", g_highest_address);
    fprintf(fp, "Starting address: = 0x%08X\n", starting_address);
    fprintf(fp, "Max Length:       = 0x%u\n\n", max_length);

    /* Now that we know the buffer size, we can allocate it. */
    /* allocate a buffer */
    *memory_block = (uint8_t *)NoFailMalloc(max_length);

    /* For EPROM or FLASH memory types, fill unused bytes with FF or the value specified by the p option */
    memset(*memory_block, pad_byte, max_length);

    rewind(file_in);
}

char *ReadDataBytes(char *p, uint8_t *memory_block, uint8_t *cs, uint16_t record_nb, uint32_t nb_bytes)
{
    uint32_t i, temp2;
    int result;

    /* Read the Data bytes. */
    /* Bytes are written in the Memory block even if checksum is wrong. */
    i = nb_bytes;

    do {
        result = sscanf(p, "%2x", &temp2);
        if (result != 1) {
            fprintf(fp, "ReadDataBytes: error in line %d of hex file\n", record_nb);
        }
        p += 2;

        /* Check that the physical address stays in the buffer's range. */
        if (g_phys_addr < max_length) {
            /* Overlapping record will erase the pad bytes */
            if (swap_wordwise) {
                if (memory_block[g_phys_addr ^ 1] != pad_byte)
                    fprintf(fp, "Overlapped record detected\n");
                memory_block[g_phys_addr++ ^ 1] = temp2;
            } else {
                if (memory_block[g_phys_addr] != pad_byte)
                    fprintf(fp, "Overlapped record detected\n");
                memory_block[g_phys_addr++] = temp2;
            }

            *cs = (*cs + temp2) & 0xFF;
        }
    } while (--i != 0);

    return p;
}

void WriteOutFile(uint8_t **memory_block)
{
    int module;
    uint8_t *memory_block_new = NULL;

    /* write binary file */
    fwrite(*memory_block, max_length, 1, file_out);
    free(*memory_block);

    // minimum_block_size is set; the memory buffer is multiple of this?
    if (minimum_block_size_setted == false) {
        return;
    }

    module = max_length % minimum_block_size;
    if (module) {
        module = minimum_block_size - module;
        memory_block_new = (uint8_t *)NoFailMalloc(module);
        memset(memory_block_new, pad_byte, module);
        fwrite(memory_block_new, module, 1, file_out);
        free(memory_block_new);
        if (max_length_setted == true) {
            fprintf(fp, "Attention Max Length changed by Minimum Block Size\n");
        }
        // extended
        max_length += module;
        g_highest_address += module;
        fprintf(fp, "Extended\nHighest address: %08X\n", g_highest_address);
        fprintf(fp, "Max Length: %u\n\n", max_length);
    }
}

/*
 * Parse options on the command line
 * variables:
 * use p for parsing arguments
 * use i for number of parameters to skip
 * use c for the current option
 */
void ParseOptions(int argc, char *argv[])
{
    int param;
    char *p;

    starting_address = 0;

    for (param = 1; param < argc; param++) {
        int i = 0;
        char c;

        p = argv[param];
        c = *(p + 1); /* Get option character */

        if (_IS_OPTION_(*p)) {
            // test for no space between option and parameter
            if (strlen(p) != 2) {
                usage(__func__, __LINE__);
            }

            switch (c) {
                case 'a':
                    address_alignment_word = true;
                    i = 0;
                    break;
                case 'b':
                    batch_mode = true;
                    i = 0;
                    break;
                case 'c':
                    enable_checksum_error = true;
                    i = 0;
                    break;
                case 'd':
                    DisplayCheckMethods();
                case 'e':
                    // GetExtension(argv[param + 1], extension);
                    i = 1; /* add 1 to param */
                    break;
                case 'E':
                    Para_E(argv[param + 1]);
                    i = 1; /* add 1 to param */
                    break;
                case 'f':
                    Para_f(argv[param + 1]);
                    i = 1; /* add 1 to param */
                    break;
                case 'F':
                    Para_F(argv[param + 1], argv[param + 2]);
                    i = 2; /* add 2 to param */
                    break;
                case 'k':
                    Para_k(argv[param + 1]);
                    i = 1; /* add 1 to param */
                    break;
                case 'l':
                    max_length = GetHex(argv[param + 1]);
                    if (max_length > 0x800000) {
                        fprintf(fp, "max_length = %u\n", max_length);
                        exit(1);
                    }
                    max_length_setted = true;
                    i = 1; /* add 1 to param */
                    break;
                case 'm':
                    minimum_block_size = GetHex(argv[param + 1]);
                    minimum_block_size_setted = true;
                    i = 1; /* add 1 to param */
                    break;
                case 'p':
                    pad_byte = GetHex(argv[param + 1]);
                    i = 1; /* add 1 to param */
                    break;
                case 'r':
                    Para_r(argv[param + 1], argv[param + 2]);
                    i = 2; /* add 2 to param */
                    break;
                case 's':
                    starting_address = GetHex(argv[param + 1]);
                    starting_address_setted = true;
                    i = 1; /* add 1 to param */
                    break;
                case 'v':
                    verbose_flag = true;
                    i = 0;
                    break;
                case 't':
                    floor_address = GetHex(argv[param + 1]);
                    floor_address_setted = true;
                    i = 1; /* add 1 to param */
                    break;
                case 'T':
                    ceiling_address = GetHex(argv[param + 1]);
                    ceiling_address_setted = true;
                    i = 1; /* add 1 to param */
                    break;
                case 'w':
                    swap_wordwise = true;
                    i = 0;
                    break;
                case 'C':
                    Para_C(argv[param + 1], argv[param + 2], argv[param + 3], argv[param + 4], argv[param + 5]);
                    i = 5; /* add 5 to param */
                    break;

                case '?':
                case 'h':
                default:
                    usage(__func__, __LINE__);
                    break;
            }

            /* Last parameter is not a filename */
            if (param == argc - 1) {
                usage(__func__, __LINE__);
            }

            // fprintf(fp,"param: %d, option: %c\n", param, c);

            /* if (param + i) < (argc -1) */
            if (param < argc - 1 - i) {
                param += i;
            } else {
                // fprintf(fp,"param: %d, argc: %d, i: %d\n", param, argc, i);
                usage(__func__, __LINE__);
            }
        } else {
            break;
        }
        /* if option */
    } /* for param */
}

FILE *GetInFile(void)
{
    return file_in;
}

bool GetAddressAlignmentWord(void)
{
    return address_alignment_word;
}

bool GetStatusChecksumError(void)
{
    return status_checksum_error;
}

void SetStatusChecksumError(bool value)
{
    status_checksum_error = value;
}

bool GetEnableChecksumError(void)
{
    return enable_checksum_error;
}

int GetPadByte(void)
{
    return pad_byte;
}

bool check_floor_address(void)
{
    bool flag = true;

    if (floor_address_setted) {
        /* Discard if lower than floor_address */
        if (g_phys_addr < (floor_address - starting_address)) {
            if (verbose_flag) {
                fprintf(fp, "Discard physical address less than %08X\n",
                    floor_address - starting_address);
            }
            flag = false;
        }
    }

    return flag;
}

bool check_ceiling_address(uint32_t temp)
{
    bool flag = true;

    if (ceiling_address_setted) {
        /* Discard if higher than ceiling_address */
        if (temp > (ceiling_address + starting_address)) {
            if (verbose_flag) {
                fprintf(fp, "Discard physical address more than %08X\n",
                    ceiling_address + starting_address);
            }
            flag = false;
        }
    }

    return flag;
}
