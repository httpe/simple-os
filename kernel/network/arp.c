#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <common.h>
#include <network.h>
#include <kernel/panic.h>
#include <kernel/ethernet.h>
#include <kernel/rtl8139.h>


int arp_announce_ip(ip_addr ip)
{
    arp pl = {
        .htype = switch_endian16(ARP_HARDWARE_TYPE_ETHERNET), 
        .ptype = switch_endian16(ARP_PROTOCOL_TYPE_IPv4),
        .hlen = ARP_HARDWARE_ADDR_LEN_ETHERNET,
        .plen = ARP_PROTOCAL_ADDR_LEN_IPv4,
        .opcode = switch_endian16(ARP_OP_CODE_REQUEST)
    };
    pl.sha = rtl8139_mac();
    pl.spa = ip;
    pl.tpa = ip;
    mac_addr dest_mac = {.addr = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}}; // broadcast
    int res = send_ethernet_packet(dest_mac, ETHER_TYPE_ARP, &pl, sizeof(pl));
    return res;
}