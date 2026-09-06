#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

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
    
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
  } else {
    Serial.print("Receive error, code: ");
    Serial.println(state);
  }
}