import { useState } from 'react';
import { signInWithEmailAndPassword } from 'firebase/auth';
import { auth } from '../firebaseConfig'; 
import { Lock, Mail, KeyRound, AlertCircle } from 'lucide-react';
import AmbientBackground from '../components/AmbientBackground';

const Login = () => {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleLogin = async (e) => {
    e.preventDefault();
    setError('');
    setLoading(true);
    
    try {
      await signInWithEmailAndPassword(auth, email, password);
    } catch {
      setError('Błędny e-mail lub hasło. Spróbuj ponownie.');
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen w-full flex items-center justify-center bg-[#0d0a08] text-slate-200 font-custom overflow-hidden relative px-4">
      
      <style>{`
        input:-webkit-autofill,
        input:-webkit-autofill:hover, 
        input:-webkit-autofill:focus, 
        input:-webkit-autofill:active {
          transition: background-color 5000s ease-in-out 0s;
          -webkit-text-fill-color: white !important;
        }
      `}</style>

      <AmbientBackground />

      <div className="relative z-10 w-full max-w-md animate-in fade-in zoom-in duration-500">
        <div className="text-center mb-10">
          <h1 className="text-5xl font-light tracking-tight text-white flex items-center justify-center gap-3 drop-shadow-[0_0_15px_rgba(255,255,255,0.1)]">
            <Lock className="text-[#4dc4c9]" size={40} />
            loriota
          </h1>
          <p className="text-white/40 mt-3 font-light text-sm tracking-wide uppercase">Modułowy system automatyki domowej oparty na IoT</p>
        </div>

        <form onSubmit={handleLogin} className="bg-[#151210]/60 border border-white/10 backdrop-blur-2xl rounded-[32px] p-8 shadow-2xl">
          
          {error && (
            <div className="mb-6 p-4 bg-red-500/10 border border-red-500/20 rounded-2xl flex items-center gap-3 text-red-400 text-sm font-light">
              <AlertCircle size={18} />
              {error}
            </div>
          )}

          <div className="space-y-5">
            <div>
              <label className="block text-xs font-medium text-white/50 uppercase tracking-wider mb-2 ml-1">E-mail</label>
              <div className="relative">
                <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none z-10">
                  <Mail size={18} className="text-white/30" />
                </div>
                <input
                  type="email"
                  value={email}
                  onChange={(e) => setEmail(e.target.value)}
                  className="w-full bg-black/20 border border-white/5 rounded-2xl py-3 pl-12 pr-4 text-white placeholder:text-white/20 focus:outline-none focus:border-[#4dc4c9]/50 focus:ring-1 focus:ring-[#4dc4c9]/50 transition-all font-light"
                  placeholder="admin@dom.pl"
                  required
                />
              </div>
            </div>

            <div>
              <label className="block text-xs font-medium text-white/50 uppercase tracking-wider mb-2 ml-1">Hasło</label>
              <div className="relative">
                <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none z-10">
                  <KeyRound size={18} className="text-white/30" />
                </div>
                <input
                  type="password"
                  value={password}
                  onChange={(e) => setPassword(e.target.value)}
                  className="w-full bg-black/20 border border-white/5 rounded-2xl py-3 pl-12 pr-4 text-white placeholder:text-white/20 focus:outline-none focus:border-[#4dc4c9]/50 focus:ring-1 focus:ring-[#4dc4c9]/50 transition-all font-light"
                  placeholder="••••••••"
                  required
                />
              </div>
            </div>
          </div>

          <button
            type="submit"
            disabled={loading}
            className="w-full mt-8 relative overflow-hidden rounded-2xl group transition-all shadow-[0_0_20px_rgba(77,196,201,0.2)] disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <div className="absolute inset-0 bg-gradient-to-r from-[#4dc4c9] to-[#2b8a8e] group-hover:scale-105 transition-transform duration-300"></div>
            <div className="relative py-3.5 text-white font-medium">
              {loading ? 'Weryfikacja...' : 'Zaloguj się'}
            </div>
          </button>
        </form>
      </div>
    </div>
  );
};

export default Login;
