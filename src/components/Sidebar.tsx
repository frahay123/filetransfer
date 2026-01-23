import { Smartphone, Send, Settings, RefreshCw, Zap } from 'lucide-react';

interface SidebarProps {
  activeView: 'devices' | 'transfer' | 'settings';
  onViewChange: (view: 'devices' | 'transfer' | 'settings') => void;
  deviceCount: number;
  onRefresh: () => void;
  isScanning: boolean;
}

export default function Sidebar({ activeView, onViewChange, deviceCount, onRefresh, isScanning }: SidebarProps) {
  return (
    <aside className="w-72 glass border-r border-white/5 flex flex-col">
      {/* Logo */}
      <div className="p-6 border-b border-white/5">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-primary-500 to-purple-600 flex items-center justify-center glow">
            <Zap className="w-5 h-5 text-white" />
          </div>
          <div>
            <h1 className="font-bold text-lg">Photo Transfer</h1>
            <p className="text-xs text-dark-400">Fast & Secure</p>
          </div>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex-1 p-4 space-y-2">
        <NavItem
          icon={<Smartphone className="w-5 h-5" />}
          label="Devices"
          badge={deviceCount > 0 ? deviceCount : undefined}
          active={activeView === 'devices'}
          onClick={() => onViewChange('devices')}
        />
        <NavItem
          icon={<Send className="w-5 h-5" />}
          label="Transfers"
          active={activeView === 'transfer'}
          onClick={() => onViewChange('transfer')}
        />
        <NavItem
          icon={<Settings className="w-5 h-5" />}
          label="Settings"
          active={activeView === 'settings'}
          onClick={() => onViewChange('settings')}
        />
      </nav>

      {/* Refresh Button */}
      <div className="p-4 border-t border-white/5">
        <button
          onClick={onRefresh}
          disabled={isScanning}
          className="w-full flex items-center justify-center gap-2 py-3 px-4 rounded-xl bg-dark-800 hover:bg-dark-700 transition-colors disabled:opacity-50"
        >
          <RefreshCw className={`w-4 h-4 ${isScanning ? 'animate-spin' : ''}`} />
          <span>{isScanning ? 'Scanning...' : 'Refresh Devices'}</span>
        </button>
      </div>

      {/* Version */}
      <div className="p-4 text-center text-xs text-dark-500">
        Version 2.0.0
      </div>
    </aside>
  );
}

function NavItem({ 
  icon, 
  label, 
  badge, 
  active, 
  onClick 
}: { 
  icon: React.ReactNode; 
  label: string; 
  badge?: number; 
  active: boolean; 
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl transition-all ${
        active 
          ? 'bg-primary-500/20 text-primary-400' 
          : 'text-dark-300 hover:bg-dark-800 hover:text-white'
      }`}
    >
      {icon}
      <span className="font-medium">{label}</span>
      {badge !== undefined && (
        <span className="ml-auto px-2 py-0.5 bg-primary-500 text-white text-xs rounded-full">
          {badge}
        </span>
      )}
    </button>
  );
}
