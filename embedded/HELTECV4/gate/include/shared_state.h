#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <Arduino.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WebServer.h>
#include <Preferences.h>
#include <map>
#include "lora_frame.h"

struct QueueMessage
{
    LoRaNodeData data;
    float rssi;
};

extern FirebaseData fbdo;
extern FirebaseAuth auth;
extern FirebaseConfig config;
extern bool firebaseInitialized;

extern SX1262 radio;
extern volatile bool receivedFlag;

extern WebServer configServer;
extern Preferences wifiPreferences;

extern bool configPortalActive;
extern bool wifiConnected;
extern bool wifiConnectionFailed;
extern bool closePortalAfterConnection;
extern unsigned long wifiConnectStarted;
extern unsigned long wifiConnectedAt;
extern unsigned long lastWifiRetry;
extern bool wifiReconnectInProgress;
extern unsigned long wifiReconnectStarted;

extern const unsigned long WIFI_CONNECT_TIMEOUT;
extern const unsigned long PORTAL_CLOSE_DELAY;

extern QueueHandle_t loraQueue;
extern SemaphoreHandle_t serialMutex;
extern unsigned long lastAlarmTime;
extern std::map<uint16_t, int> previousSensorStates;

extern const char *AP_SSID;
extern const char *AP_PASSWORD;

extern void blinkLed(int times);

#endif