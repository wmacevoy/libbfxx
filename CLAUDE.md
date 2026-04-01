# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
cmake -B build -S .          # Configure
cmake --build build           # Build all targets
cmake --build build --clean-first  # Clean rebuild
```

## Running Tests

```bash
./build/test_bf               # BF (binary float) tests
./build/test_bf_dec           # BFDec (decimal float) tests
./build/test_numeric          # numeric layer tests
```

There is no combined test runner. Run each executable individually. All tests use the vendored `facts` framework:

```cpp
FACTS(TestName) {
  FACT(expression, ==, expected);
}
```

## What This Is

A C++17 wrapper (`namespace bellard`) around Fabrice Bellard's libbf arbitrary-precision floating point library. This is the numeric subset of the [QJSON](../qjson/) project — it defines the numeric type semantics, bracket projection, and QJSON numeric I/O format. Header-only in `include/`, with vendored C dependencies in `vendor/`.

## Architecture

**QJSON type system** (`numeric.hpp`):

Bitmask-based `enum qjson_type : uint16_t` encodes type identity and group membership in the bit pattern:

| Type | Hex | Group bit |
|------|-----|-----------|
| `QJSON_NULL` | `0x000` | — |
| `QJSON_FALSE` | `0x010` | bit 4 = BOOLEAN |
| `QJSON_TRUE` | `0x011` | bit 4 = BOOLEAN |
| `QJSON_NUMBER` | `0x021` | bit 5 = NUMERIC |
| `QJSON_BIGINT` | `0x022` | bit 5 = NUMERIC |
| `QJSON_BIGFLOAT` | `0x024` | bit 5 = NUMERIC |
| `QJSON_BIGDECIMAL` | `0x028` | bit 5 = NUMERIC |
| `QJSON_BLOB` | `0x040` | bit 6 |
| `QJSON_STRING` | `0x081` | bit 7 |
| `QJSON_ARRAY` | `0x101` | bit 8 = CONTAINER |
| `QJSON_OBJECT` | `0x102` | bit 8 = CONTAINER |
| `QJSON_UNBOUND` | `0x1FF` | all bits (matches everything) |

Group tests: `type & QJSON_BOOLEAN`, `type & QJSON_NUMERIC`, `type & QJSON_CONTAINER`. Boolean truth value: `type & 0x01`. UNBOUND has all bits set so `UNBOUND & any_type == any_type`.

Type names follow QuickJS `typeof` conventions: `number`, `bigint`, `bigdecimal`, `bigfloat`.

**Core type hierarchy** (each in its own header under `include/`):

- `BF` (`bf.hpp`) — Binary floating point (base 2), supports transcendental math (`sin`, `exp`, `log`, etc.). Also backs `BIGINT` (integer stored as BF with infinite precision, matching QuickJS).
- `BFDec` (`bf.hpp`) — Decimal floating point (base 10), exact for values like 0.1.
- `unbound` (`unbound.hpp`) — Symbolic variable for constraint satisfaction. `?` (anonymous) satisfies all comparisons; `?x` (named) has identity semantics.
- `numeric` (`numeric.hpp`) — Bracket projection: packs values into `[lo, hi]` double interval for fast comparison, with lazy full-precision fallback when intervals overlap. Uses `qjson_type` for type tagging.
- `NumericVariant` (`numeric.hpp`) — `std::variant<BF, BFDec, double, unbound>` with cross-type comparison via widening.

**Context system** (`bf.hpp`):

Precision and rounding are managed through thread-local contexts using the vendored `retain-recall` library:

```cpp
BFContext ctx(128, BF_RNDN);  // 128-bit precision, round-to-nearest
retain<BFContext> use(&ctx);   // push onto thread-local stack
// BF operations now use this context
// context pops when `use` goes out of scope
```

`BFAlloc` is ref-counted, so BF/BFDec values remain valid after their creating context is destroyed.

**Type widening** (`numeric.hpp`):

Cross-type comparisons widen both operands to a common type: `double+BF` -> `BF`, `double+BFDec` -> `BFDec`, `BF+BFDec` -> `BFDec`. Any type compared with `unbound` short-circuits to `true`.

**QJSON numeric I/O** (`numeric.hpp`):

Compact text format for typed numeric literals with round-trip fidelity:
- `42` — NUMBER (IEEE 754 double)
- `42N` — BIGINT (arbitrary-precision integer)
- `67432.50M` — BIGDECIMAL (exact base-10)
- `3.14L` — BIGFLOAT (arbitrary-precision binary float)
- `?x` — UNBOUND variable

Lowercase suffixes accepted on input, uppercase on output.

## Key Conventions

- All headers use `#pragma once`
- Everything is in `namespace bellard`
- The vendored `libbf/` include directory is kept PRIVATE in CMake to avoid its `version` file shadowing the C++ `<version>` header
- C++17 required (uses `std::variant`, `std::optional`, structured bindings)
