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

using namespace std;

Config::Config() 
    : destination_folder_(getDefaultDestination()),
      device_type_("auto"),
      transfer_mode_("new_only"),
      remember_settings_(true),
      first_run_(true) {
}

string Config::getConfigDirectory() const {
#ifdef _WIN32
    // Windows: %APPDATA%\photo_transfer
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return string(path) + "\\photo_transfer";
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
        return string(home) + "/Library/Application Support/photo_transfer";
    }
    return "";
#else
    // Linux: ~/.config/photo_transfer
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0] != '\0') {
        return string(xdg_config) + "/photo_transfer";
    }
    
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return string(home) + "/.config/photo_transfer";
    }
    return "";
#endif
}

string Config::getConfigPath() const {
    string dir = getConfigDirectory();
    if (dir.empty()) return "";
    
#ifdef _WIN32
    return dir + "\\config.json";
#else
    return dir + "/config.json";
#endif
}

string Config::getDefaultDestination() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, 0, path))) {
        return string(path) + "\\PhotoTransfer";
    }
    return "C:\\Pictures\\PhotoTransfer";
#else
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return string(home) + "/Pictures/PhotoTransfer";
    }
    return "~/Pictures/PhotoTransfer";
#endif
}

bool Config::load() {
    string config_path = getConfigPath();
    if (config_path.empty()) {
        return false;
    }
    
    ifstream file(config_path);
    if (!file.is_open()) {
        first_run_ = true;
        return false; // File doesn't exist - first run
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();
    
    if (parseJson(content)) {
        first_run_ = false;
        return true;
    }
    
    return false;
}

bool Config::save() {
    string config_dir = getConfigDirectory();
    string config_path = getConfigPath();
    
    if (config_dir.empty() || config_path.empty()) {
        return false;
    }
    
    // Create config directory if it doesn't exist
    Utils::createDirectory(config_dir);
    
    string json = toJson();
    
    ofstream file(config_path);
    if (!file.is_open()) {
        cerr << "Failed to open config file for writing: " << config_path << endl;
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
    
    string config_path = getConfigPath();
    if (!config_path.empty()) {
        remove(config_path.c_str());
    }
    
    return true;
}

// Simple JSON parser (handles our specific format only)
bool Config::parseJson(const string& json) {
    auto getValue = [&json](const string& key) -> string {
        string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == string::npos) return "";
        
        pos = json.find(":", pos);
        if (pos == string::npos) return "";
        
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
            if (end != string::npos) {
                return json.substr(pos + 1, end - pos - 1);
            }
        }
        
        return "";
    };
    
    string dest = getValue("destination_folder");
    if (!dest.empty()) destination_folder_ = dest;
    
    string device = getValue("device_type");
    if (!device.empty()) device_type_ = device;
    
    string mode = getValue("transfer_mode");
    if (!mode.empty()) transfer_mode_ = mode;
    
    string remember = getValue("remember_settings");
    if (remember == "true") remember_settings_ = true;
    else if (remember == "false") remember_settings_ = false;
    
    return true;
}

string Config::toJson() const {
    stringstream ss;
    ss << "{\n";
    ss << "  \"destination_folder\": \"" << destination_folder_ << "\",\n";
    ss << "  \"device_type\": \"" << device_type_ << "\",\n";
    ss << "  \"transfer_mode\": \"" << transfer_mode_ << "\",\n";
    ss << "  \"remember_settings\": " << (remember_settings_ ? "true" : "false") << "\n";
    ss << "}\n";
    return ss.str();
}
