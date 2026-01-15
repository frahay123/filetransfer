#ifndef PHOTO_DB_H
#define PHOTO_DB_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <cstdint>

/**
 * Database handler for tracking transferred photos
 */
class PhotoDB {
public:
    PhotoDB();
    ~PhotoDB();

    // Database management
    bool open(const std::string& db_path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // Schema management
    bool createSchema();
    bool initialize();

    // Photo operations
    bool photoExists(const std::string& hash);
    bool addPhoto(const std::string& hash, 
                  const std::string& phone_path,
                  const std::string& local_path,
                  uint64_t file_size,
                  uint64_t modification_date);
    bool updatePhotoPath(const std::string& hash, const std::string& new_local_path);
    
    // Query operations
    std::string getLocalPath(const std::string& hash);
    uint64_t getLastSyncTime();
    bool setLastSyncTime(uint64_t timestamp);
    
    // Statistics
    int getPhotoCount();
    uint64_t getTotalSizeTransferred();

    // Error handling
    std::string getLastError() const { return last_error_; }

private:
    sqlite3* db_;
    std::string last_error_;
    std::string db_path_;

    void setError(const std::string& error);
    bool executeSQL(const std::string& sql);
};

#endif // PHOTO_DB_H
