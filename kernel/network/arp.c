#include <kernel/arp.h>
#include <kernel/ethernet.h>
#include <kernel/rtl8139.h>
#include <kernel/process.h>
#include <kernel/panic.h>
#include <kernel/lock.h>
#include <network.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <common.h>

typedef struct arp_record {
    ip_addr ip;
    mac_addr mac;
} arp_record;

static struct {
    arp_record* arp_cache;
    arp_record* next_free_record;
    uint cached_records;
    yield_lock lk;
} arp;

int init_arp()
{
    arp.arp_cache = (arp_record*) malloc(sizeof(arp_record) * ARP_CACHE_N_RECORD);
    memset(arp.arp_cache, 0, sizeof(arp_record) * ARP_CACHE_N_RECORD);
    arp.next_free_record = arp.arp_cache;
    return 0;
}

int arp_announce_ip(ip_addr ip)
{
    arp_packet pl = {
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
    arp_packet pl = {
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
    acquire(&arp.lk);
    PANIC_ASSERT(arp.arp_cache != NULL);
    
    arp_packet* pkt = (arp_packet*) buf;
    if(pkt->opcode == switch_endian16(ARP_OP_CODE_REPLY)) {
        // Record any ARP reply to cache
        arp.next_free_record->ip = pkt->spa;
        arp.next_free_record->mac = pkt->sha;
        arp.next_free_record++;
        if(arp.cached_records < ARP_CACHE_N_RECORD) {
            arp.cached_records++;
        }
        if(arp.next_free_record - arp.arp_cache == ARP_CACHE_N_RECORD) {
            // circular buffer
            arp.next_free_record = arp.arp_cache;
        }
        printf("ARP Cached: IP[%d.%d.%d.%d] = MAC[%x:%x:%x:%x:%x:%x]\n", 
            pkt->spa.addr[0],
            pkt->spa.addr[1],
            pkt->spa.addr[2],
            pkt->spa.addr[3],
            pkt->sha.addr[0],
            pkt->sha.addr[1],
            pkt->sha.addr[2],
            pkt->sha.addr[3],
            pkt->sha.addr[4],
            pkt->sha.addr[5]
        );
    }
    release(&arp.lk);
    return 0;
}


int arp_ip2mac(ip_addr ip, mac_addr* mac)
{
    acquire(&arp.lk);
    for(uint i=0; i<arp.cached_records; i++) {
        if(memcmp(arp.arp_cache[i].ip.addr, ip.addr, 4) == 0) {
            *mac = arp.arp_cache[i].mac;
            release(&arp.lk);
            return 0;
        }
    }
    release(&arp.lk);
    return -1;
}

