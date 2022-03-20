#include <kernel/ethernet.h>
#include <kernel/arp.h>
#include <kernel/rtl8139.h>
#include <kernel/arp.h>
#include <kernel/ipv4.h>
#include <kernel/lock.h>
#include <kernel/panic.h>
#include <string.h>
#include <common.h>
#include <stdlib.h>
#include <stdio.h>
#include <network.h>

#define CRC_POLY    0xEDB88320

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
    return 0;
}



int eth_prep_pkt(eth_opt* opt, void* buf, uint16_t buf_len) {
    if(sizeof(eth_header) > buf_len) return -1;
    eth_header* header = (eth_header*) buf;
    *header = (eth_header) {
        .dest = opt->dest_mac,
        .src = rtl8139_mac(),
        .ethertype = switch_endian16(opt->type)
    };
    return sizeof(eth_header);
}

int eth_send_pkt(void* buf, uint16_t pkt_len) {
    int res = rtl8139_send_packet(buf, pkt_len);
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