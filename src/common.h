#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* FIXME how to get it from the system/OS? */
#define MAX_FILE_NAME_SIZE 260

#ifdef DOS
#define MAX_EXTENSION_SIZE 4
#else
#define MAX_EXTENSION_SIZE 16
#endif

/* The data records can contain 255 bytes: this means 512 characters. */
#define MAX_LINE_SIZE 1024

extern const char *program_name;
extern FILE *fp;

/* This will hold binary codes translated from hex file. */
extern uint32_t g_lowest_address;
extern uint32_t g_highest_address;
extern uint32_t g_phys_addr;
extern bool verbose_flag;

extern void usage(const char *func, uint32_t line);
extern bool NoFailOpenInputFile(char *file_name);
extern void NoFailCloseInputFile(char *file_name);
extern void NoFailOpenOutputFile(char *file_name);
extern void NoFailCloseOutputFile(char *file_name);
extern void GetLine(char *str, FILE *in);
extern void GetFilename(char *dest, char *src);
extern void PutExtension(char *file_name, char *extension);

extern void VerifyRangeFloorCeil(void);
extern void Allocate_Memory_And_Rewind(uint8_t **memory_block);
extern char *ReadDataBytes(char *p, uint8_t *memory_block, uint8_t *cs, uint16_t record_nb, uint32_t nb_bytes);
extern void WriteOutFile(uint8_t **memory_block);
extern void ParseOptions(int argc, char *argv[]);

extern FILE *GetInFile(void);
extern bool GetAddressAlignmentWord(void);
extern bool GetStatusChecksumError(void);
extern void SetStatusChecksumError(bool value);
extern bool GetEnableChecksumError(void);
extern int GetPadByte(void);
extern bool check_floor_address(void);
extern bool check_ceiling_address(uint32_t temp);

#endif
