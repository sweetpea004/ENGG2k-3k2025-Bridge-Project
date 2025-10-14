import socket
import time

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
        self.road_us = array[6].upper()
        self.road_load = array[7].upper()
        self.bridge_top_limit = array[8].upper()
        self.bridge_bottom_limit = array[9].upper()
        self.gate_top_limit = array[10].upper()
        self.gate_bottom_limit = array[11].upper()
        self.road_lights = array[12].upper()
        self.waterway_lights = array[13].upper()
        self.audio = array[14].upper()
        self.error_code = array[15].upper()
    
    def toString(self):

        message = f"{self.message_code} {self.bridge_status} {self.gate_status} {self.north_us} {self.under_us} {self.south_us} {self.road_us} {self.road_load} {self.bridge_top_limit} {self.bridge_bottom_limit} {self.gate_top_limit} {self.gate_bottom_limit} {self.road_lights} {self.waterway_lights} {self.audio} {self.error_code}"

        return message
    
    def toSerializable(self):
        return {
            "message_code": self.message_code,
            "bridge_status": self.bridge_status,
            "gate_status": self.gate_status,
            "north_us": self.north_us,
            "under_us": self.under_us,
            "south_us": self.south_us,
            "road_us": self.road_us,
            "road_load": self.road_load,
            "bridge_top_limit": self.bridge_top_limit,
            "bridge_bottom_limit": self.bridge_bottom_limit,
            "gate_top_limit": self.gate_top_limit,
            "gate_bottom_limit": self.gate_bottom_limit,
            "road_lights": self.road_lights,
            "waterway_lights": self.waterway_lights,
            "audio": self.audio,
            "error_code": self.error_code
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

TEST_IP = "127.0.0.1"
TEST_PORT = 5005

ESP_IP = "172.20.10.2"
ESP_PORT = 5003

STATUS_FREQUENCY = 2000000000

# Globals
sock = socket.socket()
default_status = "STAT CLOS OPEN NONE NONE NONE TRAF TRAF TRIG NONE TRIG NONE EMER EMER NONE 0"
status = Status(default_status.split(" "))
conn = Connection()

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
    if VERBOSE:
        print("Received:", message)
    return message

def parse_message(message: str) -> Status:

    message = message.split(" ")
    if(message[0] == "STAT"):
        status = Status(message)
    return status
    
def communication():

    while True:
        if conn.value == False:
            # Connection with ESP32
            sock.connect((ESP_IP, ESP_PORT))

            # Connection with Tester
            #sock.connect((TEST_IP, TEST_PORT))

            try:
                send("REDY")

                m = receive()
                if (m == "OKOK"):
                    status.resetTime()

                conn.toTrue()
                time.sleep(1) # small delay
            except Exception as e:
                print("no connection")
    
        else:
            time_current = time.time_ns()
            if(time_current - status.recieved_status > STATUS_FREQUENCY):
                conn.toFalse()
                # attempt Reconnect
            else:
                received_string = receive().split(" ")
                match received_string[0]:
                    case "STAT":
                        status = Status(received_string)
                        send("OKOK")
                    case "OKOK":
                        status.resetTime()


