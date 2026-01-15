#include "utils.h"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <errno.h>
#include <cstdlib>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <unistd.h>
#include <pwd.h>
#endif

using namespace std;

string Utils::calculateSHA256(const vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);
    
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

string Utils::calculateFileHash(const string& file_path) {
    ifstream file(file_path, ios::binary);
    if (!file) {
        return "";
    }
    
    vector<uint8_t> buffer((istreambuf_iterator<char>(file)),
                          istreambuf_iterator<char>());
    file.close();
    
    return calculateSHA256(buffer);
}

bool Utils::fileExists(const string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool Utils::createDirectory(const string& path) {
    // Check if already exists
    struct stat info;
    if (stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        return true;
    }
    
    // Create parent directories first
    string parent = getDirectory(path);
    if (!parent.empty() && !fileExists(parent)) {
        if (!createDirectory(parent)) {
            return false;
        }
    }
    
    // Create directory
#ifdef _WIN32
    return (_mkdir(path.c_str()) == 0 || errno == EEXIST);
#else
    return (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST);
#endif
}

bool Utils::writeFile(const string& path, const vector<uint8_t>& data) {
    // Create directory if needed
    string dir = getDirectory(path);
    if (!dir.empty() && !createDirectory(dir)) {
        return false;
    }
    
    ofstream file(path, ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
    
    return file.good();
}

uint64_t Utils::getFileSize(const string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return 0;
    }
    return info.st_size;
}

uint64_t Utils::getFileModificationTime(const string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return 0;
    }
    return info.st_mtime;
}

string Utils::getDirectory(const string& file_path) {
    size_t pos = file_path.find_last_of("/\\");
    if (pos == string::npos) {
        return "";
    }
    return file_path.substr(0, pos);
}

string Utils::joinPath(const string& base, const string& path) {
    if (base.empty()) return path;
    if (path.empty()) return base;
    
    char sep = '/';
#ifdef _WIN32
    sep = '\\';
#endif
    
    if (base.back() == '/' || base.back() == '\\') {
        return base + path;
    }
    return base + sep + path;
}

string Utils::expandPath(const string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    
    string home;
    
#ifdef _WIN32
    const char* userprofile = getenv("USERPROFILE");
    if (userprofile) {
        home = userprofile;
    } else {
        const char* homedrive = getenv("HOMEDRIVE");
        const char* homepath = getenv("HOMEPATH");
        if (homedrive && homepath) {
            home = string(homedrive) + string(homepath);
        } else {
            return path; // Can't expand
        }
    }
#else
    const char* home_env = getenv("HOME");
    if (home_env) {
        home = home_env;
    } else {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        } else {
            return path; // Can't expand, return as-is
        }
    }
#endif
    
    if (path.length() == 1) {
        return home;
    }
    
    return home + path.substr(1);
}

string Utils::formatDate(uint64_t timestamp) {
    time_t time = timestamp;
    struct tm* timeinfo = localtime(&time);
    
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buffer);
}

string Utils::getDateFolder(uint64_t timestamp) {
    time_t time = timestamp;
    struct tm* timeinfo = localtime(&time);
    
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y/%m", timeinfo);
    return string(buffer);
}
