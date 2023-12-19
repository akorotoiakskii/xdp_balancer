#!/usr/bin/python3

import sys
import socket
import sys
import logging
import pathlib
import os


SERVER_PORT = 11000

if len(sys.argv) > 1:
    SERVER_IP = sys.argv[1]
else:
    SERVER_IP = ''

logs_dir_path = os.path.join(pathlib.Path(__file__).parent.resolve(),
                             "..","logs")

if not os.path.exists(logs_dir_path):
   os.makedirs(logs_dir_path)

log_fname = os.path.join(logs_dir_path,
                         'UDP_echo_server_' + SERVER_IP + '.log')

logging.basicConfig(filename = log_fname,
                    format = '%(asctime)s %(levelname)-8s %(message)s',
                    datefmt = '%Y-%m-%d %H:%M:%S',
                    level = logging.INFO,
                    encoding = 'utf-8')

# Bind the socket to the port
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

logging.info("Srart UDP echo server " + SERVER_IP)
server_address = (SERVER_IP, 11000)
sock.bind(server_address)

while True:
    # Wait for a DGRAM
    logging.info('Waiting for a DGRAM')
    data, addr = sock.recvfrom(1024)
    logging.info('Received %r from %r' % (data, addr))
    sock.sendto(data, addr)
    logging.info('Sent msg = %r to %r' %(data, addr))