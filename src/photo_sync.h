#ifndef PHOTO_SYNC_H
#define PHOTO_SYNC_H

#include "device_handler.h"
#include "photo_db.h"
#include "utils.h"
#include <string>

using namespace std;

/**
 * Photo/video synchronization handler
 * Works with any DeviceHandler implementation (Android MTP or iOS)
 */
class PhotoSync {
public:
    PhotoSync(DeviceHandler* device, PhotoDB* db, const string& destination_folder);
    
    // Sync operations
    struct SyncResult {
        int total_photos;
        int new_photos;
        int skipped_photos;
        int failed_photos;
        uint64_t total_size;
        uint64_t transferred_size;
    };
    
    SyncResult syncPhotos(bool only_new = true);
    bool transferPhoto(const MediaInfo& photo);
    
    // Configuration
    void setDestinationFolder(const string& folder) { destination_folder_ = folder; }
    string getDestinationFolder() const { return destination_folder_; }
    
    // Statistics
    int getNewPhotoCount() const { return new_photos_; }
    int getSkippedPhotoCount() const { return skipped_photos_; }
    
private:
    DeviceHandler* device_handler_;
    PhotoDB* db_;
    string destination_folder_;
    
    int new_photos_;
    int skipped_photos_;
    int failed_photos_;
    
    // Helper functions
    string generateLocalPath(const MediaInfo& photo);
    bool shouldTransferPhoto(const MediaInfo& photo, bool only_new);
    bool verifyTransfer(const string& local_path, const vector<uint8_t>& data, const string& expected_hash);
};

#endif // PHOTO_SYNC_H
