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

#define IP_FRAGMENTED 65343

/* eBPF requires all functions to be inlined */
#define INTERNAL static __attribute__((always_inline))

/* Log only in debug. */
#ifndef NDEBUG
#define LOG(fmt, ...) bpf_printk(fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

/* eBPF lacks these functions, but LLVM provides builtins */
#ifndef memset
#define memset(dest, chr, n)   __builtin_memset((dest), (chr), (n))
#endif

#ifndef memcpy
#define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

#ifndef memmove
#define memmove(dest, src, n)  __builtin_memmove((dest), (src), (n))
#endif

#ifndef SERVERS_COUNT
#define SERVERS_COUNT (2)
#endif

#ifndef MAX_CLIENS_COUNT
#define MAX_CLIENS_COUNT (512)
#endif

#ifndef OFFSET_UDP_PORT
#define OFFSET_UDP_PORT (10000)
#endif

#define MAX_UDP_LENGTH (1480)

struct datarec {
	__u32 ip_src;
	unsigned char mac_src[ETH_ALEN];
    unsigned short port;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, __u32);
	__type(value, struct datarec);
	__uint(max_entries, SERVERS_COUNT);
} servers_addr SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, __u32);
	__type(value, __u16);
	__uint(max_entries, 1);
} g_index_port SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u32);
	__type(value, __u32);
	__uint(max_entries, SERVERS_COUNT);
} servers_ip SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u16);
	__type(value, struct datarec);
	__uint(max_entries, MAX_CLIENS_COUNT);
} clients_port SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, __u64);
	__type(value, struct datarec);
	__uint(max_entries, MAX_CLIENS_COUNT);
} clients_ip_and_port SEC(".maps");


static __always_inline void swapeth(struct ethhdr *eth,unsigned  char dist_mac[ETH_ALEN])
{
    memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
    memcpy(eth->h_dest, dist_mac, ETH_ALEN);
}


static __always_inline __u16 ip_checksum(unsigned short *buf, int bufsz) {
    unsigned long sum = 0;

    while (bufsz > 1) {
        sum += *buf;
        buf++;
        bufsz -= 2;
    }

    if (bufsz == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum & 0xffff) + (sum >> 16);
    sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

static __always_inline __u16 udp_checksum(struct iphdr *ip, struct udphdr * udp, void * data_end)
{
    __u16 csum = 0;
    __u16 *buf = (__u16*)udp;

    // Compute pseudo-header checksum
    csum += ip->saddr;
    csum += ip->saddr >> 16;
    csum += ip->daddr;
    csum += ip->daddr >> 16;
    csum += (__u16)ip->protocol << 8;
    csum += udp->len;

    // Compute checksum on udp header + payload
    for (int i = 0; i < MAX_UDP_LENGTH; i += 2) {
        if ((void *)(buf + 1) > data_end) {
            break;
        }
        csum += *buf;
        buf++;
    }

    if ((void *)buf + 1 <= data_end) {
       // In case payload is not 2 bytes aligned
        csum += *(__u8 *)buf;
    }

   csum = ~csum;
   return csum;
}

INTERNAL int
process_packet(struct xdp_md *ctx, __u32 off)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth;
    struct iphdr *iph;
    __u8 protocol;

    eth = data;
    iph = data + off;
    protocol = iph->protocol;
    if ( (iph + 1) > (struct iphdr *)data_end)
        return XDP_DROP;

    if (iph->ihl != 5)
        return XDP_DROP;

  
    /* do not support fragmented packets as L4 headers may be missing */
    if (iph->frag_off & IP_FRAGMENTED)
        return XDP_DROP;

    if (protocol != IPPROTO_UDP) {
        return XDP_PASS;
    }

    struct udphdr *udph = data + sizeof(struct ethhdr) + (iph->ihl * 4);
    // Check header.
    if (udph + 1 > (struct udphdr *)data_end)
    {
        return XDP_DROP;
    }

    struct datarec *rec;

    LOG("IN IP (src=%pI4 dst=%pI4)",
        &iph->saddr, &iph->daddr);
    LOG("UDP (dst=%u src=%u)",
         udph->dest,udph->source);

    __u32  * is_server = 0;
    is_server = bpf_map_lookup_elem(&servers_ip, &iph->saddr);
    if (!is_server){
        LOG("Request from client");

        __u32 server_index = bpf_htonl(iph->saddr) % SERVERS_COUNT;
        LOG("Choose server (server_index=%d IP=%pI4)",
            server_index, (&iph->saddr));


        rec = bpf_map_lookup_elem(&servers_addr, &server_index);
        if (!rec)
            return XDP_PASS;

        __u64 ip_and_port = (iph->saddr<<16) | udph->source;
        LOG("ip_and_port val = %d",ip_and_port);
        struct datarec *old_client;
        old_client = bpf_map_lookup_elem(&clients_ip_and_port, &ip_and_port);
        if(!old_client){
            LOG("Request from new client");

            struct datarec new_client;
            new_client.ip_src = iph->saddr;
            memcpy(new_client.mac_src, eth->h_source, ETH_ALEN);
            
            __u32 key = 0;
            __u16 port = 0;
            __u16 * g_index = bpf_map_lookup_elem(&g_index_port, &key);
            if (!g_index)
                return XDP_PASS;


            port=*g_index+OFFSET_UDP_PORT;
            __u32 increment =*g_index;
            increment++;
            increment%=MAX_CLIENS_COUNT;
            bpf_map_update_elem(&g_index_port, &key, &increment, 0);


            // Check to need delete element
            struct datarec *previos_client =0;
            previos_client = bpf_map_lookup_elem(&clients_port, &port);
            if(previos_client){
                __u64 old_ip_and_port = (iph->saddr<<16) & udph->source;
                bpf_map_delete_elem(&clients_ip_and_port,&old_ip_and_port);
            }

            new_client.port=port;
            bpf_map_update_elem(&clients_ip_and_port, &ip_and_port, &new_client, 0);

            new_client.port =udph->source;
            bpf_map_update_elem(&clients_port, &port, &new_client, 0);

            udph->source=port;
            goto out;

        }
        udph->source = old_client->port;
        goto out;
    }

    LOG("Answer from server");

    rec = bpf_map_lookup_elem(&clients_port, &udph->dest);
    if (!rec){
        LOG("Error - Answer from server src=%d",udph->dest);
        return XDP_PASS;
    }

    udph->dest = rec->port;

out:

    iph->saddr = iph->daddr;
    iph->daddr = rec->ip_src;

    // Swap ethernet source and destination MAC addresses.
    swapeth(eth,rec->mac_src); 
  
    LOG("OUT IP (src=%pI4 dst=%pI4)",
        &iph->saddr, &iph->daddr);
    LOG("UDP (dst=%u src=%u)",
         udph->dest,udph->source);

    /* Update checksums */
    iph->check = 0;
    iph->check = ip_checksum((__u16 *)iph, sizeof(struct iphdr));
    udph->check = 0;
    udph->check = udp_checksum(iph, udph, data_end);
    udph->check = 0;

    return XDP_TX;
}


SEC("prog")
int xdp_balance(struct xdp_md *ctx)
{
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;
    struct ethhdr *eth = data;
    __u32 eth_proto;
    __u32 nh_off;

    nh_off = sizeof(struct ethhdr);
    if (data + nh_off > data_end)
        return XDP_DROP;
    eth_proto = eth->h_proto;

    /* demo program only accepts  ipv4/UDP packets */
    if (eth_proto == bpf_htons(ETH_P_IP)){
        LOG(" ==== NEW PACKET === ");
        return process_packet(ctx, nh_off);
    }
    else
        return XDP_PASS;
}


char _license[] SEC("license") = "GPL";