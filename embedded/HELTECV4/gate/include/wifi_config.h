#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

String jsonEscape(const String &input);
String makeConfigPage();
String makeWaitingPage();

void handleConfigRoot();
void handleConfigStatus();
void handleConfigSave();
void startConfigPortal();

bool connectToSavedWiFi(unsigned long timeoutMs = 15000);
bool reconnectToSavedWiFi();

#endif