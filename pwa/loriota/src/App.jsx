import React, { useState, useEffect } from 'react';
import { 
  DoorOpen, 
  DoorClosed, 
  Mailbox, 
  Battery, 
  BatteryWarning,
  Wifi, 
  Activity, 
  Mail,
  CheckCircle2,
  AlertOctagon,
  Bell,
  Shield,
  ShieldAlert
} from 'lucide-react';

const App = () => {
  const [status, setStatus] = useState('connecting');
  const [time, setTime] = useState(new Date());
  
  const [nodes, setNodes] = useState({
    door: {
      id: 'front_door',
      name: 'Drzwi Wejściowe',
      is_open: false,
      battery: 88,
      rssi: -62,
      last_update: new Date().toLocaleTimeString()
    },
    mailbox: {
      id: 'mailbox',
      name: 'Skrzynka na listy',
      new_letter: false,
      vibration: false,
      battery: 45,
      rssi: -78,
      last_update: new Date().toLocaleTimeString()
    }
  });

  const [location, setLocation] = useState('home');

  useEffect(() => {
    const interval = setInterval(() => setTime(new Date()), 1000);
    const timeout = setTimeout(() => setStatus('connected'), 2000);

    const mock = setInterval(() => {
      setNodes(prev => {
        const new_state = { ...prev };
        const rand_action = Math.random();
        const now = new Date().toLocaleTimeString();

        if (rand_action < 0.2) {
          new_state.door = { ...new_state.door, is_open: !new_state.door.is_open, last_update: now };
        } else if (rand_action >= 0.2 && rand_action < 0.4) {
          new_state.mailbox = { ...new_state.mailbox, new_letter: !new_state.mailbox.new_letter, last_update: now };
        } else if (rand_action >= 0.4 && rand_action < 0.6) {
          new_state.mailbox = { ...new_state.mailbox, vibration: true, last_update: now };
          
          setTimeout(() => {
            setNodes(curr => ({
              ...curr,
              mailbox: { ...curr.mailbox, vibration: false, last_update: new Date().toLocaleTimeString() }
            }));
          }, 2500);
        }
        
        return new_state;
      });
    }, 4000);

    return () => {
      clearInterval(interval);
      clearTimeout(timeout);
      clearInterval(mock);
    };
  }, []);

  const battery_state = (level) => {
    let colorClass = 'text-[#4dc4c9]';
    let Icon = Battery;
    if (level <= 20) { colorClass = 'text-red-400'; Icon = BatteryWarning; } 
    else if (level <= 50) { colorClass = 'text-[#d9652b]'; }

    return (
      <div className="flex items-center space-x-1 text-[10px] font-medium bg-black/20 px-2 py-1 rounded-full backdrop-blur-md border border-white/5">
        <Icon size={12} className={colorClass} />
        <span className="text-white/70">{level}%</span>
      </div>
    );
  };

  const signal_power = (rssi) => {
    let colorClass = 'text-[#4dc4c9]';
    if (rssi < -85) colorClass = 'text-red-400';
    else if (rssi < -70) colorClass = 'text-[#d9652b]';

    return (
      <div className="flex items-center space-x-1 text-[10px] font-medium bg-black/20 px-2 py-1 rounded-full backdrop-blur-md border border-white/5">
        <Wifi size={12} className={colorClass} />
        <span className="text-white/70">{rssi} dBm</span>
      </div>
    );
  };

  return (
    <>
      <style dangerouslySetInnerHTML={{__html: `
        @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@200;300;400;500;600&display=swap');
        .font-custom { font-family: 'Outfit', sans-serif; }
        @keyframes blob1 {
          0% { transform: translate(0px, 0px) scale(1); }
          33% { transform: translate(5vw, -5vh) scale(1.1); }
          66% { transform: translate(-3vw, 3vh) scale(0.9); }
          100% { transform: translate(0px, 0px) scale(1); }
        }
        @keyframes blob2 {
          0% { transform: translate(0px, 0px) scale(1); }
          33% { transform: translate(-5vw, 3vh) scale(0.9); }
          66% { transform: translate(3vw, -3vh) scale(1.1); }
          100% { transform: translate(0px, 0px) scale(1); }
        }
        .animate-blob1 { animation: blob1 20s infinite alternate ease-in-out; }
        .animate-blob2 { animation: blob2 25s infinite alternate-reverse ease-in-out; }
      `}} />

      <div className="min-h-screen w-full bg-[#0d0a08] text-slate-200 font-custom overflow-x-hidden relative flex flex-col items-center">
        
        {/*background*/}
        <div className="fixed inset-0 pointer-events-none z-0 overflow-hidden">
          <div className="absolute inset-0 bg-[#0d0a08]"></div>
          <div className="absolute -top-[10%] -left-[10%] w-[70vw] h-[50vh] bg-[#2b8a8e] rounded-full mix-blend-screen filter blur-[100px] opacity-30 animate-blob1"></div>
          <div className="absolute top-[20%] -right-[20%] w-[80vw] h-[70vh] bg-[#d9652b] rounded-full mix-blend-screen filter blur-[120px] opacity-40 animate-blob2"></div>
          <div className="absolute -bottom-[10%] left-[10%] w-[60vw] h-[50vh] bg-[#ff8a4c] rounded-full mix-blend-screen filter blur-[140px] opacity-20 animate-blob1" style={{ animationDelay: '2s' }}></div>
          <div className="absolute inset-0 opacity-[0.03] mix-blend-overlay" style={{ backgroundImage: 'url("data:image/svg+xml,%3Csvg viewBox=%220 0 200 200%22 xmlns=%22http://www.w3.org/2000/svg%22%3E%3Cfilter id=%22noiseFilter%22%3E%3CfeTurbulence type=%22fractalNoise%22 baseFrequency=%220.65%22 numOctaves=%223%22 stitchTiles=%22stitch%22/%3E%3C/filter%3E%3Crect width=%22100%25%22 height=%22100%25%22 filter=%22url(%23noiseFilter)%22/%3E%3C/svg%3E")' }}></div>
        </div>

        {/*app*/}
        <div className="relative z-10 w-full max-w-7xl px-4 md:px-8 py-6 md:py-12 flex flex-col min-h-screen">
          
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
            
            <button className="w-12 h-12 rounded-full bg-white/5 border border-white/10 flex items-center justify-center backdrop-blur-xl hover:bg-white/10 transition-colors shadow-lg">
              <Bell size={20} className="text-white/80" />
            </button>
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

              {/* location mode buttons */}
              <div className="grid grid-cols-2 gap-4 w-full md:w-auto md:min-w-[320px]">
                <button 
                  onClick={() => setLocation('home')}
                  className={`relative overflow-hidden rounded-3xl p-[1px] transition-all duration-300 w-full ${
                    location === 'home' ? 'shadow-[0_0_20px_rgba(77,196,201,0.2)]' : ''
                  }`}
                >
                  {location === 'home' && (
                    <div className="absolute inset-0 bg-gradient-to-r from-[#4dc4c9] to-[#2b8a8e] opacity-50 blur-sm"></div>
                  )}
                  <div className={`relative z-10 flex flex-col items-center justify-center gap-2 py-4 rounded-[23px] backdrop-blur-xl border ${
                    location === 'home' 
                      ? 'bg-[#121919]/80 border-transparent text-white' 
                      : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10'
                  }`}>
                    <Shield size={24} className={location === 'home' ? 'text-[#4dc4c9]' : ''} />
                    <span className="font-medium text-sm">W domu</span>
                  </div>
                </button>

                <button 
                  onClick={() => setLocation('away')}
                  className={`relative overflow-hidden rounded-3xl p-[1px] transition-all duration-300 w-full ${
                    location === 'away' ? 'shadow-[0_0_20px_rgba(217,101,43,0.2)]' : ''
                  }`}
                >
                  {location === 'away' && (
                    <div className="absolute inset-0 bg-gradient-to-r from-[#d9652b] to-[#a34b20] opacity-50 blur-sm"></div>
                  )}
                  <div className={`relative z-10 flex flex-col items-center justify-center gap-2 py-4 rounded-[23px] backdrop-blur-xl border ${
                    location === 'away' 
                      ? 'bg-[#1c120e]/80 border-transparent text-white' 
                      : 'bg-white/5 border-white/10 text-white/50 hover:bg-white/10'
                  }`}>
                    <ShieldAlert size={24} className={location === 'away' ? 'text-[#d9652b]' : ''} />
                    <span className="font-medium text-sm">Poza domem</span>
                  </div>
                </button>
              </div>
            </section>

            {/* node wigets */}
            <section className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 w-full">
              
              {/* front door */}
              {location === 'home' && (
                <div 
                  className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${
                    nodes.door.is_open 
                      ? 'bg-gradient-to-r from-[#d9652b]/60 to-red-500/60 shadow-[0_0_30px_rgba(217,101,43,0.2)] scale-[1.02]' 
                      : 'bg-white/5'
                  }`}
                >
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${
                     nodes.door.is_open ? 'bg-[#1c120e]/80' : 'bg-[#151210]/60'
                  }`}>
                    {nodes.door.is_open && (
                      <div className="absolute right-0 top-0 w-40 h-40 bg-[#d9652b]/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>
                    )}

                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border shadow-inner transition-colors duration-300 ${
                          nodes.door.is_open 
                            ? 'bg-[#d9652b]/20 border-[#d9652b]/30 text-[#d9652b]' 
                            : 'bg-black/30 border-white/5 text-white/40'
                        }`}>
                          {nodes.door.is_open ? <DoorOpen size={24} /> : <DoorClosed size={24} />}
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{nodes.door.name}</h3>
                          <div className="flex items-center space-x-1.5 mt-1">
                            {nodes.door.is_open ? (
                              <span className="text-sm font-medium text-[#d9652b] flex items-center gap-1.5 whitespace-nowrap">
                                <AlertOctagon size={14} /> Otwarte
                              </span>
                            ) : (
                              <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap">
                                <CheckCircle2 size={14} className="text-[#4dc4c9]" /> Zabezpieczone
                              </span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>

                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {battery_state(nodes.door.battery)}
                        {signal_power(nodes.door.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{nodes.door.last_update}</span>
                    </div>
                  </div>
                </div>
              )}

              {/* mailbox */}
              {location === 'away' && (
                <div 
                  className={`relative overflow-hidden rounded-[32px] p-[1px] transition-all duration-500 w-full animate-in fade-in zoom-in-95 ${
                    nodes.mailbox.vibration 
                      ? 'bg-gradient-to-r from-amber-400/60 to-[#d9652b]/60 scale-[1.02] shadow-[0_0_30px_rgba(245,158,11,0.2)]' 
                      : nodes.mailbox.new_letter 
                        ? 'bg-gradient-to-r from-[#4dc4c9]/50 to-blue-500/50' 
                        : 'bg-white/5'
                  }`}
                >
                  <div className={`relative z-10 rounded-[31px] p-6 h-full backdrop-blur-2xl flex flex-col justify-between min-h-[160px] ${
                     nodes.mailbox.vibration ? 'bg-[#1c160e]/80' : 'bg-[#151210]/60'
                  }`}>
                    {nodes.mailbox.vibration && (
                      <div className="absolute right-0 top-0 w-40 h-40 bg-amber-500/20 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>
                    )}
                    {nodes.mailbox.new_letter && !nodes.mailbox.vibration && (
                      <div className="absolute right-0 top-0 w-40 h-40 bg-[#4dc4c9]/10 rounded-full blur-3xl translate-x-1/4 -translate-y-1/4 pointer-events-none"></div>
                    )}

                    <div className="flex justify-between items-center mb-6 relative z-10">
                      <div className="flex items-center space-x-4 min-w-0">
                        <div className={`shrink-0 w-14 h-14 rounded-full flex items-center justify-center border relative shadow-inner transition-colors duration-300 ${
                          nodes.mailbox.vibration 
                            ? 'bg-amber-500/20 border-amber-500/30 text-amber-400' 
                            : nodes.mailbox.new_letter 
                              ? 'bg-[#4dc4c9]/20 border-[#4dc4c9]/30 text-[#4dc4c9]' 
                              : 'bg-black/30 border-white/5 text-white/40'
                        }`}>
                          <Mailbox size={24} className={nodes.mailbox.vibration ? 'animate-bounce' : ''} />
                          {nodes.mailbox.new_letter && !nodes.mailbox.vibration && (
                             <div className="absolute -top-1 -right-1 flex h-5 w-5 items-center justify-center rounded-full bg-[#4dc4c9] text-[#0d0a08] font-bold border-2 border-[#151210] text-[10px]">
                               1
                             </div>
                          )}
                        </div>
                        <div className="min-w-0">
                          <h3 className="text-lg font-light text-white truncate">{nodes.mailbox.name}</h3>
                          <div className="flex flex-col space-y-1 mt-1">
                            {nodes.mailbox.new_letter ? (
                               <span className="text-sm font-medium text-[#4dc4c9] flex items-center gap-1.5 whitespace-nowrap">
                                  <Mail size={14} /> Nowa poczta
                               </span>
                            ) : (
                               <span className="text-sm font-light text-white/50 flex items-center gap-1.5 whitespace-nowrap">
                                  Pusta
                               </span>
                            )}
                            {nodes.mailbox.vibration && (
                              <span className="text-sm font-medium text-amber-400 flex items-center gap-1.5 whitespace-nowrap">
                                <Activity size={14} /> Wykryto ruch!
                              </span>
                            )}
                          </div>
                        </div>
                      </div>
                    </div>

                    <div className="flex items-center justify-between relative z-10 mt-auto">
                      <div className="flex space-x-2">
                        {battery_state(nodes.mailbox.battery)}
                        {signal_power(nodes.mailbox.rssi)}
                      </div>
                      <span className="text-xs text-white/30 font-light">{nodes.mailbox.last_update}</span>
                    </div>
                  </div>
                </div>
              )}

            </section>
          </main>
        </div>
      </div>
    </>
  );
};

export default App;