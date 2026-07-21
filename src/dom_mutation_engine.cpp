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
// DOM MUTATION & BOX MODEL
// ==========================================

struct BoxModelMetric {
    double width;
    double height;
    double padding[4]; // top, right, bottom, left
    double margin[4];
    double border[4];
    double contentWidth;
    double contentHeight;
    std::string boxSizing; // "content-box" or "border-box"
};

enum class MutationType {
    CHILD_LIST,
    ATTRIBUTES,
    CHARACTER_DATA
};

struct DOMMutationRecord {
    std::string id;
    MutationType type;
    std::string targetNodeId;
    std::vector<std::string> addedNodeIds;
    std::vector<std::string> removedNodeIds;
    std::string attributeName;
    std::string oldValue;
    std::string newValue;
    long long timestamp;
};

class DOMEngine {
private:
    std::vector<DOMMutationRecord> mutations;
    std::map<std::string, BoxModelMetric> boxModels;
    size_t mutationIdCounter = 0;

    std::string generateId() {
        return "mut_" + std::to_string(mutationIdCounter++);
    }

public:
    DOMEngine() {}

    std::string recordMutation(
        MutationType type,
        const std::string& targetNodeId,
        const std::vector<std::string>& addedNodeIds,
        const std::vector<std::string>& removedNodeIds,
        const std::string& attributeName,
        const std::string& oldValue,
        const std::string& newValue
    ) {
        std::string id = generateId();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        mutations.push_back({id, type, targetNodeId, addedNodeIds, removedNodeIds, attributeName, oldValue, newValue, ms});
        return id;
    }

    void updateBoxModel(const std::string& nodeId, const BoxModelMetric& metrics) {
        boxModels[nodeId] = metrics;
    }

    BoxModelMetric getBoxModel(const std::string& nodeId) const {
        auto it = boxModels.find(nodeId);
        if (it != boxModels.end()) {
            return it->second;
        }
        return {0, 0, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, 0, 0, "content-box"};
    }

    const std::vector<DOMMutationRecord>& getMutations() const {
        return mutations;
    }

    void clear() {
        mutations.clear();
        boxModels.clear();
    }
};

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(dom_engine_module) {
    enum_<MutationType>("MutationType")
        .value("CHILD_LIST", MutationType::CHILD_LIST)
        .value("ATTRIBUTES", MutationType::ATTRIBUTES)
        .value("CHARACTER_DATA", MutationType::CHARACTER_DATA);

    value_array<double, 4>("ArrayDouble4")
        .element(emscripten::index<0>())
        .element(emscripten::index<1>())
        .element(emscripten::index<2>())
        .element(emscripten::index<3>());

    class_<BoxModelMetric>("BoxModelMetric")
        .property("width", &BoxModelMetric::width)
        .property("height", &BoxModelMetric::height)
        .property("contentWidth", &BoxModelMetric::contentWidth)
        .property("contentHeight", &BoxModelMetric::contentHeight)
        .property("boxSizing", &BoxModelMetric::boxSizing);
        // padding, margin, border are slightly tricky with Embind, simplified for now
        // A more advanced wrapper could expose them

    class_<DOMMutationRecord>("DOMMutationRecord")
        .property("id", &DOMMutationRecord::id)
        .property("type", &DOMMutationRecord::type)
        .property("targetNodeId", &DOMMutationRecord::targetNodeId)
        .property("addedNodeIds", &DOMMutationRecord::addedNodeIds)
        .property("removedNodeIds", &DOMMutationRecord::removedNodeIds)
        .property("attributeName", &DOMMutationRecord::attributeName)
        .property("oldValue", &DOMMutationRecord::oldValue)
        .property("newValue", &DOMMutationRecord::newValue)
        .property("timestamp", &DOMMutationRecord::timestamp);

    register_vector<DOMMutationRecord>("VectorDOMMutationRecord");
    register_vector<std::string>("VectorStringDOMMutation"); // Helper

    class_<DOMEngine>("DOMEngine")
        .constructor()
        .function("recordMutation", &DOMEngine::recordMutation)
        .function("updateBoxModel", &DOMEngine::updateBoxModel)
        .function("getBoxModel", &DOMEngine::getBoxModel)
        .function("getMutations", &DOMEngine::getMutations)
        .function("clear", &DOMEngine::clear);
}
#endif
