# Automated Benchmark System

The HFT Trading System includes a comprehensive automated benchmarking infrastructure that enables developers to easily run performance tests and detect regressions, similar to how `ctest` works for unit tests.

## Overview

The automated benchmark system provides:
- **One-command benchmark execution**: Run all benchmarks with `make benchmark`
- **Regression detection**: Automatic comparison against performance baselines
- **HFT-appropriate tolerances**: Configurable thresholds for different metric types
- **CI/CD integration**: CTest integration for automated testing
- **Baseline management**: Tools to create and update performance expectations

## Quick Start

### Running Benchmarks

```bash
# Build the project
cmake -B build -DCMAKE_BUILD_TYPE=Release
cd build && make -j

# Run all benchmarks
make benchmark

# Validate performance against baselines  
make benchmark-validate

# Run performance validation as part of test suite
ctest -L performance
```

### First-Time Setup

If you're setting up the system for the first time:

```bash
# Run benchmarks to collect current performance data
make benchmark

# Create initial baseline from current results
../scripts/manage_baseline.sh create benchmark_results/latest_results.json

# Validate the baseline works
make benchmark-validate
```

## Detailed Usage

### 1. Benchmark Execution

The system includes 9 comprehensive benchmarks covering all critical components:

- **hft_timing_benchmark**: High-precision timing framework
- **hft_object_pool_benchmark**: Memory allocation performance
- **hft_spsc_ring_buffer_benchmark**: Lock-free messaging
- **hft_treasury_yield_benchmark**: Treasury market calculations
- **hft_treasury_pool_benchmark**: Treasury-specific pools
- **hft_treasury_ring_buffer_benchmark**: Treasury message passing
- **hft_order_book_benchmark**: Order management operations
- **hft_feed_handler_benchmark**: Market data processing
- **hft_end_to_end_benchmark**: Complete tick-to-trade pipeline

#### Manual Benchmark Execution

```bash
# Run specific benchmark
./hft_end_to_end_benchmark

# Run with JSON output for analysis
./hft_end_to_end_benchmark --benchmark_format=json --benchmark_out=results.json

# Run all benchmarks manually
../scripts/run_benchmarks.sh
```

### 2. Performance Validation

The validation system compares current performance against established baselines with HFT-appropriate tolerances:

```bash
# Validate against baselines (non-strict mode)
../scripts/validate_performance.py .

# Strict mode (warnings become failures)
../scripts/validate_performance.py . --strict

# Show detailed output including passing tests
../scripts/validate_performance.py . --show-passing
```

#### Tolerance Levels

The system uses different tolerance levels based on metric criticality:

- **Critical Latency Metrics** (end-to-end, order operations): 10% tolerance
- **Throughput Metrics** (messages/sec, operations/sec): 15% tolerance  
- **Micro-benchmarks** (object pool, ring buffer): 20% tolerance
- **Component Integration**: 12% tolerance

#### Validation Results

```
✅ PASS: Metric within tolerance
⚠️  WARNING: Metric outside tolerance but < 2x tolerance (non-strict mode)
❌ FAIL: Metric outside tolerance threshold
❓ MISSING: Metric not found in results
💥 ERROR: Unable to extract metric from results
```

### 3. Baseline Management

#### Viewing Current Baselines

```bash
# Show all current baseline values
../scripts/manage_baseline.sh show

# Compare current results against baseline
../scripts/manage_baseline.sh compare benchmark_results/latest_results.json
```

#### Creating New Baselines

```bash
# Create new baseline from current results
../scripts/manage_baseline.sh create benchmark_results/latest_results.json

# Force creation (skip confirmation)
../scripts/manage_baseline.sh -f create benchmark_results/latest_results.json
```

#### Updating Existing Baselines

```bash
# Update baseline with improved performance
../scripts/manage_baseline.sh update benchmark_results/latest_results.json

# View what changes would be made first
../scripts/manage_baseline.sh compare benchmark_results/latest_results.json
```

#### Backup and Restore

```bash
# Create backup of current baseline
../scripts/manage_baseline.sh backup

# Restore from backup
../scripts/manage_baseline.sh restore baselines_backup/performance_baselines_20250621_143022.json

# Validate baseline file format
../scripts/manage_baseline.sh validate
```

## Current Performance Baselines

The system is configured with the following baseline expectations based on Phase 1-3 optimization results:

### Critical Performance Metrics

| **Component** | **Metric** | **Baseline** | **Tolerance** |
|---------------|------------|--------------|---------------|
| **End-to-End** | Tick-to-trade latency | 667ns | ±10% |
| **Feed Handler** | End-to-end throughput | 1.98M msgs/sec | ±10% |
| **Order Book** | Order addition | 407ns | ±10% |
| **Order Book** | Order cancellation | 455ns | ±10% |
| **Order Book** | Best bid/offer | 13ns | ±15% |
| **Ring Buffer** | Single operation | 30ns | ±15% |

### Secondary Metrics

| **Component** | **Metric** | **Baseline** | **Tolerance** |
|---------------|------------|--------------|---------------|
| **Object Pool** | Allocation time | 8ns | ±20% |
| **Treasury Yield** | Price-to-yield conversion | 75ns | ±20% |
| **Feed Handler** | Single message latency | 1,496ns | ±15% |
| **Order Book** | Mixed operations throughput | 2.57M ops/sec | ±10% |

## Integration with Development Workflow

### Development Workflow

```bash
# 1. Make performance-related changes
vim include/hft/trading/order_book.hpp

# 2. Build and test functionality
make && ctest

# 3. Validate performance
make benchmark-validate

# 4. If performance improved, update baseline
../scripts/manage_baseline.sh update benchmark_results/latest_results.json
```

### Continuous Integration

The system integrates with CTest for automated CI/CD:

```bash
# Run all tests including performance validation
ctest

# Run only performance tests
ctest -L performance

# Run with specific timeout
ctest -L performance --timeout 300
```

#### CI Configuration Example

```yaml
# .github/workflows/performance.yml
- name: Run Performance Tests
  run: |
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cd build
    make -j
    ctest -L performance --output-on-failure
```

## Troubleshooting

### Common Issues

#### 1. No Baseline File
```
❌ Error: Baseline file not found: performance_baselines.json
```
**Solution**: Create initial baseline with `../scripts/manage_baseline.sh create`

#### 2. Benchmark Executable Missing
```
Error: Benchmark executable hft_end_to_end_benchmark not found
```
**Solution**: Build all targets with `make -j`

#### 3. High Performance Variance
```
⚠️  WARNING: Multiple metrics show high variance
```
**Solutions**:
- Run on dedicated hardware
- Close background applications
- Use `sudo` for system optimizations
- Run multiple times and average results

#### 4. All Benchmarks Failing
```
❌ CRITICAL FAILURE: 15 critical metrics failed
```
**Solutions**:
- Check for debug build (should use Release)
- Verify system isn't under heavy load
- Check for recent code changes that might affect performance
- Compare against previous results to identify patterns

### System Optimization

For best benchmarking results:

```bash
# macOS optimization
sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.metadata.mds.plist
renice -20 $$

# Linux optimization  
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
taskset -c 0 make benchmark
```

### Debugging Performance Issues

#### 1. Identify Specific Failures
```bash
# Run with detailed output
../scripts/validate_performance.py . --show-passing

# Compare specific benchmark
../scripts/manage_baseline.sh compare benchmark_results/latest_results.json
```

#### 2. Historical Analysis
```bash
# Check recent baselines
ls baselines_backup/

# Compare against previous baseline
../scripts/manage_baseline.sh restore baselines_backup/performance_baselines_20250620_120000.json
make benchmark-validate
```

#### 3. Component-Level Investigation
```bash
# Run specific problematic benchmark
./hft_order_book_benchmark --benchmark_filter=OrderAddition

# Run with verbose output
./hft_order_book_benchmark --benchmark_format=json | jq .
```

## Advanced Configuration

### Custom Tolerance Settings

Edit `performance_baselines.json` to adjust tolerances:

```json
{
  "benchmarks": {
    "hft_end_to_end_benchmark": {
      "metrics": {
        "tick_to_trade_latency_median_ns": {
          "baseline": 667,
          "tolerance_percent": 5,  // Stricter tolerance
          "critical": true
        }
      }
    }
  }
}
```

### Adding New Metrics

To add validation for new benchmarks:

1. Add benchmark executable to CMakeLists.txt `HFT_BENCHMARK_TARGETS`
2. Add metric definitions to `performance_baselines.json`
3. Run `../scripts/manage_baseline.sh validate` to verify format

### Environment-Specific Baselines

For different environments (CI vs. development):

```bash
# Use environment-specific baseline
../scripts/validate_performance.py . --baseline performance_baselines_ci.json

# Create environment-specific baseline
cp performance_baselines.json performance_baselines_ci.json
# Edit CI-specific tolerances
```

## Best Practices

### 1. Regular Validation
- Run `make benchmark-validate` before committing performance changes
- Include performance tests in pull request validation
- Monitor baseline drift over time

### 2. Baseline Updates
- Only update baselines for legitimate performance improvements
- Always backup before updating baselines
- Document baseline changes in commit messages

### 3. Regression Investigation
- Investigate all performance failures, even small ones
- Use git bisect to identify performance regression commits
- Consider both average and tail latency impacts

### 4. System Consistency
- Run benchmarks on consistent hardware configurations
- Control for background processes and system load
- Use multiple runs for statistical significance

## Summary

The automated benchmark system provides comprehensive performance validation with minimal developer overhead. Key benefits:

✅ **One-command execution**: `make benchmark-validate`  
✅ **HFT-appropriate tolerances**: 5-20% based on metric criticality  
✅ **Regression detection**: Automatic comparison against baselines  
✅ **CI/CD integration**: Works with existing test infrastructure  
✅ **Baseline management**: Easy creation and updates of performance expectations  

This ensures the HFT trading system maintains its world-class performance characteristics (667ns tick-to-trade, 1.98M msgs/sec throughput) as development continues.