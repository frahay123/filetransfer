#ifndef DEVICE_HANDLER_H
#define DEVICE_HANDLER_H

#include <string>
#include <vector>
#include <cstdint>

using namespace std;

/**
 * Represents a photo/video file on the mobile device
 */
struct MediaInfo {
    uint32_t object_id;
    string filename;
    string path;
    uint64_t file_size;
    uint64_t modification_date;
    string mime_type;
};

/**
 * Represents a storage location on the mobile device
 */
struct DeviceStorageInfo {
    uint32_t storage_id;
    string description;
    uint64_t max_capacity;
    uint64_t free_space;
    uint16_t storage_type;
};

/**
 * Device type enumeration
 */
enum class DeviceType {
    UNKNOWN,
    ANDROID,
    IOS
};

/**
 * Abstract base class for device handlers
 * Provides a unified interface for Android (MTP) and iOS (libimobiledevice) devices
 */
class DeviceHandler {
public:
    virtual ~DeviceHandler() = default;

    // Device management
    virtual bool detectDevices() = 0;
    virtual bool connectToDevice(const string& device_name = "", bool auto_unmount = true) = 0;
    virtual void disconnect(bool auto_unmount = true) = 0;
    virtual bool isConnected() const = 0;

    // Device information
    virtual string getDeviceName() const = 0;
    virtual string getDeviceManufacturer() const = 0;
    virtual string getDeviceModel() const = 0;
    virtual DeviceType getDeviceType() const = 0;
    virtual vector<DeviceStorageInfo> getStorageInfo() const = 0;

    // File operations
    virtual vector<MediaInfo> enumerateMedia(const string& directory_path = "") = 0;
    virtual bool readFile(uint32_t object_id, vector<uint8_t>& data) = 0;
    virtual bool fileExists(uint32_t object_id) = 0;

    // Error handling
    virtual string getLastError() const = 0;

    // Static utility to get device type name
    static string getDeviceTypeName(DeviceType type) {
        switch (type) {
            case DeviceType::ANDROID: return "Android";
            case DeviceType::IOS: return "iOS";
            default: return "Unknown";
        }
    }
};

#endif // DEVICE_HANDLER_H
