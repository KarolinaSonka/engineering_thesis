#ifndef FIREBASE_SERVICE_H
#define FIREBASE_SERVICE_H

void initializeFirebase();
void sendAlarmNotification(int state);
void firebaseTask(void *pvParameters);

#endif