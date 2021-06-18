#ifndef _KERNEL_IPv4_H
#define _KERNEL_IPv4_H

#include <network.h>
#include <common.h>
#include <stdint.h>


int send_ipv4_packet(uint8_t ttl, enum ipv4_protocal protocol, ip_addr dst, void* buf, uint16_t len);


#endif