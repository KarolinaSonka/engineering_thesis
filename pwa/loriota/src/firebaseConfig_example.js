import { initializeApp } from "firebase/app";
import { getAnalytics } from "firebase/analytics";
import { getAuth } from "firebase/auth";
import { getMessaging } from "firebase/messaging";
import { getDatabase, forceWebSockets } from "firebase/database";

const firebaseConfig = {
  apiKey: "api_key",
  authDomain: "project_name.firebaseapp.com",
  projectId: "project_id",
  storageBucket: "project_name.firebasestorage.app",
  messagingSenderId: "msg_sender_id",
  appId: "app_id",
  measurementId: "meas_id"
};

const app = initializeApp(firebaseConfig);

forceWebSockets();
export const rtdb = getDatabase(
  app,
  "database_url"
);

export const auth = getAuth(app);
export const messaging = getMessaging(app);
export const analytics = getAnalytics(app);
