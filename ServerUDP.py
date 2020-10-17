import socket

ip = "192.168.0.110"
port = 3333

# Create a UDP socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Bind the socket to the port
server_address = (ip, port)
s.bind(server_address)
print("Do Ctrl+c to exit the program !!")

while True:
    print("####### Server is listening #######")
    data, address = s.recvfrom(4096)
    print("Server received: ", data.decode('utf-8'), address)    
    s.sendto("Respuesta del servidor: WISROVI".encode('utf-8'), address)
    print("**********************************")