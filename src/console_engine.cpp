/**
 * @file console_engine.cpp
 * @brief Console API Interceptor for Mobile DevTools (WASM)
 * 
 * Captures console.log, console.error, console.warn, console.table, etc.
 * Designed for low-latency mobile with circular buffer and minimal overhead.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. CONSOLE LOG STRUCTURES
// ==========================================

enum class ConsoleLevel {
    LOG,
    INFO,
    WARN,
    ERROR,
    DEBUG,
    TRACE,
    TABLE
};

struct ConsoleMessage {
    std::string id;
    ConsoleLevel level;
    long long timestamp;
    std::vector<std::string> args;
    std::string sourceFile;
    int sourceLine;
    int sourceColumn;
    std::string stackTrace;
    int groupDepth;
    bool isGroupStart;
    bool isGroupEnd;

    std::string getLevelString() const {
        switch (level) {
            case ConsoleLevel::LOG: return "log";
            case ConsoleLevel::INFO: return "info";
            case ConsoleLevel::WARN: return "warn";
            case ConsoleLevel::ERROR: return "error";
            case ConsoleLevel::DEBUG: return "debug";
            case ConsoleLevel::TRACE: return "trace";
            case ConsoleLevel::TABLE: return "table";
            default: return "unknown";
        }
    }
};

// ==========================================
// 2. CONSOLE ENGINE CORE
// ==========================================

class ConsoleEngine {
private:
    std::vector<ConsoleMessage> messages;
    static const size_t MAX_MESSAGES = 5000; // Increased from telemetry 200
    static const size_t MAX_ARGS_PER_MESSAGE = 20;
    int groupDepth = 0;
    size_t messageIdCounter = 0;
    std::map<std::string, size_t> levelCounts; // For stats
    std::map<std::string, long long> timers;
    std::map<std::string, int> counters;

    std::string generateId() {
        return "msg_" + std::to_string(messageIdCounter++);
    }

    std::string sanitizeArg(const std::string& arg) {
        // Truncate very long strings to prevent memory bloat
        if (arg.length() > 1000) {
            return arg.substr(0, 1000) + "...[truncated]";
        }
        return arg;
    }

public:
    ConsoleEngine() {
        messages.reserve(MAX_MESSAGES);
        levelCounts["log"] = 0;
        levelCounts["info"] = 0;
        levelCounts["warn"] = 0;
        levelCounts["error"] = 0;
        levelCounts["debug"] = 0;
        levelCounts["trace"] = 0;
        levelCounts["table"] = 0;
    }

    // Capture console.log/info/warn/error/debug/trace
    void captureMessage(
        ConsoleLevel level,
        const std::vector<std::string>& args,
        const std::string& sourceFile = "",
        int sourceLine = -1,
        int sourceColumn = -1,
        const std::string& stackTrace = ""
    ) {
        if (messages.size() >= MAX_MESSAGES) {
            messages.erase(messages.begin()); // FIFO circular buffer
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::vector<std::string> sanitizedArgs;
        size_t argCount = std::min(args.size(), MAX_ARGS_PER_MESSAGE);
        for (size_t i = 0; i < argCount; ++i) {
            sanitizedArgs.push_back(sanitizeArg(args[i]));
        }

        messages.push_back({
            generateId(),
            level,
            ms,
            sanitizedArgs,
            sourceFile,
            sourceLine,
            sourceColumn,
            stackTrace,
            groupDepth,
            false,
            false
        });

        levelCounts[std::string(messages.back().getLevelString())]++;
    }

    // Group logging for organized output
    void startGroup(const std::string& label) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        messages.push_back({
            generateId(),
            ConsoleLevel::LOG,
            ms,
            {label},
            "",
            -1,
            -1,
            "",
            groupDepth,
            true,
            false
        });

        groupDepth++;
    }

    void endGroup() {
        if (groupDepth > 0) groupDepth--;
    }

    // Timer functionality
    void time(const std::string& label) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        timers[label] = ms;
    }

    void timeEnd(const std::string& label) {
        auto it = timers.find(label);
        if (it != timers.end()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            long long duration = ms - it->second;
            captureMessage(ConsoleLevel::INFO, {label + ": " + std::to_string(duration) + " ms"});
            timers.erase(it);
        } else {
            captureMessage(ConsoleLevel::WARN, {"Timer '" + label + "' does not exist"});
        }
    }
    
    void timeLog(const std::string& label) {
        auto it = timers.find(label);
        if (it != timers.end()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            long long duration = ms - it->second;
            captureMessage(ConsoleLevel::INFO, {label + ": " + std::to_string(duration) + " ms"});
        } else {
            captureMessage(ConsoleLevel::WARN, {"Timer '" + label + "' does not exist"});
        }
    }

    // Counter functionality
    void count(const std::string& label) {
        counters[label]++;
        captureMessage(ConsoleLevel::INFO, {label + ": " + std::to_string(counters[label])});
    }

    void countReset(const std::string& label) {
        counters[label] = 0;
        captureMessage(ConsoleLevel::INFO, {label + ": 0"});
    }

    // Get all messages or filtered
    const std::vector<ConsoleMessage>& getMessages() const {
        return messages;
    }

    std::vector<ConsoleMessage> getMessagesByLevel(ConsoleLevel level) const {
        std::vector<ConsoleMessage> filtered;
        for (const auto& msg : messages) {
            if (msg.level == level) {
                filtered.push_back(msg);
            }
        }
        return filtered;
    }

    std::vector<ConsoleMessage> getMessagesByLevelString(const std::string& levelStr) const {
        ConsoleLevel level = ConsoleLevel::LOG;
        if (levelStr == "info") level = ConsoleLevel::INFO;
        else if (levelStr == "warn") level = ConsoleLevel::WARN;
        else if (levelStr == "error") level = ConsoleLevel::ERROR;
        else if (levelStr == "debug") level = ConsoleLevel::DEBUG;
        else if (levelStr == "trace") level = ConsoleLevel::TRACE;
        else if (levelStr == "table") level = ConsoleLevel::TABLE;

        return getMessagesByLevel(level);
    }

    // Search in console messages
    std::vector<ConsoleMessage> searchMessages(const std::string& query) const {
        std::vector<ConsoleMessage> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (const auto& msg : messages) {
            for (const auto& arg : msg.args) {
                std::string lowerArg = arg;
                std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);
                if (lowerArg.find(lowerQuery) != std::string::npos) {
                    results.push_back(msg);
                    break;
                }
            }
        }
        return results;
    }

    // Get statistics
    std::map<std::string, size_t> getStats() const {
        return levelCounts;
    }

    size_t getTotalMessages() const {
        return messages.size();
    }

    size_t getErrorCount() const {
        return levelCounts.at("error");
    }

    size_t getWarningCount() const {
        return levelCounts.at("warn");
    }

    void clear() {
        messages.clear();
        groupDepth = 0;
        messageIdCounter = 0;
        for (auto& kv : levelCounts) {
            kv.second = 0;
        }
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(console_engine_module) {
    class_<ConsoleMessage>("ConsoleMessage")
        .property("id", &ConsoleMessage::id)
        .property("level", &ConsoleMessage::getLevelString)
        .property("timestamp", &ConsoleMessage::timestamp)
        .property("args", &ConsoleMessage::args)
        .property("sourceFile", &ConsoleMessage::sourceFile)
        .property("sourceLine", &ConsoleMessage::sourceLine)
        .property("sourceColumn", &ConsoleMessage::sourceColumn)
        .property("stackTrace", &ConsoleMessage::stackTrace)
        .property("groupDepth", &ConsoleMessage::groupDepth)
        .property("isGroupStart", &ConsoleMessage::isGroupStart)
        .property("isGroupEnd", &ConsoleMessage::isGroupEnd);

    register_vector<ConsoleMessage>("VectorConsoleMessage");
    register_map<std::string, size_t>("MapStringSize");

    class_<ConsoleEngine>("ConsoleEngine")
        .constructor()
        .function("captureMessage", select_overload<void(
            ConsoleLevel,
            const std::vector<std::string>&,
            const std::string&,
            int,
            int,
            const std::string&
        )>(&ConsoleEngine::captureMessage))
        .function("startGroup", &ConsoleEngine::startGroup)
        .function("endGroup", &ConsoleEngine::endGroup)
        .function("time", &ConsoleEngine::time)
        .function("timeEnd", &ConsoleEngine::timeEnd)
        .function("timeLog", &ConsoleEngine::timeLog)
        .function("count", &ConsoleEngine::count)
        .function("countReset", &ConsoleEngine::countReset)
        .function("getMessages", &ConsoleEngine::getMessages)
        .function("getMessagesByLevelString", &ConsoleEngine::getMessagesByLevelString)
        .function("searchMessages", &ConsoleEngine::searchMessages)
        .function("getStats", &ConsoleEngine::getStats)
        .function("getTotalMessages", &ConsoleEngine::getTotalMessages)
        .function("getErrorCount", &ConsoleEngine::getErrorCount)
        .function("getWarningCount", &ConsoleEngine::getWarningCount)
        .function("clear", &ConsoleEngine::clear);

    enum_<ConsoleLevel>("ConsoleLevel")
        .value("LOG", ConsoleLevel::LOG)
        .value("INFO", ConsoleLevel::INFO)
        .value("WARN", ConsoleLevel::WARN)
        .value("ERROR", ConsoleLevel::ERROR)
        .value("DEBUG", ConsoleLevel::DEBUG)
        .value("TRACE", ConsoleLevel::TRACE)
        .value("TABLE", ConsoleLevel::TABLE);
}
#endif
