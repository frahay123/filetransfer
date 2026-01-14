#include "photo_sync.h"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <unistd.h>
#include <cstdint>

using namespace std;

PhotoSync::PhotoSync(DeviceHandler* device, PhotoDB* db, const string& destination_folder)
    : device_handler_(device), db_(db), destination_folder_(destination_folder),
      new_photos_(0), skipped_photos_(0), failed_photos_(0) {
}

PhotoSync::SyncResult PhotoSync::syncPhotos(bool only_new) {
    SyncResult result = {0, 0, 0, 0, 0, 0};
    
    if (!device_handler_ || !device_handler_->isConnected()) {
        cerr << "Error: Device not connected" << endl;
        return result;
    }
    
    if (!db_ || !db_->isOpen()) {
        cerr << "Error: Database not open" << endl;
        return result;
    }
    
    // Expand destination folder path
    string dest = Utils::expandPath(destination_folder_);
    if (!Utils::createDirectory(dest)) {
        cerr << "Error: Failed to create destination directory: " << dest << endl;
        return result;
    }
    
    cout << "\n=== Starting Photo Sync ===" << endl;
    cout << "Device Type: " << DeviceHandler::getDeviceTypeName(device_handler_->getDeviceType()) << endl;
    cout << "Destination: " << dest << endl;
    cout << "Mode: " << (only_new ? "New photos/videos only" : "All photos/videos") << endl;
    
    // Get last sync time
    uint64_t last_sync = 0;
    if (only_new) {
        last_sync = db_->getLastSyncTime();
        if (last_sync > 0) {
            cout << "Last sync: " << Utils::formatDate(last_sync) << endl;
        } else {
            cout << "First sync - will transfer all photos" << endl;
        }
    }
    
    // Enumerate photos and videos from device
    cout << "\nEnumerating photos and videos from device..." << endl;
    vector<MediaInfo> photos = device_handler_->enumerateMedia();
    result.total_photos = photos.size();
    
    cout << "Found " << photos.size() << " photos/videos on device" << endl;
    
    if (photos.empty()) {
        cout << "No photos to sync" << endl;
        return result;
    }
    
    // Filter photos if only_new
    if (only_new && last_sync > 0) {
        vector<MediaInfo> new_photos;
        for (const auto& photo : photos) {
            if (photo.modification_date > last_sync) {
                new_photos.push_back(photo);
            }
        }
        photos = new_photos;
        cout << "Filtered to " << photos.size() << " new photos (modified after last sync)" << endl;
    }
    
    // Transfer photos and videos
    cout << "\nTransferring photos and videos..." << endl;
    int transferred = 0;
    int skipped = 0;
    int failed = 0;
    uint64_t total_size = 0;
    uint64_t transferred_size = 0;
    
    for (size_t i = 0; i < photos.size(); i++) {
        const auto& photo = photos[i];
        total_size += photo.file_size;
        
        // Progress indicator
        if ((i + 1) % 10 == 0 || i == photos.size() - 1) {
            cout << "  Progress: " << (i + 1) << "/" << photos.size() 
                 << " (" << ((i + 1) * 100 / photos.size()) << "%)" << endl;
        }
        
        if (transferPhoto(photo)) {
            transferred++;
            transferred_size += photo.file_size;
        } else {
            // Check if it was skipped or failed
            if (shouldTransferPhoto(photo, only_new)) {
                failed++;
            } else {
                skipped++;
            }
        }
    }
    
    // Update last sync time
    uint64_t current_time = time(nullptr);
    db_->setLastSyncTime(current_time);
    
    result.new_photos = transferred;
    result.skipped_photos = skipped;
    result.failed_photos = failed;
    result.total_size = total_size;
    result.transferred_size = transferred_size;
    
    cout << "\n=== Sync Complete ===" << endl;
    cout << "Total photos: " << result.total_photos << endl;
    cout << "New/Transferred: " << result.new_photos << endl;
    cout << "Skipped (already exist): " << result.skipped_photos << endl;
    cout << "Failed: " << result.failed_photos << endl;
    cout << "Total size: " << (result.total_size / (1024.0 * 1024.0)) << " MB" << endl;
    cout << "Transferred: " << (result.transferred_size / (1024.0 * 1024.0)) << " MB" << endl;
    
    return result;
}

bool PhotoSync::transferPhoto(const MediaInfo& photo) {
    // Check if we should transfer this photo
    if (!shouldTransferPhoto(photo, true)) {
        return false; // Already exists, skip
    }
    
    // Read photo from device
    vector<uint8_t> data;
    if (!device_handler_->readFile(photo.object_id, data)) {
        cerr << "  Failed to read photo: " << photo.filename << endl;
        return false;
    }
    
    // Calculate hash
    string hash = Utils::calculateSHA256(data);
    
    // Double-check database (in case it was added between check and transfer)
    if (db_->photoExists(hash)) {
        string existing_path = db_->getLocalPath(hash);
        if (Utils::fileExists(existing_path)) {
            skipped_photos_++;
            return false; // Already transferred
        }
    }
    
    // Generate local path
    string local_path = generateLocalPath(photo);
    
    // Write file
    if (!Utils::writeFile(local_path, data)) {
        cerr << "  Failed to write file: " << local_path << endl;
        failed_photos_++;
        return false;
    }
    
    // Verify transfer
    if (!verifyTransfer(local_path, data, hash)) {
        cerr << "  Transfer verification failed: " << local_path << endl;
        failed_photos_++;
        // Clean up failed file
        unlink(local_path.c_str());
        return false;
    }
    
    // Update database
    if (!db_->addPhoto(hash, photo.path, local_path, 
                       photo.file_size, photo.modification_date)) {
        cerr << "  Warning: Failed to update database for: " << photo.filename << endl;
        // File was transferred successfully, so continue
    }
    
    new_photos_++;
    cout << "  ✓ Transferred: " << photo.filename << " (" 
         << (photo.file_size / 1024.0) << " KB)" << endl;
    
    return true;
}

bool PhotoSync::shouldTransferPhoto(const MediaInfo& photo, bool only_new) {
    (void)only_new; // May be used in future for filtering
    // Read photo to calculate hash
    vector<uint8_t> data;
    if (!device_handler_->readFile(photo.object_id, data)) {
        return false; // Can't read, skip
    }
    
    string hash = Utils::calculateSHA256(data);
    
    // Check database
    if (db_->photoExists(hash)) {
        string local_path = db_->getLocalPath(hash);
        // Verify file actually exists
        if (Utils::fileExists(local_path)) {
            return false; // Already transferred
        }
    }
    
    // Check file system (by hash or by path)
    string expected_path = generateLocalPath(photo);
    if (Utils::fileExists(expected_path)) {
        // File exists, verify it's the same by checking size
        if (Utils::getFileSize(expected_path) == photo.file_size) {
            // Might be the same file, add to database if not there
            if (!db_->photoExists(hash)) {
                db_->addPhoto(hash, photo.path, expected_path,
                             photo.file_size, photo.modification_date);
            }
            return false; // Already exists
        }
    }
    
    return true; // Should transfer
}

string PhotoSync::generateLocalPath(const MediaInfo& photo) {
    string dest = Utils::expandPath(destination_folder_);
    
    // Organize by date: YYYY/MM/filename
    string date_folder = Utils::getDateFolder(photo.modification_date);
    string folder = Utils::joinPath(dest, date_folder);
    
    // Use original filename
    string filename = photo.filename;
    
    // Sanitize filename (remove any path components)
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != string::npos) {
        filename = filename.substr(last_slash + 1);
    }
    
    return Utils::joinPath(folder, filename);
}

bool PhotoSync::verifyTransfer(const string& local_path, 
                               const vector<uint8_t>& original_data,
                               const string& expected_hash) {
    // Verify file exists
    if (!Utils::fileExists(local_path)) {
        return false;
    }
    
    // Verify file size
    if (Utils::getFileSize(local_path) != original_data.size()) {
        return false;
    }
    
    // Verify hash (optional but recommended)
    string file_hash = Utils::calculateFileHash(local_path);
    if (file_hash != expected_hash) {
        return false;
    }
    
    return true;
}
