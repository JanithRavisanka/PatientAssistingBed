#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h> 
#include "SinricPro.h"
#include "SinricProSwitch.h"
#include <HX711_ADC.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTTYPE DHT22   // DHT 22 (AM2302)





//DHT sensor
#define DHTTYPE DHT22
const int DHTPin = D3;
DHT dht(DHTPin, DHTTYPE);
float rTemperature; //room temperature
float humidity;
float tFahrenheit;

//body temperature sensor
float bodyTemp;
const int bodyTempPin = D0;
OneWire oneWire(bodyTempPin);

//HR sensor
const int pulsePin = A0;
int signal;
int threshold = 550;
unsigned long hrSampleInterval  =0; 
int count = 0;
int bpm = 0;
unsigned long bpmInterval = 0;

DallasTemperature sensors(&oneWire);

//Define credentials
#define WIFI_SSID "Dialog 4G 577"
#define WIFI_PASSWORD "4639000F"
#define API_KEY "AIzaSyAoxkXg7dG9hV7z8WoUc1duIbOa0d4NB-Y"
#define FIREBASE_PROJECT_ID "patient-assisting-bed-app"
#define USER_EMAIL "janithravi7@gmail.com"
#define USER_PASSWORD "12345678"
#define PATIENT_NAME "Janith"  //change user name

//for time module
const long utcOffsetInSeconds = 0; // utc +5.30
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


//sinric
#define APP_KEY           "c187674a-3a52-40d9-81d6-e9613391b6e4"
#define APP_SECRET        "3ed12387-5077-4a28-a239-2477effc6ec6-e9a6343e-db20-42b0-804d-2c4395dec443"  
#define SWITCH_ID         "648a0e1c929949c1da84cbcf"    
const int touchPin = D8;      // Pin connected to the touch sensor (D2 on NodeMCU)
const int motorPin1 = D6;    // Pin connected to IN1 of the motor driver (D6 on NodeMCU)
const int motorPin2 = D7;    // Pin connected to IN2 of the motor driver (D7 on NodeMCU)
bool motorRunning = false;
unsigned long startTime = 0;
bool isClockwise = true;


// Pins fan + Load cell
//DHT pin declared above 
const int HX711_dout = D4;  // MCU > HX711 dout pin
const int HX711_sck = D5;  // MCU > HX711 sck pin
const int RELAYPIN = D3;    // MCU > LED pin

// DHT_Unified dht(DHTPin, DHTTYPE);
// HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
unsigned long t = 0;
bool newDataReady = false;


unsigned long dataMillis = 0;

//sinric power state callback function
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


// The Firestore payload upload callback function
void fcsUploadCallback(CFS_UploadStatusInfo info)
{
    if (info.status == fb_esp_cfs_upload_status_init)
    {
        Serial.printf("\nUploading data (%d)...\n", info.size);
    }
    else if (info.status == fb_esp_cfs_upload_status_upload)
    {
        Serial.printf("Uploaded %d%s\n", (int)info.progress, "%");
    }
    else if (info.status == fb_esp_cfs_upload_status_complete)
    {
        Serial.println("Upload completed ");
    }
    else if (info.status == fb_esp_cfs_upload_status_process_response)
    {
        Serial.print("Processing the response... ");
    }
    else if (info.status == fb_esp_cfs_upload_status_error)
    {
        Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    }
}

//epoch time coverter function
String epochTimeConverter(unsigned long epochTime){
    time_t rawtime = epochTime; //
    struct tm *ptm = gmtime(&rawtime); //convert to UTC
    char utcTime[30];
    sprintf(utcTime, "%d-%02d-%02dT%02d:%02d:%02dZ", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    Serial.println(utcTime);
    return String(utcTime);
}

int isWeightDetected() {
  const int serialPrintInterval = 0;

  if (LoadCell.update())
    newDataReady = true;

  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      if (i > 400.0) {
        return 1;
      }
      newDataReady = false;
      t = millis();
    }
  }
  return 0;
}

//to find over humidity or not
int isOverHumidityFound() {
  delay(10);

  humidity = dht.readHumidity();
  if (isnan(humidity)) {
    Serial.println("Error reading humidity!");
    return 0;
  } else {
    if (humidity >= 95.0) {
      return 1;
    } else {
      return 0;
    }
  }
}

int isOverTemperatureFound() {
  delay(10);
  rTemperature = dht.readTemperature();
  if (isnan(rTemperature)) {
    Serial.println(F("Error reading temperature!"));
    return 0;
  } else {
    if (rTemperature >= 25.0) {
      return 1;
    } else {
      return 0;
    }
  }
}



void setup(){
    Serial.begin(115200);

    //connect with wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    timeClient.begin();
    
    //firebase client version
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    config.token_status_callback = tokenStatusCallback;

    //Define buffer size rx/tx
    fbdo.setBSSLBufferSize(2048, 2048);
    fbdo.setResponseSize(2048);

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    config.cfs.upload_callback = fcsUploadCallback;


    //DHT sensor +  relay + load cell
    pinMode(DHTPin, INPUT);
    pinMode(RELAYPIN, OUTPUT);
    dht.begin();

    LoadCell.begin();
    float calibrationValue;
    calibrationValue = -101.4;

    unsigned long stabilizingtime = 2000;
    boolean _tare = true;
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
        Serial.println("Timeout, check MCU > HX711 wiring and pin designations");
        while (1);
    } else {
        LoadCell.setCalFactor(calibrationValue);
        Serial.println("Startup is complete");
    }




    //body temperature sensor
    sensors.begin();

    //HR sensor
    hrSampleInterval = millis();
    unsigned long timestamp = timeClient.getEpochTime();
    Serial.println(timestamp);

    //sinric
    pinMode(motorPin1, OUTPUT);
    pinMode(motorPin2, OUTPUT);
    pinMode(touchPin, INPUT);
    SinricProSwitch& mySwitch = SinricPro[SWITCH_ID];
    mySwitch.onPowerState(onPowerState);
    SinricPro.begin(APP_KEY, APP_SECRET);
}

void loop(){
    if(Firebase.ready()){
    //HR sensorF
    signal = analogRead(pulsePin);
    if (signal > threshold && (millis() - hrSampleInterval > 400)){
        count++;
        hrSampleInterval = millis();
    }
    if(millis() - bpmInterval >= 60000){
        Serial.print("Count: ");
        Serial.println(count);
        if (count == 0){ //not a valid reading (can be a noise or a sudden movement)
             count = bpm;//therefore keep the previous bpm value
        }else if (bpm - count > 60 || count - bpm > 60){
          count = (bpm + count)/2; //if the difference is more than 60, it is a noise, therefore take the average of the two values
        }

        bpm = count; //update the bpm value
        Serial.print("BPM: ");
        Serial.println(count);
        count = 0;
        bpmInterval = millis(); //reset the interval
    }
    //after authentication in every loop runs after 6000ms (6s) but if the bpm is 0 it will not run (initially bpm is 0)
    //not sending data in initial stage
    if(Firebase.ready() && (millis() - dataMillis > 60100) && bpm != 0){
        dataMillis = millis(); //reset the interval
        //update time
        timeClient.update();

        //DHT sensor
        humidity = dht.readHumidity();
        rTemperature = dht.readTemperature();
        if (isnan(humidity) || isnan(rTemperature)) { //check if the reading is valid
            Serial.println(F("Failed to read from DHT sensor!"));
        }

        //body temperature sensor
        sensors.requestTemperatures();
        bodyTemp = sensors.getTempCByIndex(0);

        FirebaseJson content;
        String documentPath = PATIENT_NAME + String("/") + String(timeClient.getFormattedTime()); //document path is patient name and time

        //set data to json object
        content.set("fields/timestamp/timestampValue", epochTimeConverter(timeClient.getEpochTime()));
        content.set("fields/bpm/integerValue", bpm);
        content.set("fields/body temp/doubleValue", bodyTemp);
        content.set("fields/room temp/doubleValue", rTemperature);

        //send data to firestore
        Serial.print("Create a document...");
        if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw()))
            Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        else
            Serial.println(fbdo.errorReason());  
              
    }
  }

    //sinric
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

    //relay + dht to on the fan 
    int isWeight = isWeightDetected();
    int isHumidity = isOverHumidityFound();
    int isTemperature = isOverTemperatureFound();
    
    if ((isWeight == 1 && isHumidity == 1) || (isWeight == 1 && isTemperature == 1)) {
        digitalWrite(RELAYPIN, HIGH);  // Turn on the relay
    } else {
        digitalWrite(RELAYPIN, LOW);   // Turn off the relay
    }
}



