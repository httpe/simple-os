#ifndef _STDIO_H
#define _STDIO_H 1

#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>

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

typedef void FILE; // we are using file descriptor as FILE*
FILE* fopen(const char * pathname, const char * mode);
int fclose(FILE *stream);
size_t fread(void * ptr, size_t size, size_t nitems,
       FILE * stream);
size_t fwrite(const void * ptr, size_t size, size_t nitems,
       FILE * stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
ssize_t write(int fildes, const void *buf, size_t nbyte);

#ifdef __cplusplus
}
#endif

#endif
