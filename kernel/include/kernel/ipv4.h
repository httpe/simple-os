#ifndef _KERNEL_IPv4_H
#define _KERNEL_IPv4_H

#include <network.h>
#include <common.h>
#include <stdint.h>

int init_ipv4();
int ipv4_send_packet(uint8_t ttl, enum ipv4_protocol protocol, ip_addr dst, void* buf, uint16_t len);
int ipv4_process_packet(void* buf, uint len);
int ipv4_wait_for_next_packet(void* buf, uint buf_size);

#endif