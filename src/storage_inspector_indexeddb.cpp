#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// INDEXEDDB ENGINE
// ==========================================

struct IndexedDBRecord {
    std::string key;
    std::string value; // JSON or stringified
    size_t sizeBytes;
};

struct IndexedDBObjectStore {
    std::string name;
    std::string keyPath;
    bool autoIncrement;
    std::vector<IndexedDBRecord> records;
    size_t totalBytes;
    int recordCount;
};

struct IndexedDBDatabase {
    std::string name;
    int version;
    std::vector<IndexedDBObjectStore> objectStores;
    size_t totalBytes;
};

class IndexedDBEngine {
private:
    std::vector<IndexedDBDatabase> databases;

public:
    IndexedDBEngine() {}

    void addDatabase(const std::string& name, int version) {
        databases.push_back({name, version, {}, 0});
    }

    void addObjectStore(const std::string& dbName, const std::string& storeName, const std::string& keyPath, bool autoIncrement) {
        for (auto& db : databases) {
            if (db.name == dbName) {
                db.objectStores.push_back({storeName, keyPath, autoIncrement, {}, 0, 0});
                return;
            }
        }
    }

    void addRecord(const std::string& dbName, const std::string& storeName, const std::string& key, const std::string& value, size_t sizeBytes) {
        for (auto& db : databases) {
            if (db.name == dbName) {
                for (auto& store : db.objectStores) {
                    if (store.name == storeName) {
                        store.records.push_back({key, value, sizeBytes});
                        store.totalBytes += sizeBytes;
                        store.recordCount++;
                        db.totalBytes += sizeBytes;
                        return;
                    }
                }
            }
        }
    }

    const std::vector<IndexedDBDatabase>& getDatabases() const {
        return databases;
    }

    void clear() {
        databases.clear();
    }
};

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(indexeddb_engine_module) {
    class_<IndexedDBRecord>("IndexedDBRecord")
        .property("key", &IndexedDBRecord::key)
        .property("value", &IndexedDBRecord::value)
        .property("sizeBytes", &IndexedDBRecord::sizeBytes);
        
    register_vector<IndexedDBRecord>("VectorIndexedDBRecord");

    class_<IndexedDBObjectStore>("IndexedDBObjectStore")
        .property("name", &IndexedDBObjectStore::name)
        .property("keyPath", &IndexedDBObjectStore::keyPath)
        .property("autoIncrement", &IndexedDBObjectStore::autoIncrement)
        .property("records", &IndexedDBObjectStore::records)
        .property("totalBytes", &IndexedDBObjectStore::totalBytes)
        .property("recordCount", &IndexedDBObjectStore::recordCount);
        
    register_vector<IndexedDBObjectStore>("VectorIndexedDBObjectStore");

    class_<IndexedDBDatabase>("IndexedDBDatabase")
        .property("name", &IndexedDBDatabase::name)
        .property("version", &IndexedDBDatabase::version)
        .property("objectStores", &IndexedDBDatabase::objectStores)
        .property("totalBytes", &IndexedDBDatabase::totalBytes);

    register_vector<IndexedDBDatabase>("VectorIndexedDBDatabase");

    class_<IndexedDBEngine>("IndexedDBEngine")
        .constructor()
        .function("addDatabase", &IndexedDBEngine::addDatabase)
        .function("addObjectStore", &IndexedDBEngine::addObjectStore)
        .function("addRecord", &IndexedDBEngine::addRecord)
        .function("getDatabases", &IndexedDBEngine::getDatabases)
        .function("clear", &IndexedDBEngine::clear);
}
#endif
