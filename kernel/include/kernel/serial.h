#ifndef _KERNEL_SERIAL_H
#define _KERNEL_SERIAL_H

#include <stdbool.h>

bool is_serial_port_initialized();
void init_serial();
char read_serial();
void write_serial(char a);

#endif