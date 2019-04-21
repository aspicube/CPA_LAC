#!/usr/bin/env python3
import serial
import sys

if(len(sys.argv) < 2):
    print("please give the address of config file")
    sys.exit(0)
try:
    file = open(sys.argv[1])
except FileNotFoundError:
    print("config file not found\n")
    sys.exit(1)
#load configuration from config file
repeat_time = 1
start_byte = 1
finish_byte = 256
load_flag = 0
print("loading data from", sys.argv[1], "...")
try:
    for line in file.readlines():
        sublines = line.split('=')
        if sublines[0] == "kem_type":
            kem_type = sublines[1]
            load_flag += 1
        elif sublines[0] == "secret_key":
            sk_a = bytes.fromhex(sublines[1][0:(len(sublines[1]) - 1)])
            load_flag += 1
        elif sublines[0] == "repeat_time":
            repeat_time = int(sublines[1])
        elif sublines[0] == "start_byte":
            start_byte = int(sublines[1])
        elif sublines[0] == "finish_byte":
            finish_byte = int(sublines[1])
    file.close()
except Exception:
    print("file format error")
    sys.exit(1)
if load_flag < 2:
    print("file format error: need kem_type or secret_key")
    sys.exit(1)
print("configuration loaded.")
#begin to send and recv data
#set secretkey
dev = serial.Serial("/dev/ttyUSB0", 115200)
send_length = len(sublines[1])
TX_data = b"\xC3"
TX_data = TX_data + (send_length / 256)
TX_data[2] = send_length % 256
dev.write(TX_data[0:2])
RX_data = dev.read(3)
if RX_data[0] == 0xFF:
    print("failed to send secretkey: sending parameter error")
    sys.exit(1)
dev.write(sk_a)
#repeat dec
for i in range(repeat_time):
    for j in range(finish_byte - start_byte + 1):
        TX_data[0] = 0xCF
        TX_data[1] = 0
        TX_data[2] = 1
        dev.write(TX_data[0:2])

