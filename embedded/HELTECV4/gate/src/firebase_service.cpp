#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <time.h>

#include "sensitive_data.h"
#include "shared_state.h"
#include "firebase_service.h"

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

    config.database_url =
        "https://loriota-8d71a-default-rtdb.europe-west1.firebasedatabase.app/";

    config.signer.tokens.scope =
        "https://www.googleapis.com/auth/firebase.messaging "
        "https://www.googleapis.com/auth/firebase.database "
        "https://www.googleapis.com/auth/userinfo.email";

    config.token_status_callback =
        tokenStatusCallback;

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

    if (!Firebase.RTDB.getArray(
            &fbdo,
            "/config/device/tokens"))
    {
        xSemaphoreTake(serialMutex, portMAX_DELAY);

        Serial.print("[FCM] Token read failed: ");
        Serial.println(fbdo.errorReason());

        xSemaphoreGive(serialMutex);
        return;
    }

    FirebaseJsonArray tokens;
    tokens.setJsonArrayData(
        fbdo.jsonArray().raw()
    );

    const size_t tokenCount = tokens.size();

    xSemaphoreTake(serialMutex, portMAX_DELAY);

    Serial.println(
        "[FCM] Token source: Realtime Database"
    );

    Serial.printf(
        "[FCM] Available tokens: %u\n",
        (unsigned int)tokenCount
    );

    xSemaphoreGive(serialMutex);

    int successCount = 0;

    for (size_t i = 0; i < tokenCount; i++)
    {
        FirebaseJsonData tokenData;
        tokens.get(tokenData, i);

        if (!tokenData.success)
        {
            continue;
        }

        String deviceToken = tokenData.stringValue;

        if (deviceToken.length() <= 10)
        {
            continue;
        }

        FCM_HTTPv1_JSON_Message msg;

        msg.token = deviceToken;

        msg.notification.title =
            "ALARM LORIOTA!";

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

        msg.webpush.fcm_options.link =
            "https://loriota-8d71a.web.app";

        msg.android.priority = "high";

        msg.webpush.headers =
            "{\"Urgency\":\"high\"}";

        FirebaseJson customWebPush;

        customWebPush.set(
            "notification/requireInteraction",
            true
        );

        customWebPush.set(
            "notification/vibrate/[0]",
            300
        );

        customWebPush.set(
            "notification/vibrate/[1]",
            200
        );

        customWebPush.set(
            "notification/vibrate/[2]",
            300
        );

        msg.webpush.notification =
            customWebPush.raw();

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

    xSemaphoreTake(serialMutex, portMAX_DELAY);

    Serial.printf(
        "[FCM] Notification sent to %d devices.\n",
        successCount
    );

    xSemaphoreGive(serialMutex);
}

void firebaseTask(void *pvParameters)
{
    QueueMessage msg;

    for (;;)
    {
        if (xQueueReceive(
                loraQueue,
                &msg,
                portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        blinkLed(1);

        while (
            WiFi.status() != WL_CONNECTED ||
            !Firebase.ready()
        )
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        time_t now = time(nullptr);

        bool validTime =
            now > 1700000000;

        char timestamp[32] = {0};

        if (validTime)
        {
            struct tm utcTime;

            gmtime_r(&now, &utcTime);

            strftime(
                timestamp,
                sizeof(timestamp),
                "%Y-%m-%dT%H:%M:%SZ",
                &utcTime
            );
        }

        FirebaseJson liveData;

        liveData.set(
            "node_type",
            (int)msg.data.node_type
        );

        liveData.set(
            "msg_counter",
            (int)msg.data.msg_counter
        );

        liveData.set(
            "acc_x",
            (int)msg.data.acc_x
        );

        liveData.set(
            "acc_y",
            (int)msg.data.acc_y
        );

        liveData.set(
            "acc_z",
            (int)msg.data.acc_z
        );

        liveData.set(
            "sensor_state",
            (int)msg.data.sensor_state
        );

        liveData.set(
            "battery_lvl",
            (int)msg.data.battery_lvl
        );

        liveData.set(
            "rssi",
            (int)msg.rssi
        );

        if (validTime)
        {
            liveData.set(
                "timestamp",
                String(timestamp)
            );
        }

        String livePath = "/sensors/";
        livePath += String(msg.data.node_id);
        livePath += "/live";

        unsigned long uploadStarted =
            millis();

        bool liveSaved =
            Firebase.RTDB.setJSON(
                &fbdo,
                livePath.c_str(),
                &liveData
            );

        unsigned long uploadDuration =
            millis() - uploadStarted;

        xSemaphoreTake(serialMutex, portMAX_DELAY);

        if (liveSaved)
        {
            Serial.printf(
                "[RTDB] Node %u updated successfully.\n",
                msg.data.node_id
            );

            Serial.printf(
                "[RTDB] Live update duration: %lu ms\n",
                uploadDuration
            );
        }
        else
        {
            Serial.print(
                "[RTDB] Live update failed: "
            );

            Serial.println(
                fbdo.errorReason()
            );
        }

        xSemaphoreGive(serialMutex);

        if (validTime)
        {
            FirebaseJson historyData;

            historyData.set(
                "node_id",
                (int)msg.data.node_id
            );

            historyData.set(
                "node_type",
                (int)msg.data.node_type
            );

            historyData.set(
                "msg_counter",
                (int)msg.data.msg_counter
            );

            historyData.set(
                "acc_x",
                (int)msg.data.acc_x
            );

            historyData.set(
                "acc_y",
                (int)msg.data.acc_y
            );

            historyData.set(
                "acc_z",
                (int)msg.data.acc_z
            );

            historyData.set(
                "sensor_state",
                (int)msg.data.sensor_state
            );

            historyData.set(
                "battery_lvl",
                (int)msg.data.battery_lvl
            );

            historyData.set(
                "rssi",
                (int)msg.rssi
            );

            historyData.set(
                "timestamp",
                String(timestamp)
            );

            bool historySaved =
                Firebase.RTDB.pushJSON(
                    &fbdo,
                    "/history",
                    &historyData
                );

            xSemaphoreTake(
                serialMutex,
                portMAX_DELAY
            );

            if (historySaved)
            {
                Serial.println(
                    "[RTDB] Historical reading saved successfully."
                );

                Serial.print(
                    "[RTDB] History entry ID: "
                );

                Serial.println(
                    fbdo.pushName()
                );
            }
            else
            {
                Serial.print(
                    "[RTDB] History write failed: "
                );

                Serial.println(
                    fbdo.errorReason()
                );
            }

            xSemaphoreGive(serialMutex);
        }
        else
        {
            xSemaphoreTake(
                serialMutex,
                portMAX_DELAY
            );

            Serial.println(
                "[RTDB] History skipped: time not synchronized."
            );

            xSemaphoreGive(serialMutex);
        }

        uint16_t currentId =
            msg.data.node_id;

        int currentState =
            msg.data.sensor_state;

        if (
            previousSensorStates.find(currentId) ==
            previousSensorStates.end()
        )
        {
            previousSensorStates[currentId] = 0;
        }

        int previousState =
            previousSensorStates[currentId];

        if (
            currentState != previousState &&
            (currentState == 1 || currentState == 2)
        )
        {
            if (
                millis() - lastAlarmTime > 2000 ||
                lastAlarmTime == 0
            )
            {
                sendAlarmNotification(
                    currentState
                );

                lastAlarmTime = millis();
            }
            else
            {
                xSemaphoreTake(
                    serialMutex,
                    portMAX_DELAY
                );

                Serial.println(
                    "[FCM] Notification rejected (<2s)."
                );

                xSemaphoreGive(serialMutex);
            }
        }

        previousSensorStates[currentId] =
            currentState;
    }
}