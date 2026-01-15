#ifndef WPD_HANDLER_H
#define WPD_HANDLER_H

#ifdef _WIN32

#include "device_handler.h"
#include <string>
#include <vector>
#include <memory>

// Forward declarations for Windows types
struct IPortableDevice;
struct IPortableDeviceManager;
struct IPortableDeviceContent;
struct IPortableDeviceProperties;

class WPDHandler : public DeviceHandler {
public:
    WPDHandler();
    ~WPDHandler() override;

    // DeviceHandler interface implementation
    bool detectDevices() override;
    bool connectToDevice(const std::string& device_name = "", bool auto_unmount = true) override;
    void disconnect(bool auto_unmount = true) override;
    bool isConnected() const override;
    DeviceType getDeviceType() const override { return DeviceType::ANDROID; }

    std::string getDeviceName() const override;
    std::string getDeviceManufacturer() const override;
    std::string getDeviceModel() const override;
    std::vector<DeviceStorageInfo> getStorageInfo() const override;

    std::vector<MediaInfo> enumerateMedia(const std::string& directory_path = "") override;
    bool readFile(uint32_t object_id, std::vector<uint8_t>& data) override;
    bool deleteFile(uint32_t object_id) override;
    bool fileExists(uint32_t object_id) override;

    std::string getLastError() const override { return last_error_; }

private:
    bool initializeCOM();
    void uninitializeCOM();
    std::wstring stringToWide(const std::string& str);
    std::string wideToString(const std::wstring& wstr);
    
    void enumerateContent(const std::wstring& parent_id, std::vector<MediaInfo>& media);
    bool isMediaFile(const std::wstring& filename);
    std::string getMimeType(const std::wstring& filename);

    IPortableDeviceManager* device_manager_;
    IPortableDevice* device_;
    IPortableDeviceContent* content_;
    
    std::wstring device_id_;
    std::string device_name_;
    std::string device_manufacturer_;
    std::string device_model_;
    
    bool com_initialized_;
    bool connected_;
    
    // Map object IDs (string) to numeric IDs for the interface
    std::vector<std::wstring> object_id_map_;
};

#endif // _WIN32

#endif // WPD_HANDLER_H
