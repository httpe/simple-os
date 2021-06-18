#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/panic.h>
#include <kernel/rtl8139.h>
#include <kernel/arp.h>

static void* ipv4_transmit_buffer = NULL;
static uint16_t id_counter = 0x8bb4;

static uint16_t ipv4_checksum(void* buff, uint16_t len)
{
    uint32_t checksum = 0;
    uint16_t* buf = (uint16_t*) buff;
    while (len > 1) {
        checksum += * buf++;
        len -= 2;
    }
    //if any bytes left, pad the bytes and add
    if(len > 0) {
        checksum += *((uint8_t*) buf);
    }
    //Fold sum to 16 bits: add carrier to result
    while (checksum>>16) {
        checksum = (checksum & 0xffff) + (checksum >> 16);
    }
    //one's complement
    checksum = ~checksum;

    return (uint16_t) checksum;
}

int send_ipv4_packet(uint8_t ttl, enum ipv4_protocal protocol, ip_addr dst, void* buf, uint16_t len)
{
    if(ipv4_transmit_buffer == NULL) {
        ipv4_transmit_buffer = malloc(RTL8139_TRANSMIT_BUF_SIZE);
        memset(ipv4_transmit_buffer, 0, RTL8139_TRANSMIT_BUF_SIZE);
        // Announce our hardcoded ip address before sending out any packet
        arp_announce_ip(MY_IP);
    }

    PANIC_ASSERT(len + sizeof(ipv4_header) <= RTL8139_TRANSMIT_BUF_SIZE);

    mac_addr mac_dst = {0};
    if((SUBNET_MASK & *(uint32_t*) dst.addr) == (SUBNET_MASK & *(uint32_t*) GATEWAY_IP.addr)) {
        int r = arp_ip2mac(dst, &mac_dst);
        if(r < 0) {
            arp_probe(dst);
            return -1;
        }
    } else {
        int r = arp_ip2mac(GATEWAY_IP, &mac_dst);
        if(r < 0) {
            arp_probe(GATEWAY_IP);
            return -1;
        }
    }
    
    ipv4_header* hdr = (ipv4_header*) ipv4_transmit_buffer;
    *hdr = (ipv4_header) {
        .ver_ihl = IPv4_VER_IHL,
        .dscp_ecn = IPv4_DSCP_ECN,
        .len = sizeof(ipv4_header) + len,
        .id = id_counter++,
        .flags_fragoffset = IPv4_NO_FRAG,
        .ttl = ttl,
        .protocol = protocol,
        .src = MY_IP,
        .dst = dst
    };

    // swap endianess of all multi bytes fields
    // assuming ip_addr is store in big endian already
    hdr->len = switch_endian16(hdr->len);
    hdr->id =  switch_endian16(hdr->id);
    hdr->flags_fragoffset =  switch_endian16(hdr->flags_fragoffset);
    
    // the checksum is already in the right endianess
    hdr->hdr_checksum = ipv4_checksum(hdr, sizeof(ipv4_header));

    memmove(ipv4_transmit_buffer + sizeof(ipv4_header), buf, len);

    int res = send_ethernet_packet(mac_dst, ETHER_TYPE_IPv4, ipv4_transmit_buffer, len + sizeof(ipv4_header));

    return res;
}