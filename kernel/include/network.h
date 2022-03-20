#ifndef _NETWORK_H
#define _NETWORK_H

#include <kernel/types.h>
#include <stdint.h>
#include <common.h>
#include <syscall.h>

// in sync with the RTL8139 RTL8139_TRANSMIT_BUF_SIZE 
#define NETWORK_TRANSMIT_BUF_SIZE 1792

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
    ip_addr src; // ignored when sending, will use MY_IP instead
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
    enum icmp_type type;
    enum icmp_code code;
    icmp_rest_of_header rest;
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
// Socket
//
///////////////////////////////


// // Socket Domain/Address Family
// #define AF_UNSPEC 0
// #define AF_INET 1

// // Socket type
// #define SOCK_RAW 0
// #define SOCK_DGRAM 1
// #define SOCK_STREAM 2

// // Protocal type (from Linux <netinet/in.h>)
// #define IPPROTO_IP 0          /* Dummy protocol (TCP for SOCK_STREAM, UDP for SOCK_DGRAM, raw IPv4 protocol for SOCK_RAW) */
// #define IPPROTO_ICMP 1        /* Internet Control Message Protocol.  */
// #define IPPROTO_TCP 6         /* Transmission Control Protocol.  */
// #define IPPROTO_UDP 17        /* User Datagram Protocol.  */
// #define IPPROTO_RAW 255       /* Raw IP packets (user provides IPv4 header manually)  */

// // From Linux <sys/socket.h>
// /* Setsockoptions(2) level. Thanks to BSD these must match IPPROTO_xxx */
// #define SOL_IP		0
// #define SOL_ICMP	1     // linux does not have this
// #define SOL_TCP		6
// #define SOL_UDP		17
// #define SOL_RAW		255

// #define SOL_SOCKET 256  // linux is 1


// // SOL_SOCKET level options
// struct timeval {
//   time_t         tv_sec;      // seconds
//   suseconds_t    tv_usec;     // microseconds
// };
// #define SO_RCVTIMEO 1         // receive timeout, struct timeval

// // SOL_IP level options
// // https://man7.org/linux/man-pages/man7/ip.7.html
// #define IP_TTL 1              // uint8_t
// #define IP_PROTOCOL 2         // uint8_t

// // SOL_ICMP level options
// #define ICMP_TYPE 1           // uint8_t
// #define ICMP_CODE 2           // uint8_t
// #define ICMP_ECHO_ID 3        // uint16_t
// #define ICMP_ECHO_SEQ 4       // uint16_t


// typedef int32_t socklen_t;
// typedef uint32_t sa_family_t;

// typedef struct sockaddr {
//     sa_family_t  sa_family; // AF_*
//     char         sa_data[14];
// } sockaddr;

// // From POSIX netinet/in.h
// typedef uint16_t in_port_t;
// typedef uint32_t in_addr_t;
// typedef struct  in_addr {
//   in_addr_t  s_addr;
// } in_addr;
// typedef struct sockaddr_in {
//     sa_family_t  sa_family; // AF_INET
//     in_port_t       sin_port;
//     struct  in_addr sin_addr;
//     char            sin_zero[8];
// } sockaddr_in;

// #define INADDR_ANY 0
// #define INADDR_BROADCAST 0xFFFFFFFF

///////////////////////////////
//
// Network System Calls
//
//////////////////////////////


// static inline _syscall3(SYS_SOCKET_OPEN, int, syscall_socket_open, int, domain, int, type, int, protocol)
// static inline _syscall5(SYS_SOCKET_SETOPT, int, syscall_socket_setopt, int, socket, int, level, int, option_name, void*, option_value, socklen_t, option_len)
// static inline _syscall6(SYS_SOCKET_SENDTO, int, syscall_socket_sendto, int, socket, void*, message, size_t, length, int, flags, sockaddr*, dest_addr, socklen_t, dest_len)
// static inline _syscall6(SYS_SOCKET_RECVFROM, int, syscall_socket_recvfrom, int, socket, void*, buffer, size_t, length, int, flags, sockaddr*, address, socklen_t, address_len)

static inline _syscall3(SYS_NETWORK_RECEIVE_IPv4_PKT, int, syscall_receive_ipv4_packet, void*, buf, uint, len, uint, timeout_sec)

static inline _syscall3(SYS_PREP_ICMP_PKT, int, syscall_prep_icmp_packet, icmp_opt*, opt, void*, buf, uint16_t, buf_len)
static inline _syscall3(SYS_PREP_IPV4_PKT, int, syscall_prep_ipv4_packet, ipv4_opt*, opt, void*, buf, uint16_t, buf_len)
static inline _syscall2(SYS_FINALIZE_ICMP_PKT, int, syscall_finalize_icmp_packet, icmp_header*, hdr, uint16_t, pkt_len)
static inline _syscall2(SYS_SEND_IPV4_PKT, int, syscall_send_ipv4_packet, void*, buf, uint16_t, pkt_len)




#endif