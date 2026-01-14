#include "photo_db.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <cstdint>

PhotoDB::PhotoDB() : db_(nullptr) {
}

PhotoDB::~PhotoDB() {
    close();
}

void PhotoDB::setError(const string& error) {
    last_error_ = error;
    cerr << "PhotoDB Error: " << error << endl;
}

bool PhotoDB::open(const string& db_path) {
    close();
    db_path_ = db_path;
    
    int ret = sqlite3_open(db_path.c_str(), &db_);
    if (ret != SQLITE_OK) {
        setError("Failed to open database: " + string(sqlite3_errmsg(db_)));
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    
    // Enable foreign keys and set busy timeout
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000); // 5 second timeout
    
    return true;
}

void PhotoDB::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool PhotoDB::createSchema() {
    if (!db_) {
        setError("Database not open");
        return false;
    }

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS photos (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            hash TEXT UNIQUE NOT NULL,
            phone_path TEXT NOT NULL,
            local_path TEXT NOT NULL,
            transfer_date INTEGER NOT NULL,
            file_size INTEGER NOT NULL,
            modification_date INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_hash ON photos(hash);
        CREATE INDEX IF NOT EXISTS idx_transfer_date ON photos(transfer_date);
        CREATE INDEX IF NOT EXISTS idx_modification_date ON photos(modification_date);

        CREATE TABLE IF NOT EXISTS sync_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );

        INSERT OR IGNORE INTO sync_metadata (key, value) 
        VALUES ('last_sync_time', '0');
    )";

    char* err_msg = nullptr;
    int ret = sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg);
    
    if (ret != SQLITE_OK) {
        setError("Failed to create schema: " + string(err_msg ? err_msg : "unknown error"));
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    
    return true;
}

bool PhotoDB::initialize() {
    if (!db_) {
        setError("Database not open");
        return false;
    }
    
    return createSchema();
}

bool PhotoDB::photoExists(const string& hash) {
    if (!db_) return false;

    string sql = "SELECT COUNT(*) FROM photos WHERE hash = ?";
    sqlite3_stmt* stmt = nullptr;
    
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        setError("Failed to prepare statement: " + string(sqlite3_errmsg(db_)));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = (sqlite3_column_int(stmt, 0) > 0);
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

bool PhotoDB::addPhoto(const string& hash,
                       const string& phone_path,
                       const string& local_path,
                       uint64_t file_size,
                       uint64_t modification_date) {
    if (!db_) {
        setError("Database not open");
        return false;
    }

    string sql = R"(
        INSERT OR REPLACE INTO photos 
        (hash, phone_path, local_path, transfer_date, file_size, modification_date)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        setError("Failed to prepare statement: " + string(sqlite3_errmsg(db_)));
        return false;
    }

    uint64_t transfer_date = time(nullptr);
    
    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, phone_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, local_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, transfer_date);
    sqlite3_bind_int64(stmt, 5, file_size);
    sqlite3_bind_int64(stmt, 6, modification_date);

    ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (ret != SQLITE_DONE) {
        setError("Failed to insert photo: " + string(sqlite3_errmsg(db_)));
        return false;
    }

    return true;
}

string PhotoDB::getLocalPath(const string& hash) {
    if (!db_) return "";

    string sql = "SELECT local_path FROM photos WHERE hash = ? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return "";
    }
    
    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
    
    string result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* path = (const char*)sqlite3_column_text(stmt, 0);
        if (path) result = path;
    }
    
    sqlite3_finalize(stmt);
    return result;
}

uint64_t PhotoDB::getLastSyncTime() {
    if (!db_) return 0;

    string sql = "SELECT value FROM sync_metadata WHERE key = 'last_sync_time'";
    sqlite3_stmt* stmt = nullptr;
    
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    uint64_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* value = (const char*)sqlite3_column_text(stmt, 0);
        if (value) {
            result = strtoull(value, nullptr, 10);
        }
    }
    
    sqlite3_finalize(stmt);
    return result;
}

bool PhotoDB::setLastSyncTime(uint64_t timestamp) {
    if (!db_) {
        setError("Database not open");
        return false;
    }

    string sql = R"(
        INSERT OR REPLACE INTO sync_metadata (key, value)
        VALUES ('last_sync_time', ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        setError("Failed to prepare statement");
        return false;
    }

    string timestamp_str = to_string(timestamp);
    sqlite3_bind_text(stmt, 1, timestamp_str.c_str(), -1, SQLITE_STATIC);

    ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (ret == SQLITE_DONE);
}

int PhotoDB::getPhotoCount() {
    if (!db_) return 0;

    string sql = "SELECT COUNT(*) FROM photos";
    sqlite3_stmt* stmt = nullptr;
    
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

uint64_t PhotoDB::getTotalSizeTransferred() {
    if (!db_) return 0;

    string sql = "SELECT SUM(file_size) FROM photos";
    sqlite3_stmt* stmt = nullptr;
    
    int ret = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    uint64_t total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return total;
}
