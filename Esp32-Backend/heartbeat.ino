#include <WiFi.h>
#include <string.h>
#include <WiFiServer.h>
#include <ESP32Servo.h>  // servo control
#include <NewPing.h>  // ultrasonic sensor 
#include "HX710.h" // load cell amplifier

// provide a lightweight fallback if HX710.h isn't available to allow compilation

HX710 scale;

// Ultrasonic sensors
#define TRIGGER_PIN_NORTH 27 // trig 4
#define ECHO_PIN_NORTH    35 // echo 4
#define TRIGGER_PIN_SOUTH 14 // trig 5
#define ECHO_PIN_SOUTH    32 // echo 5
#define TRIGGER_PIN_ROAD  26 // trig 3
#define ECHO_PIN_ROAD     34 // echo 3
#define TRIGGER_PIN_UNDER -1 // trig 1 (disabled)
#define ECHO_PIN_UNDER    -1 // echo 1 (disabled)

// double check implimentation
// (second under-bridge sensor removed)

// Bridge-top sensor (set to -1 to disable)
#ifndef TRIGGER_PIN_BRIDGE_TOP
#define TRIGGER_PIN_BRIDGE_TOP -1
#endif
#ifndef ECHO_PIN_BRIDGE_TOP
#define ECHO_PIN_BRIDGE_TOP -1
#endif

// Pointers for optional sensors (created in setup if pins valid)
// NewPing* sonarUnder2 = nullptr;    // second under-bridge sensor (removed)
NewPing* sonarBridgeTop = nullptr; // top-looking sensor
NewPing* sonarUnder = nullptr;     // primary under-bridge sensor

// Servos
#define SERVO_BRIDGE_PIN 23
#define SERVO_GATE_PIN   22

// Shift register pins
#define LATCH_PIN 5   // RCLK
#define CLOCK_PIN 18  // SCLK
#define DATA_PIN 19   // DATA

// Speaker pins
#define SPEAKER_RX_PIN 17 // RX2
#define SPEAKER_TX_PIN 16 // TX2

// Limit switches
#define LIMIT_GATE_CLOSED_PIN 0   
#define LIMIT_GATE_OPEN_PIN   2  
#define LIMIT_BRIDGE_CLOSED_PIN 15
#define LIMIT_BRIDGE_OPEN_PIN   4 

// Load cell pins & configuration
#define LOADCELL_CLK_PIN 33
#define LOADCELL_DOUT_PIN 36

// Network Configuration
const char* ssid = "Dragan’s iPhone (2)";
const char* password = "BIGPASSWORD";
const int serverPort = 5003;

// Communication
WiFiServer server(serverPort);
WiFiClient client;
bool clientConnected = false;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 1000; // 1 second

// Pins
#define MAX_DISTANCE 500

// Timed bridge movement (ms) hard coded time for bridge movement
#define BRIDGE_MOVE_MS 2000

/// add led stuff below here
#define LEDS_OFF 0
#define LEDS_RED 1
#define LEDS_GREEN 2
byte leds1 = 0;
byte leds2 = 0;

//HX710 scale;
long loadcellTare = 0;
float calibration_factor = 717.5; // adjust to your calibration
const float LOAD_THRESHOLD_GRAMS = 5.0; // anything above this and it counts as on bridge

// Servo & Ultrasonic sensor setup
NewPing sonarNorth(TRIGGER_PIN_NORTH, ECHO_PIN_NORTH, MAX_DISTANCE);
NewPing sonarSouth(TRIGGER_PIN_SOUTH, ECHO_PIN_SOUTH, MAX_DISTANCE);
NewPing sonarRoad(TRIGGER_PIN_ROAD, ECHO_PIN_ROAD, MAX_DISTANCE);
//NewPing sonarUnder(TRIGGER_PIN_UNDER, ECHO_PIN_UNDER, MAX_DISTANCE);
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

bool initializationComplete = false;


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
void updateLEDs();
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

  // Configure limit switch pins
  // pinMode(LIMIT_GATE_CLOSED_PIN, INPUT_PULLUP);
  // pinMode(LIMIT_GATE_OPEN_PIN, INPUT_PULLUP);
  // pinMode(LIMIT_BRIDGE_CLOSED_PIN, INPUT_PULLUP);
  // pinMode(LIMIT_BRIDGE_OPEN_PIN, INPUT_PULLUP);

  // keep ADC resolution call if other sensors need it
  analogReadResolution(12);

  // Initialize optional ultrasonic sensors if pins are valid
  if (TRIGGER_PIN_UNDER != -1 && ECHO_PIN_UNDER != -1) {
    sonarUnder = new NewPing(TRIGGER_PIN_UNDER, ECHO_PIN_UNDER, MAX_DISTANCE);
    Serial.println("Initialized under-bridge ultrasonic sensor");
  }
  // removed sonarUnder2 initialization
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
    initializationComplete = true;
    client.println("OKOK");
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
    // Immediately stop any automatic movements when switching to MANUAL
    stopBridge();
    stopGate();
    // bridgeMoving = false; //redundant?
    // gateMoving = false;
    Serial.println("Switched to MANUAL mode - motors stopped");
    for (int i = 1; i < 15; i++) {
      extract = mssg.substring(i*5, 4);
      if (extract == "OPEN") {
        switch (i){ 
          case 1:
            openBridge();
            Serial.println("MANUAL MODE: Opening Bridge");
            break;

          case 2:
            openGates();
            Serial.println("MANUAL MODE: Opening Gates");
            break;

        }
      } else if (extract == "CLOS"){
        switch (i){
          case 1:
            closeBridge();
            Serial.println("MANUAL MODE: Closing Bridge");
            break;

          case 2:
            closeGates();
            Serial.println("MANUAL MODE: Closing Gates");
            break;
        }
      } else if (extract == "SHIP"){
        switch (i){ 
          case 3:
            currentState.northUS = "SHIP";
            Serial.println("MANUAL MODE: Sensor Override North US");
            break;

          case 4:
            currentState.underUS = "SHIP";
            Serial.println("MANUAL MODE: Sensor Override Under US");
            break;

          case 5:
            currentState.southUS = "SHIP";
            Serial.println("MANUAL MODE: Sensor Override South US");
            break;
        }
      } else if (extract == "TRAF"){
        currentState.roadLoad = "TRAF";
        Serial.println("MANUAL MODE: Sensor Override Load Cell");
        break;
        
      } else if (extract == "TRIG"){
        switch (i){ 
          case 7:
            currentState.roadUS = "TRIG";
            Serial.println("MANUAL MODE: Sensor Override Road US");
            break;

          case 8:
            currentState.bridgeSwitchUp = "TRIG";
            Serial.println("MANUAL MODE: Sensor Override Bridge Limit Switch Up");
            break;

          case 9:
            currentState.bridgeSwitchDown = "TRIG";
            Serial.println("MANUAL MODE: Sensor Override Bridge Limit Switch Down");
            break;

          case 10:
            currentState.gateSwitchUp = "TRIG";
            Serial.println("MANUAL MODE: Sensor Override Gate Limit Switch Up");
            break;

          case 11:
            currentState.gateSwitchDown = "TRIG";
            Serial.println("MANUAL MODE: Sensor Override Gate Limit Switch Down");
            break;
        }
        
      } else if (extract == "GOGO"){
        switch (i){ 
          case 12:
            currentState.roadLights = "GOGO";
            Serial.println("MANUAL MODE: Road Lights Green");
            break;
          case 13:
            currentState.waterwayLights = "GOGO";
            Serial.println("MANUAL MODE: Water Lights Green");
            break;
          
        }
        
      } else if (extract == "STOP"){
        switch (i){ 
          case 12:
            currentState.roadLights = "STOP";
            break;

          case 13:
            currentState.waterwayLights = "STOP";
            Serial.println("MANUAL MODE: Water Lights Red");
            break;
          
        }
        
      } else if (extract == "SLOW"){
        switch (i){ 
          case 12:
            currentState.roadLights = "SLOW";
            Serial.println("MANUAL MODE: Road Lights Slow");
            break;

          case 13:
            currentState.waterwayLights = "SLOW";
            Serial.println("MANUAL MODE: Water Lights Slow");
            break;

        }
        
      } else if (extract == "NONE"){
        switch (i){ 
          case 3:
            currentState.northUS = "NONE";
            Serial.println("MANUAL MODE: Sensor Override North US OFF");
            break;

          case 4:
            currentState.underUS = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Under US OFF");
            break;

          case 5:
            currentState.southUS = "NONE";
            Serial.println("MANUAL MODE: Sensor Override South US OFF");
            break;
          
          case 6:
            currentState.roadLoad = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Load Cell OFF");
            break;

          case 7:
            currentState.roadUS = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Road US OFF");
            break;

          case 8:
            currentState.bridgeSwitchUp = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Bridge Limit Switch Up OFF");
            break;

          case 9:
            currentState.bridgeSwitchDown = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Bridge Limit Switch Down OFF");
            break;

          case 10:
            currentState.gateSwitchUp = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Gate Limit Switch Up OFF");
            break;
          
          case 11:
            currentState.gateSwitchDown = "NONE";
            Serial.println("MANUAL MODE: Sensor Override Gate Limit Switch Down OFF");
            break;

          case 14:
            currentState.speaker = "NONE";
            Serial.println("MANUAL MODE: Speaker OFF");
            break;
        }
        
      } else if (extract == "DONE"){
        currentState.speaker = "DONE";
        Serial.println("MANUAL MODE: Speaker playing DONE");
        
      } else if (extract == "MOVN"){
        if (i = 14) currentState.speaker = "MOVN";
        Serial.println("MANUAL MODE: Speaker playing MOVN");
        
      } else if (extract == "EMER"){
        if (i = 14) currentState.speaker = "EMER";
        Serial.println("MANUAL MODE: Speaker playing EMER");
      }
    }
  }
  // TNK Update lights & sensors, etc currentstate based stuff.
}

// Read and update limit switch states (DISABLED - board issue)
void updateLimitSwitches() {
  // Limit switches disabled - stubbed out. Keep internal flags cleared
  limitGateClosed = false;
  limitGateOpen = false;
  limitBridgeClosed = false;
  limitBridgeOpen = false;

  currentState.gateSwitchDown = "NONE";
  currentState.gateSwitchUp = "NONE";
  currentState.bridgeSwitchDown = "NONE";
  currentState.bridgeSwitchUp = "NONE";
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
  //controlBridge();
  updateLEDs();
  if(initializationComplete == true){
    sendHeartbeat();
  }
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
    readMssg(command);
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
         currentState.roadLoad + " " + currentState.roadUS + " " + currentState.bridgeSwitchUp + " " +
         currentState.bridgeSwitchDown + " " + currentState.gateSwitchUp + " " + currentState.gateSwitchDown + " " +
         currentState.roadLights + " " + currentState.waterwayLights + " " + currentState.speaker + " " +
         String(currentState.errorCode);
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
    Serial.println("waiting to detect Ship");

      if (checkForShips()) {
        Serial.println("Ship detected - waiting for cars to clear...");
        state = WAIT_FOR_CARS_CLEAR;
        stateStartTime = millis();
      }
      break;

    case WAIT_FOR_CARS_CLEAR:
      // Wait a fixed grace period for cars to clear before closing gates
      if (millis() - stateStartTime >= 5000) {
        Serial.println("Grace period elapsed - closing gates...");
        // start closing gates and wait for limit switch
        startGateClose();
        currentState.roadLights = "STOP";
        currentState.waterwayLights = "STOP";
        stateStartTime = millis();
        state = GATES_CLOSING;
      }
      break;

    case GATES_CLOSING:
      // if gate reached closed limit or timed out, stop and proceed
      if (limitGateClosed || (millis() - stateStartTime) > 2000) {
        if (gateMoving) {
          stopGate();
          currentState.gateStatus = "CLOS";
          Serial.println("Gates closed (limit/time)");
        }
        // before opening the bridge ensure no weight sits on the bridge
        float mass = readLoadMass();
        if (isnan(mass)) mass = 0;
        if (mass > LOAD_THRESHOLD_GRAMS) {
          Serial.println("Weight detected on bridge (" + String(mass,1) + " g) - delaying open");
          currentState.roadLoad = "LOAD";
          // go back to waiting for cars to clear so operator can react
          state = WAIT_FOR_CARS_CLEAR;
          stateStartTime = millis();
        } else {
          Serial.println("Gates closed - starting bridge open (limit-driven)...");
          currentState.waterwayLights = "GOGO";
          // small pause to allow gate motor to settle and avoid high current when starting bridge
          delay(500);
          startBridgeOpen();
          stateStartTime = millis();
          state = BRIDGE_OPENING;
        }
      } else {
        // still moving or waiting for limit/time
        if (!gateMoving) startGateClose();
      }
      break;

    case BRIDGE_OPENING:
      // Timed open (limits disabled)
      if (!bridgeMoving) startBridgeOpen();
      if (millis() - stateStartTime >= BRIDGE_MOVE_MS) {
        if (bridgeMoving) {
          stopBridge();
          Serial.println("Bridge open (timed)");
        }
        currentState.bridgeStatus = "OPEN";
        stateStartTime = millis();
        state = BRIDGE_OPEN;
      }
      break;

    case BRIDGE_OPEN:
      if (millis() - stateStartTime > 10000) {
        Serial.println("Waiting for ship under bridge to clear...");
        state = WAIT_FOR_UNDER_CLEAR;
      }
      break;

    case WAIT_FOR_UNDER_CLEAR:
      // Wait until under-bridge sensors AND approach sensors (north/south) are clear,
      // and the bridge is confirmed open (by bridgeStatus or bridge-top sensor).
      {
        bool underClear = !checkUnderBridge();        // true when no ship under
        bool approachesClear = !checkForShips();      // true when north/south approach sensors clear

        if (underClear && approachesClear) {
          // confirm bridge is fully open before attempting to close
          bool bridgeConfirmedOpen = (currentState.bridgeStatus == "OPEN");
          if (!bridgeConfirmedOpen) {
            bridgeConfirmedOpen = checkBridgeTop(); // check bridge-top sensor if present
          }

          if (bridgeConfirmedOpen) {
            Serial.println("Under & approach sensors clear and bridge confirmed open - closing bridge...");
            currentState.waterwayLights = "STOP";
            startBridgeClose();
            stateStartTime = millis();
            state = BRIDGE_CLOSING;
          } else {
            // Bridge not confirmed open yet; keep waiting
            Serial.println("Bridge not yet confirmed open; waiting before close...");
            stateStartTime = millis();
          }
        } else {
          Serial.println("Waiting for under-bridge and approach sensors to clear...");
          stateStartTime = millis(); // reset timer
        }
      }
      break;

    case BRIDGE_CLOSING:
      // Timed close (limits disabled)
      if (!bridgeMoving) startBridgeClose();
      if (millis() - stateStartTime >= BRIDGE_MOVE_MS) {
        if (bridgeMoving) {
          stopBridge();
          Serial.println("Bridge closed (timed)");
        }
        currentState.bridgeStatus = "CLOS";
        // small pause to allow bridge motor to settle before starting gate movement
        delay(500);
        // open gates after bridge fully closed
        startGateOpen();
        currentState.roadLights = "GOGO";
        stateStartTime = millis();
        state = GATES_OPENING;
      }
      break;

    case GATES_OPENING:
      if (limitGateOpen || (millis() - stateStartTime) > 2000) {
        if (gateMoving) {
          stopGate();
          currentState.gateStatus = "OPEN";
          Serial.println("Gates opened");
        }
        state = WAIT_FOR_SHIPS;
      } else {
        if (!gateMoving) startGateOpen();
      }
      break;
  }
}

void openBridge() {
  // Timed manual open
  const unsigned long timeoutMs = BRIDGE_MOVE_MS;
  Serial.println("Manual: Opening bridge (timed)...");
  if (currentMode == EMERGENCY_MODE) {
    Serial.println("Manual open blocked: EMERGENCY mode");
    return;
  }
  bridgeServo.write(120);
  delay(timeoutMs);
  bridgeServo.write(90);
  currentState.bridgeStatus = "OPEN";
  Serial.println("Manual: Bridge opened (timed)");
}

void closeBridge() {
  // Timed manual close
  const unsigned long timeoutMs = BRIDGE_MOVE_MS;
  Serial.println("Manual: Closing bridge (timed)...");
  if (currentMode == EMERGENCY_MODE) {
    Serial.println("Manual close blocked: EMERGENCY mode");
    return;
  }
  bridgeServo.write(60);
  delay(timeoutMs);
  bridgeServo.write(90);
  currentState.bridgeStatus = "CLOS";
  Serial.println("Manual: Bridge closed (timed)");
}

void closeGates(){ 
  // Timed gate close (2 seconds)
  currentState.gateStatus = "CLOS";
  gateServo.write(60);
  delay(2000);
  gateServo.write(90);
  Serial.println("Gates closed");
}

void openGates(){
  // Timed gate open (2 seconds)
  currentState.gateStatus = "OPEN";
  gateServo.write(120);
  delay(2000);
  gateServo.write(90);
  Serial.println("Gates opened");
}

// Emergency stop function
void emergencyStop() {
  // Immediately stop all motors
  stopBridge(); // Stop bridge motor
  stopGate();   // Stop gate motor

  // Set current mode to EMERGENCY
  currentMode = EMERGENCY_MODE;


// just in case we want to disconnect clients during emergency 
  // Close client connection if present (prevent further commands)
  // if (clientConnected) {
  //   if (client) client.stop();
  //   clientConnected = false;
  //   Serial.println("Client disconnected due to EMERGENCY");
  // }

  // Set all status to emergency
  currentState.bridgeStatus = "EMER";
  currentState.gateStatus = "EMER";
  currentState.roadLights = "EMER";
  currentState.waterwayLights = "EMER";
  currentState.speaker = "EMER";
  currentState.errorCode = 7; // Emergency error code

  // Update LEDs to reflect emergency state
  updateLEDs();

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

// all cm are placeholders
// Detection thresholds (cm)
#define SHIP_DETECT_CM 30
#define CAR_DETECT_CM 5
#define UNDER_DETECT_CM 20

#define BRIDGE_TOP_CLOSED_THRESHOLD_CM 5  // reading <= this -> bridge closed (near)
#define BRIDGE_TOP_OPEN_THRESHOLD_CM   15  // reading >= this or out-of-range -> bridge open (raised)

// Check if ships are detected (north/south)
bool checkForShips() {
  if (currentMode == MANUAL_MODE) return false;

  int distanceNorth = sonarNorth.ping_cm();
  int distanceSouth = sonarSouth.ping_cm();
  bool shipDetected = false;

  if (distanceNorth > 0 && distanceNorth < SHIP_DETECT_CM) {
    currentState.northUS = "SHIP";
    shipDetected = true;
  } else {
    currentState.northUS = "NONE";
  }

  if (distanceSouth > 0 && distanceSouth < SHIP_DETECT_CM) {
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
  if (distanceRoad > 0 && distanceRoad < CAR_DETECT_CM) {
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

  int distanceUnder1 = -1;
  if (sonarUnder != nullptr) distanceUnder1 = sonarUnder->ping_cm();
  if (distanceUnder1 > 0 && distanceUnder1 < UNDER_DETECT_CM) {
    currentState.underUS = "SHIP";
    detected = true;
  } else {
    currentState.underUS = "NONE";
  }

  // second under-bridge sensor removed

  return detected;
}

// Check bridge-top sensor to determine bridge height (OPEN/CLOSED/PART/NONE)
bool checkBridgeTop() {
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

  currentState.bridgeTopUS = "PART"; // partial
  return false;
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
      //leds1 += 0b01000000;
      bitSet(leds1, 6);
      break;

    case LEDS_GREEN:
      bitSet(leds1, 7);
      break;
  }
  switch (south) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      bitSet(leds1, 4);
      break;

    case LEDS_GREEN:
      bitSet(leds1, 5);
      break;
  }
  switch (west) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      bitSet(leds1, 2);
      break;

    case LEDS_GREEN:
      bitSet(leds1, 3);
      break;
  }
  switch (east) {
    case LEDS_OFF:
      break;
    
    case LEDS_RED:
      bitSet(leds1, 0);
      break;

    case LEDS_GREEN:
      bitSet(leds1, 1);
      break;
  }
  switch (errorCode) {
    case 0:
      break;
    
    case 1:
      bitSet(leds2, 3);
      break;

    case 2:
      bitSet(leds2, 4);
      break;

    case 3:
      bitSet(leds2, 3);
      bitSet(leds2, 4);
      break;

    case 4:
      bitSet(leds2, 5);
      break;

    case 5:
      bitSet(leds2, 3);
      bitSet(leds2, 5);
      break;

    case 6:
      bitSet(leds2, 4);
      bitSet(leds2, 5);
      break;

    case 7:
      bitSet(leds2, 3);
      bitSet(leds2, 4);
      bitSet(leds2, 5);
      break;
  }
  updateShiftRegister();
}

void testLEDs(){
  currentState.waterwayLights = "GOGO";
  currentState.roadLights = "STOP";
  updateLEDs();
  delay(3000);
  currentState.waterwayLights = "STOP";
  currentState.roadLights = "GOGO";
  updateLEDs();
  delay(3000);
  currentState.waterwayLights = "SLOW";
  currentState.roadLights = "SLOW";
}

void ErrorDisplay(){

}

void updateLEDs() {
  char roadLights = LEDS_OFF;
  char waterwayLights = LEDS_OFF;

  if (currentState.roadLights == "GOGO") {
    roadLights = LEDS_GREEN;
  } else if (currentState.roadLights == "STOP") {
    roadLights = LEDS_RED;
  } else if (currentState.roadLights == "SLOW") {
    if ((millis()/1000) % 2 == 0) {
      roadLights = LEDS_RED; 
    } else roadLights = LEDS_OFF;
  }

  if (currentState.waterwayLights == "GOGO") {
    waterwayLights = LEDS_GREEN;
  } else if (currentState.waterwayLights == "STOP") {
    waterwayLights = LEDS_RED;
  } else if (currentState.waterwayLights == "SLOW") {
    if ((millis()/1000) % 2 == 0) {
      waterwayLights = LEDS_RED; 
    } else waterwayLights = LEDS_OFF;
  }
  setLEDs(waterwayLights,waterwayLights,roadLights,roadLights,currentState.errorCode);
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
    int u = (sonarUnder != nullptr) ? sonarUnder->ping_cm() : -1;
    int t = (sonarBridgeTop != nullptr) ? sonarBridgeTop->ping_cm() : -1;
    Serial.print("North:" + String(n) + " cm, South:" + String(so) + " cm, Road:" + String(r) + " cm, Under:" + String(u) + " cm");
    if (t != -1) Serial.print(", Top:" + String(t) + " cm");
    Serial.println();
    delay(delayMs);
  }
  Serial.println("[TEST] Ultrasonics done.");
}

// Test limit switches: read and print states
void testLimitSwitches(int iterations = 1, int delayMs = 200) {
  Serial.println("[TEST] Limit switches: polling states (digital)...");
  for (int i = 0; i < iterations; i++) {
    // Read raw digital values (active-low)
    int rawGateClosed = digitalRead(LIMIT_GATE_CLOSED_PIN);
    int rawGateOpen = digitalRead(LIMIT_GATE_OPEN_PIN);
    int rawBridgeClosed = digitalRead(LIMIT_BRIDGE_CLOSED_PIN);
    int rawBridgeOpen = digitalRead(LIMIT_BRIDGE_OPEN_PIN);

    // Interpret (LOW == triggered)
    bool gateClosedTrig = (rawGateClosed == LOW);
    bool gateOpenTrig = (rawGateOpen == LOW);
    bool bridgeClosedTrig = (rawBridgeClosed == LOW);
    bool bridgeOpenTrig = (rawBridgeOpen == LOW);

    // Update internal states as well
    limitGateClosed = gateClosedTrig;
    limitGateOpen = gateOpenTrig;
    limitBridgeClosed = bridgeClosedTrig;
    limitBridgeOpen = bridgeOpenTrig;

    currentState.gateSwitchDown = limitGateClosed ? "TRIG" : "NONE";
    currentState.gateSwitchUp = limitGateOpen ? "TRIG" : "NONE";
    currentState.bridgeSwitchDown = limitBridgeClosed ? "TRIG" : "NONE";
    currentState.bridgeSwitchUp = limitBridgeOpen ? "TRIG" : "NONE";

    // Print diagnostic
    Serial.print("gate closed limit switch:"); Serial.println(rawGateClosed);
    //Serial.print(" GC_TRIG:"); Serial.print(limitGateClosed);
    Serial.print("gate open limit switch:"); Serial.println(rawGateOpen);
    //Serial.print(" GO_TRIG:"); Serial.print(limitGateOpen);
    Serial.print("bridge closed limit switch:"); Serial.println(rawBridgeClosed);
    //Serial.print(" BC_TRIG:"); Serial.print(limitBridgeClosed);
    Serial.print("bridge open limit switch:"); Serial.println(rawBridgeOpen);
    //Serial.print(" BO_TRIG:"); Serial.println(limitBridgeOpen);

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

void testMotors(){
    Serial.println("[TEST] motors: testing gates and bridge");
  unsigned long t0;

  // Gates: open then close
  Serial.println("[TEST] Opening gates...");
  startGateOpen();
  delay(5000);
  stopGate();
  Serial.println("Gate open ");
  delay(2000);

  Serial.println("[TEST] Closing gates...");
  startGateClose();
  delay(5000);
  stopGate();
  Serial.println("Gate closed ");
  delay(2000);

  // Bridge: open then close
  Serial.println("[TEST] Opening bridge...");
  startBridgeOpen();
  delay(5000);
  stopBridge();
  Serial.println("Bridge open ");
  delay(500);

  Serial.println("[TEST] Closing bridge...");
  startBridgeClose();
  delay(5000);
  stopBridge();
  Serial.println("Bridge closed ");
  delay(500);

  Serial.println("[TEST] motors done.");
}

// Run all tests in sequence
void runAllTests() {
  //testSpeaker();
  //testUltrasonics();
  //testLimitSwitches();
  //testActuators(); // motors with limit switches
  //testLEDs();
  testMotors();
}


void test() {
  // Do not run tests in emergency mode
  if (currentMode == EMERGENCY_MODE) {
    Serial.println("Test skipped: system in EMERGENCY mode");
    return;
  }
  Serial.println("---------- BEGIN TEST SUITE ----------");
  runAllTests();
  Serial.println("---------- END TEST SUITE ----------");
  // Pause long so routine is not re-run unless user triggers
  delay(20000);
}