#include <kernel/icmp.h>
#include <kernel/ipv4.h>
#include <kernel/rtl8139.h>
#include <kernel/lock.h>
#include <kernel/panic.h>
#include <string.h>
#include <stdlib.h>

int init_icmp()
{
    return 0;
}

int icmp_process_packet(void* buf, uint16_t len)
{
    UNUSED_ARG(buf);
    UNUSED_ARG(len);
    return 0;
}

int icmp_prep_pkt(icmp_opt* opt, void* buf, uint16_t buf_len) {
    ipv4_opt ipv4 = {
        .dst = opt->ipv4.dst, 
        .protocol = IPv4_PROTOCAL_ICMP, 
        .ttl = opt->ipv4.ttl,
        .data_len =  sizeof(icmp_header) + opt->data_len
        };
    int len_hdr = ipv4_prep_pkt(&ipv4, buf, buf_len);
    if(len_hdr < 0) {
        return len_hdr;
    }
    if(buf_len < len_hdr + sizeof(icmp_header)) {
        return -1;
    }
    icmp_header* hdr = (icmp_header*) (buf + len_hdr);
    *hdr = (icmp_header) {
        .code = opt->code,
        .type = opt->type,
    };
    if(opt->type == ICMP_TYPE_ECHO_REQUEST) {
        hdr->rest.un.echo.id = switch_endian16(opt->rest.un.echo.id);
        hdr->rest.un.echo.sequence = switch_endian16(opt->rest.un.echo.sequence);
    } else {
        return -1;
    }
    return len_hdr + sizeof(icmp_header);
}

int icmp_send_pkt(icmp_opt* opt, void* buf, uint16_t pkt_len) {
    icmp_header* hdr = (icmp_header*) (buf + pkt_len - opt->data_len - sizeof(icmp_header));
    hdr->checksum = ipv4_icmp_checksum(hdr, sizeof(icmp_header) + opt->data_len);
    int res = ipv4_send_pkt(buf, pkt_len);
    return res;
}