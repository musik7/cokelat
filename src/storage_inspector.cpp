/**
 * @file storage_inspector.cpp
 * @brief Storage Inspector for Mobile DevTools (WASM)
 * 
 * Inspect localStorage, sessionStorage, cookies, and IndexedDB metadata
 * for debugging and performance analysis.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. STORAGE STRUCTURES
// ==========================================

enum class StorageType {
    LOCAL_STORAGE,
    SESSION_STORAGE,
    COOKIE,
    INDEXED_DB,
    CACHE_STORAGE
};

struct StorageItem {
    std::string key;
    std::string value;
    size_t sizeBytes;
    long long createdAt;
    long long updatedAt;
    long long expiresAt; // 0 = never
    bool isExpired;
};

struct StorageDatabase {
    std::string id;
    std::string name;
    StorageType type;
    std::vector<StorageItem> items;
    size_t totalBytes;
    int itemCount;
    long long createdAt;
};

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    long long expiresAt;
    bool isSecure;
    bool isHttpOnly;
    bool isSameSite;
    size_t sizeBytes;
};

struct StorageStats {
    size_t localStorageBytes;
    size_t sessionStorageBytes;
    size_t cookieBytes;
    size_t indexedDBBytes;
    size_t cacheStorageBytes;
    int localStorageItemCount;
    int sessionStorageItemCount;
    int cookieCount;
    int indexedDBCount;
    int cacheStorageCount;
    double usagePercentage; // Out of quota
};

// ==========================================
// 2. STORAGE INSPECTOR
// ==========================================

class StorageInspector {
private:
    std::vector<StorageDatabase> databases;
    std::vector<Cookie> cookies;
    static const size_t MAX_STORAGE_QUOTA = 50 * 1024 * 1024; // 50MB typical quota
    size_t totalUsedBytes = 0;
    int dbIdCounter = 0;

    std::string generateId() {
        return "storage_" + std::to_string(dbIdCounter++);
    }

    bool isExpired(const StorageItem& item) const {
        if (item.expiresAt == 0) return false;
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return ms > item.expiresAt;
    }

public:
    StorageInspector() {
        databases.reserve(100);
        cookies.reserve(100);
    }

    // Add storage database (localStorage, sessionStorage, IndexedDB)
    std::string addStorageDatabase(const std::string& name, StorageType type) {
        std::string id = generateId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        databases.push_back({
            id,
            name,
            type,
            {},
            0,
            0,
            ms
        });

        return id;
    }

    // Add item to storage
    bool addStorageItem(
        const std::string& dbId,
        const std::string& key,
        const std::string& value,
        long long expiresAt = 0
    ) {
        for (auto& db : databases) {
            if (db.id == dbId) {
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

                size_t itemSize = key.length() + value.length();

                // Check if key exists, update if so
                for (auto& item : db.items) {
                    if (item.key == key) {
                        db.totalBytes -= item.sizeBytes;
                        item.value = value;
                        item.sizeBytes = itemSize;
                        item.updatedAt = ms;
                        item.expiresAt = expiresAt;
                        db.totalBytes += itemSize;
                        totalUsedBytes = calculateTotalUsed();
                        return true;
                    }
                }

                // New item
                if (db.totalBytes + itemSize > MAX_STORAGE_QUOTA) {
                    return false; // Quota exceeded
                }

                db.items.push_back({
                    key,
                    value,
                    itemSize,
                    ms,
                    ms,
                    expiresAt,
                    false
                });
                db.itemCount++;
                db.totalBytes += itemSize;
                totalUsedBytes = calculateTotalUsed();
                return true;
            }
        }
        return false;
    }

    // Remove storage item
    bool removeStorageItem(const std::string& dbId, const std::string& key) {
        for (auto& db : databases) {
            if (db.id == dbId) {
                for (auto it = db.items.begin(); it != db.items.end(); ++it) {
                    if (it->key == key) {
                        db.totalBytes -= it->sizeBytes;
                        db.itemCount--;
                        db.items.erase(it);
                        totalUsedBytes = calculateTotalUsed();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Explicit update (alias for addStorageItem which does upsert)
    bool updateStorageItem(
        const std::string& dbId,
        const std::string& key,
        const std::string& value,
        long long expiresAt = 0
    ) {
        return addStorageItem(dbId, key, value, expiresAt);
    }

    // Explicit update (alias for addCookie which does upsert)
    void updateCookie(
        const std::string& name,
        const std::string& value,
        const std::string& domain,
        const std::string& path,
        long long expiresAt,
        bool isSecure,
        bool isHttpOnly,
        bool isSameSite
    ) {
        addCookie(name, value, domain, path, expiresAt, isSecure, isHttpOnly, isSameSite);
    }

    // Add cookie
    void addCookie(
        const std::string& name,
        const std::string& value,
        const std::string& domain,
        const std::string& path,
        long long expiresAt,
        bool isSecure,
        bool isHttpOnly,
        bool isSameSite
    ) {
        size_t cookieSize = name.length() + value.length() + domain.length() + path.length();
        
        for (auto& cookie : cookies) {
            if (cookie.name == name && cookie.domain == domain && cookie.path == path) {
                cookie.value = value;
                cookie.expiresAt = expiresAt;
                cookie.isSecure = isSecure;
                cookie.isHttpOnly = isHttpOnly;
                cookie.isSameSite = isSameSite;
                cookie.sizeBytes = cookieSize;
                totalUsedBytes = calculateTotalUsed();
                return;
            }
        }
        
        cookies.push_back({
            name,
            value,
            domain,
            path,
            expiresAt,
            isSecure,
            isHttpOnly,
            isSameSite,
            cookieSize
        });
        totalUsedBytes = calculateTotalUsed();
    }

    // Get all storage databases
    const std::vector<StorageDatabase>& getStorageDatabases() const {
        return databases;
    }

    // Get storage databases by type
    std::vector<StorageDatabase> getStorageDatabasesByType(StorageType type) const {
        std::vector<StorageDatabase> result;
        for (const auto& db : databases) {
            if (db.type == type) {
                result.push_back(db);
            }
        }
        return result;
    }

    // Get items from specific database
    const std::vector<StorageItem>& getStorageItems(const std::string& dbId) const {
        static const std::vector<StorageItem> empty;
        for (const auto& db : databases) {
            if (db.id == dbId) {
                return db.items;
            }
        }
        return empty;
    }

    // Get all cookies
    const std::vector<Cookie>& getCookies() const {
        return cookies;
    }

    // Get cookies by domain
    std::vector<Cookie> getCookiesByDomain(const std::string& domain) const {
        std::vector<Cookie> result;
        for (const auto& cookie : cookies) {
            if (cookie.domain == domain || cookie.domain.find(domain) != std::string::npos) {
                result.push_back(cookie);
            }
        }
        return result;
    }

    // Search storage items
    std::vector<StorageItem> searchStorageItems(const std::string& query) const {
        std::vector<StorageItem> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (const auto& db : databases) {
            for (const auto& item : db.items) {
                std::string lowerKey = item.key;
                std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
                std::string lowerValue = item.value;
                std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);

                if (lowerKey.find(lowerQuery) != std::string::npos ||
                    lowerValue.find(lowerQuery) != std::string::npos) {
                    results.push_back(item);
                }
            }
        }
        return results;
    }

    // Get storage statistics
    StorageStats getStats() const {
        StorageStats stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        for (const auto& db : databases) {
            switch (db.type) {
                case StorageType::LOCAL_STORAGE:
                    stats.localStorageBytes = db.totalBytes;
                    stats.localStorageItemCount = db.itemCount;
                    break;
                case StorageType::SESSION_STORAGE:
                    stats.sessionStorageBytes = db.totalBytes;
                    stats.sessionStorageItemCount = db.itemCount;
                    break;
                case StorageType::INDEXED_DB:
                    stats.indexedDBBytes = db.totalBytes;
                    stats.indexedDBCount = db.itemCount;
                    break;
                case StorageType::CACHE_STORAGE:
                    stats.cacheStorageBytes = db.totalBytes;
                    stats.cacheStorageCount = db.itemCount;
                    break;
                default:
                    break;
            }
        }

        stats.cookieBytes = 0;
        stats.cookieCount = cookies.size();
        for (const auto& cookie : cookies) {
            stats.cookieBytes += cookie.sizeBytes;
        }

        stats.usagePercentage = (double)totalUsedBytes / (double)MAX_STORAGE_QUOTA * 100.0;
        return stats;
    }

    // Clear expired items
    int clearExpired() {
        int clearedCount = 0;
        for (auto& db : databases) {
            for (auto it = db.items.begin(); it != db.items.end();) {
                if (isExpired(*it)) {
                    db.totalBytes -= it->sizeBytes;
                    db.itemCount--;
                    it = db.items.erase(it);
                    clearedCount++;
                } else {
                    ++it;
                }
            }
        }

        // Clear expired cookies
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        for (auto it = cookies.begin(); it != cookies.end();) {
            if (it->expiresAt > 0 && ms > it->expiresAt) {
                it = cookies.erase(it);
                clearedCount++;
            } else {
                ++it;
            }
        }

        totalUsedBytes = calculateTotalUsed();
        return clearedCount;
    }

    void clear() {
        databases.clear();
        cookies.clear();
        totalUsedBytes = 0;
        dbIdCounter = 0;
    }

    // Remove a cookie
    bool removeCookie(const std::string& name) {
        for (auto it = cookies.begin(); it != cookies.end(); ++it) {
            if (it->name == name) {
                cookies.erase(it);
                totalUsedBytes = calculateTotalUsed();
                return true;
            }
        }
        return false;
    }

    // Clear items in specific database (localStorage, sessionStorage, indexedDB, cacheStorage)
    bool clearDatabase(const std::string& dbId) {
        for (auto& db : databases) {
            if (db.id == dbId) {
                db.items.clear();
                db.totalBytes = 0;
                db.itemCount = 0;
                totalUsedBytes = calculateTotalUsed();
                return true;
            }
        }
        return false;
    }

    // Clear all cookies
    void clearCookies() {
        cookies.clear();
        totalUsedBytes = calculateTotalUsed();
    }

private:
    size_t calculateTotalUsed() const {
        size_t total = 0;
        for (const auto& db : databases) {
            total += db.totalBytes;
        }
        for (const auto& cookie : cookies) {
            total += cookie.sizeBytes;
        }
        return total;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(storage_inspector_module) {
    class_<StorageItem>("StorageItem")
        .property("key", &StorageItem::key)
        .property("value", &StorageItem::value)
        .property("sizeBytes", &StorageItem::sizeBytes)
        .property("createdAt", &StorageItem::createdAt)
        .property("updatedAt", &StorageItem::updatedAt)
        .property("expiresAt", &StorageItem::expiresAt)
        .property("isExpired", &StorageItem::isExpired);

    register_vector<StorageItem>("VectorStorageItem");

    class_<StorageDatabase>("StorageDatabase")
        .property("id", &StorageDatabase::id)
        .property("name", &StorageDatabase::name)
        .property("items", &StorageDatabase::items)
        .property("totalBytes", &StorageDatabase::totalBytes)
        .property("itemCount", &StorageDatabase::itemCount)
        .property("createdAt", &StorageDatabase::createdAt);

    register_vector<StorageDatabase>("VectorStorageDatabase");

    class_<Cookie>("Cookie")
        .property("name", &Cookie::name)
        .property("value", &Cookie::value)
        .property("domain", &Cookie::domain)
        .property("path", &Cookie::path)
        .property("expiresAt", &Cookie::expiresAt)
        .property("isSecure", &Cookie::isSecure)
        .property("isHttpOnly", &Cookie::isHttpOnly)
        .property("isSameSite", &Cookie::isSameSite)
        .property("sizeBytes", &Cookie::sizeBytes);

    register_vector<Cookie>("VectorCookie");

    class_<StorageStats>("StorageStats")
        .property("localStorageBytes", &StorageStats::localStorageBytes)
        .property("sessionStorageBytes", &StorageStats::sessionStorageBytes)
        .property("cookieBytes", &StorageStats::cookieBytes)
        .property("indexedDBBytes", &StorageStats::indexedDBBytes)
        .property("cacheStorageBytes", &StorageStats::cacheStorageBytes)
        .property("localStorageItemCount", &StorageStats::localStorageItemCount)
        .property("sessionStorageItemCount", &StorageStats::sessionStorageItemCount)
        .property("cookieCount", &StorageStats::cookieCount)
        .property("indexedDBCount", &StorageStats::indexedDBCount)
        .property("cacheStorageCount", &StorageStats::cacheStorageCount)
        .property("usagePercentage", &StorageStats::usagePercentage);

    class_<StorageInspector>("StorageInspector")
        .constructor()
        .function("addStorageDatabase", select_overload<std::string(
            const std::string&,
            StorageType
        )>(&StorageInspector::addStorageDatabase))
        .function("addStorageItem", select_overload<bool(
            const std::string&,
            const std::string&,
            const std::string&,
            long long
        )>(&StorageInspector::addStorageItem))
        .function("updateStorageItem", select_overload<bool(
            const std::string&,
            const std::string&,
            const std::string&,
            long long
        )>(&StorageInspector::updateStorageItem))
        .function("removeStorageItem", &StorageInspector::removeStorageItem)
        .function("addCookie", select_overload<void(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            long long,
            bool,
            bool,
            bool
        )>(&StorageInspector::addCookie))
        .function("updateCookie", select_overload<void(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            long long,
            bool,
            bool,
            bool
        )>(&StorageInspector::updateCookie))
        .function("getStorageDatabases", &StorageInspector::getStorageDatabases)
        .function("getStorageDatabasesByType", &StorageInspector::getStorageDatabasesByType)
        .function("getStorageItems", &StorageInspector::getStorageItems)
        .function("getCookies", &StorageInspector::getCookies)
        .function("getCookiesByDomain", &StorageInspector::getCookiesByDomain)
        .function("searchStorageItems", &StorageInspector::searchStorageItems)
        .function("getStats", &StorageInspector::getStats)
        .function("clearExpired", &StorageInspector::clearExpired)
        .function("clear", &StorageInspector::clear)
        .function("removeCookie", &StorageInspector::removeCookie)
        .function("clearDatabase", &StorageInspector::clearDatabase)
        .function("clearCookies", &StorageInspector::clearCookies);

    enum_<StorageType>("StorageType")
        .value("LOCAL_STORAGE", StorageType::LOCAL_STORAGE)
        .value("SESSION_STORAGE", StorageType::SESSION_STORAGE)
        .value("COOKIE", StorageType::COOKIE)
        .value("INDEXED_DB", StorageType::INDEXED_DB)
        .value("CACHE_STORAGE", StorageType::CACHE_STORAGE);
}
#endif
