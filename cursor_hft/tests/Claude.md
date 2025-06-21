# Testing Standards for HFT Components

## Test Categories (All Required)
1. **Functional Correctness Tests**
   - Core component behavior validation
   - Input/output verification
   - State management correctness

2. **Performance Benchmark Tests**
   - Latency measurements using timing framework
   - Throughput validation for data processing
   - Memory allocation verification (must be zero)
   - Cache behavior analysis where applicable

3. **Integration Tests**
   - Object pool compatibility
   - Ring buffer messaging functionality
   - Treasury data structure integration
   - Existing component interaction validation

4. **Edge Case & Error Handling Tests**
   - Boundary condition testing
   - Error recovery validation
   - Resource exhaustion scenarios
   - Invalid input handling

## Performance Validation Requirements
- **Timing Framework Integration:** Use established nanosecond precision timing
- **Statistical Analysis:** Report median, 95th, 99th percentile latencies
- **Load Testing:** Validate performance under realistic message rates
- **Regression Prevention:** Ensure new tests don't break existing functionality

## Build Integration Requirements (Critical)
```cmake
# MUST add to main CMakeLists.txt for every component
add_executable(hft_${component}_test tests/${path}/${component}_test.cpp)
target_link_libraries(hft_${component}_test 
    PRIVATE 
        hft_market_data hft_timing hft_memory hft_messaging
        gtest_main gtest)
add_test(NAME hft_${component}_test COMMAND hft_${component}_test)
```

## Test Validation Criteria
- [ ] All tests build successfully
- [ ] All existing tests continue to pass  
- [ ] New functionality tests pass
- [ ] Performance benchmarks meet component targets
- [ ] Integration tests validate compatibility
- [ ] CMakeLists.txt properly updated
