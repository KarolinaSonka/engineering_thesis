import React, { useState, useEffect } from 'react';
import { collectionGroup, query, orderBy, limit, onSnapshot } from 'firebase/firestore';
import { db } from '../firebaseConfig';
import { Clock, Activity, AlertTriangle, Lock, Unlock, Zap, CheckCircle2 } from 'lucide-react';

const History = () => {
  const [logs, setLogs] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error_msg, setErrorMsg] = useState('');

  useEffect(() => {
    const q = query(
      collectionGroup(db, 'readings'),
      orderBy('timestamp', 'desc'),
      limit(50)
    );

    const unsub = onSnapshot(q, (snapshot) => {
      const history_data = [];
      
      snapshot.forEach((doc_snap) => {
        const data = doc_snap.data();
        
        const path_segments = doc_snap.ref.path.split('/');
        const node_id = path_segments.length > 2 ? path_segments[1] : 'Nieznany';
        
        let date = null;
        let hour = "Nieznany czas";
        let day = "";
        
        if (data.timestamp) {
          try {
            date = data.timestamp.toDate ? data.timestamp.toDate() : new Date(data.timestamp);
            hour = date.toLocaleTimeString('pl-PL', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
            day = date.toLocaleDateString('pl-PL', { day: 'numeric', month: 'short' });
          } catch (e) {
            console.error("Błąd parsowania daty:", e);
          }
        }

        let activity = "Zarejestrowano aktywność";
        let event_icon = Activity;
        let icon_color = "text-white/50";
        let description = "Zarejestrowano standardowy odczyt kontrolny z urządzenia.";
        const type = Number(data.node_type);

        if (type === 0) {
          if (data.sensor_state === 1) {
            activity = "Wykryto ruch / drgania";
            event_icon = Zap;
            icon_color = "text-amber-400";
            description = `Zarejestrowano zmianę położenia. (Oś X:${data.acc_x} Y:${data.acc_y} Z:${data.acc_z})`;
          } else {
            activity = "Stan spoczynku";
            event_icon = Activity;
            icon_color = "text-white/50";
            description = `Urządzenie nie rejestruje drgań. (Oś X:${data.acc_x} Y:${data.acc_y} Z:${data.acc_z})`;
          }
        }

        if (type === 1) {
          if (data.sensor_state === 1) {
            activity = "Otwarte.";
            event_icon = Unlock; 
            icon_color = "text-[#d9652b]";
            description = "Kontaktron nie wykrywa magnesu.";
          } else {
            activity = "Zamknięte.";
            event_icon = Lock;
            icon_color = "text-[#4dc4c9]";
            description = "Kontaktron wykrywa magnes.";
          }
        }

        if (type === 2) {
          if (data.sensor_state === 1) {
            activity = "Otwarte.";
            event_icon = Unlock; 
            icon_color = "text-[#d9652b]";
            description = "Czujnik Halla nie wykrywa pola magnetycznego.";
          } else {
            activity = "Zamknięte.";
            event_icon = Lock;
            icon_color = "text-[#4dc4c9]";
            description = "Czujnik Halla wykrywa pole magnetyczne magnesu.";
          }
        }

        if (type === 3) {
          if (data.sensor_state === 1) {
            activity = "Otwarte.";
            event_icon = Activity; 
            icon_color = "text-amber-400";
            description = "Czujnik podczerwieni ma czystą linię widzenia.";
          } else {
            activity = "Zamknięte.";
            event_icon = CheckCircle2;
            icon_color = "text-[#4dc4c9]";
            description = "Czujnik podczerwieni zarejestrował obiekt w zasięgu widzenia.";
          }
        }

        history_data.push({
          id: doc_snap.id,
          node_id: node_id, 
          hour,
          day,
          title: activity,
          description: description,
          Icon: event_icon,
          icon_color
        });
      });
      
      setLogs(history_data);
      setLoading(false);
      setErrorMsg('');
    }, (error) => {
        console.error("Błąd bazy danych:", error);
    });

    return () => unsub();
  }, []);

  return (
    <div className="p-4 md:p-8 animate-in fade-in duration-500 w-full max-w-3xl mx-auto mb-20">
      
      <header className="flex items-center gap-4 mb-12 border-b border-white/5 pb-6">
        <div className="w-12 h-12 rounded-full bg-[#4dc4c9]/10 flex items-center justify-center border border-[#4dc4c9]/20">
          <Clock className="text-[#4dc4c9]" size={24} />
        </div>
        <div>
          <h1 className="text-2xl md:text-3xl font-light text-white tracking-tight">Dziennik zdarzeń</h1>
          <p className="text-white/40 font-light text-sm mt-1">Ostatnie 50 aktywności w sieci IoT</p>
        </div>
      </header>

      {error_msg && (
        <div className="bg-red-500/10 border border-red-500/20 rounded-2xl p-4 mb-8 text-red-400 text-sm font-light">
          {error_msg}
        </div>
      )}

      {loading ? (
        <div className="flex items-center justify-center py-20 text-white/50">
           <Clock className="animate-spin mr-2" size={20} /> Ładowanie danych...
        </div>
      ) : logs.length === 0 && !error_msg ? (
        <div className="text-center py-20 text-white/40 font-light">
          Brak historii zdarzeń.
        </div>
      ) : (
        <div className="relative border-l border-white/10 ml-4 md:ml-6 space-y-8">
          {logs.map((log) => (
            <div key={log.id} className="relative pl-8 md:pl-10">
              
              <span className="absolute -left-[20px] top-1 rounded-full bg-[#0d0a08] p-2 border border-white/10 shadow-lg">
                <log.Icon size={18} className={log.icon_color} />
              </span>
              
              <div className="bg-white/5 border border-white/5 backdrop-blur-md rounded-2xl p-4 transition-all hover:bg-white/10">
                <div className="flex flex-col md:flex-row md:justify-between md:items-start gap-2 mb-2">
                  <div className="flex items-center gap-3">
                    <h3 className={`text-lg font-medium ${log.icon_color}`}>
                      {log.title}
                    </h3>
                    <span className="text-[10px] font-medium tracking-wide uppercase px-2 py-0.5 rounded-md bg-white/5 border border-white/10 text-white/50">
                      Węzeł #{log.node_id}
                    </span>
                  </div>
                  
                  <div className="flex items-center gap-2 text-xs font-medium text-white/40 bg-black/20 px-3 py-1 rounded-full w-fit">
                    <span>{log.day}</span>
                    <span className="w-1 h-1 rounded-full bg-white/20"></span>
                    <span>{log.hour}</span>
                  </div>
                </div>
                <p className="text-white/60 font-light text-sm leading-relaxed">
                  {log.description}
                </p>
              </div>

            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default History;