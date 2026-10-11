#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include "sensitive_data.h"
#include "lora_frame.h"
#include <map>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseInitialized = false;

#define LORA_CS 8
#define LORA_DIO1 14
#define LORA_RST 12
#define LORA_BUSY 13
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10

#define VEXT_PIN 36
#define LED_PIN 35
#define CONFIG_BUTTON_PIN 0

unsigned long buttonPressedAt = 0;
bool buttonHandled = false;

SX1262 radio = new Module(
    LORA_CS,
    LORA_DIO1,
    LORA_RST,
    LORA_BUSY);

volatile bool receivedFlag = false;


WebServer configServer(80);
Preferences wifiPreferences;

static const char *AP_SSID = "Loriota-Setup";
static const char *AP_PASSWORD = "loriota_config";

bool configPortalActive = false;
bool wifiConnected = false;
bool wifiConnectionFailed = false;
bool closePortalAfterConnection = false;

unsigned long wifiConnectStarted = 0;
unsigned long wifiConnectedAt = 0;
unsigned long lastWifiRetry = 0;

const unsigned long WIFI_CONNECT_TIMEOUT = 20000;
const unsigned long PORTAL_CLOSE_DELAY = 45000;

struct QueueMessage
{
  LoRaNodeData data;
  float rssi;
};

QueueHandle_t loraQueue = nullptr;
SemaphoreHandle_t serialMutex = nullptr;

unsigned long lastAlarmTime = 0;
std::map<uint16_t, int> previousSensorStates;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setFlag(void)
{
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

String jsonEscape(const String &input)
{
  String output;
  output.reserve(input.length() + 8);

  for (unsigned int i = 0; i < input.length(); i++)
  {
    char c = input[i];

    if (c == '"' || c == '\\')
    {
      output += '\\';
      output += c;
    }
    else if (c == '\n')
    {
      output += "\\n";
    }
    else if (c == '\r')
    {
      output += "\\r";
    }
    else if ((uint8_t)c >= 0x20)
    {
      output += c;
    }
  }

  return output;
}

String makeConfigPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#1A2E31">
<title>Loriota | Konfiguracja Wi-Fi</title>
<style>
* { box-sizing: border-box; }

body {
    margin: 0;
    min-height: 100vh;
    padding: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #1A2E31;
    color: #F3F4F2;
    font-family: Arial, sans-serif;
}

.card {
    width: 100%;
    max-width: 420px;
    padding: 32px 24px;
    background: #243638;
    border: 1px solid #52605F;
    border-radius: 12px;
    text-align: center;
}

.logo {
    width: 54px;
    height: 54px;
    margin: 0 auto 16px;
    display: flex;
    align-items: center;
    justify-content: center;
    border: 2px solid #A85E3D;
    border-radius: 12px;
    color: #64C9CF;
    font-size: 30px;
    font-weight: bold;
}

h1 {
    margin: 0;
    font-size: 28px;
    letter-spacing: 0.5px;
}

.subtitle {
    margin: 8px 0 28px;
    color: #B4BDBB;
    font-size: 12px;
    letter-spacing: 1.5px;
    text-transform: uppercase;
}

.intro {
    margin-bottom: 24px;
    color: #B4BDBB;
    font-size: 14px;
    line-height: 1.7;
}

label {
    display: block;
    margin: 18px 0 9px;
    text-align: left;
    color: #F3F4F2;
    font-size: 13px;
    font-weight: bold;
}

select, input {
    display: block;
    width: 100%;
    min-height: 46px;
    padding: 12px;
    border: 1px solid #52605F;
    border-radius: 6px;
    background: #1A2E31;
    color: #F3F4F2;
    font-size: 14px;
    outline: none;
}

select:focus, input:focus {
    border-color: #64C9CF;
}

select { cursor: pointer; }

.hint {
    margin: 8px 0 0;
    text-align: left;
    color: #B4BDBB;
    font-size: 12px;
    line-height: 1.5;
}

button {
    display: block;
    width: 100%;
    min-height: 46px;
    margin-top: 22px;
    padding: 12px 16px;
    border: none;
    border-radius: 6px;
    background: #A85E3D;
    color: #F3F4F2;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
}

button:hover { opacity: 0.88; }

button.secondary {
    margin-top: 10px;
    border: 1px solid #52605F;
    background: transparent;
    color: #64C9CF;
}

.footer {
    margin-top: 28px;
    padding-top: 18px;
    border-top: 1px solid #52605F;
    color: #A85E3D;
    font-size: 11px;
    font-weight: bold;
    letter-spacing: 1.5px;
}
</style>
</head>
<body>
<main class="card">
    <div class="logo">L</div>
    <h1>loriota</h1>
    <div class="subtitle">Konfiguracja bramy</div>

    <p class="intro">
        Połącz bramę Loriota z lokalną siecią Wi-Fi.
        Wybierz sieć i wpisz jej hasło.
    </p>

    <form action="/save" method="POST">
        <label for="ssid">Dostępne sieci Wi-Fi</label>
        <select name="ssid" id="ssid" required>
)rawliteral";

  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0)
  {
    html += "<option value=\"\">Nie znaleziono sieci</option>";
  }
  else
  {
    for (int i = 0; i < networkCount; i++)
    {
      String ssid = WiFi.SSID(i);

      if (ssid.length() == 0)
      {
        continue;
      }

      String safeSsid = ssid;
      safeSsid.replace("&", "&amp;");
      safeSsid.replace("\"", "&quot;");
      safeSsid.replace("<", "&lt;");
      safeSsid.replace(">", "&gt;");

      html += "<option value=\"";
      html += safeSsid;
      html += "\">";
      html += safeSsid;
      html += " (";
      html += String(WiFi.RSSI(i));
      html += " dBm)";

      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
      {
        html += " - otwarta";
      }

      html += "</option>";
    }
  }

  html += R"rawliteral(
        </select>

        <p class="hint">
            Wybierz sieć Wi-Fi działającą w paśmie 2,4 GHz.
        </p>

        <label for="password">Hasło do Wi-Fi</label>
        <input
            type="password"
            id="password"
            name="password"
            placeholder="Wpisz hasło do sieci"
            autocomplete="current-password"
        >

        <button type="submit">Połącz z siecią</button>
    </form>

    <form action="/" method="GET">
        <button class="secondary" type="submit">
            Odśwież listę sieci
        </button>
    </form>

    <div class="footer">
        LORIOTA &bull; INTELIGENTNY MONITORING
    </div>
</main>
</body>
</html>
)rawliteral";

  WiFi.scanDelete();
  return html;
}

String makeWaitingPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#1A2E31">
<title>Loriota | Status połączenia</title>
<style>
* { box-sizing: border-box; }

body {
    margin: 0;
    min-height: 100vh;
    padding: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #1A2E31;
    color: #F3F4F2;
    font-family: Arial, sans-serif;
}

.card {
    width: 100%;
    max-width: 440px;
    padding: 30px 24px;
    background: #243638;
    border: 1px solid #52605F;
    border-radius: 12px;
}

.brand { text-align: center; }

.logo {
    width: 54px;
    height: 54px;
    margin: 0 auto 16px;
    display: flex;
    align-items: center;
    justify-content: center;
    border: 2px solid #A85E3D;
    border-radius: 12px;
    color: #64C9CF;
    font-size: 30px;
    font-weight: bold;
}

h1 { margin: 0; font-size: 28px; }

.subtitle {
    margin: 8px 0 28px;
    color: #B4BDBB;
    font-size: 12px;
    letter-spacing: 1.5px;
}

h2 { margin: 0 0 10px; font-size: 21px; }

.description {
    color: #B4BDBB;
    font-size: 14px;
    line-height: 1.6;
    margin-bottom: 22px;
}

.status {
    display: flex;
    align-items: flex-start;
    gap: 12px;
    padding: 15px 0;
    border-top: 1px solid #52605F;
}

.dot {
    width: 11px;
    height: 11px;
    margin-top: 4px;
    flex-shrink: 0;
    border-radius: 50%;
    background: #52605F;
}

.pending .dot {
    background: #64C9CF;
    animation: pulse 1.2s infinite;
}

.success .dot { background: #79D99A; }
.failure .dot { background: #F0A080; }

.status-title {
    font-size: 14px;
    font-weight: bold;
}

.detail {
    margin-top: 5px;
    color: #B4BDBB;
    font-size: 12px;
    line-height: 1.6;
    overflow-wrap: anywhere;
}

.footer {
    margin-top: 25px;
    padding-top: 18px;
    border-top: 1px solid #52605F;
    color: #A85E3D;
    font-size: 11px;
    text-align: center;
    font-weight: bold;
    letter-spacing: 1.5px;
}

a { color: #64C9CF; }
.hidden { display: none; }

@keyframes pulse {
    50% { opacity: 0.3; }
}

@media (prefers-reduced-motion: reduce) {
    .pending .dot { animation: none; }
}
</style>
</head>
<body>
<main class="card">
    <div class="brand">
        <div class="logo">L</div>
        <h1>loriota</h1>
        <div class="subtitle">Konfiguracja bramy</div>
    </div>

    <h2 id="heading">Łączenie z siecią Wi-Fi…</h2>
    <div class="description" id="description">
        Zapisano dane sieci. Pozostaw bramę włączoną,
        aż zakończy się proces łączenia.
    </div>

    <div class="status pending" id="wifiStatus">
        <div class="dot"></div>
        <div>
            <div class="status-title">Połączenie Wi-Fi</div>
            <div class="detail" id="wifiDetail">
                Oczekiwanie na odpowiedź bramy…
            </div>
        </div>
    </div>

    <div class="status pending" id="firebaseStatus">
        <div class="dot"></div>
        <div>
            <div class="status-title">Usługi Firebase</div>
            <div class="detail" id="firebaseDetail">
                Oczekiwanie na połączenie Wi-Fi.
            </div>
        </div>
    </div>

    <div class="status">
        <div class="dot"></div>
        <div>
            <div class="status-title">Co dalej?</div>
            <div class="detail" id="nextDetail">
                Status aktualizuje się automatycznie.
            </div>
        </div>
    </div>

    <div class="detail hidden" id="retryLink">
        <a href="/">Wróć do konfiguracji Wi-Fi</a>
    </div>

    <div class="footer">LORIOTA &bull; INTELIGENTNY MONITORING</div>
</main>

<script>
let polling = false;

const wifiStatus = document.getElementById("wifiStatus");
const firebaseStatus = document.getElementById("firebaseStatus");
const wifiDetail = document.getElementById("wifiDetail");
const firebaseDetail = document.getElementById("firebaseDetail");
const heading = document.getElementById("heading");
const description = document.getElementById("description");
const nextDetail = document.getElementById("nextDetail");
const retryLink = document.getElementById("retryLink");

function setStatus(element, state) {
    element.classList.remove("pending", "success", "failure");
    element.classList.add(state);
}

async function checkStatus() {
    if (polling) return;
    polling = true;

    try {
        const response = await fetch("/status?t=" + Date.now(), {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("Status request failed");
        }

        const data = await response.json();

        if (data.wifi === "connected") {
            setStatus(wifiStatus, "success");

            wifiDetail.textContent =
                "Połączono z siecią „" + data.ssid +
                "”. Adres IP: " + data.ip;

            if (data.firebase === "ready") {
                setStatus(firebaseStatus, "success");

                firebaseDetail.textContent =
                    "Firebase jest gotowy do obsługi danych.";

                heading.textContent = "Loriota jest gotowa!";
                description.textContent =
                    "Brama jest połączona z Wi-Fi i Firebase.";

                nextDetail.textContent =
                    "Możesz teraz otworzyć aplikację Loriota.";

            } else {
                setStatus(firebaseStatus, "pending");

                firebaseDetail.textContent =
                    "Wi-Fi działa. Oczekiwanie na uruchomienie Firebase…";

                heading.textContent = "Połączono z Wi-Fi!";
                description.textContent =
                    "Brama dołączyła do wybranej sieci.";

                nextDetail.textContent =
                    "Uruchomienie Firebase może potrwać jeszcze chwilę.";
            }

        } else if (data.wifi === "failed") {
            setStatus(wifiStatus, "failure");

            wifiDetail.textContent =
                "Nie udało się połączyć. Sprawdź hasło i wybierz sieć 2,4 GHz.";

            setStatus(firebaseStatus, "failure");

            firebaseDetail.textContent =
                "Firebase nie może się połączyć bez dostępu do Wi-Fi.";

            heading.textContent = "Nie udało się połączyć";
            description.textContent =
                "Brama nie mogła połączyć się z wybraną siecią.";

            nextDetail.textContent =
                "Wróć do konfiguracji i spróbuj ponownie.";

            retryLink.classList.remove("hidden");

        } else {
            setStatus(wifiStatus, "pending");
            wifiDetail.textContent =
                "Trwa próba połączenia z siecią Wi-Fi…";

            setStatus(firebaseStatus, "pending");
            firebaseDetail.textContent =
                "Oczekiwanie na połączenie Wi-Fi.";
        }

    } catch (error) {
        wifiDetail.textContent =
            "Oczekiwanie na odpowiedź bramy. Pozostaw tę stronę otwartą.";
    } finally {
        polling = false;
    }
}

checkStatus();
setInterval(checkStatus, 1000);
</script>
</body>
</html>
)rawliteral";

  return html;
}

void handleConfigRoot()
{
  configServer.send(
      200,
      "text/html; charset=utf-8",
      makeConfigPage());
}

void handleConfigStatus()
{
  bool connected = WiFi.status() == WL_CONNECTED;

  bool firebaseReady =
      connected &&
      firebaseInitialized &&
      Firebase.ready();

  String ssid = connected ? WiFi.SSID() : "";
  String ip = connected ? WiFi.localIP().toString() : "";

  String wifiState;

  if (connected)
  {
    wifiState = "connected";
  }
  else if (wifiConnectionFailed)
  {
    wifiState = "failed";
  }
  else
  {
    wifiState = "connecting";
  }

  String firebaseState;

  if (!connected)
  {
    firebaseState = "waiting";
  }
  else if (firebaseReady)
  {
    firebaseState = "ready";
  }
  else
  {
    firebaseState = "connecting";
  }

  String json = "{";

  json += "\"wifi\":\"";
  json += wifiState;

  json += "\",\"firebase\":\"";
  json += firebaseState;

  json += "\",\"ssid\":\"";
  json += jsonEscape(ssid);

  json += "\",\"ip\":\"";
  json += ip;

  json += "\"}";

  configServer.sendHeader("Cache-Control", "no-store");
  configServer.send(
      200,
      "application/json; charset=utf-8",

      json);
}

void handleConfigSave()
{
  if (!configServer.hasArg("ssid") ||
      !configServer.hasArg("password"))
  {

    configServer.send(
        400,
        "text/plain; charset=utf-8",
        "Brakuje wymaganych danych.");
    return;
  }

  String ssid = configServer.arg("ssid");
  String password = configServer.arg("password");

  ssid.trim();

  if (ssid.length() == 0 ||
      ssid.length() > 32 ||
      password.length() > 63 ||
      (password.length() > 0 && password.length() < 8))
  {

    configServer.send(
        400,
        "text/html; charset=utf-8",
        "<!DOCTYPE html><html lang='pl'><head><meta charset='UTF-8'>"
        "</head><body style='"
        "background:#1A2E31;color:#F3F4F2;"
        "font-family:Arial;text-align:center;padding:40px'>"
        "<h2 style='color:#64C9CF'>loriota</h2>"
        "<h3>Nieprawidłowe dane sieci</h3>"
        "<p>Sprawdź nazwę sieci i hasło. Hasło do zabezpieczonej "
        "sieci Wi-Fi musi mieć co najmniej 8 znaków.</p>"
        "<a style='color:#64C9CF' href='/'>Wróć do konfiguracji</a>"
        "</body></html>");
    return;
  }

  Serial.println("[WiFi] Saving network configuration...");
  Serial.print("[WiFi] Selected SSID: ");
  Serial.println(ssid);

  if (!wifiPreferences.begin("loriota-wifi", false))
  {
    Serial.println("[WiFi] ERROR: Could not open Preferences.");

    configServer.send(
        500,
        "text/plain; charset=utf-8",
        "Nie udało się zapisać danych sieci Wi-Fi.");
    return;
  }

  size_t ssidBytes =
      wifiPreferences.putString("ssid", ssid);

  size_t passwordBytes =
      wifiPreferences.putString("password", password);

  wifiPreferences.end();

  if (ssidBytes == 0 ||
      (!password.isEmpty() && passwordBytes == 0))
  {

    Serial.println("[WiFi] ERROR: Saving credentials failed.");

    configServer.send(
        500,
        "text/plain; charset=utf-8",
        "Could not save Wi-Fi credentials.");
    return;
  }

  Serial.println("[WiFi] Credentials saved.");

  wifiConnectionFailed = false;
  wifiConnected = false;
  closePortalAfterConnection = true;

  configServer.sendHeader("Connection", "close");
  configServer.send(
      200,
      "text/html; charset=utf-8",
      makeWaitingPage());

  delay(200);

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(100);

  WiFi.begin(ssid.c_str(), password.c_str());

  wifiConnectStarted = millis();

  Serial.println("[WiFi] Connection attempt started.");
}


void startConfigPortal()
{
  if (configPortalActive)
  {
    Serial.println("[WiFi] Configuration portal is already active.");
    return;
  }

  Serial.println("[WiFi] Starting configuration portal.");

  WiFi.mode(WIFI_AP_STA);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD))
  {
    Serial.println("[WiFi] ERROR: Could not start SoftAP.");
    return;
  }

  configServer.on("/", HTTP_GET, handleConfigRoot);
  configServer.on("/save", HTTP_POST, handleConfigSave);
  configServer.on("/status", HTTP_GET, handleConfigStatus);

  configServer.on(
      "/generate_204",
      HTTP_GET,
      handleConfigRoot);

  configServer.on(
      "/hotspot-detect.html",
      HTTP_GET,
      handleConfigRoot);

  configServer.onNotFound([]()
                          {
        configServer.sendHeader("Location", "/", true);
        configServer.send(302, "text/plain", ""); });

  configServer.begin();
  configPortalActive = true;

  Serial.print("[WiFi] Setup network: ");
  Serial.println(AP_SSID);

  Serial.print("[WiFi] Portal address: http://");
  Serial.println(WiFi.softAPIP());
}


bool connectToSavedWiFi(unsigned long timeoutMs = 15000)
{
  if (!wifiPreferences.begin("loriota-wifi", true))
  {
    Serial.println("[WiFi] Could not open saved credentials.");
    return false;
  }

  String ssid = wifiPreferences.getString("ssid", "");
  String password = wifiPreferences.getString("password", "");

  wifiPreferences.end();

  if (ssid.isEmpty())
  {
    Serial.println("[WiFi] No saved credentials.");
    return false;
  }

  Serial.print("[WiFi] Connecting to saved network: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long started = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < timeoutMs)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WiFi] Connected successfully.");

    Serial.print("[WiFi] Local IP: ");
    Serial.println(WiFi.localIP());

    wifiConnected = true;
    wifiConnectedAt = millis();

    return true;
  }

  Serial.println("[WiFi] Connection failed.");
  return false;
}

void initializeFirebase()
{
  if (firebaseInitialized ||
      WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  config.service_account.data.client_email =
      FIREBASE_CLIENT_EMAIL;

  config.service_account.data.project_id =
      FIREBASE_PROJECT_ID;

  config.service_account.data.private_key =
      FIREBASE_PRIVATE_KEY;

  config.signer.tokens.scope =
      "https://www.googleapis.com/auth/firebase.messaging "
      "https://www.googleapis.com/auth/datastore";

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  firebaseInitialized = true;

  Serial.println("[Firebase] Initialized.");
}

void sendAlarmNotification(int state)
{
  xSemaphoreTake(serialMutex, portMAX_DELAY);
  Serial.println("\n[FCM] Sending notification...");
  xSemaphoreGive(serialMutex);

  String documentPath = "config/device";

  if (Firebase.Firestore.getDocument(
          &fbdo,
          FIREBASE_PROJECT_ID,
          "",
          documentPath.c_str(),
          ""))
  {

    FirebaseJson responseJson;
    responseJson.setJsonData(fbdo.payload().c_str());

    int i = 0;
    int successCount = 0;

    while (true)
    {
      delay(20);

      FirebaseJsonData tokenData;

      String path = "fields/tokens/arrayValue/values/[";
      path += String(i);
      path += "]/stringValue";

      responseJson.get(tokenData, path);

      if (!tokenData.success)
      {
        break;
      }

      String phoneToken = tokenData.stringValue;

      if (phoneToken.length() > 10)
      {
        FCM_HTTPv1_JSON_Message msg;

        msg.token = phoneToken;
        msg.notification.title = "🚨 ALARM LORIOTA!";

        if (state == 1)
        {
          msg.notification.body =
              "Wykryto otwarcie! Sprawdź aplikację Loriota.";
        }
        else if (state == 2)
        {
          msg.notification.body =
              "Wykryto wstrząsy! Sprawdź aplikację Loriota.";
        }
        else
        {
          msg.notification.body =
              "Wykryto aktywność czujnika!";
        }

        String appUrl = "https://loriota-8d71a.web.app";
        msg.webpush.fcm_options.link = appUrl;
        msg.android.priority = "high";
        msg.webpush.headers = "{\"Urgency\":\"high\"}";

        FirebaseJson customWebPush;

        customWebPush.set(
            "notification/requireInteraction",
            true);

        customWebPush.set("notification/vibrate/[0]", 300);
        customWebPush.set("notification/vibrate/[1]", 200);
        customWebPush.set("notification/vibrate/[2]", 300);

        msg.webpush.notification = customWebPush.raw();

        if (Firebase.FCM.send(&fbdo, &msg))
        {
          successCount++;
        }
        else
        {
          xSemaphoreTake(serialMutex, portMAX_DELAY);
          Serial.print("[FCM] Send failed: ");
          Serial.println(fbdo.errorReason());
          xSemaphoreGive(serialMutex);
        }
      }

      i++;
    }

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.printf(
        "[FCM] Notification sent to %d devices.\n",
        successCount);
    xSemaphoreGive(serialMutex);
  }
  else
  {
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print("[FCM] Could not read device tokens: ");
    Serial.println(fbdo.errorReason());
    xSemaphoreGive(serialMutex);
  }
}


void firebaseTask(void *pvParameters)
{
  QueueMessage msg;

  for (;;)
  {
    if (xQueueReceive(
            loraQueue,
            &msg,
            portMAX_DELAY) == pdPASS)
    {

      blinkLed(1);

      while (WiFi.status() != WL_CONNECTED ||
             !Firebase.ready())
      {
        vTaskDelay(pdMS_TO_TICKS(500));
      }

      xSemaphoreTake(serialMutex, portMAX_DELAY);
      Serial.println(
          "\n[Firebase Task] Uploading data to Firestore");
      xSemaphoreGive(serialMutex);

      FirebaseJson content;

      content.set(
          "fields/node_type/integerValue",
          msg.data.node_type);

      content.set(
          "fields/msg_counter/integerValue",
          msg.data.msg_counter);

      content.set(
          "fields/acc_x/integerValue",
          msg.data.acc_x);

      content.set(
          "fields/acc_y/integerValue",
          msg.data.acc_y);

      content.set(
          "fields/acc_z/integerValue",
          msg.data.acc_z);

      content.set(
          "fields/sensor_state/integerValue",
          msg.data.sensor_state);

      content.set(
          "fields/battery_lvl/integerValue",
          msg.data.battery_lvl);

      content.set(
          "fields/rssi/integerValue",
          (int)msg.rssi);

      struct tm timeinfo;

      if (getLocalTime(&timeinfo))
      {
        char timeStr[30];

        strftime(
            timeStr,
            sizeof(timeStr),
            "%Y-%m-%dT%H:%M:%SZ",
            &timeinfo);

        content.set(
            "fields/timestamp/timestampValue",
            timeStr);
      }

      String documentPath = "sensors/";
      documentPath += msg.data.node_id;

      String updateMask =
          "node_type,msg_counter,acc_x,acc_y,acc_z,"
          "sensor_state,battery_lvl,rssi,timestamp";

      bool patchStatus =
          Firebase.Firestore.patchDocument(
              &fbdo,
              FIREBASE_PROJECT_ID,
              "",
              documentPath.c_str(),
              content.raw(),
              updateMask.c_str());

      delay(50);

      xSemaphoreTake(serialMutex, portMAX_DELAY);

      if (patchStatus)
      {
        Serial.println(
            "[Firebase Task] Live data updated successfully");
      }
      else
      {
        Serial.print(
            "[Firebase Task] patchDocument failed: ");
        Serial.println(fbdo.errorReason());
      }

      xSemaphoreGive(serialMutex);

      String historyPath = documentPath;
      historyPath += "/readings";

      bool createStatus =
          Firebase.Firestore.createDocument(
              &fbdo,
              FIREBASE_PROJECT_ID,
              "",
              historyPath.c_str(),
              content.raw());

      delay(50);

      xSemaphoreTake(serialMutex, portMAX_DELAY);

      if (createStatus)
      {
        Serial.println(
            "[Firebase Task] Historical record appended successfully");
      }
      else
      {
        Serial.print(
            "[Firebase Task] createDocument failed: ");
        Serial.println(fbdo.errorReason());
      }

      xSemaphoreGive(serialMutex);

      uint16_t current_id = msg.data.node_id;
      int current_state = msg.data.sensor_state;

      if (previousSensorStates.find(current_id) ==
          previousSensorStates.end())
      {
        previousSensorStates[current_id] = 0;
      }

      int prev_state = previousSensorStates[current_id];

      if (current_state != prev_state &&
          (current_state == 1 || current_state == 2))
      {

        if (millis() - lastAlarmTime > 2000 ||
            lastAlarmTime == 0)
        {

          sendAlarmNotification(current_state);
          lastAlarmTime = millis();
        }
        else
        {
          xSemaphoreTake(serialMutex, portMAX_DELAY);
          Serial.println(
              "[FCM] Notification rejected (<2s).");
          xSemaphoreGive(serialMutex);
        }
      }

      previousSensorStates[current_id] = current_state;
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

  Serial.begin(115200);
  delay(3000);

  serialMutex = xSemaphoreCreateMutex();

  if (serialMutex == nullptr)
  {
    Serial.println("[ERROR] Could not create serial mutex.");

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println();
  Serial.println("LORIOTA GATEWAY");

  WiFi.persistent(false);

  if (!connectToSavedWiFi())
  {
    startConfigPortal();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WiFi] Synchronizing system time.");

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov");

    initializeFirebase();
  }
  else
  {
    Serial.println(
        "[Firebase] Initialization deferred until Wi-Fi connects.");
  }

  loraQueue = xQueueCreate(10, sizeof(QueueMessage));

  if (loraQueue == nullptr)
  {
    Serial.println("[ERROR] Could not create LoRa queue.");

    while (true)
    {
      delay(1000);
    }
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
    {
      delay(1000);
    }
  }

  SPI.begin(
      LORA_SCK,
      LORA_MISO,
      LORA_MOSI,
      -1);

  int state = radio.begin(
      868.0,
      125.0,
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
    radio.setDio1Action(setFlag);

    state = radio.startReceive();

    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println(
          "[LoRa] Initialized, waiting for messages.");
    }
    else
    {
      Serial.print("[LoRa] Receive start failed: ");
      Serial.println(state);
    }
  }

  blinkLed(3);
}

void loop()
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

  if (configPortalActive)
  {
    configServer.handleClient();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!wifiConnected)
    {
      wifiConnected = true;
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

      Serial.println("[WiFi] Closing configuration portal.");

      configServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);

      configPortalActive = false;
      closePortalAfterConnection = false;
      wifiConnectStarted = 0;
    }
  }
  else
  {
    if (wifiConnected)
    {
      wifiConnected = false;
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
        millis() - lastWifiRetry >= 10000)
    {

      lastWifiRetry = millis();

      if (!wifiPreferences.begin("loriota-wifi", true))
      {
        Serial.println(
            "[WiFi] Could not open saved credentials.");
      }
      else
      {
        String savedSsid =
            wifiPreferences.getString("ssid", "");

        String savedPassword =
            wifiPreferences.getString("password", "");

        wifiPreferences.end();

        if (!savedSsid.isEmpty())
        {
          Serial.println("[WiFi] Retrying saved network...");

          WiFi.mode(WIFI_STA);

          WiFi.begin(
              savedSsid.c_str(),
              savedPassword.c_str());
        }
      }
    }
  }

  if (receivedFlag)
  {
    receivedFlag = false;

    uint8_t byteArr[sizeof(LoRaNodeData)];

    int state = radio.readData(
        byteArr,
        sizeof(LoRaNodeData));

    if (state == RADIOLIB_ERR_NONE)
    {
      if (radio.getPacketLength() ==
          sizeof(LoRaNodeData))
      {

        QueueMessage msg;

        memcpy(
            &msg.data,
            byteArr,
            sizeof(LoRaNodeData));

        msg.rssi = radio.getRSSI(false);

        xSemaphoreTake(serialMutex, portMAX_DELAY);

        Serial.println(
            "\n[Radio Task] Correct frame captured");

        Serial.printf(
            "Node ID: %u, Counter: %u, State: %d\n",
            msg.data.node_id,
            msg.data.msg_counter,
            msg.data.sensor_state);

        xSemaphoreGive(serialMutex);

        if (xQueueSend(
                loraQueue,
                &msg,
                (TickType_t)10) != pdPASS)
        {

          xSemaphoreTake(serialMutex, portMAX_DELAY);

          Serial.println(
              "[Radio Task] ERROR: Queue is full - dropping frame");

          xSemaphoreGive(serialMutex);
        }
      }
    }
    else
    {
      xSemaphoreTake(serialMutex, portMAX_DELAY);

      Serial.print("[LoRa] Packet read failed: ");
      Serial.println(state);

      xSemaphoreGive(serialMutex);
    }

    radio.startReceive();
  }

  delay(1);
}