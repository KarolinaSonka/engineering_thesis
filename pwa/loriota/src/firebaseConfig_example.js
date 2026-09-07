// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getAnalytics } from "firebase/analytics";
import { getFirestore } from "firebase/firestore";

const firebaseConfig = {
  apiKey: "api_key",
  authDomain: "project_name.firebaseapp.com",
  projectId: "project_id",
  storageBucket: "project_name.firebasestorage.app",
  messagingSenderId: "msg_sender_id",
  appId: "app_id",
  measurementId: "meas_id"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const analytics = getAnalytics(app);
export const db = getFirestore(app);