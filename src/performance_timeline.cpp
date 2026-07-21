/**
 * @file performance_timeline.cpp
 * @brief Performance Timeline & Waterfall Visualization Engine
 * 
 * Track and visualize performance metrics in a timeline format with
 * critical path analysis for mobile optimization.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <algorithm>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. PERFORMANCE EVENT STRUCTURES
// ==========================================

enum class PerformanceEventType {
    NAVIGATION,
    RESOURCE,
    PAINT,
    LAYOUT,
    SCRIPT,
    STYLE,
    MEMORY,
    INTERACTION,
    LONG_TASK,
    CUSTOM
};

struct PerformanceMetric {
    std::string name;
    long long timestamp;      // DOMHighResTimeStamp (relative to navigation start)
    long long duration;       // milliseconds
    std::string category;     // FCP, LCP, CLS, etc.
    double value;            // For metric values (CLS score, memory MB, etc.)
    bool isOnCriticalPath;   // Blocks other resources?
};

struct PerformanceEvent {
    std::string id;
    PerformanceEventType type;
    std::string name;
    long long startTime;      // ms from navigation start
    long long endTime;        // ms from navigation start
    long long duration;       // endTime - startTime
    std::string resourceType; // "image", "stylesheet", "script", etc.
    size_t transferSize;      // bytes transferred
    size_t decodedBodySize;   // bytes decoded
    std::string initiatorType; // "link", "script", "img", "fetch", etc.
    bool isBlocking;          // Blocks rendering?
    int dependsOnId;          // ID of dependency (-1 if none)
    std::vector<int> blockedBy; // IDs of events blocking this one
    std::map<std::string, double> metadata; // Custom key-value pairs
};

struct TimelineFrame {
    long long timestamp;
    double fps;             // Frames per second at this moment
    size_t memoryUsed;      // Bytes
    size_t memoryDelta;     // Change from previous
    std::vector<std::string> eventIds; // Events in this frame
};

struct CriticalPathAnalysis {
    std::vector<int> criticalEventIds;  // IDs of critical events
    long long totalCriticalTime;        // Total time on critical path
    double criticalPathRatio;           // Critical / Total execution time
    int bottleneckEventId;              // Slowest event on critical path
    long long bottleneckDuration;       // Duration of bottleneck
    std::vector<std::string> recommendations; // Optimization tips
};

struct PerformanceReport {
    double fcp;  // First Contentful Paint (ms)
    double lcp;  // Largest Contentful Paint (ms)
    double cls;  // Cumulative Layout Shift
    double fid;  // First Input Delay (ms)
    double ttfb; // Time to First Byte (ms)
    long long domReady;    // DOM Ready Time (ms)
    long long pageLoadTime; // Total page load time (ms)
    double avgFrameTime;   // Average frame duration (ms)
    double maxFrameTime;   // Worst frame duration (ms)
    size_t peakMemory;     // Peak memory usage (bytes)
    int longTaskCount;     // Tasks > 50ms
};

// ==========================================
// 2. PERFORMANCE TIMELINE ENGINE
// ==========================================

class PerformanceTimeline {
private:
    std::vector<PerformanceEvent> events;
    std::vector<TimelineFrame> frames;
    std::vector<PerformanceMetric> metrics;
    long long navigationStart = 0;
    size_t eventIdCounter = 0;
    static const size_t MAX_EVENTS = 5000;
    static const size_t MAX_FRAMES = 1000;

    std::string generateEventId() {
        return "perf_" + std::to_string(eventIdCounter++);
    }

    bool isOnCriticalPath(const PerformanceEvent& event) {
        // Event is critical if it blocks rendering or is render-blocking
        return event.isBlocking || event.type == PerformanceEventType::SCRIPT ||
               event.type == PerformanceEventType::STYLE;
    }

public:
    PerformanceTimeline() {
        events.reserve(MAX_EVENTS);
        frames.reserve(MAX_FRAMES);
        auto now = std::chrono::high_resolution_clock::now();
        navigationStart = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }

    // Record performance event
    std::string addEvent(
        PerformanceEventType type,
        const std::string& name,
        long long startTime,
        long long endTime,
        const std::string& resourceType = "",
        size_t transferSize = 0,
        size_t decodedBodySize = 0,
        const std::string& initiatorType = "",
        bool isBlocking = false,
        int dependsOnId = -1
    ) {
        if (events.size() >= MAX_EVENTS) {
            events.erase(events.begin());
        }

        std::string id = generateEventId();
        long long duration = endTime - startTime;

        events.push_back({
            id,
            type,
            name,
            startTime,
            endTime,
            duration,
            resourceType,
            transferSize,
            decodedBodySize,
            initiatorType,
            isBlocking,
            dependsOnId,
            {},
            {}
        });

        return id;
    }

    // Record frame (for FPS/memory tracking)
    void recordFrame(
        long long timestamp,
        double fps,
        size_t memoryUsed,
        size_t memoryDelta = 0
    ) {
        if (frames.size() >= MAX_FRAMES) {
            frames.erase(frames.begin());
        }

        frames.push_back({
            timestamp,
            fps,
            memoryUsed,
            memoryDelta,
            {}
        });
    }

    // Record performance metric (FCP, LCP, CLS, etc.)
    void recordMetric(
        const std::string& name,
        long long timestamp,
        const std::string& category,
        double value = 0.0
    ) {
        metrics.push_back({
            name,
            timestamp,
            0,
            category,
            value,
            false
        });
    }

    // Get all events
    const std::vector<PerformanceEvent>& getEvents() const {
        return events;
    }

    // Filter events by type
    std::vector<PerformanceEvent> getEventsByType(PerformanceEventType type) const {
        std::vector<PerformanceEvent> filtered;
        for (const auto& event : events) {
            if (event.type == type) {
                filtered.push_back(event);
            }
        }
        return filtered;
    }

    // Get timeline frames
    const std::vector<TimelineFrame>& getFrames() const {
        return frames;
    }

    // Get metrics
    const std::vector<PerformanceMetric>& getMetrics() const {
        return metrics;
    }

    // Analyze critical path
    CriticalPathAnalysis analyzeCriticalPath() const {
        CriticalPathAnalysis analysis = {{}, 0, 0, -1, 0, {}};

        if (events.empty()) return analysis;

        // Find critical events
        for (size_t i = 0; i < events.size(); i++) {
            if (isOnCriticalPath(events[i])) {
                analysis.criticalEventIds.push_back(i);
                analysis.totalCriticalTime += events[i].duration;

                if (events[i].duration > analysis.bottleneckDuration) {
                    analysis.bottleneckDuration = events[i].duration;
                    analysis.bottleneckEventId = i;
                }
            }
        }

        // Calculate total execution time
        long long totalTime = 0;
        if (!events.empty()) {
            auto maxEnd = std::max_element(events.begin(), events.end(),
                [](const PerformanceEvent& a, const PerformanceEvent& b) {
                    return a.endTime < b.endTime;
                });
            totalTime = maxEnd->endTime - events[0].startTime;
        }

        if (totalTime > 0) {
            analysis.criticalPathRatio = (double)analysis.totalCriticalTime / (double)totalTime;
        }

        // Generate recommendations
        if (analysis.criticalPathRatio > 0.8) {
            analysis.recommendations.push_back("High critical path ratio - consider code splitting");
        }
        if (analysis.bottleneckDuration > 1000) {
            analysis.recommendations.push_back("Bottleneck event > 1s - analyze and optimize");
        }

        return analysis;
    }

    // Generate performance report
    PerformanceReport generateReport() const {
        PerformanceReport report = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        // Find FCP, LCP, CLS
        for (const auto& metric : metrics) {
            if (metric.category == "FCP") report.fcp = metric.value;
            else if (metric.category == "LCP") report.lcp = metric.value;
            else if (metric.category == "CLS") report.cls = metric.value;
            else if (metric.category == "FID") report.fid = metric.value;
            else if (metric.category == "TTFB") report.ttfb = metric.value;
        }

        // Find DOM Ready and Page Load time
        for (const auto& event : events) {
            if (event.type == PerformanceEventType::NAVIGATION) {
                report.domReady = event.endTime;
                report.pageLoadTime = event.endTime;
            }
        }

        // Calculate frame stats
        double totalFrameTime = 0;
        double maxFrameTime = 0;
        if (!frames.empty()) {
            for (size_t i = 1; i < frames.size(); i++) {
                double frameTime = frames[i].timestamp - frames[i - 1].timestamp;
                totalFrameTime += frameTime;
                maxFrameTime = std::max(maxFrameTime, frameTime);
            }
            report.avgFrameTime = totalFrameTime / (double)(frames.size() - 1);
            report.maxFrameTime = maxFrameTime;
        }

        // Find peak memory
        for (const auto& frame : frames) {
            report.peakMemory = std::max(report.peakMemory, frame.memoryUsed);
        }

        // Count long tasks
        for (const auto& event : events) {
            if (event.duration > 50) {
                report.longTaskCount++;
            }
        }

        return report;
    }

    // Get events in time range
    std::vector<PerformanceEvent> getEventsInRange(long long startTime, long long endTime) const {
        std::vector<PerformanceEvent> result;
        for (const auto& event : events) {
            if (event.startTime >= startTime && event.endTime <= endTime) {
                result.push_back(event);
            }
        }
        return result;
    }

    void clear() {
        events.clear();
        frames.clear();
        metrics.clear();
        eventIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(performance_timeline_module) {
    class_<PerformanceEvent>("PerformanceEvent")
        .property("id", &PerformanceEvent::id)
        .property("name", &PerformanceEvent::name)
        .property("startTime", &PerformanceEvent::startTime)
        .property("endTime", &PerformanceEvent::endTime)
        .property("duration", &PerformanceEvent::duration)
        .property("resourceType", &PerformanceEvent::resourceType)
        .property("transferSize", &PerformanceEvent::transferSize)
        .property("decodedBodySize", &PerformanceEvent::decodedBodySize)
        .property("isBlocking", &PerformanceEvent::isBlocking);

    register_vector<PerformanceEvent>("VectorPerformanceEvent");

    class_<TimelineFrame>("TimelineFrame")
        .property("timestamp", &TimelineFrame::timestamp)
        .property("fps", &TimelineFrame::fps)
        .property("memoryUsed", &TimelineFrame::memoryUsed)
        .property("memoryDelta", &TimelineFrame::memoryDelta);

    register_vector<TimelineFrame>("VectorTimelineFrame");

    class_<PerformanceMetric>("PerformanceMetric")
        .property("name", &PerformanceMetric::name)
        .property("timestamp", &PerformanceMetric::timestamp)
        .property("category", &PerformanceMetric::category)
        .property("value", &PerformanceMetric::value);

    register_vector<PerformanceMetric>("VectorPerformanceMetric");

    class_<CriticalPathAnalysis>("CriticalPathAnalysis")
        .property("criticalEventIds", &CriticalPathAnalysis::criticalEventIds)
        .property("totalCriticalTime", &CriticalPathAnalysis::totalCriticalTime)
        .property("criticalPathRatio", &CriticalPathAnalysis::criticalPathRatio)
        .property("bottleneckDuration", &CriticalPathAnalysis::bottleneckDuration)
        .property("recommendations", &CriticalPathAnalysis::recommendations);

    register_vector<int>("VectorInt");
    register_vector<std::string>("VectorString");

    class_<PerformanceReport>("PerformanceReport")
        .property("fcp", &PerformanceReport::fcp)
        .property("lcp", &PerformanceReport::lcp)
        .property("cls", &PerformanceReport::cls)
        .property("fid", &PerformanceReport::fid)
        .property("ttfb", &PerformanceReport::ttfb)
        .property("domReady", &PerformanceReport::domReady)
        .property("pageLoadTime", &PerformanceReport::pageLoadTime)
        .property("avgFrameTime", &PerformanceReport::avgFrameTime)
        .property("maxFrameTime", &PerformanceReport::maxFrameTime)
        .property("peakMemory", &PerformanceReport::peakMemory)
        .property("longTaskCount", &PerformanceReport::longTaskCount);

    class_<PerformanceTimeline>("PerformanceTimeline")
        .constructor()
        .function("addEvent", select_overload<std::string(
            PerformanceEventType,
            const std::string&,
            long long,
            long long,
            const std::string&,
            size_t,
            size_t,
            const std::string&,
            bool,
            int
        )>(&PerformanceTimeline::addEvent))
        .function("recordFrame", select_overload<void(
            long long,
            double,
            size_t,
            size_t
        )>(&PerformanceTimeline::recordFrame))
        .function("recordMetric", select_overload<void(
            const std::string&,
            long long,
            const std::string&,
            double
        )>(&PerformanceTimeline::recordMetric))
        .function("getEvents", &PerformanceTimeline::getEvents)
        .function("getFrames", &PerformanceTimeline::getFrames)
        .function("getMetrics", &PerformanceTimeline::getMetrics)
        .function("analyzeCriticalPath", &PerformanceTimeline::analyzeCriticalPath)
        .function("generateReport", &PerformanceTimeline::generateReport)
        .function("getEventsInRange", &PerformanceTimeline::getEventsInRange)
        .function("clear", &PerformanceTimeline::clear);

    enum_<PerformanceEventType>("PerformanceEventType")
        .value("NAVIGATION", PerformanceEventType::NAVIGATION)
        .value("RESOURCE", PerformanceEventType::RESOURCE)
        .value("PAINT", PerformanceEventType::PAINT)
        .value("LAYOUT", PerformanceEventType::LAYOUT)
        .value("SCRIPT", PerformanceEventType::SCRIPT)
        .value("STYLE", PerformanceEventType::STYLE)
        .value("MEMORY", PerformanceEventType::MEMORY)
        .value("INTERACTION", PerformanceEventType::INTERACTION)
        .value("LONG_TASK", PerformanceEventType::LONG_TASK);
}
#endif
