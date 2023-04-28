#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "binary.h"
#include "libcrc.h"
#include "common.h"

enum Crc {
    CHK8_SUM = 0,
    CHK16,
    CRC8,
    CRC16,
    CRC32,
    CHK16_8
};

#define LAST_CHECK_METHOD CHK16_8

enum Crc Cks_Type = CHK8_SUM;
unsigned int Cks_Start = 0;
unsigned int  Cks_End = 0;
unsigned int  Cks_Addr = 0;
unsigned int  Cks_Value = 0;
bool Cks_range_set = false;
bool Cks_Addr_set = false;
bool Force_Value = false;

uint16_t Crc_Poly = 0x07;
uint16_t Crc_Init = 0;
uint16_t Crc_XorOut = 0;
bool Crc_RefIn = false;
bool Crc_RefOut = false;

uint8_t *Memory_Block = NULL;
int Endian = 0;

typedef void (*checksumHandler)(void);
struct ChecksumProcess {
    uint8_t type;
    checksumHandler handler;
};

void *NoFailMalloc(size_t size)
{
    void *result;

    if ((result = malloc(size)) == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        exit(1);
    }

    return (result);
}

int GetHex(const char *str)
{
    int result;
    unsigned int value;

    result = sscanf(str, "%x", &value);

    if (result == 1) {
        return value;
    } else {
        fprintf(stderr, "GetHex: some error occurred when parsing options.\n");
        exit(1);
    }
}

// 0 or 1
static int GetBin(const char *str)
{
    int result;
    unsigned int value;

    result = sscanf(str, "%u", &value);

    if (result == 1) {
        return value & 1;
    } else {
        fprintf(stderr, "GetBin: some error occurred when parsing options.\n");
        exit(1);
    }
}

static void WriteMemBlock16(uint16_t Value)
{
    if (Endian == 1) {
        Memory_Block[Cks_Addr - Lowest_Address] = u16_hi(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 1] = u16_lo(Value);
    } else {
        Memory_Block[Cks_Addr - Lowest_Address + 1] = u16_hi(Value);
        Memory_Block[Cks_Addr - Lowest_Address] = u16_lo(Value);
    }
}

static void WriteMemBlock32(uint32_t Value)
{
    if (Endian == 1) {
        Memory_Block[Cks_Addr - Lowest_Address] = u32_b3(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 1] = u32_b2(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 2] = u32_b1(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 3] = u32_b0(Value);
    } else {
        Memory_Block[Cks_Addr - Lowest_Address + 3] = u32_b3(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 2] = u32_b2(Value);
        Memory_Block[Cks_Addr - Lowest_Address + 1] = u32_b1(Value);
        Memory_Block[Cks_Addr - Lowest_Address] = u32_b0(Value);
    }
}

static void Checksum8(void)
{
    uint8_t wCKS = 0;

    for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
        wCKS += Memory_Block[i - Lowest_Address];
    }

    fprintf(stdout, "8-bit Checksum = %02X\n", wCKS & 0xff);
    Memory_Block[Cks_Addr - Lowest_Address] = wCKS;
    fprintf(stdout, "Addr %08X set to %02X\n", Cks_Addr, wCKS);
}

static void Checksum16(void)
{
    uint16_t wCKS = 0;
    uint16_t w;

    if (Endian == 1) {
        for (unsigned int i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i - Lowest_Address + 1] | ((uint16_t)Memory_Block[i - Lowest_Address] << 8);
            wCKS += w;
        }
    } else {
        for (unsigned int i = Cks_Start; i <= Cks_End; i += 2) {
            w = Memory_Block[i - Lowest_Address] | ((uint16_t)Memory_Block[i - Lowest_Address + 1] << 8);
            wCKS += w;
        }
    }
    fprintf(stdout, "16-bit Checksum = %04X\n", wCKS);
    WriteMemBlock16(wCKS);
    fprintf(stdout, "Addr %08X set to %04X\n", Cks_Addr, wCKS);
}

static void Checksum16_8(void)
{
    uint16_t wCKS = 0;

    for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
        wCKS += Memory_Block[i - Lowest_Address];
    }

    fprintf(stdout, "16-bit Checksum = %04X\n", wCKS);
    WriteMemBlock16(wCKS);
    fprintf(stdout, "Addr %08X set to %04X\n", Cks_Addr, wCKS);
}

static void Crc8(void)
{
    uint8_t crc8;
    void *crc_table;

    crc_table = NoFailMalloc(256);
    if (Crc_RefIn) {
        init_crc8_reflected_tab(crc_table, Reflect8[Crc_Poly]);
        crc8 = Reflect8[Crc_Init];
    } else {
        init_crc8_normal_tab(crc_table, Crc_Poly);
        crc8 = Crc_Init;
    }

    for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
        crc8 = update_crc8(crc_table, crc8, Memory_Block[i - Lowest_Address]);
    }

    crc8 = (crc8 ^ Crc_XorOut) & 0xff;
    Memory_Block[Cks_Addr - Lowest_Address] = crc8;
    fprintf(stdout, "Addr %08X set to %02X\n", Cks_Addr, crc8);

    if (crc_table != NULL) {
        free(crc_table);
    }
}

static void Crc16(void)
{
    uint16_t crc16;
    void *crc_table;

    crc_table = NoFailMalloc(256 * 2);
    if (Crc_RefIn) {
        init_crc16_reflected_tab(crc_table, Reflect16(Crc_Poly));
        crc16 = Reflect16(Crc_Init);

        for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
            crc16 = update_crc16_reflected(crc_table, crc16, Memory_Block[i - Lowest_Address]);
        }
    } else {
        init_crc16_normal_tab(crc_table, Crc_Poly);
        crc16 = Crc_Init;


        for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
            crc16 = update_crc16_normal(crc_table, crc16, Memory_Block[i - Lowest_Address]);
        }
    }

    crc16 = (crc16 ^ Crc_XorOut) & 0xffff;
    WriteMemBlock16(crc16);
    fprintf(stdout, "Addr %08X set to %04X\n", Cks_Addr, crc16);

    if (crc_table != NULL) {
        free(crc_table);
    }
}

static void Crc32(void)
{
    uint32_t crc32;
    void *crc_table;

    crc_table = NoFailMalloc(256 * 4);
    if (Crc_RefIn) {
        init_crc32_reflected_tab(crc_table, Reflect32(Crc_Poly));
        crc32 = Reflect32(Crc_Init);

        for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
            crc32 = update_crc32_reflected(crc_table, crc32, Memory_Block[i - Lowest_Address]);
        }
    } else {
        init_crc32_normal_tab(crc_table, Crc_Poly);
        crc32 = Crc_Init;

        for (unsigned int i = Cks_Start; i <= Cks_End; i++) {
            crc32 = update_crc32_normal(crc_table, crc32, Memory_Block[i - Lowest_Address]);
        }
    }

    crc32 ^= Crc_XorOut;
    WriteMemBlock32(crc32);
    fprintf(stdout, "Addr %08X set to %08X\n", Cks_Addr, crc32);

    if (crc_table != NULL) {
        free(crc_table);
    }
}

static struct ChecksumProcess ChecksumProcessTable[] = {
    { CHK8_SUM, Checksum8 },
    { CHK16, Checksum16 },
    { CHK16_8, Checksum16_8 },
    { CRC8, Crc8 },
    { CRC16, Crc16 },
    { CRC32, Crc32 },
};

void ChecksumLoop(uint8_t type)
{
    uint8_t i;

    for (i = 0; i < sizeof(ChecksumProcessTable) / sizeof(ChecksumProcessTable[0]); i++) {
        if (type == ChecksumProcessTable[i].type) {
            ChecksumProcessTable[i].handler();
            break;
        }
    }
}

void CrcParamsCheck(void)
{
    switch (Cks_Type) {
        case CRC8:
            Crc_Poly &= 0xFF;
            Crc_Init &= 0xFF;
            Crc_XorOut &= 0xFF;
            break;
        case CRC16:
            Crc_Poly &= 0xFFFF;
            Crc_Init &= 0xFFFF;
            Crc_XorOut &= 0xFFFF;
            break;
        case CRC32:
            break;
        default:
            fprintf(stderr, "See file CRC list.txt for parameters\n");
            exit(1);
    }
}

void WriteMemory(void)
{
    if ((Cks_Addr >= Lowest_Address) && (Cks_Addr < Highest_Address)) {
        if (Force_Value) {
            switch (Cks_Type) {
                case 0:
                    Memory_Block[Cks_Addr - Lowest_Address] = Cks_Value;
                    fprintf(stdout, "Addr %08X set to %02X\n", Cks_Addr, Cks_Value);
                    break;
                case 1:
                    WriteMemBlock16(Cks_Value);
                    fprintf(stdout, "Addr %08X set to %04X\n", Cks_Addr, Cks_Value);
                    break;
                case 2:
                    WriteMemBlock32(Cks_Value);
                    fprintf(stdout, "Addr %08X set to %08X\n", Cks_Addr, Cks_Value);
                    break;
                default:
                    break;
            }
        } else if (Cks_Addr_set) {
            /* Add a checksum to the binary file */
            if (!Cks_range_set) {
                Cks_Start = Lowest_Address;
                Cks_End = Highest_Address;
            }
            /* checksum range MUST BE in the array bounds */

            if (Cks_Start < Lowest_Address) {
                fprintf(stdout, "Modifying range start from %X to %X\n", Cks_Start, Lowest_Address);
                Cks_Start = Lowest_Address;
            }
            if (Cks_End > Highest_Address) {
                fprintf(stdout, "Modifying range end from %X to %X\n", Cks_End, Highest_Address);
                Cks_End = Highest_Address;
            }

            ChecksumLoop(Cks_Type);
        }
    } else {
        if (Force_Value || Cks_Addr_set) {
            fprintf(stderr, "Force/Check address outside of memory range\n");
        }
    }
}

void Para_E(const char *str)
{
    Endian = GetBin(str);
}

void Para_f(const char *str)
{
    Cks_Addr = GetHex(str);
    Cks_Addr_set = true;
}

void Para_F(const char *str1, const char *str2)
{
    Cks_Addr = GetHex(str1);
    Cks_Value = GetHex(str2);
    Force_Value = true;
}

void Para_k(const char *str)
{
    Cks_Type = GetHex(str);
    if (Cks_Type > LAST_CHECK_METHOD) {
        usage();
    }
}

void Para_r(const char *str1, const char *str2)
{
    Cks_Start = GetHex(str1);
    Cks_End = GetHex(str2);
    Cks_range_set = true;
}

// Char t/T: true f/F: false
static bool GetBoolean(const char *str)
{
    int result;
    unsigned char value, temp;

    result = sscanf(str, "%c", &value);
    temp = tolower(value);

    if ((result == 1) && ((temp == 't') || (temp == 'f'))) {
        return (temp == 't');
    } else {
        fprintf(stderr, "GetBoolean: some error occurred when parsing options.\n");
        exit(1);
    }
}

void Para_C(const char *str1, const char *str2, const char *str3, const char *str4, const char *str5)
{
    Crc_Poly = GetHex(str1);
    Crc_Init = GetHex(str2);
    Crc_RefIn = GetBoolean(str3);
    Crc_RefOut = GetBoolean(str4);
    Crc_XorOut = GetHex(str5);
    CrcParamsCheck();
}
