#ifndef MTP_HANDLER_H
#define MTP_HANDLER_H

#include "device_handler.h"
#include <libmtp.h>
#include <string>
#include <vector>
#include <memory>

using namespace std;

// Type aliases for backward compatibility
using PhotoInfo = MediaInfo;
using StorageInfo = DeviceStorageInfo;

/**
 * MTP Handler class for communicating with Android devices
 * Implements DeviceHandler interface for unified device access
 */
class MTPHandler : public DeviceHandler {
public:
    MTPHandler();
    ~MTPHandler() override;

    // DeviceHandler interface implementation
    bool detectDevices() override;
    bool connectToDevice(const string& device_name = "", bool auto_unmount = true) override;
    void disconnect(bool auto_unmount = true) override;
    bool isConnected() const override { return device_ != nullptr; }
    
    // Device information
    string getDeviceName() const override;
    string getDeviceManufacturer() const override;
    string getDeviceModel() const override;
    DeviceType getDeviceType() const override { return DeviceType::ANDROID; }
    vector<DeviceStorageInfo> getStorageInfo() const override;

    // File operations
    vector<MediaInfo> enumerateMedia(const string& directory_path = "") override;
    bool readFile(uint32_t object_id, vector<uint8_t>& data) override;
    bool fileExists(uint32_t object_id) override;

    // Error handling
    string getLastError() const override { return last_error_; }

    // MTP-specific methods (not part of DeviceHandler interface)
    static bool unmountMTPDevices();
    bool deleteFile(uint32_t object_id);
    vector<string> listDirectories(const string& path);
    uint32_t findObjectByPath(const string& path);

    // Legacy method alias for backward compatibility
    vector<PhotoInfo> enumeratePhotos(const string& directory_path = "") {
        return enumerateMedia(directory_path);
    }

private:
    LIBMTP_mtpdevice_t* device_;
    vector<LIBMTP_raw_device_t> raw_devices_;
    string last_error_;

    // Helper functions
    void setError(const string& error);
    bool findStorageForPath(const string& path, uint32_t& storage_id);
    vector<MediaInfo> enumerateDirectory(uint32_t storage_id, uint32_t parent_id, 
                                         const string& base_path);
    bool isPhotoFile(const string& filename) const;
    bool isVideoFile(const string& filename) const;
    bool isMediaFile(const string& filename) const;
    string getMimeType(const string& filename) const;
};

#endif // MTP_HANDLER_H
