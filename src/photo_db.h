#ifndef PHOTO_DB_H
#define PHOTO_DB_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

/**
 * Database handler for tracking transferred photos
 */
class PhotoDB {
public:
    PhotoDB();
    ~PhotoDB();

    // Database management
    bool open(const string& db_path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // Schema management
    bool createSchema();
    bool initialize();

    // Photo operations
    bool photoExists(const string& hash);
    bool addPhoto(const string& hash, 
                  const string& phone_path,
                  const string& local_path,
                  uint64_t file_size,
                  uint64_t modification_date);
    bool updatePhotoPath(const string& hash, const string& new_local_path);
    
    // Query operations
    string getLocalPath(const string& hash);
    uint64_t getLastSyncTime();
    bool setLastSyncTime(uint64_t timestamp);
    
    // Statistics
    int getPhotoCount();
    uint64_t getTotalSizeTransferred();

    // Error handling
    string getLastError() const { return last_error_; }

private:
    sqlite3* db_;
    string last_error_;
    string db_path_;

    void setError(const string& error);
    bool executeSQL(const string& sql);
};

#endif // PHOTO_DB_H
