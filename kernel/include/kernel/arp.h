#ifndef _KERNEL_ARP_H
#define _KERNEL_ARP_H

#include <stdint.h>
#include <network.h>

int arp_announce_ip(ip_addr ip);

#endif