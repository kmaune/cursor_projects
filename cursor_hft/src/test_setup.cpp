// Test file - delete after verification
#include <iostream>
#include <chrono>
#include <arm_neon.h>

// ARM64 cycle counter implementation
inline uint64_t read_cycle_counter() {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

int main() {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "HFT Trading System - Setup Complete" << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "Timing test: " << duration.count() << " ns" << std::endl;
    
    return 0;
}
