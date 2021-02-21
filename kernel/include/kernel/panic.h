#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include <stddef.h>

#define PANIC_ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

void panic_assert(const char* file, uint32_t line, const char* desc);

#endif