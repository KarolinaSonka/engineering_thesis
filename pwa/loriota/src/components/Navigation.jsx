import React from 'react';
import { NavLink } from 'react-router-dom';
import { LayoutDashboard, Clock, Settings } from 'lucide-react';

const Navigation = () => {
  const navItems = [
    { path: '/', icon: LayoutDashboard, label: 'Pulpit' },
    { path: '/history', icon: Clock, label: 'Historia' },
    { path: '/settings', icon: Settings, label: 'Ustawienia' },
  ];

  return (
    <div className="fixed bottom-6 left-1/2 -translate-x-1/2 z-50 w-[90%] max-w-md">
      <div className="flex items-center justify-between bg-[#151210]/80 backdrop-blur-2xl border border-white/10 rounded-full px-6 py-3 shadow-[0_10px_40px_rgba(0,0,0,0.5)]">
        {navItems.map(({ path, icon: Icon, label }) => (
          <NavLink
            key={path}
            to={path}
            className={({ isActive }) =>
              `flex flex-col items-center gap-1 transition-all duration-300 ${
                isActive ? 'text-[#4dc4c9] scale-110' : 'text-white/40 hover:text-white/70'
              }`
            }
          >
            <Icon size={20} />
            <span className="text-[10px] font-medium tracking-wide">{label}</span>
          </NavLink>
        ))}
      </div>
    </div>
  );
};

export default Navigation;