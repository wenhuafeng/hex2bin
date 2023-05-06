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

extern const char *Pgm_Name;
extern FILE *fp;

/* This will hold binary codes translated from hex file. */
extern unsigned int Lowest_Address;
extern unsigned int Highest_Address;
extern unsigned int Phys_Addr;
extern bool Verbose_Flag;

extern void usage(const char *func, unsigned int line);
extern bool NoFailOpenInputFile(char *Flnm);
extern void NoFailCloseInputFile(char *Flnm);
extern void NoFailOpenOutputFile(char *Flnm);
extern void NoFailCloseOutputFile(char *Flnm);
extern void GetLine(char *str, FILE *in);
extern void GetFilename(char *dest, char *src);
extern void PutExtension(char *Flnm, char *extension);

//extern void VerifyChecksumValue(uint8_t cs, uint16_t record_nb);
extern void VerifyRangeFloorCeil(void);
extern void Allocate_Memory_And_Rewind(uint8_t **memory_block);
extern char *ReadDataBytes(char *p, uint8_t *memory_block, uint8_t *cs, uint16_t record_nb, unsigned int Nb_Bytes);
extern void WriteOutFile(uint8_t **memory_block);
extern void ParseOptions(int argc, char *argv[]);

extern FILE *GetInFile(void);
extern bool GetAddressAlignmentWord(void);
extern bool GetStatusChecksumError(void);
extern void SetStatusChecksumError(bool value);
extern bool GetEnableChecksumError(void);
extern int GetPadByte(void);
extern bool floor_address(void);
extern bool ceiling_address(unsigned int temp);

#endif
