#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <queue>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// VIRTUAL LAYOUT ENGINE (WASM DOM)
// ==========================================

enum class OperationType {
    CREATE,
    APPEND,
    INSERT_BEFORE,
    REMOVE,
    UPDATE_ATTR,
    UPDATE_TEXT,
    REPLACE,
    ADD_EVENT_LISTENER,
    REMOVE_EVENT_LISTENER,
    EMIT_CUSTOM_EVENT
};

struct LayoutOperation {
    OperationType type;
    std::string nodeId;
    std::string targetId; // parentId or referenceId
    std::string data1; // attr name or text content or event type
    std::string data2; // attr value or payload
};

struct LayoutAttribute {
    std::string name;
    std::string value;
};

struct LayoutNode {
    std::string id;
    std::string tagName;
    std::string textContent;
    std::vector<LayoutAttribute> attributes;
    std::vector<std::string> childIds;
    std::string parentId;
    std::vector<std::string> callbacks; // Event bindings (e.g. "click", "hover")
    
    // Virtual List / Lazy Loading Properties
    bool isVirtualList;
    int totalItems;
    double itemSize;
    int startIndex;
    int endIndex;
    std::string templateId; // Puzzle piece reference
};

class LayoutEngine {
private:
    std::map<std::string, LayoutNode> nodes;
    std::vector<LayoutOperation> renderQueue; // Batch operation queue
    
    // Object Pooling for DOM Recycling
    std::map<std::string, std::queue<std::string>> templatePools; 
    
    size_t nodeCounter = 0;
    
    // Global State
    std::string activeNodeId;
    std::string scrollingNodeId;

    std::string generateId() {
        return "frag_" + std::to_string(nodeCounter++);
    }

    void removeNodeRecursive(const std::string& id) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return;
        
        // Remove children recursively
        std::vector<std::string> childrenToCopy = it->second.childIds;
        for (const auto& childId : childrenToCopy) {
            removeNodeRecursive(childId);
        }
        
        nodes.erase(it);
        renderQueue.push_back({OperationType::REMOVE, id, "", "", ""});
    }

    std::string applyDataBinding(std::string text, const std::map<std::string, std::string>& data) {
        for (const auto& kv : data) {
            std::string placeholder = "{{" + kv.first + "}}";
            size_t pos = 0;
            while ((pos = text.find(placeholder, pos)) != std::string::npos) {
                text.replace(pos, placeholder.length(), kv.second);
                pos += kv.second.length();
            }
        }
        return text;
    }

    std::string cloneNodeInternal(const std::string& id, bool deep, const std::map<std::string, std::string>& dataBindings) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return "";

        std::string cloneId = generateId();
        LayoutNode clone = it->second;
        clone.id = cloneId;
        clone.parentId = "";
        
        // Apply data bindings to textContent and attributes
        clone.textContent = applyDataBinding(clone.textContent, dataBindings);
        for (auto& attr : clone.attributes) {
            attr.value = applyDataBinding(attr.value, dataBindings);
        }
        
        if (!deep) {
            clone.childIds.clear();
        } else {
            std::vector<std::string> newChildren;
            for (const auto& childId : clone.childIds) {
                std::string clonedChildId = cloneNodeInternal(childId, true, dataBindings);
                if (!clonedChildId.empty()) {
                    newChildren.push_back(clonedChildId);
                    nodes[clonedChildId].parentId = cloneId;
                }
            }
            clone.childIds = newChildren;
        }
        
        nodes[cloneId] = clone;
        renderQueue.push_back({OperationType::CREATE, cloneId, clone.tagName, "cloned", ""});
        
        // Also queue updates for bound attributes/text if any changed
        if (!clone.textContent.empty()) {
             renderQueue.push_back({OperationType::UPDATE_TEXT, cloneId, "", clone.textContent, ""});
        }
        for (const auto& attr : clone.attributes) {
             renderQueue.push_back({OperationType::UPDATE_ATTR, cloneId, "", attr.name, attr.value});
        }
        // Restore Event Listeners for cloned nodes
        for (const auto& cb : clone.callbacks) {
             renderQueue.push_back({OperationType::ADD_EVENT_LISTENER, cloneId, "", cb, ""});
        }
        
        return cloneId;
    }

public:
    LayoutEngine() {}

    // 1. Puzzle Piece (Fragment) Creation - creates detached node
    std::string createFragment(const std::string& tagName, const std::string& id = "") {
        std::string actualId = id.empty() ? generateId() : id;
        nodes[actualId] = {actualId, tagName, "", {}, {}, "", {}, false, 0, 0.0, 0, 0, ""};
        
        renderQueue.push_back({OperationType::CREATE, actualId, tagName, "", ""});
        return actualId;
    }

    // 2. DOM Manipulations
    bool appendChild(const std::string& parentId, const std::string& childId) {
        if (nodes.find(parentId) == nodes.end() || nodes.find(childId) == nodes.end()) {
            return false;
        }
        
        std::string oldParent = nodes[childId].parentId;
        if (!oldParent.empty() && nodes.find(oldParent) != nodes.end()) {
            auto& siblings = nodes[oldParent].childIds;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), childId), siblings.end());
        }

        nodes[parentId].childIds.push_back(childId);
        nodes[childId].parentId = parentId;
        
        renderQueue.push_back({OperationType::APPEND, childId, parentId, "", ""});
        return true;
    }
    
    bool insertBefore(const std::string& parentId, const std::string& childId, const std::string& referenceId) {
        if (nodes.find(parentId) == nodes.end() || nodes.find(childId) == nodes.end()) return false;
        
        std::string oldParent = nodes[childId].parentId;
        if (!oldParent.empty() && nodes.find(oldParent) != nodes.end()) {
            auto& siblings = nodes[oldParent].childIds;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), childId), siblings.end());
        }
        
        auto& children = nodes[parentId].childIds;
        auto it = std::find(children.begin(), children.end(), referenceId);
        if (it != children.end()) {
            children.insert(it, childId);
        } else {
            children.push_back(childId);
        }
        nodes[childId].parentId = parentId;
        
        renderQueue.push_back({OperationType::INSERT_BEFORE, childId, parentId, referenceId, ""});
        return true;
    }

    // Deep Memory Leak Prevention using Recursive Deletion
    bool removeNode(const std::string& id) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return false;

        std::string parentId = it->second.parentId;
        if (!parentId.empty() && nodes.find(parentId) != nodes.end()) {
            auto& siblings = nodes[parentId].childIds;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        }

        // Deep delete
        removeNodeRecursive(id);
        
        if (activeNodeId == id) activeNodeId = "";
        if (scrollingNodeId == id) scrollingNodeId = "";
        
        return true;
    }

    std::string cloneNode(const std::string& id, bool deep) {
        std::map<std::string, std::string> emptyBindings;
        return cloneNodeInternal(id, deep, emptyBindings);
    }

    // 3. Properties and Attributes
    bool setAttribute(const std::string& id, const std::string& name, const std::string& value) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return false;

        for (auto& attr : it->second.attributes) {
            if (attr.name == name) {
                attr.value = value;
                renderQueue.push_back({OperationType::UPDATE_ATTR, id, "", name, value});
                return true;
            }
        }
        it->second.attributes.push_back({name, value});
        renderQueue.push_back({OperationType::UPDATE_ATTR, id, "", name, value});
        return true;
    }
    
    bool setTextContent(const std::string& id, const std::string& text) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return false;
        it->second.textContent = text;
        renderQueue.push_back({OperationType::UPDATE_TEXT, id, "", text, ""});
        return true;
    }

    // 4. Puzzle Assembly with Data Binding
    std::string instantiatePuzzleBound(const std::string& templateNodeId, const std::string& targetParentId, const std::map<std::string, std::string>& dataBindings) {
        // DOM Recycling - Check pool first
        std::string instanceId;
        
        if (!templatePools[templateNodeId].empty()) {
            instanceId = templatePools[templateNodeId].front();
            templatePools[templateNodeId].pop();
        } else {
            instanceId = cloneNodeInternal(templateNodeId, true, dataBindings);
        }

        if (!instanceId.empty() && !targetParentId.empty()) {
            appendChild(targetParentId, instanceId);
        }
        return instanceId; // Returns the root of the assembled puzzle piece
    }

    std::string instantiatePuzzle(const std::string& templateNodeId, const std::string& targetParentId) {
        std::map<std::string, std::string> emptyBindings;
        return instantiatePuzzleBound(templateNodeId, targetParentId, emptyBindings);
    }

    // DOM Recycling - Recycle Puzzle Piece
    bool recyclePuzzle(const std::string& instanceId, const std::string& templateNodeId) {
        auto it = nodes.find(instanceId);
        if (it == nodes.end()) return false;
        
        // Detach from parent
        std::string parentId = it->second.parentId;
        if (!parentId.empty() && nodes.find(parentId) != nodes.end()) {
            auto& siblings = nodes[parentId].childIds;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), instanceId), siblings.end());
            it->second.parentId = "";
        }
        
        // Instruct JS to detach but NOT destroy
        renderQueue.push_back({OperationType::REMOVE, instanceId, parentId, "recycle", ""});
        
        templatePools[templateNodeId].push(instanceId);
        return true;
    }

    // 5. Virtual List & Lazy Loading Mechanics
    bool setupVirtualList(const std::string& listNodeId, const std::string& templateId, int totalItems, double itemSize) {
        auto it = nodes.find(listNodeId);
        if (it == nodes.end()) return false;

        it->second.isVirtualList = true;
        it->second.templateId = templateId;
        it->second.totalItems = totalItems;
        it->second.itemSize = itemSize;
        it->second.startIndex = 0;
        it->second.endIndex = 0;
        
        return true;
    }

    bool updateVirtualListScroll(const std::string& listNodeId, double scrollTop, double viewportHeight) {
        auto it = nodes.find(listNodeId);
        if (it == nodes.end() || !it->second.isVirtualList) return false;

        int newStart = std::max(0, static_cast<int>(std::floor(scrollTop / it->second.itemSize)));
        int visibleCount = std::ceil(viewportHeight / it->second.itemSize) + 1; // 1 extra for partial visibility
        int newEnd = std::min(it->second.totalItems, newStart + visibleCount);

        if (newStart != it->second.startIndex || newEnd != it->second.endIndex) {
            it->second.startIndex = newStart;
            it->second.endIndex = newEnd;
            
            // Queue special event for JS to process the visible window delta
            renderQueue.push_back({OperationType::REPLACE, listNodeId, "VIRTUAL_LIST_UPDATE", std::to_string(newStart), std::to_string(newEnd)});
            return true;
        }

        return false;
    }
    
    // 6. EVENT EMITTER & EVENT DELEGATION
    bool addEventListener(const std::string& id, const std::string& eventType) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return false;
        
        if (std::find(it->second.callbacks.begin(), it->second.callbacks.end(), eventType) == it->second.callbacks.end()) {
            it->second.callbacks.push_back(eventType);
            // Instruksikan JS untuk pasang EventListener asli di elemen ini
            renderQueue.push_back({OperationType::ADD_EVENT_LISTENER, id, "", eventType, ""});
        }
        return true;
    }

    bool removeEventListener(const std::string& id, const std::string& eventType) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return false;
        
        auto& callbacks = it->second.callbacks;
        auto cbIt = std::find(callbacks.begin(), callbacks.end(), eventType);
        if (cbIt != callbacks.end()) {
            callbacks.erase(cbIt);
            // Instruksikan JS untuk hapus EventListener
            renderQueue.push_back({OperationType::REMOVE_EVENT_LISTENER, id, "", eventType, ""});
        }
        return true;
    }
    
    // JS memanggil ini ketika Event asli terjadi di browser
    void triggerEvent(const std::string& id, const std::string& eventType, const std::string& payload) {
        auto it = nodes.find(id);
        if (it == nodes.end()) return;
        
        if (eventType == "click" || eventType == "focus") {
            activeNodeId = id;
            // Emit internal custom event to JS listener
            renderQueue.push_back({OperationType::EMIT_CUSTOM_EVENT, id, "state_change", "active", payload});
        }
        else if (eventType == "scroll") {
            scrollingNodeId = id;
            // payload bisa berupa JSON dengan scrollTop dan viewportHeight
            // Di implementasi aslinya kita parse JSON payload, untuk simplisitas di sini pass saja via event
            renderQueue.push_back({OperationType::EMIT_CUSTOM_EVENT, id, "on_scroll", payload, ""});
        }
        else if (eventType == "blur") {
            if (activeNodeId == id) activeNodeId = "";
            renderQueue.push_back({OperationType::EMIT_CUSTOM_EVENT, id, "state_change", "blur", payload});
        }
    }

    std::string getActiveNodeId() const { return activeNodeId; }
    std::string getScrollingNodeId() const { return scrollingNodeId; }

    // 7. Queue Processing
    std::vector<LayoutOperation> flushQueue() {
        std::vector<LayoutOperation> currentQueue = renderQueue;
        renderQueue.clear();
        return currentQueue;
    }

    LayoutNode getNode(const std::string& id) const {
        auto it = nodes.find(id);
        if (it != nodes.end()) {
            return it->second;
        }
        return {"", "", "", {}, {}, "", {}, false, 0, 0.0, 0, 0, ""};
    }

    void clear() {
        nodes.clear();
        renderQueue.clear();
        templatePools.clear();
        nodeCounter = 0;
        activeNodeId = "";
        scrollingNodeId = "";
    }
};

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(layout_engine_module) {
    enum_<OperationType>("OperationType")
        .value("CREATE", OperationType::CREATE)
        .value("APPEND", OperationType::APPEND)
        .value("INSERT_BEFORE", OperationType::INSERT_BEFORE)
        .value("REMOVE", OperationType::REMOVE)
        .value("UPDATE_ATTR", OperationType::UPDATE_ATTR)
        .value("UPDATE_TEXT", OperationType::UPDATE_TEXT)
        .value("REPLACE", OperationType::REPLACE)
        .value("ADD_EVENT_LISTENER", OperationType::ADD_EVENT_LISTENER)
        .value("REMOVE_EVENT_LISTENER", OperationType::REMOVE_EVENT_LISTENER)
        .value("EMIT_CUSTOM_EVENT", OperationType::EMIT_CUSTOM_EVENT);

    class_<LayoutOperation>("LayoutOperation")
        .property("type", &LayoutOperation::type)
        .property("nodeId", &LayoutOperation::nodeId)
        .property("targetId", &LayoutOperation::targetId)
        .property("data1", &LayoutOperation::data1)
        .property("data2", &LayoutOperation::data2);

    register_vector<LayoutOperation>("VectorLayoutOperation");

    class_<LayoutAttribute>("LayoutAttribute")
        .property("name", &LayoutAttribute::name)
        .property("value", &LayoutAttribute::value);

    register_vector<LayoutAttribute>("VectorLayoutAttribute");
    register_vector<std::string>("VectorStringID");
    
    // Register Map for Data Binding
    register_map<std::string, std::string>("MapStringString");

    class_<LayoutNode>("LayoutNode")
        .property("id", &LayoutNode::id)
        .property("tagName", &LayoutNode::tagName)
        .property("textContent", &LayoutNode::textContent)
        .property("attributes", &LayoutNode::attributes)
        .property("childIds", &LayoutNode::childIds)
        .property("parentId", &LayoutNode::parentId)
        .property("callbacks", &LayoutNode::callbacks)
        .property("isVirtualList", &LayoutNode::isVirtualList)
        .property("totalItems", &LayoutNode::totalItems)
        .property("itemSize", &LayoutNode::itemSize)
        .property("startIndex", &LayoutNode::startIndex)
        .property("endIndex", &LayoutNode::endIndex)
        .property("templateId", &LayoutNode::templateId);

    register_vector<LayoutNode>("VectorLayoutNode");

    class_<LayoutEngine>("LayoutEngine")
        .constructor()
        .function("createFragment", &LayoutEngine::createFragment)
        .function("appendChild", &LayoutEngine::appendChild)
        .function("insertBefore", &LayoutEngine::insertBefore)
        .function("removeNode", &LayoutEngine::removeNode)
        .function("cloneNode", &LayoutEngine::cloneNode)
        .function("setAttribute", &LayoutEngine::setAttribute)
        .function("setTextContent", &LayoutEngine::setTextContent)
        .function("instantiatePuzzle", &LayoutEngine::instantiatePuzzle)
        .function("instantiatePuzzleBound", &LayoutEngine::instantiatePuzzleBound)
        .function("recyclePuzzle", &LayoutEngine::recyclePuzzle)
        .function("setupVirtualList", &LayoutEngine::setupVirtualList)
        .function("updateVirtualListScroll", &LayoutEngine::updateVirtualListScroll)
        .function("addEventListener", &LayoutEngine::addEventListener)
        .function("removeEventListener", &LayoutEngine::removeEventListener)
        .function("triggerEvent", &LayoutEngine::triggerEvent)
        .function("getActiveNodeId", &LayoutEngine::getActiveNodeId)
        .function("getScrollingNodeId", &LayoutEngine::getScrollingNodeId)
        .function("flushQueue", &LayoutEngine::flushQueue)
        .function("getNode", &LayoutEngine::getNode)
        .function("clear", &LayoutEngine::clear);
}
#endif
