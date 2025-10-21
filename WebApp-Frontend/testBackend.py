import socket
import time

# HOST = "10.126.242.252"  # Standard loopback interface address (localhost)
HOST = "192.168.0.150" # Set to your own ip address, can be checked with 'ipconfig' on windows, or 'ip addr' on linux
PORT = 5005        # Port to listen on (non-privileged ports are > 1023)

BUF_SIZE = 8192

STATUS_FREQUENCY = 2000000000

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

status = "STAT CLOS OPEN NONE NONE NONE TRAF NONE TRIG TRIG NONE EMER EMER NONE 0"

def send(message: str, s: socket.socket):

    # send message in string form
    print("Sent:", message)
    s.sendall(bytes(f"{message}\n", encoding="utf-8"))

def receive(s: socket.socket) -> str:

    # recieve string message 
    data = b''
    while True:
        part = s.recv(BUF_SIZE)
        data += part
        if len(part) < BUF_SIZE: # Check if reached end of message
            break

    message = data.decode().strip()
    print("Received:", message)
    return message

#with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((HOST, PORT))
s.listen(5)
print(f"Listening on {HOST}:{PORT}")
conn, addr = s.accept()
if conn is not None:
    print(f"Connected by {addr}")
    receive(conn) # REDY
    send("OKOK", conn)

    last_time = time.time_ns()

    while True:
        if (time.time_ns() - last_time >= STATUS_FREQUENCY):
            try:
                m = receive(conn).split(" ")
                match m[0]:
                    case "PUSH":
                        new_status = m[1:]
                        status = "STAT" + " ".join(new_status)
                        send("OKOK", conn)
                    case "AUTO":
                        send("OKOK", conn)
                    case "EMER":
                        send("OKOK", conn)
            except:
                print("")
        send(status, conn)
        receive(conn) # OKOK
        last_time = time.time_ns()
s.close()
             