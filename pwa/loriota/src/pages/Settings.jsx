import React, { useState, useEffect } from 'react';
import { collection, onSnapshot, doc, updateDoc, deleteDoc } from 'firebase/firestore';
import { db } from '../firebaseConfig';
import { 
  Settings as SettingsIcon, 
  Save, 
  Bell, 
  Smartphone, 
  Mail, 
  Shield, 
  ShieldAlert,
  Cpu,
  Trash2
} from 'lucide-react';

const Settings = () => {
  const [nodes, setNodes] = useState([]);
  const [loading, setLoading] = useState(true);
  const [saving_id, setSavingId] = useState(null); 
  const [deleting_id, setDeletingId] = useState(null); 

  const [form_data, setFormData] = useState({});

  useEffect(() => {
    const unsub = onSnapshot(collection(db, "sensors"), (snapshot) => {
      const nodes_data = [];
      const initial_form_state = {};

      snapshot.forEach((docSnap) => {
        const data = docSnap.data();
        const node_id = docSnap.id;

        nodes_data.push({
          id: node_id,
          type: data.node_type,
          custom_name: data.custom_name || '', 
          location_tab: data.location_tab || 'unassigned' 
        });

        initial_form_state[node_id] = {
          custom_name: data.custom_name || '',
          location_tab: data.location_tab || 'unassigned'
        };
      });

      setNodes(nodes_data);
      setFormData(prev => Object.keys(prev).length === 0 ? initial_form_state : prev);
      setLoading(false);
    });

    return () => unsub();
  }, []);

  const handleInputChange = (node_id, field, value) => {
    setFormData(prev => ({
      ...prev,
      [node_id]: {
        ...prev[node_id],
        [field]: value
      }
    }));
  };

  const handleSaveNode = async (node_id) => {
    setSavingId(node_id);
    try {
      const node_ref = doc(db, "sensors", node_id);
      await updateDoc(node_ref, {
        custom_name: form_data[node_id].custom_name,
        location_tab: form_data[node_id].location_tab
      });
      setTimeout(() => setSavingId(null), 500); 
    } catch (error) {
      console.error("Błąd podczas zapisywania ustawień:", error);
      setSavingId(null);
    }
  };

  const handleDeleteNode = async (node_id) => {
    const is_confirmed = window.confirm(`Czy na pewno chcesz usunąć Węzeł #${node_id} z systemu?`);
    if (!is_confirmed) return;

    setDeletingId(node_id);
    try {
      await deleteDoc(doc(db, "sensors", node_id));
    } catch (error) {
      console.error("Błąd podczas usuwania węzła:", error);
    } finally {
      setDeletingId(null);
    }
  };

  return (
    <div className="p-4 md:p-8 animate-in fade-in duration-500 w-full max-w-3xl mx-auto mb-20 font-custom text-slate-200">
      
      <header className="flex items-center gap-4 mb-12 border-b border-white/5 pb-6">
        <div className="w-12 h-12 rounded-full bg-slate-500/10 flex items-center justify-center border border-slate-500/20">
          <SettingsIcon className="text-slate-300" size={24} />
        </div>
        <div>
          <h1 className="text-2xl md:text-3xl font-light text-white tracking-tight">Ustawienia systemu</h1>
          <p className="text-white/40 font-light text-sm mt-1">Konfiguracja węzłów IoT i powiadomień</p>
        </div>
      </header>

      <section className="mb-12">
        <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
          <Cpu size={20} className="text-[#4dc4c9]" /> Moje Urządzenia
        </h2>

        {loading ? (
          <div className="text-white/50 text-sm">Ładowanie urządzeń...</div>
        ) : nodes.length === 0 ? (
          <div className="text-white/40 text-sm">Brak podłączonych urządzeń w bazie.</div>
        ) : (
          <div className="space-y-4">
            {nodes.map(node => (
              <div key={node.id} className="bg-white/5 border border-white/10 rounded-2xl p-5 backdrop-blur-md transition-all hover:bg-white/10">
                
                <div className="flex justify-between items-center mb-4 border-b border-white/5 pb-4">
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-mono bg-black/30 px-2 py-1 rounded-md text-white/50 border border-white/5">
                      ID: {node.id}
                    </span>
                    <span className="text-xs text-white/30 uppercase tracking-wider">
                      (Typ sprzętowy: {node.type})
                    </span>
                  </div>
                  
                  <div className="flex items-center gap-2">
                    <button 
                      onClick={() => handleDeleteNode(node.id)}
                      disabled={deleting_id === node.id}
                      className="flex items-center justify-center p-2 bg-red-500/10 hover:bg-red-500/20 text-red-400 rounded-xl transition-colors border border-red-500/20"
                      title="Usuń to urządzenie"
                    >
                      <Trash2 size={16} className={deleting_id === node.id ? "animate-pulse" : ""} />
                    </button>

                    <button 
                      onClick={() => handleSaveNode(node.id)}
                      disabled={saving_id === node.id}
                      className="flex items-center gap-2 bg-[#4dc4c9]/20 hover:bg-[#4dc4c9]/30 text-[#4dc4c9] px-4 py-2 rounded-xl text-sm transition-colors border border-[#4dc4c9]/20"
                    >
                      {saving_id === node.id ? (
                        <span className="animate-pulse">Zapisywanie...</span>
                      ) : (
                        <><Save size={16} /> Zapisz</>
                      )}
                    </button>
                  </div>
                </div>

                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div className="flex flex-col gap-2">
                    <label className="text-xs text-white/50 uppercase tracking-wide ml-1">Nazwa własna</label>
                    <input 
                      type="text" 
                      placeholder={`np. Węzeł #${node.id}`}
                      value={form_data[node.id]?.custom_name || ''}
                      onChange={(e) => handleInputChange(node.id, 'custom_name', e.target.value)}
                      className="bg-black/20 border border-white/10 rounded-xl px-4 py-2.5 text-white outline-none focus:border-[#4dc4c9]/50 transition-colors placeholder:text-white/20 text-sm"
                    />
                  </div>

                  <div className="flex flex-col gap-2">
                    <div className="flex justify-between items-end">
                       <label className="text-xs text-white/50 uppercase tracking-wide ml-1">Strefa (Zakładka)</label>
                       {form_data[node.id]?.location_tab === 'unassigned' && (
                          <span className="text-[10px] text-red-400 font-medium animate-pulse">Wymaga przypisania!</span>
                       )}
                    </div>
                    <div className="flex bg-black/20 rounded-xl p-1 border border-white/10">
                      <button 
                        onClick={() => handleInputChange(node.id, 'location_tab', 'home')}
                        className={`flex-1 flex items-center justify-center gap-2 py-1.5 rounded-lg text-sm transition-all ${form_data[node.id]?.location_tab === 'home' ? 'bg-white/10 text-white shadow-sm' : 'text-white/40 hover:text-white/70'}`}
                      >
                        <Shield size={14} /> W domu
                      </button>
                      <button 
                        onClick={() => handleInputChange(node.id, 'location_tab', 'away')}
                        className={`flex-1 flex items-center justify-center gap-2 py-1.5 rounded-lg text-sm transition-all ${form_data[node.id]?.location_tab === 'away' ? 'bg-white/10 text-white shadow-sm' : 'text-white/40 hover:text-white/70'}`}
                      >
                        <ShieldAlert size={14} /> Na zewnątrz
                      </button>
                    </div>
                  </div>
                </div>

              </div>
            ))}
          </div>
        )}
      </section>

      <section>
        <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
          <Bell size={20} className="text-[#d9652b]" /> Powiadomienia o alarmach
        </h2>
        
        <div className="bg-white/5 border border-white/10 rounded-2xl p-2 backdrop-blur-md">
          <div className="flex items-center justify-between p-4 border-b border-white/5">
            <div className="flex items-center gap-3">
              <div className="w-10 h-10 rounded-full bg-blue-500/10 flex items-center justify-center text-blue-400">
                <Smartphone size={20} />
              </div>
              <div>
                <h3 className="text-sm font-medium text-white">Aplikacja (PWA Push)</h3>
                <p className="text-xs text-white/40 mt-0.5">Otrzymuj natychmiastowe alerty na telefon</p>
              </div>
            </div>
            <div className="w-12 h-6 rounded-full bg-[#4dc4c9] relative cursor-pointer opacity-50 hover:opacity-100 transition-opacity">
               <div className="absolute right-1 top-1 w-4 h-4 rounded-full bg-white shadow-sm"></div>
            </div>
          </div>
          <div className="flex items-center justify-between p-4">
            <div className="flex items-center gap-3">
              <div className="w-10 h-10 rounded-full bg-orange-500/10 flex items-center justify-center text-orange-400">
                <Mail size={20} />
              </div>
              <div>
                <h3 className="text-sm font-medium text-white">E-mail (Raporty)</h3>
                <p className="text-xs text-white/40 mt-0.5">Dzienne podsumowania aktywności</p>
              </div>
            </div>
            <div className="w-12 h-6 rounded-full bg-white/10 relative cursor-pointer hover:bg-white/20 transition-colors">
               <div className="absolute left-1 top-1 w-4 h-4 rounded-full bg-white/50 shadow-sm"></div>
            </div>
          </div>
        </div>
      </section>
    </div>
  );
};

export default Settings;