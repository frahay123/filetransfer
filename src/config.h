#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

/**
 * Configuration manager for storing user preferences
 * Supports platform-specific config file locations:
 * - Linux: ~/.config/photo_transfer/config.json
 * - macOS: ~/Library/Application Support/photo_transfer/config.json
 * - Windows: %APPDATA%\photo_transfer\config.json
 */
class Config {
public:
    Config();
    
    // Load/Save configuration
    bool load();
    bool save();
    bool reset();
    
    // Configuration properties
    std::string getDestinationFolder() const { return destination_folder_; }
    void setDestinationFolder(const std::string& folder) { destination_folder_ = folder; }
    
    std::string getDeviceType() const { return device_type_; }
    void setDeviceType(const std::string& type) { device_type_ = type; }
    
    std::string getTransferMode() const { return transfer_mode_; }
    void setTransferMode(const std::string& mode) { transfer_mode_ = mode; }
    
    bool getRememberSettings() const { return remember_settings_; }
    void setRememberSettings(bool remember) { remember_settings_ = remember; }
    
    // Check if this is first run (no config file exists)
    bool isFirstRun() const { return first_run_; }
    
    // Get config file path
    std::string getConfigPath() const;
    
    // Default values
    static std::string getDefaultDestination();
    
private:
    std::string destination_folder_;
    std::string device_type_;
    std::string transfer_mode_;
    bool remember_settings_;
    bool first_run_;
    
    // Get platform-specific config directory
    std::string getConfigDirectory() const;
    
    // Simple JSON parsing/writing (minimal implementation to avoid dependencies)
    bool parseJson(const std::string& json);
    std::string toJson() const;
};

#endif // CONFIG_H
