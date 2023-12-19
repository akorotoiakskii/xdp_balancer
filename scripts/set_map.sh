#!/bin/bash

echo "=== Create netns and devs ===="

IP4_SUBNET=172.11.0.
NS="test_ns"
INSIDE_MAC=de:ad:be:ef:00:02
SERVER_COUNT=2

write_bpf_map()
{
    echo "  = Creating VLAN $1 ="
    local inside_ip="${IP4_SUBNET}2$1"
    local inside_mac=de:ad:be:ef:00:2$1

    local a b c d
    IFS=. read -r a b c d <<< "$inside_ip"
    printf "$a $b $c $d\n" 

    sudo bpftool map update name servers_addr key $1 0 0 0 value "$a" "$b" "$c"  "$d"   0xde 0xad 0xbe  0xef  0  2   0 0
    sudo bpftool map update name servers_ip key "$a" "$b" "$c" "$d" value 1 0 0 0
}


for (( counter=0; counter<SERVER_COUNT; counter++ ))
    do
    write_bpf_map $counter
done
