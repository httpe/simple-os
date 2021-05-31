#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
void init_pci();

#endif