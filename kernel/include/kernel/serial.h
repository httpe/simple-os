#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>

bool is_serial_port_initialized();
int init_serial();
char read_serial();
void write_serial(char a);

#endif