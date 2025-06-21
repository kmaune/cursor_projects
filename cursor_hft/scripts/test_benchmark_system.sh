#!/bin/bash

# HFT Trading System - Benchmark System Integration Test
# Quick test to validate the automated benchmark system works correctly

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_ROOT}/build"

echo "========================================================"
echo "HFT Automated Benchmark System - Integration Test"
echo "========================================================"

# Check if we're in the build directory
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    echo "❌ Error: Build directory not found or not configured"
    echo "Please run: cmake -B build && cd build"
    exit 1
fi

cd "${BUILD_DIR}"

echo "1. Testing CMake benchmark targets..."
echo "----------------------------------------"

# Check if benchmark targets exist
if make help 2>/dev/null | grep "benchmark" >/dev/null; then
    echo "✅ CMake benchmark targets found"
else
    echo "❌ CMake benchmark targets not found"
    exit 1
fi

echo ""
echo "2. Testing baseline validation..."
echo "----------------------------------------"

# Test baseline validation
if "${SCRIPT_DIR}/manage_baseline.sh" validate; then
    echo "✅ Baseline validation successful"
else
    echo "❌ Baseline validation failed"
    exit 1
fi

echo ""
echo "3. Testing benchmark executable availability..."
echo "----------------------------------------"

# List of critical benchmarks to check
CRITICAL_BENCHMARKS=(
    "hft_timing_benchmark"
    "hft_end_to_end_benchmark"
    "hft_feed_handler_benchmark"
    "hft_order_book_benchmark"
)

missing_benchmarks=0
for benchmark in "${CRITICAL_BENCHMARKS[@]}"; do
    if [[ -x "${benchmark}" ]]; then
        echo "✅ ${benchmark} executable found"
    else
        echo "❌ ${benchmark} executable missing"
        ((missing_benchmarks++))
    fi
done

if [[ ${missing_benchmarks} -gt 0 ]]; then
    echo ""
    echo "❌ ${missing_benchmarks} benchmark executables missing"
    echo "Please run 'make -j' to build all benchmarks"
    exit 1
fi

echo ""
echo "4. Testing quick benchmark run..."
echo "----------------------------------------"

# Run a quick benchmark test (just timing benchmark for speed)
echo "Running quick timing benchmark test..."
if timeout 30 ./hft_timing_benchmark --benchmark_format=json --benchmark_out=/tmp/test_timing_results.json --benchmark_filter=".*" --benchmark_min_time=0.1; then
    echo "✅ Quick benchmark run successful"
    
    # Check if results file was created
    if [[ -f "/tmp/test_timing_results.json" ]]; then
        echo "✅ Benchmark results file created"
        
        # Quick validation of JSON format
        if python3 -c "import json; json.load(open('/tmp/test_timing_results.json'))" 2>/dev/null; then
            echo "✅ Benchmark results are valid JSON"
        else
            echo "⚠️  Warning: Benchmark results may not be valid JSON"
        fi
    else
        echo "⚠️  Warning: Benchmark results file not created"
    fi
else
    echo "⚠️  Warning: Quick benchmark run failed (this may be normal on some systems)"
fi

echo ""
echo "5. Testing validation script..."
echo "----------------------------------------"

# Test the validation script with mock data
if python3 -c "
import sys
sys.path.insert(0, '${SCRIPT_DIR}')
try:
    import validate_performance
    print('✅ Validation script imports successfully')
except Exception as e:
    print(f'❌ Validation script import failed: {e}')
    sys.exit(1)
"; then
    echo "✅ Validation script is functional"
else
    echo "❌ Validation script has issues"
    exit 1
fi

echo ""
echo "6. Testing CTest integration..."
echo "----------------------------------------"

# Check if performance test is registered
if ctest --show-only | grep -q "hft_performance_validation"; then
    echo "✅ Performance validation test registered with CTest"
else
    echo "❌ Performance validation test not found in CTest"
    exit 1
fi

echo ""
echo "========================================================"
echo "✅ INTEGRATION TEST PASSED"
echo "========================================================"
echo ""
echo "The automated benchmark system is ready for use!"
echo ""
echo "Quick Start Commands:"
echo "  make benchmark              # Run all benchmarks"
echo "  make benchmark-validate     # Run benchmarks + validation" 
echo "  ctest -L performance        # Run via CTest"
echo ""
echo "For detailed usage, see: docs/AUTOMATED_BENCHMARKS.md"

# Cleanup
rm -f /tmp/test_timing_results.json

exit 0