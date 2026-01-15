#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * Utility functions for file operations and hashing
 */
namespace Utils {
    // Hash calculation
    std::string calculateSHA256(const std::vector<uint8_t>& data);
    std::string calculateFileHash(const std::string& file_path);
    
    // File operations
    bool fileExists(const std::string& path);
    bool createDirectory(const std::string& path);
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    uint64_t getFileSize(const std::string& path);
    uint64_t getFileModificationTime(const std::string& path);
    
    // Path operations
    std::string getDirectory(const std::string& file_path);
    std::string joinPath(const std::string& base, const std::string& path);
    std::string expandPath(const std::string& path); // Expand ~ to home directory
    
    // Date/Time operations
    std::string formatDate(uint64_t timestamp);
    std::string getDateFolder(uint64_t timestamp); // Returns "YYYY/MM" format
}

#endif // UTILS_H
