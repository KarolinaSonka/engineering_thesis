import React from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import Navigation from './components/Navigation';
import Dashboard from './pages/Dashboard';
import History from './pages/History';
import Settings from './pages/Settings';

const App = () => {
  return (
    <Router>
      <div className="min-h-screen w-full bg-[#0d0a08] text-slate-200 font-custom overflow-x-hidden relative flex flex-col items-center pb-24">
        
        {/*background*/}
        <div className="fixed inset-0 pointer-events-none z-0 overflow-hidden">
          <div className="absolute inset-0 bg-[#0d0a08]"></div>
          <div className="absolute -top-[10%] -left-[10%] w-[70vw] h-[50vh] bg-[#2b8a8e] rounded-full mix-blend-screen filter blur-[100px] opacity-30 animate-blob1"></div>
          <div className="absolute top-[20%] -right-[20%] w-[80vw] h-[70vh] bg-[#d9652b] rounded-full mix-blend-screen filter blur-[120px] opacity-40 animate-blob2"></div>
          <div className="absolute -bottom-[10%] left-[10%] w-[60vw] h-[50vh] bg-[#ff8a4c] rounded-full mix-blend-screen filter blur-[140px] opacity-20 animate-blob1" style={{ animationDelay: '2s' }}></div>
          <div className="absolute inset-0 opacity-[0.03] mix-blend-overlay" style={{ backgroundImage: 'url("data:image/svg+xml,%3Csvg viewBox=%220 0 200 200%22 xmlns=%22http://www.w3.org/2000/svg%22%3E%3Cfilter id=%22noiseFilter%22%3E%3CfeTurbulence type=%22fractalNoise%22 baseFrequency=%220.65%22 numOctaves=%223%22 stitchTiles=%22stitch%22/%3E%3C/filter%3E%3Crect width=%22100%25%22 height=%22100%25%22 filter=%22url(%23noiseFilter)%22/%3E%3C/svg%3E")' }}></div>
        </div>

        {/*screens*/}
        <div className="relative z-10 w-full max-w-7xl flex flex-col min-h-screen">
          <Routes>
            <Route path="/" element={<Dashboard />} />
            <Route path="/history" element={<History />} />
            <Route path="/settings" element={<Settings />} />
          </Routes>
        </div>

        {/*navigation panel*/}
        <Navigation />
        
      </div>
    </Router>
  );
};

export default App;