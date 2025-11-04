import socket
import time
import os

class Status:
    def __init__(self, array):
        self.recieved_status = time.time_ns()

        # message contents
        self.message_code = array[0].upper()
        self.bridge_status = array[1].upper()
        self.gate_status = array[2].upper()
        self.north_us = array[3].upper()
        self.under_us = array[4].upper()
        self.south_us = array[5].upper()
        self.road_load = array[6].upper()
        self.road_us = array[7].upper()
        self.road_lights = array[8].upper()
        self.waterway_lights = array[9].upper()
        self.audio = array[10].upper()
        self.state_code = array[11].upper()
    
    def toString(self):

        message = f"{self.message_code} {self.bridge_status} {self.gate_status} {self.north_us} {self.under_us} {self.south_us} {self.road_load} {self.road_us} {self.road_lights} {self.waterway_lights} {self.audio} {self.state_code}"

        return message
    
    def toSerializable(self):
        return {
            "message_code": self.message_code,
            "bridge_status": self.bridge_status,
            "gate_status": self.gate_status,
            "north_us": self.north_us,
            "under_us": self.under_us,
            "south_us": self.south_us,
            "road_load": self.road_load,
            "road_us": self.road_us,
            "road_lights": self.road_lights,
            "waterway_lights": self.waterway_lights,
            "audio": self.audio,
            "state_code": self.state_code
        }
    
    def resetTime(self):
        self.recieved_status = time.time_ns()
    
class Connection:
    def __init__(self):
        self.value = False
    
    def toFalse(self):
        self.value = False
    
    def toTrue(self):
        self.value = True

# CONSTANTS
BUF_SIZE = 4000
VERBOSE = True # Controls whether messages are printed to console

FRONTEND_PATH = "WebApp-Frontend"
FILES_PATH = "files"
TO_BE_SENT_FILE = "to_be_sent.txt"
STATUS_FILE = "status.txt"

TO_BE_SENT_PATH = os.path.join(FRONTEND_PATH, FILES_PATH, TO_BE_SENT_FILE)
STATUS_PATH = os.path.join(FRONTEND_PATH, FILES_PATH, STATUS_FILE)

TEST_IP = "10.126.242.252"
TEST_PORT = 5005

#ESP_IP = "172.20.10.2" #Dragans hotspot
#ESP_IP = "10.236.167.40" #Persephone's hotspot
#ESP_IP = "10.220.101.40" #Lena's hotspot
ESP_IP = "10.252.99.214" #Nikola's hotspot
ESP_PORT = 5003

# Globals
sock = socket.socket()
conn = Connection()

def save_to_be_sent(message: str):
    with open(TO_BE_SENT_PATH, "w") as f:
        f.write(message)

def read_to_be_sent() -> str:
    message = ""
    with open(TO_BE_SENT_PATH, "r") as f:
        message = f.read()
    return message

def parse_message(message: str):
    split_message = message.split(" ")
    if(split_message[0] == "STAT"):
        strip_message = message[:57] # ensure only one line 
        with open(STATUS_PATH, "w") as f:
            f.write(strip_message)

def read_status() -> Status:
    message = ""
    with open(STATUS_PATH, "r") as f:
        message = f.read()
    
    return Status(message.split(" "))

def send(message: str):

    # send message in string form
    if VERBOSE:
        print("Sent:", message)
    sock.sendall(bytes(f"{message}\n", encoding="utf-8"))

def receive() -> str:

    # recieve string message 
    data = b''

    while True:
        part = sock.recv(BUF_SIZE)
        data += part
        if len(part) < BUF_SIZE: # Check if reached end of message
            break

    message = data.decode().strip()
    if VERBOSE and message != "":
        print("Received:", message)
    return message
    
def communication():
    global conn
    global sock

    print("Starting connection")
    save_to_be_sent("") # reset to empty

    # Connection with ESP32
    sock.connect((ESP_IP, ESP_PORT))

    # Connection with Tester
    #sock.connect((TEST_IP, TEST_PORT))

    conn.toTrue()
    time.sleep(1) # small delay

    send("REDY")

    receive() #OKOK

    time.sleep(1) # small delay

    while conn.value:
        received_string = receive()
        if (received_string != ""):
            parse_message(received_string)

            # response
            to_be_sent = read_to_be_sent()
            if (to_be_sent != ""):
                send(to_be_sent)
                save_to_be_sent("") # reset to empty
            else: 
                send("OKOK")

def test_status_change():
    while True:
        time.sleep(1)
        parse_message("STAT CLOS OPEN NONE NONE NONE NONE NONE STOP GOGO NONE 0")
        time.sleep(1)
        parse_message("STAT EMER EMER SHIP NONE NONE NONE NONE EMER EMER EMER 7")


