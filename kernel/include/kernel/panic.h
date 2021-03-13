#ifndef _KERNEL_PANIC_H
#define _KERNEL_PANIC_H

#include <stdint.h>
#include <stddef.h>

#define PANIC_ASSERT(condition) ((condition) ? (void)0 : panic(__FILE__, __LINE__, "ASSERTION-FAILED", #condition))
#define PANIC(reason) (panic(__FILE__, __LINE__, "KERNEL PANIC", #reason))

void panic(const char* file, uint32_t line, const char* panic_type, const char* desc) __attribute__ ((noreturn));

#endif