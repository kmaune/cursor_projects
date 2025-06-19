# CRITICAL BUG: Memory Layout Mismatch in Trade Parsing

**Issue Identified:** The trade size corruption shows the parser is reading price data instead of size data.

**Root Cause:** Memory offset mismatch between test data generation and parsing logic.

## Test Data Layout (in `make_raw_msg`):
```cpp
// Test puts data like this:
std::memcpy(msg.raw_data, &price_val, sizeof(double));        // 0-7: price
std::memcpy(msg.raw_data + 8, &price_val, sizeof(double));    // 8-15: DUPLICATE PRICE (BUG!)
std::memcpy(msg.raw_data + 16, &size, sizeof(uint64_t));      // 16-23: size
std::memcpy(msg.raw_data + 24, &size, sizeof(uint64_t));      // 24-31: size
```

## Parser Expectation (in `parse_trade`):
```cpp
// Parser expects:
// 0-7: trade_price
// 8-15: trade_size  ← READING DUPLICATE PRICE INSTEAD!
// 16-31: trade_id
```

## THE BUG:
Test helper `make_raw_msg` puts **duplicate price** at offset 8, but parser expects **size** at offset 8.

## Fix Required:
**Option 1:** Fix test data generation in `make_raw_msg` for trade messages:
```cpp
// For trade messages, layout should be:
std::memcpy(msg.raw_data, &price_val, sizeof(double));     // 0-7: price  
std::memcpy(msg.raw_data + 8, &size, sizeof(uint64_t));    // 8-15: size
std::memcpy(msg.raw_data + 16, trade_id, 16);              // 16-31: trade_id
```

**Option 2:** Fix parser to match test layout:
```cpp
// In parse_trade, change offset:
std::memcpy(&out.trade_size, raw.raw_data + 16, sizeof(uint64_t));  // Read from offset 16
```

## Sequence Gap Issue:
The gap logic is still overcounting by 1. Debug the initial expected_sequence_ state.

**Choose one fix approach and apply it consistently between test data generation and parsing logic.**
