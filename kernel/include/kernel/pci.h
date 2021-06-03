#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t size);
void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t size, uint32_t value);

void init_pci();

// For any header type
#define PCI_VENDER_ID(bus,device,function) ((uint16_t) pci_read_reg((bus), (device), (function), 0, 2))
#define PCI_DEVICE_ID(bus,device,function) ((uint16_t) pci_read_reg((bus), (device), (function), 2, 2))
#define PCI_HEADER_TYPE(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x0E, 1))
#define PCI_BASE_CLASS(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x0B, 1))
#define PCI_SUB_CLASS(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x0B, 1))

#define PCI_COMMAND(bus,device,function) ((uint16_t) pci_read_reg((bus), (device), (function), 4, 2))
#define PCI_W_COMMAND(bus,device,function,value) pci_write_reg((bus), (device), (function), 4, 2, (value))
#define PCI_COMMAND_BUS_MASTER (1 << 2)
#define PCI_COMMAND_INT_DISABLE (1 << 10)

#define PCI_STATUS(bus,device,function) ((uint16_t) pci_read_reg((bus), (device), (function), 6, 2))

// For header type 0 and 1
#define PCI_BAR_0(bus,device,function) pci_read_reg((bus), (device), (function), 0x10, 4)
#define PCI_BAR_1(bus,device,function) pci_read_reg((bus), (device), (function), 0x14, 4)
#define PCI_INT_PIN(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x3D, 1)
#define PCI_INT_LINE(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x3C, 1))

// For header type 1
#define PCI_SECONDARY_BUS(bus,device,function) ((uint8_t) pci_read_reg((bus), (device), (function), 0x19, 1))




#endif