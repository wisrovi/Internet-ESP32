import socket

client  = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) # UDP
client.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)  # Enable broadcasting mode
client.settimeout(0.2)

import time
while True:
    print("**********************************")
    client.sendto("queso loro".encode('utf-8'), ('<broadcast>', 3333))
    data, address = client.recvfrom(4096)
    print("Client received : ", data.decode('utf-8'), address)
    print("**********************************")
    time.sleep(5)
