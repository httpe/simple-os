#include <stdio.h>
#include <common.h>
#include <kernel/panic.h>
#include <kernel/pci.h>
#include <arch/i386/kernel/port_io.h>
#include <arch/i386/kernel/rtl8139.h>

// Ref: https://wiki.osdev.org/PCI

// PCI_CONFIG_ADDRESS register layout
// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Offset
#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

#define VENDER_ID(bus,device,function) (uint16_t) (pci_read_reg((bus), (device), (function), 0) & 0xFFFF)
#define DEVICE_ID(bus,device,function) (uint16_t) ((pci_read_reg((bus), (device), (function), 0) >> 16) & 0xFFFF)
#define HEADER_TYPE(bus,device,function) (uint8_t) ((pci_read_reg((bus), (device), (function), 3) >> 16) & 0xFF)
#define BASE_CLASS(bus,device,function) (uint8_t) ((pci_read_reg((bus), (device), (function), 2) >> 24) & 0xFF)
#define SUB_CLASS(bus,device,function) (uint8_t) ((pci_read_reg((bus), (device), (function), 2) >> 16) & 0xFF)
#define SECONDARY_BUS(bus,device,function) (uint8_t) ((pci_read_reg((bus), (device), (function), 6) >> 8) & 0xFF)
#define BAR_0(bus,device,function) pci_read_reg((bus), (device), (function), 4)
#define BAR_1(bus,device,function) pci_read_reg((bus), (device), (function), 5)

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

void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg, uint32_t value)
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

    /* write out the data */
    outl(PCI_CONFIG_DATA_PORT, value);
    
}


static void init_pci_device(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id = VENDER_ID(bus, device, function);
    uint16_t device_id = DEVICE_ID(bus, device, function);
    uint8_t header_type = HEADER_TYPE(bus, device, function);
    uint32_t bar0 = BAR_0(bus, device, function);
    uint32_t bar1 = BAR_1(bus, device, function);
    printf("PCI Device Found: BUS[%u]:DEV[%u]:FUNC[%u]:VENDOR[0x%x]:DEV_ID[0x%x]:H[%u]:B0[0x%x]:B1[0x%x]\n", 
        bus, device, function, vendor_id, device_id, header_type, bar0, bar1);

    // Hard coded list of know PCI devices
    if(vendor_id == 0x10EC && device_id == 0x8139) {
        init_rtl8139(bus, device, function);
    }
}

static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t secondaryBus;

    init_pci_device(bus, device, function);

    baseClass = BASE_CLASS(bus, device, function);
    subClass = SUB_CLASS(bus, device, function);
    if( (baseClass == 0x06) && (subClass == 0x04) ) {
        secondaryBus = SECONDARY_BUS(bus, device, function);
        pci_check_bus(secondaryBus);
    }
}

static void pci_check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorID = VENDER_ID(bus, device, function);
    if(vendorID == 0xFFFF) return;        // Device doesn't exist

    pci_check_function(bus, device, function);
    uint8_t headerType = HEADER_TYPE(bus, device, function);
    if( (headerType & 0x80) != 0) {
        /* It is a multi-function device, so check remaining functions */
        for(function = 1; function < 8; function++) {
            if(VENDER_ID(bus, device, function) != 0xFFFF) {
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

    uint8_t headerType = HEADER_TYPE(0, 0, 0);
    if( (headerType & 0x80) == 0) {
        /* Single PCI host controller */
        pci_check_bus(0);
    } else {
        /* Multiple PCI host controllers */
        for(function = 0; function < 8; function++) {
            if(VENDER_ID(0, 0, function) != 0xFFFF) break;
            bus = function;
            pci_check_bus(bus);
        }
    }
}

void init_pci()
{
    pci_check_all_bus();
}

