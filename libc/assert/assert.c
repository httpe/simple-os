#include <stdint.h>
#include <stdio.h>

void user_panic(const char* file, uint32_t line, const char* panic_type, const char* desc) {
    printf("%s(%s) at %s:%d\n", panic_type, desc, file, line);
    // Halt by going into an infinite loop.
    for (;;);
}