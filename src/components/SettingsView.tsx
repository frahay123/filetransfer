import { useState } from 'react';
import { Folder, Calendar, Copy, Trash2, Image, Video, Save } from 'lucide-react';

interface SettingsViewProps {
  destinationPath: string;
  onDestinationChange: (path: string) => void;
}

export default function SettingsView({ destinationPath, onDestinationChange }: SettingsViewProps) {
  const [organizeByDate, setOrganizeByDate] = useState(true);
  const [skipDuplicates, setSkipDuplicates] = useState(true);
  const [deleteAfterTransfer, setDeleteAfterTransfer] = useState(false);
  const [transferPhotos, setTransferPhotos] = useState(true);
  const [transferVideos, setTransferVideos] = useState(true);
  const [saved, setSaved] = useState(false);

  const handleSave = () => {
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  };

  return (
    <div className="flex-1 overflow-auto p-8">
      <div className="max-w-2xl mx-auto animate-fade-in">
        <h1 className="text-3xl font-bold mb-2">Settings</h1>
        <p className="text-dark-400 mb-8">Configure how photos are transferred and organized</p>

        {/* Destination */}
        <section className="mb-8">
          <h2 className="text-lg font-semibold mb-4 flex items-center gap-2">
            <Folder className="w-5 h-5 text-primary-400" />
            Destination Folder
          </h2>
          <div className="glass rounded-xl p-4">
            <div className="flex gap-3">
              <input
                type="text"
                value={destinationPath}
                onChange={(e) => onDestinationChange(e.target.value)}
                className="flex-1 px-4 py-3 bg-dark-800 border border-dark-700 rounded-lg focus:outline-none focus:border-primary-500 transition-colors"
              />
              <button className="px-4 py-3 bg-dark-700 hover:bg-dark-600 rounded-lg transition-colors">
                Browse
              </button>
            </div>
            <p className="text-sm text-dark-400 mt-2">
              Photos will be saved to this folder
            </p>
          </div>
        </section>

        {/* Organization */}
        <section className="mb-8">
          <h2 className="text-lg font-semibold mb-4 flex items-center gap-2">
            <Calendar className="w-5 h-5 text-purple-400" />
            Organization
          </h2>
          <div className="glass rounded-xl divide-y divide-white/5">
            <ToggleSetting
              icon={<Calendar className="w-5 h-5" />}
              title="Organize by Date"
              description="Create folders like 2024/January/15"
              enabled={organizeByDate}
              onToggle={() => setOrganizeByDate(!organizeByDate)}
            />
            <ToggleSetting
              icon={<Copy className="w-5 h-5" />}
              title="Skip Duplicates"
              description="Don't transfer files that already exist"
              enabled={skipDuplicates}
              onToggle={() => setSkipDuplicates(!skipDuplicates)}
            />
          </div>
        </section>

        {/* File Types */}
        <section className="mb-8">
          <h2 className="text-lg font-semibold mb-4 flex items-center gap-2">
            <Image className="w-5 h-5 text-green-400" />
            File Types
          </h2>
          <div className="glass rounded-xl divide-y divide-white/5">
            <ToggleSetting
              icon={<Image className="w-5 h-5" />}
              title="Transfer Photos"
              description="JPG, PNG, HEIC, RAW and other image formats"
              enabled={transferPhotos}
              onToggle={() => setTransferPhotos(!transferPhotos)}
            />
            <ToggleSetting
              icon={<Video className="w-5 h-5" />}
              title="Transfer Videos"
              description="MP4, MOV, and other video formats"
              enabled={transferVideos}
              onToggle={() => setTransferVideos(!transferVideos)}
            />
          </div>
        </section>

        {/* Danger Zone */}
        <section className="mb-8">
          <h2 className="text-lg font-semibold mb-4 flex items-center gap-2 text-red-400">
            <Trash2 className="w-5 h-5" />
            After Transfer
          </h2>
          <div className="glass rounded-xl border border-red-500/20">
            <ToggleSetting
              icon={<Trash2 className="w-5 h-5 text-red-400" />}
              title="Delete from Phone"
              description="Remove files from phone after successful transfer"
              enabled={deleteAfterTransfer}
              onToggle={() => setDeleteAfterTransfer(!deleteAfterTransfer)}
              danger
            />
          </div>
        </section>

        {/* Save Button */}
        <button
          onClick={handleSave}
          className={`w-full flex items-center justify-center gap-2 py-4 rounded-xl font-medium transition-all ${
            saved 
              ? 'bg-green-500 text-white' 
              : 'bg-primary-500 hover:bg-primary-600 text-white'
          }`}
        >
          <Save className="w-5 h-5" />
          {saved ? 'Settings Saved!' : 'Save Settings'}
        </button>
      </div>
    </div>
  );
}

function ToggleSetting({
  icon,
  title,
  description,
  enabled,
  onToggle,
  danger = false,
}: {
  icon: React.ReactNode;
  title: string;
  description: string;
  enabled: boolean;
  onToggle: () => void;
  danger?: boolean;
}) {
  return (
    <div className="flex items-center gap-4 p-4">
      <div className={`w-10 h-10 rounded-lg flex items-center justify-center ${
        danger ? 'bg-red-500/20' : 'bg-dark-800'
      }`}>
        {icon}
      </div>
      <div className="flex-1">
        <h3 className="font-medium">{title}</h3>
        <p className="text-sm text-dark-400">{description}</p>
      </div>
      <button
        onClick={onToggle}
        className={`w-12 h-7 rounded-full transition-colors relative ${
          enabled 
            ? danger ? 'bg-red-500' : 'bg-primary-500' 
            : 'bg-dark-700'
        }`}
      >
        <div
          className={`absolute top-1 w-5 h-5 rounded-full bg-white transition-transform ${
            enabled ? 'left-6' : 'left-1'
          }`}
        />
      </button>
    </div>
  );
}
