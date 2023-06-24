#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
#endif

#include "SinricPro.h"
#include "SinricProSwitch.h"

#define WIFI_SSID         "Dialog 4G 492"
#define WIFI_PASS         "Madu@1105"
#define APP_KEY           "c187674a-3a52-40d9-81d6-e9613391b6e4"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "3ed12387-5077-4a28-a239-2477effc6ec6-e9a6343e-db20-42b0-804d-2c4395dec443"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define SWITCH_ID         "648a0e1c929949c1da84cbcf"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE         9600                // Change baudrate to your need

const int touchPin = D2;      // Pin connected to the touch sensor (D2 on NodeMCU)
const int motorPin1 = D6;    // Pin connected to IN1 of the motor driver (D6 on NodeMCU)
const int motorPin2 = D7;    // Pin connected to IN2 of the motor driver (D7 on NodeMCU)

bool motorRunning = false;
unsigned long startTime = 0;
bool isClockwise = true;

bool onPowerState(const String &deviceId, bool &state) {
  if (state) {
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);
    Serial.println("Motor started (clockwise)");
    startTime = millis();
    isClockwise = true;
  } else {
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, HIGH);
    Serial.println("Motor started (counter-clockwise)");
    startTime = millis();
    isClockwise = false;
  }
  motorRunning = true;
  return true;
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(touchPin, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.print(WiFi.localIP());

  SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
  mySwitch.onPowerState(onPowerState);
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void loop() {
  SinricPro.handle();

  int touchState = digitalRead(touchPin);

  if (!motorRunning && touchState == HIGH) {
    delay(50);  // Debounce delay
    if (!motorRunning && touchState == HIGH) {
      // Toggle motor direction
      isClockwise = !isClockwise;

      if (isClockwise) {
        digitalWrite(motorPin1, HIGH);
        digitalWrite(motorPin2, LOW);
        Serial.println("Motor started (clockwise)");
      } else {
        digitalWrite(motorPin1, LOW);
        digitalWrite(motorPin2, HIGH);
        Serial.println("Motor started (counter-clockwise)");
      }

      startTime = millis(); // Record the motor start time
      motorRunning = true; // Set the motor running flag
    }
  } else if (motorRunning) {
    // Stop the motor if the running duration has elapsed
    if (millis() - startTime >= 15000) {
      digitalWrite(motorPin1, LOW);
      digitalWrite(motorPin2, LOW);
      motorRunning = false; // Reset the motor running flag
      Serial.println("Motor stopped");
    }
  }
}