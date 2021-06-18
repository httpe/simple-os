#ifndef _KERNEL_ARP_H
#define _KERNEL_ARP_H

#include <stdint.h>
#include <network.h>

// Number of cached IP-MAC mapping
#define ARP_CACHE_N_RECORD 512

int init_arp();
int arp_announce_ip(ip_addr ip);
int arp_probe(ip_addr ip);
int arp_process_packet(void* buf, uint16_t len);
int arp_ip2mac(ip_addr ip, mac_addr* mac);

#endif