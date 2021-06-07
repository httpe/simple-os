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
// Ref: https://www.cs.usfca.edu/~cruse/cs326f04/RTL8139D_DataSheet.pdf

// Receive buffer size, +1500 to allow WRAP=1 in Rx Configuration Register
#define RTL8139_RECEIVE_BUF_SIZE (8192+16+1500)
#define RTL8139_TRANSMIT_BUF_SIZE 1792

typedef struct rtl8139 {
    uint16_t io_base;
    uint8_t mac[6];
    void* receive_buff;
    uint32_t receive_buff_phy_addr;
    uint32_t rx_offset;
    uint packet_sent;
    void* send_buff;
    uint32_t send_buff_phy_addr;
} rtl8139;

typedef struct rtl18139_packet_header {
    union
    {
        struct {
            uint16_t ROK:1;
            uint16_t FAE:1;
            uint16_t CRC:1;
            uint16_t LONG:1;
            uint16_t RUNT:1;
            uint16_t ISE:1;
            uint16_t reserved:7;
            uint16_t BAR:1;
            uint16_t PAM:1;
            uint16_t MAR:1;

        } status_detail;
        uint16_t status;
    } packet_status;
    uint16_t packet_len;
} rtl18139_packet_header;

static int initialized = 0;
static rtl8139 dev;

static void rtl8139_irq_handler(trapframe* tf)
{
    UNUSED_ARG(tf);

    uint16_t int_reg = inw(dev.io_base + RTL8139_ISR);
    printf("RTL8139 IRQ: Int reg 0x%x\n", int_reg);

    // Acknowledge IRQ
    outw(dev.io_base + RTL8139_ISR, int_reg);

    if(int_reg & RTL8139_IxR_ROK) {
        printf("RTL8139 IRQ: Receive OK (ROK)\n");

        rtl18139_packet_header* header = dev.receive_buff + dev.rx_offset;
        printf("RTL8139 IRQ: Receive Status[0x%x], Len[%u], Content:", 
            header->packet_status.status,
            header->packet_len
            );
        unsigned char* buf = dev.receive_buff + dev.rx_offset + sizeof(rtl18139_packet_header);
        for(int i=0; i<header->packet_len - 4; i++) { // 4: CRC trailing the packet data
            if(i % 16 == 0) {
                printf("\n");
            }
            printf("0x%x ", buf[i]);
        }
        printf("\nCRC[0x%x]\n", *(uint32_t*) &buf[header->packet_len - 4]);

        dev.rx_offset += (dev.rx_offset + header->packet_len + sizeof(rtl18139_packet_header) + 3) & -4;
        if(dev.rx_offset >= RTL8139_RECEIVE_BUF_SIZE) {
            dev.rx_offset -= RTL8139_RECEIVE_BUF_SIZE;
        }
    }
    if(int_reg & RTL8139_IxR_TOK) {
        printf("RTL8139 IRQ: Transmit OK (TOK)\n");
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
    outb(dev.io_base + RTL8139_CONFIG1, 0x0);

    // Software Reset
    outb(dev.io_base + RTL8139_CR, RTL8139_CR_RST);
    while( (inb(dev.io_base + RTL8139_CR) & RTL8139_CR_RST) != 0);

    // Read MAC
    uint32_t mac_lo = inl(dev.io_base + RTL8139_IDR_HI);
    uint16_t mac_hi = inw(dev.io_base + RTL8139_IDR_LO);
    memmove(dev.mac, &mac_lo, 4);
    memmove(dev.mac + 4, &mac_hi, 2);

    printf("RTL8139 MAC: %x:%x:%x:%x:%x:%x\n", dev.mac[0],dev.mac[1],dev.mac[2],dev.mac[3],dev.mac[4],dev.mac[5]);

    // Init Receive buffer
    uint page_count = PAGE_COUNT_FROM_BYTES(RTL8139_RECEIVE_BUF_SIZE);
    dev.receive_buff = (void*) alloc_pages_consecutive_frames(curr_page_dir(), page_count, true, &dev.receive_buff_phy_addr);
    memset(dev.receive_buff, 0, page_count*PAGE_SIZE);
    dev.rx_offset = 0;
    outw(dev.io_base + RTL8139_CAPR, 0);
    outl(dev.io_base + RTL8139_RBSTART, dev.receive_buff_phy_addr);

    // Init transmit buffer
    page_count = PAGE_COUNT_FROM_BYTES(RTL8139_TRANSMIT_BUF_SIZE * 4);
    dev.send_buff = (void*) alloc_pages_consecutive_frames(curr_page_dir(), page_count, true, &dev.send_buff_phy_addr);
    memset(dev.send_buff, 0, page_count*PAGE_SIZE);

    // Get interrupt line
    uint8_t irq = PCI_INT_LINE(bus,device,function);
    printf("RTL8139 is using IRQ(%u)\n", irq);
    register_interrupt_handler(IRQ_TO_INTERRUPT(irq), rtl8139_irq_handler);
    IRQ_clear_mask(irq);

    // Set IMR + ISR
    // To set the RTL8139 to accept only the Transmit OK (TOK) and Receive OK (ROK) interrupts
    outw(dev.io_base + RTL8139_IMR, RTL8139_IxR_TOK | RTL8139_IxR_ROK);

    // Configuring receive buffer (RCR)
    // AB (bit 3) - Accept Broadcast: Accept broadcast packets sent to mac ff:ff:ff:ff:ff:ff
    // AM (bit 2) - Accept Multicast: Accept multicast packets.
    // APM (bit 1) - Accept Physical Match: Accept packets send to NIC's MAC address.
    // AAP (bit 0) - Accept All Packets. Accept all packets (run in promiscuous mode)
    outl(dev.io_base + RTL8139_RCR, RTL8139_RCR_AB | RTL8139_RCR_AM | RTL8139_RCR_APM | RTL8139_RCR_WRAP); // (1 << 7) is the WRAP bit, 0xE is AB+AM+APM

    // Enable Receive and Transmitter
    outb(dev.io_base + RTL8139_CR, RTL8139_CR_RXEN | RTL8139_CR_TXEN); // Sets the RE (Receiver Enabled) and the TE (Transmitter Enabled) bits high

    initialized = 1;
}

void rtl8139_send_packet(void* buf, uint size)
{
    PANIC_ASSERT(size <= RTL8139_TRANSMIT_BUF_SIZE);
    uint buff_idx = dev.packet_sent % 4; // there are four transimit buffer used iteratively
    uint32_t transmit_buff_paddr = dev.send_buff_phy_addr + RTL8139_CR_TXEN*buff_idx;
    uint start_reg = dev.io_base + RTL8139_TSAD_n(buff_idx);
    uint status_reg = dev.io_base + RTL8139_TSD_n(buff_idx);

    memmove(dev.send_buff + RTL8139_TRANSMIT_BUF_SIZE*buff_idx, buf, size);
    outl(start_reg, transmit_buff_paddr);
    outl(status_reg, size);

    dev.packet_sent++;
}