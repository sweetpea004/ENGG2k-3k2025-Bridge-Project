#include <WiFi.h>
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
#define TRIGGER_PIN  13
#define ECHO_PIN     12
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
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
Servo bridgeServo; // Servo object
Servo gateServo; // Servo object

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
// Message readMssg(String mssg) {
//   // PRE: receive a string in the Appendix E format
//   // POST: output a message struct with the string contents
//   Message retMsg;
//   retMsg.msgType = mssg.substr(0,5);
//   retMsg.bridgeStatus = mssg.substr(5,5);
//   retMsg.gateStatus = mssg.substr(10,5);
//   retMsg.northUS = mssg.substr(15,5);
//   retMsg.underUS = mssg.substr(20,5);
//   retMsg.southUS = mssg.substr(25,5);
//   retMsg.roadLoad = mssg.substr(30,5);
//   retMsg.roadLights = mssg.substr(35,5);
//   retMsg.waterwayLights = mssg.substr(40,5);
//   retMsg.audioSys = mssg.substr(45,5);
//   retMsg.errorCode = mssg[50] = '0';
//   return retMsg;
// }

// Bridge State Structure
struct BridgeState {
  String bridgeStatus = "CLOS";
  String gateStatus = "OPEN";
  String northUS = "NONE";
  String underUS = "NONE";
  String southUS = "NONE";
  String roadLoad = "NONE";
  String roadLights = "GOGO";
  String waterwayLights = "STOP";
  String audioSys = "NONE";
  String speaker = "placeholder";
  int errorCode = 0;
};

struct Message {
  String msgType = "OKOK";
  String bridgeStatus = "CLOS";
  String gateStatus = "OPEN";
  String northUS = "NONE";
  String underUS = "NONE";
  String southUS = "NONE";
  String roadLoad = "NONE";
  String roadLights = "GOGO";
  String waterwayLights = "STOP";
  String audioSys = "NONE";
  int errorCode = 0;
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
  //handleClient();
  //controlBridge();
  //sendHeartbeat();
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
    Serial.println(command);
    processCommand(command);
  }
}

// Process commands from the client
void processCommand(String command) {
  if (command == "REDY") {
    client.println("OKOK");
  } else if (command == "EMER") {
    currentMode = EMERGENCY_MODE;
    emergencyStop();
    client.println("OK");
  } else if (command == "AUTO") {
    if (currentMode == EMERGENCY_MODE) {
      client.println("ERROR: Cannot switch from EMERGENCY mode - reset required");
      return;
    }
    currentMode = AUTO_MODE;
    resetBridgeControlState(); // Reset state machine when switching to auto
    Serial.println("Switched to AUTO mode");
    client.println("OK");
  } else if (command == "MANUAL") {
    if (currentMode == EMERGENCY_MODE) {
      client.println("ERROR: Cannot switch from EMERGENCY mode - reset required");
      return;
    }
    currentMode = MANUAL_MODE;
    Serial.println("Switched to MANUAL mode");
    client.println("OK");
  } else if (command.startsWith("PUSH ")) {
    if (currentMode == MANUAL_MODE) {
      handleManualCommand(command);
    } else {
      client.println("ERROR: Manual commands only allowed in MANUAL mode");
    }
  } else {
    client.println("ERROR: Unknown command");
  }
}

// Handle manual commands when in MANUAL mode
void handleManualCommand(String command) {
  // Don't allow manual commands in emergency mode
  if (currentMode == EMERGENCY_MODE) {
    client.println("ERROR: Manual commands blocked in EMERGENCY mode");
    return;
  }
  
  // Extract the manual command (e.g., "PUSH BRIDGE_OPEN", "PUSH GATE_CLOSE")
  String action = command.substring(5); // Remove "PUSH " prefix
  
  if (action == "BRIDGE_OPEN") {
    openBridge();
    client.println("OK");
  } else if (action == "BRIDGE_CLOSE") {
    closeBridge();
    client.println("OK");
  } else if (action == "GATE_OPEN") {
    openGates();
    client.println("OK");
  } else if (action == "GATE_CLOSE") {
    closeGates();
    client.println("OK");
  } else if (action == "LIGHTS_ROAD_GO") {
    currentState.roadLights = "GOGO";
    client.println("OK");
  } else if (action == "LIGHTS_ROAD_STOP") {
    currentState.roadLights = "STOP";
    client.println("OK");
  } else if (action == "LIGHTS_WATER_GO") {
    currentState.waterwayLights = "GOGO";
    client.println("OK");
  } else if (action == "LIGHTS_WATER_STOP") {
    currentState.waterwayLights = "STOP";
    client.println("OK");
  } else {
    client.println("ERROR: Unknown manual command");
  }
}

// Send heartbeat to client
void sendHeartbeat() {
  if (clientConnected && (millis() - lastHeartbeat >= heartbeatInterval)) {
    String status = buildStatusMessage();
    client.println(status);
    lastHeartbeat = millis();
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

  static enum { IDLE, PREPARE, BRIDGE_OPEN, BRIDGE_CLOSE } state = IDLE;
  static unsigned long stateStartTime = 0;

  switch (state) {
    case IDLE:
      if (checkForShips()) {
        // Ship detected, prepare to open the bridge
        Serial.println("Ship detected - Preparing to open bridge...");
        closeGates(); // Close gates
        currentState.roadLights = "STOP";       // Stop road traffic
        currentState.waterwayLights = "STOP";   // Stop waterway traffic
        stateStartTime = millis();
        state = PREPARE;
      }
      break;

    case PREPARE:
      closeGates(); 
      if (millis() - stateStartTime > 3000) {  // Simulate time to close gates
        // Gates are closed, now open the bridge
        Serial.println("Opening bridge...");
        currentState.waterwayLights = "GOGO";  // Allow ships to pass
        openBridge(); 
        stateStartTime = millis();
        state = BRIDGE_OPEN;
      }
      break;

    case BRIDGE_OPEN:
      if (millis() - stateStartTime > 10000) {  // Keep bridge open for 10 seconds (make this longer
        // Time to close the bridge
        Serial.println("Closing bridge...");
        currentState.waterwayLights = "STOP";  // Stop waterway traffic
        closeBridge(); //close bridge
        stateStartTime = millis();
        state = BRIDGE_CLOSE;
      }
      break;

    case BRIDGE_CLOSE:
      if (millis() - stateStartTime > 3000) {  // Simulate time to close bridge and reopen traffic
        // Gates open again for traffic
        Serial.println("Reopening gates for traffic...");
<<<<<<< HEAD
        openGates();
=======
        openGates(); // Open gates
>>>>>>> 3d190d6f501f77cdfbeafeada647e6fdb3be8d64
        currentState.roadLights = "GOGO";      // Green for road traffic
        state = IDLE;
      }
      break;
  }
}

void openBridge() {
  currentState.bridgeStatus = "OPEN";
  bridgeServo.write(120);
  delay(2000);
  bridgeServo.write(90);
  currentState.bridgeStatus = "OPEN";  // Update heartbeat status
  Serial.println("Bridge opened");
}

void closeBridge() {
  currentState.bridgeStatus = "CLOS";
  bridgeServo.write(60);
  delay(2000);
  bridgeServo.write(90);
  currentState.bridgeStatus = "CLOS";  // Update heartbeat status
  Serial.println("Bridge closed");
}

// Check if ships are detected
bool checkForShips() {
  int distance = sonar.ping_cm();
  // Check ultrasonic sensors for approaching ships
  if (distance > 0 && distance < 30) {
    currentState.northUS = "SHIP";  // Ship detected
    //Serial.println("ship detected"); /// for testing
    return true;
  } else {
    currentState.northUS = "NONE";  // No ship
    //Serial.println("no ship detected"); /// for testing
    return false;
  }
}

void closeGates(){
  currentState.gateStatus = "CLOS";
  gateServo.write(60);
  delay(2000); 
  gateServo.write(90);
  currentState.gateStatus = "CLOS";  // Update heartbeat status
  Serial.println("Gates closed");
}

void openGates(){
  currentState.gateStatus = "OPEN";
  gateServo.write(120);
  delay(2000);
  gateServo.write(90);
  currentState.gateStatus = "OPEN";  // Update heartbeat status
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
    leds1 += 0b01000000;

    case LEDS_GREEN:
    leds1 += 0b10000000;
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
    leds2 += 0b0010000;

    case 2:
    leds2 += 0b01000000;

    case 3:
    leds2 += 0b01100000;

    case 4:
    leds2 += 0b10000000;

    case 5:
    leds2 += 0b10100000;

    case 6:
    leds2 += 0b11000000;

    case 7:
    leds2 += 0b11100000;
  }
  updateShiftRegister();
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
