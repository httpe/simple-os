#ifndef _NETWORK_H
#define _NETWORK_H

#include <stdint.h>

typedef struct mac_addr {
    uint8_t addr[6]; // store as big endian
} mac_addr;

typedef struct eth_header {
    mac_addr dest;
    mac_addr src;
    uint16_t ethertype; // enum ether_type
}  __attribute__((packed)) eth_header;

enum ether_type {
    ETHER_TYPE_IPv4 = 0x0800,
    ETHER_TYPE_ARP = 0x0806
};


typedef struct ip_addr {
    uint8_t addr[4]; // store as big endian
} ip_addr;

// Hardcode IP addr before we implement DHCP
#define MY_IP ((ip_addr) {.addr = {10,0,2,16}})

static inline uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}

static inline uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
            ((nb<<8)&0xff0000)   |
            ((nb>>8)&0xff00)     |
            ((nb<<24)&0xff000000);
}

#define ARP_HARDWARE_TYPE_ETHERNET 0x1
#define ARP_PROTOCOL_TYPE_IPv4 0x0800
#define ARP_HARDWARE_ADDR_LEN_ETHERNET 6
#define ARP_PROTOCAL_ADDR_LEN_IPv4 4
#define ARP_OP_CODE_REQUEST 1
#define ARP_OP_CODE_REPLY 2

typedef struct arp
{
    uint16_t htype; // Hardware type, Ethernet is 0x1
    uint16_t ptype; // Protocol type, IP is 0x0800
    uint8_t  hlen; // Hardware address length (Ethernet = 6)
    uint8_t  plen; // Protocol address length (IPv4 = 4)
    uint16_t opcode; // ARP Operation Code
    mac_addr  sha; // Source hardware address - hlen bytes (see above)
    ip_addr  spa; // Source protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
    mac_addr  tha; // Destination hardware address - hlen bytes (see above)
    ip_addr  tpa; // Destination protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
}  __attribute__((packed)) arp;

#endif