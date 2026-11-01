#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "shared_state.h"
#include "wifi_config.h"
#include "firebase_service.h"

static constexpr int MAX_WIFI_NETWORKS = 10;
static int reconnectIndex = 0;

static String htmlEscape(String value)
{
    value.replace("&", "&amp;");
    value.replace("\"", "&quot;");
    value.replace("<", "&lt;");
    value.replace(">", "&gt;");
    value.replace("'", "&#39;");
    return value;
}

String jsonEscape(const String &value)
{
    String result;

    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];

        if (c == '"' || c == '\\')
        {
            result += '\\';
            result += c;
        }
        else if (c == '\n')
        {
            result += "\\n";
        }
        else if (c == '\r')
        {
            result += "\\r";
        }
        else if (static_cast<uint8_t>(c) >= 0x20)
        {
            result += c;
        }
    }

    return result;
}

static int loadSavedNetworks(String ssids[], String passwords[])
{
    if (!wifiPreferences.begin("loriota-wifi", true))
    {
        return 0;
    }

    int count = wifiPreferences.getInt("count", 0);

    if (count < 0)
        count = 0;

    if (count > MAX_WIFI_NETWORKS)
        count = MAX_WIFI_NETWORKS;

    for (int i = 0; i < count; i++)
    {
        String sKey = "ssid";
        sKey += i;

        String pKey = "pass";
        pKey += i;

        ssids[i] = wifiPreferences.getString(sKey.c_str(), "");
        passwords[i] = wifiPreferences.getString(pKey.c_str(), "");
    }

    wifiPreferences.end();

    return count;
}

static bool saveWiFiNetwork(const String &ssid, const String &password)
{
    String ssids[MAX_WIFI_NETWORKS];
    String passwords[MAX_WIFI_NETWORKS];

    int count = loadSavedNetworks(ssids, passwords);
    int total = 0;

    for (int i = 0; i < count; i++)
    {
        if (ssids[i].length() > 0 && ssids[i] != ssid)
        {
            ssids[total] = ssids[i];
            passwords[total] = passwords[i];
            total++;
        }
    }

    if (total == MAX_WIFI_NETWORKS)
        total--;

    for (int i = total; i > 0; i--)
    {
        ssids[i] = ssids[i - 1];
        passwords[i] = passwords[i - 1];
    }

    ssids[0] = ssid;
    passwords[0] = password;
    total++;

    if (!wifiPreferences.begin("loriota-wifi", false))
        return false;

    bool ok = true;

    for (int i = 0; i < total; i++)
    {
        String sKey = "ssid";
        sKey += i;

        String pKey = "pass";
        pKey += i;

        if (wifiPreferences.putString(sKey.c_str(), ssids[i]) != ssids[i].length())
            ok = false;

        if (wifiPreferences.putString(pKey.c_str(), passwords[i]) != passwords[i].length())
            ok = false;
    }

    if (ok && !wifiPreferences.putInt("count", total))
        ok = false;

    wifiPreferences.end();

    return ok;
}

String makeConfigPage()
{
    String html = R"HTML(
<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Loriota — Wi-Fi</title>
<style>
body {
  margin: 0;
  background: #1A2E31;
  color: #F3F4F2;
  font: 15px Arial, sans-serif;
  padding: 20px;
}
main {
  max-width: 420px;
  margin: 7vh auto;
  background: #243638;
  padding: 25px;
  border: 1px solid #52605F;
  border-radius: 10px;
}
h1 { color: #64C9CF; }
p { color: #B4BDBB; line-height: 1.5; }
select, input, button {
  box-sizing: border-box;
  width: 100%;
  padding: 12px;
  margin: 9px 0;
  border-radius: 6px;
  font: inherit;
}
select, input {
  background: #202E30;
  color: white;
  border: 1px solid #52605F;
}
button {
  background: #A85E3D;
  border: 0;
  color: white;
  cursor: pointer;
}
</style>
</head>
<body>
<main>
<h1>loriota</h1>
<p>Wybierz sieć Wi-Fi 2,4 GHz i wpisz hasło.</p>
<form method="POST" action="/save">
<label for="ssid">Dostępne sieci:</label>
<select id="ssid" name="ssid" required>
)HTML";

    int count = WiFi.scanNetworks();

    if (count <= 0)
    {
        html += "<option value=''>Nie znaleziono sieci</option>";
    }

    for (int i = 0; i < count; ++i)
    {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty())
            continue;

        String safe = htmlEscape(ssid);

        html += "<option value=\"";
        html += safe;
        html += "\">";
        html += safe;
        html += " (";
        html += String(WiFi.RSSI(i));
        html += " dBm)</option>";
    }

    WiFi.scanDelete();

    html += R"HTML(
</select>
<label for="password">Hasło:</label>
<input id="password" name="password" type="password"
       maxlength="63" autocomplete="new-password">
<button type="submit">Zapisz i połącz</button>
</form>
<form action="/" method="GET">
<button type="submit">Odśwież sieci</button>
</form>
</main>
</body>
</html>
)HTML";

    return html;
}

String makeWaitingPage()
{
    return R"HTML(
<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Loriota — status</title>
<style>
body {
  margin: 0;
  padding: 20px;
  background: #1A2E31;
  color: #F3F4F2;
  font: 15px Arial;
}
main {
  max-width: 420px;
  margin: 7vh auto;
  background: #243638;
  padding: 25px;
  border: 1px solid #52605F;
  border-radius: 10px;
}
h1 { color: #64C9CF; }
p { line-height: 1.6; }
a { color: #64C9CF; }
</style>
</head>
<body>
<main>
<h1>loriota</h1>
<h2 id="title">Łączenie z Wi-Fi…</h2>
<p id="wifi">Trwa nawiązywanie połączenia.</p>
<p id="firebase">Firebase: oczekiwanie.</p>
<p id="retry"></p>
</main>
<script>
async function poll() {
  try {
    const r = await fetch('/status', {cache: 'no-store'});
    if (!r.ok) return;

    const s = await r.json();

    if (s.wifi === 'connected') {
      document.getElementById('title').textContent = 'Połączono z Wi-Fi';
      document.getElementById('wifi').textContent =
        s.ssid + ' — IP: ' + s.ip;
      document.getElementById('firebase').textContent =
        'Firebase: ' + (s.firebase === 'ready' ? 'gotowy' : 'łączenie…');
    } else if (s.wifi === 'failed') {
      document.getElementById('title').textContent =
        'Nie udało się połączyć';
      document.getElementById('wifi').textContent =
        'Sprawdź hasło i sieć 2,4 GHz.';
      document.getElementById('retry').innerHTML =
        '<a href="/">Wróć do konfiguracji</a>';
    }
  } catch (e) {}
}

poll();
setInterval(poll, 1000);
</script>
</body>
</html>
)HTML";
}

void handleConfigRoot()
{
    configServer.send(200, "text/html; charset=utf-8", makeConfigPage());
}

void handleConfigStatus()
{
    bool connected = WiFi.status() == WL_CONNECTED;

    String wifi = connected ? "connected" : (wifiConnectionFailed ? "failed" : "connecting");

    String firebase = !connected ? "waiting" : (firebaseServiceReady() ? "ready" : "connecting");

    String result = "{\"wifi\":\"";
    result += wifi;
    result += "\",\"firebase\":\"";
    result += firebase;
    result += "\",\"ssid\":\"";
    result += jsonEscape(connected ? WiFi.SSID() : "");
    result += "\",\"ip\":\"";
    result += connected ? WiFi.localIP().toString() : "";
    result += "\"}";

    configServer.sendHeader("Cache-Control", "no-store");
    configServer.send(200, "application/json; charset=utf-8", result);
}

void handleConfigSave()
{
    if (!configServer.hasArg("ssid") ||
        !configServer.hasArg("password"))
    {
        configServer.send(400, "text/plain; charset=utf-8", "Brak danych sieci.");
        return;
    }

    String ssid = configServer.arg("ssid");
    String password = configServer.arg("password");
    ssid.trim();

    if (ssid.isEmpty() || ssid.length() > 32 ||
        password.length() > 63 ||
        (!password.isEmpty() && password.length() < 8))
    {
        configServer.send(
            400, "text/plain; charset=utf-8",
            "Nieprawidłowa nazwa sieci lub hasło.");
        return;
    }

    if (!saveWiFiNetwork(ssid, password))
    {
        configServer.send(
            500, "text/plain; charset=utf-8",
            "Nie udało się zapisać Wi-Fi.");
        return;
    }

    Serial.print("[WiFi] Saved new network: ");
    Serial.println(ssid);

    wifiConnectionFailed = false;
    wifiConnected = false;
    closePortalAfterConnection = true;

    configServer.sendHeader("Connection", "close");
    configServer.send(
        200, "text/html; charset=utf-8",
        makeWaitingPage());

    delay(200);
    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());

    wifiConnectStarted = millis();
}

void startConfigPortal()
{
    if (configPortalActive)
        return;

    Serial.println("[WiFi] Starting configuration portal.");
    WiFi.mode(WIFI_AP_STA);

    if (!WiFi.softAP(AP_SSID, AP_PASSWORD))
    {
        Serial.println("[WiFi] Could not start access point.");
        return;
    }

    configServer.on("/", HTTP_GET, handleConfigRoot);
    configServer.on("/save", HTTP_POST, handleConfigSave);
    configServer.on("/status", HTTP_GET, handleConfigStatus);
    configServer.on("/generate_204", HTTP_GET, handleConfigRoot);
    configServer.on("/hotspot-detect.html", HTTP_GET, handleConfigRoot);

    configServer.onNotFound([]()
                            {
    configServer.sendHeader("Location", "/", true);
    configServer.send(302, "text/plain", ""); });

    configServer.begin();
    configPortalActive = true;

    Serial.print("[WiFi] Portal: http://");
    Serial.println(WiFi.softAPIP());
}

static void scanSavedNetworks(const String ssids[], int count, bool visible[])
{
    int found = WiFi.scanNetworks();

    for (int i = 0; i < count; i++)
    {
        visible[i] = false;

        for (int j = 0; j < found; j++)
        {
            if (!ssids[i].isEmpty() && ssids[i] == WiFi.SSID(j))
            {
                visible[i] = true;
                break;
            }
        }
    }

    WiFi.scanDelete();
}

bool connectToSavedWiFi(unsigned long timeoutMs)
{
    String ssids[MAX_WIFI_NETWORKS];
    String passwords[MAX_WIFI_NETWORKS];

    int count = loadSavedNetworks(ssids, passwords);
    if (count == 0)
        return false;

    WiFi.mode(WIFI_STA);

    bool visible[MAX_WIFI_NETWORKS] = {};
    scanSavedNetworks(ssids, count, visible);

    unsigned long started = millis();

    for (int i = 0; i < count && millis() - started < timeoutMs; i++)
    {
        if (!visible[i])
            continue;

        Serial.print("[WiFi] Trying known network: ");
        Serial.println(ssids[i]);

        WiFi.disconnect(false, false);
        WiFi.begin(ssids[i].c_str(), passwords[i].c_str());

        unsigned long attempt = millis();

        while (WiFi.status() != WL_CONNECTED &&
               millis() - attempt < 5000 &&
               millis() - started < timeoutMs)
        {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            wifiConnected = true;
            wifiConnectedAt = millis();
            return true;
        }
    }

    WiFi.disconnect(false, false);
    return false;
}

bool reconnectToSavedWiFi()
{
    String ssids[MAX_WIFI_NETWORKS];
    String passwords[MAX_WIFI_NETWORKS];

    int count = loadSavedNetworks(ssids, passwords);
    if (count == 0)
        return false;

    WiFi.mode(WIFI_STA);

    bool visible[MAX_WIFI_NETWORKS] = {};
    scanSavedNetworks(ssids, count, visible);

    for (int step = 0; step < count; step++)
    {
        int i = (reconnectIndex + step) % count;

        if (!visible[i])
            continue;

        reconnectIndex = (i + 1) % count;

        Serial.print("[WiFi] Reconnecting to: ");
        Serial.println(ssids[i]);

        WiFi.disconnect(false, false);
        WiFi.begin(ssids[i].c_str(), passwords[i].c_str());

        return true;
    }

    return false;
}