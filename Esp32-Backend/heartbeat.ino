#include <WiFi.h>
#include <WiFiServer.h>
#include <ESP32Servo.h>  // Added for servo control
#include <NewPing.h>  // Added for ultrasonic sensor

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
#define SERVO_PIN 15
#define MOTOR_PIN1  12
#define MOTOR_PIN2  13
#define MOTOR_ENABLE_PIN 14

// Servo & Ultrasonic sensor setup
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
Servo myservo; // Servo object

// error codes:
// 0: No Error
// 1: Bridge Hung
// 2: Gates Hung
// 3: Lights Hung
// 4: Ultrasonics Hung
// 5: 
// 6: 
// 7: 
// 8: 

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
bool autoMode = true;

// Setup
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Bridge Control - Heartbeat Program");

  // Initialize motor pins
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_ENABLE_PIN, OUTPUT);

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

  // Setup the servo
  myservo.attach(SERVO_PIN, 500, 2500); // Attach servo with range
  myservo.write(90);  // Initial position (bridge closed)
  delay(1000);
}

// Main Loop
void loop() {
  handleClient();
  controlBridge();
  sendHeartbeat();
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
    emergencyStop();
    client.println("OK");
  } else if (command == "AUTO") {
    autoMode = true;
    client.println("OK");
  } else if (command.startsWith("PUSH ")) {
    // Manual control logic here (e.g., control motor or servo manually)
    client.println("OK");
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
  if (!autoMode) return;

  static enum { IDLE, PREPARE, BRIDGE_OPEN, BRIDGE_CLOSE } state = IDLE;
  static unsigned long stateStartTime = 0;

  switch (state) {
    case IDLE:
      if (checkForShips()) {
        // Ship detected, prepare to open the bridge
        Serial.println("Ship detected - Preparing to open bridge...");
        currentState.gateStatus = "CLOS";       // Close gates
        currentState.roadLights = "STOP";       // Stop road traffic
        currentState.waterwayLights = "STOP";   // Stop waterway traffic
        stateStartTime = millis();
        state = PREPARE;
      }
      break;

    case PREPARE:
      if (millis() - stateStartTime > 3000) {  // Simulate time to close gates
        // Gates are closed, now open the bridge
        Serial.println("Opening bridge...");
        currentState.bridgeStatus = "OPEN";    // Open bridge
        currentState.waterwayLights = "GOGO";  // Allow ships to pass
        openBridge();  // Trigger servo and motors
        stateStartTime = millis();
        state = BRIDGE_OPEN;
      }
      break;

    case BRIDGE_OPEN:
      if (millis() - stateStartTime > 10000) {  // Keep bridge open for 10 seconds
        // Time to close the bridge
        Serial.println("Closing bridge...");
        currentState.bridgeStatus = "CLOS";    // Close bridge
        currentState.waterwayLights = "STOP";  // Stop waterway traffic
        closeBridge();  // Trigger servo and motors
        stateStartTime = millis();
        state = BRIDGE_CLOSE;
      }
      break;

    case BRIDGE_CLOSE:
      if (millis() - stateStartTime > 3000) {  // Simulate time to close bridge and reopen traffic
        // Gates open again for traffic
        Serial.println("Reopening gates for traffic...");
        currentState.gateStatus = "OPEN";      // Open gates
        currentState.roadLights = "GOGO";      // Green for road traffic
        state = IDLE;
      }
      break;
  }
}

void openBridge() {
  myservo.write(120);  // Move servo to open position
  digitalWrite(MOTOR_PIN1, HIGH);
  digitalWrite(MOTOR_PIN2, LOW);
  digitalWrite(MOTOR_ENABLE_PIN, HIGH); // Enable motor
  delay(2000); // Simulate motor running for opening
}

void closeBridge() {
  myservo.write(90);   // Move servo to closed position
  digitalWrite(MOTOR_PIN1, LOW);
  digitalWrite(MOTOR_PIN2, HIGH);
  digitalWrite(MOTOR_ENABLE_PIN, HIGH); // Enable motor
  delay(2000); // Simulate motor running for closing
}

// Check if ships are detected
bool checkForShips() {
  // Check ultrasonic sensors for approaching ships
  bool northShip = (currentState.northUS != "NONE");
  bool southShip = (currentState.southUS != "NONE");
  return northShip || southShip;
}

// Emergency stop function
void emergencyStop() {
  myservo.write(90);  // Return servo to default position
  digitalWrite(MOTOR_ENABLE_PIN, LOW);  // Stop the motor
  currentState.bridgeStatus = "EMER";
  currentState.gateStatus = "EMER";
  currentState.roadLights = "EMER";
  currentState.waterwayLights = "EMER";
  currentState.speaker = "EMER";
}
