#include <stddef.h>
#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include "bpf/bpf_helpers.h"
#include "bpf/bpf_endian.h"
#include <linux/if_ether.h>
#include <linux/ip.h>
SEC("prog")
int xdp_main(struct xdp_md *ctx)
{
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";