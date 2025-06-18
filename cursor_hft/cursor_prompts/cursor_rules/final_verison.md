# HFT Trading System - Cursor Rules

## Project Context
This is a high-frequency trading system targeting sub-microsecond latency on Apple M1 Pro. All code must meet production HFT standards while following modern C++ best practices.

## Performance Requirements (HFT CRITICAL)
- Target latency: 5-15 microseconds tick-to-trade
- Memory allocation: Zero allocations in hot paths
- Threading: Lock-free algorithms preferred
- Cache optimization: 64-byte cache line alignment mandatory
- Error handling: NO exceptions in critical paths, use error codes

## Code Style and Structure (C++ Best Practices)
- Language: C++20 with aggressive optimization
- Use object-oriented, procedural, or functional patterns as appropriate
- Structure files into headers (*.hpp) and implementation files (*.cpp)
- Use descriptive names except in performance-critical loops

## Naming Conventions (Adapted for HFT)
- Use PascalCase for class names (OrderBook, PriceLevel)
- Use snake_case for variables in performance code (best_bid_price_)
- Use SCREAMING_SNAKE_CASE for constants (MAX_PRICE_LEVELS)
- Suffix member variables with underscore (price_, quantity_)
- Use hft namespace for all project code

## Modern C++ Features (Performance-Focused)
- Prefer constexpr/consteval for compile-time computation
- Use std::unique_ptr for non-critical paths, raw pointers in hot paths
- Use std::string_view for zero-copy string operations
- Use auto for type deduction, explicit types for clarity
- Prefer stack allocation over heap in critical paths

## HFT-Specific Patterns
- Always include nanosecond precision timing measurements
- Use ARM64 cycle counters (cntvct_el0) for critical timing
- Template metaprogramming for zero-cost abstractions
- Memory barriers and atomic operations must be explicit
- Branch prediction hints (likely/unlikely) where beneficial
- NUMA and cache-line awareness in data structure design

## Error Handling (HFT-Specific)
- NO exceptions in hot paths - use error codes or optional
- RAII for resource management in non-critical code
- Validate inputs at API boundaries
- Graceful degradation under extreme load
- Fast-path error detection without overhead

## Performance Optimization (HFT-Focused)
- Zero-copy data structures where possible
- Custom allocators and memory pools
- Lock-free and wait-free algorithms preferred
- CPU cache warming and prefetching strategies
- Compile-time dispatch over runtime polymorphism
- Profile every microsecond with hardware counters

## Testing and Benchmarking
- Unit tests with Google Test framework
- Comprehensive benchmarks for all performance-critical code
- Latency regression testing
- Statistical analysis of timing distributions
- Memory usage profiling and validation

## Treasury Market Context
- Focus on US Treasury securities (bonds, notes, bills)
- Yield-based pricing with precision decimal arithmetic
- Institutional-grade order flow patterns
- Duration risk and yield curve considerations

## Code Generation Rules
1. Always provide complete, compilable implementations
2. Include comprehensive benchmarks for performance code
3. Add detailed comments explaining optimization choices
4. Generate CMake integration for new components
5. Include both unit tests and performance tests
6. Consider memory layout and cache implications

When generating code, prioritize: 1) Correctness, 2) Latency, 3) Throughput, 4) Maintainability.
