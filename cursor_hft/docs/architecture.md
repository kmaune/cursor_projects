# HFT System Architecture

## Performance Targets
- Tick-to-trade: 5-15μs
- Order book update: 200-500ns
- Market data processing: 1-3μs

## Key Components
1. Timing Framework (Phase 1)
2. Lock-free Messaging (Phase 1)
3. Memory Management (Phase 1)
4. Order Book (Phase 1)

## Hardware: Apple M1 Pro
- ARM64 architecture
- Unified memory
- 64-byte cache lines
- Virtual cycle counter (cntvct_el0)
