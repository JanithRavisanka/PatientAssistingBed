#include <Arduino.h>
#include <ESP8266WiFi.h>

// firebase client library
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Define the WiFi credentials 
#define WIFI_SSID "Dialog 4G 577"
#define WIFI_PASSWORD "4639000F"

// project details
#define API_KEY "AIzaSyAoxkXg7dG9hV7z8WoUc1duIbOa0d4NB-Y"
#define FIREBASE_PROJECT_ID "patient-assisting-bed-app"

//user details
#define USER_EMAIL "janithravi7@gmail.com"
#define USER_PASSWORD "12345678"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int count = 0;


void setup(){
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wifi");
    while (WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    //Assign the API key
    config.api_key = API_KEY;

    //User credentials
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    config.token_status_callback = tokenStatusCallback;

    //Rx and Tx buffer sizes
    fbdo.setBSSLBufferSize(2048, 2048);

    fbdo.setResponseSize(2048);

    Firebase.begin(&config, &auth);

    Firebase.reconnectWiFi(true);
}
void loop(){
    if(Firebase.ready()){
        FirebaseJson content;
        String documentPath = String(count);
        content.set("name:S", )
    }
}