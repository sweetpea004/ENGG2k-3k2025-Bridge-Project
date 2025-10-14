import socket
import time

HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 5005        # Port to listen on (non-privileged ports are > 1023)

BUF_SIZE = 4000

STATUS_FREQUENCY = 2000000000

status = "STAT CLOS OPEN NONE NONE NONE TRAF NONE TRIG TRIG NONE EMER EMER NONE 0"

def send(message: str):

    # send message in string form
    print("Sent:", message)
    s.sendall(bytes(f"{message}\n", encoding="utf-8"))

def receive() -> str:

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

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    print(f"Listening on {HOST}:{PORT}")
    conn, addr = s.accept()
    if conn == True:
        print(f"Connected by {addr}")
        receive() # REDY
        send("OKOK")

        last_time = time.time_ns()

        while True:
            if (time.time_ns() - last_time >= STATUS_FREQUENCY):
                send(status)
                receive() # OKOK
                last_time = time.time_ns()
            else:
                try:
                    m = receive().split(" ")
                    match m[0]:
                        case "PUSH":
                            new_status = m[1:]
                            status = "STAT" + " ".join(new_status)
                            send("OKOK")
                        case "AUTO":
                            send("OKOK")
                        case "EMER":
                            send("OKOK")
                except:
                    print("")
                    