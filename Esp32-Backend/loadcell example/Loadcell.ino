#include <Arduino.h>
#include "HX710.h"

const int DOUT = 16; //gpio 16
const int CLK = 3; //gpio 3

// ------- calibration_factor = [Your Recorded Number] / [Known mass number]
float calibration_factor = 717.5; // calibrateion_factor = 374450 / 514g

HX710 scale;
long tareOffset = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Loadcell Start . . .");

  //initialise that puppy
  scale.initialize(CLK, DOUT);
  Serial.println("Loadcell Initialized.");

  // tare it
  Serial.println("Zeroing Loadcell. . .");
  
  long tareSum = 0;
  for (int i = 0; i < 20; i++) {
    while (!scale.isReady());

    scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
    tareSum += scale.getLastDifferentialInput();
    delay(10);
  }
  tareOffset = tareSum / 20;

  Serial.print("Loadcell Zero'd. Offset value is: ");
  Serial.println(tareOffset);
}

void loop() {
  while (!scale.isReady());

  // read the value
  scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
  long rawReading = scale.getLastDifferentialInput();
  long actualReading = rawReading - tareOffset;

  // calculate the mass
  float mass = actualReading / calibration_factor;

  // If the mass is negative, treat it as zero
  if (mass < 0.0) {
    mass = 0.0;
  }

  Serial.print("Mass: ");
  Serial.print(mass, 1); // print with 1 decimal place
  Serial.println(" g");

  delay(100);
}