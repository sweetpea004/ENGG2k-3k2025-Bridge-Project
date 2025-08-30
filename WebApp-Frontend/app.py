from flask import Flask, render_template, request, flash
import os
import multiprocessing
import socket
import time

ESP_PORT = 5000
APP_PORT = 5003
BUF_SIZE = 4000
VERBOSE = True # Controls whether messages are printed to console

# Globals
conn = False # boolean value representing if the app is connected to the ESP32

app = Flask(__name__)
app.secret_key = os.urandom(32)

class Status:
    def __init__(self, array):
        self.time_current = time.time_ns()
        self.time_since_last_status = time.time_ns()
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
    status = Status(message.split(" "))
    if(status.message_code == "STAT"):
        status.time_since_last_status = status.time_current
    return status

def send(message: str):

    # send message in strong form
    if VERBOSE:
        print("Sent:", message)
    sock.sendall(bytes(f"{message}\n", encoding="utf-8"))

@app.route("/", methods=['GET', 'POST'])
def redirect_dashboard():

    default_status = "STAT CLOS OPEN NONE NONE NONE TRAF NONE TRIG TRIG NONE EMER EMER NONE 0"
    status = Status(default_status.split(" "))
    status.time_current = time.time_ns()
    if(status.time_current - status.time_since_last_status > 2000000000):
        # attempt Reconnect
        print("Test")

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
        print(new_message)
        #send(new_message)

    # Load page
    return render_template("dashboard.html", status=status, conn=conn)

if __name__ == "__main__":

    # Connection with ESP32
    sock = socket.socket()
    #sock.connect(("localhost", ESP_PORT))

    default_status = "STAT CLOS OPEN NONE NONE NONE TRAF NONE TRIG TRIG NONE EMER EMER NONE 0"
    status = Status(default_status.split(" "))
    
    # Thread 1: Running App
    app.run(debug=True, port=APP_PORT)

    # Thread 2: Communications

    
