#include <kernel/socket.h>
#include <kernel/ipv4.h>
#include <kernel/icmp.h>
#include <kernel/errno.h>
#include <kernel/process.h>
#include <kernel/panic.h>
#include <kernel/lock.h>
#include <stdlib.h>
#include <string.h>

static struct {
	socket_descriptor sockets[MAX_SOCKET_OPENED];
	yield_lock lk;
} global;

static void add_pkt_to_cache(socket_descriptor* psd, struct sockaddr* src, socklen_t src_len, void* pkt, uint16_t pkt_len)
{
	pkt_cache* new_cache = malloc(sizeof(pkt_cache));
	*new_cache = (pkt_cache) {
		.buff = malloc(pkt_len),
		.next = NULL,
		.pkt_len = pkt_len,
		.src =  *src,
		.src_len = src_len
	};
	memmove(new_cache->buff, pkt, pkt_len);
	if(psd->cache == NULL) {
		psd->cache = new_cache;
	} else {
		new_cache->next = psd->cache;
		psd->cache = new_cache;
	}
}

static pkt_cache* free_cache(pkt_cache* cache)
{
	free(cache->buff);
	pkt_cache* next = cache->next;
	free(cache);
	return next;
}

int socket(int domain, int type, int protocol)
{
	if(domain != AF_INET) {
		return -EAFNOSUPPORT;
	}

	if(type == SOCK_RAW) {
		if(!(protocol == IPPROTO_RAW || protocol == IPPROTO_IP)) {
			// SOCK_RAW + IPPROTO_RAW = IPv4 pkt with user supplied IPv4 header
			// SOCK_RAW + IPPROTO_IP = IPv4 pkt with system generated IPv4 header (from socket options)
			return -EPROTONOSUPPORT;
		}
	} else if(type == SOCK_DGRAM) {
		if(protocol != IPPROTO_ICMP) {
			return -EPROTONOSUPPORT;
		}
	} else {
		return -EPROTOTYPE;
	}

	int socket_idx;
	socket_descriptor* psd = NULL;
	acquire(&global.lk);
	for(socket_idx=0; socket_idx<MAX_SOCKET_OPENED; socket_idx++) {
		psd = &global.sockets[socket_idx];
		if(psd->ref == 0) {
			memset(psd, 0, sizeof(*psd));
			void* transmit_buff = malloc(NETWORK_TRANSMIT_BUF_SIZE);
			memset(transmit_buff, 0 , NETWORK_TRANSMIT_BUF_SIZE);
			*psd = (socket_descriptor) {
				.domain = domain,
				.type = type,
				.protocol = protocol,
				.ref = 1,
				.cache = NULL,
				.transmit_buff = transmit_buff,
			};
			
			PANIC_ASSERT(protocol == IPPROTO_ICMP);
			if(protocol == IPPROTO_ICMP) {
				psd->ip_opt.protocol = IPv4_PROTOCAL_ICMP;
				icmp_opt* opt = malloc(sizeof(icmp_opt));
				memset(opt, 0, sizeof(icmp_opt));
				psd->protocol_opt = opt;
			}

			break;
		}
		if(socket_idx == MAX_SOCKET_OPENED-1) {
			release(&global.lk);
			return -ENFILE;
		}
	}
    release(&global.lk);
	return 0;
}

static struct socket_descriptor* get_socket(int socket)
{
	if(socket < 0 || socket >= MAX_SOCKET_OPENED) return NULL;
	return &global.sockets[socket];
}


int close_socket(int socket)
{
	acquire(&global.lk);
	struct socket_descriptor* psd = get_socket(socket);
	if(psd == NULL) {
		release(&global.lk);
		return -1;
	}
	psd->ref--;
	if(psd->ref == 0) {
		free(psd->transmit_buff);
		psd->transmit_buff = NULL;
		pkt_cache* cache = psd->cache;
		while(cache) {
			cache = free_cache(cache);
		}
		if(psd->protocol_opt != NULL) {
			free(psd->protocol_opt);
		}
		psd->transmit_buff = NULL;
	}
	release(&global.lk);
	return 0;
}

// static struct in_addr ip_addr2in_addr(ip_addr addr)
// {
// 	return (struct in_addr) {.s_addr = *(uint32_t*) addr.addr};
// }

// static ip_addr in_addr2ip_addr(in_addr* addr)
// {
// 	struct ip_addr ip;
// 	(* (uint32_t*) ip.addr) = addr->s_addr;
// 	return ip;
// }




// static int ip_pass_filter(intip_addr ip, ip_addr filter)
// {
// 	return (memcmp(&ip, &INADDR_BROADCAST, sizeof(ip_addr)) == 0) || 
// 			(memcmp(&filter, &INADDR_ANY, sizeof(ip_addr)) == 0) ||
// 			(memcmp(&ip, &filter, sizeof(ip_addr)) == 0);
// }



static int pkt_in_scope(int protocol, socket_descriptor* psd)
{
	// if(addr->sa_family != AF_INET) PANIC('Unsupported address family');
	// if(psd->filter.sa_family != AF_INET) PANIC('Unsupported address family');

	if(psd->type == SOCK_RAW || psd->protocol == IPPROTO_RAW || psd->protocol == IPPROTO_IP) {
		// for raw socket or IP protocol sockets, all packets are in scope 
		return 1;
	}
	if(psd->protocol != protocol) return 0;
	
	if(psd->protocol == IPPROTO_ICMP) {
		// do not filter ICMP packet based on e.g id and seq
		// leave it to the userspace 
		return 1;
	} else {
		// unsupported protocol
		return 0;
	}

	// struct sockaddr_in* addr_in = (struct sockaddr_in*) addr;
	// struct sockaddr_in* filter_in = (struct sockaddr_in*) &psd->filter;
	// // Match IP addr
	// if(
	// 	(addr_in->sin_addr.s_addr == INADDR_BROADCAST) ||
	// 	(filter_in->sin_addr.s_addr == INADDR_ANY) ||
	// 	(addr_in->sin_addr.s_addr == filter_in->sin_addr.s_addr)
	//  ) {
	// 	 // Match port (or if protocal has no port concept)
	// 	if(protocol == IPPROTO_RAW || protocol == IPPROTO_ICMP || addr_in->sin_port == filter_in->sin_port) {
	// 		return 1;
	// 	}
	// }

	return 0;
}

// static struct sockaddr* dup_addr(struct sockaddr* addr)
// {
// 	if(addr->sa_family != AF_INET) PANIC('Unsupported address family');
// 	struct sockaddr* new_addr = malloc(sizeof(struct sockaddr));
// 	*new_addr = *addr;
// 	return new_addr;
// }



int socket_process_pkt(int protocol, void* pkt, uint16_t pkt_len)
{
	int processed = 0;
	acquire(&global.lk);

	ipv4_opt opt;
	ipv4_opt_from_header(pkt, &opt);

	for(int socket_idx=0; socket_idx<MAX_SOCKET_OPENED; socket_idx++) {
		socket_descriptor* psd = &global.sockets[socket_idx];
		if(psd->ref == 0) continue;
		if(!pkt_in_scope(protocol, psd)) continue;

		struct sockaddr_in src;
		socklen_t src_len = sizeof(struct sockaddr_in); //TODO: correct?
		src = (struct sockaddr_in) {
				.sa_family = AF_INET, 
				.sin_addr = (struct in_addr) {.s_addr = *(uint32_t*) opt.src.addr},
				.sin_zero = {0}
		};

		int hdr_len;
		if(psd->protocol == IPPROTO_RAW) {
			hdr_len = 0;
		} else if(psd->protocol == IPPROTO_IP) {
			hdr_len = sizeof(ipv4_header);
		} else if(psd->protocol == IPPROTO_ICMP) {
			hdr_len = sizeof(ipv4_header);
		} else {
			PANIC("No such socket protocol");
		}
		if(pkt_len < hdr_len) continue;
		add_pkt_to_cache(psd, (struct sockaddr*) &src, src_len, pkt + hdr_len, pkt_len - hdr_len);

		processed++;
	}
	release(&global.lk);
	return processed;
}



ssize_t sendto(int socket, const void *message, size_t length,
       int flags, const struct sockaddr *dest_addr,
       socklen_t dest_len)
{
	UNUSED_ARG(flags);
	UNUSED_ARG(dest_len);

	struct socket_descriptor* psd = get_socket(socket);
	if(psd == NULL) return -1;
	sockaddr_in* dest = (sockaddr_in*) dest_addr;
	if(dest->sa_family != AF_INET) return -1;
	
	if(psd->type == SOCK_RAW) {
		if(psd->protocol == IPPROTO_RAW) {
			int r_hdr = ipv4_opt_from_header(message, &psd->ip_opt);
			if(r_hdr < 0) return -1;
			int hdr_len = ipv4_prep_pkt(&psd->ip_opt, psd->transmit_buff, NETWORK_TRANSMIT_BUF_SIZE);
			if(hdr_len < 0) return -1;
			memmove(psd->transmit_buff + hdr_len, message + hdr_len, length - hdr_len);
			int r = ipv4_send_pkt(psd->transmit_buff, length);
			return r;
		} else if(psd->protocol == IPPROTO_IP) {
			psd->ip_opt.dst = *(ip_addr*) &dest->sin_addr;
			int hdr_len = ipv4_prep_pkt(&psd->ip_opt, psd->transmit_buff, NETWORK_TRANSMIT_BUF_SIZE);
			if(hdr_len < 0) return -1;
			memmove(psd->transmit_buff + hdr_len, message, length);
			int r = ipv4_send_pkt(psd->transmit_buff, hdr_len + length);
			return r;
		} else {
			return -1;
		}
	} else if(psd->type == SOCK_DGRAM) {
		if(psd->protocol == IPPROTO_ICMP) {
			psd->ip_opt.dst = *(ip_addr*) &dest->sin_addr;
			int ip_hdr_len = ipv4_prep_pkt(&psd->ip_opt, psd->transmit_buff, NETWORK_TRANSMIT_BUF_SIZE);
			if(ip_hdr_len < 0) return -1;
			int icmp_hdr_len = icmp_prep_pkt((icmp_opt*) psd->protocol_opt, psd->transmit_buff + ip_hdr_len, NETWORK_TRANSMIT_BUF_SIZE - ip_hdr_len);
			if(icmp_hdr_len < 0) return -1;
			if(ip_hdr_len + icmp_hdr_len + length > NETWORK_TRANSMIT_BUF_SIZE) return -1;
			memmove(psd->transmit_buff + ip_hdr_len + icmp_hdr_len, message, length);
			int r_icmp = icmp_finalize_pkt(psd->transmit_buff + ip_hdr_len, icmp_hdr_len + length);
			if(r_icmp < 0) return -1;
			int r = ipv4_send_pkt(psd->transmit_buff, ip_hdr_len + icmp_hdr_len + length);
			return r;
		} else {
			return -1;
		}
	} else {
		return -1;
	}
}

int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len)
{
	struct socket_descriptor* psd = get_socket(socket);
	if(psd == NULL) return -1;
	
	if(level == SOL_SOCKET) {
		if(option_name == SO_RCVTIMEO) {
			struct timeval* tv = (struct timeval*) option_value;
			psd->opt.recv_timeout = *tv;
			return 0;
		}
	} else if(level == SOL_IP || level == SOL_RAW) {
		if(option_name == IP_TTL && option_len == sizeof(uint8_t)) {
			psd->ip_opt.ttl = *(uint8_t*) option_value;
			return 0;
		} else if(option_name == IP_PROTOCOL && option_len == sizeof(uint8_t)) {
			psd->ip_opt.protocol = *(uint8_t*) option_value;
			return 0;
		}
	} else if(level == SOL_ICMP && psd->protocol == IPPROTO_ICMP) {
		icmp_opt* opt = (icmp_opt*) psd->protocol_opt;
		if(option_name == ICMP_TYPE && option_len == sizeof(uint8_t)) {
			opt->type = *(uint8_t*) option_value;
			return 0;
		} else if(option_name == ICMP_CODE && option_len == sizeof(uint8_t)) {
			opt->code = *(uint8_t*) option_value;
			return 0;
		} else if(option_name == ICMP_ECHO_ID && option_len == sizeof(uint16_t)) {
			opt->rest.un.echo.id = *(uint16_t*) option_value;
			return 0;
		} else if(option_name == ICMP_ECHO_SEQ && option_len == sizeof(uint16_t)) {
			opt->rest.un.echo.sequence = *(uint16_t*) option_value;
			return 0;
		}
	}
	return -1;
}

ssize_t recvfrom(int socket, void * buffer, size_t length,
       int flags, struct sockaddr * address,
       socklen_t * address_len)
{
	UNUSED_ARG(flags);

	struct socket_descriptor* psd = get_socket(socket);
	if(psd == NULL) return -1;
	
	acquire(&global.lk);
	while(psd->cache == NULL) {
		release(&global.lk);
		yield(); // this is an unconditional yield/sleep, so does not suffer the lost-wakeup problem
		acquire(&global.lk);
	}

	if(address && address_len) {
		*address = psd->cache->src;
		*address_len = psd->cache->src_len;
	}

	ssize_t written = 0;
	if(length < psd->cache->pkt_len) {
		memmove(buffer, psd->cache->buff, length);
		written = length;
	} else {
		memmove(buffer, psd->cache->buff, psd->cache->pkt_len);
		written = psd->cache->pkt_len;
	}
	
	psd->cache = free_cache(psd->cache);

	release(&global.lk);
	return written;

}