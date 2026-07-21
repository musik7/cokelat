#!/bin/bash
set -e

echo "Compiling WASM modules..."

mkdir -p dist

# Core Engine
emcc src/devtools_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=67108864 -s MAXIMUM_MEMORY=1073741824 -s MODULARIZE=1 -s EXPORT_NAME="'createDevToolsEngine'" --bind -msimd128 -pthread -fno-exceptions -fno-rtti -ffast-math -o dist/devtools_engine_wasm.js

# Storage Inspector
emcc src/storage_inspector.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createStorageInspector'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/storage_inspector_wasm.js

# Storage Inspector IndexedDB
emcc src/storage_inspector_indexeddb.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createIndexedDBEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/storage_inspector_indexeddb_wasm.js

# Network Inspector
emcc src/network_inspector.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createNetworkInspector'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/network_inspector_wasm.js

# DOM Mutation Engine
emcc src/dom_mutation_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createDomEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/dom_mutation_engine_wasm.js

# CPU Profiler
emcc src/cpu_profiler.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createCpuProfiler'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/cpu_profiler_wasm.js

# Debugger Engine
emcc src/debugger_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createDebuggerEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/debugger_engine_wasm.js

# Console Engine
emcc src/console_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createConsoleEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/console_engine_wasm.js

# Layout Engine
emcc src/layout_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createLayoutEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/layout_engine_wasm.js



# Accessibility Audit
emcc src/accessibility_audit.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createAccessibilityAudit'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/accessibility_audit_wasm.js

# CSS Engine
emcc src/css_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createCssEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/css_engine_wasm.js

# CSS Engine Advanced
emcc src/css_engine_advanced.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createCssEngineAdvanced'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/css_engine_advanced_wasm.js

# Event Correlation
emcc src/event_correlation.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createEventCorrelation'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/event_correlation_wasm.js

# Heap Profiler
emcc src/heap_profiler_fixed.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createHeapProfiler'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/heap_profiler_wasm.js

# Lexer Enhanced
emcc src/lexer_enhanced.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createLexerEnhanced'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/lexer_enhanced_wasm.js

# Performance Timeline
emcc src/performance_timeline.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createPerformanceTimeline'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/performance_timeline_wasm.js

# Service Worker Monitor
emcc src/service_worker_monitor.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createServiceWorkerMonitor'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/service_worker_monitor_wasm.js

# Stack Trace Engine
emcc src/stack_trace_engine.cpp -O3 -flto -s ENVIRONMENT=web,worker -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME="'createStackTraceEngine'" --bind -msimd128 -fno-exceptions -fno-rtti -ffast-math -o dist/stack_trace_engine_wasm.js
echo "Compilation successful! Outputs in dist/"
