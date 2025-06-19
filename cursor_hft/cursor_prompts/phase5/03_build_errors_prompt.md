# Critical Feed Handler Compilation Fixes Needed

The feed handler has several compilation errors that need immediate fixing in `include/hft/market_data/feed_handler.hpp`:

## Error 1: Forward Declaration Issue
**Problem:** `MessageNormalizer` is used before it's declared in the `MessageParser` class.

**Fix:** Move the `MessageNormalizer` class definition BEFORE the `MessageParser` class, or use forward declarations properly.

## Error 2: Namespace Issues in Test File
**Problem:** Test file references `HFTTimer::ns_t` but needs `hft::HFTTimer::ns_t`

**Fix:** Add proper namespace qualification or using declarations.

## Error 3: Template Iterator Compilation Error
**Problem:** The `parse_batch` function has auto type deduction issues:
```cpp
for (auto it = raw_begin, out = parsed_begin; ...)
```

**Fix:** Use separate auto declarations:
```cpp
for (auto it = raw_begin, out = parsed_begin; it != raw_end && out != parsed_end; ++it, ++out) {
```
Should be:
```cpp
auto it = raw_begin;
auto out = parsed_begin;
for (; it != raw_end && out != parsed_end; ++it, ++out) {
```

## Error 4: Prefetch Pointer Conversion
**Problem:** `__builtin_prefetch((it + 1), 0, 3)` fails because iterator can't convert to void*.

**Fix:** Dereference the iterator:
```cpp
if ((it + 1) != raw_end) __builtin_prefetch(&(*(it + 1)), 0, 3);
```

## Error 5: Missing Using Declarations
**Problem:** Code uses `HFTTimer` without proper namespace qualification.

**Fix:** Add at the top of the file:
```cpp
using hft::HFTTimer;
using hft::LatencyHistogram;
```

## Specific Fixes Required:

1. **Reorder class definitions:** Put `MessageNormalizer` before `MessageParser`
2. **Fix template function:** Separate iterator declarations in `parse_batch`
3. **Fix prefetch calls:** Proper pointer dereferencing
4. **Add namespace qualifications:** Use proper `hft::` prefixes or using declarations
5. **Fix test file:** Add proper namespace qualifications for HFTTimer

## Performance Critical:
Maintain all existing performance optimizations while fixing these compilation issues. The fixes should not impact the <200ns parsing target or >1M messages/second throughput.

Please fix all compilation errors while preserving the existing functionality and performance characteristics.
