#include <stdio.h>
#include <kernel/pci.h>
#include <arch/i386/kernel/port_io.h>

// Ref: https://wiki.osdev.org/PCI

#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC
// PCI_CONFIG_ADDRESS register layout
// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Offset


static void pci_check_bus(uint8_t bus);

uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldevice = (uint32_t)device;
    uint32_t lfunction = (uint32_t)function;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (ldevice << 11) |
              (lfunction << 8) | (reg << 2) | ((uint32_t)0x80000000));
 
    /* write out the address */
    outl(PCI_CONFIG_ADDRESS_PORT, address);

    /* read in the data */
    uint32_t r = inl(PCI_CONFIG_DATA_PORT);
    
    return r;
}

uint16_t pci_get_vender_id(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 0);
    return (uint16_t) (reg & 0xFFFF);
}

uint16_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 0);
    return (uint16_t) ((reg >> 16) & 0xFFFF);
}

uint8_t pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 3);
    return (uint8_t) ((reg >> 16) & 0xFF);
}

uint8_t pci_get_base_class(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 2);
    return (uint8_t) ((reg >> 24) & 0xFF);
}

uint8_t pci_get_sub_class(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 2);
    return (uint8_t) ((reg >> 16) & 0xFF);
}

uint8_t pci_get_secondary_bus(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t reg = pci_read_reg(bus, device, function, 6);
    return (uint8_t) ((reg >> 8) & 0xFF);
}



static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t secondaryBus;

    baseClass = pci_get_base_class(bus, device, function);
    subClass = pci_get_sub_class(bus, device, function);
    if( (baseClass == 0x06) && (subClass == 0x04) ) {
        secondaryBus = pci_get_secondary_bus(bus, device, function);
        pci_check_bus(secondaryBus);
    }
}

static void pci_check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorID = pci_get_vender_id(bus, device, function);
    if(vendorID == 0xFFFF) return;        // Device doesn't exist

    uint16_t device_id = pci_get_device_id(bus, device, function);
    printf("PCI Device Found: BUS[%u]:DEV[%u]:VENDOR_ID[0x%x]:DEV_ID[0x%x]\n", bus, device, vendorID, device_id);

    pci_check_function(bus, device, function);
    uint8_t headerType = pci_get_header_type(bus, device, function);
    if( (headerType & 0x80) != 0) {
        /* It is a multi-function device, so check remaining functions */
        for(function = 1; function < 8; function++) {
            if(pci_get_vender_id(bus, device, function) != 0xFFFF) {
                pci_check_function(bus, device, function);
            }
        }
    }
}


static void pci_check_bus(uint8_t bus) {
    uint8_t device;

    for(device = 0; device < 32; device++) {
        pci_check_device(bus, device);
    }
}

static void pci_check_all_bus(void) {
    uint8_t function;
    uint8_t bus;

    uint8_t headerType = pci_get_header_type(0, 0, 0);
    if( (headerType & 0x80) == 0) {
        /* Single PCI host controller */
        pci_check_bus(0);
    } else {
        /* Multiple PCI host controllers */
        for(function = 0; function < 8; function++) {
            if(pci_get_vender_id(0, 0, function) != 0xFFFF) break;
            bus = function;
            pci_check_bus(bus);
        }
    }
}

void init_pci()
{
    pci_check_all_bus();
}

