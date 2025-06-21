#!/bin/bash

# HFT Trading System - Baseline Management Script
# Create, update, and manage performance baselines

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BASELINE_FILE="${PROJECT_ROOT}/performance_baselines.json"

# Helper functions
usage() {
    cat << EOF
HFT Baseline Management Script

Usage: $0 [OPTIONS] COMMAND

Commands:
    create <results_file>     Create new baseline from benchmark results
    update <results_file>     Update existing baseline with new values
    backup                    Create backup of current baseline
    restore <backup_file>     Restore baseline from backup
    validate                  Validate current baseline format
    show                      Display current baseline values
    compare <results_file>    Compare results against current baseline

Options:
    -h, --help               Show this help message
    -f, --force              Force operation without confirmation
    -v, --verbose            Enable verbose output
    -b, --backup-dir DIR     Specify backup directory (default: backups/)

Examples:
    $0 create build/benchmark_results/latest_results.json
    $0 update build/benchmark_results/latest_results.json
    $0 backup
    $0 compare build/benchmark_results/latest_results.json

EOF
}

log() {
    if [[ "${VERBOSE:-0}" == "1" ]]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') [INFO] $*"
    fi
}

error() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') [ERROR] $*" >&2
}

confirm() {
    if [[ "${FORCE:-0}" == "1" ]]; then
        return 0
    fi
    
    read -p "$1 (y/N): " -n 1 -r
    echo
    [[ $REPLY =~ ^[Yy]$ ]]
}

# Extract metric value from Google Benchmark JSON
extract_metric_value() {
    local results_file="$1"
    local benchmark_name="$2" 
    local metric_pattern="$3"
    
    # Use Python to extract metric values
    python3 << EOF
import json
import sys

try:
    with open('$results_file', 'r') as f:
        data = json.load(f)
    
    benchmarks = data.get('benchmarks', {}).get('$benchmark_name', {}).get('benchmarks', [])
    
    for benchmark in benchmarks:
        name = benchmark.get('name', '').lower()
        if '$metric_pattern' in name:
            # Extract primary timing value
            if 'real_time' in benchmark:
                print(benchmark['real_time'])
                sys.exit(0)
            elif 'cpu_time' in benchmark:
                print(benchmark['cpu_time'])
                sys.exit(0)
            elif 'items_per_second' in benchmark:
                print(benchmark['items_per_second'])
                sys.exit(0)
        
        # Check counters
        if 'counters' in benchmark:
            for counter_name, counter_value in benchmark['counters'].items():
                if '$metric_pattern' in counter_name.lower():
                    if isinstance(counter_value, dict):
                        print(counter_value.get('value', 0))
                    else:
                        print(counter_value)
                    sys.exit(0)
    
    print(0)  # Default value if not found

except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    print(0)
EOF
}

# Create new baseline from benchmark results
create_baseline() {
    local results_file="$1"
    
    if [[ ! -f "$results_file" ]]; then
        error "Results file not found: $results_file"
        return 1
    fi
    
    if [[ -f "$BASELINE_FILE" ]]; then
        if ! confirm "Baseline file already exists. Overwrite?"; then
            echo "Operation cancelled."
            return 1
        fi
        backup_baseline
    fi
    
    log "Creating new baseline from $results_file"
    
    # Extract key metrics from results and update baseline template
    python3 << EOF
import json
import sys
from datetime import datetime

# Load current results
try:
    with open('$results_file', 'r') as f:
        results = json.load(f)
except Exception as e:
    print(f"Error loading results: {e}", file=sys.stderr)
    sys.exit(1)

# Load baseline template
try:
    with open('$BASELINE_FILE', 'r') as f:
        baseline = json.load(f)
except:
    # Create minimal baseline structure if file doesn't exist
    baseline = {
        "version": "1.0",
        "description": "HFT Trading System Performance Baselines",
        "benchmarks": {},
        "global_settings": {
            "default_tolerance_percent": 15,
            "critical_failure_threshold_percent": 25
        }
    }

# Update metadata
baseline["last_updated"] = datetime.now().isoformat()
baseline["source_results"] = "$results_file"

# Extract and update benchmark metrics
benchmarks_data = results.get("benchmarks", {})

for benchmark_name, benchmark_data in benchmarks_data.items():
    if benchmark_data and "benchmarks" in benchmark_data:
        log(f"Processing {benchmark_name}")
        
        # Initialize benchmark in baseline if it doesn't exist
        if benchmark_name not in baseline["benchmarks"]:
            baseline["benchmarks"][benchmark_name] = {
                "description": f"{benchmark_name} performance metrics",
                "metrics": {}
            }
        
        # Extract common metrics based on benchmark content
        for bench in benchmark_data["benchmarks"]:
            name = bench.get("name", "").lower()
            
            # Extract timing metrics
            if "real_time" in bench:
                metric_name = name.split('/')[-1] if '/' in name else "latency_ns"
                baseline["benchmarks"][benchmark_name]["metrics"][metric_name] = {
                    "baseline": bench["real_time"],
                    "tolerance_percent": 15,
                    "critical": "latency" in metric_name or "time" in metric_name
                }
            
            # Extract throughput metrics
            if "items_per_second" in bench:
                metric_name = "throughput_ops_sec"
                baseline["benchmarks"][benchmark_name]["metrics"][metric_name] = {
                    "baseline": bench["items_per_second"],
                    "tolerance_percent": 10,
                    "critical": True
                }
            
            # Extract counter metrics
            if "counters" in bench:
                for counter_name, counter_value in bench["counters"].items():
                    value = counter_value.get("value", counter_value) if isinstance(counter_value, dict) else counter_value
                    baseline["benchmarks"][benchmark_name]["metrics"][counter_name.lower()] = {
                        "baseline": value,
                        "tolerance_percent": 15,
                        "critical": False
                    }

# Save updated baseline
try:
    with open('$BASELINE_FILE', 'w') as f:
        json.dump(baseline, f, indent=2)
    print(f"✅ Baseline created successfully: $BASELINE_FILE")
except Exception as e:
    print(f"Error saving baseline: {e}", file=sys.stderr)
    sys.exit(1)
EOF
    
    log "Baseline creation completed"
}

# Update existing baseline with new values
update_baseline() {
    local results_file="$1"
    
    if [[ ! -f "$results_file" ]]; then
        error "Results file not found: $results_file"
        return 1
    fi
    
    if [[ ! -f "$BASELINE_FILE" ]]; then
        error "Baseline file not found: $BASELINE_FILE"
        echo "Use 'create' command to create a new baseline first."
        return 1
    fi
    
    if ! confirm "Update baseline with new performance values from $results_file?"; then
        echo "Operation cancelled."
        return 1
    fi
    
    # Backup current baseline
    backup_baseline
    
    log "Updating baseline from $results_file"
    
    # Run comparison to see what would change
    echo "Performance changes that will be applied:"
    "${SCRIPT_DIR}/validate_performance.py" "$(dirname "$results_file")" --results "$results_file" || true
    
    if ! confirm "Proceed with baseline update?"; then
        echo "Operation cancelled."
        return 1
    fi
    
    # Update baseline (similar to create but preserves existing structure)
    python3 << EOF
import json
from datetime import datetime

# Load current baseline and results
with open('$BASELINE_FILE', 'r') as f:
    baseline = json.load(f)

with open('$results_file', 'r') as f:
    results = json.load(f)

# Update metadata
baseline["last_updated"] = datetime.now().isoformat()
baseline["source_results"] = "$results_file"

# Update metrics while preserving configuration
benchmarks_data = results.get("benchmarks", {})

for benchmark_name, benchmark_data in benchmarks_data.items():
    if benchmark_name in baseline["benchmarks"] and benchmark_data and "benchmarks" in benchmark_data:
        
        for bench in benchmark_data["benchmarks"]:
            # Update existing metrics
            for metric_name, metric_config in baseline["benchmarks"][benchmark_name]["metrics"].items():
                # Extract new value based on metric pattern
                if "real_time" in bench and "latency" in metric_name:
                    metric_config["baseline"] = bench["real_time"]
                elif "items_per_second" in bench and "throughput" in metric_name:
                    metric_config["baseline"] = bench["items_per_second"]
                elif "counters" in bench:
                    for counter_name, counter_value in bench["counters"].items():
                        if metric_name in counter_name.lower():
                            value = counter_value.get("value", counter_value) if isinstance(counter_value, dict) else counter_value
                            metric_config["baseline"] = value

# Save updated baseline
with open('$BASELINE_FILE', 'w') as f:
    json.dump(baseline, f, indent=2)

print(f"✅ Baseline updated successfully")
EOF
    
    log "Baseline update completed"
}

# Backup current baseline
backup_baseline() {
    local backup_dir="${BACKUP_DIR:-${PROJECT_ROOT}/baselines_backup}"
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local backup_file="${backup_dir}/performance_baselines_${timestamp}.json"
    
    mkdir -p "$backup_dir"
    
    if [[ -f "$BASELINE_FILE" ]]; then
        cp "$BASELINE_FILE" "$backup_file"
        log "Baseline backed up to: $backup_file"
        echo "Backup created: $backup_file"
    else
        log "No baseline file to backup"
    fi
}

# Restore baseline from backup
restore_baseline() {
    local backup_file="$1"
    
    if [[ ! -f "$backup_file" ]]; then
        error "Backup file not found: $backup_file"
        return 1
    fi
    
    if ! confirm "Restore baseline from $backup_file?"; then
        echo "Operation cancelled."
        return 1
    fi
    
    # Backup current baseline before restoring
    if [[ -f "$BASELINE_FILE" ]]; then
        backup_baseline
    fi
    
    cp "$backup_file" "$BASELINE_FILE"
    echo "✅ Baseline restored from: $backup_file"
}

# Validate baseline format
validate_baseline() {
    if [[ ! -f "$BASELINE_FILE" ]]; then
        error "Baseline file not found: $BASELINE_FILE"
        return 1
    fi
    
    python3 << EOF
import json
import sys

try:
    with open('$BASELINE_FILE', 'r') as f:
        baseline = json.load(f)
    
    # Check required structure
    required_keys = ["version", "benchmarks", "global_settings"]
    for key in required_keys:
        if key not in baseline:
            print(f"❌ Missing required key: {key}")
            sys.exit(1)
    
    # Validate benchmarks structure
    for bench_name, bench_data in baseline["benchmarks"].items():
        if "metrics" not in bench_data:
            print(f"❌ Missing metrics in benchmark: {bench_name}")
            sys.exit(1)
        
        for metric_name, metric_data in bench_data["metrics"].items():
            required_metric_keys = ["baseline", "tolerance_percent"]
            for key in required_metric_keys:
                if key not in metric_data:
                    print(f"❌ Missing {key} in {bench_name}.{metric_name}")
                    sys.exit(1)
    
    print("✅ Baseline file is valid")
    
except json.JSONDecodeError as e:
    print(f"❌ Invalid JSON: {e}")
    sys.exit(1)
except Exception as e:
    print(f"❌ Validation error: {e}")
    sys.exit(1)
EOF
}

# Show current baseline values
show_baseline() {
    if [[ ! -f "$BASELINE_FILE" ]]; then
        error "Baseline file not found: $BASELINE_FILE"
        return 1
    fi
    
    echo "Current Performance Baselines:"
    echo "=============================="
    
    python3 << EOF
import json

with open('$BASELINE_FILE', 'r') as f:
    baseline = json.load(f)

print(f"Version: {baseline.get('version', 'unknown')}")
print(f"Last updated: {baseline.get('last_updated', 'unknown')}")
print(f"Description: {baseline.get('description', 'none')}")
print()

for bench_name, bench_data in baseline["benchmarks"].items():
    print(f"{bench_name.upper()}:")
    print("-" * 40)
    
    for metric_name, metric_data in bench_data["metrics"].items():
        baseline_val = metric_data["baseline"]
        tolerance = metric_data["tolerance_percent"]
        critical = metric_data.get("critical", False)
        critical_marker = " [CRITICAL]" if critical else ""
        
        print(f"  {metric_name}: {baseline_val:.1f} (±{tolerance}%){critical_marker}")
    print()
EOF
}

# Compare results against baseline
compare_baseline() {
    local results_file="$1"
    
    if [[ ! -f "$results_file" ]]; then
        error "Results file not found: $results_file"
        return 1
    fi
    
    echo "Comparing results against current baseline..."
    "${SCRIPT_DIR}/validate_performance.py" "$(dirname "$results_file")" --results "$results_file" --show-passing
}

# Parse command line arguments
VERBOSE=0
FORCE=0
BACKUP_DIR=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -f|--force)
            FORCE=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -b|--backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        *)
            break
            ;;
    esac
done

# Check for command
if [[ $# -eq 0 ]]; then
    usage
    exit 1
fi

COMMAND="$1"
shift

# Execute command
case "$COMMAND" in
    create)
        if [[ $# -ne 1 ]]; then
            echo "Error: create command requires results file argument"
            usage
            exit 1
        fi
        create_baseline "$1"
        ;;
    update)
        if [[ $# -ne 1 ]]; then
            echo "Error: update command requires results file argument"
            usage
            exit 1
        fi
        update_baseline "$1"
        ;;
    backup)
        backup_baseline
        ;;
    restore)
        if [[ $# -ne 1 ]]; then
            echo "Error: restore command requires backup file argument"
            usage
            exit 1
        fi
        restore_baseline "$1"
        ;;
    validate)
        validate_baseline
        ;;
    show)
        show_baseline
        ;;
    compare)
        if [[ $# -ne 1 ]]; then
            echo "Error: compare command requires results file argument"
            usage
            exit 1
        fi
        compare_baseline "$1"
        ;;
    *)
        echo "Error: Unknown command: $COMMAND"
        usage
        exit 1
        ;;
esac