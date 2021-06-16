#ifndef _KERNEL_ETHERNET_H
#define _KERNEL_ETHERNET_H

#include <stdint.h>
#include <common.h>
#include <network.h>

int send_ethernet_packet(mac_addr dest_mac, enum ether_type type, void* buf, uint16_t len);

#endif