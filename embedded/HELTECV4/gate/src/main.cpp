#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "sensitive_data.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10

#define VEXT_PIN 36
#define LED_PIN 35 

SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

String message;

void blinkLed(int times)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(100);

        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

void setup() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.begin(115200);
  delay(3000);

  Serial.print("Connecting with WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Connected");

  config.api_key = API_KEY;
  config.token_status_callback = tokenStatusCallback; // Będzie wypisywać w konsoli, co się dzieje z tokenem

  Serial.println("Anonymous registration");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Authorization successful");
  } else {
    Serial.print("Authorization error: ");
    Serial.println(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);

  Serial.println("Starting radio on HELTECV4");

  int state = radio.begin(868.0, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8f);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Radio init error: ");
    Serial.println(state);
    while (true) {
    }
}

  state = radio.setDio2AsRfSwitch(true);

  if (state != RADIOLIB_ERR_NONE) {
      Serial.print("RF switch config error: ");
      Serial.println(state);
      while (true) {}
}

Serial.println("Radio initialized - waiting for messages");
blinkLed(3);
}

void loop() {
  int state = radio.receive(message);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Received message from node");
    Serial.print("Data: ");
    Serial.println(message);
    
    Serial.print("RSSI: ");
    Serial.print(radio.getRSSI());
    Serial.println(" dBm");

    blinkLed(1);

    if (Firebase.ready()) {
      Serial.println("Sending received data to Firestore");

      static int messageCounter = 0;
      messageCounter++;
      
      FirebaseJson content;
      content.set("fields/status/stringValue", message); 
      content.set("fields/rssi/doubleValue", radio.getRSSI());
      content.set("fields/update_count/integerValue", messageCounter); 
      
      String documentPath = "sensors/my_first_sensor";
      
      if (Firebase.Firestore.patchDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw(), "status,rssi,update_count")) {
        Serial.println("Success - cloud data updated");
      } else {
        Serial.print("Error saving in Firebase: ");
        Serial.println(fbdo.errorReason());
      }
    }
    
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
  } else {
    Serial.print("Receive error, code: ");
    Serial.println(state);
  }
}