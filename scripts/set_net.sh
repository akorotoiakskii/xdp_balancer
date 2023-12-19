#!/bin/bash

echo "=== Create netns and devs ===="

IP4_FULL_PREFIX_SIZE=16 # Size of IP4_SUBNET
IP4_PREFIX_SIZE=24 # Size of assigned prefixes
IP4_SUBNET=172.11.0.
INSIDE_IP4="${IP4_SUBNET}2"
OUTSIDE_IP4="${IP4_SUBNET}1"
NS="test_ns"

INSIDE_MAC=de:ad:be:ef:00:02
OUTSIDE_MAC=de:ad:be:ef:00:01
VLAN_COUNT=6

ip netns add "$NS"
ip link add dev balancer type veth peer name veth netns "$NS"

# Root namespace
echo "  = Set Root namespace="
ip link set dev balancer up
ip addr add dev balancer "${OUTSIDE_IP4}/${IP4_PREFIX_SIZE}"
ethtool -K balancer rxvlan off txvlan off
ip link set dev balancer address "$OUTSIDE_MAC"
ip neigh add "$OUTSIDE_IP4" lladdr "$OUTSIDE_MAC"  dev balancer nud permanent

# Guest namespace
echo "  = Set "$NS" namespace="
ip -n "$NS" link set dev lo up
ip -n "$NS" link set dev veth up
ip -n "$NS" addr add dev veth "${INSIDE_IP4}/${IP4_PREFIX_SIZE}"
ip netns exec "$NS" ethtool -K veth rxvlan off txvlan off

ip -n "$NS" link set dev veth address "$INSIDE_MAC"
ip -n "$NS" neigh add "$OUTSIDE_IP4" lladdr "$INSIDE_MAC" dev veth nud permanent


# Add route for whole test subnet, to make it easier to communicate between
# namespaces
ip -n "$NS" route add "172.11/$IP4_FULL_PREFIX_SIZE" via "$OUTSIDE_IP4" dev veth

create_vlan()
{   
    namedev="$2"
    echo "  = Creating VLAN $1 ="
    local inside_ip="${IP4_SUBNET}2$1"
    local inside_mac=de:ad:be:ef:00:2$1
    
    ip -n "$NS" link add dev "$namedev.$1" link veth type vlan id $1
    ip -n "$NS" link set dev "$namedev.$1" up
    ip -n "$NS" addr add dev "$namedev.$1" "${inside_ip}/${IP4_PREFIX_SIZE}"
    ip netns exec "$NS" ethtool -K "$namedev.$1" rxvlan off txvlan off
    ip -n "$NS" link set dev "$namedev.$1" address $inside_mac
    ip -n "$NS" neigh add "$inside_ip" lladdr $inside_mac dev "$namedev.$1" nud permanent
}


for (( counter=0; counter<VLAN_COUNT; counter++ ))
    do
    if [[ $counter -ge 2 ]]
    then
        create_vlan $counter "client"
    else
        create_vlan $counter "server"
    fi
done
