#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <common.h>
#include <network.h>

// static inline _syscall2(SYS_TEST, int, sys_send_network_packet, void*, buf, uint, size)
// static inline _syscall4(SYS_TEST, int, send_ethernet_packet, mac_addr*, dest, enum ether_type, type, void*, buf, uint, size)
// static inline _syscall5(SYS_TEST, int, send_ipv4_packet, uint, ttl, enum ipv4_protocol, protocol, ip_addr*, dst, void*, buf, uint, len)


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

	ping_packet pkt = (ping_packet) {
		.type = ICMP_TYPE_ECHO_REQUEST,
		.code = ICMP_CODE_ECHO,
		.identifier = switch_endian16(getpid()),
		.sequence_number = 0,
		.data = "A ping packet from Simple-OS!"
	};

	pkt.checksum = ipv4_icmp_checksum(&pkt, sizeof(ping_packet));

	uint8_t* receive_buff = malloc(65535);

	int r = syscall_send_ipv4_packet(0x40, IPv4_PROTOCAL_ICMP, &dst, &pkt, sizeof(ping_packet));
	if(r < 0) {
		printf("Ping: Send error\n");
		exit(-r);
	}

	while(1) {
		int received = syscall_receive_ipv4_packet(receive_buff, 65535);
		if(received < 0) {
			printf("Ping: Receive error\n");
			exit(-r);
		}

		ipv4_header* hdr = (ipv4_header*) receive_buff;
		if(memcmp(hdr->src.addr,  dst.addr, sizeof(dst.addr)) != 0) {
			printf("Ping: Skipping packet not from target\n");
			continue;
		}
		if(hdr->protocol != IPv4_PROTOCAL_ICMP) {
			printf("Ping: Skipping non-ICMP packet\n");
			continue;
		}

		if(received < (int) (sizeof(ipv4_header) + sizeof(ping))) {
			printf("Ping: Skipping packet too small\n");
			continue;
		}

		ping* p = (ping*) (receive_buff + sizeof(ipv4_header));
		if(p->type != ICMP_TYPE_ECHO_REPLY || p->code != ICMP_CODE_ECHO || 
			p->identifier != pkt.identifier || p->sequence_number != pkt.sequence_number
		) {
			printf("Ping: Skipping other ICMP packet\n");
			continue;
		}
		
		if(memcmp(p->data, pkt.data, PING_DATA_SIZE) == 0) {
			printf("Ping: GOOD reply received!\n");
			exit(0);
		} else {
			printf("Ping: reply corrupted!\n");
			exit(1);
		}
		
	}

	
}
