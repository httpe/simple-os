#include <kernel/panic.h>
#include <stdio.h>

void panic(const char* file, uint32_t line, const char* panic_type, const char* desc) {
    // Manually triggered kernel panic
    asm volatile("cli"); // Disable interrupts.

    printf("%s(%s) at %s:%d\n", panic_type, desc, file, line);
    // Halt by going into an infinite loop.
    for (;;);
}