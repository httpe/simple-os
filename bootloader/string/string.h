#ifndef STRING_H
#define STRING_H


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int memcmp(const void* aptr, const void* bptr, size_t size);
size_t strlen(const char* str);

#endif