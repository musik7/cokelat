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
// SCOPE & DEBUGGER ENGINE
// ==========================================

enum class ScopeType {
    GLOBAL,
    LOCAL,
    BLOCK,
    CLOSURE,
    CATCH
};

struct ScopeVariable {
    std::string name;
    std::string value;
    std::string type;
    bool isMutable;
};

struct ExecutionScope {
    std::string id;
    ScopeType type;
    std::string name;
    std::vector<ScopeVariable> variables;
};

struct CallFrame {
    std::string id;
    std::string functionName;
    std::string scriptId;
    std::string url;
    int lineNumber;
    int columnNumber;
    std::vector<ExecutionScope> scopes;
    std::string thisValue;
};

struct Breakpoint {
    std::string id;
    std::string url;
    int lineNumber;
    int columnNumber;
    std::string condition;
    bool enabled;
};

class DebuggerEngine {
private:
    std::vector<Breakpoint> breakpoints;
    std::vector<CallFrame> callStack;
    bool isPaused;
    std::string pauseReason;
    size_t idCounter = 0;

    std::string generateId(const std::string& prefix) {
        return prefix + "_" + std::to_string(idCounter++);
    }

public:
    DebuggerEngine() : isPaused(false) {}

    std::string addBreakpoint(const std::string& url, int line, int col, const std::string& condition) {
        std::string id = generateId("bp");
        breakpoints.push_back({id, url, line, col, condition, true});
        return id;
    }

    bool removeBreakpoint(const std::string& id) {
        for (auto it = breakpoints.begin(); it != breakpoints.end(); ++it) {
            if (it->id == id) {
                breakpoints.erase(it);
                return true;
            }
        }
        return false;
    }

    bool toggleBreakpoint(const std::string& id, bool enabled) {
        for (auto& bp : breakpoints) {
            if (bp.id == id) {
                bp.enabled = enabled;
                return true;
            }
        }
        return false;
    }

    const std::vector<Breakpoint>& getBreakpoints() const {
        return breakpoints;
    }

    void pauseExecution(const std::string& reason, const std::vector<CallFrame>& stack) {
        isPaused = true;
        pauseReason = reason;
        callStack = stack;
    }

    void resumeExecution() {
        isPaused = false;
        pauseReason = "";
        callStack.clear();
    }

    void stepOver() {}
    void stepInto() {}
    void stepOut() {}

    bool getIsPaused() const { return isPaused; }
    std::string getPauseReason() const { return pauseReason; }
    const std::vector<CallFrame>& getCallStack() const { return callStack; }
    
    // Evaluate in scope simulation
    std::string evaluateInScope(const std::string& frameId, const std::string& expression) {
        return "Evaluation Result: [Simulated] " + expression;
    }

    void clear() {
        breakpoints.clear();
        callStack.clear();
        isPaused = false;
    }
};

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(debugger_engine_module) {
    enum_<ScopeType>("ScopeType")
        .value("GLOBAL", ScopeType::GLOBAL)
        .value("LOCAL", ScopeType::LOCAL)
        .value("BLOCK", ScopeType::BLOCK)
        .value("CLOSURE", ScopeType::CLOSURE)
        .value("CATCH", ScopeType::CATCH);

    class_<ScopeVariable>("ScopeVariable")
        .property("name", &ScopeVariable::name)
        .property("value", &ScopeVariable::value)
        .property("type", &ScopeVariable::type)
        .property("isMutable", &ScopeVariable::isMutable);

    register_vector<ScopeVariable>("VectorScopeVariable");

    class_<ExecutionScope>("ExecutionScope")
        .property("id", &ExecutionScope::id)
        .property("type", &ExecutionScope::type)
        .property("name", &ExecutionScope::name)
        .property("variables", &ExecutionScope::variables);

    register_vector<ExecutionScope>("VectorExecutionScope");

    class_<CallFrame>("CallFrame")
        .property("id", &CallFrame::id)
        .property("functionName", &CallFrame::functionName)
        .property("scriptId", &CallFrame::scriptId)
        .property("url", &CallFrame::url)
        .property("lineNumber", &CallFrame::lineNumber)
        .property("columnNumber", &CallFrame::columnNumber)
        .property("scopes", &CallFrame::scopes)
        .property("thisValue", &CallFrame::thisValue);

    register_vector<CallFrame>("VectorCallFrame");

    class_<Breakpoint>("Breakpoint")
        .property("id", &Breakpoint::id)
        .property("url", &Breakpoint::url)
        .property("lineNumber", &Breakpoint::lineNumber)
        .property("columnNumber", &Breakpoint::columnNumber)
        .property("condition", &Breakpoint::condition)
        .property("enabled", &Breakpoint::enabled);

    register_vector<Breakpoint>("VectorBreakpoint");

    class_<DebuggerEngine>("DebuggerEngine")
        .constructor()
        .function("addBreakpoint", &DebuggerEngine::addBreakpoint)
        .function("removeBreakpoint", &DebuggerEngine::removeBreakpoint)
        .function("toggleBreakpoint", &DebuggerEngine::toggleBreakpoint)
        .function("getBreakpoints", &DebuggerEngine::getBreakpoints)
        .function("pauseExecution", &DebuggerEngine::pauseExecution)
        .function("resumeExecution", &DebuggerEngine::resumeExecution)
        .function("stepOver", &DebuggerEngine::stepOver)
        .function("stepInto", &DebuggerEngine::stepInto)
        .function("stepOut", &DebuggerEngine::stepOut)
        .function("getIsPaused", &DebuggerEngine::getIsPaused)
        .function("getPauseReason", &DebuggerEngine::getPauseReason)
        .function("getCallStack", &DebuggerEngine::getCallStack)
        .function("evaluateInScope", &DebuggerEngine::evaluateInScope)
        .function("clear", &DebuggerEngine::clear);
}
#endif
