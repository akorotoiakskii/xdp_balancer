# xdp_balancer

===========================================================

sudo VS/scripts/set_net.sh
cd VS/src/
make
sudo ip -force link set dev balancer xdp object xdp_redirect.o  verbose
../scripts/set_map.sh

# Вывод идет в канал трассировки ядра, который нужно включить:
echo -n 1 | sudo tee /sys/kernel/debug/tracing/options/trace_printk

# Просмотр потока сообщений:
sudo cat /sys/kernel/debug/tracing/trace_pipe


===========================================================
sudo nsenter --net=/var/run/netns/test_ns bash
../UDP/UDP_echo_server.py 172.11.0.20


===========================================================
sudo nsenter --net=/var/run/netns/test_ns bash
../UDP/UDP_echo_server.py 172.11.0.21


===========================================================
sudo nsenter --net=/var/run/netns/test_ns bash
ip -force link set dev veth xdp object xdp_pass.o verbose
../UDP/UDP_client.py  172.11.0.1 172.11.0.22
../UDP/UDP_client.py  172.11.0.1 172.11.0.23
../UDP/UDP_client.py  172.11.0.1 172.11.0.24
../UDP/UDP_client.py  172.11.0.1 172.11.0.25