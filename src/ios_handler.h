#ifndef IOS_HANDLER_H
#define IOS_HANDLER_H

#ifdef ENABLE_IOS

#include "device_handler.h"
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>
#include <string>
#include <vector>

using namespace std;

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
    bool connectToDevice(const string& device_name = "", bool auto_unmount = true) override;
    void disconnect(bool auto_unmount = true) override;
    bool isConnected() const override { return device_ != nullptr && afc_ != nullptr; }
    
    // Device information
    string getDeviceName() const override;
    string getDeviceManufacturer() const override;
    string getDeviceModel() const override;
    DeviceType getDeviceType() const override { return DeviceType::IOS; }
    vector<DeviceStorageInfo> getStorageInfo() const override;

    // File operations
    vector<MediaInfo> enumerateMedia(const string& directory_path = "") override;
    bool readFile(uint32_t object_id, vector<uint8_t>& data) override;
    bool fileExists(uint32_t object_id) override;

    // Error handling
    string getLastError() const override { return last_error_; }

    // iOS-specific methods
    bool readFileByPath(const string& path, vector<uint8_t>& data);

private:
    idevice_t device_;
    lockdownd_client_t lockdown_;
    afc_client_t afc_;
    vector<string> device_udids_;
    string last_error_;
    
    // File path to object ID mapping (since iOS doesn't use object IDs)
    vector<string> file_paths_;
    
    // Device info cache
    string device_name_;
    string device_model_;
    string product_type_;
    
    // Helper functions
    void setError(const string& error);
    vector<MediaInfo> enumerateDirectory(const string& path, const string& base_path);
    bool isMediaFile(const string& filename) const;
    bool isPhotoFile(const string& filename) const;
    bool isVideoFile(const string& filename) const;
    string getMimeType(const string& filename) const;
    
    // Get device info from lockdown
    string getDeviceValue(const string& key) const;
};

#endif // ENABLE_IOS

#endif // IOS_HANDLER_H
