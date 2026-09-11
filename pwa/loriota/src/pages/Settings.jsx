import React from 'react';
import { Settings as SettingsIcon } from 'lucide-react';

const Settings = () => {
  return (
    <div className="p-4 md:p-8 animate-in fade-in duration-500">
      <header className="flex items-center gap-3 mb-8">
        <SettingsIcon className="text-[#d9652b]" size={28} />
        <h1 className="text-2xl md:text-3xl font-light text-white">Ustawienia węzłów</h1>
      </header>
      <div className="text-white/50 font-light">
        Tutaj będzie przypisywanie nazw do ID węzłów oraz parowanie i powiadomienia
      </div>
    </div>
  );
};

export default Settings;