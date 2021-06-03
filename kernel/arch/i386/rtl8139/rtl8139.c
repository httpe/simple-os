#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <common.h>
#include <kernel/panic.h>
#include <kernel/pci.h>
#include <arch/i386/kernel/rtl8139.h>
#include <arch/i386/kernel/port_io.h>
#include <kernel/paging.h>
#include <arch/i386/kernel/isr.h>
#include <arch/i386/kernel/pic.h>

// Ref: https://wiki.osdev.org/RTL8139

typedef struct rtl8139 {
    uint16_t io_base;
    uint8_t mac[6];
    void* receive_buff;
    uint32_t receive_buff_phy_addr;
    uint packet_sent;
    void* send_buff;
    uint32_t send_buff_phy_addr;
} rtl8139;

static int initialized = 0;
static rtl8139 dev;

static void rtl8139_irq_handler(trapframe* tf)
{
    UNUSED_ARG(tf);

    uint16_t int_reg = inw(dev.io_base + 0x3E);
    printf("RTL8139 IRQ: Int reg 0x%x\n", int_reg);

    if(int_reg & 1) {
        printf("RTL8139 IRQ: Receive OK (ROK)\n");
        // Confirm IRQ processed
        outw(dev.io_base + 0x3E, 1);
    }
    if(int_reg & (1 << 2)) {
        printf("RTL8139 IRQ: Transmit OK (TOK)\n");
        // Confirm IRQ processed
        outw(dev.io_base + 0x3E, 1 << 2);
    }

}


void init_rtl8139(uint8_t bus, uint8_t device, uint8_t function)
{
    // only support one RTL8139
    PANIC_ASSERT(!initialized);
    
    uint16_t command = PCI_COMMAND(bus,device,function);
    command |= PCI_COMMAND_BUS_MASTER; // Enable PCI Bus Mastering
    command &= ~PCI_COMMAND_INT_DISABLE; // Enable PCI interrupt
    PCI_W_COMMAND(bus, device, function, command);

    uint32_t bar0 =  PCI_BAR_0(bus, device, function);
    // Make sure BAR0 is I/O space address
    PANIC_ASSERT(bar0 & 1);
    PANIC_ASSERT((bar0 & ~0x3) < 0xFFFF);
    
    memset(&dev, 0, sizeof(dev));
    dev.io_base = (bar0 & ~0x3) & 0xFFFF;
    printf("RTL8139 base I/O port: 0x%x\n", dev.io_base);

    // Turning on the RTL8139
    outb(dev.io_base + 0x52, 0x0);

    // Software Reset
    outb(dev.io_base + 0x37, 0x10);
    while( (inb(dev.io_base + 0x37) & 0x10) != 0);

    // Read MAC
    uint32_t mac_lo = inl(dev.io_base + 0x0);
    uint16_t mac_hi = inw(dev.io_base + 0x4);
    memmove(dev.mac, &mac_lo, 4);
    memmove(dev.mac + 4, &mac_hi, 2);

    printf("RTL8139 MAC: %x:%x:%x:%x:%x:%x\n", dev.mac[0],dev.mac[1],dev.mac[2],dev.mac[3],dev.mac[4],dev.mac[5]);

    // Init Receive buffer
    uint page_count = PAGE_COUNT_FROM_BYTES(8192 + 16 + 150);
    dev.receive_buff = (void*) alloc_pages_consecutive_frames(curr_page_dir(), page_count, true, &dev.receive_buff_phy_addr);
    memset(dev.receive_buff, 0, page_count*PAGE_SIZE);
    outl(dev.io_base + 0x30, dev.receive_buff_phy_addr);

    // Init transmit buffer
    page_count = PAGE_COUNT_FROM_BYTES(1792 * 4);
    dev.send_buff = (void*) alloc_pages_consecutive_frames(curr_page_dir(), page_count, true, &dev.send_buff_phy_addr);
    memset(dev.send_buff, 0, page_count*PAGE_SIZE);

    // Get interrupt line
    uint8_t irq = PCI_INT_LINE(bus,device,function);
    printf("RTL8139 is using IRQ(%u)\n", irq);
    register_interrupt_handler(IRQ_TO_INTERRUPT(irq), rtl8139_irq_handler);
    IRQ_clear_mask(irq);

    // Set IMR + ISR
    // To set the RTL8139 to accept only the Transmit OK (TOK) and Receive OK (ROK) interrupts
    outw(dev.io_base + 0x3C, 0x0005);

    // Configuring receive buffer (RCR)
    // AB - Accept Broadcast: Accept broadcast packets sent to mac ff:ff:ff:ff:ff:ff
    // AM - Accept Multicast: Accept multicast packets.
    // APM - Accept Physical Match: Accept packets send to NIC's MAC address.
    // AAP - Accept All Packets. Accept all packets (run in promiscuous mode).
    outl(dev.io_base + 0x44, 0xF | (1 << 7)); // (1 << 7) is the WRAP bit, 0xF is AB+AM+APM+AAP

    // Enable Receive and Transmitter
    outb(dev.io_base + 0x37, 0x0C); // Sets the RE (Receiver Enabled) and the TE (Transmitter Enabled) bits high

    initialized = 1;
}

void rtl8139_send_packet(void* buf, uint size)
{
    PANIC_ASSERT(size <= 1792);
    uint buff_idx = dev.packet_sent % 4;
    uint32_t transmit_buff_paddr = dev.send_buff_phy_addr + 1792*buff_idx;
    uint start_reg = dev.io_base + 0x20 + 4*buff_idx;
    uint status_reg = dev.io_base + 0x10 + 4*buff_idx;

    memmove(dev.send_buff + 1792*buff_idx, buf, size);
    outl(start_reg, transmit_buff_paddr);
    outl(status_reg, size);

    dev.packet_sent++;
}