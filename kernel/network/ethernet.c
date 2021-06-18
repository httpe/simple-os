#include <string.h>
#include <common.h>
#include <stdlib.h>
#include <stdio.h>
#include <network.h>
#include <kernel/panic.h>
#include <kernel/ethernet.h>
#include <kernel/arp.h>
#include <kernel/rtl8139.h>
#include <kernel/arp.h>
#include <kernel/ipv4.h>

#define CRC_POLY    0xEDB88320

static void* ethernet_transmit_buffer = NULL;

// Source: https://stackoverflow.com/questions/60684565/c-crc32-checksum-does-not-match-wireshark-on-ethernet-frame-check-sequence
static uint32_t crc32(uint8_t *data, uint len)
{
    uint i, j;
    uint32_t crc;

    if (!data)
        return 0;

    if (len < 1)
        return 0;

    crc = 0xFFFFFFFF;

    for (j = 0; j < len; j++) {
        crc ^= data[j];

        for (i = 0; i < 8; i++) {
             crc = (crc & 1) ? ((crc >> 1) ^ CRC_POLY) : (crc >> 1);
        }
    }

    return (crc ^ 0xFFFFFFFF);
}

int init_ethernet()
{
    ethernet_transmit_buffer = malloc(RTL8139_TRANSMIT_BUF_SIZE);
    memset(ethernet_transmit_buffer, 0, RTL8139_TRANSMIT_BUF_SIZE);
    return 0;
}

int send_ethernet_packet(mac_addr dest_mac, enum ether_type type, void* buf, uint16_t len)
{
    PANIC_ASSERT(ethernet_transmit_buffer != NULL);

    if(buf == NULL || len == 0) {
        return 0;
    }

    uint pkt_len = sizeof(eth_header) + len;
    PANIC_ASSERT(pkt_len <= RTL8139_TRANSMIT_BUF_SIZE);
    eth_header* header = (eth_header*) ethernet_transmit_buffer;
    header->dest = dest_mac;
    header->src = rtl8139_mac();
    header->ethertype = switch_endian16(type);
    // TODO: very inefficient, should implement a buffer queue 
    memmove(ethernet_transmit_buffer + sizeof(eth_header), buf, len);
    int res = rtl8139_send_packet(ethernet_transmit_buffer, pkt_len);
    return res;
}


int process_ethernet_packet(void* buf, uint16_t len, uint32_t crc)
{
    eth_header* head = (eth_header*) buf;

    uint8_t* buff = (uint8_t*) buf;
    uint32_t our_crc = crc32(buff, len);
    char* eq = (crc == our_crc) ? "==":"!=";
    printf("Ethernet Pkt Received, CRC: Received[0x%x] %s Calculated[0x%x]:\n", crc, eq, our_crc);

    if(head->ethertype == switch_endian16(ETHER_TYPE_ARP)) {
        arp_process_packet(buf + sizeof(eth_header), len - sizeof(eth_header));
    } else if(head->ethertype == switch_endian16(ETHER_TYPE_IPv4)) {
        ipv4_process_packet(buf + sizeof(eth_header), len - sizeof(eth_header));
    }

    return 0;
}