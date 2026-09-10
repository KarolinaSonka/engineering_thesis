#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "sensitive_data.h"
#include "lora_frame.h"

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

  Serial.println("Synchronizing system time with NTP server");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  config.api_key = API_KEY;
  config.token_status_callback = tokenStatusCallback; 

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
  uint8_t byteArr[sizeof(LoRaNodeData)];
  int state = radio.receive(byteArr, sizeof(LoRaNodeData));

  if (state == RADIOLIB_ERR_NONE) {
    if (radio.getPacketLength() == sizeof(LoRaNodeData)) {
      
      LoRaNodeData receivedData;
      memcpy(&receivedData, byteArr, sizeof(LoRaNodeData));
      
      Serial.println("Correct frame captured");
      Serial.printf("Node ID: %u\n", receivedData.node_id);
      Serial.printf("Type: %d, Licznik: %u, Stan: %d\n", receivedData.node_type, receivedData.msg_counter, receivedData.sensor_state);
      Serial.printf("Aceelerometer: X: %d, Y: %d, Z: %d\n", receivedData.acc_x, receivedData.acc_y, receivedData.acc_z);
      Serial.printf("Battery: %d %%, RSSI: %.1f dBm\n", receivedData.battery_lvl, radio.getRSSI());

      blinkLed(1);

      if (Firebase.ready()) {
        Serial.println("Uploading telemetry to Firestore...");
        
        FirebaseJson content;
        content.set("fields/node_type/integerValue", receivedData.node_type);
        content.set("fields/msg_counter/integerValue", receivedData.msg_counter);
        content.set("fields/acc_x/integerValue", receivedData.acc_x);
        content.set("fields/acc_y/integerValue", receivedData.acc_y);
        content.set("fields/acc_z/integerValue", receivedData.acc_z);
        content.set("fields/sensor_state/integerValue", receivedData.sensor_state);
        content.set("fields/battery_lvl/integerValue", receivedData.battery_lvl);
        content.set("fields/rssi/doubleValue", radio.getRSSI());

        // format ISO 8601 / RFC 3339 UTC timestamp required by Firestore
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          char timeStr[30];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
          content.set("fields/timestamp/timestampValue", timeStr);
        } else {
          Serial.println("Warning: NTP time not yet synchronized");
        }
        
        String documentPath = "sensors/";
        documentPath += receivedData.node_id;
        String updateMask = "node_type,msg_counter,acc_x,acc_y,acc_z,sensor_state,battery_lvl,rssi,timestamp";
        
        if (Firebase.Firestore.patchDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw(), updateMask.c_str())) {
          Serial.println("Live document updated successfully");
        } else {
          Serial.print("patchDocument failed: ");
          Serial.println(fbdo.errorReason());
        }

        String historyPath = documentPath;
        historyPath += "/readings";
        
        if (Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, "", historyPath.c_str(), content.raw())) {
          Serial.println("Historical record appended successfully");
        } else {
          Serial.print("createDocument failed: ");
          Serial.println(fbdo.errorReason());
        }
      }
    } else {
      Serial.println("Unknown frame received - rejecting");
    }
    
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
  } else {
    Serial.print("Error druing receiving message: ");
    Serial.println(state);
  }
}