#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <map>
#include <time.h>
#include <string.h>

#include "sensitive_data.h"
#include "lora_frame.h"
#include "shared_state.h"
#include "wifi_config.h"
#include "firebase_service.h"

#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10
#define LORA_FEM_POWER 7
#define LORA_FEM_ENABLE 2
#define LORA_FEM_MODE 5

#define VEXT_PIN 36
#define LED_PIN 35
#define CONFIG_BUTTON_PIN 0

bool firebaseInitialized = false;

SX1262 radio = new Module(
    LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

TaskHandle_t loraTaskHandle = nullptr;

WebServer configServer(80);
Preferences wifiPreferences;

const char *AP_SSID = "Loriota-Setup";
const char *AP_PASSWORD = "loriota_config";

bool configPortalActive = false;
bool wifiConnected = false;
bool wifiConnectionFailed = false;
bool closePortalAfterConnection = false;
bool wifiReconnectInProgress = false;

unsigned long wifiConnectStarted = 0;
unsigned long wifiConnectedAt = 0;
unsigned long lastWifiRetry = 0;
unsigned long wifiReconnectStarted = 0;
unsigned long buttonPressedAt = 0;

bool buttonHandled = false;

extern const unsigned long WIFI_CONNECT_TIMEOUT = 20000;
extern const unsigned long PORTAL_CLOSE_DELAY = 45000;

QueueHandle_t loraQueue = nullptr;
SemaphoreHandle_t serialMutex = nullptr;

unsigned long lastAlarmTime = 0;
std::map<uint16_t, int> previousSensorStates;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void)
{
  if (loraTaskHandle == nullptr)
  {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;

  vTaskNotifyGiveFromISR(
      loraTaskHandle,
      &higherPriorityTaskWoken);

  if (higherPriorityTaskWoken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }
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

void processLoRa()
{
  uint8_t byteArr[sizeof(LoRaNodeData)] = {};

  const size_t packetLength = radio.getPacketLength();
  const int state = radio.readData(
      byteArr,
      sizeof(byteArr));

  if (state != RADIOLIB_ERR_NONE)
  {
    if (Serial &&
        xSemaphoreTake(serialMutex, 0) == pdTRUE)
    {
      Serial.printf(
          "[LoRa] Packet read failed: %d\n",
          state);

      xSemaphoreGive(serialMutex);
    }

    return;
  }

  if (packetLength != sizeof(LoRaNodeData))
  {
    return;
  }

  QueueMessage msg = {};

  memcpy(
      &msg.data,
      byteArr,
      sizeof(LoRaNodeData));

  msg.rssi = radio.getRSSI(true);

  const float snr = radio.getSNR();

  BaseType_t queueStatus =
      xQueueSend(loraQueue, &msg, 0);

  if (Serial &&
      xSemaphoreTake(serialMutex, 0) == pdTRUE)
  {
    if (queueStatus == pdPASS)
    {
      Serial.printf(
          "[LoRa] Node: %u | Counter: %u | "
          "State: %d | RSSI: %.1f dBm | "
          "SNR: %.1f dB\n",
          msg.data.node_id,
          msg.data.msg_counter,
          msg.data.sensor_state,
          (double)msg.rssi,
          (double)snr);
    }
    else
    {
      Serial.println(
          "[LoRa] Queue full - frame dropped.");
    }

    xSemaphoreGive(serialMutex);
  }
}

void loraTask(void *pvParameters)
{
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    processLoRa();
  }
}

void initializeLoRaFEM()
{
  pinMode(LORA_FEM_MODE, OUTPUT);
  digitalWrite(LORA_FEM_MODE, HIGH);

  pinMode(LORA_FEM_POWER, OUTPUT);
  digitalWrite(LORA_FEM_POWER, HIGH);

  delay(10);

  pinMode(LORA_FEM_ENABLE, OUTPUT);
  digitalWrite(LORA_FEM_ENABLE, HIGH);

  delay(10);

  Serial.println("[LoRa] KCT8103L FEM initialized in RX Bypass mode.");
}

static void initializeSerial()
{
  Serial.begin(115200);
  Serial.setTxTimeoutMs(5);
  delay(3000);

  serialMutex = xSemaphoreCreateMutex();

  if (serialMutex == nullptr)
  {
    Serial.println("[ERROR] Could not create serial mutex.");
    while (true)
      delay(1000);
  }

  Serial.println();
  Serial.println("LORIOTA GATEWAY");
  Serial.printf("LoRa frame size: %u bytes\n",
                (unsigned int)sizeof(LoRaNodeData));
}

static void initializeWiFi()
{
  WiFi.persistent(false);

  if (digitalRead(CONFIG_BUTTON_PIN) == LOW)
  {
    Serial.println("[WiFi] Configuration requested at startup.");
    startConfigPortal();
  }
  else if (!connectToSavedWiFi())
  {
    startConfigPortal();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WiFi] Synchronizing system time.");

    configTime(
        0, 0,
        "pool.ntp.org",
        "time.nist.gov");

    initializeFirebase();
  }
  else
  {
    Serial.println("[Firebase] Initialization deferred until Wi-Fi connects.");
  }
}

static void initializeFirebaseTask()
{
  loraQueue = xQueueCreate(10, sizeof(QueueMessage));

  if (loraQueue == nullptr)
  {
    Serial.println("[ERROR] Could not create LoRa queue.");
    while (true)
      delay(1000);
  }

  BaseType_t taskCreated = xTaskCreatePinnedToCore(
      firebaseTask,
      "FirebaseTask",
      10000,
      nullptr,
      1,
      nullptr,
      1);

  if (taskCreated != pdPASS)
  {
    Serial.println("[ERROR] Could not create Firebase task.");
    while (true)
      delay(1000);
  }
}

static void initializeLoRa()
{
  initializeLoRaFEM();

  SPI.begin(
      LORA_SCK,
      LORA_MISO,
      LORA_MOSI,
      -1);

  int state = radio.begin(
      868.3,
      500.0,
      7,
      5,
      RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
      14,
      8,
      1.8f);

  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.print("[LoRa] Initialization failed: ");
    Serial.println(state);
  }
  else
  {
    radio.setDio2AsRfSwitch(true);

    BaseType_t radioTaskCreated = xTaskCreatePinnedToCore(
        loraTask,
        "LoRaTask",
        4096,
        nullptr,
        2,
        &loraTaskHandle,
        1);

    if (radioTaskCreated != pdPASS)
    {
      Serial.println("[ERROR] Could not create LoRa task.");

      while (true)
      {
        delay(1000);
      }
    }

    radio.setDio1Action(setFlag);

    state = radio.startReceive();

    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println("[LoRa] Initialized, waiting for messages.");
    }
    else
    {
      Serial.print("[LoRa] Receive start failed: ");
      Serial.println(state);
    }
  }
}

void setup()
{
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  initializeSerial();
  initializeWiFi();
  initializeFirebaseTask();
  initializeLoRa();

  blinkLed(3);
}

static void handleConfigurationButton()
{
  if (digitalRead(CONFIG_BUTTON_PIN) == LOW)
  {
    if (buttonPressedAt == 0)
    {
      buttonPressedAt = millis();
    }

    if (!buttonHandled &&
        millis() - buttonPressedAt >= 2000)
    {
      buttonHandled = true;

      Serial.println(
          "[WiFi] Manual configuration requested.");

      startConfigPortal();
    }
  }
  else
  {
    buttonPressedAt = 0;
    buttonHandled = false;
  }
}

static void handleConnectedWiFi()
{
  if (!wifiConnected)
  {
    wifiConnected = true;
    wifiReconnectInProgress = false;
    wifiReconnectStarted = 0;
    wifiConnectionFailed = false;
    wifiConnectedAt = millis();

    Serial.println("[WiFi] Connected successfully.");
    Serial.print("[WiFi] Local IP: ");
    Serial.println(WiFi.localIP());

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov");

    initializeFirebase();
  }

  if (configPortalActive &&
      closePortalAfterConnection &&
      millis() - wifiConnectedAt >= PORTAL_CLOSE_DELAY)
  {
    Serial.println(
        "[WiFi] Closing configuration portal.");

    configServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    configPortalActive = false;
    closePortalAfterConnection = false;
    wifiConnectStarted = 0;
  }
}

static void handleDisconnectedWiFi()
{
  if (wifiConnected)
  {
    wifiConnected = false;
    wifiReconnectInProgress = false;
    wifiReconnectStarted = 0;
    lastWifiRetry = millis();

    Serial.println("[WiFi] Connection lost.");
  }

  if (configPortalActive &&
      wifiConnectStarted != 0 &&
      millis() - wifiConnectStarted >= WIFI_CONNECT_TIMEOUT)
  {
    wifiConnectionFailed = true;
    wifiConnectStarted = 0;

    Serial.println("[WiFi] Connection timed out.");
    Serial.println(
        "[WiFi] Check the password and 2.4 GHz network.");

    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP_STA);
  }

  if (!configPortalActive &&
      wifiReconnectInProgress &&
      millis() - wifiReconnectStarted >= WIFI_CONNECT_TIMEOUT)
  {
    WiFi.disconnect(false, false);

    wifiReconnectInProgress = false;
    wifiReconnectStarted = 0;
    lastWifiRetry = millis();

    Serial.println(
        "[WiFi] Reconnection attempt timed out.");
  }

  if (!configPortalActive &&
      !wifiReconnectInProgress &&
      millis() - lastWifiRetry >= 10000)
  {
    lastWifiRetry = millis();

    Serial.println(
        "[WiFi] Trying to reconnect to a saved network.");

    if (reconnectToSavedWiFi())
    {
      wifiReconnectInProgress = true;
      wifiReconnectStarted = millis();
    }
  }
}

static void updateWiFiConnection()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    handleConnectedWiFi();
  }
  else
  {
    handleDisconnectedWiFi();
  }
}

void loop()
{
  handleConfigurationButton();

  if (configPortalActive)
  {
    configServer.handleClient();
  }

  updateWiFiConnection();

  delay(1);
}
