import socket

HOST = "127.0.0.1"  # Standard loopback interface address (localhost)
PORT = 5005        # Port to listen on (non-privileged ports are > 1023)

BUF_SIZE = 4000

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

def send(message: str):

    # send message in string form
    print("Sent:", message)
    s.sendall(bytes(f"{message}\n", encoding="utf-8"))

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    print(f"Listening on {HOST}:{PORT}")
    conn, addr = s.accept()
    if conn == True:
        print(f"Connected by {addr}")
        while True:
            received = receive()

            match received:
                case "REDY":
                    send("OKOK")