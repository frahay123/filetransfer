import { Smartphone, RefreshCw, Usb, Wifi } from 'lucide-react';

interface EmptyStateProps {
  onRefresh: () => void;
  isScanning: boolean;
}

export default function EmptyState({ onRefresh, isScanning }: EmptyStateProps) {
  return (
    <div className="flex-1 flex items-center justify-center p-8">
      <div className="text-center max-w-md animate-fade-in">
        <div className="w-24 h-24 mx-auto mb-8 rounded-3xl bg-gradient-to-br from-dark-800 to-dark-900 flex items-center justify-center">
          <Smartphone className="w-12 h-12 text-dark-500" />
        </div>
        
        <h2 className="text-2xl font-bold mb-3">No Devices Found</h2>
        <p className="text-dark-400 mb-8">
          Connect your phone via USB cable and make sure it's unlocked
        </p>

        <div className="space-y-4 mb-8">
          <div className="flex items-center gap-4 p-4 glass rounded-xl text-left">
            <div className="w-10 h-10 rounded-lg bg-primary-500/20 flex items-center justify-center">
              <Usb className="w-5 h-5 text-primary-400" />
            </div>
            <div>
              <h3 className="font-medium">USB Connection</h3>
              <p className="text-sm text-dark-400">Connect your device with a USB cable</p>
            </div>
          </div>

          <div className="flex items-center gap-4 p-4 glass rounded-xl text-left">
            <div className="w-10 h-10 rounded-lg bg-green-500/20 flex items-center justify-center">
              <Wifi className="w-5 h-5 text-green-400" />
            </div>
            <div>
              <h3 className="font-medium">Trust This Computer</h3>
              <p className="text-sm text-dark-400">Tap "Trust" when prompted on your device</p>
            </div>
          </div>
        </div>

        <button
          onClick={onRefresh}
          disabled={isScanning}
          className="inline-flex items-center gap-2 px-6 py-3 bg-primary-500 hover:bg-primary-600 rounded-xl font-medium transition-colors disabled:opacity-50"
        >
          <RefreshCw className={`w-4 h-4 ${isScanning ? 'animate-spin' : ''}`} />
          <span>{isScanning ? 'Scanning...' : 'Scan for Devices'}</span>
        </button>
      </div>
    </div>
  );
}
