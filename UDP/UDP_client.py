#!/usr/bin/python3

import socket
import sys
import pathlib
import os


SERVER_IP= sys.argv[1]
SERVER_PORT = 11000

if len(sys.argv) > 2:
    CLIENT_IP = sys.argv[2]
else:
    CLIENT_IP = ''

logs_dir_path = os.path.join(pathlib.Path(__file__).parent.resolve(),
                             "..","logs")
if not os.path.exists(logs_dir_path):
    os.makedirs(logs_dir_path)

log_fname = os.path.join(logs_dir_path,
                         'UDP_client_' + CLIENT_IP + '.log')

import logging
logging.basicConfig(filename = log_fname,
                    format = '%(asctime)s %(levelname)-8s %(message)s',
                    datefmt = '%Y-%m-%d %H:%M:%S',
                    level = logging.INFO,
                    encoding = 'utf-8')


addr = SERVER_IP, SERVER_PORT
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind((CLIENT_IP, 0))
logging.info('UDP echo client ready')
msg = "Udp"
# while True:
s.sendto(msg.encode('utf-8'), addr)
logging.info('Sent msg = %r to %r' %(msg, SERVER_IP))
data, fromaddr = s.recvfrom(1024)
logging.info('Recived msg = %r from %r' %(msg, SERVER_IP))
#time.sleep(2)
