#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg);
void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t value);

uint16_t pci_get_vender_id(uint8_t bus, uint8_t device, uint8_t function);
uint16_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_base_class(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_sub_class(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_secondary_bus(uint8_t bus, uint8_t device, uint8_t function);

void init_pci();

#endif