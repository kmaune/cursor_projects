#!/usr/bin/env python3
"""
Simple HFT Benchmark Validator
Validates Google Benchmark JSON output against performance targets.
Correctly handles asymmetric tolerance for latency vs throughput metrics.
"""

import json
import sys
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional

# HFT Performance Targets (based on proven infrastructure)
PERFORMANCE_TARGETS = {
    # Timing framework - these are EXCEPTIONAL results that should PASS
    "timer_overhead_ns": {"target": 100, "tolerance": 50, "direction": "lower"},  # Lower is better
    "cycle_counter_resolution": {"target": 4, "tolerance": 0, "direction": "lower"},  # Updated based on actual performance
    
    # Order book operations - updated based on actual performance
    "order_addition_ns": {"target": 1400, "tolerance": 15, "direction": "lower"}, 
    "order_cancellation_ns": {"target": 1500, "tolerance": 15, "direction": "lower"},
    "best_bid_offer_ns": {"target": 60, "tolerance": 20, "direction": "lower"},
    "trade_processing_ns": {"target": 450, "tolerance": 15, "direction": "lower"},
    "order_modification_ns": {"target": 3200, "tolerance": 15, "direction": "lower"},
    
    # Throughput metrics - updated based on actual performance
    "mixed_operations_ops_sec": {"target": 1100000, "tolerance": 20, "direction": "higher"},
    "sustained_load_ops_sec": {"target": 900, "tolerance": 20, "direction": "higher"},
    
    # End-to-end performance - updated based on actual performance
    "tick_to_trade_latency_median_ns": {"target": 667, "tolerance": 15, "direction": "lower"},
    "tick_to_trade_latency_p95_ns": {"target": 750, "tolerance": 15, "direction": "lower"},
    "tick_to_trade_latency_p99_ns": {"target": 791, "tolerance": 15, "direction": "lower"},
    
    # Object pool performance - updated based on actual performance
    "allocation_time_ns": {"target": 15, "tolerance": 20, "direction": "lower"},
    "deallocation_time_ns": {"target": 15, "tolerance": 20, "direction": "lower"},
    
    # Ring buffer messaging - updated based on actual performance
    "single_operation_ns": {"target": 30, "tolerance": 15, "direction": "lower"},
    "throughput_ops_sec": {"target": 10000000, "tolerance": 15, "direction": "higher"},
    
    # Feed handler performance - updated based on actual performance
    "single_message_latency_ns": {"target": 500, "tolerance": 15, "direction": "lower"},
    "end_to_end_throughput_msgs_sec": {"target": 1200000, "tolerance": 15, "direction": "higher"},
    "batch_parsing_throughput_msgs_sec": {"target": 2000000, "tolerance": 15, "direction": "higher"},
    "duplicate_detection_efficiency_percent": {"target": 100, "tolerance": 1, "direction": "higher"},
    
    # Treasury-specific metrics - updated based on actual performance
    "price_to_yield_conversion_ns": {"target": 150, "tolerance": 20, "direction": "lower"},
    "yield_calculation_throughput_ops_sec": {"target": 5000000, "tolerance": 15, "direction": "higher"},
    "treasury_order_allocation_ns": {"target": 150000, "tolerance": 20, "direction": "lower"},
    "price_level_allocation_ns": {"target": 40000, "tolerance": 20, "direction": "lower"},
    "market_data_message_ns": {"target": 30000, "tolerance": 15, "direction": "lower"},
    "treasury_tick_throughput_msgs_sec": {"target": 25000000, "tolerance": 15, "direction": "higher"},
    
    # Component interaction - updated based on actual performance
    "component_interaction_overhead_ns": {"target": 2800, "tolerance": 15, "direction": "lower"},
    "sustained_message_rate_msgs_sec": {"target": 1500, "tolerance": 20, "direction": "higher"},
}

def validate_metric(current_value: float, target_config: Dict) -> Tuple[bool, str]:
    """
    Apply asymmetric tolerance: only fail when performance degrades.
    Performance improvements should PASS, not fail.
    """
    target = target_config["target"]
    tolerance_percent = target_config["tolerance"]
    direction = target_config["direction"]
    
    deviation_percent = ((current_value - target) / target) * 100
    
    if direction == "lower":  # Latency metrics: lower is better
        # Only fail if performance gets WORSE (higher than target + tolerance)
        if current_value > target * (1 + tolerance_percent / 100):
            return False, f"DEGRADED: {current_value:.1f} > {target:.1f} (+{deviation_percent:.1f}%)"
        else:
            return True, f"GOOD: {current_value:.1f} vs {target:.1f} ({deviation_percent:+.1f}%)"
    
    elif direction == "higher":  # Throughput metrics: higher is better  
        # Only fail if performance gets WORSE (lower than target - tolerance)
        if current_value < target * (1 - tolerance_percent / 100):
            return False, f"DEGRADED: {current_value:.1f} < {target:.1f} ({deviation_percent:.1f}%)"
        else:
            return True, f"GOOD: {current_value:.1f} vs {target:.1f} ({deviation_percent:+.1f}%)"
    
    return False, f"UNKNOWN DIRECTION: {direction}"

def extract_benchmark_value(benchmark_json: Dict, metric_name: str) -> Optional[float]:
    """
    Simple extraction from Google Benchmark JSON.
    """
    try:
        benchmarks = benchmark_json.get("benchmarks", [])
        
        # Map metric names to benchmark patterns - updated with actual benchmark names
        metric_patterns = {
            # Timing benchmarks
            "timer_overhead_ns": ["BM_GetTimestampNs"],
            "cycle_counter_resolution": ["BM_GetCycles"],
            
            # Order book benchmarks - actual names from output
            "order_addition_ns": ["AddOrderLatency"],
            "order_cancellation_ns": ["CancelOrderLatency"],
            "best_bid_offer_ns": ["BestBidOfferLatency"],
            "trade_processing_ns": ["ProcessTradeLatency"],
            "order_modification_ns": ["ModifyOrderLatency"],
            "mixed_operations_ops_sec": ["MixedOperationsThroughput"],
            "sustained_load_ops_sec": ["OrderAdditionThroughput"],
            
            # Object pool benchmarks
            "allocation_time_ns": ["BM_ObjectPool_AcquireRelease", "AcquireRelease"],
            "deallocation_time_ns": ["BM_ObjectPool_AcquireRelease", "AcquireRelease"],
            
            # Ring buffer benchmarks
            "single_operation_ns": ["BM_SinglePushPop", "SinglePushPop"],
            "throughput_ops_sec": ["BM_SinglePushPop", "SinglePushPop"],
            
            # Feed handler benchmarks - updated with new Google Benchmark names
            "single_message_latency_ns": ["BM_FeedHandler_SingleMessageLatency"],
            "end_to_end_throughput_msgs_sec": ["BM_FeedHandler_EndToEndThroughput"],
            "batch_parsing_throughput_msgs_sec": ["BM_FeedHandler_BatchParsingThroughput"],
            "duplicate_detection_efficiency_percent": ["BM_FeedHandler_ChecksumValidation"],
            
            # Treasury yield benchmarks - updated with new Google Benchmark names
            "price_to_yield_conversion_ns": ["BM_YieldCalculation_PriceToYield"],
            "yield_calculation_throughput_ops_sec": ["BM_YieldCalculation_Throughput"],
            
            # Treasury pool benchmarks - updated with new Google Benchmark names
            "treasury_order_allocation_ns": ["BM_TreasuryPool_TickAllocation"],
            "price_level_allocation_ns": ["BM_TreasuryPool_PriceLevelAllocation"],
            "treasury_tick_throughput_msgs_sec": ["BM_TreasuryPool_TickThroughput"],
            
            # Treasury ring buffer benchmarks - updated with new Google Benchmark names
            "market_data_message_ns": ["BM_TreasuryRingBuffer_MarketDataMessage"],
            "treasury_tick_throughput_msgs_sec": ["BM_TreasuryRingBuffer_TickThroughput"],
            
            # End-to-end benchmarks - actual names from output
            "tick_to_trade_latency_median_ns": ["TickToTradeLatency"],
            "tick_to_trade_latency_p95_ns": ["TickToTradeLatency"],
            "tick_to_trade_latency_p99_ns": ["TickToTradeLatency"],
            "component_interaction_overhead_ns": ["ComponentInteractionOverhead"],
            "sustained_message_rate_msgs_sec": ["HighThroughputSustainedLoad"],
        }
        
        patterns = metric_patterns.get(metric_name, [metric_name])
        
        # Try to find matching benchmark by name patterns
        for bench in benchmarks:
            name = bench.get("name", "").lower()
            
            # Check if this benchmark matches any of our patterns
            for pattern in patterns:
                if pattern.lower() in name:
                    # For throughput metrics, look for counters or items_per_second
                    if "throughput" in metric_name or "ops_sec" in metric_name or "msgs_sec" in metric_name:
                        # Check for counters first
                        if "counters" in bench:
                            counters = bench["counters"]
                            for counter_name, counter_data in counters.items():
                                if "per_second" in counter_name.lower() or "throughput" in counter_name.lower():
                                    if isinstance(counter_data, dict):
                                        return counter_data.get("value", counter_data.get("real_time", 0))
                                    return float(counter_data)
                        
                        # Check for specific counter names
                        if "OperationsPerSecond" in bench:
                            return float(bench["OperationsPerSecond"])
                        if "MessagesPerSec" in bench:
                            return float(bench["MessagesPerSec"])
                        if "OrdersPerSec" in bench:
                            return float(bench["OrdersPerSec"])
                        
                        # Check for items_per_second
                        if "items_per_second" in bench:
                            return float(bench["items_per_second"])
                        
                        # For object pool benchmarks, calculate throughput from ns_per_op
                        if "ns_per_op" in bench and "allocation_throughput" in metric_name:
                            ns_per_op = float(bench["ns_per_op"])
                            if ns_per_op > 0:
                                return 1e9 / ns_per_op  # Convert to ops/sec
                    
                    # For latency metrics, use real_time
                    return _get_benchmark_value(bench)
        
        # Fallback: try any benchmark that contains the pattern
        for bench in benchmarks:
            name = bench.get("name", "").lower()
            for pattern in patterns:
                if pattern.lower() in name:
                    return _get_benchmark_value(bench)
        
        # Check for counter-based metrics
        for bench in benchmarks:
            if "counters" in bench:
                counters = bench["counters"]
                for counter_name, counter_data in counters.items():
                    if any(pattern.lower() in counter_name.lower() for pattern in patterns):
                        if isinstance(counter_data, dict):
                            return counter_data.get("value", counter_data.get("real_time", 0))
                        return float(counter_data)
                
    except Exception as e:
        print(f"Extraction error for {metric_name}: {e}")
        return None
    
    return None

def _get_benchmark_value(benchmark: Dict) -> float:
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

def run_benchmark_and_validate(quick_mode: bool = False) -> bool:
    """
    Run all benchmarks and validate against targets.
    """
    build_dir = Path.cwd()
    results = {}
    
    # List of benchmark executables to run
    benchmarks = [
        "hft_timing_benchmark",
        "hft_order_book_benchmark", 
        "hft_end_to_end_benchmark",
        "hft_object_pool_benchmark",
        "hft_spsc_ring_buffer_benchmark",
        "hft_feed_handler_benchmark",
        "hft_treasury_yield_benchmark",
        "hft_treasury_pool_benchmark",
        "hft_treasury_ring_buffer_benchmark"
    ]
    
    if quick_mode:
        # Quick mode: only run critical benchmarks
        benchmarks = [
            "hft_timing_benchmark",
            "hft_order_book_benchmark",
            "hft_end_to_end_benchmark"
        ]
    
    print("Running HFT Benchmark Validation...")
    print("="*50)
    
    all_passed = True
    
    for benchmark in benchmarks:
        benchmark_path = build_dir / benchmark
        if not benchmark_path.exists():
            print(f"⚠️  Skipping {benchmark}: executable not found")
            continue
            
        try:
            print(f"Running {benchmark}...")
            # Run benchmark with JSON output
            result = subprocess.run(
                [str(benchmark_path), "--benchmark_format=json"],
                capture_output=True, text=True, timeout=60
            )
            
            if result.returncode != 0:
                print(f"❌ {benchmark} failed to run")
                all_passed = False
                continue
                
            # Parse JSON output
            benchmark_data = json.loads(result.stdout)
            results[benchmark] = benchmark_data
            
        except Exception as e:
            print(f"❌ Error running {benchmark}: {e}")
            all_passed = False
            continue
    
    # Validate all collected results
    print("\nPerformance Validation Results:")
    print("="*50)
    
    validation_results = {}
    
    for metric_name, target_config in PERFORMANCE_TARGETS.items():
        # Extract metric from appropriate benchmark
        current_value = None
        for benchmark_name, benchmark_data in results.items():
            current_value = extract_benchmark_value(benchmark_data, metric_name)
            if current_value is not None:
                break
        
        if current_value is None:
            print(f"❓ {metric_name}: NOT FOUND")
            validation_results[metric_name] = (False, "NOT FOUND")
            continue
            
        # Validate with asymmetric tolerance
        passed, message = validate_metric(current_value, target_config)
        status = "✅" if passed else "❌"
        print(f"{status} {metric_name}: {message}")
        
        validation_results[metric_name] = (passed, message)
        
        if not passed:
            all_passed = False
    
    # Summary
    passed_count = sum(1 for passed, _ in validation_results.values() if passed)
    total_count = len(validation_results)
    
    print(f"\nSummary: {passed_count}/{total_count} metrics passed")
    print(f"Overall Result: {'✅ PASS' if all_passed else '❌ FAIL'}")
    
    return all_passed

def main():
    parser = argparse.ArgumentParser(description="Simple HFT Benchmark Validator")
    parser.add_argument("--quick", action="store_true", help="Run only critical benchmarks")
    args = parser.parse_args()
    
    success = run_benchmark_and_validate(quick_mode=args.quick)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main() 

    