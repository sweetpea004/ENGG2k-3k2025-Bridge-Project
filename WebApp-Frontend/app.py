from flask import Flask, render_template, request, flash
from flask_socketio import SocketIO, emit
import os
import communication as commProg
import threading

# CONSTANTS
APP_PORT = 5004

# APP INITIALISATION
app = Flask(__name__)
app.secret_key = os.urandom(32)
socketio = SocketIO(app)

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

                # determine selected settings for bridge loadcell
                road_detect = request.form.getlist('road-detect')
                if "loadcell" in road_detect:
                    push.append("TRAF")
                else:
                    push.append("NONE")

                # determine Limit Switch Inputs
                limit_switches = request.form.getlist('limit-switch')
                if "road-us" in limit_switches:
                    push.append("TRIG")
                else:
                    push.append("NONE")
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
                to_status = commProg.Status(push)

                # set message to be sent to string
                new_message = to_status.toString()

                # flash confirmation message
                flash('Pushed Manual Changes', 'info')

            elif mode == "auto":
                # Send: "AUTO"
                new_message = "AUTO"

                # flash confirmation message
                flash('Changed To Automatic Mode', 'info')

        print(new_message)
        commProg.newMessage(new_message) # sets to_be_sent global to new message to respond with

    # Load page
    return render_template("dashboard.html")

@socketio.on("retrieve_stat_data")
def handle_update():
    emit('update_stat_data', (commProg.status.toSerializable(), commProg.conn.value), broadcast=True)

if __name__ == "__main__":
    #commThread = threading.Thread(target=commProg.communication, daemon=True)
    #commThread.start()

    run_app()
    
    #commThread.join()
