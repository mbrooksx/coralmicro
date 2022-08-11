#include "Arduino.h"

int analogPin = A0;
int val = 0;

void setup() {
  Serial.begin(115200);
  // Turn on Status LED to show the board is on.
  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);
  Serial.println("Arduino Analog Read!");

  analogReadResolution(12);
}

void loop() {
  val = analogRead(analogPin);
  Serial.println(val);
}
