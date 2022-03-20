#ifndef _KERNEL_ETHERNET_H
#define _KERNEL_ETHERNET_H

#include <stdint.h>
#include <common.h>
#include <network.h>

int init_ethernet();
int process_ethernet_packet(void* buf, uint16_t len, uint32_t crc);

int eth_prep_pkt(eth_opt* opt, void* buf, uint16_t buf_len);
int eth_send_pkt(void* buf, uint16_t pkt_len);

#endif