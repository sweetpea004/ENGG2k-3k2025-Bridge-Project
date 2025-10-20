#include <Arduino.h>
#include "HX710.h"

const int DOUT = 16;
const int CLK = 3;

HX710 scale;
long tareOffset = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Loadcell Calibration Moment");

  // get the scale ready
  scale.initialize(CLK, DOUT);

  // zeroing this thing
  Serial.println("Zeroing Loadcell...");
  long tareSum = 0;
  for (int i = 0; i < 20; i++)
  {
    while (!scale.isReady())
      ;
    scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
    tareSum += scale.getLastDifferentialInput();
    delay(10);
  }
  tareOffset = tareSum / 20;
  Serial.println("Loadcell Zero'd. Put your known mass on the scale now :3");
}

void loop()
{
  while (!scale.isReady())
    ;

  // read the raw value after taring
  scale.readAndSelectNextData(HX710_DIFFERENTIAL_INPUT_40HZ);
  long rawReading = scale.getLastDifferentialInput();
  long adjustedValue = rawReading - tareOffset;
  
  // this is the number you need to write down
  Serial.print("Tared: ");
  Serial.println(adjustedValue);
  delay(250);
}