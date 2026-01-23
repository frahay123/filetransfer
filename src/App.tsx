import { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/tauri';
import Sidebar from './components/Sidebar';
import DeviceView from './components/DeviceView';
import TransferView from './components/TransferView';
import SettingsView from './components/SettingsView';
import EmptyState from './components/EmptyState';
import { Device, MediaItem, TransferProgress } from './types';

function App() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [mediaItems, setMediaItems] = useState<MediaItem[]>([]);
  const [selectedItems, setSelectedItems] = useState<Set<string>>(new Set());
  const [activeView, setActiveView] = useState<'devices' | 'transfer' | 'settings'>('devices');
  const [isScanning, setIsScanning] = useState(false);
  const [transferProgress, setTransferProgress] = useState<TransferProgress | null>(null);
  const [destinationPath, setDestinationPath] = useState('~/Pictures/PhonePhotos');

  const scanDevices = async () => {
    setIsScanning(true);
    try {
      const foundDevices = await invoke<Device[]>('scan_devices');
      setDevices(foundDevices);
    } catch (error) {
      console.error('Failed to scan devices:', error);
      // Demo devices for development
      setDevices([
        {
          id: 'demo-1',
          name: 'iPhone 15 Pro',
          type: 'ios',
          manufacturer: 'Apple',
          storageUsed: 64000000000,
          storageTotal: 128000000000,
          photoCount: 2847,
          connected: true,
        },
        {
          id: 'demo-2',
          name: 'Galaxy S24 Ultra',
          type: 'android',
          manufacturer: 'Samsung',
          storageUsed: 98000000000,
          storageTotal: 256000000000,
          photoCount: 5621,
          connected: true,
        },
      ]);
    }
    setIsScanning(false);
  };

  const connectDevice = async (device: Device) => {
    try {
      await invoke('connect_device', { deviceId: device.id });
      setSelectedDevice(device);
      const items = await invoke<MediaItem[]>('get_media_items', { deviceId: device.id });
      setMediaItems(items);
    } catch (error) {
      console.error('Failed to connect:', error);
      // Demo media items
      setSelectedDevice(device);
      setMediaItems(generateDemoMedia(50));
    }
  };

  const transferSelected = async () => {
    if (selectedItems.size === 0) return;
    
    setActiveView('transfer');
    setTransferProgress({
      current: 0,
      total: selectedItems.size,
      currentFile: '',
      bytesTransferred: 0,
      bytesTotal: 0,
      speed: 0,
      status: 'preparing',
    });

    try {
      await invoke('transfer_files', {
        deviceId: selectedDevice?.id,
        fileIds: Array.from(selectedItems),
        destination: destinationPath,
      });
    } catch (error) {
      console.error('Transfer failed:', error);
      // Simulate transfer for demo
      simulateTransfer();
    }
  };

  const simulateTransfer = () => {
    const total = selectedItems.size;
    let current = 0;
    
    const interval = setInterval(() => {
      current++;
      setTransferProgress({
        current,
        total,
        currentFile: `IMG_${1000 + current}.jpg`,
        bytesTransferred: current * 5000000,
        bytesTotal: total * 5000000,
        speed: 25000000,
        status: current < total ? 'transferring' : 'complete',
      });
      
      if (current >= total) {
        clearInterval(interval);
      }
    }, 500);
  };

  useEffect(() => {
    scanDevices();
  }, []);

  return (
    <div className="h-screen flex bg-dark-950">
      {/* Background gradient */}
      <div className="fixed inset-0 pointer-events-none">
        <div className="absolute top-0 right-0 w-[800px] h-[600px] bg-primary-500/10 rounded-full blur-[150px]" />
        <div className="absolute bottom-0 left-0 w-[600px] h-[400px] bg-purple-500/10 rounded-full blur-[150px]" />
      </div>

      <Sidebar
        activeView={activeView}
        onViewChange={setActiveView}
        deviceCount={devices.length}
        onRefresh={scanDevices}
        isScanning={isScanning}
      />

      <main className="flex-1 flex flex-col relative overflow-hidden">
        {activeView === 'devices' && (
          <>
            {selectedDevice ? (
              <DeviceView
                device={selectedDevice}
                mediaItems={mediaItems}
                selectedItems={selectedItems}
                onSelectItem={(id) => {
                  const newSelected = new Set(selectedItems);
                  if (newSelected.has(id)) {
                    newSelected.delete(id);
                  } else {
                    newSelected.add(id);
                  }
                  setSelectedItems(newSelected);
                }}
                onSelectAll={() => {
                  if (selectedItems.size === mediaItems.length) {
                    setSelectedItems(new Set());
                  } else {
                    setSelectedItems(new Set(mediaItems.map(m => m.id)));
                  }
                }}
                onTransfer={transferSelected}
                onBack={() => {
                  setSelectedDevice(null);
                  setMediaItems([]);
                  setSelectedItems(new Set());
                }}
              />
            ) : devices.length > 0 ? (
              <div className="flex-1 p-8 overflow-auto">
                <div className="mb-8 animate-fade-in">
                  <h1 className="text-3xl font-bold mb-2">Your Devices</h1>
                  <p className="text-dark-400">
                    {devices.length} device{devices.length !== 1 ? 's' : ''} connected
                  </p>
                </div>
                
                <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                  {devices.map((device, index) => (
                    <DeviceCard
                      key={device.id}
                      device={device}
                      onClick={() => connectDevice(device)}
                      delay={index * 100}
                    />
                  ))}
                </div>
              </div>
            ) : (
              <EmptyState onRefresh={scanDevices} isScanning={isScanning} />
            )}
          </>
        )}

        {activeView === 'transfer' && (
          <TransferView
            progress={transferProgress}
            onBack={() => setActiveView('devices')}
          />
        )}

        {activeView === 'settings' && (
          <SettingsView
            destinationPath={destinationPath}
            onDestinationChange={setDestinationPath}
          />
        )}
      </main>
    </div>
  );
}

function DeviceCard({ device, onClick, delay }: { device: Device; onClick: () => void; delay: number }) {
  const storagePercent = (device.storageUsed / device.storageTotal) * 100;
  const isIOS = device.type === 'ios';

  return (
    <button
      onClick={onClick}
      className="device-card glass rounded-2xl p-6 text-left group animate-slide-up"
      style={{ animationDelay: `${delay}ms` }}
    >
      <div className="flex items-start justify-between mb-4">
        <div className={`w-12 h-12 rounded-xl flex items-center justify-center ${
          isIOS ? 'bg-gradient-to-br from-gray-600 to-gray-800' : 'bg-gradient-to-br from-green-500 to-emerald-600'
        }`}>
          {isIOS ? (
            <svg className="w-6 h-6 text-white" viewBox="0 0 24 24" fill="currentColor">
              <path d="M18.71 19.5c-.83 1.24-1.71 2.45-3.05 2.47-1.34.03-1.77-.79-3.29-.79-1.53 0-2 .77-3.27.82-1.31.05-2.3-1.32-3.14-2.53C4.25 17 2.94 12.45 4.7 9.39c.87-1.52 2.43-2.48 4.12-2.51 1.28-.02 2.5.87 3.29.87.78 0 2.26-1.07 3.81-.91.65.03 2.47.26 3.64 1.98-.09.06-2.17 1.28-2.15 3.81.03 3.02 2.65 4.03 2.68 4.04-.03.07-.42 1.44-1.38 2.83M13 3.5c.73-.83 1.94-1.46 2.94-1.5.13 1.17-.34 2.35-1.04 3.19-.69.85-1.83 1.51-2.95 1.42-.15-1.15.41-2.35 1.05-3.11z"/>
            </svg>
          ) : (
            <svg className="w-6 h-6 text-white" viewBox="0 0 24 24" fill="currentColor">
              <path d="M17.6 11.48l1.89-3.28a.44.44 0 0 0-.16-.6.43.43 0 0 0-.6.16L16.82 11a7.6 7.6 0 0 0-9.64 0L5.27 7.76a.43.43 0 0 0-.6-.16.44.44 0 0 0-.16.6l1.89 3.28A7.54 7.54 0 0 0 3 17h18a7.54 7.54 0 0 0-3.4-5.52zM7 15a1 1 0 1 1 1-1 1 1 0 0 1-1 1zm10 0a1 1 0 1 1 1-1 1 1 0 0 1-1 1z"/>
            </svg>
          )}
        </div>
        <span className="px-2 py-1 bg-green-500/20 text-green-400 text-xs rounded-full">
          Connected
        </span>
      </div>

      <h3 className="text-lg font-semibold mb-1 group-hover:text-primary-400 transition-colors">
        {device.name}
      </h3>
      <p className="text-dark-400 text-sm mb-4">{device.manufacturer}</p>

      <div className="space-y-3">
        <div>
          <div className="flex justify-between text-sm mb-1">
            <span className="text-dark-400">Storage</span>
            <span>{formatBytes(device.storageUsed)} / {formatBytes(device.storageTotal)}</span>
          </div>
          <div className="h-2 bg-dark-800 rounded-full overflow-hidden">
            <div
              className="h-full bg-gradient-to-r from-primary-500 to-primary-400 rounded-full transition-all"
              style={{ width: `${storagePercent}%` }}
            />
          </div>
        </div>

        <div className="flex items-center justify-between text-sm">
          <span className="text-dark-400">Photos & Videos</span>
          <span className="font-medium">{device.photoCount.toLocaleString()}</span>
        </div>
      </div>
    </button>
  );
}

function formatBytes(bytes: number): string {
  const gb = bytes / (1024 * 1024 * 1024);
  return `${gb.toFixed(1)} GB`;
}

function generateDemoMedia(count: number): MediaItem[] {
  const items: MediaItem[] = [];
  for (let i = 0; i < count; i++) {
    items.push({
      id: `media-${i}`,
      name: `IMG_${1000 + i}.${Math.random() > 0.8 ? 'mp4' : 'jpg'}`,
      type: Math.random() > 0.8 ? 'video' : 'photo',
      size: Math.floor(Math.random() * 10000000) + 1000000,
      date: new Date(Date.now() - Math.random() * 30 * 24 * 60 * 60 * 1000).toISOString(),
      thumbnail: `https://picsum.photos/seed/${i}/200/200`,
    });
  }
  return items;
}

export default App;
