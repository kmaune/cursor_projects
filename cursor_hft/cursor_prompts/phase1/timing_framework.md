Create a high-performance timing framework for our HFT system that can measure nanosecond-precision latencies with comprehensive statistical analysis. 

Requirements:
- ARM64 cycle counter integration (cntvct_el0)
- Zero allocation in measurement hot paths
- Thread-safe latency histogram collection
- Statistical analysis (min/max/mean/percentiles/standard deviation)
- Latency regression detection
- Performance benchmark suite
- CMake integration

The framework should support:
1. Single measurement timing
2. Automatic scope-based timing (RAII)
3. High-frequency measurement collection (millions per second)
4. Real-time latency monitoring
5. Export capabilities for analysis

Generate complete implementation with header files, source files, comprehensive unit tests, and benchmarks.
