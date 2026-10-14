#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <time.h>
#include <string.h>

#include "sensitive_data.h"
#include "shared_state.h"
#include "firebase_service.h"

void initializeFirebase()
{
    if (firebaseInitialized || WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    config.service_account.data.client_email = FIREBASE_CLIENT_EMAIL;
    config.service_account.data.project_id = FIREBASE_PROJECT_ID;
    config.service_account.data.private_key = FIREBASE_PRIVATE_KEY;

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
            &fbdo, FIREBASE_PROJECT_ID, "",
            documentPath.c_str(), ""))
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
                break;

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
                customWebPush.set("notification/requireInteraction", true);
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
        Serial.printf("[FCM] Notification sent to %d devices.\n", successCount);
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
        if (xQueueReceive(loraQueue, &msg, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        blinkLed(1);

        while (WiFi.status() != WL_CONNECTED || !Firebase.ready())
        {
            vTaskDelay(pdMS_TO_TICKS(500));
        }

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
        content.set("fields/rssi/integerValue", (int)msg.rssi);

        struct tm timeinfo;

        if (getLocalTime(&timeinfo))
        {
            char timeStr[30];

            strftime(
                timeStr, sizeof(timeStr),
                "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

            content.set("fields/timestamp/timestampValue", timeStr);
        }

        String documentPath = "sensors/";
        documentPath += msg.data.node_id;

        String updateMask =
            "node_type,msg_counter,acc_x,acc_y,acc_z,"
            "sensor_state,battery_lvl,rssi,timestamp";

        bool patchStatus = Firebase.Firestore.patchDocument(
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
            Serial.println("[Firebase Task] Live data updated successfully");
        }
        else
        {
            Serial.print("[Firebase Task] patchDocument failed: ");
            Serial.println(fbdo.errorReason());
        }

        xSemaphoreGive(serialMutex);

        String historyPath = documentPath;
        historyPath += "/readings";

        bool createStatus = Firebase.Firestore.createDocument(
            &fbdo,
            FIREBASE_PROJECT_ID,
            "",
            historyPath.c_str(),
            content.raw());

        delay(50);

        xSemaphoreTake(serialMutex, portMAX_DELAY);

        if (createStatus)
        {
            Serial.println("[Firebase Task] Historical record appended successfully");
        }
        else
        {
            Serial.print("[Firebase Task] createDocument failed: ");
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

            if (millis() - lastAlarmTime > 2000 || lastAlarmTime == 0)
            {
                sendAlarmNotification(current_state);
                lastAlarmTime = millis();
            }
            else
            {
                xSemaphoreTake(serialMutex, portMAX_DELAY);
                Serial.println("[FCM] Notification rejected (<2s).");
                xSemaphoreGive(serialMutex);
            }
        }

        previousSensorStates[current_id] = current_state;
    }
}