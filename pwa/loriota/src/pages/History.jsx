import React from 'react';
import { Clock } from 'lucide-react';

const History = () => {
  return (
    <div className="p-4 md:p-8 animate-in fade-in duration-500">
      <header className="flex items-center gap-3 mb-8">
        <Clock className="text-[#4dc4c9]" size={28} />
        <h1 className="text-2xl md:text-3xl font-light text-white">Dziennik zdarzeń</h1>
      </header>
      <div className="text-white/50 font-light">
        Tutaj pojawi się oś czasu z pobranymi logami z Firebase
      </div>
    </div>
  );
};

export default History;