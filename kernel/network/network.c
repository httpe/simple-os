#include <network.h>
#include <kernel/network.h>
#include <kernel/ethernet.h>
#include <kernel/arp.h>
#include <kernel/ipv4.h>

int init_network()
{
    init_ethernet();
    init_arp();
    init_ipv4();
    return 0;
}
