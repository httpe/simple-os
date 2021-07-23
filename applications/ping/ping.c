#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <common.h>
#include <network.h>


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

	icmp_opt opt = {
		.type = ICMP_TYPE_ECHO_REQUEST,
		.code = ICMP_CODE_ECHO,
		.ipv4.dst = dst,
		.ipv4.ttl = 0x40,
		.rest.un.echo.id = getpid(),
		.rest.un.echo.sequence = 0,
		.data_len = 56
	};

	char* transmit_buff = malloc(65535);
	memset(transmit_buff, 0, 65535);
	int hdr_len = syscall_prep_icmp_packet(&opt, transmit_buff, 65535);
	if(hdr_len < 0) {
		printf("Ping: Prepare packet error\n");
		exit(-hdr_len);
	}
	const char* msg = "A ping packet from Simple-OS!";
	memmove(transmit_buff + hdr_len, msg, strlen(msg));
	int r_send = syscall_send_icmp_packet(&opt, transmit_buff, hdr_len + opt.data_len);
	if(r_send < 0) {
		printf("Ping: Send error\n");
		exit(-r_send);
	}

	uint8_t* receive_buff = malloc(65535);
	while(1) {
		int received = syscall_receive_ipv4_packet(receive_buff, 65535, 5);
		if(received < 0) {
			printf("Ping: Receive error\n");
			exit(-received);
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

		if(received < (int) (sizeof(ipv4_header) + sizeof(icmp_header) + opt.data_len)) {
			printf("Ping: Skipping packet too small\n");
			continue;
		}

		icmp_header* p = (icmp_header*) (receive_buff + sizeof(ipv4_header));
		if(p->type != ICMP_TYPE_ECHO_REPLY || p->code != ICMP_CODE_ECHO || 
			p->rest.un.echo.id != switch_endian16(opt.rest.un.echo.id) || p->rest.un.echo.sequence != switch_endian16(opt.rest.un.echo.sequence)
		) {
			printf("Ping: Skipping other ICMP packet\n");
			continue;
		}
		
		if(memcmp(p + 1, transmit_buff + hdr_len, opt.data_len) == 0) {
			printf("Ping: GOOD reply received!\n");
			exit(0);
		} else {
			printf("Ping: reply corrupted, data: \n%s\n", (char*) (p+1));
			exit(1);
		}
		
	}

	
}
