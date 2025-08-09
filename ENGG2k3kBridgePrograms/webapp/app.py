from flask import Flask, render_template, request, flash, redirect
import multiprocessing
import socket

ESP_PORT = 5000
APP_PORT = 5001
BUF_SIZE = 4000
VERBOSE = True # Controls whether messages are printed to console

# Globals
conn = False # boolean value representing if the app is connected to the ESP32

app = Flask(__name__)

class Status:
    def __init__(self, array):

        self.bridge_status = array[1].upper()
        self.gate_status = array[2].upper()
        self.north_us = array[3].upper()
        self.under_us = array[4].upper()
        self.south_us = array[5].upper()
        self.road_load = array[6].upper()
        self.road_lights = array[7].upper()
        self.waterway_lights = array[8].upper()
        self.speaker = array[9].upper()

        if (array[0] == "STAT"):
            self.error_code = array[10].upper()
    
    def toString(self):
        message = ""

        #TODO - get string

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

        if request.form.get('action') == 'emergency':
            # TODO - Send: EMER
            value = "EMER"
            
        elif request.form.get('action') == 'push':
            # TODO - Send: "PUSH [bridge] [gate] [northUS] [underUS] [southUS] [loadcell] [roadlights] [waterlights] [speaker]""
            value = "PUSH"
            print(value)

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

    
