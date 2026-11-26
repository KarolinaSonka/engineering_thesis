# HELTECV4 Gateway

This folder contains the PlatformIO firmware for the Heltec WiFi LoRa 32 V4 gateway. The gateway receives LoRa frames from the STM32 sensor nodes, forwards readings to Firebase Realtime Database, and sends Firebase Cloud Messaging alerts for sensor-state events.

## Project layout

- `gate/src/main.cpp` initializes the board, LoRa radio, FreeRTOS tasks, and Wi-Fi reconnection handling.
- `gate/src/wifi_config.cpp` provides the Wi-Fi setup portal and stores network credentials in ESP32 Preferences.
- `gate/src/firebase_service.cpp` queues live and historical readings, retries cloud requests, and sends notifications.
- `gate/include/lora_frame.h` defines the packed LoRa frame shared with the STM32 nodes.

## Build and configure

Open `gate` as a PlatformIO project and build the `heltec_wifi_lora_32_V3` environment. To configure Wi-Fi, start the gateway while holding the configuration button (PRG) and use the setup portal when no saved network is available.

Firebase credentials are provided through `gate/include/sensitive_data.h`. Use `sensitive_data_example.h` as a template and keep real credentials out of version control.
