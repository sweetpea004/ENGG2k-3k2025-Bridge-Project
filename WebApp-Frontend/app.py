from flask import Flask, render_template, request, flash
from flask_socketio import SocketIO, emit
import os
import threading
import socket
import time
import json

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
        self.bridge_top_limit = array[7].upper()
        self.bridge_bottom_limit = array[8].upper()
        self.gate_top_limit = array[9].upper()
        self.gate_bottom_limit = array[10].upper()
        self.road_lights = array[11].upper()
        self.waterway_lights = array[12].upper()
        self.audio = array[13].upper()
        self.error_code = array[14].upper()
    
    def toString(self):

        message = f"{self.message_code} {self.bridge_status} {self.gate_status} {self.north_us} {self.under_us} {self.south_us} {self.road_load} {self.bridge_top_limit} {self.bridge_bottom_limit} {self.gate_top_limit} {self.gate_bottom_limit} {self.road_lights} {self.waterway_lights} {self.audio} {self.error_code}"

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
            "bridge_top_limit": self.bridge_top_limit,
            "bridge_bottom_limit": self.bridge_bottom_limit,
            "gate_top_limit": self.gate_top_limit,
            "gate_bottom_limit": self.gate_bottom_limit,
            "road_lights": self.road_lights,
            "waterway_lights": self.waterway_lights,
            "audio": self.audio,
            "error_code": self.error_code
        }
    
class Connection:
    def __init__(self):
        self.value = False
    
    def toFalse(self):
        self.value = False
    
    def toTrue(self):
        self.value = True

# CONSTANTS
TEST_IP = "127.0.0.1"
TEST_PORT = 5005

ESP_IP = "172.20.10.2"
ESP_PORT = 5003

APP_PORT = 5004
BUF_SIZE = 4000
VERBOSE = True # Controls whether messages are printed to console

# Globals
sock = socket.socket()
default_status = "STAT CLOS OPEN NONE NONE NONE TRAF NONE TRIG TRIG NONE EMER EMER NONE 0"
status = Status(default_status.split(" "))
conn = Connection()

# APP INITIALISATION
app = Flask(__name__)
app.secret_key = os.urandom(32)
socketio = SocketIO(app)

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
            send("REDY")

            try:
                m = receive()
                conn.toTrue()
            except Exception as e:
                m = "no connection"
    
        '''
        else:
            time_current = time.time_ns()
            if(time_current - status.recieved_status > 2000000000):
                conn.toFalse()
                # attempt Reconnect
                print("Test")
            else:
        '''

def run_app():
    socketio.run(app, debug=True, port=APP_PORT)

@app.route("/", methods=['GET', 'POST'])
def redirect_dashboard():

    if request.method == 'POST':
        
        new_message = ""

        if request.form.get('action') == 'emergency':
            # Send EMER 
            new_message = "EMER"

            # flash confirmation message for Emergency
            flash('Stopping Bridge Processes for Emergency', 'error')
            
        elif request.form.get('action') == 'push':
            # Check the mode (manual vs automatic)
            mode = request.form['mode']

            if mode == "manual":                
                # create array in order to make a new status obj
                push = ["PUSH"]

                # determine selected inputs for bridge and gates
                push.append(request.form['bridge'])
                push.append(request.form['gates'])

                # determine selected inputs for ultrasonics
                ultrasonics = request.form.getlist('ultrasonics')
                if "north-us" in ultrasonics:
                    push.append("SHIP")
                else:
                    push.append("NONE")
                if "under-us" in ultrasonics:
                    push.append("SHIP")
                else:
                    push.append("NONE")
                if "south-us" in ultrasonics:
                    push.append("SHIP")
                else:
                    push.append("NONE")
                
                # determine selected settings for loadcell
                loadcell = request.form.getlist('loadcell')
                if "loadcell" in loadcell:
                    push.append("TRAF")
                else:
                    push.append("NONE")

                # determine Limit Switch Inputs
                limit_switches = request.form.getlist('limit-switch')
                if "bridge-top" in limit_switches:
                    push.append("TRIG")
                else:
                    push.append("NONE")
                if "bridge-bottom" in limit_switches:
                    push.append("TRIG")
                else:
                    push.append("NONE")
                if "gate-top" in limit_switches:
                    push.append("TRIG")
                else:
                    push.append("NONE")
                if "gate-bottom" in limit_switches:
                    push.append("TRIG")
                else:
                    push.append("NONE")

                # determine selected settings for traffic lights 
                push.append(request.form['road-lights'])
                push.append(request.form['waterway-lights'])

                # determine audio system selection
                push.append(request.form['audio'])

                # determine Error code selected 
                push.append(request.form['error-code'])
                
                # convert "push" array to Status Obj
                to_status = Status(push)

                # set message to be sent to string
                new_message = to_status.toString()

                # flash confirmation message
                flash('Pushed Manual Changes', 'info')

            elif mode == "auto":
                # Send: "AUTO"
                new_message = "AUTO"

                # flash confirmation message
                flash('Changed To Automatic Mode', 'info')

        # TODO - send message
        #send(new_message)

    # Load page
    return render_template("dashboard.html")

@socketio.on("retrieve_stat_data")
def handle_update():
    emit('update_stat_data', (status.toSerializable(), conn.value), broadcast=True)

def main():
    # Connection with ESP32
    sock.connect((ESP_IP, ESP_PORT))

    # Connection with Tester
    #sock.connect((TEST_IP, TEST_PORT))

    threads = []

    # thread for running app
    ui_thread = threading.Thread(target=run_app, daemon=True)
    threads.append(ui_thread)

    # thread for simultaneously communicating
    comm_thread = threading.Thread(target=communication, daemon=True)
    threads.append(comm_thread)

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()

if __name__ == "__main__":
    main()