#!/bin/bash

# HFT Trading System - Automated Benchmark Runner
# Runs all benchmarks and collects results for performance validation

set -euo pipefail

# Auto-detect project root and build directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build}"
RESULTS_DIR="${BUILD_DIR}/benchmark_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULTS_FILE="${RESULTS_DIR}/benchmark_results_${TIMESTAMP}.json"
LATEST_RESULTS="${RESULTS_DIR}/latest_results.json"

# System optimization for benchmarking
optimize_system() {
    echo "Optimizing system for benchmarking..."
    
    # macOS-specific optimizations
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Reduce background processes interference
        sudo launchctl unload -w /System/Library/LaunchDaemons/com.apple.metadata.mds.plist 2>/dev/null || true
        
        # Set high priority for benchmark process
        renice -20 $$ 2>/dev/null || echo "Warning: Could not set high priority (run with sudo for better results)"
        
        echo "macOS system optimizations applied"
    fi
    
    # Linux-specific optimizations
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Disable CPU frequency scaling
        echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true
        
        # Set CPU affinity if possible
        taskset -c 0 bash 2>/dev/null || echo "Warning: Could not set CPU affinity"
        
        echo "Linux system optimizations applied"
    fi
}

# Restore system settings
cleanup_system() {
    echo "Restoring system settings..."
    
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Re-enable spotlight indexing
        sudo launchctl load -w /System/Library/LaunchDaemons/com.apple.metadata.mds.plist 2>/dev/null || true
    fi
    
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Restore CPU frequency scaling
        echo ondemand | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true
    fi
}

# Trap to ensure cleanup on exit
trap cleanup_system EXIT

# Ensure results directory exists
mkdir -p "${RESULTS_DIR}"

# List of benchmarks to run
BENCHMARKS=(
    "hft_timing_benchmark"
    "hft_object_pool_benchmark" 
    "hft_spsc_ring_buffer_benchmark"
    "hft_treasury_yield_benchmark"
    "hft_treasury_pool_benchmark"
    "hft_treasury_ring_buffer_benchmark"
    "hft_order_book_benchmark"
    "hft_feed_handler_benchmark"
    "hft_end_to_end_benchmark"
)

# Check if all benchmark executables exist
echo "Checking benchmark executables..."
for benchmark in "${BENCHMARKS[@]}"; do
    if [[ ! -x "${BUILD_DIR}/${benchmark}" ]]; then
        echo "Error: Benchmark executable ${benchmark} not found in ${BUILD_DIR}"
        echo "Please run 'make' to build all benchmarks first"
        exit 1
    fi
done

echo "Starting HFT benchmark suite..."
echo "Build directory: ${BUILD_DIR}"
echo "Results will be saved to: ${RESULTS_FILE}"
echo ""

# Apply system optimizations
optimize_system

# Initialize combined results file
echo "{" > "${RESULTS_FILE}"
echo "  \"timestamp\": \"$(date -Iseconds)\"," >> "${RESULTS_FILE}"
echo "  \"system_info\": {" >> "${RESULTS_FILE}"
echo "    \"hostname\": \"$(hostname)\"," >> "${RESULTS_FILE}"
echo "    \"os\": \"$(uname -s)\"," >> "${RESULTS_FILE}"
echo "    \"arch\": \"$(uname -m)\"," >> "${RESULTS_FILE}"
echo "    \"kernel\": \"$(uname -r)\"" >> "${RESULTS_FILE}"
echo "  }," >> "${RESULTS_FILE}"
echo "  \"benchmarks\": {" >> "${RESULTS_FILE}"

# Run each benchmark
BENCHMARK_COUNT=${#BENCHMARKS[@]}
CURRENT=1

for benchmark in "${BENCHMARKS[@]}"; do
    echo "==================================================="
    echo "Running benchmark ${CURRENT}/${BENCHMARK_COUNT}: ${benchmark}"
    echo "==================================================="
    
    # Create individual result file
    INDIVIDUAL_RESULT="${RESULTS_DIR}/${benchmark}_${TIMESTAMP}.json"
    
    # Run benchmark with JSON output and error handling
    # Skip timeout on macOS for simplicity (benchmarks should complete quickly)
    echo "Running: ${BUILD_DIR}/${benchmark}"
    
    if "${BUILD_DIR}/${benchmark}" \
        --benchmark_format=json \
        --benchmark_out="${INDIVIDUAL_RESULT}" \
        --benchmark_repetitions=3 \
        --benchmark_report_aggregates_only=true \
        --benchmark_display_aggregates_only=true; then
        
        echo "✅ ${benchmark} completed successfully"
        
        # Extract and add results to combined file
        if [[ -f "${INDIVIDUAL_RESULT}" && -s "${INDIVIDUAL_RESULT}" ]]; then
            echo "    \"${benchmark}\": " >> "${RESULTS_FILE}"
            cat "${INDIVIDUAL_RESULT}" >> "${RESULTS_FILE}"
            
            # Add comma except for last benchmark
            if [[ ${CURRENT} -lt ${BENCHMARK_COUNT} ]]; then
                echo "," >> "${RESULTS_FILE}"
            fi
        else
            echo "⚠️  Warning: ${benchmark} produced no output"
            echo "    \"${benchmark}\": null" >> "${RESULTS_FILE}"
            if [[ ${CURRENT} -lt ${BENCHMARK_COUNT} ]]; then
                echo "," >> "${RESULTS_FILE}"
            fi
        fi
    else
        exit_code=$?
        echo "❌ ${benchmark} failed with exit code ${exit_code}"
        
        echo "    \"${benchmark}\": {\"error\": \"benchmark_failed\", \"exit_code\": ${exit_code}}" >> "${RESULTS_FILE}"
        if [[ ${CURRENT} -lt ${BENCHMARK_COUNT} ]]; then
            echo "," >> "${RESULTS_FILE}"
        fi
    fi
    
    echo ""
    ((CURRENT++))
done

# Close JSON structure
echo "  }" >> "${RESULTS_FILE}"
echo "}" >> "${RESULTS_FILE}"

# Create symlink to latest results
ln -sf "$(basename "${RESULTS_FILE}")" "${LATEST_RESULTS}"

echo "==================================================="
echo "HFT Benchmark Suite Complete"
echo "==================================================="
echo "Results saved to: ${RESULTS_FILE}"
echo "Latest results: ${LATEST_RESULTS}"
echo ""

# Generate summary
echo "Benchmark Summary:"
echo "=================="
for benchmark in "${BENCHMARKS[@]}"; do
    if grep -q "\"${benchmark}\".*error" "${RESULTS_FILE}"; then
        echo "❌ ${benchmark}: FAILED"
    elif grep -q "\"${benchmark}\".*null" "${RESULTS_FILE}"; then
        echo "⚠️  ${benchmark}: NO OUTPUT"
    else
        echo "✅ ${benchmark}: SUCCESS"
    fi
done

echo ""
echo "To validate performance against baselines, run:"
echo "  make benchmark-validate"
echo "Or directly:"
echo "  scripts/validate_performance.py ${BUILD_DIR}"