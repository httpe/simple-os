#include <string.h>
#include <common.h>
#include <stdlib.h>
#include <kernel/panic.h>
#include <kernel/ethernet.h>
#include <kernel/rtl8139.h>

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

static uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}

static uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
            ((nb<<8)&0xff0000)   |
            ((nb>>8)&0xff00)     |
            ((nb<<24)&0xff000000);
}

int send_ethernet_packet(mac_addr dest_mac, enum ether_type type, void* buf, uint16_t len)
{
    uint pkt_len = sizeof(eth_header) + len;
    PANIC_ASSERT(pkt_len <= RTL8139_TRANSMIT_BUF_SIZE);
    // TODO: very inefficient, should implement a buffer queue
    char* buffer = malloc(pkt_len); 
    eth_header* header = (eth_header*) buffer;
    header->dest = dest_mac;
    header->src = rtl8139_mac();
    header->ethertype = switch_endian16(type);
    memmove(buffer + sizeof(eth_header), buf, len);
    // pkt->crc = crc32((uint8_t*) pkt, len);
    // return rtl8139_send_packet(pkt, len + sizeof(pkt->crc));
    int res = rtl8139_send_packet(buffer, pkt_len);
    free(buffer);
    return res;
}
