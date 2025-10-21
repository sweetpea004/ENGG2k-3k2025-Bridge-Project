#include <WiFi.h>
#include <string.h>
#include <WiFiServer.h>
#include <ESP32Servo.h>  // servo control
#include <NewPing.h>  // ultrasonic sensor 

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
#define TRIGGER_PIN_NORTH  13
#define ECHO_PIN_NORTH     12
#define TRIGGER_PIN_SOUTH  14
#define ECHO_PIN_SOUTH     15
#define TRIGGER_PIN_ROAD   16
#define ECHO_PIN_ROAD      17
#define TRIGGER_PIN_UNDER  18
#define ECHO_PIN_UNDER     19
#define MAX_DISTANCE 500

#define SERVO_BRIDGE_PIN 23
#define SERVO_GATE_PIN 22

/// add led stuff below here
#define LATCH_PIN 22
#define CLOCK_PIN 23
#define DATA_PIN 24
#define LEDS_OFF 0
#define LEDS_RED 1
#define LEDS_GREEN 2
byte leds1 = 0;
byte leds2 = 0;

// Servo & Ultrasonic sensor setup
NewPing sonarNorth(TRIGGER_PIN_NORTH, ECHO_PIN_NORTH, MAX_DISTANCE);
NewPing sonarSouth(TRIGGER_PIN_SOUTH, ECHO_PIN_SOUTH, MAX_DISTANCE);
NewPing sonarRoad(TRIGGER_PIN_ROAD, ECHO_PIN_ROAD, MAX_DISTANCE);
NewPing sonarUnder(TRIGGER_PIN_UNDER, ECHO_PIN_UNDER, MAX_DISTANCE);
Servo bridgeServo; // Servo object
Servo gateServo; // Servo object

// Speaker / Voice module (uses Serial2 on ESP32)
#define SPEAKER_RX_PIN 27
#define SPEAKER_TX_PIN 26

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

// error codes:
// 0: No Error
// 1: Bridge limit switch not detecting bridge
// 2: Gates limit switch not detecting gates
// 3: Gates & Bridge
// 4: Road traffic detection error
// 5: Road traffic & Bridge
// 6: Gates & Road Traffic 
// 7: All errors

//String interpretation
void readMssg(String mssg) {
  // PRE: receive a string in the Appendix E format
  // POST: read the message and run the appropriate response command
  switch (mssg.substr(0,4)) {
    case "REDY":
      // send OKOK to frontend

    case "OKOK":
      // Nothing

    case "AUTO":
      // put the bridge into automatic mode
      if (currentMode == EMERGENCY_MODE) {
      //"ERROR: Cannot switch from EMERGENCY mode - reset required"
        return;
      }
      currentMode = AUTO_MODE;
      resetBridgeControlState(); // Reset state machine when switching to auto
      Serial.println("Switched to AUTO mode");

    case "EMER":
      // put the bridge into emergency mode
      currentMode = EMERGENCY_MODE;
      emergencyStop();

    case "PUSH":
      // 1 to 14
      currentMode = MANUAL_MODE;
      for (int i = 1; i < 15; i++) {
        String extract = mssg.substr(i*5, 4);
        switch (extract) {
          case "OPEN": 
            switch (i){ 
              case 1:
                openBridge();

              case 2:
                openGates();
            }

          case "CLOS":
            switch (i){
              case 1:
                closeBridge();
              case 2:
                closeGates();
            }

          case "SHIP":
            switch (i){ 
              case 3:
                currentState.northUS = "SHIP";

              case 4:
                currentState.underUS = "SHIP";

              case 5:
                currentState.southUS = "SHIP";

            }

          case "TRAF":
            switch (i){ 
              case 6:
                currentState.roadUS = "TRAF";

              case 7:
                currentState.roadLoad = "TRAF";

            }

          case "TRIG":
            switch (i){ 
              case 8:

              case 9:

              case 10:

              case 11:

            }

          case "GOGO":
            switch (i){ 
              case 12:
                currentState.roadLights = "GOGO";
              case 13:
                currentState.waterwayLights = "GOGO";
              
            }

          case "STOP":
            switch (i){ 
              case 12:
                currentState.roadLights = "STOP";

              case 13:
                currentState.waterwayLights = "STOP";
              
            }

          case "SLOW":
            switch (i){ 
              case 12:
                currentState.roadLights = "SLOW";

              case 13:
                currentState.waterwayLights = "SLOW";

            }

          case "NONE":
            switch (i){ 
              case 3:

              case 4:

              case 5:
              
              case 6:

              case 7:

              case 8:

              case 9:

              case 10:
              
              case 11:

              case 14:
            }
        }       
      }
  }
}

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
  String audioSys = "NONE";
  String speaker = "placeholder";
  int errorCode = 0;
  // add sections for limit switches
};

BridgeState currentState;

// Control Mode Enum
enum ControlMode {
  AUTO_MODE,
  MANUAL_MODE,
  EMERGENCY_MODE
};

ControlMode currentMode = AUTO_MODE;

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

  delay(1000);
}

// Main Loop
void loop() {
  //test();
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
    String command = client.readStringUntil('\n');
    command.trim();
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
         currentState.roadLoad + " " + currentState.roadLights + " " + currentState.waterwayLights + " " + 
         String(currentState.errorCode);
}

// Control the bridge states (open/close)
void controlBridge() {
  // Don't run automatic control in manual or emergency mode
  if (currentMode != AUTO_MODE) return;

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
        closeGates();
        currentState.roadLights = "STOP";
        currentState.waterwayLights = "STOP";
        stateStartTime = millis();
        state = GATES_CLOSING;
      }
      break;

    case GATES_CLOSING:
      if (millis() - stateStartTime > 3000) {
        Serial.println("Gates closed - opening bridge...");
        currentState.waterwayLights = "GOGO";
        openBridge();
        stateStartTime = millis();
        state = BRIDGE_OPENING;
      }
      break;

    case BRIDGE_OPENING:
      if (millis() - stateStartTime > 2000) {
        Serial.println("Bridge is open.");
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
      if (!checkUnderBridge()) {
        Serial.println("No ship under bridge - closing bridge...");
        currentState.waterwayLights = "STOP";
        closeBridge();
        stateStartTime = millis();
        state = BRIDGE_CLOSING;
      } else {
        Serial.println("Ship still under bridge - waiting...");
        stateStartTime = millis(); // reset timer
      }
      break;

    case BRIDGE_CLOSING:
      if (millis() - stateStartTime > 2000) {
        Serial.println("Bridge closed - opening gates for traffic...");
        openGates();
        currentState.roadLights = "GOGO";
        stateStartTime = millis();
        state = GATES_OPENING;
      }
      break;

    case GATES_OPENING:
      if (millis() - stateStartTime > 2000) {
        Serial.println("Gates open - waiting for next ship...");
        state = WAIT_FOR_SHIPS;
      }
      break;
  }
}

void openBridge() {
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
  if (currentMode = MANUAL_MODE) {
    return false;
  }
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
  if (currentMode = MANUAL_MODE) {
    return false;
  }
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
  if (currentMode = MANUAL_MODE) {
    return false;
  }
  int distanceUnder = sonarUnder.ping_cm();
  if (distanceUnder > 0 && distanceUnder < 30) {
    currentState.underUS = "SHIP";
    return true;
  } else {
    currentState.underUS = "NONE";
    return false;
  }
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
  currentState.audioSys = "EMER";
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
      leds2 += 000b100000;

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


////////////// testing stuff /////////////////////

void test(){
  //test motor
  Serial.println("testing motors");
  testMotors();
  Serial.println("finished testing motors");
  // test sensor
  Serial.println("testing sensor");
  for(int i=0; i<=20; i++){
    checkForShips();
    delay(1000);
  }
  //
  Serial.println("finished all testing");
    delay(10000);
}

void testMotors(){
  openBridge();
  delay(5000);
  closeBridge();
  delay(5000);
}
