/**
 * @file network_inspector.cpp
 * @brief Enhanced Network Inspector for Mobile DevTools (WASM)
 * 
 * Full-featured network monitoring with headers, body inspection,
 * timing analysis, and request grouping.
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
// 1. NETWORK REQUEST/RESPONSE STRUCTURES
// ==========================================

struct NetworkHeader {
    std::string name;
    std::string value;
};

struct NetworkTiming {
    long long blocked;      // Time spent in queue before connection
    long long dns;          // DNS lookup time
    long long connect;      // TCP connection time
    long long send;         // Time to send request
    long long wait;         // Time waiting for response (TTFB)
    long long receive;      // Time to download response
    long long total;        // Total request duration
};

struct WebSocketFrame {
    std::string type; // "send" or "receive"
    long long timestamp;
    std::string payload;
    size_t length;
    int opcode; // 1 = text, 2 = binary, 8 = close, 9 = ping, 10 = pong
};

struct NetworkRequest {
    std::string id;
    std::string method;
    std::string url;
    std::string domain;
    std::string resourceType; // "xhr", "fetch", "document", "stylesheet", "image", "websocket", etc.
    long long timestamp;
    int status;
    std::string statusText;
    std::vector<NetworkHeader> requestHeaders;
    std::vector<NetworkHeader> responseHeaders;
    std::string requestBody;
    std::string responseBody;
    std::vector<WebSocketFrame> websocketFrames;
    size_t responseSize;
    NetworkTiming timing;
    bool isComplete;
    bool isError;

    std::string getResourceType() const { return resourceType; }
    bool isCached() const { return status == 304 || status == 0; }
};

// ==========================================
// 2. NETWORK INSPECTOR ENGINE
// ==========================================

struct ThrottlingProfile {
    std::string name; // "offline", "slow-3g", "fast-3g", "no-throttling"
    long long downloadThroughput; // bytes per sec
    long long uploadThroughput; // bytes per sec
    long long latency; // ms
};

class NetworkInspector {
private:
    std::vector<NetworkRequest> requests;
    static const size_t MAX_REQUESTS = 500;
    size_t requestIdCounter = 0;
    std::map<std::string, std::vector<NetworkRequest>> requestsByDomain;
    std::map<int, size_t> statusCodeCounts;
    ThrottlingProfile activeThrottlingProfile;

    std::string generateId() {
        return "req_" + std::to_string(requestIdCounter++);
    }

    std::string extractDomain(const std::string& url) {
        // Extract domain from URL
        size_t start = url.find("://");
        if (start == std::string::npos) start = 0;
        else start += 3;

        size_t end = url.find('/', start);
        if (end == std::string::npos) end = url.length();

        return url.substr(start, end - start);
    }

    std::string inferResourceType(const std::string& url, const std::vector<NetworkHeader>& headers) {
        // Try to infer from Content-Type header
        for (const auto& h : headers) {
            if (h.name == "Content-Type" || h.name == "content-type") {
                if (h.value.find("image") != std::string::npos) return "image";
                if (h.value.find("stylesheet") != std::string::npos || h.value.find("css") != std::string::npos) return "stylesheet";
                if (h.value.find("javascript") != std::string::npos) return "script";
                if (h.value.find("json") != std::string::npos) return "xhr";
            }
        }

        // Fallback to URL pattern
        if (url.find(".css") != std::string::npos) return "stylesheet";
        if (url.find(".js") != std::string::npos) return "script";
        if (url.find(".json") != std::string::npos) return "xhr";
        if (url.find(".png") != std::string::npos || url.find(".jpg") != std::string::npos || url.find(".gif") != std::string::npos) return "image";

        return "other";
    }

public:
    NetworkInspector() {
        requests.reserve(MAX_REQUESTS);
        activeThrottlingProfile = {"no-throttling", -1, -1, 0};
    }

    void setThrottlingProfile(const std::string& name, long long dl, long long ul, long long latency) {
        activeThrottlingProfile = {name, dl, ul, latency};
    }

    ThrottlingProfile getThrottlingProfile() const {
        return activeThrottlingProfile;
    }

    // Record new network request
    std::string addRequest(
        const std::string& method,
        const std::string& url,
        const std::vector<NetworkHeader>& requestHeaders = {}
    ) {
        if (requests.size() >= MAX_REQUESTS) {
            requests.erase(requests.begin()); // FIFO
        }

        std::string id = generateId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::string domain = extractDomain(url);
        std::string resourceType = inferResourceType(url, requestHeaders);

        requests.push_back({
            id,
            method,
            url,
            domain,
            resourceType,
            ms,
            0,
            "",
            requestHeaders,
            {},
            "",
            "",
            0,
            {0, 0, 0, 0, 0, 0, 0},
            false,
            false
        });

        return id;
    }

    // Update request with response data
    bool updateRequest(
        const std::string& id,
        int status,
        const std::string& statusText,
        const std::vector<NetworkHeader>& responseHeaders,
        const std::string& responseBody,
        const NetworkTiming& timing
    ) {
        for (auto& req : requests) {
            if (req.id == id) {
                req.status = status;
                req.statusText = statusText;
                req.responseHeaders = responseHeaders;
                req.responseBody = responseBody;
                req.responseSize = responseBody.length();
                req.timing = timing;
                req.isComplete = true;
                req.isError = (status >= 400);
                statusCodeCounts[status]++;
                return true;
            }
        }
        return false;
    }

    // Set request body
    bool setRequestBody(const std::string& id, const std::string& body) {
        for (auto& req : requests) {
            if (req.id == id) {
                req.requestBody = body;
                return true;
            }
        }
        return false;
    }

    bool addWebSocketFrame(const std::string& id, const std::string& type, const std::string& payload, int opcode) {
        for (auto& req : requests) {
            if (req.id == id && req.resourceType == "websocket") {
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                req.websocketFrames.push_back({type, ms, payload, payload.length(), opcode});
                return true;
            }
        }
        return false;
    }

    // Get all requests
    const std::vector<NetworkRequest>& getRequests() const {
        return requests;
    }

    // Filter by resource type
    std::vector<NetworkRequest> getRequestsByType(const std::string& type) const {
        std::vector<NetworkRequest> filtered;
        for (const auto& req : requests) {
            if (req.resourceType == type) {
                filtered.push_back(req);
            }
        }
        return filtered;
    }

    // Filter by domain
    std::vector<NetworkRequest> getRequestsByDomain(const std::string& domain) const {
        std::vector<NetworkRequest> filtered;
        for (const auto& req : requests) {
            if (req.domain == domain || req.domain.find(domain) != std::string::npos) {
                filtered.push_back(req);
            }
        }
        return filtered;
    }

    // Filter by status code range (e.g., 4xx errors)
    std::vector<NetworkRequest> getRequestsByStatusRange(int minStatus, int maxStatus) const {
        std::vector<NetworkRequest> filtered;
        for (const auto& req : requests) {
            if (req.status >= minStatus && req.status <= maxStatus) {
                filtered.push_back(req);
            }
        }
        return filtered;
    }

    // Search in requests
    std::vector<NetworkRequest> searchRequests(const std::string& query) const {
        std::vector<NetworkRequest> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (const auto& req : requests) {
            std::string lowerUrl = req.url;
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);

            if (lowerUrl.find(lowerQuery) != std::string::npos ||
                req.method.find(lowerQuery) != std::string::npos ||
                req.domain.find(lowerQuery) != std::string::npos) {
                results.push_back(req);
            }
        }
        return results;
    }

    // Get statistics
    std::map<int, size_t> getStatusCodeCounts() const {
        return statusCodeCounts;
    }

    size_t getTotalRequests() const { return requests.size(); }
    size_t getErrorCount() const {
        size_t count = 0;
        for (const auto& req : requests) {
            if (req.isError) count++;
        }
        return count;
    }

    size_t getCachedCount() const {
        size_t count = 0;
        for (const auto& req : requests) {
            if (req.isCached()) count++;
        }
        return count;
    }

    size_t getTotalBytes() const {
        size_t total = 0;
        for (const auto& req : requests) {
            total += req.responseSize;
        }
        return total;
    }

    long long getTotalDuration() const {
        long long total = 0;
        for (const auto& req : requests) {
            if (req.isComplete) total += req.timing.total;
        }
        return total;
    }

    void clear() {
        requests.clear();
        requestIdCounter = 0;
        requestsByDomain.clear();
        statusCodeCounts.clear();
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(network_inspector_module) {
    class_<NetworkHeader>("NetworkHeader")
        .property("name", &NetworkHeader::name)
        .property("value", &NetworkHeader::value);

    register_vector<NetworkHeader>("VectorNetworkHeader");

    class_<NetworkTiming>("NetworkTiming")
        .property("blocked", &NetworkTiming::blocked)
        .property("dns", &NetworkTiming::dns)
        .property("connect", &NetworkTiming::connect)
        .property("send", &NetworkTiming::send)
        .property("wait", &NetworkTiming::wait)
        .property("receive", &NetworkTiming::receive)
        .property("total", &NetworkTiming::total);

    class_<WebSocketFrame>("WebSocketFrame")
        .property("type", &WebSocketFrame::type)
        .property("timestamp", &WebSocketFrame::timestamp)
        .property("payload", &WebSocketFrame::payload)
        .property("length", &WebSocketFrame::length)
        .property("opcode", &WebSocketFrame::opcode);

    register_vector<WebSocketFrame>("VectorWebSocketFrame");

    class_<ThrottlingProfile>("ThrottlingProfile")
        .property("name", &ThrottlingProfile::name)
        .property("downloadThroughput", &ThrottlingProfile::downloadThroughput)
        .property("uploadThroughput", &ThrottlingProfile::uploadThroughput)
        .property("latency", &ThrottlingProfile::latency);

    class_<NetworkRequest>("NetworkRequest")
        .property("id", &NetworkRequest::id)
        .property("method", &NetworkRequest::method)
        .property("url", &NetworkRequest::url)
        .property("domain", &NetworkRequest::domain)
        .property("resourceType", &NetworkRequest::getResourceType)
        .property("timestamp", &NetworkRequest::timestamp)
        .property("status", &NetworkRequest::status)
        .property("statusText", &NetworkRequest::statusText)
        .property("requestHeaders", &NetworkRequest::requestHeaders)
        .property("responseHeaders", &NetworkRequest::responseHeaders)
        .property("requestBody", &NetworkRequest::requestBody)
        .property("responseBody", &NetworkRequest::responseBody)
        .property("websocketFrames", &NetworkRequest::websocketFrames)
        .property("responseSize", &NetworkRequest::responseSize)
        .property("timing", &NetworkRequest::timing)
        .property("isComplete", &NetworkRequest::isComplete)
        .property("isError", &NetworkRequest::isError)
        .property("isCached", &NetworkRequest::isCached);

    register_vector<NetworkRequest>("VectorNetworkRequest");
    register_map<int, size_t>("MapIntSize");

    class_<NetworkInspector>("NetworkInspector")
        .constructor()
        .function("setThrottlingProfile", &NetworkInspector::setThrottlingProfile)
        .function("getThrottlingProfile", &NetworkInspector::getThrottlingProfile)
        .function("addRequest", select_overload<std::string(
            const std::string&,
            const std::string&,
            const std::vector<NetworkHeader>&
        )>(&NetworkInspector::addRequest))
        .function("updateRequest", select_overload<bool(
            const std::string&,
            int,
            const std::string&,
            const std::vector<NetworkHeader>&,
            const std::string&,
            const NetworkTiming&
        )>(&NetworkInspector::updateRequest))
        .function("setRequestBody", &NetworkInspector::setRequestBody)
        .function("addWebSocketFrame", &NetworkInspector::addWebSocketFrame)
        .function("getRequests", &NetworkInspector::getRequests)
        .function("getRequestsByType", &NetworkInspector::getRequestsByType)
        .function("getRequestsByDomain", &NetworkInspector::getRequestsByDomain)
        .function("getRequestsByStatusRange", &NetworkInspector::getRequestsByStatusRange)
        .function("searchRequests", &NetworkInspector::searchRequests)
        .function("getStatusCodeCounts", &NetworkInspector::getStatusCodeCounts)
        .function("getTotalRequests", &NetworkInspector::getTotalRequests)
        .function("getErrorCount", &NetworkInspector::getErrorCount)
        .function("getCachedCount", &NetworkInspector::getCachedCount)
        .function("getTotalBytes", &NetworkInspector::getTotalBytes)
        .function("getTotalDuration", &NetworkInspector::getTotalDuration)
        .function("clear", &NetworkInspector::clear);
}
#endif

struct ThrottlingProfile {
    std::string name; // "offline", "slow-3g", "fast-3g"
    long long downloadThroughput; // bytes per sec
    long long uploadThroughput; // bytes per sec
    long long latency; // ms
};

