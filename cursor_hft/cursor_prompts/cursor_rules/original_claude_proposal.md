# HFT Trading System - Cursor Rules

## Project Context
This is a high-frequency trading system targeting sub-microsecond latency on Apple M1 Pro. All code must meet production HFT standards.

## Performance Requirements
- Target latency: 5-15 microseconds tick-to-trade
- Memory allocation: Zero allocations in hot paths
- Threading: Lock-free algorithms preferred
- Cache optimization: 64-byte cache line alignment mandatory

## Code Standards
- Language: C++20 with aggressive optimization
- Architecture: ARM64 Apple Silicon optimized
- Patterns: RAII, zero-cost abstractions, template metaprogramming
- Memory: Custom allocators, object pools, cache-friendly layouts
- Concurrency: Atomic operations, memory ordering, lock-free data structures

## HFT-Specific Requirements
- Always include nanosecond precision timing
- Use ARM64 cycle counters (cntvct_el0) for critical measurements
- Prefer compile-time computation (constexpr/consteval)
- Include comprehensive benchmarks for any performance-critical code
- Memory barriers and synchronization must be explicit
- Branch prediction optimization (likely/unlikely hints)

## Code Generation Rules
1. Always provide complete, compilable implementations
2. Include error handling without exceptions in hot paths
3. Add comprehensive unit tests and benchmarks
4. Use modern C++ features for zero-overhead abstractions
5. Include detailed performance comments explaining optimization choices
6. Generate CMake integration for all new components

## Treasury Market Context
- Focus on US Treasury securities (bonds, notes, bills)
- Yield-based pricing considerations
- Institutional-grade order flow patterns
- Duration risk and yield curve awareness

When generating code, prioritize correctness, then latency, then throughput, then maintainability.
