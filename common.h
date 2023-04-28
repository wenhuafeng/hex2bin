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

extern uint8_t Checksum;
extern unsigned int Record_Nb;
extern unsigned int Nb_Bytes;

/* This will hold binary codes translated from hex file. */
extern unsigned int Lowest_Address, Highest_Address;
extern unsigned int Starting_Address, Phys_Addr;
extern unsigned int Records_Start; // Lowest address of the records
extern unsigned int Floor_Address;
extern bool Verbose_Flag;

extern void usage(void);
extern void NoFailOpenInputFile(char *Flnm);
extern void NoFailOpenOutputFile(char *Flnm);
extern void GetLine(char *str, FILE *in);
extern void GetFilename(char *dest, char *src);
extern void PutExtension(char *Flnm, char *Extension);

extern void VerifyChecksumValue(void);
extern void VerifyRangeFloorCeil(void);
extern void WriteMemory(void);
extern void Allocate_Memory_And_Rewind(void);
extern char *ReadDataBytes(char *p);
extern void ParseOptions(int argc, char *argv[]);

extern FILE *GetInFile(void);
extern bool GetFloorAddressSetted(void);
extern bool GetCeilingAddressSetted(void);
extern bool GetAddressAlignmentWord(void);
extern bool GetStatusChecksumError(void);
extern void SetStatusChecksumError(bool value);
extern bool GetEnableChecksumError(void);
extern unsigned int GetCeilingAddress(void);
extern int GetPadByte(void);

#endif
