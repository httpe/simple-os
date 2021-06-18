#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <common.h>
#include <network.h>
#include <kernel/panic.h>
#include <kernel/ethernet.h>
#include <kernel/rtl8139.h>
#include <kernel/arp.h>
#include <kernel/process.h>

typedef struct arp_record {
    ip_addr ip;
    mac_addr mac;
} arp_record;

static arp_record* arp_cache = NULL;
static arp_record* next_free_record = NULL;
static uint cached_records = 0;

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

int arp_probe(ip_addr ip)
{
    arp pl = {
        .htype = switch_endian16(ARP_HARDWARE_TYPE_ETHERNET), 
        .ptype = switch_endian16(ARP_PROTOCOL_TYPE_IPv4),
        .hlen = ARP_HARDWARE_ADDR_LEN_ETHERNET,
        .plen = ARP_PROTOCAL_ADDR_LEN_IPv4,
        .opcode = switch_endian16(ARP_OP_CODE_REQUEST)
    };
    pl.sha = rtl8139_mac();
    // pl.spa = MY_IP;
    pl.tpa = ip;
    mac_addr dest_mac = {.addr = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}}; // broadcast
    int res = send_ethernet_packet(dest_mac, ETHER_TYPE_ARP, &pl, sizeof(pl));
    return res;
}

int arp_process_packet(void* buf, uint16_t len)
{
    UNUSED_ARG(len);
    if(arp_cache == NULL) {
        arp_cache = (arp_record*) malloc(sizeof(arp_record) * ARP_CACHE_N_RECORD);
        memset(arp_cache, 0, sizeof(arp_record) * ARP_CACHE_N_RECORD);
        next_free_record = arp_cache;
    }
    arp* head = (arp*) buf;
    if(head->opcode == switch_endian16(ARP_OP_CODE_REPLY)) {
        // Record any ARP reply to cache
        next_free_record->ip = head->spa;
        next_free_record->mac = head->sha;
        next_free_record++;
        if(cached_records < ARP_CACHE_N_RECORD) {
            cached_records++;
        }
        if(next_free_record - arp_cache == ARP_CACHE_N_RECORD) {
            // circular buffer
            next_free_record = arp_cache;
        }
        printf("ARP Cached: IP[%d.%d.%d.%d] = MAC[%x:%x:%x:%x:%x:%x]\n", 
            head->spa.addr[0],
            head->spa.addr[1],
            head->spa.addr[2],
            head->spa.addr[3],
            head->sha.addr[0],
            head->sha.addr[1],
            head->sha.addr[2],
            head->sha.addr[3],
            head->sha.addr[4],
            head->sha.addr[5]
        );
    }
    return 0;
}


int arp_ip2mac(ip_addr ip, mac_addr* mac)
{
    for(uint i=0; i<cached_records; i++) {
        if(memcmp(arp_cache[i].ip.addr, ip.addr, 4) == 0) {
            *mac = arp_cache[i].mac;
            return 0;
        }
    }
    return -1;
}

