import { useState } from 'react';
import { ArrowLeft, Check, Image, Video, Download, Grid3X3, List, Search } from 'lucide-react';
import { Device, MediaItem } from '../types';

interface DeviceViewProps {
  device: Device;
  mediaItems: MediaItem[];
  selectedItems: Set<string>;
  onSelectItem: (id: string) => void;
  onSelectAll: () => void;
  onTransfer: () => void;
  onBack: () => void;
}

export default function DeviceView({
  device,
  mediaItems,
  selectedItems,
  onSelectItem,
  onSelectAll,
  onTransfer,
  onBack,
}: DeviceViewProps) {
  const [viewMode, setViewMode] = useState<'grid' | 'list'>('grid');
  const [filter, setFilter] = useState<'all' | 'photos' | 'videos'>('all');
  const [searchQuery, setSearchQuery] = useState('');

  const filteredItems = mediaItems.filter(item => {
    if (filter === 'photos' && item.type !== 'photo') return false;
    if (filter === 'videos' && item.type !== 'video') return false;
    if (searchQuery && !item.name.toLowerCase().includes(searchQuery.toLowerCase())) return false;
    return true;
  });

  const totalSize = Array.from(selectedItems)
    .map(id => mediaItems.find(m => m.id === id)?.size || 0)
    .reduce((a, b) => a + b, 0);

  return (
    <div className="flex-1 flex flex-col">
      {/* Header */}
      <div className="p-6 border-b border-white/5 glass">
        <div className="flex items-center gap-4 mb-4">
          <button
            onClick={onBack}
            className="p-2 rounded-lg hover:bg-dark-800 transition-colors"
          >
            <ArrowLeft className="w-5 h-5" />
          </button>
          <div>
            <h1 className="text-xl font-bold">{device.name}</h1>
            <p className="text-sm text-dark-400">{device.manufacturer}</p>
          </div>
        </div>

        <div className="flex items-center gap-4">
          {/* Search */}
          <div className="flex-1 relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-dark-500" />
            <input
              type="text"
              placeholder="Search files..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-10 pr-4 py-2 bg-dark-800 border border-dark-700 rounded-lg focus:outline-none focus:border-primary-500 transition-colors"
            />
          </div>

          {/* Filter */}
          <div className="flex bg-dark-800 rounded-lg p-1">
            <FilterButton active={filter === 'all'} onClick={() => setFilter('all')}>
              All
            </FilterButton>
            <FilterButton active={filter === 'photos'} onClick={() => setFilter('photos')}>
              <Image className="w-4 h-4" />
            </FilterButton>
            <FilterButton active={filter === 'videos'} onClick={() => setFilter('videos')}>
              <Video className="w-4 h-4" />
            </FilterButton>
          </div>

          {/* View mode */}
          <div className="flex bg-dark-800 rounded-lg p-1">
            <button
              onClick={() => setViewMode('grid')}
              className={`p-2 rounded ${viewMode === 'grid' ? 'bg-dark-700' : ''}`}
            >
              <Grid3X3 className="w-4 h-4" />
            </button>
            <button
              onClick={() => setViewMode('list')}
              className={`p-2 rounded ${viewMode === 'list' ? 'bg-dark-700' : ''}`}
            >
              <List className="w-4 h-4" />
            </button>
          </div>
        </div>
      </div>

      {/* Media Grid */}
      <div className="flex-1 overflow-auto p-6">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-4">
            <button
              onClick={onSelectAll}
              className="flex items-center gap-2 text-sm text-dark-400 hover:text-white transition-colors"
            >
              <div className={`w-5 h-5 rounded border-2 flex items-center justify-center transition-colors ${
                selectedItems.size === mediaItems.length && mediaItems.length > 0
                  ? 'bg-primary-500 border-primary-500'
                  : 'border-dark-600'
              }`}>
                {selectedItems.size === mediaItems.length && mediaItems.length > 0 && (
                  <Check className="w-3 h-3" />
                )}
              </div>
              Select All
            </button>
            <span className="text-dark-500">|</span>
            <span className="text-sm text-dark-400">
              {filteredItems.length} items
            </span>
          </div>
          
          {selectedItems.size > 0 && (
            <span className="text-sm text-primary-400">
              {selectedItems.size} selected ({formatBytes(totalSize)})
            </span>
          )}
        </div>

        {viewMode === 'grid' ? (
          <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6 gap-3">
            {filteredItems.map((item, index) => (
              <MediaCard
                key={item.id}
                item={item}
                selected={selectedItems.has(item.id)}
                onSelect={() => onSelectItem(item.id)}
                delay={index * 20}
              />
            ))}
          </div>
        ) : (
          <div className="space-y-2">
            {filteredItems.map((item, index) => (
              <MediaListItem
                key={item.id}
                item={item}
                selected={selectedItems.has(item.id)}
                onSelect={() => onSelectItem(item.id)}
                delay={index * 20}
              />
            ))}
          </div>
        )}
      </div>

      {/* Bottom Action Bar */}
      {selectedItems.size > 0 && (
        <div className="p-4 border-t border-white/5 glass animate-slide-up">
          <div className="flex items-center justify-between">
            <div>
              <p className="font-medium">{selectedItems.size} files selected</p>
              <p className="text-sm text-dark-400">Total size: {formatBytes(totalSize)}</p>
            </div>
            <button
              onClick={onTransfer}
              className="flex items-center gap-2 px-6 py-3 bg-gradient-to-r from-primary-500 to-primary-600 hover:from-primary-600 hover:to-primary-700 rounded-xl font-medium transition-all glow"
            >
              <Download className="w-5 h-5" />
              Transfer to Computer
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

function FilterButton({ 
  children, 
  active, 
  onClick 
}: { 
  children: React.ReactNode; 
  active: boolean; 
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={`px-3 py-1 rounded transition-colors ${
        active ? 'bg-dark-700 text-white' : 'text-dark-400 hover:text-white'
      }`}
    >
      {children}
    </button>
  );
}

function MediaCard({ 
  item, 
  selected, 
  onSelect,
  delay 
}: { 
  item: MediaItem; 
  selected: boolean; 
  onSelect: () => void;
  delay: number;
}) {
  return (
    <button
      onClick={onSelect}
      className="relative aspect-square rounded-xl overflow-hidden group animate-fade-in"
      style={{ animationDelay: `${delay}ms` }}
    >
      <img
        src={item.thumbnail || `https://picsum.photos/seed/${item.id}/200/200`}
        alt={item.name}
        className="w-full h-full object-cover transition-transform group-hover:scale-105"
      />
      
      {/* Overlay */}
      <div className={`absolute inset-0 transition-colors ${
        selected ? 'bg-primary-500/30' : 'bg-black/0 group-hover:bg-black/20'
      }`} />

      {/* Selection indicator */}
      <div className={`absolute top-2 right-2 w-6 h-6 rounded-full border-2 flex items-center justify-center transition-all ${
        selected 
          ? 'bg-primary-500 border-primary-500 scale-100' 
          : 'border-white/50 scale-90 group-hover:scale-100'
      }`}>
        {selected && <Check className="w-3 h-3" />}
      </div>

      {/* Video indicator */}
      {item.type === 'video' && (
        <div className="absolute bottom-2 left-2 px-2 py-1 bg-black/60 rounded text-xs flex items-center gap-1">
          <Video className="w-3 h-3" />
        </div>
      )}
    </button>
  );
}

function MediaListItem({ 
  item, 
  selected, 
  onSelect,
  delay 
}: { 
  item: MediaItem; 
  selected: boolean; 
  onSelect: () => void;
  delay: number;
}) {
  return (
    <button
      onClick={onSelect}
      className={`w-full flex items-center gap-4 p-3 rounded-xl transition-all animate-fade-in ${
        selected ? 'bg-primary-500/20' : 'hover:bg-dark-800'
      }`}
      style={{ animationDelay: `${delay}ms` }}
    >
      <div className={`w-6 h-6 rounded-full border-2 flex items-center justify-center ${
        selected ? 'bg-primary-500 border-primary-500' : 'border-dark-600'
      }`}>
        {selected && <Check className="w-3 h-3" />}
      </div>
      
      <img
        src={item.thumbnail || `https://picsum.photos/seed/${item.id}/200/200`}
        alt={item.name}
        className="w-12 h-12 rounded-lg object-cover"
      />
      
      <div className="flex-1 text-left">
        <p className="font-medium truncate">{item.name}</p>
        <p className="text-sm text-dark-400">
          {new Date(item.date).toLocaleDateString()} • {formatBytes(item.size)}
        </p>
      </div>

      {item.type === 'video' && (
        <Video className="w-4 h-4 text-dark-500" />
      )}
    </button>
  );
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}
