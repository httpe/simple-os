#include <stdio.h>
#include <common.h>
#include <kernel/panic.h>
#include <kernel/pci.h>
#include <arch/i386/kernel/port_io.h>
#include <kernel/rtl8139.h>

// Ref: https://wiki.osdev.org/PCI

// PCI_CONFIG_ADDRESS register layout
// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Offset
#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

static void pci_check_bus(uint8_t bus);


uint32_t pci_read_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t size)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldevice = (uint32_t)device;
    uint32_t lfunction = (uint32_t)function;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (ldevice << 11) |
              (lfunction << 8) | (offset & ~3) | ((uint32_t)0x80000000));
 
    /* write out the address */
    outl(PCI_CONFIG_ADDRESS_PORT, address);

    /* read in the data */
    uint32_t r = inl(PCI_CONFIG_DATA_PORT);
    r = r >> (offset & 3)*8;
    if(size == 4) {
        return r;
    } else if(size == 2) {
        return r & 0xFFFF;
    } else if(size == 1) {
        return r & 0xFF;
    }

    PANIC("pci_read_reg size not supported");
}

void pci_write_reg(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t size, uint32_t value)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldevice = (uint32_t)device;
    uint32_t lfunction = (uint32_t)function;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (ldevice << 11) |
              (lfunction << 8) | (offset &  ~3) | ((uint32_t)0x80000000));
 
    /* write out the address */
    outl(PCI_CONFIG_ADDRESS_PORT, address);

    /* write out the data */
    if(size == 4) {
        outl(PCI_CONFIG_DATA_PORT, value);
    } else {
        uint32_t reg = inl(PCI_CONFIG_DATA_PORT);
        if(size == 2) {
            uint16_t* p_reg = (uint16_t*) &reg;
            p_reg[(offset & 3)/2] = (uint16_t) value;
        } else if (size == 1) {
            uint8_t* p_reg = (uint8_t*) &reg;
            p_reg[offset & 3] = (uint8_t) value;
        } else {
            PANIC("pci_write_reg size not supported");
        }
        outl(PCI_CONFIG_DATA_PORT, reg);
    }
    
}

static void init_pci_device(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id = PCI_VENDER_ID(bus, device, function);
    uint16_t PCI_DEVICE_ID = PCI_DEVICE_ID(bus, device, function);
    uint8_t PCI_HEADER_TYPE = PCI_HEADER_TYPE(bus, device, function);
    uint32_t bar0 = PCI_BAR_0(bus, device, function);
    uint32_t bar1 = PCI_BAR_1(bus, device, function);
    printf("PCI Device Found: BUS[%u]:DEV[%u]:FUNC[%u]:VENDOR[0x%x]:DEV_ID[0x%x]:H[%u]:B0[0x%x]:B1[0x%x]\n", 
        bus, device, function, vendor_id, PCI_DEVICE_ID, PCI_HEADER_TYPE, bar0, bar1);

    // Hard coded list of know PCI devices
    if(vendor_id == 0x10EC && PCI_DEVICE_ID == 0x8139) {
        init_rtl8139(bus, device, function);
    }
}

static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint8_t baseClass;
    uint8_t subClass;
    uint8_t secondaryBus;

    init_pci_device(bus, device, function);

    baseClass = PCI_BASE_CLASS(bus, device, function);
    subClass = PCI_SUB_CLASS(bus, device, function);
    if( (baseClass == 0x06) && (subClass == 0x04) ) {
        secondaryBus = PCI_SECONDARY_BUS(bus, device, function);
        pci_check_bus(secondaryBus);
    }
}

static void pci_check_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    uint16_t vendorID = PCI_VENDER_ID(bus, device, function);
    if(vendorID == 0xFFFF) return;        // Device doesn't exist

    pci_check_function(bus, device, function);
    uint8_t headerType = PCI_HEADER_TYPE(bus, device, function);
    if( (headerType & 0x80) != 0) {
        /* It is a multi-function device, so check remaining functions */
        for(function = 1; function < 8; function++) {
            if(PCI_VENDER_ID(bus, device, function) != 0xFFFF) {
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

    uint8_t headerType = PCI_HEADER_TYPE(0, 0, 0);
    if( (headerType & 0x80) == 0) {
        /* Single PCI host controller */
        pci_check_bus(0);
    } else {
        /* Multiple PCI host controllers */
        for(function = 0; function < 8; function++) {
            if(PCI_VENDER_ID(0, 0, function) != 0xFFFF) break;
            bus = function;
            pci_check_bus(bus);
        }
    }
}

void init_pci()
{
    pci_check_all_bus();
}

