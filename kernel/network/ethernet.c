#include <string.h>
#include <common.h>
#include <stdlib.h>
#include <stdio.h>
#include <network.h>
#include <kernel/panic.h>
#include <kernel/ethernet.h>
#include <kernel/arp.h>
#include <kernel/rtl8139.h>

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

int send_ethernet_packet(mac_addr dest_mac, enum ether_type type, void* buf, uint16_t len)
{
    // TODO: very inefficient, should implement a buffer queue 
    if(ethernet_transmit_buffer == NULL) {
        // Initialization
        ethernet_transmit_buffer = malloc(RTL8139_TRANSMIT_BUF_SIZE);
        // Announce our harcoded ip address before sending out any packet
        arp_announce_ip(MY_IP);
    }
    uint pkt_len = sizeof(eth_header) + len;
    PANIC_ASSERT(pkt_len <= RTL8139_TRANSMIT_BUF_SIZE);
    eth_header* header = (eth_header*) ethernet_transmit_buffer;
    header->dest = dest_mac;
    header->src = rtl8139_mac();
    header->ethertype = switch_endian16(type);
    memmove(ethernet_transmit_buffer + sizeof(eth_header), buf, len);
    // pkt->crc = crc32((uint8_t*) pkt, len);
    // return rtl8139_send_packet(pkt, len + sizeof(pkt->crc));
    int res = rtl8139_send_packet(ethernet_transmit_buffer, pkt_len);
    return res;
}


int process_ethernet_packet(void* buf, uint16_t len, uint32_t crc)
{
    uint8_t* buff = (uint8_t*) buf;
    for(int i=0; i<len; i++) {
        if(i % 16 == 0 && i != 0) {
            printf("\n");
        }
        printf("0x%x ", buff[i]);
    }
    uint32_t our_crc = crc32(buff, len);
    char* eq = (crc == our_crc) ? "==":"!=";
    printf("\nCRC: Received[0x%x] %s Calculated[0x%x]\n", crc, eq, eq,our_crc);
    return 0;
}