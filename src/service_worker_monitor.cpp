/**
 * @file service_worker_monitor.cpp
 * @brief Service Worker & Background Task Monitor for Mobile DevTools
 * 
 * Track Service Worker lifecycle, message passing, and background tasks.
 */

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
// 1. SERVICE WORKER STRUCTURES
// ==========================================

enum class ServiceWorkerState {
    INSTALLING,
    INSTALLED,
    ACTIVATING,
    ACTIVATED,
    REDUNDANT
};

enum class MessageType {
    PUSH,
    SYNC,
    MESSAGE,
    FETCH_EVENT,
    CACHE_UPDATE,
    CUSTOM
};

struct ServiceWorkerInstance {
    std::string id;
    std::string url;
    std::string scope;
    ServiceWorkerState state;
    long long registrationTime;
    long long lastUpdateTime;
    size_t scriptSize;        // bytes
    int version;
    bool isActive;
    int clientCount;          // Number of controlled clients
    std::vector<std::string> messageLog; // Recent messages
};

struct BackgroundTask {
    std::string id;
    std::string name;
    MessageType type;
    long long startTime;
    long long endTime;
    long long duration;
    std::string status;       // "pending", "completed", "failed"
    std::string initiator;    // "push", "sync", "timer", etc.
    size_t dataSize;          // Data transferred
    bool isRecurring;
};

struct ServiceWorkerMessage {
    std::string id;
    MessageType type;
    long long timestamp;
    std::string fromSource;   // "sw" or "client"
    std::string toTarget;     // "client" or "sw"
    std::string payload;
    int payloadSize;
    std::string status;       // "sent", "received", "error"
};

struct CacheInfo {
    std::string cacheName;
    int entryCount;
    size_t totalSize;       // bytes
    std::vector<std::pair<std::string, size_t>> entries; // {url, size}
};

struct SyncEvent {
    std::string id;
    std::string tag;
    long long scheduledTime;
    long long executedTime;
    std::string status;     // "pending", "completed", "failed"
    int retryCount;
};

// ==========================================
// 2. SERVICE WORKER MONITOR
// ==========================================

class ServiceWorkerMonitor {
private:
    std::vector<ServiceWorkerInstance> workers;
    std::vector<BackgroundTask> tasks;
    std::vector<ServiceWorkerMessage> messages;
    std::vector<CacheInfo> caches;
    std::vector<SyncEvent> syncEvents;
    size_t workerIdCounter = 0;
    size_t taskIdCounter = 0;
    size_t messageIdCounter = 0;
    size_t syncIdCounter = 0;
    static const size_t MAX_WORKERS = 50;
    static const size_t MAX_TASKS = 1000;
    static const size_t MAX_MESSAGES = 5000;

    std::string generateWorkerId() { return "sw_" + std::to_string(workerIdCounter++); }
    std::string generateTaskId() { return "task_" + std::to_string(taskIdCounter++); }
    std::string generateMessageId() { return "msg_" + std::to_string(messageIdCounter++); }
    std::string generateSyncId() { return "sync_" + std::to_string(syncIdCounter++); }

public:
    ServiceWorkerMonitor() {
        workers.reserve(MAX_WORKERS);
        tasks.reserve(MAX_TASKS);
        messages.reserve(MAX_MESSAGES);
    }

    // Register Service Worker
    std::string registerWorker(
        const std::string& url,
        const std::string& scope,
        size_t scriptSize
    ) {
        if (workers.size() >= MAX_WORKERS) {
            workers.erase(workers.begin());
        }

        std::string id = generateWorkerId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        workers.push_back({
            id,
            url,
            scope,
            ServiceWorkerState::INSTALLING,
            ms,
            ms,
            scriptSize,
            1,
            false,
            0,
            {}
        });

        return id;
    }

    // Update Service Worker state
    bool updateWorkerState(const std::string& workerId, ServiceWorkerState newState) {
        for (auto& worker : workers) {
            if (worker.id == workerId) {
                worker.state = newState;
                if (newState == ServiceWorkerState::ACTIVATED) {
                    worker.isActive = true;
                    auto now = std::chrono::high_resolution_clock::now();
                    worker.lastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                }
                return true;
            }
        }
        return false;
    }

    // Record background task
    std::string recordTask(
        const std::string& name,
        MessageType type,
        long long startTime,
        long long endTime,
        const std::string& initiator,
        size_t dataSize = 0,
        bool isRecurring = false
    ) {
        if (tasks.size() >= MAX_TASKS) {
            tasks.erase(tasks.begin());
        }

        std::string id = generateTaskId();
        std::string status = "completed";

        tasks.push_back({
            id,
            name,
            type,
            startTime,
            endTime,
            endTime - startTime,
            status,
            initiator,
            dataSize,
            isRecurring
        });

        return id;
    }

    // Record message between SW and client
    std::string recordMessage(
        MessageType type,
        const std::string& fromSource,
        const std::string& toTarget,
        const std::string& payload
    ) {
        if (messages.size() >= MAX_MESSAGES) {
            messages.erase(messages.begin());
        }

        std::string id = generateMessageId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        messages.push_back({
            id,
            type,
            ms,
            fromSource,
            toTarget,
            payload,
            (int)payload.length(),
            "sent"
        });

        return id;
    }

    // Add cache info
    void addCacheInfo(
        const std::string& cacheName,
        const std::vector<std::pair<std::string, size_t>>& entries
    ) {
        size_t totalSize = 0;
        for (const auto& entry : entries) {
            totalSize += entry.second;
        }

        caches.push_back({
            cacheName,
            (int)entries.size(),
            totalSize,
            entries
        });
    }

    // Schedule background sync
    std::string scheduleSync(const std::string& tag) {
        std::string id = generateSyncId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        syncEvents.push_back({
            id,
            tag,
            ms,
            0,
            "pending",
            0
        });

        return id;
    }

    // Get all workers
    const std::vector<ServiceWorkerInstance>& getWorkers() const {
        return workers;
    }

    // Get active worker
    ServiceWorkerInstance getActiveWorker() const {
        for (const auto& worker : workers) {
            if (worker.isActive) {
                return worker;
            }
        }
        return {};
    }

    // Get all tasks
    const std::vector<BackgroundTask>& getTasks() const {
        return tasks;
    }

    // Get all messages
    const std::vector<ServiceWorkerMessage>& getMessages() const {
        return messages;
    }

    // Get all caches
    const std::vector<CacheInfo>& getCaches() const {
        return caches;
    }

    // Get all sync events
    const std::vector<SyncEvent>& getSyncEvents() const {
        return syncEvents;
    }

    // Get total cache size
    size_t getTotalCacheSize() const {
        size_t total = 0;
        for (const auto& cache : caches) {
            total += cache.totalSize;
        }
        return total;
    }

    // Get task statistics
    std::map<std::string, size_t> getTaskStats() const {
        std::map<std::string, size_t> stats;
        stats["totalTasks"] = tasks.size();
        stats["pushTasks"] = 0;
        stats["syncTasks"] = 0;
        stats["fetchTasks"] = 0;

        for (const auto& task : tasks) {
            if (task.type == MessageType::PUSH) stats["pushTasks"]++;
            else if (task.type == MessageType::SYNC) stats["syncTasks"]++;
            else if (task.type == MessageType::FETCH_EVENT) stats["fetchTasks"]++;
        }

        return stats;
    }

    // Unregister a service worker by ID
    bool unregisterWorker(const std::string& workerId) {
        for (auto it = workers.begin(); it != workers.end(); ++it) {
            if (it->id == workerId) {
                workers.erase(it);
                return true;
            }
        }
        return false;
    }

    // Stop (deactivate) a service worker
    bool stopWorker(const std::string& workerId) {
        for (auto& worker : workers) {
            if (worker.id == workerId) {
                worker.isActive = false;
                worker.state = ServiceWorkerState::INSTALLED; // fallback to inactive installed
                worker.messageLog.push_back("Worker thread stopped manually via DevTools.");
                return true;
            }
        }
        return false;
    }

    // Start (activate) a service worker
    bool startWorker(const std::string& workerId) {
        for (auto& worker : workers) {
            if (worker.id == workerId) {
                worker.isActive = true;
                worker.state = ServiceWorkerState::ACTIVATED;
                worker.messageLog.push_back("Worker thread started manually via DevTools.");
                return true;
            }
        }
        return false;
    }

    // Trigger a mock Push Notification event
    bool triggerPush(const std::string& workerId, const std::string& payload) {
        for (auto& worker : workers) {
            if (worker.id == workerId) {
                worker.messageLog.push_back("[Server -> SW] Push Notification Triggered: " + payload);
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                recordTask("Push notification: " + payload.substr(0, 30), MessageType::PUSH, ms - 15, ms, "push_trigger", payload.length(), false);
                return true;
            }
        }
        return false;
    }

    // Trigger a mock Background Sync event
    bool triggerSync(const std::string& workerId, const std::string& tag) {
        for (auto& worker : workers) {
            if (worker.id == workerId) {
                worker.messageLog.push_back("[Client -> SW] Background Sync Triggered: " + tag);
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                scheduleSync(tag);
                recordTask("Background Sync: " + tag, MessageType::SYNC, ms - 50, ms, "sync_trigger", 128, false);
                return true;
            }
        }
        return false;
    }

    void clear() {
        workers.clear();
        tasks.clear();
        messages.clear();
        caches.clear();
        syncEvents.clear();
        workerIdCounter = 0;
        taskIdCounter = 0;
        messageIdCounter = 0;
        syncIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(service_worker_monitor_module) {
    class_<ServiceWorkerInstance>("ServiceWorkerInstance")
        .property("id", &ServiceWorkerInstance::id)
        .property("url", &ServiceWorkerInstance::url)
        .property("scope", &ServiceWorkerInstance::scope)
        .property("version", &ServiceWorkerInstance::version)
        .property("isActive", &ServiceWorkerInstance::isActive)
        .property("clientCount", &ServiceWorkerInstance::clientCount)
        .property("scriptSize", &ServiceWorkerInstance::scriptSize);

    register_vector<ServiceWorkerInstance>("VectorServiceWorkerInstance");

    class_<BackgroundTask>("BackgroundTask")
        .property("id", &BackgroundTask::id)
        .property("name", &BackgroundTask::name)
        .property("startTime", &BackgroundTask::startTime)
        .property("duration", &BackgroundTask::duration)
        .property("status", &BackgroundTask::status)
        .property("dataSize", &BackgroundTask::dataSize);

    register_vector<BackgroundTask>("VectorBackgroundTask");

    class_<ServiceWorkerMessage>("ServiceWorkerMessage")
        .property("id", &ServiceWorkerMessage::id)
        .property("timestamp", &ServiceWorkerMessage::timestamp)
        .property("fromSource", &ServiceWorkerMessage::fromSource)
        .property("toTarget", &ServiceWorkerMessage::toTarget)
        .property("payloadSize", &ServiceWorkerMessage::payloadSize)
        .property("status", &ServiceWorkerMessage::status);

    register_vector<ServiceWorkerMessage>("VectorServiceWorkerMessage");

    class_<CacheInfo>("CacheInfo")
        .property("cacheName", &CacheInfo::cacheName)
        .property("entryCount", &CacheInfo::entryCount)
        .property("totalSize", &CacheInfo::totalSize);

    register_vector<CacheInfo>("VectorCacheInfo");

    class_<SyncEvent>("SyncEvent")
        .property("id", &SyncEvent::id)
        .property("tag", &SyncEvent::tag)
        .property("status", &SyncEvent::status)
        .property("retryCount", &SyncEvent::retryCount);

    register_vector<SyncEvent>("VectorSyncEvent");

    class_<ServiceWorkerMonitor>("ServiceWorkerMonitor")
        .constructor()
        .function("registerWorker", &ServiceWorkerMonitor::registerWorker)
        .function("updateWorkerState", select_overload<bool(
            const std::string&,
            ServiceWorkerState
        )>(&ServiceWorkerMonitor::updateWorkerState))
        .function("recordTask", select_overload<std::string(
            const std::string&,
            MessageType,
            long long,
            long long,
            const std::string&,
            size_t,
            bool
        )>(&ServiceWorkerMonitor::recordTask))
        .function("recordMessage", select_overload<std::string(
            MessageType,
            const std::string&,
            const std::string&,
            const std::string&
        )>(&ServiceWorkerMonitor::recordMessage))
        .function("addCacheInfo", select_overload<void(
            const std::string&,
            const std::vector<std::pair<std::string, size_t>>&
        )>(&ServiceWorkerMonitor::addCacheInfo))
        .function("scheduleSync", &ServiceWorkerMonitor::scheduleSync)
        .function("getWorkers", &ServiceWorkerMonitor::getWorkers)
        .function("getActiveWorker", &ServiceWorkerMonitor::getActiveWorker)
        .function("getTasks", &ServiceWorkerMonitor::getTasks)
        .function("getMessages", &ServiceWorkerMonitor::getMessages)
        .function("getCaches", &ServiceWorkerMonitor::getCaches)
        .function("getSyncEvents", &ServiceWorkerMonitor::getSyncEvents)
        .function("getTotalCacheSize", &ServiceWorkerMonitor::getTotalCacheSize)
        .function("getTaskStats", &ServiceWorkerMonitor::getTaskStats)
        .function("unregisterWorker", &ServiceWorkerMonitor::unregisterWorker)
        .function("stopWorker", &ServiceWorkerMonitor::stopWorker)
        .function("startWorker", &ServiceWorkerMonitor::startWorker)
        .function("triggerPush", &ServiceWorkerMonitor::triggerPush)
        .function("triggerSync", &ServiceWorkerMonitor::triggerSync)
        .function("clear", &ServiceWorkerMonitor::clear);

    enum_<ServiceWorkerState>("ServiceWorkerState")
        .value("INSTALLING", ServiceWorkerState::INSTALLING)
        .value("INSTALLED", ServiceWorkerState::INSTALLED)
        .value("ACTIVATING", ServiceWorkerState::ACTIVATING)
        .value("ACTIVATED", ServiceWorkerState::ACTIVATED)
        .value("REDUNDANT", ServiceWorkerState::REDUNDANT);

    enum_<MessageType>("MessageType")
        .value("PUSH", MessageType::PUSH)
        .value("SYNC", MessageType::SYNC)
        .value("MESSAGE", MessageType::MESSAGE)
        .value("FETCH_EVENT", MessageType::FETCH_EVENT)
        .value("CACHE_UPDATE", MessageType::CACHE_UPDATE);
}
#endif
