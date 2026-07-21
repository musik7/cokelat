/**
 * @file stack_trace_engine.cpp
 * @brief Stack Trace & Source Map Engine for Mobile DevTools (WASM)
 * 
 * Capture and decode stack traces with source mapping support for
 * minified/transpiled code.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. STACK TRACE STRUCTURES
// ==========================================

struct StackFrame {
    std::string functionName;
    std::string fileName;
    int lineNumber;
    int columnNumber;
    std::string source; // Original line of code if available
    bool isNative;
    bool isMapped; // Whether source map was applied
    std::string originalFunctionName; // Before minification
    std::string originalFileName; // Before bundling
    int originalLineNumber;
    int originalColumnNumber;
};

struct StackTrace {
    std::string id;
    std::string message;
    std::string errorType; // "TypeError", "ReferenceError", etc.
    long long timestamp;
    std::vector<StackFrame> frames;
    std::string url; // Page URL where error occurred
    int severity; // 0 = info, 1 = warning, 2 = error, 3 = critical
};

struct SourceMap {
    std::string sourceFile; // Original file
    std::string bundledFile; // Minified file
    std::map<int, int> lineMapping; // bundled line -> original line
    std::map<int, int> columnMapping; // bundled col -> original col
    std::vector<std::string> sources;
};

// ==========================================
// 2. STACK TRACE PARSER
// ==========================================

class StackTraceParser {
public:
    static std::vector<StackFrame> parseStackTrace(const std::string& stackString) {
        std::vector<StackFrame> frames;
        std::istringstream stream(stackString);
        std::string line;
        int frameIndex = 0;

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            StackFrame frame = parseFrame(line, frameIndex++);
            frames.push_back(frame);
        }

        return frames;
    }

private:
    static StackFrame parseFrame(const std::string& line, int index) {
        StackFrame frame = {
            "",
            "",
            -1,
            -1,
            line,
            false,
            false,
            "",
            "",
            -1,
            -1
        };

        // Try to parse "at functionName (file:line:col)" format
        size_t atPos = line.find("at ");
        if (atPos == std::string::npos) return frame;

        size_t funcStart = atPos + 3;
        size_t parenPos = line.find('(', funcStart);
        if (parenPos != std::string::npos) {
            frame.functionName = trim(line.substr(funcStart, parenPos - funcStart));
        }

        // Parse file:line:col
        size_t colonPos1 = line.rfind(':');
        if (colonPos1 != std::string::npos) {
            // Get column
            std::string colStr = line.substr(colonPos1 + 1);
            colStr = colStr.substr(0, colStr.find(')'));
            frame.columnNumber = std::stoi(colStr);
        }

        size_t colonPos2 = line.rfind(':', colonPos1 - 1);
        if (colonPos2 != std::string::npos) {
            // Get line
            std::string lineStr = line.substr(colonPos2 + 1, colonPos1 - colonPos2 - 1);
            frame.lineNumber = std::stoi(lineStr);
        }

        // Get filename
        size_t parenStart = line.find('(');
        if (parenStart != std::string::npos && colonPos2 != std::string::npos) {
            frame.fileName = trim(line.substr(parenStart + 1, colonPos2 - parenStart - 1));
        }

        return frame;
    }

    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
};

// ==========================================
// 3. STACK TRACE ENGINE
// ==========================================

class StackTraceEngine {
private:
    std::vector<StackTrace> traces;
    std::map<std::string, SourceMap> sourceMaps;
    static const size_t MAX_TRACES = 1000;
    size_t traceIdCounter = 0;

    std::string generateId() {
        return "trace_" + std::to_string(traceIdCounter++);
    }

public:
    StackTraceEngine() {
        traces.reserve(MAX_TRACES);
    }

    // Capture new stack trace
    std::string captureTrace(
        const std::string& message,
        const std::string& errorType,
        const std::string& stackString,
        const std::string& url = "",
        int severity = 2
    ) {
        if (traces.size() >= MAX_TRACES) {
            traces.erase(traces.begin()); // FIFO
        }

        std::string id = generateId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::vector<StackFrame> frames = StackTraceParser::parseStackTrace(stackString);

        // Try to apply source maps
        for (auto& frame : frames) {
            applySourceMap(frame);
        }

        traces.push_back({
            id,
            message,
            errorType,
            ms,
            frames,
            url,
            severity
        });

        return id;
    }

    // Register source map
    void registerSourceMap(
        const std::string& bundledFile,
        const std::string& originalSourceFile,
        const std::map<int, int>& lineMapping,
        const std::map<int, int>& columnMapping
    ) {
        sourceMaps[bundledFile] = {
            originalSourceFile,
            bundledFile,
            lineMapping,
            columnMapping,
            {}
        };
    }

    // Apply source map to a frame
    void applySourceMap(StackFrame& frame) {
        auto it = sourceMaps.find(frame.fileName);
        if (it == sourceMaps.end()) return;

        const auto& sourceMap = it->second;

        // Map line number
        auto lineIt = sourceMap.lineMapping.find(frame.lineNumber);
        if (lineIt != sourceMap.lineMapping.end()) {
            frame.originalLineNumber = lineIt->second;
            frame.originalFileName = sourceMap.sourceFile;
            frame.isMapped = true;
        }

        // Map column number
        auto colIt = sourceMap.columnMapping.find(frame.columnNumber);
        if (colIt != sourceMap.columnMapping.end()) {
            frame.originalColumnNumber = colIt->second;
        }
    }

    // Get all traces
    const std::vector<StackTrace>& getTraces() const {
        return traces;
    }

    // Get traces by error type
    std::vector<StackTrace> getTracesByErrorType(const std::string& errorType) const {
        std::vector<StackTrace> filtered;
        for (const auto& trace : traces) {
            if (trace.errorType == errorType) {
                filtered.push_back(trace);
            }
        }
        return filtered;
    }

    // Get critical errors (severity >= 2)
    std::vector<StackTrace> getCriticalErrors() const {
        std::vector<StackTrace> filtered;
        for (const auto& trace : traces) {
            if (trace.severity >= 2) {
                filtered.push_back(trace);
            }
        }
        return filtered;
    }

    // Search traces by message or file
    std::vector<StackTrace> searchTraces(const std::string& query) const {
        std::vector<StackTrace> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (const auto& trace : traces) {
            std::string lowerMsg = trace.message;
            std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);

            if (lowerMsg.find(lowerQuery) != std::string::npos) {
                results.push_back(trace);
                continue;
            }

            for (const auto& frame : trace.frames) {
                std::string lowerFile = frame.fileName;
                std::transform(lowerFile.begin(), lowerFile.end(), lowerFile.begin(), ::tolower);
                if (lowerFile.find(lowerQuery) != std::string::npos) {
                    results.push_back(trace);
                    break;
                }
            }
        }
        return results;
    }

    // Get statistics
    std::map<std::string, size_t> getStats() const {
        std::map<std::string, size_t> stats;
        stats["totalTraces"] = traces.size();
        stats["totalMappedFiles"] = sourceMaps.size();

        for (const auto& trace : traces) {
            stats[trace.errorType]++;
        }
        return stats;
    }

    void clear() {
        traces.clear();
        sourceMaps.clear();
        traceIdCounter = 0;
    }
};

// ==========================================
// 4. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(stack_trace_engine_module) {
    class_<StackFrame>("StackFrame")
        .property("functionName", &StackFrame::functionName)
        .property("fileName", &StackFrame::fileName)
        .property("lineNumber", &StackFrame::lineNumber)
        .property("columnNumber", &StackFrame::columnNumber)
        .property("source", &StackFrame::source)
        .property("isNative", &StackFrame::isNative)
        .property("isMapped", &StackFrame::isMapped)
        .property("originalFunctionName", &StackFrame::originalFunctionName)
        .property("originalFileName", &StackFrame::originalFileName)
        .property("originalLineNumber", &StackFrame::originalLineNumber)
        .property("originalColumnNumber", &StackFrame::originalColumnNumber);

    register_vector<StackFrame>("VectorStackFrame");

    class_<StackTrace>("StackTrace")
        .property("id", &StackTrace::id)
        .property("message", &StackTrace::message)
        .property("errorType", &StackTrace::errorType)
        .property("timestamp", &StackTrace::timestamp)
        .property("frames", &StackTrace::frames)
        .property("url", &StackTrace::url)
        .property("severity", &StackTrace::severity);

    register_vector<StackTrace>("VectorStackTrace");

    class_<StackTraceEngine>("StackTraceEngine")
        .constructor()
        .function("captureTrace", select_overload<std::string(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            int
        )>(&StackTraceEngine::captureTrace))
        .function("registerSourceMap", select_overload<void(
            const std::string&,
            const std::string&,
            const std::map<int, int>&,
            const std::map<int, int>&
        )>(&StackTraceEngine::registerSourceMap))
        .function("getTraces", &StackTraceEngine::getTraces)
        .function("getTracesByErrorType", &StackTraceEngine::getTracesByErrorType)
        .function("getCriticalErrors", &StackTraceEngine::getCriticalErrors)
        .function("searchTraces", &StackTraceEngine::searchTraces)
        .function("getStats", &StackTraceEngine::getStats)
        .function("clear", &StackTraceEngine::clear);
}
#endif
