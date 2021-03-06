#ifndef _NETWORK_H
#define _NETWORK_H

#include <stdint.h>
#include <common.h>
#include <syscall.h>



///////////////////////////////
//
// Ethernet Protocol
//
///////////////////////////////


typedef struct mac_addr {
    uint8_t addr[6]; // store as big endian
} mac_addr;

typedef struct eth_header {
    mac_addr dest;
    mac_addr src;
    uint16_t ethertype; // enum ether_type
}  __attribute__((packed)) eth_header;

enum ether_type {
    ETHER_TYPE_IPv4 = 0x0800,
    ETHER_TYPE_ARP = 0x0806
};

typedef struct {
    mac_addr dest_mac;
    enum ether_type type;
    uint16_t data_len;
} eth_opt;

///////////////////////////////
//
// IPv4 Protocol
//
///////////////////////////////

// Hardcode IP addrs before we implement DHCP
#define MY_IP ((ip_addr) {.addr = {10,0,2,16}})
#define DEFAULT_GATEWAY_IP ((ip_addr) {.addr = {10,0,2,2}})
#define SUBNET_MASK 0xFFFFFF00

typedef struct ip_addr {
    uint8_t addr[4]; // store as big endian
} ip_addr;

// version = 4, IHL (header length) = 20 bytes
#define IPv4_VER_IHL 0x45
// DSCP (Differentiated Services Codepoint) = default, ECN (Explicit Congestion Notification) = No
#define IPv4_DSCP_ECN 0
// flags_fragoffset: no frag
#define IPv4_NO_FRAG 0x4000

enum ipv4_protocol {
    IPv4_PROTOCAL_ICMP = 1
};

typedef struct ipv4_header
{
    uint8_t ver_ihl;
    uint8_t dscp_ecn;
    uint16_t len;
    uint16_t id;
    uint16_t flags_fragoffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    ip_addr src;
    ip_addr dst;
}  __attribute__((packed)) ipv4_header;

// 16-bit ones' complement of the ones' complement sum of all 16-bit words
static inline uint16_t ipv4_icmp_checksum(void* buff, uint16_t len)
{
    uint32_t checksum = 0;
    uint16_t* buf = (uint16_t*) buff;
    while (len > 1) {
        checksum += * buf++;
        len -= 2;
    }
    //if any bytes left, pad the bytes and add
    if(len > 0) {
        checksum += *((uint8_t*) buf);
    }
    //Fold sum to 16 bits: add carrier to result
    while (checksum>>16) {
        checksum = (checksum & 0xffff) + (checksum >> 16);
    }
    //one's complement
    checksum = ~checksum;

    return (uint16_t) checksum;
}

typedef struct {
    uint8_t ttl;
    enum ipv4_protocol protocol;
    ip_addr dst;
    uint16_t data_len;
} ipv4_opt;

///////////////////////////////
//
// ARP Protocol
//
///////////////////////////////

#define ARP_HARDWARE_TYPE_ETHERNET 0x1
#define ARP_PROTOCOL_TYPE_IPv4 0x0800
#define ARP_HARDWARE_ADDR_LEN_ETHERNET 6
#define ARP_PROTOCAL_ADDR_LEN_IPv4 4
#define ARP_OP_CODE_REQUEST 1
#define ARP_OP_CODE_REPLY 2

typedef struct arp_packet
{
    uint16_t htype; // Hardware type, Ethernet is 0x1
    uint16_t ptype; // Protocol type, IP is 0x0800
    uint8_t  hlen; // Hardware address length (Ethernet = 6)
    uint8_t  plen; // Protocol address length (IPv4 = 4)
    uint16_t opcode; // ARP Operation Code
    mac_addr  sha; // Source hardware address - hlen bytes (see above)
    ip_addr  spa; // Source protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
    mac_addr  tha; // Destination hardware address - hlen bytes (see above)
    ip_addr  tpa; // Destination protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
}  __attribute__((packed)) arp_packet;


///////////////////////////////
//
// ICMP Protocol
//
///////////////////////////////


enum icmp_type {
    ICMP_TYPE_ECHO_REPLY = 0,
    ICMP_TYPE_ECHO_REQUEST = 8
};

enum icmp_code {
    ICMP_CODE_ECHO = 0
};

//mimic Linux: /usr/include/netinet/ip_icmp.h
typedef struct icmp_rest_of_header {
    union
    {
        struct
        {
        uint16_t	id;
        uint16_t	sequence;
        } echo;			/* echo datagram */
        uint32_t	gateway;	/* gateway address */
        struct
        {
        uint16_t	__glibc_reserved;
        uint16_t	mtu;
        } frag;			/* path mtu discovery */
    } un;
} __attribute__((packed)) icmp_rest_of_header;


typedef struct icmp_header {
	uint8_t type; // enum icmp_type
	uint8_t code; // enum icmp_code
	uint16_t checksum;
    icmp_rest_of_header rest;
} __attribute__((packed)) icmp_header;


typedef struct {
    struct {
        uint8_t ttl;
        ip_addr dst;
    } ipv4;

    enum icmp_type type;
    enum icmp_code code;
    icmp_rest_of_header rest;
    uint16_t data_len;
} icmp_opt;

///////////////////////////////
//
// Inline Network Utility
//
///////////////////////////////

static inline uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}

static inline uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
            ((nb<<8)&0xff0000)   |
            ((nb>>8)&0xff00)     |
            ((nb<<24)&0xff000000);
}

static inline void switch_endian(uint8_t* buf, uint32_t n) {
    for(uint i=0; i<n/2; i++) {
        uint8_t b = buf[i];
        buf[i] = buf[n-1-i];
        buf[n-1-i] = b;
    }
}


///////////////////////////////
//
// Network System Calls
//
//////////////////////////////

static inline _syscall3(SYS_NETWORK_RECEIVE_IPv4_PKT, int, syscall_receive_ipv4_packet, void*, buf, uint, len, uint, timeout_sec)

static inline _syscall3(SYS_PREP_ICMP_PKT, int, syscall_prep_icmp_packet, icmp_opt*, opt, void*, buf, uint16_t, buf_len)
static inline _syscall3(SYS_SEND_ICMP_PKT, int, syscall_send_icmp_packet, icmp_opt*, opt, void*, buf, uint16_t, pkt_len)


#endif