#ifndef _NETWORK_H
#define _NETWORK_H

#include <stdint.h>

typedef struct mac_addr {
    uint8_t addr[6];
} mac_addr;

typedef struct eth_header {
    mac_addr dest;
    mac_addr src;
    uint16_t ethertype; // enum ether_type
}  __attribute__((packed)) eth_header;

enum ether_type {
    ETHER_TYPE_IPv4 = 0x0800
};

#endif