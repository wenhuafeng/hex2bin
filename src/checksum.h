#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>

extern uint8_t *Memory_Block;

extern void *NoFailMalloc(size_t size);
extern int GetHex(const char *str);

extern void ChecksumLoop(uint8_t type);
extern void CrcParamsCheck(void);
extern void WriteMemory(void);

extern void Para_E(const char *str);
extern void Para_f(const char *str);
extern void Para_F(const char *str1, const char *str2);
extern void Para_k(const char *str);
extern void Para_r(const char *str1, const char *str2);
extern void Para_C(const char *str1, const char *str2, const char *str3, const char *str4, const char *str5);

#endif
