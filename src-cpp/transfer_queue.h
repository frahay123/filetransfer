#ifndef TRANSFER_QUEUE_H
#define TRANSFER_QUEUE_H

#include "device_handler.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <chrono>

/**
 * Transfer item with state tracking for resume support
 */
struct TransferItem {
    MediaInfo media;
    std::string local_path;
    std::string hash;
    
    enum class Status {
        PENDING,
        IN_PROGRESS,
        COMPLETED,
        FAILED,
        SKIPPED
    };
    
    Status status = Status::PENDING;
    uint64_t bytes_transferred = 0;
    std::string error_message;
    int retry_count = 0;
    
    // For resume support
    bool is_resumable = false;
    std::string temp_path;  // Temporary file path during transfer
};

/**
 * Transfer statistics
 */
struct TransferStats {
    int total_items = 0;
    int completed = 0;
    int failed = 0;
    int skipped = 0;
    int pending = 0;
    
    uint64_t total_bytes = 0;
    uint64_t transferred_bytes = 0;
    
    double transfer_speed = 0.0;  // bytes per second
    int eta_seconds = 0;  // estimated time remaining
    
    std::string current_file;
};

/**
 * Progress callback types
 */
using ProgressCallback = std::function<void(const TransferStats& stats)>;
using ItemCallback = std::function<void(const TransferItem& item)>;

/**
 * Transfer queue manager with resume support
 */
class TransferQueue {
public:
    TransferQueue();
    ~TransferQueue();
    
    // Queue management
    void addItem(const MediaInfo& media);
    void clear();
    int getQueueSize() const;
    
    // State persistence for resume
    bool saveState(const std::string& state_file);
    bool loadState(const std::string& state_file);
    bool hasIncompleteTransfers() const;
    
    // Transfer control
    void start();
    void pause();
    void resume();
    void cancel();
    bool isRunning() const { return is_running_; }
    bool isPaused() const { return is_paused_; }
    
    // Configuration
    void setDestinationFolder(const std::string& folder) { destination_folder_ = folder; }
    void setDeviceHandler(DeviceHandler* handler) { device_handler_ = handler; }
    void setMaxRetries(int retries) { max_retries_ = retries; }
    
    // Callbacks
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = callback; }
    void setItemCompletedCallback(ItemCallback callback) { item_completed_callback_ = callback; }
    void setItemFailedCallback(ItemCallback callback) { item_failed_callback_ = callback; }
    
    // Statistics
    TransferStats getStats() const;
    std::vector<TransferItem> getItems() const;
    
private:
    std::vector<TransferItem> items_;
    mutable std::mutex items_mutex_;
    
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> cancel_requested_{false};
    
    std::string destination_folder_;
    DeviceHandler* device_handler_ = nullptr;
    int max_retries_ = 3;
    
    ProgressCallback progress_callback_;
    ItemCallback item_completed_callback_;
    ItemCallback item_failed_callback_;
    
    // Transfer timing for speed calculation
    std::chrono::steady_clock::time_point transfer_start_time_;
    uint64_t bytes_at_start_ = 0;
    
    // Internal methods
    bool transferItem(TransferItem& item);
    std::string generateTempPath(const TransferItem& item);
    bool finalizeTempFile(TransferItem& item);
    void updateStats();
    void notifyProgress();
};

#endif // TRANSFER_QUEUE_H
