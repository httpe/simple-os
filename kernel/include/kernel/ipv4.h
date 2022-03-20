#ifndef _KERNEL_IPv4_H
#define _KERNEL_IPv4_H

#include <network.h>
#include <common.h>
#include <stdint.h>

int init_ipv4();
int ipv4_process_packet(void* buf, uint len);
int ipv4_wait_for_next_packet(void* buf, uint buf_size, uint timeout_sec);

int ipv4_prep_pkt(ipv4_opt* opt, void* buf, uint16_t buf_len);
int ipv4_send_pkt(void* buf, uint16_t pkt_len);

int ipv4_opt_from_header(const ipv4_header* hdr, ipv4_opt* opt);

#endif