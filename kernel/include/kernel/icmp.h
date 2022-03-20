#ifndef _KERNEL_ICMP_H
#define _KERNEL_ICMP_H

#include <network.h>
#include <stdint.h>

int init_icmp();
// int icmp_process_packet(ip_addr src_ip, void* buf, uint16_t pkt_len);
// int icmp_send_packet(ip_addr dst, enum icmp_type type, enum icmp_code code, icmp_rest_of_header rest, void* data, uint16_t data_len);

int icmp_prep_pkt(icmp_opt* opt, void* buf, uint16_t buf_len);
int icmp_finalize_pkt(icmp_header* buf, uint16_t pkt_len);
// int icmp_send_pkt(int header_offset, void* buf, uint16_t pkt_len);

#endif