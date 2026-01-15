#ifndef IOS_HANDLER_H
#define IOS_HANDLER_H

#ifdef ENABLE_IOS

#include "device_handler.h"
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>
#include <string>
#include <vector>

/**
 * iOS Handler class for communicating with iPhone/iPad devices
 * Uses libimobiledevice for communication via AFC (Apple File Conduit) protocol
 */
class iOSHandler : public DeviceHandler {
public:
    iOSHandler();
    ~iOSHandler() override;

    // DeviceHandler interface implementation
    bool detectDevices() override;
    bool connectToDevice(const std::string& device_name = "", bool auto_unmount = true) override;
    void disconnect(bool auto_unmount = true) override;
    bool isConnected() const override { return device_ != nullptr && afc_ != nullptr; }
    
    // Device information
    std::string getDeviceName() const override;
    std::string getDeviceManufacturer() const override;
    std::string getDeviceModel() const override;
    DeviceType getDeviceType() const override { return DeviceType::IOS; }
    std::vector<DeviceStorageInfo> getStorageInfo() const override;

    // File operations
    std::vector<MediaInfo> enumerateMedia(const std::string& directory_path = "") override;
    bool readFile(uint32_t object_id, std::vector<uint8_t>& data) override;
    bool fileExists(uint32_t object_id) override;

    // Error handling
    std::string getLastError() const override { return last_error_; }

    // iOS-specific methods
    bool readFileByPath(const std::string& path, std::vector<uint8_t>& data);

private:
    idevice_t device_;
    lockdownd_client_t lockdown_;
    afc_client_t afc_;
    std::vector<std::string> device_udids_;
    std::string last_error_;
    
    // File path to object ID mapping (since iOS doesn't use object IDs)
    std::vector<std::string> file_paths_;
    
    // Device info cache
    std::string device_name_;
    std::string device_model_;
    std::string product_type_;
    
    // Helper functions
    void setError(const std::string& error);
    std::vector<MediaInfo> enumerateDirectory(const std::string& path, const std::string& base_path);
    bool isMediaFile(const std::string& filename) const;
    bool isPhotoFile(const std::string& filename) const;
    bool isVideoFile(const std::string& filename) const;
    std::string getMimeType(const std::string& filename) const;
    
    // Get device info from lockdown
    std::string getDeviceValue(const std::string& key) const;
};

#endif // ENABLE_IOS

#endif // IOS_HANDLER_H
