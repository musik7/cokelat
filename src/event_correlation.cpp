/**
 * @file event_correlation.cpp
 * @brief Event Correlation Engine for Mobile DevTools
 * 
 * Link console events, network requests, and errors to create
 * a unified view of application behavior.
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
// 1. EVENT CORRELATION STRUCTURES
// ==========================================

enum class EventCategory {
    CONSOLE,
    NETWORK,
    ERROR,
    PERFORMANCE,
    USER_INTERACTION,
    STORAGE,
    MEMORY
};

struct CorrelatedEvent {
    std::string id;
    EventCategory category;
    long long timestamp;
    std::string sourceId;  // ID from original event (console msg id, network req id, etc.)
    std::string title;     // Display title
    std::string description; // Event details
    int severity;          // 0=info, 1=warning, 2=error, 3=critical
    std::map<std::string, std::string> metadata;
};

struct EventCorrelation {
    std::string id;
    std::vector<std::string> relatedEventIds; // IDs of correlated events
    std::string correlationType; // "error_chain", "network_flow", "memory_spike", etc.
    std::string description;
    long long startTime;
    long long endTime;
    int severity;
};

struct EventTimeline {
    std::vector<CorrelatedEvent> events;
    std::vector<EventCorrelation> correlations;
};

struct EventInsight {
    std::string title;
    std::string description;
    std::vector<std::string> affectedEventIds;
    int severity;
    std::vector<std::string> suggestions;
};

// ==========================================
// 2. EVENT CORRELATION ENGINE
// ==========================================

class EventCorrelationEngine {
private:
    std::vector<CorrelatedEvent> events;
    std::vector<EventCorrelation> correlations;
    size_t eventIdCounter = 0;
    size_t correlationIdCounter = 0;
    static const size_t MAX_EVENTS = 10000;
    static const long long TIME_WINDOW_MS = 5000; // 5 second correlation window

    std::string generateEventId() {
        return "evt_" + std::to_string(eventIdCounter++);
    }

    std::string generateCorrelationId() {
        return "corr_" + std::to_string(correlationIdCounter++);
    }

    bool isWithinWindow(long long time1, long long time2) {
        return std::abs(time1 - time2) <= TIME_WINDOW_MS;
    }

public:
    EventCorrelationEngine() {
        events.reserve(MAX_EVENTS);
    }

    // Add correlated event
    std::string addEvent(
        EventCategory category,
        long long timestamp,
        const std::string& sourceId,
        const std::string& title,
        const std::string& description,
        int severity = 0
    ) {
        if (events.size() >= MAX_EVENTS) {
            events.erase(events.begin());
        }

        std::string id = generateEventId();
        events.push_back({
            id,
            category,
            timestamp,
            sourceId,
            title,
            description,
            severity,
            {}
        });

        return id;
    }

    // Find correlated events (within time window + related categories)
    std::vector<CorrelatedEvent> findCorrelated(const std::string& eventId) const {
        std::vector<CorrelatedEvent> result;

        // Find the source event
        const CorrelatedEvent* sourceEvent = nullptr;
        for (const auto& evt : events) {
            if (evt.id == eventId) {
                sourceEvent = &evt;
                break;
            }
        }

        if (!sourceEvent) return result;

        // Find related events within time window
        for (const auto& evt : events) {
            if (evt.id == eventId) continue;
            if (!isWithinWindow(sourceEvent->timestamp, evt.timestamp)) continue;

            // Check if categories are related
            bool related = false;
            if ((sourceEvent->category == EventCategory::NETWORK && evt.category == EventCategory::ERROR) ||
                (sourceEvent->category == EventCategory::ERROR && evt.category == EventCategory::CONSOLE) ||
                (sourceEvent->category == EventCategory::CONSOLE && evt.category == EventCategory::NETWORK) ||
                (sourceEvent->category == EventCategory::MEMORY && evt.category == EventCategory::PERFORMANCE)) {
                related = true;
            }

            if (related) {
                result.push_back(evt);
            }
        }

        return result;
    }

    // Auto-correlate all events
    void autoCorrelate() {
        correlations.clear();

        for (const auto& event : events) {
            auto related = findCorrelated(event.id);
            if (!related.empty()) {
                std::vector<std::string> relatedIds;
                for (const auto& rel : related) {
                    relatedIds.push_back(rel.id);
                }

                std::string corrType = "event_chain";
                if (event.category == EventCategory::ERROR) corrType = "error_chain";
                else if (event.category == EventCategory::NETWORK) corrType = "network_flow";
                else if (event.category == EventCategory::MEMORY) corrType = "memory_spike";

                correlations.push_back({
                    generateCorrelationId(),
                    relatedIds,
                    corrType,
                    "",
                    event.timestamp,
                    event.timestamp,
                    event.severity
                });
            }
        }
    }

    // Get events by category
    std::vector<CorrelatedEvent> getEventsByCategory(EventCategory category) const {
        std::vector<CorrelatedEvent> result;
        for (const auto& evt : events) {
            if (evt.category == category) {
                result.push_back(evt);
            }
        }
        return result;
    }

    // Get events in time range
    std::vector<CorrelatedEvent> getEventsInRange(long long startTime, long long endTime) const {
        std::vector<CorrelatedEvent> result;
        for (const auto& evt : events) {
            if (evt.timestamp >= startTime && evt.timestamp <= endTime) {
                result.push_back(evt);
            }
        }
        return result;
    }

    // Detect error chains
    std::vector<EventCorrelation> getErrorChains() const {
        std::vector<EventCorrelation> chains;
        for (const auto& corr : correlations) {
            if (corr.correlationType == "error_chain") {
                chains.push_back(corr);
            }
        }
        return chains;
    }

    // Generate insights
    std::vector<EventInsight> generateInsights() const {
        std::vector<EventInsight> insights;

        // Insight 1: Error spikes
        auto errorEvents = getEventsByCategory(EventCategory::ERROR);
        if (errorEvents.size() > 5) {
            EventInsight insight;
            insight.title = "High Error Rate Detected";
            insight.description = std::to_string(errorEvents.size()) + " errors detected in last period";
            insight.severity = 2;
            for (const auto& err : errorEvents) {
                insight.affectedEventIds.push_back(err.id);
            }
            insight.suggestions.push_back("Check error logs for common patterns");
            insight.suggestions.push_back("Review recent code changes");
            insights.push_back(insight);
        }

        // Insight 2: Slow network requests
        auto networkEvents = getEventsByCategory(EventCategory::NETWORK);
        size_t slowCount = 0;
        for (const auto& net : networkEvents) {
            auto it = net.metadata.find("duration_ms");
            if (it != net.metadata.end() && std::stoll(it->second) > 3000) {
                slowCount++;
            }
        }
        if (slowCount > 0) {
            EventInsight insight;
            insight.title = "Slow Network Requests";
            insight.description = std::to_string(slowCount) + " requests took > 3 seconds";
            insight.severity = 1;
            insight.suggestions.push_back("Optimize API endpoints");
            insight.suggestions.push_back("Consider compression");
            insight.suggestions.push_back("Check network conditions");
            insights.push_back(insight);
        }

        return insights;
    }

    // Get all events
    const std::vector<CorrelatedEvent>& getAllEvents() const {
        return events;
    }

    // Get all correlations
    const std::vector<EventCorrelation>& getAllCorrelations() const {
        return correlations;
    }

    // Get timeline
    EventTimeline getTimeline(long long startTime, long long endTime) const {
        EventTimeline timeline;
        timeline.events = getEventsInRange(startTime, endTime);
        timeline.correlations = correlations; // All correlations shown
        return timeline;
    }

    // Get event statistics
    std::map<std::string, size_t> getStats() const {
        std::map<std::string, size_t> stats;
        stats["totalEvents"] = events.size();
        stats["totalCorrelations"] = correlations.size();
        stats["errorCount"] = 0;
        stats["warningCount"] = 0;
        stats["infoCount"] = 0;

        for (const auto& evt : events) {
            if (evt.severity == 3 || evt.severity == 2) stats["errorCount"]++;
            else if (evt.severity == 1) stats["warningCount"]++;
            else stats["infoCount"]++;
        }

        return stats;
    }

    void clear() {
        events.clear();
        correlations.clear();
        eventIdCounter = 0;
        correlationIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(event_correlation_module) {
    class_<CorrelatedEvent>("CorrelatedEvent")
        .property("id", &CorrelatedEvent::id)
        .property("title", &CorrelatedEvent::title)
        .property("description", &CorrelatedEvent::description)
        .property("timestamp", &CorrelatedEvent::timestamp)
        .property("severity", &CorrelatedEvent::severity);

    register_vector<CorrelatedEvent>("VectorCorrelatedEvent");

    class_<EventCorrelation>("EventCorrelation")
        .property("id", &EventCorrelation::id)
        .property("relatedEventIds", &EventCorrelation::relatedEventIds)
        .property("correlationType", &EventCorrelation::correlationType)
        .property("description", &EventCorrelation::description)
        .property("severity", &EventCorrelation::severity);

    register_vector<EventCorrelation>("VectorEventCorrelation");

    class_<EventTimeline>("EventTimeline")
        .property("events", &EventTimeline::events)
        .property("correlations", &EventTimeline::correlations);

    class_<EventInsight>("EventInsight")
        .property("title", &EventInsight::title)
        .property("description", &EventInsight::description)
        .property("affectedEventIds", &EventInsight::affectedEventIds)
        .property("severity", &EventInsight::severity)
        .property("suggestions", &EventInsight::suggestions);

    register_vector<EventInsight>("VectorEventInsight");

    class_<EventCorrelationEngine>("EventCorrelationEngine")
        .constructor()
        .function("addEvent", select_overload<std::string(
            EventCategory,
            long long,
            const std::string&,
            const std::string&,
            const std::string&,
            int
        )>(&EventCorrelationEngine::addEvent))
        .function("findCorrelated", &EventCorrelationEngine::findCorrelated)
        .function("autoCorrelate", &EventCorrelationEngine::autoCorrelate)
        .function("getEventsByCategory", &EventCorrelationEngine::getEventsByCategory)
        .function("getEventsInRange", &EventCorrelationEngine::getEventsInRange)
        .function("getErrorChains", &EventCorrelationEngine::getErrorChains)
        .function("generateInsights", &EventCorrelationEngine::generateInsights)
        .function("getAllEvents", &EventCorrelationEngine::getAllEvents)
        .function("getAllCorrelations", &EventCorrelationEngine::getAllCorrelations)
        .function("getTimeline", &EventCorrelationEngine::getTimeline)
        .function("getStats", &EventCorrelationEngine::getStats)
        .function("clear", &EventCorrelationEngine::clear);

    enum_<EventCategory>("EventCategory")
        .value("CONSOLE", EventCategory::CONSOLE)
        .value("NETWORK", EventCategory::NETWORK)
        .value("ERROR", EventCategory::ERROR)
        .value("PERFORMANCE", EventCategory::PERFORMANCE)
        .value("USER_INTERACTION", EventCategory::USER_INTERACTION)
        .value("STORAGE", EventCategory::STORAGE)
        .value("MEMORY", EventCategory::MEMORY);
}
#endif
