#!/usr/bin/env python3

"""
HFT Trading System - Performance Validation Script

Compares current benchmark results against established baselines
and reports pass/fail status with detailed analysis.
"""

import json
import sys
import argparse
import os
import statistics
from pathlib import Path
from typing import Dict, Any, List, Tuple
from dataclasses import dataclass
from enum import Enum

class ValidationResult(Enum):
    PASS = "PASS"
    WARNING = "WARNING" 
    FAIL = "FAIL"
    MISSING = "MISSING"
    ERROR = "ERROR"

@dataclass
class MetricResult:
    """Results for a single performance metric validation"""
    name: str
    baseline: float
    current: float
    tolerance_percent: float
    result: ValidationResult
    deviation_percent: float
    is_critical: bool
    
    def __str__(self) -> str:
        status_symbols = {
            ValidationResult.PASS: "✅",
            ValidationResult.WARNING: "⚠️",
            ValidationResult.FAIL: "❌",
            ValidationResult.MISSING: "❓",
            ValidationResult.ERROR: "💥"
        }
        
        symbol = status_symbols[self.result]
        critical_marker = " [CRITICAL]" if self.is_critical else ""
        
        return (f"{symbol} {self.name}: {self.current:.1f} "
                f"(baseline: {self.baseline:.1f}, "
                f"deviation: {self.deviation_percent:+.1f}%, "
                f"tolerance: ±{self.tolerance_percent:.1f}%){critical_marker}")

class PerformanceValidator:
    """Main performance validation engine"""
    
    def __init__(self, baseline_file: str, strict_mode: bool = False):
        self.baseline_file = baseline_file
        self.strict_mode = strict_mode
        self.baselines = self._load_baselines()
        
    def _load_baselines(self) -> Dict[str, Any]:
        """Load performance baselines from JSON file"""
        try:
            with open(self.baseline_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"❌ Error: Baseline file not found: {self.baseline_file}")
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"❌ Error: Invalid JSON in baseline file: {e}")
            sys.exit(1)
    
    def _extract_metric_value(self, benchmark_data: Dict[str, Any], metric_name: str) -> float:
        """Extract metric value from Google Benchmark JSON output"""
        if not benchmark_data or "benchmarks" not in benchmark_data:
            raise ValueError("Invalid benchmark data structure")
        
        benchmarks = benchmark_data["benchmarks"]
        
        # Handle different metric extraction patterns
        for bench in benchmarks:
            name = bench.get("name", "")
            
            # Direct metric match
            if metric_name in name.lower():
                return self._get_benchmark_value(bench)
            
            # Counter-based metrics
            if "counters" in bench:
                counters = bench["counters"]
                for counter_name, counter_data in counters.items():
                    if metric_name.replace("_", "").lower() in counter_name.replace("_", "").lower():
                        if isinstance(counter_data, dict):
                            return counter_data.get("value", counter_data.get("real_time", 0))
                        return float(counter_data)
        
        # Extract from benchmark name patterns
        for bench in benchmarks:
            if self._matches_metric_pattern(bench.get("name", ""), metric_name):
                return self._get_benchmark_value(bench)
        
        raise ValueError(f"Metric {metric_name} not found in benchmark data")
    
    def _get_benchmark_value(self, benchmark: Dict[str, Any]) -> float:
        """Extract the primary value from a benchmark result"""
        # Prefer real_time over cpu_time for latency measurements
        if "real_time" in benchmark:
            return float(benchmark["real_time"])
        elif "cpu_time" in benchmark:
            return float(benchmark["cpu_time"])
        elif "items_per_second" in benchmark:
            return float(benchmark["items_per_second"])
        elif "bytes_per_second" in benchmark:
            return float(benchmark["bytes_per_second"])
        else:
            raise ValueError("No recognized timing metric found in benchmark")
    
    def _matches_metric_pattern(self, benchmark_name: str, metric_name: str) -> bool:
        """Check if benchmark name matches metric pattern"""
        benchmark_lower = benchmark_name.lower()
        metric_lower = metric_name.lower()
        
        # Common patterns for HFT benchmarks
        patterns = [
            ("latency", "ns", "time"),
            ("throughput", "ops", "sec", "rate"),
            ("allocation", "alloc", "memory"),
            ("overhead", "cost"),
        ]
        
        for pattern in patterns:
            if any(p in metric_lower for p in pattern) and any(p in benchmark_lower for p in pattern):
                return True
        
        return False
    
    def validate_metric(self, benchmark_name: str, metric_name: str, 
                       benchmark_data: Dict[str, Any]) -> MetricResult:
        """Validate a single performance metric"""
        
        # Get baseline configuration
        baseline_config = self.baselines["benchmarks"].get(benchmark_name, {})
        metric_config = baseline_config.get("metrics", {}).get(metric_name, {})
        
        if not metric_config:
            return MetricResult(
                name=metric_name,
                baseline=0,
                current=0,
                tolerance_percent=0,
                result=ValidationResult.MISSING,
                deviation_percent=0,
                is_critical=False
            )
        
        baseline_value = metric_config["baseline"]
        tolerance_percent = metric_config.get("tolerance_percent", 
                                            self.baselines["global_settings"]["default_tolerance_percent"])
        is_critical = metric_config.get("critical", False)
        
        try:
            current_value = self._extract_metric_value(benchmark_data, metric_name)
        except Exception as e:
            print(f"⚠️  Warning: Could not extract {metric_name} from {benchmark_name}: {e}")
            return MetricResult(
                name=metric_name,
                baseline=baseline_value,
                current=0,
                tolerance_percent=tolerance_percent,
                result=ValidationResult.ERROR,
                deviation_percent=0,
                is_critical=is_critical
            )
        
        # Calculate deviation
        deviation_percent = ((current_value - baseline_value) / baseline_value) * 100
        
        # Determine result based on tolerance
        if abs(deviation_percent) <= tolerance_percent:
            result = ValidationResult.PASS
        elif abs(deviation_percent) <= tolerance_percent * 2:
            result = ValidationResult.WARNING if not self.strict_mode else ValidationResult.FAIL
        else:
            result = ValidationResult.FAIL
        
        return MetricResult(
            name=metric_name,
            baseline=baseline_value,
            current=current_value,
            tolerance_percent=tolerance_percent,
            result=result,
            deviation_percent=deviation_percent,
            is_critical=is_critical
        )
    
    def validate_benchmark(self, benchmark_name: str, 
                          benchmark_data: Dict[str, Any]) -> List[MetricResult]:
        """Validate all metrics for a specific benchmark"""
        results = []
        
        baseline_config = self.baselines["benchmarks"].get(benchmark_name, {})
        metrics = baseline_config.get("metrics", {})
        
        for metric_name in metrics.keys():
            result = self.validate_metric(benchmark_name, metric_name, benchmark_data)
            results.append(result)
        
        return results
    
    def validate_all(self, results_file: str) -> Tuple[List[MetricResult], Dict[str, int]]:
        """Validate all benchmarks and return comprehensive results"""
        
        # Load current results
        try:
            with open(results_file, 'r') as f:
                current_results = json.load(f)
        except FileNotFoundError:
            print(f"❌ Error: Results file not found: {results_file}")
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"❌ Error: Invalid JSON in results file: {e}")
            sys.exit(1)
        
        all_results = []
        summary = {result.value: 0 for result in ValidationResult}
        
        # Validate each benchmark
        benchmarks_data = current_results.get("benchmarks", {})
        
        for benchmark_name in self.baselines["benchmarks"].keys():
            benchmark_data = benchmarks_data.get(benchmark_name)
            
            if not benchmark_data:
                print(f"⚠️  Warning: No results found for {benchmark_name}")
                continue
            
            if benchmark_data is None or "error" in str(benchmark_data):
                print(f"❌ Error: Benchmark {benchmark_name} failed to run")
                continue
            
            benchmark_results = self.validate_benchmark(benchmark_name, benchmark_data)
            all_results.extend(benchmark_results)
        
        # Generate summary statistics
        for result in all_results:
            summary[result.result.value] += 1
        
        return all_results, summary

def print_detailed_results(results: List[MetricResult], show_passing: bool = True):
    """Print detailed validation results"""
    
    # Group by benchmark
    by_benchmark = {}
    for result in results:
        benchmark = result.name.split('_')[0] if '_' in result.name else "unknown"
        if benchmark not in by_benchmark:
            by_benchmark[benchmark] = []
        by_benchmark[benchmark].append(result)
    
    print("\nDetailed Performance Validation Results:")
    print("=" * 60)
    
    for benchmark_name, benchmark_results in by_benchmark.items():
        print(f"\n{benchmark_name.upper()}:")
        print("-" * 30)
        
        for result in benchmark_results:
            if show_passing or result.result != ValidationResult.PASS:
                print(f"  {result}")

def print_summary(summary: Dict[str, int], total_metrics: int):
    """Print validation summary"""
    print(f"\nPerformance Validation Summary:")
    print("=" * 40)
    print(f"Total metrics validated: {total_metrics}")
    print(f"✅ Passed: {summary['PASS']}")
    print(f"⚠️  Warnings: {summary['WARNING']}")
    print(f"❌ Failed: {summary['FAIL']}")
    print(f"❓ Missing: {summary['MISSING']}")
    print(f"💥 Errors: {summary['ERROR']}")
    
    pass_rate = (summary['PASS'] / max(total_metrics, 1)) * 100
    print(f"\nPass rate: {pass_rate:.1f}%")

def main():
    parser = argparse.ArgumentParser(description="Validate HFT benchmark performance")
    parser.add_argument("build_dir", nargs="?", help="Build directory containing benchmark results (default: auto-detect)")
    parser.add_argument("--strict", action="store_true", 
                       help="Strict mode: treat warnings as failures")
    parser.add_argument("--show-passing", action="store_true",
                       help="Show passing tests in detailed output")
    parser.add_argument("--baseline", default="performance_baselines.json",
                       help="Baseline configuration file")
    parser.add_argument("--results", help="Specific results file to validate")
    
    args = parser.parse_args()
    
    # Determine paths
    project_root = Path(__file__).parent.parent
    baseline_file = project_root / args.baseline
    
    # Auto-detect build directory if not provided
    if args.build_dir:
        build_dir = Path(args.build_dir)
    else:
        build_dir = project_root / "build"
    
    if args.results:
        results_file = Path(args.results)
    else:
        results_file = build_dir / "benchmark_results" / "latest_results.json"
    
    if not baseline_file.exists():
        print(f"❌ Error: Baseline file not found: {baseline_file}")
        sys.exit(1)
    
    if not results_file.exists():
        print(f"❌ Error: Results file not found: {results_file}")
        print("Run benchmarks first with: make benchmark")
        sys.exit(1)
    
    print(f"Validating performance against baselines...")
    print(f"Baseline file: {baseline_file}")
    print(f"Results file: {results_file}")
    print(f"Strict mode: {'ON' if args.strict else 'OFF'}")
    
    # Validate performance
    validator = PerformanceValidator(str(baseline_file), args.strict)
    results, summary = validator.validate_all(str(results_file))
    
    # Print results
    print_detailed_results(results, args.show_passing)
    print_summary(summary, len(results))
    
    # Determine exit code
    critical_failures = sum(1 for r in results if r.is_critical and r.result == ValidationResult.FAIL)
    total_failures = summary['FAIL']
    
    if critical_failures > 0:
        print(f"\n❌ CRITICAL FAILURE: {critical_failures} critical metrics failed")
        sys.exit(1)
    elif total_failures > 0:
        print(f"\n⚠️  WARNING: {total_failures} metrics failed (non-critical)")
        if args.strict:
            sys.exit(1)
        else:
            sys.exit(0)
    else:
        print(f"\n✅ SUCCESS: All performance metrics within acceptable ranges")
        sys.exit(0)

if __name__ == "__main__":
    main()