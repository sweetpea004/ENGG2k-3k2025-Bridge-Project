#include <WiFi.h>
#include <string.h>
#include <WiFiServer.h>
#include <ESP32Servo.h>  // servo control
#include <NewPing.h>  // ultrasonic sensor 
#include "HX710.h" // load cell amplifier

// provide a lightweight fallback if HX710.h isn't available to allow compilation

HX710 scale;

// Ultrasonic sensors
#define TRIGGER_PIN_NORTH 25
#define ECHO_PIN_NORTH    34
#define TRIGGER_PIN_SOUTH 26
#define ECHO_PIN_SOUTH    35
#define TRIGGER_PIN_ROAD  27
#define ECHO_PIN_ROAD     36
#define TRIGGER_PIN_UNDER 14
#define ECHO_PIN_UNDER    39

// Optional additional ultrasonic sensors (set to -1 to disable)
#ifndef TRIGGER_PIN_UNDER2
#define TRIGGER_PIN_UNDER2 -1
#endif
#ifndef ECHO_PIN_UNDER2
#define ECHO_PIN_UNDER2 -1
#endif
#ifndef TRIGGER_PIN_BRIDGE_TOP
#define TRIGGER_PIN_BRIDGE_TOP -1
#endif
#ifndef ECHO_PIN_BRIDGE_TOP
#define ECHO_PIN_BRIDGE_TOP -1
#endif

// Pointers for optional sensors (created in setup if pins valid)
NewPing* sonarUnder2 = nullptr;    // second under-bridge sensor
NewPing* sonarBridgeTop = nullptr; // top-looking sensor

// Servos
#define SERVO_BRIDGE_PIN 23
#define SERVO_GATE_PIN   22

// Shift register pins
#define LATCH_PIN 5   // RCLK
#define CLOCK_PIN 18  // SCLK
#define DATA_PIN 19   // DATA

// Speaker pins
#define SPEAKER_RX_PIN 16 // RX2
#define SPEAKER_TX_PIN 17 // TX2

// Limit switches
#define LIMIT_GATE_CLOSED_PIN 4   // LimitSwitch_1
#define LIMIT_GATE_OPEN_PIN   13  // LimitSwitch_2
#define LIMIT_BRIDGE_CLOSED_PIN 12 // LimitSwitch_3
#define LIMIT_BRIDGE_OPEN_PIN   15 // LimitSwitch_4

// Network Configuration
const char* ssid = "Dragan’s iPhone (2)";
const char* password = "bigpassword";
const int serverPort = 5003;

// Communication
WiFiServer server(serverPort);
WiFiClient client;
bool clientConnected = false;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 1000; // 1 second

// Pins
#define MAX_DISTANCE 500

/// add led stuff below here
#define LEDS_OFF 0
#define LEDS_RED 1
#define LEDS_GREEN 2
byte leds1 = 0;
byte leds2 = 0;

// Load cell pins & configuration
#define LOADCELL_CLK_PIN 33
#define LOADCELL_DOUT_PIN 32
//HX710 scale;
long loadcellTare = 0;
float calibration_factor = 717.5; // adjust to your calibration
const float LOAD_THRESHOLD_GRAMS = 5.0; // anything above this and it counts as on bridge

// Servo & Ultrasonic sensor setup
NewPing sonarNorth(TRIGGER_PIN_NORTH, ECHO_PIN_NORTH, MAX_DISTANCE);
NewPing sonarSouth(TRIGGER_PIN_SOUTH, ECHO_PIN_SOUTH, MAX_DISTANCE);
NewPing sonarRoad(TRIGGER_PIN_ROAD, ECHO_PIN_ROAD, MAX_DISTANCE);
NewPing sonarUnder(TRIGGER_PIN_UNDER, ECHO_PIN_UNDER, MAX_DISTANCE);
Servo bridgeServo; // Servo object
Servo gateServo; // Servo object

// Play a specific track on the voice module
void playVoice(uint8_t track) {
  uint8_t playCmd[6] = {0xAA, 0x07, 0x02, 0x00, track, (uint8_t)(track + 0xB3)};
  Serial2.write(playCmd, 6);
}

// Set voice module volume (0x00 - 0x1E)
void setVolume(uint8_t vol) {
  uint8_t volCmd[5] = {0xAA, 0x13, 0x01, vol, (uint8_t)(vol + 0xBE)};
  Serial2.write(volCmd, 5);
}

// Convenience wrappers for open/close alarms
void playOpenAlarm() { playVoice(0x01); }
void playCloseAlarm() { playVoice(0x02); }

// Limit switch and movement state variables
bool limitGateClosed = false;
bool limitGateOpen = false;
bool limitBridgeClosed = false;
bool limitBridgeOpen = false;

bool gateMoving = false;
bool bridgeMoving = false;



//voids
void readMssg(String mssg);
void handleClient();
void sendHeartbeat();
void controlBridge();
void openBridge();
void closeBridge();
bool checkForShips();
bool checkForCars();
bool checkUnderBridge();
bool checkBridgeTop();
void updateLimitSwitches();
void startGateClose();
void startGateOpen();
void stopGate();
void startBridgeOpen();
void startBridgeClose();
void stopBridge();
void resetBridgeControlState();
void emergencyStop();
void openGates();
void closeGates();
// forward declaration for load mass reader
float readLoadMass();


void testUltrasonics(int samples, int delayMs);
void testLimitSwitches(int iterations, int delayMs);
void testActuators(unsigned long timeoutMs);
void runAllTests();
void test();
void testMotors();
void testLEDs();
void testSpeaker();
// error codes:
// 0: No Error
// 1: Bridge limit switch not detecting bridge
// 2: Gates limit switch not detecting gates
// 3: Gates & Bridge
// 4: Road traffic detection error
// 5: Road traffic & Bridge
// 6: Gates & Road Traffic 
// 7: All errors

// Bridge State Structure
struct BridgeState {
  String bridgeStatus = "CLOS";
  String gateStatus = "OPEN";
  String northUS = "NONE";
  String southUS = "NONE";
  String underUS = "NONE";
  String roadUS = "NONE";
  String roadLoad = "NONE";
  String roadLights = "GOGO";
  String waterwayLights = "STOP";
  String gateSwitchUp = "NONE";
  String gateSwitchDown = "NONE";
  String bridgeSwitchUp = "NONE";
  String bridgeSwitchDown = "NONE";
  String speaker = "NONE";
  int errorCode = 0;

  // Additional fields for optional sensors
  String underUS2 = "NONE";
  String bridgeTopUS = "NONE";
};

// Control Mode Enum
enum ControlMode {
  AUTO_MODE,
  MANUAL_MODE,
  EMERGENCY_MODE
};

ControlMode currentMode = AUTO_MODE;
BridgeState currentState;

// Setup
void setup() {
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);  
  pinMode(CLOCK_PIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("ESP32 Bridge Control - Heartbeat Program");

  // Setup WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  
  // Start server
  server.begin();
  Serial.println("Server started on port 5003");

  // Initialize voice module serial (Serial2) on chosen pins
  Serial2.begin(9600, SERIAL_8N1, SPEAKER_RX_PIN, SPEAKER_TX_PIN);
  delay(200);
  setVolume(0x1E); // set to max by default 

  // Initialize load cell
  scale.initialize(LOADCELL_CLK_PIN, LOADCELL_DOUT_PIN);
  Serial.println("Initializing load cell and taring...");
  long tareSum = 0;
  for (int i = 0; i < 20; i++) {
    while (!scale.isReady());
    //scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
    tareSum += scale.getLastDifferentialInput();
    delay(10);
  }
  loadcellTare = tareSum / 20;
  Serial.print("Loadcell tared. Offset=");
  Serial.println(loadcellTare);

  // esp timer
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Setup the servo
  bridgeServo.attach(SERVO_BRIDGE_PIN, 500, 2500); // Attach servo with range
  bridgeServo.write(90);  // ake sure motor is stopped

  gateServo.attach(SERVO_GATE_PIN, 500, 2500); // Attach servo with range
  gateServo.write(90);  // make sure motor is stopped

  // Configure limit switch pins (use INPUT_PULLUP, switches expected to pull LOW when triggered)
  pinMode(LIMIT_GATE_CLOSED_PIN, INPUT_PULLUP);
  pinMode(LIMIT_GATE_OPEN_PIN, INPUT_PULLUP);
  pinMode(LIMIT_BRIDGE_CLOSED_PIN, INPUT_PULLUP);
  pinMode(LIMIT_BRIDGE_OPEN_PIN, INPUT_PULLUP);

  // Initialize optional ultrasonic sensors if pins are valid
  if (TRIGGER_PIN_UNDER2 != -1 && ECHO_PIN_UNDER2 != -1) {
    sonarUnder2 = new NewPing(TRIGGER_PIN_UNDER2, ECHO_PIN_UNDER2, MAX_DISTANCE);
    Serial.println("Initialized second under-bridge ultrasonic sensor");
  }
  if (TRIGGER_PIN_BRIDGE_TOP != -1 && ECHO_PIN_BRIDGE_TOP != -1) {
    sonarBridgeTop = new NewPing(TRIGGER_PIN_BRIDGE_TOP, ECHO_PIN_BRIDGE_TOP, MAX_DISTANCE);
    Serial.println("Initialized bridge-top ultrasonic sensor");
  }

  delay(1000);
}

// String interpretation
void readMssg(String mssg) {
  // PRE: receive a string in the Appendix E format
  // POST: read the message and run the appropriate response command
  String extract = mssg.substring(0,4);
  if (extract == "REDY") {
    // TNK send OKOK to frontend

    return;
  } else if (extract == "OKOK") {
    // Nothing
    return;
  } else if (extract == "AUTO") {
    // put the bridge into automatic mode
    if (currentMode == EMERGENCY_MODE) {
    //"ERROR: Cannot switch from EMERGENCY mode - reset required"
      return;
    }
    currentMode = AUTO_MODE;
    resetBridgeControlState(); // Reset state machine when switching to auto
    Serial.println("Switched to AUTO mode");
  } else if (extract == "EMER") {
    // put the bridge into emergency mode
    currentMode = EMERGENCY_MODE;
    emergencyStop();
  } else if (extract == "PUSH") {
    currentMode = MANUAL_MODE;
    for (int i = 1; i < 15; i++) {
      extract = mssg.substring(i*5, 4);
      if (extract == "OPEN") {
        switch (i){ 
          case 1:
            openBridge();

          case 2:
            openGates();
        }
      } else if (extract == "CLOS"){
        switch (i){
          case 1:
            closeBridge();
          case 2:
            closeGates();
        }
      } else if (extract == "SHIP"){
        switch (i){ 
          case 3:
            currentState.northUS = "SHIP";

          case 4:
            currentState.underUS = "SHIP";

          case 5:
            currentState.southUS = "SHIP";
        }
      } else if (extract == "TRAF"){
        currentState.roadLoad = "TRAF";
        
      } else if (extract == "TRIG"){
        switch (i){ 
          case 7:
            currentState.roadUS = "TRIG";

          case 8:
            currentState.bridgeSwitchUp = "TRIG";

          case 9:
            currentState.bridgeSwitchDown = "TRIG";

          case 10:
            currentState.gateSwitchUp = "TRIG";

          case 11:
            currentState.gateSwitchDown = "TRIG";
        }
        
      } else if (extract == "GOGO"){
        switch (i){ 
          case 12:
            currentState.roadLights = "GOGO";
          case 13:
            currentState.waterwayLights = "GOGO";
          
        }
        
      } else if (extract == "STOP"){
        switch (i){ 
          case 12:
            currentState.roadLights = "STOP";

          case 13:
            currentState.waterwayLights = "STOP";
          
        }
        
      } else if (extract == "SLOW"){
        switch (i){ 
          case 12:
            currentState.roadLights = "SLOW";

          case 13:
            currentState.waterwayLights = "SLOW";

        }
        
      } else if (extract == "NONE"){
        switch (i){ 
          case 3:
            currentState.northUS = "NONE";

          case 4:
            currentState.underUS = "NONE";

          case 5:
            currentState.southUS = "NONE";
          
          case 6:
            currentState.roadLoad = "NONE";

          case 7:
            currentState.roadUS = "NONE";

          case 8:
            currentState.bridgeSwitchUp = "NONE";

          case 9:
            currentState.bridgeSwitchDown = "NONE";

          case 10:
            currentState.gateSwitchUp = "NONE";
          
          case 11:
            currentState.gateSwitchDown = "NONE";

          case 14:
            currentState.speaker = "NONE";
        }
        
      } else if (extract == "DONE"){
        currentState.speaker = "DONE";
        
      } else if (extract == "MOVN"){
        if (i = 14) currentState.speaker = "MOVN";
        
      } else if (extract == "EMER"){
        if (i = 14) currentState.speaker = "EMER";
      }
    }
  }
}

// Read and update limit switch states
void updateLimitSwitches() {
  // active-low switches: LOW means triggered
  // TNK Limit Switches
  limitGateClosed    = (digitalRead(LIMIT_GATE_CLOSED_PIN) == LOW);
  if (limitGateClosed) currentState.gateSwitchDown = "TRIG" else currentState.gateSwitchDown = "NONE";
  limitGateOpen      = (digitalRead(LIMIT_GATE_OPEN_PIN) == LOW);
  if (limitGateClosed) currentState.gateSwitchUp = "TRIG" else currentState.gateSwitchUp = "NONE";
  limitBridgeClosed  = (digitalRead(LIMIT_BRIDGE_CLOSED_PIN) == LOW);
  if (limitGateClosed) currentState.bridgeSwitchDown = "TRIG" else currentState.bridgeSwitchDown = "NONE";
  limitBridgeOpen    = (digitalRead(LIMIT_BRIDGE_OPEN_PIN) == LOW);
  if (limitGateClosed) currentState.bridgeSwitchUp = "TRIG" else currentState.gateSwitchUp = "NONE";
}

// Start/stop gate movement (non-blocking)
void startGateClose() {
  gateServo.write(60); // move towards closed
  gateMoving = true;
}
void startGateOpen() {
  gateServo.write(120); // move towards open
  gateMoving = true;
}
void stopGate() {
  gateServo.write(90); // stop
  gateMoving = false;
}

// Start/stop bridge movement (non-blocking)
void startBridgeOpen() {
  playOpenAlarm();
  bridgeServo.write(120); // move towards open
  bridgeMoving = true;
}
void startBridgeClose() {
  playCloseAlarm();
  bridgeServo.write(60); // move towards closed
  bridgeMoving = true;
}
void stopBridge() {
  bridgeServo.write(90); // stop
  bridgeMoving = false;
}


// Main Loop
void loop() {

  //      ~tests~      //
  test(); // runs all tests
  //  ~end of tests~  //

  updateLimitSwitches();
  handleClient();
  controlBridge();
  sendHeartbeat();
  testLEDs();
  delay(50);
}

// Handle client requests
void handleClient() {
  if (!clientConnected) {
    client = server.available();
    if (client) {
      clientConnected = true;
      Serial.println("Client connected");
    }
  }
  
  if (clientConnected && client.available()) {
    // v receive message from client
    String command = client.readStringUntil('\n');
    command.trim();
    // v update heartbeat timer
    lastHeartbeat = 0;
    Serial.println(command);
    //readMssg(command);  deosnt work
  }
}

// Send heartbeat to client
void sendHeartbeat() {
  // Attempt to accept a client if not already connected (just in case idk)
  if (!clientConnected && WiFi.status() == WL_CONNECTED) {
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      clientConnected = true;
      Serial.println("Client connected (heartbeat)");
    }
  }

  // ensure client connected
  if (clientConnected) {
    if (!client || !client.connected()) {
      clientConnected = false;
      if (client) client.stop();
      Serial.println("Client disconnected (heartbeat)");
      return;
    }

    // Send heartbeat at the configured interval
    if (millis() - lastHeartbeat >= heartbeatInterval) {
      String status = buildStatusMessage();
      client.println(status); // send to client
      Serial.println("Heartbeat sent: " + status);
      lastHeartbeat = millis();
    }
  }
}

// Build status message
String buildStatusMessage() {
  return "STAT " + currentState.bridgeStatus + " " + currentState.gateStatus + " " + 
         currentState.northUS + " " + currentState.underUS + " " + currentState.southUS + " " +
         currentState.roadLoad + " " + currentState.roadLights + " " + currentState.waterwayLights + " " + 
         String(currentState.errorCode) + " " + currentState.underUS2 + " " + currentState.bridgeTopUS;
}

// Control the bridge states (open/close)
void controlBridge() {
  // Don't run automatic control in manual or emergency mode
  if (currentMode != AUTO_MODE) return;

  // ensure switches are up-to-date
  updateLimitSwitches();

  static enum {
    WAIT_FOR_SHIPS,
    WAIT_FOR_CARS_CLEAR,
    GATES_CLOSING,
    BRIDGE_OPENING,
    BRIDGE_OPEN,
    WAIT_FOR_UNDER_CLEAR,
    BRIDGE_CLOSING,
    GATES_OPENING
  } state = WAIT_FOR_SHIPS;
  static unsigned long stateStartTime = 0;

  switch (state) {
    case WAIT_FOR_SHIPS:
      if (checkForShips()) {
        Serial.println("Ship detected - waiting for cars to clear...");
        state = WAIT_FOR_CARS_CLEAR;
      }
      break;

    case WAIT_FOR_CARS_CLEAR:
      if (!checkForCars()) {
        Serial.println("No cars detected - closing gates...");
        // start closing gates and wait for limit switch
        startGateClose();
        currentState.roadLights = "STOP";
        currentState.waterwayLights = "STOP";
        stateStartTime = millis();
        state = GATES_CLOSING;
      }
      break;

    case GATES_CLOSING:
      // if gate reached closed limit, stop and proceed
      if (limitGateClosed) {
        if (gateMoving) {
          stopGate();
          Serial.println("Gates closed (limit switch)");
        }
        // before opening the bridge ensure no weight sits on the bridge
        float mass = readLoadMass();
        if (isnan(mass)) mass = 0;
        if (mass > LOAD_THRESHOLD_GRAMS) {
          Serial.println("Weight detected on bridge (" + String(mass,1) + " g) - delaying open");
          currentState.roadLoad = "LOAD";
          // go back to waiting for cars to clear so operator can react
          state = WAIT_FOR_CARS_CLEAR;
        } else {
          Serial.println("Gates closed - starting bridge open (limit-driven)...");
          currentState.waterwayLights = "GOGO";
          startBridgeOpen();
          stateStartTime = millis();
          state = BRIDGE_OPENING;
        }
      } else {
        // still moving or waiting for limit
        if (!gateMoving) startGateClose();
      }
      break;

    case BRIDGE_OPENING:
      // Safety: If the closed-limit is active while attempting to open, stop and raise an error
      if (limitBridgeClosed) {
        if (bridgeMoving) {
          stopBridge();
        }
        Serial.println("ERROR: Bridge closed limit active while opening - emergency stop");
        currentState.errorCode = 1; // bridge limit error
        emergencyStop();
        state = WAIT_FOR_SHIPS; // reset state until manual/auto cleared
      } else if (limitBridgeOpen) {
        if (bridgeMoving) {
          stopBridge();
          Serial.println("Bridge open (limit switch)");
        }
        stateStartTime = millis();
        state = BRIDGE_OPEN;
      } else {
        if (!bridgeMoving) startBridgeOpen();
      }
      break;

    case BRIDGE_OPEN:
      if (millis() - stateStartTime > 10000) {
        Serial.println("Waiting for ship under bridge to clear...");
        state = WAIT_FOR_UNDER_CLEAR;
      }
      break;

    case WAIT_FOR_UNDER_CLEAR:
      if (!checkUnderBridge()) {
        Serial.println("No ship under bridge - closing bridge...");
        currentState.waterwayLights = "STOP";
        startBridgeClose();
        stateStartTime = millis();
        state = BRIDGE_CLOSING;
      } else {
        Serial.println("Ship still under bridge - waiting...");
        stateStartTime = millis(); // reset timer
      }
      break;

    case BRIDGE_CLOSING:
      if (limitBridgeClosed) {
        if (bridgeMoving) {
          stopBridge();
          Serial.println("Bridge closed (limit switch)");
        }
        // open gates after bridge fully closed
        startGateOpen();
        currentState.roadLights = "GOGO";
        stateStartTime = millis();
        state = GATES_OPENING;
      } else {
        if (!bridgeMoving) startBridgeClose();
      }
      break;

    case GATES_OPENING:
      if (limitGateOpen) {
        if (gateMoving) {
          stopGate();
          Serial.println("Gates opened (limit switch)");
        }
        state = WAIT_FOR_SHIPS;
      } else {
        if (!gateMoving) startGateOpen();
      }
      break;
  }
}

void openBridge() { // add limit switches
  // Play open alarm before movement
  playOpenAlarm();

  currentState.bridgeStatus = "OPEN";
  bridgeServo.write(120);
  delay(2000);
  bridgeServo.write(90);
  currentState.bridgeStatus = "OPEN";  // Update heartbeat status
  Serial.println("Bridge opened");
}

void closeBridge() {
  // Play close alarm before movement
  playCloseAlarm();

  currentState.bridgeStatus = "CLOS";
  bridgeServo.write(60);
  delay(2000);
  bridgeServo.write(90);
  currentState.bridgeStatus = "CLOS";  // Update heartbeat status
  Serial.println("Bridge closed");
}

// Check if ships are detected (north/south)
bool checkForShips() {
  // Do not run automatic detection while in MANUAL mode
  if (currentMode == MANUAL_MODE) return false;

  int distanceNorth = sonarNorth.ping_cm();
  int distanceSouth = sonarSouth.ping_cm();
  bool shipDetected = false;
  if (distanceNorth > 0 && distanceNorth < 30) {
    currentState.northUS = "SHIP";
    shipDetected = true;
  } else {
    currentState.northUS = "NONE";
  }
  if (distanceSouth > 0 && distanceSouth < 30) {
    currentState.southUS = "SHIP";
    shipDetected = true;
  } else {
    currentState.southUS = "NONE";
  }
  return shipDetected;
}

// Check if cars are detected on the road
bool checkForCars() {
  if (currentMode == MANUAL_MODE) return false;
  int distanceRoad = sonarRoad.ping_cm();
  if (distanceRoad > 0 && distanceRoad < 30) {
    currentState.roadUS = "CAR";
    return true;
  } else {
    currentState.roadUS = "NONE";
    return false;
  }
}

// Check if ship is under the bridge before closing
bool checkUnderBridge() {
  if (currentMode == MANUAL_MODE) return false;

  bool detected = false;

  int distanceUnder1 = sonarUnder.ping_cm();
  if (distanceUnder1 > 0 && distanceUnder1 < 30) {
    currentState.underUS = "SHIP";
    detected = true;
  } else {
    currentState.underUS = "NONE";
  }

  if (sonarUnder2 != nullptr) {
    int distanceUnder2 = sonarUnder2->ping_cm();
    if (distanceUnder2 > 0 && distanceUnder2 < 30) {
      currentState.underUS2 = "SHIP";
      detected = true;
    } else {
      currentState.underUS2 = "NONE";
    }
  } else {
    currentState.underUS2 = "NONE";
  }

  return detected;
}

// Bridge-top thresholds (cm)
#define BRIDGE_TOP_CLOSED_THRESHOLD_CM 15  // reading <= this -> bridge closed (near)
#define BRIDGE_TOP_OPEN_THRESHOLD_CM   60  // reading >= this or out-of-range -> bridge open (raised)

// Check if there is anything on top of the bridge (interpreted as bridge height: open/closed)
bool checkBridgeTop() {
  // If no sensor or in manual mode, we don't auto-evaluate
  if (currentMode == MANUAL_MODE) return false;
  if (sonarBridgeTop == nullptr) {
    currentState.bridgeTopUS = "NONE";
    return false;
  }

  int distanceTop = sonarBridgeTop->ping_cm();
  // distanceTop == 0 means no echo (out of range) — treat as OPEN (bridge high)
  if (distanceTop == 0) {
    currentState.bridgeTopUS = "OPEN";
    return true;
  }

  if (distanceTop <= BRIDGE_TOP_CLOSED_THRESHOLD_CM) {
    currentState.bridgeTopUS = "CLOSED";
    return false;
  }

  if (distanceTop >= BRIDGE_TOP_OPEN_THRESHOLD_CM) {
    currentState.bridgeTopUS = "OPEN";
    return true;
  }

  // Between thresholds -> partially open/transitioning
  currentState.bridgeTopUS = "PART"; // partial
  return false;
}

void closeGates(){
  currentState.gateStatus = "CLOS";
  gateServo.write(60);
  delay(2000); 
  gateServo.write(90);
  Serial.println("Gates closed");
}

void openGates(){
  currentState.gateStatus = "OPEN";
  gateServo.write(120);
  delay(2000);
  gateServo.write(90);
  Serial.println("Gates opened");
}

// Emergency stop function
void emergencyStop() {
  // Immediately stop all motors
  bridgeServo.write(90); // Stop bridge motor
  gateServo.write(90);   // Stop gate motor
  
  // Set all status to emergency
  currentState.bridgeStatus = "EMER";
  currentState.gateStatus = "EMER";
  currentState.roadLights = "EMER";
  currentState.waterwayLights = "EMER";
  currentState.speaker = "EMER";
  currentState.errorCode = 8; // Emergency error code
  
  // Reset any ongoing operations
  resetBridgeControlState();
  
  Serial.println("EMERGENCY STOP ACTIVATED!");
  Serial.println("All systems halted - Manual intervention required");
}

// Reset the internal state machine for bridge control
void resetBridgeControlState() {
  // This function helps reset any static variables in controlBridge()
  // Call this when switching modes or during emergency
  static bool forceReset = true;
  if (forceReset) {
    // The next call to controlBridge() will start fresh
    forceReset = false;
  }
}

/////// LED functions ///////

// LED shift register handling
void updateShiftRegister() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, leds2);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, leds1);
  digitalWrite(LATCH_PIN, HIGH);
}

// sets LED bytes and pushes to set LED colours
void setLEDs(char north, char south, char west, char east, char errorCode) {
  leds1 = 0;
  leds2 = 0;
  switch (north) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      leds1 += 0b01000000;

    case LEDS_GREEN:
      leds1 += 0b10000000;
  }
  switch (south) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      leds1 += 0b00010000;

    case LEDS_GREEN:
      leds1 += 0b00100000;
  }
  switch (west) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      leds1 += 0b00000100;

    case LEDS_GREEN:
      leds1 += 0b00001000;
  }
  switch (east) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      leds1 += 0b00000001;

    case LEDS_GREEN:
      leds1 += 0b00000010;
  }
  switch (errorCode) {
    case 0:
      break;
    
    case 1:
      leds2 += 0b00001000;

    case 2:
      leds2 += 0b00010000;

    case 3:
      leds2 += 0b00011000;

    case 4:
      leds2 += 0b00100000;

    case 5:
      leds2 += 0b00101000;

    case 6:
      leds2 += 0b00110000;

    case 7:
      leds2 += 0b00111000;
  }
  updateShiftRegister();
  leds1 = 0;
  leds2 = 0;
}

void testLEDs(){
  setLEDs(LEDS_GREEN, LEDS_GREEN, LEDS_GREEN, LEDS_GREEN, 7);
  delay(1000);
  for (int i = 0 ; i < 8 ; i ++) {
    setLEDs(LEDS_RED, LEDS_RED, LEDS_RED, LEDS_RED, i);
    delay(1000);
  }
  setLEDs(LEDS_GREEN, LEDS_RED, LEDS_RED, LEDS_RED, 0);
  delay(500);
  setLEDs(LEDS_RED, LEDS_GREEN, LEDS_RED, LEDS_RED, 0);
  delay(500);
  setLEDs(LEDS_RED, LEDS_RED, LEDS_GREEN, LEDS_RED, 0);
  delay(500);
  setLEDs(LEDS_RED, LEDS_RED, LEDS_RED, LEDS_GREEN, 0);
  delay(500);
  setLEDs(LEDS_OFF, LEDS_OFF, LEDS_OFF, LEDS_OFF, 0);
}

void ErrorDisplay(){

}

void waterwayLights(String command){
  if(command == "STOP"){
    // turn on red light
  }
  else if(command == "GOGO"){
    // turn on green light
  }
  else if(command == "WARN"){
    // turn on yellow light
  }
}

void bridgelights(String command){
  if(command == "STOP"){
    // turn on red light
  }
  else if(command == "GOGO"){
    // turn on green light
  }
  else if(command == "WARN"){
    // turn on yellow light
  }
}

// Read mass from loadcell in grams (uses calibration_factor and tare)
float readLoadMass() {
  if (!scale.isReady()) {
    return NAN; // not ready
  }
  //scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
  long raw = scale.getLastDifferentialInput();
  long net = raw - loadcellTare;
  float mass = net / calibration_factor;
  if (mass < 0) mass = 0;
  return mass;
}

// Returns true if loadcell reports something on the bridge
bool isWeightOnBridge() {
  float m = readLoadMass();
  if (isnan(m)) return false; // treat as no weight if read failed
  return (m > LOAD_THRESHOLD_GRAMS);
}


//-----------------------------------Tests----------------------------------//

// Test speaker: set volume and play open/close tracks
void testSpeaker() {
  Serial.println("[TEST] Speaker: setting volume and playing sample tracks...");
  setVolume(0x1E);
  delay(100);
  playOpenAlarm();
  delay(1200);
  playCloseAlarm();
  delay(1200);
  Serial.println("[TEST] Speaker done.");
}

// Test ultrasonics: print distance readings for all four sensors
void testUltrasonics(int samples = 3, int delayMs = 200) {
  Serial.println("[TEST] Ultrasonics: reading sensors...");
  for (int s = 0; s < samples; s++) {
    int n = sonarNorth.ping_cm();
    int so = sonarSouth.ping_cm();
    int r = sonarRoad.ping_cm();
    int u = sonarUnder.ping_cm();
    int u2 = (sonarUnder2 != nullptr) ? sonarUnder2->ping_cm() : -1;
    int t = (sonarBridgeTop != nullptr) ? sonarBridgeTop->ping_cm() : -1;
    Serial.print("North:" + String(n) + " cm, South:" + String(so) + " cm, Road:" + String(r) + " cm, Under:" + String(u) + " cm");
    if (u2 != -1) Serial.print(", Under2:" + String(u2) + " cm");
    if (t != -1) Serial.print(", Top:" + String(t) + " cm");
    Serial.println();
    delay(delayMs);
  }
  Serial.println("[TEST] Ultrasonics done.");
}

// Test limit switches: read and print states
void testLimitSwitches(int iterations = 10, int delayMs = 200) {
  Serial.println("[TEST] Limit switches: polling states...");
  for (int i = 0; i < iterations; i++) {
    updateLimitSwitches();
    Serial.print("GateClosed:"); Serial.print(limitGateClosed);
    Serial.print(" GateOpen:"); Serial.print(limitGateOpen);
    Serial.print(" BridgeClosed:"); Serial.print(limitBridgeClosed);
    Serial.print(" BridgeOpen:"); Serial.println(limitBridgeOpen);
    delay(delayMs);
  }
  Serial.println("[TEST] Limit switches done.");
}

// Test actuators: move gates and bridge using limit switches with timeout
void testActuators(unsigned long timeoutMs = 8000) {
  Serial.println("[TEST] Actuators: testing gates and bridge (using limits)...");
  unsigned long t0;

  // Gates: open then close
  Serial.println("[TEST] Opening gates...");
  startGateOpen();
  t0 = millis();
  while (!limitGateOpen && (millis() - t0) < timeoutMs) {
    updateLimitSwitches();
    delay(20);
  }
  stopGate();
  Serial.println(limitGateOpen ? "Gate open limit reached" : "Gate open timeout");
  delay(500);

  Serial.println("[TEST] Closing gates...");
  startGateClose();
  t0 = millis();
  while (!limitGateClosed && (millis() - t0) < timeoutMs) {
    updateLimitSwitches();
    delay(20);
  }
  stopGate();
  Serial.println(limitGateClosed ? "Gate closed limit reached" : "Gate close timeout");
  delay(500);

  // Bridge: open then close
  Serial.println("[TEST] Opening bridge...");
  startBridgeOpen();
  t0 = millis();
  while (!limitBridgeOpen && (millis() - t0) < timeoutMs) {
    updateLimitSwitches();
    delay(20);
  }
  stopBridge();
  Serial.println(limitBridgeOpen ? "Bridge open limit reached" : "Bridge open timeout");
  delay(500);

  Serial.println("[TEST] Closing bridge...");
  startBridgeClose();
  t0 = millis();
  while (!limitBridgeClosed && (millis() - t0) < timeoutMs) {
    updateLimitSwitches();
    delay(20);
  }
  stopBridge();
  Serial.println(limitBridgeClosed ? "Bridge closed limit reached" : "Bridge close timeout");
  delay(500);

  Serial.println("[TEST] Actuators done.");
}

// Run all tests in sequence
void runAllTests() {
  testSpeaker();
  testUltrasonics();
  testLimitSwitches();
  testActuators();
}


void test() {
  Serial.println("---------- BEGIN TEST SUITE ----------");
  runAllTests();
  Serial.println("---------- END TEST SUITE ----------");
  // Pause long so routine is not re-run unless user triggers
  delay(20000);
}