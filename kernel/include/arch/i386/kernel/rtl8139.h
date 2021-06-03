#ifndef _ARCH_I386_RTL8139_H
#define _ARCH_I386_RTL8139_H

#include <stdint.h>

void init_rtl8139(uint8_t bus, uint8_t device, uint8_t function);
void rtl8139_send_packet(void* buf, uint size);

#endif