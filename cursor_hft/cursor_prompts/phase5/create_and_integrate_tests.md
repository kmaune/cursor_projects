# Complete Feed Handler Build Integration & Testing

I have implemented a feed handler framework (`include/hft/market_data/feed_handler.hpp`) but need complete build system integration and comprehensive testing.

## Required Updates

### 1. CMakeLists.txt Integration
Add feed handler components to the main CMakeLists.txt:
- Feed handler test executable (`hft_feed_handler_test`)
- Feed handler benchmark executable (`hft_feed_handler_benchmark`) 
- Proper linking with existing libraries (hft_market_data, hft_timing, hft_memory, hft_messaging)
- Integration with CTest framework

### 2. Comprehensive Test Suite
Create `tests/market_data/feed_handler_test.cpp` with:
- Message structure validation (64-byte alignment)
- Message parsing accuracy tests (tick/trade message types)
- Checksum validation tests (valid/invalid scenarios)
- Batch parsing performance and correctness
- Feed handler processing workflow tests
- Quality control validation (duplicates, sequence gaps)
- Message normalization tests
- Integration tests with object pools and ring buffers
- Error handling and edge cases

### 3. Performance Benchmark Suite  
Create `benchmarks/market_data/feed_handler_benchmark.cpp` with:
- Single message parsing latency (target: <200ns)
- Batch processing throughput (target: >1M messages/second)
- Feed handler end-to-end processing
- Object pool integration performance
- Checksum validation speed
- Message normalization overhead
- Throughput measurement and validation

## Performance Requirements
- **Message parsing:** <200ns per message
- **Throughput:** >1M messages/second sustained
- **Quality controls:** <10% overhead
- **Integration:** Match existing component performance (29.7ns ring buffer, 8.26ns object pool)

## Integration Requirements
- Must work seamlessly with existing treasury data structures
- Leverage existing HFTTimer for latency measurement
- Use existing ObjectPool and SPSCRingBuffer templates
- Maintain zero-allocation design in hot paths
- Follow existing project patterns and conventions

## Test Data Generation
Include helper functions to generate realistic test data:
- Sample RawMarketMessage creation with proper checksums
- Random but valid treasury market data
- Performance stress test scenarios
- Edge case data (invalid checksums, sequence gaps, duplicates)

## Expected Directory Structure
```
tests/market_data/feed_handler_test.cpp
benchmarks/market_data/feed_handler_benchmark.cpp
CMakeLists.txt (updated with new executables)
```

Generate all necessary files and build system updates to create a complete, production-ready testing and benchmarking infrastructure for the feed handler framework.
