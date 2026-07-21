/**
 * @file heap_profiler_fixed.cpp
 * @brief Fixed Heap Profiler with Correct Fragmentation Calculation
 * 
 * Proper fragmentation detection: measures external fragmentation
 * (free blocks between allocated blocks) and internal fragmentation.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
using namespace emscripten;
#endif

// ==========================================
// 1. HEAP BLOCK STRUCTURES (IMPROVED)
// ==========================================

enum class BlockState {
    ALLOCATED,
    FREE,
    DEALLOCATED
};

struct HeapBlockFixed {
    std::string address;
    size_t offset;          // Actual position in heap
    size_t requestedSize;   // Size requested by application
    size_t allocatedSize;   // Actual size (with padding/alignment)
    std::string type;       // "object", "array", "string", etc.
    BlockState state;
    long long allocationTime;
    long long deallocationTime;
    size_t allocationCount; // Track how many times reused
};

struct HeapFragmentationReport {
    double externalFragmentation;  // Free blocks between allocated blocks (%)
    double internalFragmentation;  // Wasted space due to alignment (%)
    double totalFragmentation;     // Combined fragmentation (%)
    size_t totalAllocatedBytes;
    size_t totalFreeBytes;
    size_t wastedBytes;            // Internal fragmentation waste
    size_t largestFreeBlock;       // Largest contiguous free space
    size_t smallestAllocatedBlock; // Smallest allocated block
    size_t averageBlockSize;
    int fragmentedRegionCount;     // Number of free blocks
};

// ==========================================
// 2. FIXED HEAP PROFILER
// ==========================================

class HeapProfilerFixed {
private:
    std::vector<HeapBlockFixed> blocks;
    size_t totalAllocatedBytes = 0;
    size_t totalDeallocatedBytes = 0;
    size_t heapSize = 0;
    int blockIdCounter = 0;

    std::string generateAddress() {
        return "0x" + std::to_string(blockIdCounter++);
    }

public:
    HeapProfilerFixed(size_t heapSizeBytes = 10 * 1024 * 1024) : heapSize(heapSizeBytes) {
        blocks.reserve(100000);
    }

    // Register memory allocation
    std::string allocate(
        size_t requestedSize,
        const std::string& type = "object"
    ) {
        std::string addr = generateAddress();
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        // Simulate alignment: 8-byte alignment for mobile
        size_t allocatedSize = (requestedSize + 7) & ~7;

        size_t offset = 0;
        for (const auto& b : blocks) {
            if (b.state != BlockState::FREE) {
                offset = b.offset + b.allocatedSize;
            }
        }

        blocks.push_back({
            addr,
            offset,
            requestedSize,
            allocatedSize,
            type,
            BlockState::ALLOCATED,
            ms,
            0,
            1
        });

        totalAllocatedBytes += allocatedSize;
        return addr;
    }

    // Deallocate memory
    bool deallocate(const std::string& address) {
        for (auto& block : blocks) {
            if (block.address == address && block.state == BlockState::ALLOCATED) {
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

                block.state = BlockState::FREE;
                block.deallocationTime = ms;
                totalDeallocatedBytes += block.allocatedSize;
                return true;
            }
        }
        return false;
    }

    // Reallocate (resize) memory
    std::string reallocate(const std::string& oldAddress, size_t newSize) {
        deallocate(oldAddress);
        return allocate(newSize, "reallocated");
    }

    // CORRECT fragmentation calculation
    HeapFragmentationReport analyzeFragmentation() const {
        HeapFragmentationReport report = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        if (blocks.empty()) {
            return report;
        }

        // Sort blocks by offset for contiguous analysis
        std::vector<HeapBlockFixed> sortedBlocks = blocks;
        std::sort(sortedBlocks.begin(), sortedBlocks.end(),
                  [](const HeapBlockFixed& a, const HeapBlockFixed& b) {
                      return a.offset < b.offset;
                  });

        report.totalAllocatedBytes = totalAllocatedBytes - totalDeallocatedBytes;
        report.totalFreeBytes = totalDeallocatedBytes;

        // Calculate external fragmentation: free blocks between allocated blocks
        size_t externalFragBytes = 0;
        size_t largestFree = 0;
        int fragmentCount = 0;
        size_t lastAllocatedEnd = 0;
        bool inAllocatedRegion = false;

        for (const auto& block : sortedBlocks) {
            if (block.state == BlockState::ALLOCATED) {
                // Gap between last allocated and this one = external fragmentation
                if (inAllocatedRegion && block.offset > lastAllocatedEnd) {
                    size_t gapSize = block.offset - lastAllocatedEnd;
                    externalFragBytes += gapSize;
                    largestFree = std::max(largestFree, gapSize);
                    fragmentCount++;
                }
                lastAllocatedEnd = block.offset + block.allocatedSize;
                inAllocatedRegion = true;
            }
        }

        report.largestFreeBlock = largestFree;
        report.fragmentedRegionCount = fragmentCount;

        // Calculate internal fragmentation: wasted space due to alignment/padding
        size_t internalFragBytes = 0;
        for (const auto& block : sortedBlocks) {
            if (block.state == BlockState::ALLOCATED) {
                size_t waste = block.allocatedSize - block.requestedSize;
                internalFragBytes += waste;
            }
        }

        report.wastedBytes = internalFragBytes;

        // Calculate percentages
        size_t usedBytes = report.totalAllocatedBytes;
        size_t totalUsedWithFragmentation = usedBytes + externalFragBytes + internalFragBytes;

        if (usedBytes > 0) {
            report.internalFragmentation = (double)internalFragBytes / (double)usedBytes * 100.0;
        }

        if (totalUsedWithFragmentation > 0) {
            report.externalFragmentation = (double)externalFragBytes / (double)totalUsedWithFragmentation * 100.0;
            report.totalFragmentation = 
                ((double)(externalFragBytes + internalFragBytes) / (double)totalUsedWithFragmentation) * 100.0;
        }

        // Average block size
        if (!blocks.empty()) {
            size_t totalSize = 0;
            size_t allocatedBlockCount = 0;
            size_t smallestAllocated = SIZE_MAX;

            for (const auto& block : blocks) {
                if (block.state == BlockState::ALLOCATED) {
                    totalSize += block.allocatedSize;
                    allocatedBlockCount++;
                    smallestAllocated = std::min(smallestAllocated, block.allocatedSize);
                }
            }

            if (allocatedBlockCount > 0) {
                report.averageBlockSize = totalSize / allocatedBlockCount;
                report.smallestAllocatedBlock = smallestAllocated;
            }
        }

        return report;
    }

    // Get active allocations
    std::vector<HeapBlockFixed> getActiveAllocations() const {
        std::vector<HeapBlockFixed> active;
        for (const auto& block : blocks) {
            if (block.state == BlockState::ALLOCATED) {
                active.push_back(block);
            }
        }
        return active;
    }

    // Get memory leak candidates (allocated but never freed)
    std::vector<HeapBlockFixed> getMemoryLeakCandidates() const {
        std::vector<HeapBlockFixed> leaks;
        auto now = std::chrono::high_resolution_clock::now();
        auto currentMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        const long long LEAK_THRESHOLD_MS = 60000; // 60 seconds

        for (const auto& block : blocks) {
            if (block.state == BlockState::ALLOCATED) {
                if (currentMs - block.allocationTime > LEAK_THRESHOLD_MS) {
                    leaks.push_back(block);
                }
            }
        }
        return leaks;
    }

    // Get statistics
    std::map<std::string, size_t> getStats() const {
        std::map<std::string, size_t> stats;
        stats["totalAllocations"] = 0;
        stats["totalDeallocations"] = 0;
        stats["activeBlocks"] = 0;
        stats["freeBlocks"] = 0;

        for (const auto& block : blocks) {
            if (block.state == BlockState::ALLOCATED) {
                stats["activeBlocks"]++;
                stats["totalAllocations"]++;
            } else if (block.state == BlockState::FREE) {
                stats["freeBlocks"]++;
                stats["totalDeallocations"]++;
            }
        }
        return stats;
    }

    size_t getActiveMemory() const {
        return totalAllocatedBytes - totalDeallocatedBytes;
    }

    void clear() {
        blocks.clear();
        totalAllocatedBytes = 0;
        totalDeallocatedBytes = 0;
        blockIdCounter = 0;
    }
};

// ==========================================
// 3. EMSCRIPTEN BINDINGS
// ==========================================

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(heap_profiler_fixed_module) {
    class_<HeapBlockFixed>("HeapBlockFixed")
        .property("address", &HeapBlockFixed::address)
        .property("offset", &HeapBlockFixed::offset)
        .property("requestedSize", &HeapBlockFixed::requestedSize)
        .property("allocatedSize", &HeapBlockFixed::allocatedSize)
        .property("type", &HeapBlockFixed::type);

    register_vector<HeapBlockFixed>("VectorHeapBlockFixed");

    class_<HeapFragmentationReport>("HeapFragmentationReport")
        .property("externalFragmentation", &HeapFragmentationReport::externalFragmentation)
        .property("internalFragmentation", &HeapFragmentationReport::internalFragmentation)
        .property("totalFragmentation", &HeapFragmentationReport::totalFragmentation)
        .property("totalAllocatedBytes", &HeapFragmentationReport::totalAllocatedBytes)
        .property("totalFreeBytes", &HeapFragmentationReport::totalFreeBytes)
        .property("wastedBytes", &HeapFragmentationReport::wastedBytes)
        .property("largestFreeBlock", &HeapFragmentationReport::largestFreeBlock)
        .property("smallestAllocatedBlock", &HeapFragmentationReport::smallestAllocatedBlock)
        .property("averageBlockSize", &HeapFragmentationReport::averageBlockSize)
        .property("fragmentedRegionCount", &HeapFragmentationReport::fragmentedRegionCount);

    class_<HeapProfilerFixed>("HeapProfilerFixed")
        .constructor<size_t>()
        .function("allocate", select_overload<std::string(
            size_t,
            const std::string&
        )>(&HeapProfilerFixed::allocate))
        .function("deallocate", &HeapProfilerFixed::deallocate)
        .function("reallocate", &HeapProfilerFixed::reallocate)
        .function("analyzeFragmentation", &HeapProfilerFixed::analyzeFragmentation)
        .function("getActiveAllocations", &HeapProfilerFixed::getActiveAllocations)
        .function("getMemoryLeakCandidates", &HeapProfilerFixed::getMemoryLeakCandidates)
        .function("getStats", &HeapProfilerFixed::getStats)
        .function("getActiveMemory", &HeapProfilerFixed::getActiveMemory)
        .function("clear", &HeapProfilerFixed::clear);

    enum_<BlockState>("BlockState")
        .value("ALLOCATED", BlockState::ALLOCATED)
        .value("FREE", BlockState::FREE)
        .value("DEALLOCATED", BlockState::DEALLOCATED);
}
#endif
