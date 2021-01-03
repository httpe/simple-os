#ifndef TTY_H
#define TTY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "vga.h"
#include "../../string/string.h"

void print_str(const char* str, uint8_t row, uint8_t col);
void print_memory(const char* buff, size_t size, uint8_t row, uint8_t col);
void print_memory_hex(const char* buff, size_t size, uint8_t row);

#endif