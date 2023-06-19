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



//DHT sensor
#define DHTTYPE DHT22
uint8_t DHTPin = 12;
DHT dht(DHTPin, DHTTYPE);
float rTemperature; //room temperature
float humidity;
float tFahrenheit;

//body temperature sensor
float bodyTemp;
const int bodyTempPin = 14;
OneWire oneWire(bodyTempPin);

//HR sensor
const int pulsePin = 0;
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


unsigned long dataMillis = 0;


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
String epochTimeConverter(unsigned long epochTime);

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


    //DHT sensor
    pinMode(DHTPin, INPUT);
    dht.begin();

    //body temperature sensor
    sensors.begin();

    //HR sensor
    hrSampleInterval = millis();
    unsigned long timestamp = timeClient.getEpochTime();
    Serial.println(timestamp);
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

