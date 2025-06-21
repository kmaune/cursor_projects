# Performance Benchmarking Standards

## Benchmark Requirements
Every component must include comprehensive benchmarks that validate:
- **Latency targets** for the specific component
- **Throughput capabilities** under realistic loads
- **Memory allocation behavior** (zero allocation verification)
- **Cache performance** where relevant to optimization

## Benchmark Implementation Pattern
```cpp
// Use established timing framework
#include "hft/timing/high_precision_timer.hpp"

// Statistical analysis of latency distribution
void BenchmarkComponentLatency() {
    std::vector<uint64_t> latencies;
    // Collect latency samples
    // Report percentiles: median, 95th, 99th, max
}
```

## Performance Target Validation
- **Component-level targets:** Specific to each component's function
- **Integration overhead:** Measure actual vs theoretical performance
- **Load behavior:** Performance degradation under increasing load
- **Consistency:** Latency variance and tail behavior analysis

## Build Integration for Benchmarks
```cmake
# MUST add to main CMakeLists.txt
add_executable(hft_${component}_benchmark benchmarks/${path}/${component}_benchmark.cpp)
target_link_libraries(hft_${component}_benchmark 
    PRIVATE 
        hft_market_data hft_timing hft_memory hft_messaging
        benchmark::benchmark benchmark::benchmark_main)
```
