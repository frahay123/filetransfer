#ifdef ENABLE_IOS

#include "ios_handler.h"
#include "utils.h"
#include <iostream>
#include <algorithm>
#include <cstring>

using namespace std;

iOSHandler::iOSHandler() 
    : device_(nullptr), lockdown_(nullptr), afc_(nullptr) {
}

iOSHandler::~iOSHandler() {
    disconnect();
}

void iOSHandler::setError(const string& error) {
    last_error_ = error;
    cerr << "iOS Error: " << error << endl;
}

bool iOSHandler::detectDevices() {
    device_udids_.clear();
    last_error_.clear();
    
    char** udid_list = nullptr;
    int device_count = 0;
    
    idevice_error_t ret = idevice_get_device_list(&udid_list, &device_count);
    
    if (ret != IDEVICE_E_SUCCESS || device_count == 0) {
        setError("No iOS devices detected. Make sure your iPhone/iPad is connected and trusted.");
        if (udid_list) {
            idevice_device_list_free(udid_list);
        }
        return false;
    }
    
    // Store device UDIDs
    for (int i = 0; i < device_count; i++) {
        device_udids_.push_back(string(udid_list[i]));
        cout << "Found iOS device: " << udid_list[i] << endl;
    }
    
    idevice_device_list_free(udid_list);
    return true;
}

bool iOSHandler::connectToDevice(const string& device_name, bool auto_unmount) {
    (void)auto_unmount; // iOS doesn't typically need unmounting like MTP
    
    if (device_udids_.empty()) {
        if (!detectDevices()) {
            return false;
        }
    }
    
    // Use first device or find by name/UDID
    string target_udid = device_udids_[0];
    if (!device_name.empty()) {
        for (const auto& udid : device_udids_) {
            if (udid.find(device_name) != string::npos) {
                target_udid = udid;
                break;
            }
        }
    }
    
    // Connect to device
    idevice_error_t ret = idevice_new(&device_, target_udid.c_str());
    if (ret != IDEVICE_E_SUCCESS) {
        setError("Failed to connect to iOS device: " + target_udid);
        return false;
    }
    
    // Connect to lockdown service
    lockdownd_error_t lock_ret = lockdownd_client_new_with_handshake(device_, &lockdown_, "photo_transfer");
    if (lock_ret != LOCKDOWN_E_SUCCESS) {
        setError("Failed to connect to lockdown service. Make sure the device is unlocked and trusted.");
        idevice_free(device_);
        device_ = nullptr;
        return false;
    }
    
    // Get device info
    device_name_ = getDeviceValue("DeviceName");
    device_model_ = getDeviceValue("ProductType");
    product_type_ = getDeviceValue("ProductType");
    
    cout << "Connected to iOS device: " << device_name_ << " (" << device_model_ << ")" << endl;
    
    // Start AFC service for file access
    lockdownd_service_descriptor_t service = nullptr;
    lock_ret = lockdownd_start_service(lockdown_, "com.apple.afc", &service);
    if (lock_ret != LOCKDOWN_E_SUCCESS || service == nullptr) {
        setError("Failed to start AFC service");
        lockdownd_client_free(lockdown_);
        lockdown_ = nullptr;
        idevice_free(device_);
        device_ = nullptr;
        return false;
    }
    
    afc_error_t afc_ret = afc_client_new(device_, service, &afc_);
    lockdownd_service_descriptor_free(service);
    
    if (afc_ret != AFC_E_SUCCESS) {
        setError("Failed to create AFC client");
        lockdownd_client_free(lockdown_);
        lockdown_ = nullptr;
        idevice_free(device_);
        device_ = nullptr;
        return false;
    }
    
    return true;
}

void iOSHandler::disconnect(bool auto_unmount) {
    (void)auto_unmount;
    
    if (afc_) {
        afc_client_free(afc_);
        afc_ = nullptr;
    }
    
    if (lockdown_) {
        lockdownd_client_free(lockdown_);
        lockdown_ = nullptr;
    }
    
    if (device_) {
        idevice_free(device_);
        device_ = nullptr;
    }
    
    file_paths_.clear();
}

string iOSHandler::getDeviceName() const {
    if (!device_name_.empty()) {
        return device_name_;
    }
    return "iOS Device";
}

string iOSHandler::getDeviceManufacturer() const {
    return "Apple";
}

string iOSHandler::getDeviceModel() const {
    if (!device_model_.empty()) {
        return device_model_;
    }
    return "iPhone/iPad";
}

string iOSHandler::getDeviceValue(const string& key) const {
    if (!lockdown_) return "";
    
    plist_t value = nullptr;
    lockdownd_error_t ret = lockdownd_get_value(lockdown_, nullptr, key.c_str(), &value);
    
    if (ret == LOCKDOWN_E_SUCCESS && value) {
        char* str_value = nullptr;
        plist_get_string_val(value, &str_value);
        plist_free(value);
        
        if (str_value) {
            string result(str_value);
            free(str_value);
            return result;
        }
    }
    
    return "";
}

vector<DeviceStorageInfo> iOSHandler::getStorageInfo() const {
    vector<DeviceStorageInfo> storages;
    
    if (!afc_) return storages;
    
    // Get device info for storage
    char** info = nullptr;
    afc_error_t ret = afc_get_device_info(afc_, &info);
    
    if (ret == AFC_E_SUCCESS && info) {
        DeviceStorageInfo storage;
        storage.storage_id = 1;
        storage.description = "iOS Media Storage";
        storage.max_capacity = 0;
        storage.free_space = 0;
        storage.storage_type = 0;
        
        // Parse device info
        for (int i = 0; info[i]; i += 2) {
            if (info[i + 1] == nullptr) break;
            
            string key = info[i];
            string value = info[i + 1];
            
            if (key == "FSTotalBytes") {
                storage.max_capacity = stoull(value);
            } else if (key == "FSFreeBytes") {
                storage.free_space = stoull(value);
            }
        }
        
        afc_dictionary_free(info);
        storages.push_back(storage);
    }
    
    return storages;
}

vector<MediaInfo> iOSHandler::enumerateMedia(const string& directory_path) {
    file_paths_.clear();
    
    if (!afc_) {
        setError("Not connected to device");
        return {};
    }
    
    string search_path = directory_path.empty() ? "/DCIM" : directory_path;
    cout << "Enumerating media from: " << search_path << endl;
    
    return enumerateDirectory(search_path, "");
}

vector<MediaInfo> iOSHandler::enumerateDirectory(const string& path, const string& base_path) {
    vector<MediaInfo> media;
    
    if (!afc_) return media;
    
    char** dir_list = nullptr;
    afc_error_t ret = afc_read_directory(afc_, path.c_str(), &dir_list);
    
    if (ret != AFC_E_SUCCESS || !dir_list) {
        return media;
    }
    
    for (int i = 0; dir_list[i]; i++) {
        string name = dir_list[i];
        
        // Skip . and ..
        if (name == "." || name == "..") continue;
        
        string full_path = path + "/" + name;
        string relative_path = base_path.empty() ? name : base_path + "/" + name;
        
        // Get file info
        char** file_info = nullptr;
        ret = afc_get_file_info(afc_, full_path.c_str(), &file_info);
        
        if (ret == AFC_E_SUCCESS && file_info) {
            bool is_dir = false;
            uint64_t file_size = 0;
            uint64_t mod_time = 0;
            
            for (int j = 0; file_info[j]; j += 2) {
                if (file_info[j + 1] == nullptr) break;
                
                string key = file_info[j];
                string value = file_info[j + 1];
                
                if (key == "st_ifmt" && value == "S_IFDIR") {
                    is_dir = true;
                } else if (key == "st_size") {
                    file_size = stoull(value);
                } else if (key == "st_mtime") {
                    // Convert nanoseconds to seconds
                    mod_time = stoull(value) / 1000000000ULL;
                }
            }
            
            afc_dictionary_free(file_info);
            
            if (is_dir) {
                // Recurse into subdirectory
                auto sub_media = enumerateDirectory(full_path, relative_path);
                media.insert(media.end(), sub_media.begin(), sub_media.end());
            } else if (isMediaFile(name)) {
                // Add media file
                MediaInfo info;
                info.object_id = file_paths_.size(); // Use index as object ID
                info.filename = name;
                info.path = full_path; // Store full path for reading
                info.file_size = file_size;
                info.modification_date = mod_time;
                info.mime_type = getMimeType(name);
                
                file_paths_.push_back(full_path);
                media.push_back(info);
            }
        }
    }
    
    afc_dictionary_free(dir_list);
    return media;
}

bool iOSHandler::readFile(uint32_t object_id, vector<uint8_t>& data) {
    if (object_id >= file_paths_.size()) {
        setError("Invalid object ID");
        return false;
    }
    
    return readFileByPath(file_paths_[object_id], data);
}

bool iOSHandler::readFileByPath(const string& path, vector<uint8_t>& data) {
    if (!afc_) {
        setError("Not connected to device");
        return false;
    }
    
    uint64_t handle = 0;
    afc_error_t ret = afc_file_open(afc_, path.c_str(), AFC_FOPEN_RDONLY, &handle);
    
    if (ret != AFC_E_SUCCESS) {
        setError("Failed to open file: " + path);
        return false;
    }
    
    // Get file size
    char** file_info = nullptr;
    ret = afc_get_file_info(afc_, path.c_str(), &file_info);
    
    uint64_t file_size = 0;
    if (ret == AFC_E_SUCCESS && file_info) {
        for (int i = 0; file_info[i]; i += 2) {
            if (file_info[i + 1] && string(file_info[i]) == "st_size") {
                file_size = stoull(file_info[i + 1]);
                break;
            }
        }
        afc_dictionary_free(file_info);
    }
    
    if (file_size == 0) {
        afc_file_close(afc_, handle);
        setError("File size is 0 or could not be determined");
        return false;
    }
    
    // Read file in chunks
    data.resize(file_size);
    uint32_t bytes_read = 0;
    uint64_t total_read = 0;
    
    while (total_read < file_size) {
        uint32_t to_read = min((uint64_t)(1024 * 1024), file_size - total_read); // 1MB chunks
        ret = afc_file_read(afc_, handle, reinterpret_cast<char*>(data.data() + total_read), to_read, &bytes_read);
        
        if (ret != AFC_E_SUCCESS || bytes_read == 0) {
            break;
        }
        
        total_read += bytes_read;
    }
    
    afc_file_close(afc_, handle);
    
    if (total_read != file_size) {
        data.resize(total_read);
    }
    
    return total_read > 0;
}

bool iOSHandler::fileExists(uint32_t object_id) {
    if (object_id >= file_paths_.size()) {
        return false;
    }
    
    if (!afc_) return false;
    
    char** file_info = nullptr;
    afc_error_t ret = afc_get_file_info(afc_, file_paths_[object_id].c_str(), &file_info);
    
    if (ret == AFC_E_SUCCESS && file_info) {
        afc_dictionary_free(file_info);
        return true;
    }
    
    return false;
}

// C++17 compatible ends_with helper
static bool endsWith(const string& str, const string& suffix) {
    return str.length() >= suffix.length() &&
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool iOSHandler::isPhotoFile(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return endsWith(lower, ".jpg") || endsWith(lower, ".jpeg") ||
           endsWith(lower, ".png") || endsWith(lower, ".gif") ||
           endsWith(lower, ".bmp") || endsWith(lower, ".webp") ||
           endsWith(lower, ".heic") || endsWith(lower, ".heif") ||
           endsWith(lower, ".dng") || endsWith(lower, ".raw");
}

bool iOSHandler::isVideoFile(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    return endsWith(lower, ".mp4") || endsWith(lower, ".mov") ||
           endsWith(lower, ".m4v") || endsWith(lower, ".avi") ||
           endsWith(lower, ".mkv") || endsWith(lower, ".3gp") ||
           endsWith(lower, ".webm");
}

bool iOSHandler::isMediaFile(const string& filename) const {
    return isPhotoFile(filename) || isVideoFile(filename);
}

string iOSHandler::getMimeType(const string& filename) const {
    string lower = filename;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Image MIME types
    if (endsWith(lower, ".jpg") || endsWith(lower, ".jpeg")) return "image/jpeg";
    if (endsWith(lower, ".png")) return "image/png";
    if (endsWith(lower, ".gif")) return "image/gif";
    if (endsWith(lower, ".bmp")) return "image/bmp";
    if (endsWith(lower, ".webp")) return "image/webp";
    if (endsWith(lower, ".heic") || endsWith(lower, ".heif")) return "image/heic";
    if (endsWith(lower, ".dng")) return "image/x-adobe-dng";
    
    // Video MIME types
    if (endsWith(lower, ".mp4") || endsWith(lower, ".m4v")) return "video/mp4";
    if (endsWith(lower, ".mov")) return "video/quicktime";
    if (endsWith(lower, ".avi")) return "video/x-msvideo";
    if (endsWith(lower, ".mkv")) return "video/x-matroska";
    if (endsWith(lower, ".3gp")) return "video/3gpp";
    if (endsWith(lower, ".webm")) return "video/webm";
    
    return "application/octet-stream";
}

#endif // ENABLE_IOS
