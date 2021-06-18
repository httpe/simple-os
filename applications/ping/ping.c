#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <common.h>
#include <network.h>

// static inline _syscall2(SYS_TEST, int, sys_send_network_packet, void*, buf, uint, size)
// static inline _syscall4(SYS_TEST, int, send_ethernet_packet, mac_addr*, dest, enum ether_type, type, void*, buf, uint, size)
static inline _syscall5(SYS_TEST, int, send_ipv4_packet, uint, ttl, enum ipv4_protocal, protocol, ip_addr*, dst, void*, buf, uint, len)

typedef struct ping {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence_number;
	char data[56];
} ping;

int main(int argc, char* argv[]) {

	ip_addr dst = {.addr = {8,8,8,8}};

	if(argc > 1) {
		char* part = strtok(argv[1],".");
		for(int i=0; i<4; i++) {
			if(part == NULL) {
				exit(1);
			}
			dst.addr[i] = atoi(part);
			part = strtok(NULL,".");
		}
	}

	ping pkt = (ping) {
		.type = ICMP_TYPE_ECHO,
		.code = ICMP_CODE_ECHO,
		.identifier = getpid(),
		.sequence_number = 0,
		.data = "A ping packet from Simple-OS!"
	};

	pkt.checksum = ipv4_icmp_checksum(&pkt, sizeof(ping));

	int r = send_ipv4_packet(0x40, IPv4_PROTOCAL_ICMP, &dst, &pkt, sizeof(ping));

    exit(r);
}
