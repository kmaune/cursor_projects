Create a high-performance lock-free messaging system for our HFT trading system. This will be the communication backbone between all components (market data, order book, strategies, risk management).

Requirements:
- SPSC (Single Producer Single Consumer) and MPSC (Multi Producer Single Consumer) ring buffers
- Zero-copy message passing with template-based type safety
- ARM64 cache-line optimization (64-byte alignment)
- Deterministic memory ordering and no dynamic allocation
- Nanosecond-precision message timestamping using our HFTTimer
- Batch processing capabilities for high throughput
- Comprehensive benchmarks measuring latency distributions

Key Components:
1. RingBuffer template class with configurable capacity
2. MessageDispatcher for routing typed messages between components  
3. EventLoop for deterministic message processing
4. Message base class with timing integration
5. Lock-free queue implementations (SPSC/MPSC variants)

The system should support:
- Market data messages (ticks, orders, trades)
- Trading messages (new orders, cancellations, fills)
- System messages (heartbeats, status updates)
- Priority-based message handling
- Message batching for throughput optimization
- Real-time latency monitoring integration

Generate complete implementation with comprehensive unit tests, benchmarks, and CMake integration.
