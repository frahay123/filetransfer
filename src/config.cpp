#include "config.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

Config::Config() 
    : destination_folder_(getDefaultDestination()),
      device_type_("auto"),
      transfer_mode_("new_only"),
      remember_settings_(true),
      first_run_(true) {
}

std::string Config::getConfigDirectory() const {
#ifdef _WIN32
    // Windows: %APPDATA%\photo_transfer
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\photo_transfer";
    }
    return "";
#elif __APPLE__
    // macOS: ~/Library/Application Support/photo_transfer
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return std::string(home) + "/Library/Application Support/photo_transfer";
    }
    return "";
#else
    // Linux: ~/.config/photo_transfer
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0] != '\0') {
        return std::string(xdg_config) + "/photo_transfer";
    }
    
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return std::string(home) + "/.config/photo_transfer";
    }
    return "";
#endif
}

std::string Config::getConfigPath() const {
    std::string dir = getConfigDirectory();
    if (dir.empty()) return "";
    
#ifdef _WIN32
    return dir + "\\config.json";
#else
    return dir + "/config.json";
#endif
}

std::string Config::getDefaultDestination() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, 0, path))) {
        return std::string(path) + "\\PhotoTransfer";
    }
    return "C:\\Pictures\\PhotoTransfer";
#else
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return std::string(home) + "/Pictures/PhotoTransfer";
    }
    return "~/Pictures/PhotoTransfer";
#endif
}

bool Config::load() {
    std::string config_path = getConfigPath();
    if (config_path.empty()) {
        return false;
    }
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        first_run_ = true;
        return false; // File doesn't exist - first run
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    if (parseJson(content)) {
        first_run_ = false;
        return true;
    }
    
    return false;
}

bool Config::save() {
    std::string config_dir = getConfigDirectory();
    std::string config_path = getConfigPath();
    
    if (config_dir.empty() || config_path.empty()) {
        return false;
    }
    
    // Create config directory if it doesn't exist
    Utils::createDirectory(config_dir);
    
    std::string json = toJson();
    
    std::ofstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << config_path << std::endl;
        return false;
    }
    
    file << json;
    file.close();
    
    first_run_ = false;
    return true;
}

bool Config::reset() {
    destination_folder_ = getDefaultDestination();
    device_type_ = "auto";
    transfer_mode_ = "new_only";
    remember_settings_ = true;
    first_run_ = true;
    
    std::string config_path = getConfigPath();
    if (!config_path.empty()) {
        remove(config_path.c_str());
    }
    
    return true;
}

// Simple JSON parser (handles our specific format only)
bool Config::parseJson(const std::string& json) {
    auto getValue = [&json](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == std::string::npos) return "";
        
        // Skip whitespace
        pos++;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
            pos++;
        }
        
        if (pos >= json.size()) return "";
        
        // Check for boolean
        if (json.substr(pos, 4) == "true") return "true";
        if (json.substr(pos, 5) == "false") return "false";
        
        // Check for string (quoted)
        if (json[pos] == '"') {
            size_t end = json.find("\"", pos + 1);
            if (end != std::string::npos) {
                return json.substr(pos + 1, end - pos - 1);
            }
        }
        
        return "";
    };
    
    std::string dest = getValue("destination_folder");
    if (!dest.empty()) destination_folder_ = dest;
    
    std::string device = getValue("device_type");
    if (!device.empty()) device_type_ = device;
    
    std::string mode = getValue("transfer_mode");
    if (!mode.empty()) transfer_mode_ = mode;
    
    std::string remember = getValue("remember_settings");
    if (remember == "true") remember_settings_ = true;
    else if (remember == "false") remember_settings_ = false;
    
    return true;
}

std::string Config::toJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"destination_folder\": \"" << destination_folder_ << "\",\n";
    ss << "  \"device_type\": \"" << device_type_ << "\",\n";
    ss << "  \"transfer_mode\": \"" << transfer_mode_ << "\",\n";
    ss << "  \"remember_settings\": " << (remember_settings_ ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}
