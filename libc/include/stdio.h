#ifndef _STDIO_H
#define _STDIO_H 1

#include <stddef.h>
#include <sys/cdefs.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* __restrict format, ...);
int kprintf(const char* __restrict format, ...);
int snprintf(char* buf, size_t size, const char* __restrict format, ...);
int putchar(int ic);
int serial_putchar(int ic);
int puts(const char* string);

#ifdef __cplusplus
}
#endif

#endif
