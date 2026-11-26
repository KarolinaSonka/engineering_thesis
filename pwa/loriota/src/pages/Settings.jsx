import {
  ref,
  get,
  onValue,
  update,
  remove,
  runTransaction
} from 'firebase/database';

import { rtdb, messaging } from '../firebaseConfig';
import { getToken } from 'firebase/messaging';

import {
  Settings as SettingsIcon,
  Save,
  Bell,
  Smartphone,
  Shield,
  ShieldAlert,
  Cpu,
  Trash2,
  Wifi
} from 'lucide-react';

import { useState, useEffect, useRef } from 'react';

const FCM_VAPID_KEY = 'BDg2d1Lb39u3e0SGQEC7DgJ-C_eeTjQ7ctKDITA956y-FzO2nk9vcpP1RmONa6B_GHEabvfPKHWOjh7PWZJIa2w';

const Settings = () => {
  const [nodes, setNodes] = useState([]);
  const [loading, setLoading] = useState(true);

  const [saving_id, setSavingId] = useState(null);
  const [deleting_id, setDeletingId] = useState(null);

  const [form_data, setFormData] = useState({});
  const [pushEnabled, setPushEnabled] = useState(false);

  const dirtyNodesRef = useRef(new Set());

  useEffect(() => {
    let cancelled = false;

    const checkNotificationStatus = async () => {
      if (Notification.permission !== 'granted') {
        setPushEnabled(false);
        return;
      }

      try {
        const token = await getToken(messaging, {
          vapidKey: FCM_VAPID_KEY
        });

        const snapshot = await get(
          ref(rtdb, "config/device/tokens")
        );

        const tokens = snapshot.val();

        if (!cancelled) {
          setPushEnabled(
            Boolean(token) &&
            Array.isArray(tokens) &&
            tokens.includes(token)
          );
        }

      } catch (error) {
        console.error(
          "[FCM] Failed to check notification status:",
          error
        );

        if (!cancelled) {
          setPushEnabled(false);
        }
      }
    };

    checkNotificationStatus();

    return () => {
      cancelled = true;
    };
  }, []);


  useEffect(() => {
    const unsubscribe = onValue(
      ref(rtdb, "sensors"),

      (snapshot) => {
        const realtimeNodes = snapshot.val() || {};

        const nodesData = Object.entries(realtimeNodes).map(
          ([nodeId, entry]) => {
            const settings = entry?.settings || {};
            const live = entry?.live || {};

            return {
              id: nodeId,

              type: Number(
                live.node_type ??
                settings.node_type ??
                -1
              ),

              custom_name:
                settings.custom_name ?? '',

              location_tab:
                settings.location_tab ?? 'unassigned'
            };
          }
        );

        setNodes(nodesData);

        setFormData((previous) => {
          const updated = {};

          nodesData.forEach((node) => {
            if (
              dirtyNodesRef.current.has(node.id) &&
              previous[node.id]
            ) {
              updated[node.id] = previous[node.id];
            } else {
              updated[node.id] = {
                custom_name: node.custom_name,
                location_tab: node.location_tab
              };
            }
          });

          return updated;
        });

        setLoading(false);
      },

      (error) => {
        console.error(
          "[RTDB] Failed to load sensors:",
          error
        );

        setLoading(false);
      }
    );

    return () => unsubscribe();
  }, []);


  const handleInputChange = (node_id, field, value) => {
    dirtyNodesRef.current.add(node_id);

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
      const settingsRef = ref(
        rtdb,
        `sensors/${node_id}/settings`
      );

      await update(settingsRef, {
        custom_name: form_data[node_id].custom_name,
        location_tab: form_data[node_id].location_tab
      });

      dirtyNodesRef.current.delete(node_id);

      setTimeout(() => {
        setSavingId(null);
      }, 500);

    } catch (error) {
      console.error(
        "[RTDB] Failed to save sensor settings:",
        error
      );

      setSavingId(null);

      alert(
        "Nie udało się zapisać ustawień czujnika."
      );
    }
  };

  const handleDeleteNode = async (node_id) => {
    const confirmed = window.confirm(
      "Czy na pewno chcesz usunąć węzeł #" + node_id +
      " z systemu?\n\n" +
      "Zostaną usunięte jego ustawienia i ostatni odczyt. " +
      "Historia zdarzeń pozostanie w bazie.\n\n" +
      "Jeśli urządzenie później wyśle nową ramkę, " +
      "pojawi się ponownie z nazwą domyślną."
    );

    if (!confirmed) return;

    setDeletingId(node_id);

    try {
      await remove(
        ref(rtdb, "sensors/" + node_id)
      );

      dirtyNodesRef.current.delete(node_id);

    } catch (error) {
      console.error(
        "[RTDB] Failed to delete node:",
        error
      );

      alert(
        "Nie udało się usunąć węzła.\n" +
        error.message
      );

    } finally {
      setDeletingId(null);
    }
  };

  const handlePushToggle = async () => {
    try {
      const isCurrentlyEnabled =
        Notification.permission === 'granted' &&
        pushEnabled;

      if (
        !isCurrentlyEnabled &&
        Notification.permission !== 'granted'
      ) {
        const permission =
          await Notification.requestPermission();

        if (permission !== 'granted') {
          setPushEnabled(false);
          return;
        }
      }

      const currentToken = await getToken(messaging, {
        vapidKey: FCM_VAPID_KEY
      });

      if (!currentToken) {
        throw new Error(
          "Could not obtain FCM token."
        );
      }

      const tokensRef = ref(
        rtdb,
        "config/device/tokens"
      );

      const result = await runTransaction(
        tokensRef,

        (currentTokens) => {
          const tokens = Array.isArray(currentTokens)
            ? currentTokens.filter(
              token => typeof token === 'string'
            )
            : [];

          if (isCurrentlyEnabled) {
            return tokens.filter(
              token => token !== currentToken
            );
          }

          if (!tokens.includes(currentToken)) {
            tokens.push(currentToken);
          }

          return tokens;
        }
      );

      if (!result.committed) {
        throw new Error(
          "RTDB transaction was not committed."
        );
      }

      setPushEnabled(!isCurrentlyEnabled);

      if (isCurrentlyEnabled) {
        alert(
          "Powiadomienia zostały wyłączone dla tego urządzenia."
        );
      } else {
        alert(
          "Powiadomienia pomyślnie aktywowane!"
        );
      }

    } catch (error) {
      console.error(
        "[FCM] Notification settings update failed:",
        error
      );

      alert(
        "Nie udało się zmienić ustawień powiadomień.\n" +
        error.message
      );
    }
  };

  const openGatewaySetup = () => {
    const confirmed = window.confirm(
      'Aby skonfigurować Wi-Fi bramki Loriota:\n\n' +
      '1. Połącz telefon lub komputer z siecią Loriota-Setup.\n' +
      '2. Wpisz hasło: loriota_config.\n' +
      '3. Jeśli urządzenie zgłasza brak Internetu, pozostań w tej sieci.\n' +
      '4. Otwórz lokalny portal konfiguracji.\n\n' +
      'Czy otworzyć stronę konfiguracji?'
    );

    if (confirmed) {
      window.open(
        'http://192.168.4.1/',
        '_blank',
        'noopener,noreferrer'
      );
    }
  };


  return (
    <div className="p-4 md:p-8 animate-in fade-in duration-500 w-full max-w-3xl mx-auto mb-20 font-custom text-slate-200">

      <header className="flex items-center gap-4 mb-12 border-b border-white/5 pb-6">
        <div className="w-12 h-12 rounded-full bg-slate-500/10 flex items-center justify-center border border-slate-500/20">
          <SettingsIcon
            className="text-slate-300"
            size={24}
          />
        </div>

        <div>
          <h1 className="text-2xl md:text-3xl font-light text-white tracking-tight">
            Ustawienia systemu
          </h1>

          <p className="text-white/40 font-light text-sm mt-1">
            Konfiguracja węzłów IoT i powiadomień
          </p>
        </div>
      </header>

      <section className="mb-12">
        <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
          <Wifi size={20} className="text-[#64C9CF]" />
          Bramka Loriota
        </h2>

        <div className="bg-white/5 border border-white/10 rounded-2xl p-5 backdrop-blur-md">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">

            <div className="flex items-start gap-3">
              <div className="w-11 h-11 shrink-0 rounded-xl bg-[#64C9CF]/10 flex items-center justify-center">
                <Wifi
                  size={22}
                  className="text-[#64C9CF]"
                />
              </div>

              <div>
                <h3 className="text-white font-medium">
                  Konfiguracja Wi-Fi
                </h3>

                <p className="text-sm text-white/50 mt-1">
                  Połącz bramkę z domową siecią bez zmiany
                  ustawień w kodzie urządzenia.
                </p>

                <p className="text-xs text-white/35 mt-2">
                  Sieć konfiguracyjna: Loriota-Setup
                </p>

                <p className="text-xs text-white/35 mt-1">
                  Hasło: loriota_config
                </p>

              </div>
            </div>

            <button
              type="button"
              onClick={openGatewaySetup}
              className="shrink-0 flex items-center justify-center gap-2 bg-[#64C9CF]/15 hover:bg-[#64C9CF]/25] text-[#64C9CF] border border-[#64C9CF]/20 rounded-xl px-4 py-3 text-sm transition-colors"
            >
              <Wifi size={16} />
              Konfiguruj Wi-Fi
            </button>

          </div>
        </div>
      </section>

      <section className="mb-12">
        <h2 className="text-lg font-medium text-white mb-6 flex items-center gap-2">
          <Cpu size={20} className="text-[#4dc4c9]" />
          Moje Urządzenia
        </h2>

        {loading ? (
          <div className="text-white/50 text-sm">
            Ładowanie urządzeń...
          </div>

        ) : nodes.length === 0 ? (
          <div className="text-white/40 text-sm">
            Brak podłączonych urządzeń w bazie.
          </div>

        ) : (
          <div className="space-y-4">
            {nodes.map(node => (
              <div
                key={node.id}
                className="bg-white/5 border border-white/10 rounded-2xl p-5 backdrop-blur-md transition-all hover:bg-white/10"
              >

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
                      <Trash2
                        size={16}
                        className={
                          deleting_id === node.id
                            ? "animate-pulse"
                            : ""
                        }
                      />
                    </button>

                    <button
                      onClick={() => handleSaveNode(node.id)}
                      disabled={saving_id === node.id}
                      className="flex items-center gap-2 bg-[#4dc4c9]/20 hover:bg-[#4dc4c9]/30 text-[#4dc4c9] px-4 py-2 rounded-xl text-sm transition-colors border border-[#4dc4c9]/20"
                    >
                      {saving_id === node.id ? (
                        <span className="animate-pulse">
                          Zapisywanie...
                        </span>
                      ) : (
                        <>
                          <Save size={16} />
                          Zapisz
                        </>
                      )}
                    </button>

                  </div>
                </div>


                <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                  <div className="flex flex-col gap-2">
                    <label className="text-xs text-white/50 uppercase tracking-wide ml-1">
                      Nazwa własna
                    </label>

                    <input
                      type="text"
                      placeholder={`np. Węzeł #${node.id}`}
                      value={
                        form_data[node.id]?.custom_name || ''
                      }
                      onChange={(e) =>
                        handleInputChange(
                          node.id,
                          'custom_name',
                          e.target.value
                        )
                      }
                      className="bg-black/20 border border-white/10 rounded-xl px-4 py-2.5 text-white outline-none focus:border-[#4dc4c9]/50 transition-colors placeholder:text-white/20 text-sm"
                    />
                  </div>

                  <div className="flex flex-col gap-2">

                    <div className="flex justify-between items-end">
                      <label className="text-xs text-white/50 uppercase tracking-wide ml-1">
                        Strefa (Zakładka)
                      </label>

                      {form_data[node.id]?.location_tab === 'unassigned' && (
                        <span className="text-[10px] text-red-400 font-medium animate-pulse">
                          Wymaga przypisania!
                        </span>
                      )}
                    </div>

                    <div className="flex bg-black/20 rounded-xl p-1 border border-white/10">

                      <button
                        onClick={() =>
                          handleInputChange(
                            node.id,
                            'location_tab',
                            'home'
                          )
                        }
                        className={`flex-1 flex items-center justify-center gap-2 py-1.5 rounded-lg text-sm transition-all ${form_data[node.id]?.location_tab === 'home'
                            ? 'bg-white/10 text-white shadow-sm'
                            : 'text-white/40 hover:text-white/70'
                          }`}
                      >
                        <Shield size={14} />
                        W domu
                      </button>

                      <button
                        onClick={() =>
                          handleInputChange(
                            node.id,
                            'location_tab',
                            'away'
                          )
                        }
                        className={`flex-1 flex items-center justify-center gap-2 py-1.5 rounded-lg text-sm transition-all ${form_data[node.id]?.location_tab === 'away'
                            ? 'bg-white/10 text-white shadow-sm'
                            : 'text-white/40 hover:text-white/70'
                          }`}
                      >
                        <ShieldAlert size={14} />
                        Na zewnątrz
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
          <Bell size={20} className="text-[#d9652b]" />
          Powiadomienia o alarmach
        </h2>

        <div className="bg-white/5 border border-white/10 rounded-2xl p-2 backdrop-blur-md">

          <div className="flex items-center justify-between p-4">

            <div className="flex items-center gap-3">
              <div className="w-10 h-10 rounded-full bg-blue-500/10 flex items-center justify-center text-blue-400">
                <Smartphone size={20} />
              </div>

              <div>
                <h3 className="text-sm font-medium text-white">
                  Aplikacja (PWA Push)
                </h3>

                <p className="text-xs text-white/40 mt-0.5">
                  Otrzymuj natychmiastowe alerty na telefon
                </p>
              </div>
            </div>

            <div
              onClick={handlePushToggle}
              className={`w-12 h-6 rounded-full relative cursor-pointer transition-colors duration-300 ${pushEnabled
                  ? 'bg-[#4dc4c9]'
                  : 'bg-white/10 hover:bg-white/20'
                }`}
            >
              <div
                className={`absolute top-1 w-4 h-4 rounded-full shadow-sm transition-all duration-300 ${pushEnabled
                    ? 'right-1 bg-white'
                    : 'left-1 bg-white/50'
                  }`}
              />
            </div>

          </div>
        </div>
      </section>

    </div>
  );
};

export default Settings;
