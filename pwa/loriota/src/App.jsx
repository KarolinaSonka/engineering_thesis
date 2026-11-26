import { useState, useEffect } from 'react';
import { BrowserRouter as Router, Routes, Route, Navigate } from 'react-router-dom';
import { onAuthStateChanged } from 'firebase/auth';
import { onMessage } from 'firebase/messaging';
import { auth, messaging } from './firebaseConfig';

import Navigation from './components/Navigation';
import AmbientBackground from './components/AmbientBackground';
import Dashboard from './pages/Dashboard';
import History from './pages/History';
import Settings from './pages/Settings';
import Login from './pages/Login';

const App = () => {
  const [user, setUser] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, (currentUser) => {
      setUser(currentUser);
      setLoading(false);
    });

    return () => unsubscribe();
  }, []);

  useEffect(() => {
    const unsubscribe = onMessage(messaging, async (payload) => {
      if (Notification.permission !== 'granted') {
        return;
      }

      try {
        const registration = await navigator.serviceWorker.ready;

        await registration.showNotification(
          payload.notification?.title || 'ALARM LORIOTA!',
          {
            body: payload.notification?.body || 'Wykryto zdarzenie alarmowe.',
            icon: '/pwa-192x192.png'
          }
        );
      } catch (error) {
        console.error('[FCM] Notification display failed:', error);
      }
    });

    return () => unsubscribe();
  }, []);

  if (loading) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-[#0d0a08]">
        <div className="w-8 h-8 border-2 border-[#4dc4c9] border-t-transparent rounded-full animate-spin"></div>
      </div>
    );
  }

  if (!user) {
    return <Login />;
  }

  return (
    <Router>
      <div className="min-h-screen w-full bg-[#0d0a08] text-slate-200 font-custom overflow-x-hidden relative flex flex-col items-center pb-24">
        <AmbientBackground />

        <div className="relative z-10 w-full max-w-7xl flex flex-col min-h-screen">
          <Routes>
            <Route path="/" element={<Dashboard />} />
            <Route path="/history" element={<History />} />
            <Route path="/settings" element={<Settings />} />
            <Route path="*" element={<Navigate to="/" />} />
          </Routes>
        </div>

        <Navigation />
      </div>
    </Router>
  );
};

export default App;
