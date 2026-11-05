#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <deque>
#include <map>
#include <vector>
#include <esp_arduino_version.h>

#define ENABLE_SERVICE_AUTH
#define ENABLE_DATABASE
#define ENABLE_MESSAGING
#include <FirebaseClient.h>

#include "sensitive_data.h"
#include "shared_state.h"
#include "firebase_service.h"

namespace
{
    extern const char google_roots_pem[] asm("_binary_data_cert_roots_pem_start");

    struct Reading
    {
        QueueMessage message;
        String timestamp;
    };

    struct Alarm
    {
        uint16_t nodeId;
        int state;
        uint8_t attempts;
    };

    constexpr size_t MAX_HISTORY_QUEUE = 64;
    constexpr size_t MAX_ALARM_QUEUE = 32;
    constexpr uint32_t RETRY_DELAY_MS = 2000;
    constexpr uint32_t HISTORY_TIMEOUT_MS = 30000;
    constexpr uint32_t HISTORY_RECOVERY_LOG_MS = 10000;

    constexpr const char *DATABASE_URL =
        "https://loriota-8d71a-default-rtdb.europe-west1.firebasedatabase.app/";

    constexpr const char *APP_URL =
        "https://loriota-8d71a.web.app";

    ServiceAuth serviceAuth(
        FIREBASE_CLIENT_EMAIL,
        FIREBASE_PROJECT_ID,
        FIREBASE_PRIVATE_KEY,
        3000);

    FirebaseApp firebaseApp;

    WiFiClientSecure authSSL;
    WiFiClientSecure liveSSL;
    WiFiClientSecure historySSL;
    WiFiClientSecure notificationSSL;

    AsyncClientClass authClient(authSSL);
    AsyncClientClass liveClient(liveSSL);
    AsyncClientClass historyClient(historySSL);
    AsyncClientClass notificationClient(notificationSSL);

    RealtimeDatabase database;
    Messaging messaging;

    std::map<uint16_t, Reading> pendingLive;
    std::deque<Reading> pendingHistory;
    std::deque<Alarm> pendingAlarms;
    std::map<uint16_t, uint32_t> lastAlarmByNode;

    std::vector<String> activeTokens;

    Reading currentLive;
    Alarm currentAlarm = {0, 0, 0};

    bool liveInFlight = false;
    bool historyInFlight = false;
    bool historyRecovering = false;
    bool tokenReadInFlight = false;
    bool notificationInFlight = false;
    bool alarmActive = false;

    volatile bool initializationRequested = false;
    volatile bool firebaseReadyFlag = false;

    size_t activeTokenIndex = 0;
    int activeSuccessCount = 0;

    uint32_t liveStarted = 0;
    uint32_t historyStarted = 0;
    uint32_t lastHistoryRecoveryLog = 0;
    uint32_t alarmStarted = 0;

    uint32_t nextLiveRetry = 0;
    uint32_t nextHistoryRetry = 0;
    uint32_t nextAlarmRetry = 0;

    bool due(uint32_t deadline)
    {
        return static_cast<int32_t>(millis() - deadline) >= 0;
    }

    void logMessage(const String &message)
    {
        if (serialMutex != nullptr)
        {
            xSemaphoreTake(serialMutex, portMAX_DELAY);
        }

        Serial.println(message);

        if (serialMutex != nullptr)
        {
            xSemaphoreGive(serialMutex);
        }
    }

    Reading makeReading(const QueueMessage &message)
    {
        Reading reading;
        reading.message = message;

        time_t now = time(nullptr);

        if (now > 1700000000)
        {
            struct tm utcTime;
            gmtime_r(&now, &utcTime);

            char buffer[32];

            strftime(
                buffer,
                sizeof(buffer),
                "%Y-%m-%dT%H:%M:%SZ",
                &utcTime);

            reading.timestamp = buffer;
        }

        return reading;
    }

    String createSensorJSON(
        const Reading &reading,
        bool includeNodeId)
    {
        const auto &data = reading.message.data;

        String json = "{";

        if (includeNodeId)
        {
            json += "\"node_id\":";
            json += String(data.node_id);
            json += ",";
        }

        json += "\"node_type\":";
        json += String(data.node_type);

        json += ",\"msg_counter\":";
        json += String(data.msg_counter);

        json += ",\"acc_x\":";
        json += String(data.acc_x);

        json += ",\"acc_y\":";
        json += String(data.acc_y);

        json += ",\"acc_z\":";
        json += String(data.acc_z);

        json += ",\"sensor_state\":";
        json += String(data.sensor_state);

        json += ",\"battery_lvl\":";
        json += String(data.battery_lvl);

        json += ",\"rssi\":";
        json += String(static_cast<int>(reading.message.rssi));

        if (!reading.timestamp.isEmpty())
        {
            json += ",\"timestamp\":\"";
            json += reading.timestamp;
            json += "\"";
        }

        json += "}";

        return json;
    }

    std::vector<String> parseTokens(const String &json)
    {
        std::vector<String> result;

        bool quoted = false;
        bool escaped = false;

        String value;

        for (size_t i = 0; i < json.length(); ++i)
        {
            char c = json[i];

            if (!quoted)
            {
                if (c == '"')
                {
                    quoted = true;
                    escaped = false;
                    value = "";
                }

                continue;
            }

            if (escaped)
            {
                value += c;
                escaped = false;
                continue;
            }

            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == '"')
            {
                quoted = false;

                if (value.length() > 30 && value.indexOf(':') >= 0)
                {
                    result.push_back(value);
                }

                continue;
            }

            value += c;
        }

        return result;
    }

    void onAuthResult(AsyncResult &result)
    {
        if (!result.isResult())
        {
            return;
        }

        if (result.isError())
        {
            logMessage(
                "[Firebase] Authentication error: " +
                result.error().message());
        }
    }

    void onLiveResult(AsyncResult &result)
    {
        if (!result.isResult())
        {
            return;
        }

        if (!result.isError() && !result.available())
        {
            return;
        }

        uint16_t nodeId = currentLive.message.data.node_id;
        uint32_t counter = currentLive.message.data.msg_counter;

        if (result.isError())
        {
            logMessage(
                "[RTDB] Live update failed for node " +
                String(nodeId) +
                ", counter " +
                String(counter) +
                ": " +
                result.error().message());

            nextLiveRetry = millis() + RETRY_DELAY_MS;
        }
        else
        {
            auto it = pendingLive.find(nodeId);

            if (it != pendingLive.end() &&
                it->second.message.data.msg_counter == counter)
            {
                pendingLive.erase(it);
            }

            logMessage(
                "[RTDB] Node " +
                String(nodeId) +
                " updated successfully.");

            logMessage(
                "[RTDB] Live update duration: " +
                String(millis() - liveStarted) +
                " ms");
        }

        liveInFlight = false;
    }

    void onHistoryResult(AsyncResult &result)
    {
        if (!result.isResult())
        {
            return;
        }

        if (!result.isError() && !result.available())
        {
            return;
        }

        logMessage(
            "[PERF] History write duration: " +
            String(millis() - historyStarted) +
            " ms");

        if (result.isError())
        {
            logMessage(
                "[RTDB] History write failed. Code: " +
                String(result.error().code()) +
                ", reason: " +
                result.error().message());

            logMessage(
                "[RTDB] History client tasks: " +
                String(historyClient.taskCount()));

            logMessage(
                "[RTDB] Pending records: " +
                String(pendingHistory.size()));

            logMessage(
                "[RTDB] Free heap: " +
                String(ESP.getFreeHeap()) + " bytes");

            nextHistoryRetry = millis() + RETRY_DELAY_MS;
        }
        else
        {
            if (!pendingHistory.empty())
            {
                pendingHistory.pop_front();
            }

            logMessage(
                "[RTDB] Historical reading saved successfully.");

            String entryId =
                result.to<RealtimeDatabaseResult>().name();

            if (!entryId.isEmpty())
            {
                logMessage(
                    "[RTDB] History entry ID: " + entryId);
            }
        }

        historyInFlight = false;
    }

    void finishAlarm()
    {
        logMessage(
            "[FCM] Notification sent to " +
            String(activeSuccessCount) +
            " devices.");

        logMessage(
            "[PERF] FCM duration: " +
            String(millis() - alarmStarted) +
            " ms");

        activeTokens.clear();

        activeTokenIndex = 0;
        activeSuccessCount = 0;

        alarmActive = false;
        notificationInFlight = false;
    }

    void onNotificationResult(AsyncResult &result)
    {
        if (!result.isResult())
        {
            return;
        }

        if (!result.isError() && !result.available())
        {
            return;
        }

        if (result.isError())
        {
            logMessage(
                "[FCM] Send failed: " +
                result.error().message());
        }
        else
        {
            ++activeSuccessCount;
        }

        notificationInFlight = false;
        ++activeTokenIndex;

        if (activeTokenIndex >= activeTokens.size())
        {
            finishAlarm();
        }
    }

    void onTokensResult(AsyncResult &result)
    {
        if (!result.isResult())
        {
            return;
        }

        if (!result.isError() && !result.available())
        {
            return;
        }

        tokenReadInFlight = false;

        if (result.isError())
        {
            logMessage(
                "[FCM] Token read failed: " +
                result.error().message());

            if (currentAlarm.attempts < 2)
            {
                ++currentAlarm.attempts;
                pendingAlarms.push_front(currentAlarm);

                nextAlarmRetry =
                    millis() + RETRY_DELAY_MS;
            }

            return;
        }

        activeTokens = parseTokens(String(result.c_str()));

        activeTokenIndex = 0;
        activeSuccessCount = 0;
        alarmStarted = millis();
        alarmActive = true;

        logMessage("\n[FCM] Sending notification...");
        logMessage("[FCM] Token source: Realtime Database");

        logMessage(
            "[FCM] Available tokens: " +
            String(activeTokens.size()));

        if (activeTokens.empty())
        {
            finishAlarm();
        }
    }

    void enqueueReading(const QueueMessage &message)
    {
        Reading reading = makeReading(message);

        uint16_t nodeId = message.data.node_id;
        int state = message.data.sensor_state;

        pendingLive[nodeId] = reading;

        if (pendingHistory.size() >= MAX_HISTORY_QUEUE)
        {
            if (historyInFlight && pendingHistory.size() > 1)
            {
                pendingHistory.erase(
                    pendingHistory.begin() + 1);
            }
            else
            {
                pendingHistory.pop_front();
            }

            logMessage(
                "[RTDB] History buffer full: oldest record discarded.");
        }

        pendingHistory.push_back(reading);

        int previousState = 0;

        auto previousIt =
            previousSensorStates.find(nodeId);

        if (previousIt != previousSensorStates.end())
        {
            previousState = previousIt->second;
        }

        if (state != previousState &&
            (state == 1 || state == 2))
        {
            uint32_t now = millis();

            auto alarmIt =
                lastAlarmByNode.find(nodeId);

            bool allowed =
                alarmIt == lastAlarmByNode.end() ||
                static_cast<uint32_t>(
                    now - alarmIt->second) >= 2000;

            if (allowed)
            {
                if (pendingAlarms.size() < MAX_ALARM_QUEUE)
                {
                    pendingAlarms.push_back(
                        {nodeId, state, 0});

                    lastAlarmByNode[nodeId] = now;
                    lastAlarmTime = now;
                }
                else
                {
                    logMessage(
                        "[FCM] Alarm buffer full.");
                }
            }
            else
            {
                logMessage(
                    "[FCM] Notification rejected (<2s for this node).");
            }
        }

        previousSensorStates[nodeId] = state;

        logMessage(
            "[PERF] Processing node " +
            String(nodeId) +
            ", counter " +
            String(message.data.msg_counter) +
            ", queued frames: " +
            String(uxQueueMessagesWaiting(loraQueue)));
    }

    void beginFirebase()
    {
        time_t now = time(nullptr);

        if (now <= 1700000000)
        {
            return;
        }

        authSSL.setCACert(google_roots_pem);
        liveSSL.setCACert(google_roots_pem);
        historySSL.setCACert(google_roots_pem);
        notificationSSL.setCACert(google_roots_pem);

        firebaseApp.setTime(
            static_cast<uint32_t>(now));

        initializeApp(
            authClient,
            firebaseApp,
            getAuth(serviceAuth),
            onAuthResult,
            "auth");

        firebaseApp.getApp<RealtimeDatabase>(database);
        firebaseApp.getApp<Messaging>(messaging);

        database.url(DATABASE_URL);

        firebaseInitialized = true;

        logMessage("[Firebase] Initialized.");
    }

    void scheduleLive()
    {
        if (liveInFlight ||
            pendingLive.empty() ||
            !due(nextLiveRetry))
        {
            return;
        }

        auto it = pendingLive.begin();

        currentLive = it->second;
        liveStarted = millis();
        liveInFlight = true;

        String path =
            "/sensors/" +
            String(currentLive.message.data.node_id) +
            "/live";

        String json = createSensorJSON(
            currentLive,
            false);

        database.set<object_t>(
            liveClient,
            path,
            object_t(json),
            onLiveResult,
            "live");
    }

    void scheduleHistory()
    {
        if (historyInFlight ||
            historyRecovering ||
            pendingHistory.empty() ||
            !due(nextHistoryRetry))
        {
            return;
        }

        const Reading &reading = pendingHistory.front();

        if (reading.timestamp.isEmpty())
        {
            pendingHistory.pop_front();
            logMessage(
                "[RTDB] History skipped: time not synchronized.");
            return;
        }

        historyStarted = millis();
        historyInFlight = true;

        String json = createSensorJSON(
            reading,
            true);

        database.push<object_t>(
            historyClient,
            "/history",
            object_t(json),
            onHistoryResult,
            "history");
    }

    void checkHistoryWatchdog()
    {
        uint32_t now = millis();

        if (historyRecovering)
        {
            size_t tasks = historyClient.taskCount();

            if (tasks == 0)
            {
                historyRecovering = false;
                historyInFlight = false;
                nextHistoryRetry = now + RETRY_DELAY_MS;

                logMessage(
                    "[RTDB] History client recovered. Retry scheduled.");

                return;
            }

            if ((uint32_t)(now - lastHistoryRecoveryLog) >=
                HISTORY_RECOVERY_LOG_MS)
            {
                lastHistoryRecoveryLog = now;

                logMessage(
                    "[RTDB] Waiting for history client cleanup. Tasks: " +
                    String(tasks));
            }

            return;
        }

        if (!historyInFlight)
        {
            return;
        }

        uint32_t elapsed = now - historyStarted;

        if (elapsed < HISTORY_TIMEOUT_MS)
        {
            return;
        }

        logMessage(
            "[RTDB] History watchdog timeout after " +
            String(elapsed) + " ms");

        logMessage(
            "[RTDB] History client tasks: " +
            String(historyClient.taskCount()));

        logMessage(
            "[RTDB] Pending history records: " +
            String(pendingHistory.size()));

        logMessage(
            "[RTDB] Free heap: " +
            String(ESP.getFreeHeap()) + " bytes");

        historyRecovering = true;
        lastHistoryRecoveryLog = now;

        historyClient.stopAsync(true);
    }

    void scheduleTokenRead()
    {
        if (tokenReadInFlight ||
            alarmActive ||
            pendingAlarms.empty() ||
            !due(nextAlarmRetry))
        {
            return;
        }

        currentAlarm = pendingAlarms.front();
        pendingAlarms.pop_front();

        tokenReadInFlight = true;

        database.get(
            notificationClient,
            "/config/device/tokens",
            onTokensResult,
            false,
            "tokens");
    }

    void scheduleNotification()
    {
        if (!alarmActive ||
            notificationInFlight ||
            activeTokenIndex >= activeTokens.size())
        {
            return;
        }

        const String &token =
            activeTokens[activeTokenIndex];

        Messages::Message msg;
        msg.token(token);

        Messages::Notification notification;

        notification.title("ALARM LORIOTA!");

        if (currentAlarm.state == 1)
        {
            notification.body(
                "Wykryto otwarcie! Sprawdź aplikację Loriota.");
        }
        else if (currentAlarm.state == 2)
        {
            notification.body(
                "Wykryto wstrząsy! Sprawdź aplikację Loriota.");
        }
        else
        {
            notification.body(
                "Wykryto aktywność czujnika!");
        }

        msg.notification(notification);

        Messages::WebpushFcmOptions webOptions;
        webOptions.link(APP_URL);

        Messages::WebpushConfig webpush;
        webpush.fcm_options(webOptions);

        msg.webpush(webpush);

        notificationInFlight = true;

        messaging.send(
            notificationClient,
            Messages::Parent(FIREBASE_PROJECT_ID),
            msg,
            onNotificationResult,
            "fcm");
    }
}

void initializeFirebase()
{
    initializationRequested = true;
}

bool firebaseServiceReady()
{
    return firebaseReadyFlag;
}

void sendAlarmNotification(int state)
{
    if (pendingAlarms.size() >= MAX_ALARM_QUEUE)
    {
        logMessage("[FCM] Alarm buffer full.");
        return;
    }

    pendingAlarms.push_back({0, state, 0});
}

void firebaseTask(void *pvParameters)
{
    QueueMessage message;

    for (;;)
    {
        while (xQueueReceive(
                   loraQueue,
                   &message,
                   0) == pdPASS)
        {
            enqueueReading(message);
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            firebaseReadyFlag = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (initializationRequested &&
            !firebaseInitialized)
        {
            beginFirebase();
        }

        if (firebaseInitialized)
        {
            firebaseApp.loop();

            checkHistoryWatchdog();

            firebaseReadyFlag = firebaseApp.ready();

            if (firebaseReadyFlag)
            {
                scheduleLive();
                scheduleHistory();

                if (alarmActive)
                {
                    scheduleNotification();
                }
                else
                {
                    scheduleTokenRead();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}