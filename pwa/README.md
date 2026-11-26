# Loriota Progressive Web App

Loriota is a React-based web application for monitoring the sensor nodes and gateway. It uses Firebase Authentication, Realtime Database, and Cloud Messaging, and can be installed as a Progressive Web App.

## Features

- Sign in with a Firebase Authentication account.
- View the latest node readings, connection state, battery level, radio signal strength, and last update on the dashboard.
- Filter nodes by all locations, **home**, or **away**.
- Review the latest 50 records in the event history.
- Rename and organize nodes, remove a node from the current sensor list, manage browser push notifications, and open the gateway Wi-Fi setup portal.

## Firebase data used by the app

- `sensors/{nodeId}/live` — latest sensor reading.
- `sensors/{nodeId}/settings` — node name, type, and location assignment.
- `history` — timestamped readings displayed in the event history.
- `config/device/tokens` — browser notification tokens used by the gateway.

## Local setup

1. Install Node.js and npm.
2. Copy `src/firebaseConfig_example.js` to `src/firebaseConfig.js` and fill in the Firebase web app configuration and Realtime Database URL.
3. Copy `public/firebase-messaging-sw_example.js` to `public/firebase-messaging-sw.js` and use the same Firebase project configuration for background messaging.
4. Enable Firebase Authentication, Realtime Database, and Cloud Messaging for the project. Set `FCM_VAPID_KEY` in `src/pages/Settings.jsx` to the project's Web Push certificate key.
5. Install dependencies and start the development server:

   ```sh
   npm install
   npm run dev
   ```

The two configuration files are excluded by `.gitignore`; keep project-specific configuration out of commits. Browser notifications require permission and a secure context such as HTTPS or localhost.

## Deploy

Run these commands from `pwa/loriota` with the Firebase CLI authenticated and the correct project selected:

```sh
npm run build
firebase deploy
```

## Source layout

- `src/pages/` — login, dashboard, history, and settings screens.
- `src/components/` — shared navigation and visual components.
- `src/firebaseConfig_example.js` — Firebase client configuration template.
- `public/firebase-messaging-sw_example.js` — background Firebase Messaging service worker template.
