import React, { useState, useEffect } from 'react';
import { collection, onSnapshot } from 'firebase/firestore';
import { db } from '../firebaseConfig';
import { 
  DoorOpen, DoorClosed, Mailbox, Battery, BatteryWarning,
  Wifi, Activity, CheckCircle2, AlertOctagon,
  Bell, Shield, ShieldAlert, Layers, Unlock, Lock,
  Layout, LayoutTemplate, Settings as SettingsIcon
} from 'lucide-react';

const Dashboard = () => {
  const [status, setStatus] = useState('connecting');
  const [time, setTime] = useState(new Date());
  
  const [nodes, setNodes] = useState({});
  const [location, setLocation] = useState('all'); 

  useEffect(() => {
    const interval = setInterval(() => setTime(new Date()), 1000);
    const timeout = setTimeout(() => setStatus('connected'), 2000);

    const unsub = onSnapshot(collection(db, "sensors"), (snapshot) => {
      const live_nodes = {};
      
      snapshot.forEach((doc_snap) => {
        const data = doc_snap.data();
        const node_id = doc_snap.id; 
        
        let formatted_time = "Brak danych";
        if (data.timestamp) {
          try {
            const date_obj = data.timestamp.toDate ? data.timestamp.toDate() : new Date(data.timestamp);
            formatted_time = date_obj.toLocaleTimeString('pl-PL', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
          } catch(e) {
            formatted_time = "Błąd czasu";
          }
        }

        live_nodes[node_id] = {
          id: node_id,
          name: data.custom_name || `Czujnik #${node_id}`, 
          type: Number(data.node_type), 
          state: data.sensor_state,
          battery: data.battery_lvl || 0,
          rssi: Math.round(data.rssi) || 0,
          last_update: formatted_time,
          location_tab: data.location_tab || 'unassigned',
          acc_x: data.acc_x,
          acc_y: data.acc_y,
          acc_z: data.acc_z,
        };
      });
      
      setNodes(live_nodes);
    }, (error) => {
        console.error("Błąd połączenia z bazą:", error);
    });

    return () => {
      clearInterval(interval);
      clearTimeout(timeout);
      unsub(); 
    };
  }, []);

  const batteryState = (level) => {
    let color_class = 'text-[#4dc4c9]';
    let StatusIcon = Battery;
    if (level <= 20) { color_class = 'text-red-400'; StatusIcon = BatteryWarning; } 
    else if (level <= 50) { color_class = 'text-[#d9652b]'; }

    return (
      <div className="flex items-center space-x-1 text-[10px] font-medium bg-black/20 px-2 py-1 rounded-full backdrop-blur-md border border-white/5">
        <StatusIcon size={12} className={color_class} />
        <span className="text-white/70">{level}%</span>
      </div>
    );
  };

  const signalPower = (rssi) => {
    let color_class = 'text-[#4dc4c9]';
    if (rssi < -85) color_class = 'text-red-400';
    else if (rssi < -70) color_class = 'text-[#d9652b]';

    return (
      <div className="flex items-center space-x-1 text-[10px] font-medium bg-black/20 px-2 py-1 rounded-full backdrop-blur-md border border-white/5">
        <Wifi size={12} className={color_class} />
        <span className="text-white/70">{rssi} dBm</span>
      </div>
    );
  };

  const unassigned_count = Object.values(nodes).filter(n => n.location_tab === 'unassigned').length;

  return (
    <div className="relative z-10 w-full px-4 md:px-8 py-6 md:py-12 flex flex-col min-h-screen">
      
      <header className="flex justify-between items-center mb-8 md:mb-16">
        <h1 className="text-2xl md:text-3xl font-light tracking-tight text-white flex items-center gap-3">
          loriota
          {status === 'connected' ? (
             <span className="relative flex h-2.5 w-2.5">
               <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-[#4dc4c9] opacity-75"></span>
               <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-[#4dc4c9] shadow-[0_0_8px_rgba(77,196,201,0.8)]"></span>
             </span>
          ) : (
            <span className="relative flex h-2.5 w-2.5">
               <span className="relative inline-flex rounded-full h-2.5 w-2.5 bg-[#d9652b]"></span>
             </span>
          )}
        </h1>
        
        <div className="flex items-center gap-3">
          <button className="relative w-12 h-12 rounded-full bg-white/5 border border-white/10 flex items-center justify-center backdrop-blur-xl hover:bg-white/10 transition-colors shadow-lg">
            <SettingsIcon size={20} className="text-white/80" />
            {unassigned_count > 0 && (
              <span className="absolute top-0 right-0 w-3 h-3 bg-red-500 border border-[#151210] rounded-full animate-pulse shadow-[0_0_8px_rgba(239,68,68,0.8)]"></span>
            )}
          </button>

          <button className="w-12 h-12 rounded-full bg-white/5 border border-white/10 flex items-center justify-center backdrop-blur-xl hover:bg-white/10 transition-colors shadow-lg">
            <Bell size={20} className="text-white/80" />
          </button>
        </div>
      </header>

      <main className="flex-1 w-full flex flex-col gap-10">
        
        <section className="flex flex-col md:flex-row items-start md:items-end justify-between gap-8">
          <div>
            <div className="text-6xl md:text-[7rem] leading-none font-light tracking-tighter text-white drop-shadow-[0_0_15px_rgba(255,255,255,0.1)]">
              {time.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
            </div>
            <div className="text-sm md:text-base font-light text-white/50 mt-4 uppercase tracking-[0.2em] ml-1">
              {time.toLocaleDateString('pl-PL', { weekday: 'long', month: 'long', day: 'numeric' })}
            </div>
          </div>

          <div className="grid grid-cols-3 gap-2 md:gap-4 w-full md:w-auto md:min-w-[420px]">
            <button onClick={() => setLocation('all')} className={`relative overflow-hidden rounded-3xl p-[1px] transition-all duration-300 w-full ${location === 'all' ? 'shadow-[0_0_20px_rgba(255,255,255,0.1)]' : ''}`}>
              {location === 'all' && <div className="absolute inset-0 bg-gradient-to-r from-slate-400 to-slate-600 opacity-30 blur-sm"></div>}
              <div className={`relative z-10 flex flex-col items-center justify-center gap-1 md:gap-2 py-3 md:py-4 rounded-[23px] backdrop-blur-xl border ${location === 'all' ? 'bg-[#1a1a1a]/80 border-transparent text-white' : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10'}`}>
                <Layers size={24} className={location === 'all' ? 'text-slate-300' : ''} />
                <span className="font-medium text-xs md:text-sm">Wszystkie</span>
              </div>
            </button>
            <button onClick={() => setLocation('home')} className={`relative overflow-hidden rounded-3xl p-[1px] transition-all duration-300 w-full ${location === 'home' ? 'shadow-[0_0_20px_rgba(77,196,201,0.2)]' : ''}`}>
              {location === 'home' && <div className="absolute inset-0 bg-gradient-to-r from-[#4dc4c9] to-[#2b8a8e] opacity-50 blur-sm"></div>}
              <div className={`relative z-10 flex flex-col items-center justify-center gap-1 md:gap-2 py-3 md:py-4 rounded-[23px] backdrop-blur-xl border ${location === 'home' ? 'bg-[#121919]/80 border-transparent text-white' : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10'}`}>
                <Shield size={24} className={location === 'home' ? 'text-[#4dc4c9]' : ''} />
                <span className="font-medium text-xs md:text-sm">W domu</span>
              </div>
            </button>
            <button onClick={() => setLocation('away')} className={`relative overflow-hidden rounded-3xl p-[1px] transition-all duration-300 w-full ${location === 'away' ? 'shadow-[0_0_20px_rgba(217,101,43,0.2)]' : ''}`}>
              {location === 'away' && <div className="absolute inset-0 bg-gradient-to-r from-[#d9652b] to-[#a34b20] opacity-50 blur-sm"></div>}
              <div className={`relative z-10 flex flex-col items-center justify-center gap-1 md:gap-2 py-3 md:py-4 rounded-[23px] backdrop-blur-xl border ${location === 'away' ? 'bg-[#1c120e]/80 border-transparent text-white' : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10'}`}>
                <ShieldAlert size={24} className={location === 'away' ? 'text-[#d9652b]' : ''} />
                <span className="font-medium text-xs md:text-sm">Na zewnątrz</span>
              </div>
            </button>
          </div>
        </section>

        <section className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 w-full">
          
          {Object.values(nodes)
            .filter(node => {
              if (location === 'all') return true;
              return node.location_tab === location;
            })
            .map(node => {
            
            const is_open = node.state === 1;

            if (node.type === 0) {
              const has_vibration = node.state === 1;
              const has_new_letter = false; 
              
              return (
                <div key={node.id} className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${has_vibration ? 'bg-gradient-to-r from-amber-400/60 to-[#d9652b]/60 scale-[1.02] shadow-[0_0_30px_rgba(245,158,11,0.2)]' : has_new_letter ? 'bg-gradient-to-r from-[#4dc4c9]/50 to-blue-500/50' : 'bg-white/5'}`}>
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${has_vibration ? 'bg-[#1c160e]/80' : 'bg-[#151210]/60'}`}>
                    {has_vibration && <div className="absolute right-0 top-0 w-40 h-40 bg-amber-500/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>}

                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border relative shadow-inner transition-colors duration-300 ${has_vibration ? 'bg-amber-500/20 border-amber-500/30 text-amber-400' : has_new_letter ? 'bg-[#4dc4c9]/20 border-[#4dc4c9]/30 text-[#4dc4c9]' : 'bg-black/30 border-white/5 text-white/40'}`}>
                          <Mailbox size={24} className={has_vibration ? 'animate-bounce' : ''} />
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{node.name}</h3>
                          <div className="flex flex-col space-y-1 mt-1">
                            {has_vibration ? (
                              <span className="text-sm font-medium text-amber-400 flex items-center gap-1.5 whitespace-nowrap"><Activity size={14} /> Wykryto ruch!</span>
                            ) : (
                              <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap">Czuwanie</span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>

                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {batteryState(node.battery)}
                        {signalPower(node.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{node.last_update}</span>
                    </div>
                  </div>
                </div>
              );
            }

            if (node.type === 1) {
              return (
                <div key={node.id} className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${is_open ? 'bg-gradient-to-r from-[#d9652b]/60 to-red-500/60 shadow-[0_0_30px_rgba(217,101,43,0.2)] scale-[1.02]' : 'bg-white/5'}`}>
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${is_open ? 'bg-[#1c120e]/80' : 'bg-[#151210]/60'}`}>
                    {is_open && <div className="absolute right-0 top-0 w-40 h-40 bg-[#d9652b]/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>}
                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border shadow-inner transition-colors duration-300 ${is_open ? 'bg-[#d9652b]/20 border-[#d9652b]/30 text-[#d9652b]' : 'bg-black/30 border-white/5 text-white/40'}`}>
                          {is_open ? <DoorOpen size={24} /> : <DoorClosed size={24} />}
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{node.name}</h3>
                          <div className="flex items-center space-x-1.5 mt-1">
                            {is_open ? (
                              <span className="text-sm font-medium text-[#d9652b] flex items-center gap-1.5 whitespace-nowrap"><AlertOctagon size={14} /> Otwarte</span>
                            ) : (
                              <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap"><CheckCircle2 size={14} className="text-[#4dc4c9]" /> Zabezpieczone</span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {batteryState(node.battery)}
                        {signalPower(node.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{node.last_update}</span>
                    </div>
                  </div>
                </div>
              );
            }

            if (node.type === 2) {
              return (
                <div key={node.id} className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${is_open ? 'bg-gradient-to-r from-blue-400/60 to-purple-500/60 shadow-[0_0_30px_rgba(59,130,246,0.2)] scale-[1.02]' : 'bg-white/5'}`}>
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${is_open ? 'bg-[#0f172a]/80' : 'bg-[#151210]/60'}`}>
                    {is_open && <div className="absolute right-0 top-0 w-40 h-40 bg-blue-500/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>}
                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border shadow-inner transition-colors duration-300 ${is_open ? 'bg-blue-500/20 border-blue-500/30 text-blue-400' : 'bg-black/30 border-white/5 text-white/40'}`}>
                          {is_open ? <Unlock size={24} /> : <Lock size={24} />}
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{node.name}</h3>
                          <div className="flex items-center space-x-1.5 mt-1">
                            {is_open ? (
                              <span className="text-sm font-medium text-blue-400 flex items-center gap-1.5 whitespace-nowrap"><AlertOctagon size={14} /> Otwarte</span>
                            ) : (
                              <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap"><CheckCircle2 size={14} className="text-[#4dc4c9]" /> Zabezpieczone</span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {batteryState(node.battery)}
                        {signalPower(node.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{node.last_update}</span>
                    </div>
                  </div>
                </div>
              );
            }

            if (node.type === 3) {
              return (
                <div key={node.id} className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${is_open ? 'bg-gradient-to-r from-orange-400/60 to-red-500/60 shadow-[0_0_30px_rgba(249,115,22,0.2)] scale-[1.02]' : 'bg-white/5'}`}>
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${is_open ? 'bg-[#2a130c]/80' : 'bg-[#151210]/60'}`}>
                    {is_open && <div className="absolute right-0 top-0 w-40 h-40 bg-orange-500/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>}
                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border shadow-inner transition-colors duration-300 ${is_open ? 'bg-orange-500/20 border-orange-500/30 text-orange-400' : 'bg-black/30 border-white/5 text-white/40'}`}>
                          {is_open ? <Layout size={24} /> : <LayoutTemplate size={24} />}
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{node.name}</h3>
                          <div className="flex items-center space-x-1.5 mt-1">
                            {is_open ? (
                              <span className="text-sm font-medium text-orange-400 flex items-center gap-1.5 whitespace-nowrap"><AlertOctagon size={14} /> Otwarta</span>
                            ) : (
                              <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap"><CheckCircle2 size={14} className="text-[#4dc4c9]" /> Zabezpieczona</span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {batteryState(node.battery)}
                        {signalPower(node.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{node.last_update}</span>
                    </div>
                  </div>
                </div>
              );
            }

            return (
              <div key={node.id} className="relative overflow-hidden rounded-[32px] p-[1px] bg-white/5 opacity-50 w-full animate-in fade-in">
                <div className="relative z-10 rounded-[31px] p-6 h-full bg-[#151210]/60 backdrop-blur-2xl flex flex-col justify-center">
                  <h3 className="text-lg font-medium text-red-400 mb-2">Błąd typu urządzenia</h3>
                  <div className="text-sm font-light text-white/50 space-y-1">
                    <p>Węzeł: #{node.id}</p>
                    <p>Typ odebrany z bazy: "{node.type}"</p>
                  </div>
                </div>
              </div>
            );
          })}

        </section>
      </main>
    </div>
  );
};

export default Dashboard;