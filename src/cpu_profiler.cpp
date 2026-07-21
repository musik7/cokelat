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
// CPU PROFILER & FLAME CHART
// ==========================================

struct TraceEvent {
    std::string name;
    std::string category;
    std::string phase; // B (begin), E (end), I (instant), X (complete)
    long long timestamp; // microsecond resolution ideally
    long long duration; // for 'X' phase
    int threadId;
    int processId;
    std::string argsJson; // JSON representation of arguments
};

struct ProfileNode {
    int id;
    std::string functionName;
    std::string scriptId;
    std::string url;
    int lineNumber;
    int columnNumber;
    long long hitCount;
    std::vector<int> children; // Child node IDs
};

class CpuProfiler {
private:
    std::vector<TraceEvent> traceEvents;
    std::vector<ProfileNode> profileNodes;
    std::vector<int> profileSamples; // Sequence of node IDs
    std::vector<long long> sampleTimestamps;

    bool isRecording;
    long long startTime;

    int nodeIdCounter = 0;

public:
    CpuProfiler() : isRecording(false), startTime(0) {}

    void startRecording() {
        isRecording = true;
        auto now = std::chrono::high_resolution_clock::now();
        startTime = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        
        traceEvents.clear();
        profileNodes.clear();
        profileSamples.clear();
        sampleTimestamps.clear();
        nodeIdCounter = 0;

        // Add root node
        profileNodes.push_back({
            nodeIdCounter++, "(root)", "", "", 0, 0, 0, {}
        });
    }

    void stopRecording() {
        isRecording = false;
    }

    void addTraceEvent(const std::string& name, const std::string& category, const std::string& phase, long long timestamp, long long duration, int tid, int pid, const std::string& argsJson) {
        if (!isRecording) return;
        traceEvents.push_back({name, category, phase, timestamp, duration, tid, pid, argsJson});
    }

    int addProfileNode(const std::string& functionName, const std::string& url, int lineNumber, int parentId) {
        if (!isRecording) return -1;
        int id = nodeIdCounter++;
        profileNodes.push_back({id, functionName, "", url, lineNumber, 0, 0, {}});
        if (parentId >= 0 && parentId < profileNodes.size()) {
            profileNodes[parentId].children.push_back(id);
        }
        return id;
    }

    void addSample(int nodeId, long long timestamp) {
        if (!isRecording) return;
        profileSamples.push_back(nodeId);
        sampleTimestamps.push_back(timestamp);
        if (nodeId >= 0 && nodeId < profileNodes.size()) {
            profileNodes[nodeId].hitCount++;
        }
    }

    const std::vector<TraceEvent>& getTraceEvents() const { return traceEvents; }
    const std::vector<ProfileNode>& getProfileNodes() const { return profileNodes; }
    const std::vector<int>& getProfileSamples() const { return profileSamples; }
    const std::vector<long long>& getSampleTimestamps() const { return sampleTimestamps; }

    bool getIsRecording() const { return isRecording; }

    void clear() {
        traceEvents.clear();
        profileNodes.clear();
        profileSamples.clear();
        sampleTimestamps.clear();
    }
};

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(cpu_profiler_module) {
    class_<TraceEvent>("TraceEvent")
        .property("name", &TraceEvent::name)
        .property("category", &TraceEvent::category)
        .property("phase", &TraceEvent::phase)
        .property("timestamp", &TraceEvent::timestamp)
        .property("duration", &TraceEvent::duration)
        .property("threadId", &TraceEvent::threadId)
        .property("processId", &TraceEvent::processId)
        .property("argsJson", &TraceEvent::argsJson);

    register_vector<TraceEvent>("VectorTraceEvent");

    class_<ProfileNode>("ProfileNode")
        .property("id", &ProfileNode::id)
        .property("functionName", &ProfileNode::functionName)
        .property("scriptId", &ProfileNode::scriptId)
        .property("url", &ProfileNode::url)
        .property("lineNumber", &ProfileNode::lineNumber)
        .property("columnNumber", &ProfileNode::columnNumber)
        .property("hitCount", &ProfileNode::hitCount)
        .property("children", &ProfileNode::children);

    register_vector<ProfileNode>("VectorProfileNode");
    register_vector<int>("VectorIntProfileSamples");
    register_vector<long long>("VectorLongLongTimestamps");

    class_<CpuProfiler>("CpuProfiler")
        .constructor()
        .function("startRecording", &CpuProfiler::startRecording)
        .function("stopRecording", &CpuProfiler::stopRecording)
        .function("addTraceEvent", &CpuProfiler::addTraceEvent)
        .function("addProfileNode", &CpuProfiler::addProfileNode)
        .function("addSample", &CpuProfiler::addSample)
        .function("getTraceEvents", &CpuProfiler::getTraceEvents)
        .function("getProfileNodes", &CpuProfiler::getProfileNodes)
        .function("getProfileSamples", &CpuProfiler::getProfileSamples)
        .function("getSampleTimestamps", &CpuProfiler::getSampleTimestamps)
        .function("getIsRecording", &CpuProfiler::getIsRecording)
        .function("clear", &CpuProfiler::clear);
}
#endif
