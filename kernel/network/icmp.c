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

int icmp_prep_pkt(icmp_opt* opt, void* buf, uint16_t buf_len) {
    if(buf_len < sizeof(icmp_header)) {
        return -1;
    }
    icmp_header* hdr = (icmp_header*) buf;
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
    return sizeof(icmp_header);
}

// int icmp_send_pkt(int header_offset, void* buf, uint16_t pkt_len) {
//     if(header_offset < 0) return -1;
//     icmp_header* hdr = (icmp_header*) (buf + header_offset);
//     hdr->checksum = ipv4_icmp_checksum(hdr, pkt_len - header_offset);
//     int res = ipv4_send_pkt(buf, pkt_len);
//     return res;
// }

int icmp_finalize_pkt(icmp_header* hdr, uint16_t pkt_len)
{
    hdr->checksum = ipv4_icmp_checksum(hdr, pkt_len);
    return 0;
}

// icmp_opt* new_icmp_opt()
// {
//     icmp_opt* opt = malloc(sizeof(icmp_opt));
//     memset(opt, 0, sizeof(icmp_opt));
//     opt->ipv4.protocol = IPv4_PROTOCAL_ICMP;
//     return opt;
// }