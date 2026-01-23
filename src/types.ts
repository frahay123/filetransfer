export interface Device {
  id: string;
  name: string;
  type: 'ios' | 'android';
  manufacturer: string;
  storageUsed: number;
  storageTotal: number;
  photoCount: number;
  connected: boolean;
}

export interface MediaItem {
  id: string;
  name: string;
  type: 'photo' | 'video';
  size: number;
  date: string;
  thumbnail?: string;
}

export interface TransferProgress {
  current: number;
  total: number;
  currentFile: string;
  bytesTransferred: number;
  bytesTotal: number;
  speed: number;
  status: 'preparing' | 'transferring' | 'complete' | 'error';
}

export interface Settings {
  destinationPath: string;
  organizeByDate: boolean;
  skipDuplicates: boolean;
  deleteAfterTransfer: boolean;
  transferVideos: boolean;
  transferPhotos: boolean;
}
