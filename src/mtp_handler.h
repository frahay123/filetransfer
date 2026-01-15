#ifndef MTP_HANDLER_H
#define MTP_HANDLER_H

#include "device_handler.h"
#include <libmtp.h>
#include <string>
#include <vector>
#include <memory>

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
    bool connectToDevice(const std::string& device_name = "", bool auto_unmount = true) override;
    void disconnect(bool auto_unmount = true) override;
    bool isConnected() const override { return device_ != nullptr; }
    
    // Device information
    std::string getDeviceName() const override;
    std::string getDeviceManufacturer() const override;
    std::string getDeviceModel() const override;
    DeviceType getDeviceType() const override { return DeviceType::ANDROID; }
    std::vector<DeviceStorageInfo> getStorageInfo() const override;

    // File operations
    std::vector<MediaInfo> enumerateMedia(const std::string& directory_path = "") override;
    bool readFile(uint32_t object_id, std::vector<uint8_t>& data) override;
    bool fileExists(uint32_t object_id) override;

    // Error handling
    std::string getLastError() const override { return last_error_; }

    // MTP-specific methods (not part of DeviceHandler interface)
    static bool unmountMTPDevices();
    bool deleteFile(uint32_t object_id);
    std::vector<std::string> listDirectories(const std::string& path);
    uint32_t findObjectByPath(const std::string& path);

    // Legacy method alias for backward compatibility
    std::vector<PhotoInfo> enumeratePhotos(const std::string& directory_path = "") {
        return enumerateMedia(directory_path);
    }

private:
    LIBMTP_mtpdevice_t* device_;
    std::vector<LIBMTP_raw_device_t> raw_devices_;
    std::string last_error_;

    // Helper functions
    void setError(const std::string& error);
    bool findStorageForPath(const std::string& path, uint32_t& storage_id);
    std::vector<MediaInfo> enumerateDirectory(uint32_t storage_id, uint32_t parent_id, 
                                         const std::string& base_path);
    bool isPhotoFile(const std::string& filename) const;
    bool isVideoFile(const std::string& filename) const;
    bool isMediaFile(const std::string& filename) const;
    std::string getMimeType(const std::string& filename) const;
};

#endif // MTP_HANDLER_H
