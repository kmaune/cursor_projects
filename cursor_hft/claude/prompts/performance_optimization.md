# Performance Optimization Prompt Templates

Reusable prompt templates for HFT performance analysis and optimization. These templates provide systematic approaches for different performance optimization scenarios.

**Recommended Agent**: `@claude/agents/hft-performance-optimizer.md`

## Template Categories

### 1. Latency Analysis and Optimization

**Use Case**: Systematic latency bottleneck identification and resolution  
**Customization Parameters**: `[COMPONENT_NAME]`, `[CURRENT_LATENCY]`, `[TARGET_LATENCY]`, `[OPTIMIZATION_FOCUS]`

```
Analyze and optimize [COMPONENT_NAME] latency performance from [CURRENT_LATENCY] to target [TARGET_LATENCY] with focus on [OPTIMIZATION_FOCUS].

CURRENT PERFORMANCE BASELINE:
- Component: [COMPONENT_NAME]
- Current latency: [CURRENT_LATENCY] (measured with HFTTimer)
- Target latency: [TARGET_LATENCY] 
- Optimization focus: [OPTIMIZATION_FOCUS] (algorithm/memory/cache/concurrency)

ANALYSIS REQUIREMENTS:
- Systematic bottleneck identification using profiling tools
- Component-level latency breakdown and critical path analysis
- ARM64-specific performance counter analysis (cache misses, branch mispredictions)
- Memory access pattern analysis and optimization opportunities
- Integration overhead assessment with existing infrastructure

OPTIMIZATION STRATEGY:
- Algorithm complexity reduction where possible
- Memory layout optimization for ARM64 cache hierarchy
- Branch prediction optimization with likely/unlikely hints
- Template specialization for compile-time optimization
- Lock-free algorithm implementation where applicable

IMPLEMENTATION APPROACH:
1. **Baseline Measurement**: Comprehensive latency profiling with statistical analysis
2. **Bottleneck Identification**: Critical path analysis and performance counter data
3. **Optimization Implementation**: Systematic application of optimization techniques
4. **Performance Validation**: Before/after comparison with regression testing
5. **Integration Testing**: Ensure optimizations don't break existing functionality

MEASUREMENT FRAMEWORK:
```cpp
// Performance measurement template
class LatencyOptimizer {
    LatencyHistogram baseline_hist_;
    LatencyHistogram optimized_hist_;
    
public:
    void measure_baseline() {
        for (int i = 0; i < 100000; ++i) {
            auto start = HFTTimer::get_cycles();
            [COMPONENT_NAME]_operation();
            auto end = HFTTimer::get_cycles();
            baseline_hist_.record_latency(HFTTimer::cycles_to_ns(end - start));
        }
    }
    
    OptimizationResults analyze_improvement() {
        auto baseline_stats = baseline_hist_.get_stats();
        auto optimized_stats = optimized_hist_.get_stats();
        
        return OptimizationResults{
            .latency_improvement = baseline_stats.mean_latency - optimized_stats.mean_latency,
            .percentage_improvement = (improvement / baseline_stats.mean_latency) * 100.0,
            .target_achievement = optimized_stats.percentiles[2] <= [TARGET_LATENCY] // 95th percentile
        };
    }
};
```

OPTIMIZATION TECHNIQUES:
- **Algorithm optimization**: Reduce computational complexity
- **Memory optimization**: Cache-friendly data layouts and access patterns  
- **Branch optimization**: Eliminate unpredictable branches in hot paths
- **Template optimization**: Compile-time specialization and inlining
- **ARM64 optimization**: Architecture-specific instruction usage

SUCCESS CRITERIA:
- Achieve [TARGET_LATENCY] latency target (95th percentile)
- Maintain or improve reliability and correctness
- No regression in other performance metrics
- Successful integration with existing infrastructure

Provide systematic optimization approach with measurable improvements and comprehensive validation.
```

### 2. Throughput Improvement

**Use Case**: Maximizing system throughput and processing capacity  
**Customization Parameters**: `[SYSTEM_COMPONENT]`, `[CURRENT_THROUGHPUT]`, `[TARGET_THROUGHPUT]`, `[BOTTLENECK_TYPE]`

```
Optimize [SYSTEM_COMPONENT] throughput from [CURRENT_THROUGHPUT] to [TARGET_THROUGHPUT] by addressing [BOTTLENECK_TYPE] bottlenecks.

THROUGHPUT ANALYSIS:
- Component: [SYSTEM_COMPONENT]  
- Current throughput: [CURRENT_THROUGHPUT] operations/second
- Target throughput: [TARGET_THROUGHPUT] operations/second
- Bottleneck type: [BOTTLENECK_TYPE] (CPU/memory/I-O/concurrency)

OPTIMIZATION OBJECTIVES:
- Maximize sustainable throughput under realistic load
- Maintain low latency characteristics while improving throughput
- Optimize resource utilization (CPU, memory bandwidth, cache)
- Scale efficiently with available hardware resources

ANALYSIS FRAMEWORK:
- Throughput measurement under various load conditions
- Resource utilization analysis (CPU, memory, cache)
- Scalability testing with increasing load and parallel execution
- Efficiency analysis (operations per CPU cycle, memory bandwidth utilization)

OPTIMIZATION STRATEGIES:
- **Batch processing**: Group operations for better cache utilization
- **Pipeline optimization**: Overlap computation and data movement
- **Concurrency improvement**: Reduce lock contention and false sharing
- **Memory bandwidth optimization**: Efficient data access patterns
- **CPU utilization**: Reduce idle time and improve instruction throughput

IMPLEMENTATION TECHNIQUES:
```cpp
// Throughput optimization framework
class ThroughputOptimizer {
public:
    struct ThroughputResults {
        uint64_t operations_per_second;
        double cpu_utilization;
        double cache_hit_rate;
        double memory_bandwidth_utilization;
    };
    
    ThroughputResults measure_throughput(std::chrono::seconds duration) {
        auto start_time = std::chrono::steady_clock::now();
        auto start_cycles = HFTTimer::get_cycles();
        uint64_t operations = 0;
        
        while (std::chrono::steady_clock::now() - start_time < duration) {
            // Batch processing for efficiency
            constexpr size_t BATCH_SIZE = 64;
            process_batch(BATCH_SIZE);
            operations += BATCH_SIZE;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        return ThroughputResults{
            .operations_per_second = operations * 1000 / elapsed.count(),
            .cpu_utilization = calculate_cpu_utilization(),
            .cache_hit_rate = measure_cache_efficiency(),
            .memory_bandwidth_utilization = measure_memory_bandwidth()
        };
    }
};
```

BOTTLENECK-SPECIFIC OPTIMIZATIONS:
- **CPU bottlenecks**: Algorithm optimization, vectorization, parallel processing
- **Memory bottlenecks**: Cache optimization, prefetching, data layout improvement
- **I/O bottlenecks**: Asynchronous processing, batching, queue optimization
- **Concurrency bottlenecks**: Lock-free algorithms, reduced contention, work stealing

VALIDATION REQUIREMENTS:
- Sustained throughput measurement over extended periods
- Latency distribution analysis under high throughput conditions
- Resource utilization monitoring and optimization
- Scalability testing with multiple concurrent processes

SUCCESS CRITERIA:
- Achieve [TARGET_THROUGHPUT] sustainable throughput
- Maintain latency characteristics within acceptable bounds
- Efficient resource utilization without saturation
- Scalable performance with additional hardware resources

Provide comprehensive throughput optimization with sustainable performance improvements.
```

### 3. Memory Access Pattern Optimization

**Use Case**: Cache efficiency improvement and memory bandwidth optimization  
**Customization Parameters**: `[DATA_STRUCTURE]`, `[ACCESS_PATTERN]`, `[CACHE_TARGET]`, `[MEMORY_LAYOUT]`

```
Optimize memory access patterns for [DATA_STRUCTURE] with [ACCESS_PATTERN] to achieve [CACHE_TARGET] cache efficiency through [MEMORY_LAYOUT] optimization.

MEMORY ANALYSIS:
- Data structure: [DATA_STRUCTURE]
- Access pattern: [ACCESS_PATTERN] (sequential/random/hybrid)
- Cache target: [CACHE_TARGET] hit rate goal
- Memory layout: [MEMORY_LAYOUT] optimization focus

CACHE HIERARCHY (ARM64):
- L1 Data Cache: 64 KiB (64-byte lines, ~1000 entries)
- L2 Cache: 4 MiB unified per core cluster
- System Cache: 32 MiB shared across cores
- Memory Bandwidth: ~200 GB/s unified memory

OPTIMIZATION OBJECTIVES:
- Maximize L1 cache hit rate for hot data
- Minimize cache line conflicts and false sharing
- Optimize data layout for predictable access patterns
- Reduce memory bandwidth utilization through better locality

ANALYSIS TECHNIQUES:
- Cache miss analysis using ARM64 performance counters
- Memory access pattern profiling and visualization
- Data structure layout analysis and hot path identification
- Memory bandwidth utilization measurement

OPTIMIZATION STRATEGIES:
```cpp
// Cache-optimized data layout
namespace cache_optimization {

// Structure-of-Arrays for better vectorization
template<typename T, size_t N>
class SOA_Layout {
    // Hot data grouped together
    alignas(64) std::array<typename T::hot_field1_t, N> hot_field1_;
    alignas(64) std::array<typename T::hot_field2_t, N> hot_field2_;
    
    // Cold data in separate cache lines
    alignas(64) std::array<typename T::cold_data_t, N> cold_data_;
    
public:
    // Cache-friendly iteration
    template<typename Func>
    void for_each_hot_data(Func&& func) noexcept {
        // Process hot data with perfect cache locality
        for (size_t i = 0; i < N; ++i) {
            func(hot_field1_[i], hot_field2_[i]);
        }
    }
};

// Cache line padding to prevent false sharing
template<typename T>
struct cache_line_padded {
    alignas(64) T data;
    uint8_t padding[64 - sizeof(T) % 64];
};

} // namespace cache_optimization
```

MEMORY LAYOUT TECHNIQUES:
- **Data structure packing**: Minimize memory footprint and improve cache utilization
- **Hot/cold data separation**: Group frequently accessed data together
- **Cache line alignment**: Align critical data structures to 64-byte boundaries
- **Prefetching**: Strategic prefetch instructions for predictable access patterns
- **Memory pooling**: Reduce allocation overhead and improve locality

ARM64-SPECIFIC OPTIMIZATIONS:
- Cache line prefetching with __builtin_prefetch
- NEON-friendly data layouts for vectorized operations
- Memory ordering optimization for atomic operations
- NUMA-aware allocation for multi-socket systems

MEASUREMENT FRAMEWORK:
- L1/L2 cache miss rate measurement
- Memory bandwidth utilization analysis
- TLB miss rate and virtual memory efficiency
- Data structure memory footprint analysis

VALIDATION REQUIREMENTS:
- Cache performance measurement with performance counters
- Memory access pattern visualization and analysis
- Performance improvement quantification
- Integration testing with realistic workloads

SUCCESS CRITERIA:
- Achieve [CACHE_TARGET] L1 cache hit rate
- Reduce memory bandwidth utilization by measurable percentage
- Improve overall system performance through better memory efficiency
- Maintain correctness while optimizing memory access patterns

Provide memory layout optimization with measurable cache efficiency improvements.
```

### 4. Regression Investigation

**Use Case**: Identifying and resolving performance regressions  
**Customization Parameters**: `[REGRESSION_TYPE]`, `[BASELINE_VERSION]`, `[CURRENT_VERSION]`, `[AFFECTED_COMPONENT]`

```
Investigate [REGRESSION_TYPE] performance regression in [AFFECTED_COMPONENT] between [BASELINE_VERSION] and [CURRENT_VERSION].

REGRESSION ANALYSIS:
- Regression type: [REGRESSION_TYPE] (latency increase/throughput decrease/memory usage)
- Baseline version: [BASELINE_VERSION] with known good performance
- Current version: [CURRENT_VERSION] showing regression
- Affected component: [AFFECTED_COMPONENT]

INVESTIGATION METHODOLOGY:
1. **Performance Comparison**: Detailed before/after analysis
2. **Change Analysis**: Code diff analysis between versions
3. **Profiling**: Identify specific regression sources
4. **Root Cause Analysis**: Systematic elimination of potential causes
5. **Fix Validation**: Ensure resolution doesn't introduce new issues

MEASUREMENT FRAMEWORK:
```cpp
// Regression analysis framework
class RegressionAnalyzer {
public:
    struct RegressionReport {
        double performance_change_percent;
        std::vector<std::string> likely_causes;
        std::vector<std::string> affected_functions;
        std::string root_cause_analysis;
    };
    
    RegressionReport analyze_regression() {
        // Compare baseline vs current performance
        auto baseline_metrics = load_baseline_metrics();
        auto current_metrics = measure_current_performance();
        
        // Identify specific regression areas
        auto regression_areas = identify_regression_sources(baseline_metrics, current_metrics);
        
        // Generate detailed analysis
        return generate_regression_report(regression_areas);
    }
    
private:
    PerformanceMetrics measure_current_performance() {
        // Comprehensive performance measurement
        return PerformanceMetrics{
            .latency_distribution = measure_latency_distribution(),
            .throughput_metrics = measure_throughput(),
            .resource_utilization = measure_resource_usage(),
            .cache_behavior = analyze_cache_performance()
        };
    }
};
```

ANALYSIS TECHNIQUES:
- **Git bisect**: Automated regression source identification
- **Benchmark comparison**: Statistical analysis of performance changes
- **Profiling comparison**: Before/after profiling analysis
- **Code impact analysis**: Change assessment and potential performance impact

COMMON REGRESSION SOURCES:
- **Algorithm changes**: Increased computational complexity
- **Memory allocation**: New allocations in hot paths
- **Synchronization**: Additional locking or atomic operations
- **Compiler optimization**: Changes affecting optimization effectiveness
- **Data structure changes**: Layout modifications affecting cache behavior

INVESTIGATION PROCESS:
1. **Quantify regression**: Precise measurement of performance impact
2. **Isolate changes**: Identify specific code changes between versions
3. **Profile differences**: Compare execution profiles between versions
4. **Test hypotheses**: Systematic testing of potential regression sources
5. **Validate fixes**: Ensure proposed fixes resolve regression without side effects

RESOLUTION STRATEGIES:
- **Code rollback**: Revert problematic changes if no alternative
- **Algorithm optimization**: Improve new algorithm implementation
- **Memory optimization**: Remove unnecessary allocations
- **Compiler optimization**: Adjust compilation flags or pragmas
- **Alternative implementation**: Different approach achieving same functionality

SUCCESS CRITERIA:
- Identify root cause of [REGRESSION_TYPE] regression
- Restore performance to [BASELINE_VERSION] levels or better
- Prevent future regressions through improved testing
- Document regression cause and resolution for future reference

Provide systematic regression investigation with root cause identification and validated resolution.
```

### 5. System-Wide Performance Analysis

**Use Case**: Comprehensive end-to-end performance optimization  
**Customization Parameters**: `[SYSTEM_SCOPE]`, `[ANALYSIS_DEPTH]`, `[OPTIMIZATION_PRIORITY]`

```
Conduct comprehensive system-wide performance analysis of [SYSTEM_SCOPE] with [ANALYSIS_DEPTH] and [OPTIMIZATION_PRIORITY] focus.

SYSTEM ANALYSIS SCOPE:
- System scope: [SYSTEM_SCOPE] (full pipeline/component integration/specific workflow)
- Analysis depth: [ANALYSIS_DEPTH] (surface-level/detailed/comprehensive)
- Optimization priority: [OPTIMIZATION_PRIORITY] (latency/throughput/resource efficiency)

PERFORMANCE BASELINE (Current HFT System):
- End-to-end tick-to-trade: 667ns median (target <15μs, 22.5x better)
- Available headroom: 14.333μs (95.6% unused capacity)
- Component performance: All exceed targets by 3-21x margins
- System utilization: Opportunity for sophisticated algorithm implementation

ANALYSIS FRAMEWORK:
1. **Component Performance**: Individual component latency and throughput analysis
2. **Integration Overhead**: Inter-component communication and coordination costs
3. **Resource Utilization**: CPU, memory, cache, and bandwidth efficiency
4. **Bottleneck Identification**: Critical path analysis and constraint identification
5. **Optimization Opportunities**: Systematic improvement potential assessment

MEASUREMENT APPROACH:
```cpp
// System-wide performance analysis
class SystemPerformanceAnalyzer {
public:
    struct SystemMetrics {
        // End-to-end performance
        LatencyDistribution end_to_end_latency;
        double throughput_ops_per_second;
        
        // Component breakdown
        std::map<std::string, ComponentMetrics> component_performance;
        
        // Resource utilization
        ResourceUtilization resource_usage;
        
        // Bottleneck analysis
        std::vector<PerformanceBottleneck> bottlenecks;
    };
    
    SystemMetrics analyze_system_performance() {
        // Comprehensive system measurement
        return SystemMetrics{
            .end_to_end_latency = measure_end_to_end_pipeline(),
            .throughput_ops_per_second = measure_system_throughput(),
            .component_performance = profile_all_components(),
            .resource_usage = analyze_resource_utilization(),
            .bottlenecks = identify_system_bottlenecks()
        };
    }
    
private:
    // Comprehensive component profiling
    std::map<std::string, ComponentMetrics> profile_all_components() {
        std::map<std::string, ComponentMetrics> results;
        
        // Profile each system component
        results["feed_handler"] = profile_feed_handler();
        results["order_book"] = profile_order_book();
        results["strategy_engine"] = profile_strategy_engine();
        results["risk_manager"] = profile_risk_manager();
        results["order_router"] = profile_order_router();
        
        return results;
    }
};
```

OPTIMIZATION OPPORTUNITY ASSESSMENT:
- **Algorithm sophistication**: Leverage 95.6% unused latency budget for complex algorithms
- **Multi-agent coordination**: Implement parallel strategy development
- **Machine learning integration**: Real-time learning and adaptation
- **Advanced risk management**: Sophisticated risk models and monitoring

HEADROOM UTILIZATION STRATEGY:
```cpp
// Performance headroom allocation
struct HeadroomAllocation {
    HFTTimer::ns_t strategy_algorithm_budget = 5700;    // 5.7μs for complex algorithms
    HFTTimer::ns_t ml_processing_budget = 2900;         // 2.9μs for ML integration
    HFTTimer::ns_t multi_agent_coordination = 2100;    // 2.1μs for agent coordination
    HFTTimer::ns_t advanced_risk_budget = 1400;        // 1.4μs for risk models
    HFTTimer::ns_t monitoring_overhead = 700;          // 0.7μs for enhanced monitoring
    
    // Total allocated: 12.8μs out of 14.333μs available (89% utilization)
    // Remaining buffer: 1.533μs for safety margin
};
```

OPTIMIZATION PRIORITIES:
- **Maintain world-class performance**: Preserve 22.5x margin over targets
- **Leverage available headroom**: Use unused capacity for sophistication
- **System-wide efficiency**: Optimize component integration overhead
- **Future-proof design**: Prepare for increased complexity and features

VALIDATION REQUIREMENTS:
- System-wide performance regression testing
- Component integration validation
- Resource utilization monitoring
- End-to-end performance validation under realistic load

SUCCESS CRITERIA:
- Maintain current world-class performance levels
- Efficiently utilize available performance headroom
- Identify and implement high-impact optimization opportunities
- Establish baseline for future sophisticated algorithm development

Provide comprehensive system analysis with strategic optimization recommendations and headroom utilization plan.
```

## Template Usage Guidelines

### Analysis Approach Selection
- **Targeted optimization**: Use specific templates for focused improvements
- **Systematic analysis**: Use comprehensive templates for broad optimization
- **Regression investigation**: Use investigation templates for problem resolution
- **System-wide view**: Use system analysis for strategic optimization planning

### Measurement Best Practices
- **Statistical rigor**: Multiple measurements with proper statistical analysis
- **Realistic conditions**: Test under actual operating conditions
- **Baseline establishment**: Proper baseline measurement before optimization
- **Regression prevention**: Comprehensive testing to prevent performance regressions

### Optimization Validation
- **Performance improvement quantification**: Measure and document improvements
- **Correctness preservation**: Ensure optimizations don't break functionality
- **Integration testing**: Validate optimizations work with existing infrastructure
- **Long-term monitoring**: Track optimization effectiveness over time

These templates provide systematic approaches to HFT performance optimization while maintaining the high standards required for financial trading systems.
