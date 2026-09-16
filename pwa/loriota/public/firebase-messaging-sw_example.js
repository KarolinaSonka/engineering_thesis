importScripts('https://www.gstatic.com/firebasejs/10.8.0/firebase-app-compat.js');
importScripts('https://www.gstatic.com/firebasejs/10.8.0/firebase-messaging-compat.js');

const firebaseConfig = {
  apiKey: "api_key",
  authDomain: "project_name.firebaseapp.com",
  projectId: "project_id",
  storageBucket: "project_name.firebasestorage.app",
  messagingSenderId: "msg_sender_id",
  appId: "app_id",
  measurementId: "meas_id"
};

firebase.initializeApp(firebaseConfig);
const messaging = firebase.messaging();

messaging.onBackgroundMessage((payload) => {
  console.log('[firebase-messaging-sw.js] Otrzymano alarm w tle!', payload);
});