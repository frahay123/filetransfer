import { ArrowLeft, CheckCircle, XCircle, FileImage, Loader2 } from 'lucide-react';
import { TransferProgress } from '../types';

interface TransferViewProps {
  progress: TransferProgress | null;
  onBack: () => void;
}

export default function TransferView({ progress, onBack }: TransferViewProps) {
  if (!progress) {
    return (
      <div className="flex-1 flex items-center justify-center">
        <div className="text-center">
          <div className="w-24 h-24 mx-auto mb-6 rounded-3xl bg-dark-800 flex items-center justify-center">
            <FileImage className="w-12 h-12 text-dark-500" />
          </div>
          <h2 className="text-xl font-bold mb-2">No Active Transfer</h2>
          <p className="text-dark-400 mb-6">Select files from a device to start transferring</p>
          <button
            onClick={onBack}
            className="flex items-center gap-2 px-4 py-2 mx-auto bg-dark-800 hover:bg-dark-700 rounded-lg transition-colors"
          >
            <ArrowLeft className="w-4 h-4" />
            Back to Devices
          </button>
        </div>
      </div>
    );
  }

  const percentage = Math.round((progress.current / progress.total) * 100);
  const isComplete = progress.status === 'complete';
  const isError = progress.status === 'error';

  return (
    <div className="flex-1 flex items-center justify-center p-8">
      <div className="w-full max-w-lg animate-fade-in">
        {/* Status Icon */}
        <div className="text-center mb-8">
          {isComplete ? (
            <div className="w-24 h-24 mx-auto mb-6 rounded-full bg-green-500/20 flex items-center justify-center">
              <CheckCircle className="w-12 h-12 text-green-500" />
            </div>
          ) : isError ? (
            <div className="w-24 h-24 mx-auto mb-6 rounded-full bg-red-500/20 flex items-center justify-center">
              <XCircle className="w-12 h-12 text-red-500" />
            </div>
          ) : (
            <div className="w-24 h-24 mx-auto mb-6 rounded-full bg-primary-500/20 flex items-center justify-center">
              <Loader2 className="w-12 h-12 text-primary-500 animate-spin" />
            </div>
          )}

          <h2 className="text-2xl font-bold mb-2">
            {isComplete ? 'Transfer Complete!' : isError ? 'Transfer Failed' : 'Transferring Files...'}
          </h2>
          <p className="text-dark-400">
            {isComplete 
              ? `Successfully transferred ${progress.total} files`
              : isError
              ? 'An error occurred during transfer'
              : progress.currentFile
            }
          </p>
        </div>

        {/* Progress Bar */}
        <div className="mb-6">
          <div className="flex justify-between text-sm mb-2">
            <span>{progress.current} of {progress.total} files</span>
            <span>{percentage}%</span>
          </div>
          <div className="h-3 bg-dark-800 rounded-full overflow-hidden">
            <div
              className={`h-full rounded-full transition-all duration-300 ${
                isComplete 
                  ? 'bg-green-500' 
                  : isError 
                  ? 'bg-red-500' 
                  : 'bg-gradient-to-r from-primary-500 to-primary-400'
              }`}
              style={{ width: `${percentage}%` }}
            />
          </div>
        </div>

        {/* Stats */}
        <div className="grid grid-cols-2 gap-4 mb-8">
          <div className="glass rounded-xl p-4 text-center">
            <p className="text-2xl font-bold">{formatBytes(progress.bytesTransferred)}</p>
            <p className="text-sm text-dark-400">Transferred</p>
          </div>
          <div className="glass rounded-xl p-4 text-center">
            <p className="text-2xl font-bold">{formatSpeed(progress.speed)}</p>
            <p className="text-sm text-dark-400">Speed</p>
          </div>
        </div>

        {/* Actions */}
        {(isComplete || isError) && (
          <div className="flex justify-center gap-4">
            <button
              onClick={onBack}
              className="flex items-center gap-2 px-6 py-3 bg-dark-800 hover:bg-dark-700 rounded-xl transition-colors"
            >
              <ArrowLeft className="w-4 h-4" />
              Back to Devices
            </button>
            {isComplete && (
              <button
                onClick={() => {
                  // Open destination folder
                }}
                className="px-6 py-3 bg-primary-500 hover:bg-primary-600 rounded-xl transition-colors"
              >
                Open Folder
              </button>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}

function formatSpeed(bytesPerSecond: number): string {
  if (bytesPerSecond < 1024 * 1024) return `${(bytesPerSecond / 1024).toFixed(1)} KB/s`;
  return `${(bytesPerSecond / (1024 * 1024)).toFixed(1)} MB/s`;
}
