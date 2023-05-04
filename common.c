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

char Extension[MAX_EXTENSION_SIZE]; /* filename extension for output files */

FILE *Filin;  /* input files */
FILE *Filout; /* output files */

#ifdef USE_FILE_BUFFERS
char *FilinBuf;  /* text buffer for file input */
char *FiloutBuf; /* text buffer for file output */
#endif

int Pad_Byte = 0xFF;
bool Enable_Checksum_Error = false;
bool Status_Checksum_Error = false;
uint8_t Checksum;
unsigned int Record_Nb;
unsigned int Nb_Bytes;

/* This will hold binary codes translated from hex file. */
unsigned int Lowest_Address, Highest_Address;
unsigned int Starting_Address, Phys_Addr;
unsigned int Records_Start; // Lowest address of the records
unsigned int Max_Length = 0;
unsigned int Minimum_Block_Size = 0x1000; // 4096 byte
unsigned int Floor_Address = 0x0;
unsigned int Ceiling_Address = 0xFFFFFFFF;
int Module;
bool Minimum_Block_Size_Setted = false;
bool Starting_Address_Setted = false;
bool Floor_Address_Setted = false;
bool Ceiling_Address_Setted = false;
bool Max_Length_Setted = false;
bool Swap_Wordwise = false;
bool Address_Alignment_Word = false;
bool Batch_Mode = false;
bool Verbose_Flag = false;

//int Endian = 0;

/* procedure USAGE */
void usage(void)
{
    fprintf(stderr,
        "\n"
        "usage: %s [OPTIONS] filename\n"
        "Options:\n"
        "  -a            Address Alignment Word (hex2bin only)\n"
        "  -b            Batch mode: exits if specified file doesn't exist\n"
        "  -c            Enable record checksum verification\n"
        "  -C [Poly][Init][RefIn][RefOut][XorOut]\n                CRC parameters\n"
        "  -e [ext]      Output filename extension (without the dot)\n"
        "  -E [0|1]      Endian for checksum/CRC, 0: little, 1: big\n"
        "  -f [address]  Address of check result to write\n"
        "  -F [address] [value]\n                Address and value to force\n"
        "  -k [0-5]      Select check method (checksum or CRC) and size\n"
        "  -d            display list of check methods/value size\n"
        "  -l [length]   Maximal Length (Starting address + Length -1 is Max Address)\n"
        "                File will be filled with Pattern until Max Address is reached\n"
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
        Pgm_Name, Pad_Byte);
    exit(1);
}

static void DisplayCheckMethods(void)
{
    fprintf(stderr, "Check methods/value size:\n"
        "0:  Checksum  8-bit\n"
        "1:  Checksum 16-bit (adds 16-bit words into a 16-bit sum, data and result BE or LE)\n"
        "2:  CRC8\n"
        "3:  CRC16\n"
        "4:  CRC32\n"
        "5:  Checksum 16-bit (adds bytes into a 16-bit sum, result BE or LE)\n");
    exit(1);
}

/* Open the input file, with error checking */
void NoFailOpenInputFile(char *Flnm)
{
    while ((Filin = fopen(Flnm, "r")) == NULL) {
        if (Batch_Mode) {
            fprintf(stderr, "Input file %s cannot be opened.\n", Flnm);
            exit(1);
        } else {
            fprintf(stderr, "Input file %s cannot be opened. Enter new filename: ", Flnm);
            if (Flnm[strlen(Flnm) - 1] == '\n') {
                Flnm[strlen(Flnm) - 1] = '\0';
            }
        }
    }

#ifdef USE_FILE_BUFFERS
    FilinBuf = (char *)NoFailMalloc(BUFFSZ);
    setvbuf(Filin, FilinBuf, _IOFBF, BUFFSZ);
#endif
} /* procedure OPENFILIN */

void NoFailCloseInputFile(char *Flnm)
{
    fclose(Filin);
}

/* Open the output file, with error checking */
void NoFailOpenOutputFile(char *Flnm)
{
    while ((Filout = fopen(Flnm, "wb")) == NULL) {
        if (Batch_Mode) {
            fprintf(stderr, "Output file %s cannot be opened.\n", Flnm);
            exit(1);
        } else {
            /* Failure to open the output file may be
             simply due to an insufficient permission setting. */
            fprintf(stderr, "Output file %s cannot be opened. Enter new file name: ", Flnm);
            if (Flnm[strlen(Flnm) - 1] == '\n') {
                Flnm[strlen(Flnm) - 1] = '\0';
            }
        }
    }

#ifdef USE_FILE_BUFFERS
    FiloutBuf = (char *)NoFailMalloc(BUFFSZ);
    setvbuf(Filout, FiloutBuf, _IOFBF, BUFFSZ);
#endif
} /* procedure OPENFILOUT */

void NoFailCloseOutputFile(char *Flnm)
{
    //fclose(fileOut);
}

void GetLine(char *str, FILE *in)
{
    char *result;

    result = fgets(str, MAX_LINE_SIZE, in);
    if ((result == NULL) && !feof(in)) {
        fprintf(stderr, "Error occurred while reading from file\n");
    }
}

#if 0
static int GetDec(const char *str)
{
    int result;
    unsigned int value;

    result = sscanf(str, "%u", &value);

    if (result == 1) {
        return value;
    } else {
        fprintf(stderr, "GetDec: some error occurred when parsing options.\n");
        exit(1);
    }
}
#endif

void GetFilename(char *dest, char *src)
{
    if (strlen(src) < MAX_FILE_NAME_SIZE) {
        strcpy(dest, src);
    } else {
        fprintf(stderr, "filename length exceeds %d characters.\n", MAX_FILE_NAME_SIZE);
        exit(1);
    }
}

static void GetExtension(const char *str, char *ext)
{
    if (strlen(str) > MAX_EXTENSION_SIZE) {
        usage();
    }

    strcpy(ext, str);
}

/* Adds an extension to a file name */
void PutExtension(char *Flnm, char *Extension)
{
    char *Period; /* location of period in file name */

    /* This assumes DOS like file names */
    /* Don't use strchr(): consider the following filename:
     ../my.dir/file.hex
    */
    if ((Period = strrchr(Flnm, '.')) != NULL) {
        *(Period) = '\0';
        if (strcmp(Extension, Period + 1) == 0) {
            fprintf(stderr, "Input and output filenames (%s) are the same.\n", Flnm);
            exit(1);
        }
    }
    strcat(Flnm, ".");
    strcat(Flnm, Extension);
}

void VerifyChecksumValue(void)
{
    if ((Checksum != 0) && Enable_Checksum_Error) {
        fprintf(stderr, "Checksum error in record %d: should be %02X\n", Record_Nb, (256 - Checksum) & 0xFF);
        Status_Checksum_Error = true;
    }
}

/* Check if are set Floor and Ceiling Address and range is coherent */
void VerifyRangeFloorCeil(void)
{
    if (Floor_Address_Setted && Ceiling_Address_Setted && (Floor_Address >= Ceiling_Address)) {
        fprintf(stderr, "Floor address %08X higher than Ceiling address %08X\n", Floor_Address, Ceiling_Address);
        exit(1);
    }
}

void Allocate_Memory_And_Rewind(void)
{
    Records_Start = Lowest_Address;

    if (Starting_Address_Setted == true) {
        Lowest_Address = Starting_Address;
    } else {
        Starting_Address = Lowest_Address;
    }

    if (Max_Length_Setted == false) {
        Max_Length = Highest_Address - Lowest_Address + 1;
    } else {
        Highest_Address = Lowest_Address + Max_Length - 1;
    }

    fprintf(stdout, "Allocate_Memory_and_Rewind:\n");
    fprintf(stdout, "Lowest address:   %08X\n", Lowest_Address);
    fprintf(stdout, "Highest address:  %08X\n", Highest_Address);
    fprintf(stdout, "Starting address: %08X\n", Starting_Address);
    fprintf(stdout, "Max Length:       %u\n\n", Max_Length);

    /* Now that we know the buffer size, we can allocate it. */
    /* allocate a buffer */
    Memory_Block = (uint8_t *)NoFailMalloc(Max_Length);

    /* For EPROM or FLASH memory types, fill unused bytes with FF or the value specified by the p option */
    memset(Memory_Block, Pad_Byte, Max_Length);

    rewind(Filin);
}

char *ReadDataBytes(char *p)
{
    unsigned int i, temp2;
    int result;

    /* Read the Data bytes. */
    /* Bytes are written in the Memory block even if checksum is wrong. */
    i = Nb_Bytes;

    do {
        result = sscanf(p, "%2x", &temp2);
        if (result != 1) {
            fprintf(stderr, "ReadDataBytes: error in line %d of hex file\n", Record_Nb);
        }
        p += 2;

        /* Check that the physical address stays in the buffer's range. */
        if (Phys_Addr < Max_Length) {
            /* Overlapping record will erase the pad bytes */
            if (Swap_Wordwise) {
                if (Memory_Block[Phys_Addr ^ 1] != Pad_Byte)
                    fprintf(stderr, "Overlapped record detected\n");
                Memory_Block[Phys_Addr++ ^ 1] = temp2;
            } else {
                if (Memory_Block[Phys_Addr] != Pad_Byte)
                    fprintf(stderr, "Overlapped record detected\n");
                Memory_Block[Phys_Addr++] = temp2;
            }

            Checksum = (Checksum + temp2) & 0xFF;
        }
    } while (--i != 0);

    return p;
}

void WriteOutFile(void)
{
    /* write binary file */
    fwrite(Memory_Block, Max_Length, 1, Filout);

    free(Memory_Block);

    // Minimum_Block_Size is set; the memory buffer is multiple of this?
    if (Minimum_Block_Size_Setted == true) {
        Module = Max_Length % Minimum_Block_Size;
        if (Module) {
            Module = Minimum_Block_Size - Module;
            Memory_Block = (uint8_t *)NoFailMalloc(Module);
            memset(Memory_Block, Pad_Byte, Module);
            fwrite(Memory_Block, Module, 1, Filout);
            free(Memory_Block);
            if (Max_Length_Setted == true)
                fprintf(stdout, "Attention Max Length changed by Minimum Block Size\n");
            // extended
            Max_Length += Module;
            Highest_Address += Module;
            fprintf(stdout, "Extended\nHighest address: %08X\n", Highest_Address);
            fprintf(stdout, "Max Length: %u\n\n", Max_Length);
        }
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
    int Param;
    char *p;

    Starting_Address = 0;

    for (Param = 1; Param < argc; Param++) {
        int i = 0;
        char c;

        p = argv[Param];
        c = *(p + 1); /* Get option character */

        if (_IS_OPTION_(*p)) {
            // test for no space between option and parameter
            if (strlen(p) != 2) {
                usage();
            }

            switch (c) {
                case 'a':
                    Address_Alignment_Word = true;
                    i = 0;
                    break;
                case 'b':
                    Batch_Mode = true;
                    i = 0;
                    break;
                case 'c':
                    Enable_Checksum_Error = true;
                    i = 0;
                    break;
                case 'd':
                    DisplayCheckMethods();
                case 'e':
                    GetExtension(argv[Param + 1], Extension);
                    i = 1; /* add 1 to Param */
                    break;
                case 'E':
                    Para_E(argv[Param + 1]);
                    i = 1; /* add 1 to Param */
                    break;
                case 'f':
                    Para_f(argv[Param + 1]);
                    i = 1; /* add 1 to Param */
                    break;
                case 'F':
                    Para_F(argv[Param + 1], argv[Param + 2]);
                    i = 2; /* add 2 to Param */
                    break;
                case 'k':
                    Para_k(argv[Param + 1]);
                    i = 1; /* add 1 to Param */
                    break;
                case 'l':
                    Max_Length = GetHex(argv[Param + 1]);
                    if (Max_Length > 0x800000) {
                        fprintf(stderr, "Max_Length = %u\n", Max_Length);
                        exit(1);
                    }
                    Max_Length_Setted = true;
                    i = 1; /* add 1 to Param */
                    break;
                case 'm':
                    Minimum_Block_Size = GetHex(argv[Param + 1]);
                    Minimum_Block_Size_Setted = true;
                    i = 1; /* add 1 to Param */
                    break;
                case 'p':
                    Pad_Byte = GetHex(argv[Param + 1]);
                    i = 1; /* add 1 to Param */
                    break;
                case 'r':
                    Para_r(argv[Param + 1], argv[Param + 2]);
                    i = 2; /* add 2 to Param */
                    break;
                case 's':
                    Starting_Address = GetHex(argv[Param + 1]);
                    Starting_Address_Setted = true;
                    i = 1; /* add 1 to Param */
                    break;
                case 'v':
                    Verbose_Flag = true;
                    i = 0;
                    break;
                case 't':
                    Floor_Address = GetHex(argv[Param + 1]);
                    Floor_Address_Setted = true;
                    i = 1; /* add 1 to Param */
                    break;
                case 'T':
                    Ceiling_Address = GetHex(argv[Param + 1]);
                    Ceiling_Address_Setted = true;
                    i = 1; /* add 1 to Param */
                    break;
                case 'w':
                    Swap_Wordwise = true;
                    i = 0;
                    break;
                case 'C':
                    Para_C(argv[Param + 1], argv[Param + 2], argv[Param + 3], argv[Param + 4], argv[Param + 5]);
                    i = 5; /* add 5 to Param */
                    break;

                case '?':
                case 'h':
                default:
                    usage();
                    break;
            }

            /* Last parameter is not a filename */
            if (Param == argc - 1) {
                usage();
            }

            // fprintf(stderr,"Param: %d, option: %c\n",Param,c);

            /* if (Param + i) < (argc -1) */
            if (Param < argc - 1 - i) {
                Param += i;
            } else {
                usage();
            }
        } else {
            break;
        }
        /* if option */
    } /* for Param */
}

FILE *GetInFile(void)
{
    return Filin;
}

bool GetFloorAddressSetted(void)
{
    return Floor_Address_Setted;
}

bool GetCeilingAddressSetted(void)
{
    return Ceiling_Address_Setted;
}

bool GetAddressAlignmentWord(void)
{
    return Address_Alignment_Word;
}

bool GetStatusChecksumError(void)
{
    return Status_Checksum_Error;
}

void SetStatusChecksumError(bool value)
{
    Status_Checksum_Error = value;
}

bool GetEnableChecksumError(void)
{
    return Enable_Checksum_Error;
}

unsigned int GetCeilingAddress(void)
{
    return Ceiling_Address;
}

int GetPadByte(void)
{
    return Pad_Byte;
}
