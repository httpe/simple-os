#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/panic.h>
#include <kernel/rtl8139.h>
#include <kernel/arp.h>
#include <kernel/ipv4.h>

static void* ipv4_transmit_buffer = NULL;
static uint16_t id_counter = 0x8bb4;

int init_ipv4()
{
    ipv4_transmit_buffer = malloc(RTL8139_TRANSMIT_BUF_SIZE);
    memset(ipv4_transmit_buffer, 0, RTL8139_TRANSMIT_BUF_SIZE);
    // Announce our hardcoded ip address before sending out any packet
    arp_announce_ip(MY_IP);
    // Get default gateway MAC address
    arp_probe(DEFAULT_GATEWAY_IP);
    return 0;
}

int ipv4_send_packet(uint8_t ttl, enum ipv4_protocol protocol, ip_addr dst, void* buf, uint16_t len)
{
    PANIC_ASSERT(ipv4_transmit_buffer != NULL);

    PANIC_ASSERT(len + sizeof(ipv4_header) <= RTL8139_TRANSMIT_BUF_SIZE);

    // Hardcoded type of IP routing table
    // If local (determined by SUBNET_MASK), send to the target machine
    // Otherwise, send to default gateway
    mac_addr mac_dst = {0};
    if((SUBNET_MASK & *(uint32_t*) dst.addr) == (SUBNET_MASK & *(uint32_t*) DEFAULT_GATEWAY_IP.addr)) {
        int r = arp_ip2mac(dst, &mac_dst);
        if(r < 0) {
            arp_probe(dst);
            return -1;
        }
    } else {
        int r = arp_ip2mac(DEFAULT_GATEWAY_IP, &mac_dst);
        if(r < 0) {
            arp_probe(DEFAULT_GATEWAY_IP);
            return -1;
        }
    }
    
    ipv4_header* hdr = (ipv4_header*) ipv4_transmit_buffer;
    *hdr = (ipv4_header) {
        .ver_ihl = IPv4_VER_IHL,
        .dscp_ecn = IPv4_DSCP_ECN,
        .len = sizeof(ipv4_header) + len,
        .id = id_counter++,
        .flags_fragoffset = IPv4_NO_FRAG,
        .ttl = ttl,
        .protocol = protocol,
        .src = MY_IP,
        .dst = dst
    };

    // swap endianess of all multi bytes fields
    // assuming ip_addr is store in big endian already
    hdr->len = switch_endian16(hdr->len);
    hdr->id =  switch_endian16(hdr->id);
    hdr->flags_fragoffset =  switch_endian16(hdr->flags_fragoffset);
    
    // the checksum is already in the right endianess
    hdr->hdr_checksum = ipv4_icmp_checksum(hdr, sizeof(ipv4_header));

    memmove(ipv4_transmit_buffer + sizeof(ipv4_header), buf, len);

    int res = send_ethernet_packet(mac_dst, ETHER_TYPE_IPv4, ipv4_transmit_buffer, len + sizeof(ipv4_header));

    return res;
}

int ipv4_process_packet(void* buf, uint len)
{
    ipv4_header* hdr = (ipv4_header*) buf;

    uint8_t header_len = ((hdr->ver_ihl) & 0x0F) * 4;
    uint16_t orig_checksum = hdr->hdr_checksum;
    hdr->hdr_checksum = 0;
    uint16_t our_checksum = ipv4_icmp_checksum(buf, header_len);
    hdr->hdr_checksum = orig_checksum;
    char* eq = (orig_checksum == our_checksum) ? "==":"!=";
    
    printf("IPv4 Pkt Received, Chksum: Received[0x%x] %s Calculated[0x%x]:\n", orig_checksum, eq, our_checksum);
 
    uint8_t* buff = (uint8_t*) buf;
    for(uint i=0; i<len; i++) {
        if(i % 16 == 0 && i != 0) {
            printf("\n");
        }
        printf("0x%x ", buff[i]);
    }
    printf("\n");

    return 0;
}
