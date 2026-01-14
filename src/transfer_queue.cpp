#include "transfer_queue.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>
#include <unistd.h>

using namespace std;

TransferQueue::TransferQueue() {}

TransferQueue::~TransferQueue() {
    cancel();
}

void TransferQueue::addItem(const MediaInfo& media) {
    lock_guard<mutex> lock(items_mutex_);
    
    TransferItem item;
    item.media = media;
    item.status = TransferItem::Status::PENDING;
    item.is_resumable = true;
    
    items_.push_back(item);
}

void TransferQueue::clear() {
    lock_guard<mutex> lock(items_mutex_);
    items_.clear();
}

int TransferQueue::getQueueSize() const {
    lock_guard<mutex> lock(items_mutex_);
    return items_.size();
}

bool TransferQueue::saveState(const string& state_file) {
    lock_guard<mutex> lock(items_mutex_);
    
    ofstream file(state_file);
    if (!file) {
        return false;
    }
    
    // Simple format: one item per line
    // status|object_id|filename|path|file_size|bytes_transferred|local_path|temp_path|hash
    file << "# PhotoTransfer Queue State v1.0\n";
    file << "# Generated: " << time(nullptr) << "\n";
    file << "destination:" << destination_folder_ << "\n";
    
    for (const auto& item : items_) {
        file << static_cast<int>(item.status) << "|"
             << item.media.object_id << "|"
             << item.media.filename << "|"
             << item.media.path << "|"
             << item.media.file_size << "|"
             << item.bytes_transferred << "|"
             << item.local_path << "|"
             << item.temp_path << "|"
             << item.hash << "\n";
    }
    
    return true;
}

bool TransferQueue::loadState(const string& state_file) {
    ifstream file(state_file);
    if (!file) {
        return false;
    }
    
    lock_guard<mutex> lock(items_mutex_);
    items_.clear();
    
    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (line.substr(0, 12) == "destination:") {
            destination_folder_ = line.substr(12);
            continue;
        }
        
        // Parse item line
        stringstream ss(line);
        string token;
        vector<string> tokens;
        
        while (getline(ss, token, '|')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 9) {
            TransferItem item;
            item.status = static_cast<TransferItem::Status>(stoi(tokens[0]));
            item.media.object_id = stoul(tokens[1]);
            item.media.filename = tokens[2];
            item.media.path = tokens[3];
            item.media.file_size = stoull(tokens[4]);
            item.bytes_transferred = stoull(tokens[5]);
            item.local_path = tokens[6];
            item.temp_path = tokens[7];
            item.hash = tokens[8];
            item.is_resumable = true;
            
            // Reset in-progress items to pending for resume
            if (item.status == TransferItem::Status::IN_PROGRESS) {
                item.status = TransferItem::Status::PENDING;
            }
            
            items_.push_back(item);
        }
    }
    
    return true;
}

bool TransferQueue::hasIncompleteTransfers() const {
    lock_guard<mutex> lock(items_mutex_);
    
    for (const auto& item : items_) {
        if (item.status == TransferItem::Status::PENDING ||
            item.status == TransferItem::Status::IN_PROGRESS) {
            return true;
        }
    }
    return false;
}

void TransferQueue::start() {
    if (is_running_) return;
    
    is_running_ = true;
    is_paused_ = false;
    cancel_requested_ = false;
    
    transfer_start_time_ = chrono::steady_clock::now();
    bytes_at_start_ = 0;
    
    // Process items
    for (size_t i = 0; i < items_.size() && !cancel_requested_; i++) {
        while (is_paused_ && !cancel_requested_) {
            std::this_thread::sleep_for(chrono::milliseconds(100));
        }
        
        if (cancel_requested_) break;
        
        TransferItem& item = items_[i];
        
        if (item.status != TransferItem::Status::PENDING) {
            continue;
        }
        
        item.status = TransferItem::Status::IN_PROGRESS;
        notifyProgress();
        
        bool success = transferItem(item);
        
        if (success) {
            item.status = TransferItem::Status::COMPLETED;
            if (item_completed_callback_) {
                item_completed_callback_(item);
            }
        } else {
            if (item.retry_count < max_retries_) {
                item.retry_count++;
                item.status = TransferItem::Status::PENDING;
                i--; // Retry this item
            } else {
                item.status = TransferItem::Status::FAILED;
                if (item_failed_callback_) {
                    item_failed_callback_(item);
                }
            }
        }
        
        notifyProgress();
    }
    
    is_running_ = false;
}

void TransferQueue::pause() {
    is_paused_ = true;
}

void TransferQueue::resume() {
    is_paused_ = false;
}

void TransferQueue::cancel() {
    cancel_requested_ = true;
    is_paused_ = false;
    
    // Wait for running transfer to stop
    while (is_running_) {
        std::this_thread::sleep_for(chrono::milliseconds(50));
    }
}

TransferStats TransferQueue::getStats() const {
    lock_guard<mutex> lock(items_mutex_);
    
    TransferStats stats;
    stats.total_items = items_.size();
    
    for (const auto& item : items_) {
        stats.total_bytes += item.media.file_size;
        
        switch (item.status) {
            case TransferItem::Status::COMPLETED:
                stats.completed++;
                stats.transferred_bytes += item.media.file_size;
                break;
            case TransferItem::Status::FAILED:
                stats.failed++;
                break;
            case TransferItem::Status::SKIPPED:
                stats.skipped++;
                break;
            case TransferItem::Status::IN_PROGRESS:
                stats.transferred_bytes += item.bytes_transferred;
                stats.current_file = item.media.filename;
                [[fallthrough]];
            case TransferItem::Status::PENDING:
                stats.pending++;
                break;
        }
    }
    
    // Calculate transfer speed
    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::seconds>(now - transfer_start_time_).count();
    
    if (elapsed > 0 && stats.transferred_bytes > bytes_at_start_) {
        stats.transfer_speed = (stats.transferred_bytes - bytes_at_start_) / static_cast<double>(elapsed);
        
        uint64_t remaining_bytes = stats.total_bytes - stats.transferred_bytes;
        if (stats.transfer_speed > 0) {
            stats.eta_seconds = remaining_bytes / stats.transfer_speed;
        }
    }
    
    return stats;
}

vector<TransferItem> TransferQueue::getItems() const {
    lock_guard<mutex> lock(items_mutex_);
    return items_;
}

bool TransferQueue::transferItem(TransferItem& item) {
    if (!device_handler_ || !device_handler_->isConnected()) {
        item.error_message = "Device not connected";
        return false;
    }
    
    // Generate paths
    item.local_path = Utils::joinPath(
        Utils::expandPath(destination_folder_),
        Utils::joinPath(Utils::getDateFolder(item.media.modification_date), item.media.filename)
    );
    item.temp_path = generateTempPath(item);
    
    // Create directory
    string dir = Utils::getDirectory(item.local_path);
    if (!Utils::createDirectory(dir)) {
        item.error_message = "Failed to create directory: " + dir;
        return false;
    }
    
    // Check if already exists
    if (Utils::fileExists(item.local_path)) {
        if (Utils::getFileSize(item.local_path) == item.media.file_size) {
            item.status = TransferItem::Status::SKIPPED;
            return true;
        }
    }
    
    // Read file from device
    vector<uint8_t> data;
    if (!device_handler_->readFile(item.media.object_id, data)) {
        item.error_message = "Failed to read file from device";
        return false;
    }
    
    item.bytes_transferred = data.size();
    
    // Calculate hash
    item.hash = Utils::calculateSHA256(data);
    
    // Write to temp file first
    if (!Utils::writeFile(item.temp_path, data)) {
        item.error_message = "Failed to write temp file";
        return false;
    }
    
    // Verify and finalize
    if (!finalizeTempFile(item)) {
        item.error_message = "Failed to finalize transfer";
        return false;
    }
    
    return true;
}

string TransferQueue::generateTempPath(const TransferItem& item) {
    return item.local_path + ".part";
}

bool TransferQueue::finalizeTempFile(TransferItem& item) {
    // Verify temp file
    if (!Utils::fileExists(item.temp_path)) {
        return false;
    }
    
    // Verify hash
    string file_hash = Utils::calculateFileHash(item.temp_path);
    if (file_hash != item.hash) {
        unlink(item.temp_path.c_str());
        return false;
    }
    
    // Move temp to final location
    if (rename(item.temp_path.c_str(), item.local_path.c_str()) != 0) {
        // If rename fails, try copy and delete
        ifstream src(item.temp_path, ios::binary);
        ofstream dst(item.local_path, ios::binary);
        
        if (!src || !dst) {
            return false;
        }
        
        dst << src.rdbuf();
        src.close();
        dst.close();
        
        unlink(item.temp_path.c_str());
    }
    
    return true;
}

void TransferQueue::notifyProgress() {
    if (progress_callback_) {
        progress_callback_(getStats());
    }
}
