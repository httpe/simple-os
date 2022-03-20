#include <network.h>
#include <syscall.h>
#include <common.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


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

	int s = syscall_socket_open(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	int r = 0;
	struct timeval tv = {.tv_sec = 5};
	uint8_t ttl = 0x40, type = ICMP_TYPE_ECHO_REQUEST, code = ICMP_CODE_ECHO;
	uint16_t echo_id =  getpid(), echo_seq = 0;
	r += syscall_socket_setopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
	r += syscall_socket_setopt(s, SOL_IP, IP_TTL, &ttl, sizeof(uint8_t));
	r += syscall_socket_setopt(s, SOL_ICMP, ICMP_TYPE, &type, sizeof(uint8_t));
	r += syscall_socket_setopt(s, SOL_ICMP, ICMP_CODE, &code, sizeof(uint8_t));
	r += syscall_socket_setopt(s, SOL_ICMP, ICMP_ECHO_ID, &echo_id, sizeof(uint16_t));
	r += syscall_socket_setopt(s, SOL_ICMP, ICMP_ECHO_SEQ, &echo_seq, sizeof(uint16_t));
	if(r<0) {
		printf("Set socket opt failed\n");
		exit(-r);
	}

	char* buff = malloc(65535);
	memset(buff, 0, 65535);
	const char* msg = "A ping packet from Simple-OS!";
	memmove(buff, msg, strlen(msg));

	sockaddr_in socket_dst = {.sa_family = AF_INET, .sin_addr.s_addr = *(uint32_t*) dst.addr};
	r = syscall_socket_sendto(s, buff, strlen(msg), 0, (sockaddr*) &socket_dst, sizeof(sockaddr_in));

	uint8_t* receive_buff = malloc(65535);
	sockaddr_in socket_src;
	while(1) {
		socklen_t address_len = 0;
		int received = syscall_socket_recvfrom(s, receive_buff, 65535, 0, (sockaddr*) &socket_src, &address_len);
		if(received < 0) {
			printf("Ping: error in receiving packet\n");
			exit(1);
		}

		if(received < (int) (sizeof(icmp_header) + strlen(msg))) {
			printf("Ping: Skipping packet too small\n");
			continue;
		}

		icmp_header* p = (icmp_header*) receive_buff;
		if(p->type != ICMP_TYPE_ECHO_REPLY || p->code != ICMP_CODE_ECHO || 
			p->rest.un.echo.id != switch_endian16(echo_id) || p->rest.un.echo.sequence != switch_endian16(echo_seq)
		) {
			printf("Ping: Skipping other ICMP packet\n");
			continue;
		}
		
		if(memcmp(p + 1, msg, received - sizeof(icmp_header)) == 0) {
			printf("Ping: GOOD reply received!\n");
			exit(0);
		} else {
			printf("Ping: reply corrupted, data: \n%s\n", (char*) (p+1));
			exit(1);
		}
	}
	
}
