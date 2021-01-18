#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>

bool is_serial_port_initialized();
int init_serial();
int serial_received();
char read_serial();
int is_transmit_empty();
void write_serial(char a);

#endif