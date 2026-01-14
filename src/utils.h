#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>

using namespace std;

/**
 * Utility functions for file operations and hashing
 */
namespace Utils {
    // Hash calculation
    string calculateSHA256(const vector<uint8_t>& data);
    string calculateFileHash(const string& file_path);
    
    // File operations
    bool fileExists(const string& path);
    bool createDirectory(const string& path);
    bool writeFile(const string& path, const vector<uint8_t>& data);
    uint64_t getFileSize(const string& path);
    uint64_t getFileModificationTime(const string& path);
    
    // Path operations
    string getDirectory(const string& file_path);
    string joinPath(const string& base, const string& path);
    string expandPath(const string& path); // Expand ~ to home directory
    
    // Date/Time operations
    string formatDate(uint64_t timestamp);
    string getDateFolder(uint64_t timestamp); // Returns "YYYY/MM" format
}

#endif // UTILS_H
