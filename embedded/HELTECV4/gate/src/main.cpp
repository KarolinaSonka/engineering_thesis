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

struct QueueMessage {
  LoRaNodeData data;
  float rssi;
};

QueueHandle_t loraQueue;
SemaphoreHandle_t serialMutex;

volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

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

void firebaseTask(void *pvParameters) {
  QueueMessage msg;
  
  for(;;) {
    if (xQueueReceive(loraQueue, &msg, portMAX_DELAY) == pdPASS) {
      
      blinkLed(1); 
      
      if (Firebase.ready()) {
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        Serial.println("\n[Firebase Task] Uploading data to Firestore");
        xSemaphoreGive(serialMutex);
        
        FirebaseJson content;
        content.set("fields/node_type/integerValue", msg.data.node_type);
        content.set("fields/msg_counter/integerValue", msg.data.msg_counter);
        content.set("fields/acc_x/integerValue", msg.data.acc_x);
        content.set("fields/acc_y/integerValue", msg.data.acc_y);
        content.set("fields/acc_z/integerValue", msg.data.acc_z);
        content.set("fields/sensor_state/integerValue", msg.data.sensor_state);
        content.set("fields/battery_lvl/integerValue", msg.data.battery_lvl);
        content.set("fields/rssi/doubleValue", msg.rssi);

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          char timeStr[30];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
          content.set("fields/timestamp/timestampValue", timeStr);
        }
        
        String documentPath = "sensors/";
        documentPath += msg.data.node_id;
        String updateMask = "node_type,msg_counter,acc_x,acc_y,acc_z,sensor_state,battery_lvl,rssi,timestamp";
        
        bool patchStatus = Firebase.Firestore.patchDocument(&fbdo, PROJECT_ID, "", documentPath.c_str(), content.raw(), updateMask.c_str());
        
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        if (patchStatus) {
          Serial.println("[Firebase Task] Live data updated successfully");
        } else {
          Serial.print("[Firebase Task] patchDocument failed: ");
          Serial.println(fbdo.errorReason());
        }
        xSemaphoreGive(serialMutex);

        String historyPath = documentPath;
        historyPath += "/readings";
        
        bool createStatus = Firebase.Firestore.createDocument(&fbdo, PROJECT_ID, "", historyPath.c_str(), content.raw());
        
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        if (createStatus) {
          Serial.println("[Firebase Task] Historical record appended successfully");
        } else {
          Serial.print("[Firebase Task] createDocument failed: ");
          Serial.println(fbdo.errorReason());
        }
        xSemaphoreGive(serialMutex);
      }
    }
  }
}

void setup() {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  Serial.begin(115200);
  delay(3000);

  serialMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.print("Connecting with WiFi");
  xSemaphoreGive(serialMutex);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print(".");
    xSemaphoreGive(serialMutex);
    delay(500);
  }
  
  xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.println("\nConnected");
  Serial.println("Synchronizing system time with NTP server");
  xSemaphoreGive(serialMutex);
  
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  config.api_key = API_KEY;
  config.token_status_callback = tokenStatusCallback; 

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  loraQueue = xQueueCreate(10, sizeof(QueueMessage));

  xTaskCreatePinnedToCore(
    firebaseTask, "FirebaseTask", 10000, NULL, 1, NULL, 0
  );

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
  int state = radio.begin(868.0, 125.0, 7, 5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8f);
  radio.setDio2AsRfSwitch(true);
  radio.setDio1Action(setFlag);
  
  state = radio.startReceive();
  
  xSemaphoreTake(serialMutex, portMAX_DELAY);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio initialized, waiting for messages");
  } else {
    Serial.print("Error starting receive: ");
    Serial.println(state);
  }
  xSemaphoreGive(serialMutex);
  
  blinkLed(3);
}

void loop() {
  if (receivedFlag) {
    receivedFlag = false; 
    
    uint8_t byteArr[sizeof(LoRaNodeData)];
    
    int state = radio.readData(byteArr, sizeof(LoRaNodeData));

    if (state == RADIOLIB_ERR_NONE) {
      if (radio.getPacketLength() == sizeof(LoRaNodeData)) {
        
        QueueMessage msg;
        memcpy(&msg.data, byteArr, sizeof(LoRaNodeData));
        msg.rssi = radio.getRSSI();
        
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        Serial.println("\n[Radio Task] Correct frame captured");
        Serial.printf("Node ID: %u, Licznik: %u, Stan: %d\n", msg.data.node_id, msg.data.msg_counter, msg.data.sensor_state);
        xSemaphoreGive(serialMutex);
        
        if (xQueueSend(loraQueue, &msg, (TickType_t)10) != pdPASS) {
          xSemaphoreTake(serialMutex, portMAX_DELAY);
          Serial.println("[Radio Task] ERROR: Queue is full - dropping frame");
          xSemaphoreGive(serialMutex);
        }
      }
    }
    radio.startReceive();
  }
  
  delay(1); 
}