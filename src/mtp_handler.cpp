#include "mtp_handler.h"
#include "utils.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

MTPHandler::MTPHandler() : device_(nullptr) {
    LIBMTP_Init();
}

MTPHandler::~MTPHandler() {
    disconnect();
    // No global release function needed - device is released in disconnect()
}

void MTPHandler::setError(const string& error) {
    last_error_ = error;
    cerr << "MTP Error: " << error << endl;
}

bool MTPHandler::detectDevices() {
    raw_devices_.clear();
    last_error_.clear();

    int num_devices = 0;
    LIBMTP_raw_device_t* raw_device_list = nullptr;

    int ret = LIBMTP_Detect_Raw_Devices(&raw_device_list, &num_devices);
    
    if (ret != 0 || num_devices == 0) {
        setError("No MTP devices detected. Make sure your phone is connected and unlocked.");
        return false;
    }

    // Store detected devices
    for (int i = 0; i < num_devices; i++) {
        raw_devices_.push_back(raw_device_list[i]);
    }

    free(raw_device_list);
    return true;
}

bool MTPHandler::unmountMTPDevices() {
    bool unmounted_something = false;
    
    cout << "  Attempting to release MTP device from system..." << endl;
    
    // Method 1: Use gio mount (modern Ubuntu 22.04+)
    // List all mounts and unmount MTP ones
    FILE* pipe = popen("gio mount -l 2>/dev/null", "r");
    if (pipe != nullptr) {
        char buffer[512];
        string current_mount;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            string line = buffer;
            // Look for MTP mount lines
            if (line.find("mtp://") != string::npos) {
                // Extract the mtp:// URI
                size_t start = line.find("mtp://");
                if (start != string::npos) {
                    size_t end = line.find_first_of(" \n\t", start);
                    if (end == string::npos) end = line.length();
                    current_mount = line.substr(start, end - start);
                    // Clean up the mount path
                    while (!current_mount.empty() && 
                           (current_mount.back() == '\n' || current_mount.back() == ' ')) {
                        current_mount.pop_back();
                    }
                }
            }
            // Also check for "Mount(" format
            if (line.find("Mount(") != string::npos && !current_mount.empty()) {
                string unmount_cmd = "gio mount -u '" + current_mount + "' 2>/dev/null";
                int ret = system(unmount_cmd.c_str());
                if (ret == 0) {
                    cout << "  ✓ Unmounted via gio: " << current_mount << endl;
                    unmounted_something = true;
                }
                current_mount.clear();
            }
        }
        pclose(pipe);
        
        // If we found a mount but didn't unmount yet
        if (!current_mount.empty()) {
            string unmount_cmd = "gio mount -u '" + current_mount + "' 2>/dev/null";
            int ret = system(unmount_cmd.c_str());
            if (ret == 0) {
                cout << "  ✓ Unmounted via gio: " << current_mount << endl;
                unmounted_something = true;
            }
        }
    }
    
    // Method 2: Try legacy gvfs-mount command (older systems)
    pipe = popen("gvfs-mount -l 2>/dev/null | grep -i 'mtp://'", "r");
    if (pipe != nullptr) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            string line = buffer;
            size_t start = line.find("mtp://");
            if (start != string::npos) {
                size_t end = line.find_first_of(" \n\t", start);
                if (end == string::npos) end = line.length();
                string mount_path = line.substr(start, end - start);
                while (!mount_path.empty() && mount_path.back() == '\n') {
                    mount_path.pop_back();
                }
                if (!mount_path.empty()) {
                    string unmount_cmd = "gvfs-mount -u '" + mount_path + "' 2>/dev/null";
                    int ret = system(unmount_cmd.c_str());
                    if (ret == 0) {
                        cout << "  ✓ Unmounted via gvfs-mount: " << mount_path << endl;
                        unmounted_something = true;
                    }
                }
            }
        }
        pclose(pipe);
    }
    
    // Method 3: Direct unmount from GVFS directory
    string gvfs_dir = "/run/user/" + to_string(getuid()) + "/gvfs";
    string find_cmd = "find '" + gvfs_dir + "' -maxdepth 1 -type d -name '*mtp*' 2>/dev/null";
    pipe = popen(find_cmd.c_str(), "r");
    if (pipe != nullptr) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            string mount_path = buffer;
            while (!mount_path.empty() && mount_path.back() == '\n') {
                mount_path.pop_back();
            }
            if (!mount_path.empty()) {
                // Try gio first
                string unmount_cmd = "gio mount -u 'file://" + mount_path + "' 2>/dev/null";
                int ret = system(unmount_cmd.c_str());
                if (ret != 0) {
                    // Try fusermount
                    unmount_cmd = "fusermount -u '" + mount_path + "' 2>/dev/null";
                    ret = system(unmount_cmd.c_str());
                }
                if (ret == 0) {
                    cout << "  ✓ Unmounted: " << mount_path << endl;
                    unmounted_something = true;
                }
            }
        }
        pclose(pipe);
    }
    
    // Method 4: Kill gvfsd-mtp process (last resort)
    // This forces GVFS to release the MTP device
    pipe = popen("pgrep -f gvfsd-mtp 2>/dev/null", "r");
    if (pipe != nullptr) {
        char buffer[64];
        bool has_process = (fgets(buffer, sizeof(buffer), pipe) != nullptr);
        pclose(pipe);
        
        if (has_process) {
            cout << "  Killing gvfsd-mtp process to release device..." << endl;
            int ret = system("pkill -f gvfsd-mtp 2>/dev/null");
            if (ret == 0) {
                cout << "  ✓ Killed gvfsd-mtp process" << endl;
                unmounted_something = true;
            }
        }
    }
    
    // Wait for device to be fully released
    if (unmounted_something) {
        cout << "  Waiting for device to be released..." << endl;
        usleep(1500000); // 1.5 seconds
    } else {
        usleep(500000); // 0.5 seconds even if nothing unmounted
    }
    
    return true;
}

bool MTPHandler::connectToDevice(const string& device_name, bool auto_unmount) {
    if (raw_devices_.empty()) {
        if (!detectDevices()) {
            return false;
        }
    }
    
    // Try to unmount any existing MTP mounts before connecting
    if (auto_unmount) {
        cout << "Checking for existing MTP mounts..." << endl;
        unmountMTPDevices();
        cout << "Waiting for device to be released..." << endl;
        usleep(1000000); // Wait 1 second for device to be released
    }

    // If device_name is specified, try to find matching device
    // Otherwise, connect to first available device
    int device_index = 0;
    
    if (!device_name.empty()) {
        // Try to find device by name (we'll need to connect first to get name)
        // For now, just connect to first device
        device_index = 0;
    }

    if (device_index >= static_cast<int>(raw_devices_.size())) {
        setError("Device index out of range");
        return false;
    }

    // Use uncached version to avoid "cached device" errors when enumerating files
    // Try multiple times with increasing wait periods
    const int max_retries = 5;
    int retry_wait_ms[] = {500, 1000, 2000, 3000, 4000}; // Increasing wait times
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        if (attempt > 0) {
            cout << "Retry attempt " << attempt << "/" << (max_retries - 1) << "..." << endl;
        }
        
        device_ = LIBMTP_Open_Raw_Device_Uncached(&raw_devices_[device_index]);
        
        if (device_ != nullptr) {
            break; // Success!
        }
        
        // If connection failed and we have retries left, try unmounting again
        if (attempt < max_retries - 1) {
            cout << "Connection failed, device may still be busy. Releasing..." << endl;
            
            // Force unmount on retry
            unmountMTPDevices();
            
            // Wait before retry (increasing wait time each attempt)
            cout << "Waiting " << retry_wait_ms[attempt] << "ms before retry..." << endl;
            usleep(retry_wait_ms[attempt] * 1000);
            
            // Re-detect devices after unmount
            raw_devices_.clear();
            if (!detectDevices()) {
                setError("Failed to detect devices after unmount attempt");
                return false;
            }
        }
    }
    
    if (device_ == nullptr) {
        setError("Failed to open MTP device after " + to_string(max_retries) + 
                 " attempts. Try manually ejecting the device in your file manager.");
        return false;
    }

    // Get device information
    char* manufacturer = LIBMTP_Get_Manufacturername(device_);
    char* model = LIBMTP_Get_Modelname(device_);
    
    cout << "Connected to device: " 
         << (manufacturer ? manufacturer : "Unknown") << " "
         << (model ? model : "Unknown") << endl;

    if (manufacturer) free(manufacturer);
    if (model) free(model);

    return true;
}

void MTPHandler::disconnect(bool auto_unmount) {
    if (device_ != nullptr) {
        LIBMTP_Release_Device(device_);
        device_ = nullptr;
        
        // Optionally unmount after disconnecting
        if (auto_unmount) {
            unmountMTPDevices();
        }
    }
}

string MTPHandler::getDeviceName() const {
    if (!device_) return "";
    
    char* manufacturer = LIBMTP_Get_Manufacturername(device_);
    char* model = LIBMTP_Get_Modelname(device_);
    
    string name;
    if (manufacturer && model) {
        name = string(manufacturer) + " " + string(model);
    } else if (model) {
        name = string(model);
    } else if (manufacturer) {
        name = string(manufacturer);
    }
    
    if (manufacturer) free(manufacturer);
    if (model) free(model);
    
    return name;
}

string MTPHandler::getDeviceManufacturer() const {
    if (!device_) return "";
    
    char* manufacturer = LIBMTP_Get_Manufacturername(device_);
    string result = manufacturer ? string(manufacturer) : "";
    
    if (manufacturer) free(manufacturer);
    return result;
}

string MTPHandler::getDeviceModel() const {
    if (!device_) return "";
    
    char* model = LIBMTP_Get_Modelname(device_);
    string result = model ? string(model) : "";
    
    if (model) free(model);
    return result;
}

vector<DeviceStorageInfo> MTPHandler::getStorageInfo() const {
    vector<DeviceStorageInfo> storages;
    
    if (!device_) return storages;

    // Get storage - it populates device_->storage
    int ret = LIBMTP_Get_Storage(device_, LIBMTP_STORAGE_SORTBY_NOTSORTED);
    
    // Check if storage is already populated (some devices populate it on connect)
    if (device_->storage == nullptr && ret == 0) {
        // Try to access storage directly - sometimes it's already there
        // or try common storage IDs as fallback
        cerr << "Warning: LIBMTP_Get_Storage returned 0 but storage is null. "
             << "Trying common storage IDs..." << endl;
        
        // Try common storage IDs as fallback
        uint32_t common_storage_ids[] = {0x00010001, 0x00010002, 0x00010003, 0x00000001, 0x00000002};
        for (uint32_t storage_id : common_storage_ids) {
            // Try to get files from this storage to see if it exists
            LIBMTP_file_t* test_files = LIBMTP_Get_Files_And_Folders(device_, storage_id, 0);
            if (test_files != nullptr) {
                DeviceStorageInfo info;
                info.storage_id = storage_id;
                info.description = "Storage " + to_string(storage_id);
                info.max_capacity = 0; // Unknown
                info.free_space = 0;    // Unknown
                info.storage_type = 0;  // Unknown
                storages.push_back(info);
                LIBMTP_destroy_file_t(test_files);
                break; // Found at least one, use it
            }
        }
        return storages;
    }
    
    if (ret != 0) {
        cerr << "Warning: LIBMTP_Get_Storage returned error code: " << ret << endl;
        // Still try to use device_->storage if it exists
    }
    
    if (device_->storage == nullptr) {
        return storages;
    }

    LIBMTP_devicestorage_t* storage_entry = device_->storage;
    while (storage_entry != nullptr) {
        DeviceStorageInfo info;
        info.storage_id = storage_entry->id;
        info.description = storage_entry->StorageDescription ? 
                          string(storage_entry->StorageDescription) : "Unknown";
        info.max_capacity = storage_entry->MaxCapacity;
        info.free_space = storage_entry->FreeSpaceInBytes;
        info.storage_type = storage_entry->StorageType;
        
        storages.push_back(info);
        storage_entry = storage_entry->next;
    }

    // Storage list is managed by libmtp, no need to free manually
    return storages;
}

bool MTPHandler::isPhotoFile(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto endsWith = [](const string& str, const string& suffix) {
        return str.length() >= suffix.length() &&
               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    return endsWith(lower, ".jpg") || endsWith(lower, ".jpeg") || 
           endsWith(lower, ".png") || endsWith(lower, ".gif") ||
           endsWith(lower, ".bmp") || endsWith(lower, ".webp") ||
           endsWith(lower, ".heic") || endsWith(lower, ".heif");
}

bool MTPHandler::isVideoFile(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto endsWith = [](const string& str, const string& suffix) {
        return str.length() >= suffix.length() &&
               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    return endsWith(lower, ".mp4") || endsWith(lower, ".mov") ||
           endsWith(lower, ".avi") || endsWith(lower, ".mkv") ||
           endsWith(lower, ".m4v") || endsWith(lower, ".3gp") ||
           endsWith(lower, ".webm") || endsWith(lower, ".flv");
}

bool MTPHandler::isMediaFile(const string& filename) const {
    return isPhotoFile(filename) || isVideoFile(filename);
}

string MTPHandler::getMimeType(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    auto endsWith = [](const string& str, const string& suffix) {
        return str.length() >= suffix.length() &&
               str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    // Image MIME types
    if (endsWith(lower, ".jpg") || endsWith(lower, ".jpeg")) return "image/jpeg";
    if (endsWith(lower, ".png")) return "image/png";
    if (endsWith(lower, ".gif")) return "image/gif";
    if (endsWith(lower, ".bmp")) return "image/bmp";
    if (endsWith(lower, ".webp")) return "image/webp";
    if (endsWith(lower, ".heic") || endsWith(lower, ".heif")) return "image/heic";
    
    // Video MIME types
    if (endsWith(lower, ".mp4") || endsWith(lower, ".m4v")) return "video/mp4";
    if (endsWith(lower, ".mov")) return "video/quicktime";
    if (endsWith(lower, ".avi")) return "video/x-msvideo";
    if (endsWith(lower, ".mkv")) return "video/x-matroska";
    if (endsWith(lower, ".3gp")) return "video/3gpp";
    if (endsWith(lower, ".webm")) return "video/webm";
    if (endsWith(lower, ".flv")) return "video/x-flv";
    
    return "application/octet-stream"; // default
}

vector<MediaInfo> MTPHandler::enumerateDirectory(uint32_t storage_id, 
                                                  uint32_t parent_id,
                                                  const string& base_path) {
    vector<MediaInfo> photos;
    
    if (!device_) return photos;

    // Use LIBMTP_Get_Files_And_Folders which is available in all libmtp versions
    LIBMTP_file_t* files = LIBMTP_Get_Files_And_Folders(device_, storage_id, parent_id);
    
    if (files != nullptr) {
        LIBMTP_file_t* file = files;
        
        while (file != nullptr) {
            if (file->filetype == LIBMTP_FILETYPE_FOLDER) {
                string sub_path = base_path.empty() ? 
                    (file->filename ? string(file->filename) : "") :
                    base_path + "/" + (file->filename ? string(file->filename) : "");
                auto sub_photos = enumerateDirectory(storage_id, file->item_id, sub_path);
                photos.insert(photos.end(), sub_photos.begin(), sub_photos.end());
            } else {
                string filename = file->filename ? string(file->filename) : "";
                // Check for photos, videos, or unknown file types that might be media
                if (file->filetype == LIBMTP_FILETYPE_JPEG || 
                    file->filetype == LIBMTP_FILETYPE_PNG ||
                    file->filetype == LIBMTP_FILETYPE_GIF ||
                    file->filetype == LIBMTP_FILETYPE_BMP ||
                    file->filetype == LIBMTP_FILETYPE_MP4 ||
                    file->filetype == LIBMTP_FILETYPE_AVI ||
                    file->filetype == LIBMTP_FILETYPE_UNDEF_VIDEO ||
                    isMediaFile(filename)) {
                    MediaInfo info;
                    info.object_id = file->item_id;
                    info.filename = filename;
                    info.path = base_path.empty() ? filename : base_path + "/" + filename;
                    info.file_size = file->filesize;
                    info.modification_date = file->modificationdate;
                    info.mime_type = getMimeType(filename);
                    photos.push_back(info);
                }
            }
            file = file->next;
        }

        LIBMTP_destroy_file_t(files);
    }

    return photos;
}

vector<MediaInfo> MTPHandler::enumerateMedia(const string& directory_path) {
    vector<MediaInfo> photos;
    
    if (!device_) {
        setError("Device not connected");
        return photos;
    }

    // Get storage information
    auto storages = getStorageInfo();
    
    // If no storage found, try common storage IDs directly
    if (storages.empty()) {
        cerr << "Warning: No storage info found, trying common storage IDs..." << endl;
        uint32_t common_storage_ids[] = {0x00010001, 0x00010002, 0x00010003, 0x00000001, 0x00000002};
        for (uint32_t storage_id : common_storage_ids) {
            auto found_photos = enumerateDirectory(storage_id, 0, "");
            if (!found_photos.empty()) {
                // Found photos, add this storage ID
                DeviceStorageInfo info;
                info.storage_id = storage_id;
                info.description = "Storage " + to_string(storage_id);
                info.max_capacity = 0;
                info.free_space = 0;
                info.storage_type = 0;
                storages.push_back(info);
                photos.insert(photos.end(), found_photos.begin(), found_photos.end());
                break; // Use first working storage
            }
        }
        
        if (photos.empty()) {
            setError("No storage found on device and common storage IDs failed");
            return photos;
        }
    }

    // Default paths to search if none specified
    // Search broadly to find all camera roll content
    vector<string> search_paths;
    if (directory_path.empty()) {
        // Don't filter by path - enumerate all and filter by file type
        // This will capture everything in camera roll and other locations
        search_paths.clear(); // Empty means search everything
    } else {
        search_paths.push_back(directory_path);
    }

    // Try to find photos in each storage
    // Enumerate once per storage to avoid duplicates
    for (const auto& storage : storages) {
        cout << "  Enumerating storage ID " << storage.storage_id << "..." << endl;
        
        // Try different parent IDs - Android devices may use different root IDs
        vector<uint32_t> root_ids_to_try;
        
        // Add device default folders if available (these are usually the best starting points)
        if (device_->default_picture_folder != 0) {
            root_ids_to_try.push_back(device_->default_picture_folder);
        }
        if (device_->default_music_folder != 0) {
            root_ids_to_try.push_back(device_->default_music_folder);
        }
        
        // Add common root IDs as fallback
        root_ids_to_try.insert(root_ids_to_try.end(), {
            0,              // Standard root
            0xFFFFFFFF,     // Common alternative root (uint32_t max)
            0x00000001      // Some devices use this
        });
        
        vector<MediaInfo> found_photos;
        
        // Try each root ID until we find photos
        for (uint32_t root_id : root_ids_to_try) {
            auto photos_from_root = enumerateDirectory(storage.storage_id, root_id, "");
            if (!photos_from_root.empty()) {
                found_photos.insert(found_photos.end(), 
                                   photos_from_root.begin(), 
                                   photos_from_root.end());
                break; // Found photos, use this root
            }
        }
        
        // If still nothing, try to search for DCIM folder by name using all root IDs
        if (found_photos.empty()) {
            for (uint32_t root_id : root_ids_to_try) {
                LIBMTP_file_t* files = LIBMTP_Get_Files_And_Folders(device_, storage.storage_id, root_id);
                if (files != nullptr) {
                    LIBMTP_file_t* file = files;
                    while (file != nullptr) {
                        if (file->filetype == LIBMTP_FILETYPE_FOLDER) {
                            string folder_name = file->filename ? string(file->filename) : "";
                            string lower_name = folder_name;
                            transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
                            
                            // Enumerate all folders to get everything (not just DCIM)
                            // This ensures we capture camera roll content from all locations
                            auto folder_photos = enumerateDirectory(storage.storage_id, file->item_id, folder_name);
                            found_photos.insert(found_photos.end(), 
                                               folder_photos.begin(), 
                                               folder_photos.end());
                        }
                        file = file->next;
                    }
                    LIBMTP_destroy_file_t(files);
                    if (!found_photos.empty()) break;
                }
            }
        }
        
        cout << "  Found " << found_photos.size() << " total files during enumeration" << endl;
        
        // Filter by path if specified
        if (!directory_path.empty()) {
            for (auto& photo : found_photos) {
                if (photo.path.find(directory_path) != string::npos) {
                    photos.push_back(photo);
                }
            }
        } else {
            // Filter to only include photos from common directories
            // But if no photos match, include all photos as fallback
            bool found_matching = false;
            for (auto& photo : found_photos) {
                string lower_path = photo.path;
                transform(lower_path.begin(), lower_path.end(), 
                         lower_path.begin(), ::tolower);
                
                if (lower_path.find("dcim") != string::npos ||
                    lower_path.find("camera") != string::npos ||
                    lower_path.find("pictures") != string::npos) {
                    photos.push_back(photo);
                    found_matching = true;
                }
            }
            
            // If no photos matched the filter, include all photos found
            if (!found_matching && !found_photos.empty()) {
                cout << "  No photos in DCIM/Camera/Pictures, including all " 
                     << found_photos.size() << " photos found" << endl;
                photos.insert(photos.end(), found_photos.begin(), found_photos.end());
            }
        }
    }

    return photos;
}

// Callback structure for file reading
struct FileReadData {
    vector<uint8_t>* buffer;
    size_t offset;
};

static uint16_t file_read_callback(void* params, void* priv, uint32_t sendlen, 
                                   unsigned char* data, uint32_t* putlen) {
    (void)params; // Unused
    FileReadData* read_data = static_cast<FileReadData*>(priv);
    
    if (read_data->offset + sendlen > read_data->buffer->size()) {
        *putlen = 0;
        return LIBMTP_HANDLER_RETURN_ERROR;
    }
    
    memcpy(read_data->buffer->data() + read_data->offset, data, sendlen);
    read_data->offset += sendlen;
    *putlen = sendlen;
    return LIBMTP_HANDLER_RETURN_OK;
}

bool MTPHandler::readFile(uint32_t object_id, vector<uint8_t>& data) {
    if (!device_) {
        setError("Device not connected");
        return false;
    }

    LIBMTP_file_t* file = LIBMTP_Get_Filemetadata(device_, object_id);
    if (file == nullptr) {
        setError("Failed to get file metadata");
        return false;
    }

    uint64_t expected_size = file->filesize;
    data.resize(expected_size);
    
    FileReadData read_data;
    read_data.buffer = &data;
    read_data.offset = 0;
    
    int ret = LIBMTP_Get_File_To_Handler(device_, object_id, 
                                         file_read_callback,
                                         &read_data,
                                         nullptr,  // No progress callback
                                         nullptr); // No progress data

    LIBMTP_destroy_file_t(file);

    if (ret != 0 || read_data.offset != expected_size) {
        setError("Failed to read file data");
        return false;
    }

    return true;
}

bool MTPHandler::deleteFile(uint32_t object_id) {
    if (!device_) {
        setError("Device not connected");
        return false;
    }

    int ret = LIBMTP_Delete_Object(device_, object_id);
    if (ret != 0) {
        setError("Failed to delete file");
        return false;
    }

    return true;
}

bool MTPHandler::fileExists(uint32_t object_id) {
    if (!device_) return false;
    
    LIBMTP_file_t* file = LIBMTP_Get_Filemetadata(device_, object_id);
    bool exists = (file != nullptr);
    
    if (file) {
        LIBMTP_destroy_file_t(file);
    }
    
    return exists;
}

uint32_t MTPHandler::findObjectByPath(const string& path) {
    // This is a simplified implementation
    // In a full implementation, we'd traverse the directory tree
    // For now, we'll use enumerateMedia and search
    auto photos = enumerateMedia();
    
    for (const auto& photo : photos) {
        if (photo.path == path) {
            return photo.object_id;
        }
        // Check if path ends with the search path
        if (photo.path.length() >= path.length() &&
            photo.path.compare(photo.path.length() - path.length(), path.length(), path) == 0) {
            return photo.object_id;
        }
    }
    
    return 0; // Not found
}

vector<string> MTPHandler::listDirectories(const string& path) {
    (void)path;  // Unused for now
    vector<string> directories;
    // Implementation would traverse directory structure
    // For Phase 1, this is a placeholder
    return directories;
}
