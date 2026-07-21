# Kompilasi WASM Lengkap (Semua Fitur Chromium DevTools)

Untuk melakukan kompilasi semua modul engine (IndexedDB, Network Simulator, CPU Profiler, dll), jalankan skrip berikut:

```bash
./build_all.sh
```

Modul yang akan dihasilkan ke direktori `dist/`:
1. `devtools_engine_wasm.js` (Core AST/Parser & Virtual FS)
2. `storage_inspector_wasm.js` (Cookie, Local/Session Storage)
3. `storage_inspector_indexeddb_wasm.js` (IndexedDB Engine)
4. `network_inspector_wasm.js` (Network, SSE, Throttling Simulator)
5. `dom_mutation_engine_wasm.js` (DOM Mutation & Box Model)
6. `cpu_profiler_wasm.js` (Flame Chart & Trace Event)
7. `debugger_engine_wasm.js` (Debugger & Scope Engine)
8. `console_engine_wasm.js` (Console Engine)
9. `layout_engine_wasm.js` (Virtual DOM / Layout Engine)
10. `accessibility_audit_wasm.js` (Accessibility Audit & ARIA Checks)
11. `css_engine_wasm.js` (CSS Rule Parsing & Specificity)
12. `css_engine_advanced_wasm.js` (Advanced CSSOM & Variables)
13. `event_correlation_wasm.js` (Event Graph & Correlation)
14. `heap_profiler_wasm.js` (Memory Leaks & Heap Snapshots)
15. `lexer_enhanced_wasm.js` (Enhanced Tokenizer for JS/CSS/HTML)
16. `performance_timeline_wasm.js` (Performance Metrics & Web Vitals)
17. `service_worker_monitor_wasm.js` (SW Registration & Caches)
18. `stack_trace_engine_wasm.js` (Async Stack Traces & Source Maps)
