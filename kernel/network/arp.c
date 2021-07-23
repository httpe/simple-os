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

#define ARP_TRANSMIT_BUFF_LEN 64

static struct {
    char transmit_buff[ARP_TRANSMIT_BUFF_LEN];
    uint16_t transmit_hdr_len;
    yield_lock transmit_lk;

    arp_record* arp_cache;
    arp_record* next_free_record;
    uint cached_records;
    yield_lock arp_cache_lk;
} arp;

int init_arp()
{
    // for announce and prob the ethernet header and parts of the arp packet are the same, 
    // so prepare the transmit buffer when init
    arp_packet pl = {
        .htype = switch_endian16(ARP_HARDWARE_TYPE_ETHERNET), 
        .ptype = switch_endian16(ARP_PROTOCOL_TYPE_IPv4),
        .hlen = ARP_HARDWARE_ADDR_LEN_ETHERNET,
        .plen = ARP_PROTOCAL_ADDR_LEN_IPv4,
        .opcode = switch_endian16(ARP_OP_CODE_REQUEST),
        .sha = rtl8139_mac()
    };
    mac_addr dest_mac = {.addr = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}}; // broadcast
    eth_opt opt = (eth_opt) {.dest_mac = dest_mac, .type = ETHER_TYPE_ARP, .data_len = sizeof(pl)};
    arp.transmit_hdr_len = eth_prep_pkt(&opt, arp.transmit_buff, ARP_TRANSMIT_BUFF_LEN);
    PANIC_ASSERT(arp.transmit_hdr_len > 0);
    memmove(arp.transmit_buff + arp.transmit_hdr_len, &pl, sizeof(pl));

    arp.arp_cache = (arp_record*) malloc(sizeof(arp_record) * ARP_CACHE_N_RECORD);
    memset(arp.arp_cache, 0, sizeof(arp_record) * ARP_CACHE_N_RECORD);
    arp.next_free_record = arp.arp_cache;
    return 0;
}

int arp_announce_ip(ip_addr ip) 
{
    acquire(&arp.transmit_lk);
    arp_packet* pl = (arp_packet*) (arp.transmit_buff + arp.transmit_hdr_len);
    pl->spa = ip;
    pl->tpa = ip;
    int res = eth_send_pkt(arp.transmit_buff, arp.transmit_hdr_len + sizeof(arp_packet));
    release(&arp.transmit_lk);
    return res;
}

int arp_probe(ip_addr ip)
{
    acquire(&arp.transmit_lk);
    arp_packet* pl = (arp_packet*) (arp.transmit_buff + arp.transmit_hdr_len);
    pl->spa = (ip_addr) {0};
    pl->tpa = ip;
    int res = eth_send_pkt(arp.transmit_buff, arp.transmit_hdr_len + sizeof(arp_packet));
    release(&arp.transmit_lk);
    return res;
}

int arp_process_packet(void* buf, uint16_t len)
{
    UNUSED_ARG(len);
    acquire(&arp.arp_cache_lk);
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
    release(&arp.arp_cache_lk);
    return 0;
}


int arp_ip2mac(ip_addr ip, mac_addr* mac)
{
    acquire(&arp.arp_cache_lk);
    for(uint i=0; i<arp.cached_records; i++) {
        if(memcmp(arp.arp_cache[i].ip.addr, ip.addr, 4) == 0) {
            *mac = arp.arp_cache[i].mac;
            release(&arp.arp_cache_lk);
            return 0;
        }
    }
    release(&arp.arp_cache_lk);
    return -1;
}

