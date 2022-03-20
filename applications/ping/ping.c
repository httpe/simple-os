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

	icmp_opt icmp = {
		.type = ICMP_TYPE_ECHO_REQUEST,
		.code = ICMP_CODE_ECHO,
		.rest.un.echo.id = getpid(),
		.rest.un.echo.sequence = 0,
	};

	ipv4_opt ipv4 = {
		.dst = dst,
		.ttl = 0x40,
		.protocol = IPv4_PROTOCAL_ICMP
	};

	uint buf_len = 65535;
	void* transmit_buff = malloc(buf_len);
	memset(transmit_buff, 0, buf_len);

	int ipv4_hdr_len = syscall_prep_ipv4_packet(&ipv4, transmit_buff, buf_len);
	if(ipv4_hdr_len < 0) {
		printf("Ping: Prepare IPv4 packet error\n");
		exit(-ipv4_hdr_len);
	}
	int icmp_hdr_len = syscall_prep_icmp_packet(&icmp, transmit_buff + ipv4_hdr_len, buf_len - ipv4_hdr_len);
	if(icmp_hdr_len < 0) {
		printf("Ping: Prepare ICMP packet error\n");
		exit(-icmp_hdr_len);
	}
	const char* msg = "A ping packet from Simple-OS!";
	uint data_len = strlen(msg);
	memmove(transmit_buff + ipv4_hdr_len + icmp_hdr_len, msg, data_len);
	int r_finalize_icmp = syscall_finalize_icmp_packet(transmit_buff + ipv4_hdr_len, icmp_hdr_len + data_len);
	if(r_finalize_icmp < 0) {
		printf("Ping: Finalize ICMP packet error\n");
		exit(-r_finalize_icmp);
	}
	int r_send = syscall_send_ipv4_packet(transmit_buff, ipv4_hdr_len + icmp_hdr_len + data_len);
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

		if(received < (int) (sizeof(ipv4_header) + sizeof(icmp_header) + data_len)) {
			printf("Ping: Skipping packet too small\n");
			continue;
		}

		icmp_header* p = (icmp_header*) (receive_buff + sizeof(ipv4_header));
		if(p->type != ICMP_TYPE_ECHO_REPLY || p->code != ICMP_CODE_ECHO || 
			p->rest.un.echo.id != switch_endian16(icmp.rest.un.echo.id) || p->rest.un.echo.sequence != switch_endian16(icmp.rest.un.echo.sequence)
		) {
			printf("Ping: Skipping other ICMP packet\n");
			continue;
		}
		
		if(memcmp(p + 1, transmit_buff + ipv4_hdr_len + icmp_hdr_len, data_len) == 0) {
			printf("Ping: GOOD reply received!\n");
			exit(0);
		} else {
			printf("Ping: reply corrupted, data: \n%s\n", (char*) (p+1));
			exit(1);
		}
		
	}

	
}
