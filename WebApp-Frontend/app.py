from flask import Flask, render_template, request, flash
import os
import multiprocessing
import socket

ESP_PORT = 5000
APP_PORT = 5001
BUF_SIZE = 4000
VERBOSE = True # Controls whether messages are printed to console

# Globals
conn = False # boolean value representing if the app is connected to the ESP32

app = Flask(__name__)
app.secret_key = os.urandom(32)

class Status:
    def __init__(self, array):

        self.message_code = array[0].upper()
        self.bridge_status = array[1].upper()
        self.gate_status = array[2].upper()
        self.north_us = array[3].upper()
        self.under_us = array[4].upper()
        self.south_us = array[5].upper()
        self.road_load = array[6].upper()
        self.road_lights = array[7].upper()
        self.waterway_lights = array[8].upper()
        self.speaker = array[9].upper()

        if self.message_code == "STAT":
            self.error_code = array[10].upper()
    
    def toString(self):

        message = f"{self.message_code} {self.bridge_status} {self.gate_status} {self.north_us} {self.under_us} {self.south_us} {self.road_load} {self.road_lights} {self.waterway_lights} {self.speaker}"

        if self.message_code == "STAT":
            message += f" {self.error_code}"

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

def send(message: str):

    # send message in strong form
    if VERBOSE:
        print("Sent:", message)
    sock.sendall(bytes(f"{message}\n", encoding="utf-8"))

@app.route("/", methods=['GET', 'POST'])
def redirect_home():

    # if POST
    if request.method == 'POST':
        # Save message to be sent
        new_message = ""

        if request.form.get('action') == 'emergency':
            # Send EMER 
            new_message = "EMER"
            flash('Stopping Bridge Processes for Emergency', 'error')
            
        elif request.form.get('action') == 'push':
            # Check the mode (manual vs automatic)
            mode = request.form['mode']

            if mode == "manual":
                # Send: "PUSH [bridge] [gate] [northUS] [underUS] [southUS] [loadcell] [roadlights] [waterlights] [speaker]"
                
                # create array in order to make a new status obj
                push = ["PUSH"]

                push.append(request.form['bridge'])
                push.append(request.form['gates'])

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

                loadcell = request.form.getlist('loadcell')
                if "loadcell" in loadcell:
                    push.append("TRAF")
                else:
                    push.append("NONE")

                traffic_lights = request.form['traffic-lights']
                if traffic_lights == "road-lights":
                    push.append("GO") # roadway traffic lights
                    push.append("STOP") # waterway traffic lights
                elif traffic_lights == "waterway-lights":
                    push.append("STOP") # roadway traffic lights
                    push.append("GO") # waterway traffic lights
                elif traffic_lights == "emergency-lights":
                    push.append("EMER") # roadway traffic lights
                    push.append("EMER") # waterway traffic lights
                
                push.append(request.form['speaker'])
                
                to_status = Status(push)

                new_message = to_status.toString()
                flash('Pushed Manual Changes', 'info')

            elif mode == "auto":
                # Send: "AUTO"
                new_message = "AUTO"
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

    default_status = "STAT CLOS OPEN NONE NONE NONE TRAF GO STOP NONE 1"
    status = Status(default_status.split(" "))
    
    # Thread 1: Running App
    app.run(debug=True, port=APP_PORT)

    # Thread 2: Communications

    
