# Loriota IoT System

This project combines low-power sensor nodes, a LoRa gateway, and a browser-based Progressive Web App for monitoring the system.

## System components

| Component | Role | Documentation |
| --- | --- | --- |
| STM32 sensor nodes | Sample the ADXL345 accelerometer and, depending on the node, an MC-38 reed switch, A3114 Hall sensor, or TCRT5000 infrared sensor. Send reports over LoRa. | [`embedded/STM32WLE5JC`](embedded/STM32WLE5JC/README.md) |
| Heltec gateway | Receives node reports and forwards current readings, history, and sensor alarms to Firebase. | [`embedded/HELTECV4`](embedded/HELTECV4/README.md) |
| Loriota PWA | Authenticated dashboard for live readings, event history, node settings, and browser notifications. | [`pwa/loriota`](pwa/loriota/README.md) |

## Data flow

Sensor nodes send LoRa frames to the Heltec gateway. The gateway stores current and historical readings in Firebase Realtime Database and sends Firebase Cloud Messaging alerts for sensor-state events. The PWA reads those records and lets users manage node labels, locations, and notifications.

See the component READMEs for build instructions and local configuration. Real Firebase configuration and credentials are project-specific and are not included in the repository; example templates are provided where needed.

To publish the PWA, go to `pwa/loriota`, run `npm run build`, then run `firebase deploy` with the Firebase CLI configured for the target project.
