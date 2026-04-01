# libbf++

A modern C++17 wrapper around Fabrice Bellard's [libbf](https://bellard.org/libbf/) -- a small, fast arbitrary precision floating point library. This is the numeric subset of the [QJSON](https://github.com/wmacevoy/qjson) project -- it defines the numeric type semantics, bracket projection, and QJSON numeric I/O. Everything lives in `namespace bellard`.

## Types

| Type | Description |
|---|---|
| `BF` | Binary floating point (base 2), with transcendental math functions |
| `BFDec` | Decimal floating point (base 10), exact for values like `0.1` |
| `unbound` | Symbolic variable for constraint satisfaction (universal or named) |
| `numeric` | Bracket projection: packs any of the above into a `[lo, hi]` double interval with lazy full-precision fallback |
| `NumericVariant` | `std::variant<BF, BFDec, double, unbound>` with cross-type comparison via widening |

## Quick Example

```cpp
#include <iostream>
#include "numeric.hpp"

using namespace bellard;

int main() {
    BFContext context(256, 34);   // 256-bit binary, 34-digit decimal
    retain<BFContext> use(&context);

    // Binary float: 0.1 is not exactly representable
    BF bf_tenth = BF(1) / BF(10);
    std::cout << "BF  1/10 = " << bf_tenth << std::endl;

    // Decimal float: 0.1 is exact
    BFDec bd_tenth("0.1");
    std::cout << "Dec 0.1  = " << bd_tenth << std::endl;

    // Decimal arithmetic: no binary rounding surprises
    BFDec price("19.99");
    BFDec tax("0.0725");
    BFDec total = price + price * tax;
    std::cout << "$19.99 + 7.25% tax = $" << total << std::endl;

    // QJSON numeric I/O
    numeric n(BFDec("0.1"));
    std::cout << n << std::endl;   // "0.1M"
}
```

## Building

```bash
cmake -B build -S .
cmake --build build
```

Requires CMake 3.16+ and a C++17 compiler.

## Running Tests

```bash
./build/test_bf          # BF constructors, arithmetic, math, precision, int boundaries, extreme values
./build/test_bf_dec      # BFDec constructors, exactness, arithmetic, int boundaries, powers of 2, edge cases
./build/test_numeric     # unbound, variant, numeric bracket, QJSON I/O, round-trip
```

## Architecture

### Context and Memory Management

All operations use an implicit thread-local context managed via the `retain`/`recall` pattern:

```cpp
BFContext context(prec, dprec, rnd);
retain<BFContext> use(&context);
```

The allocator (`BFAlloc`) is ref-counted via `shared_ptr`. Each `BF`/`BFDec` captures a `shared_ptr<BFAlloc>` at creation, so values remain valid even after their creating context goes out of scope. Contexts can nest:

```cpp
BFContext outer(128, 34);
retain<BFContext> r1(&outer);      // bf_prec() == 128
{
    BFContext inner(1024, 100);
    retain<BFContext> r2(&inner);  // bf_prec() == 1024
}
// bf_prec() == 128 again
```

Different contexts can coexist -- libbf operations only require the result's allocator to be alive; operands can come from any context.

| Function | Description |
|---|---|
| `bf_prec()` / `bf_prec(p)` | Get/set binary precision (bits) |
| `bf_dprec()` / `bf_dprec(p)` | Get/set decimal precision (digits) |
| `bf_rnd()` / `bf_rnd(mode)` | Get/set rounding mode |
| `bf_alloc()` | Get current context's `shared_ptr<BFAlloc>` |

Rounding modes: `BF_RNDN` (nearest, ties to even), `BF_RNDZ` (toward zero), `BF_RNDD` (down), `BF_RNDU` (up), `BF_RNDA` (away from zero), `BF_RNDF` (faithful).

### BF (binary floating point)

```cpp
BF a(42);                     // from int
BF b(3.14);                   // from double
BF c("123.456", 10);          // from string (radix 2-36)
BF d = sqrt(a);               // math functions
double v = a.to_double();     // convert out
std::string s = a.to_string();
limb_t p = a.precision();     // actual bits carried
```

**Arithmetic:** `+`, `-`, `*`, `/` (and `+=`, `-=`, `*=`, `/=`)
**Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=`
**Math:** `sqrt`, `exp`, `log`, `pow`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `abs`, `pi`, `ln2`
**Queries:** `is_nan()`, `is_zero()`, `is_finite()`, `is_inf()`

### BFDec (decimal floating point)

```cpp
BFDec a("19.99");             // exact decimal
BFDec b(42);                  // from int
BFDec c(0.5);                 // from double (via BF)
BFDec d = a + b;
```

Same operators and queries as `BF`. Math: `sqrt`, `abs`, `pow` (integer exponent).

### unbound (symbolic variable)

```cpp
unbound()          // universal -- satisfies all comparisons
unbound("x")       // named -- x == x, but x < y for different names
```

All comparisons between unbound and any concrete type return `true` (constraint satisfaction semantics). Same-named unbounds behave like identity: `x < x` is false, `x == x` is true.

### QJSON type system

Bitmask-based `enum qjson_type : uint16_t` encodes type identity and group membership in the bit pattern. Type names follow QuickJS `typeof` conventions.

| Type | Suffix | Hex | Group |
|------|--------|-----|-------|
| `QJSON_NUMBER` | (none) | `0x021` | NUMERIC |
| `QJSON_BIGINT` | `N` | `0x022` | NUMERIC |
| `QJSON_BIGFLOAT` | `L` | `0x024` | NUMERIC |
| `QJSON_BIGDECIMAL` | `M` | `0x028` | NUMERIC |
| `QJSON_UNBOUND` | `?` | `0x1FF` | all bits |

Group tests: `type & QJSON_NUMERIC`, `type & QJSON_BOOLEAN`, `type & QJSON_CONTAINER`. The full enum also includes `NULL`, `FALSE`, `TRUE`, `BLOB`, `STRING`, `ARRAY`, `OBJECT` for forward compatibility with the QJSON project.

### numeric (bracket projection)

Packs a value into a double interval `[lo, hi]` for fast comparison. Falls back to full-precision string reconstruction only when brackets overlap.

```cpp
numeric n(BFDec("0.1"));      // BIGDECIMAL, lo < hi, rep = "0.1"
numeric m(0.5);                // NUMBER, lo == hi == 0.5
numeric u(unbound("x"));      // UNBOUND, lo = -inf, hi = +inf

std::cout << n;                // "0.1M"
std::cin >> n;                 // parses QJSON numeric syntax
```

**QJSON I/O format:**
- `42` -- NUMBER (IEEE 754 double)
- `42N` / `42n` -- BIGINT (arbitrary-precision integer, stored as BF)
- `67432.50M` / `67432.50m` -- BIGDECIMAL (exact base-10)
- `3.14L` / `3.14l` -- BIGFLOAT (arbitrary-precision binary float)
- `?` -- anonymous unbound
- `?x` -- named unbound (bare identifier)
- `?"Bob's Memo"` -- quoted unbound

### Cross-type comparison (widening)

When comparing different numeric types, `numeric_op` widens both operands to a common type:

| A | B | Widest |
|---|---|---|
| `double` | `BF` | `BF` |
| `double` | `BFDec` | `BFDec` |
| `BF` | `BFDec` | `BFDec` |
| anything | `unbound` | (short-circuits to `true`) |

The `widen<BFDec, BF>` specialization temporarily bumps decimal precision to `BF::precision()` bits (since `1/2^n` needs `n` decimal digits for exact representation).

## Project Structure

```
include/
  bf.hpp              BFAlloc, BFContext, BF, BFDec, math functions
  unbound.hpp         unbound type and comparison operators
  numeric.hpp         qjson_type enum, NumericWiden, widen, numeric_op, NumericVariant, numeric

src/
  round.cpp           BF/BFDec example
  numeric.cpp         numeric sorting example
  test_bf.cpp         BF tests
  test_bf_dec.cpp     BFDec tests
  test_numeric.cpp    numeric/unbound/variant tests

vendor/
  libbf/              Bellard's libbf (vendored from bellard.org/libbf/)
  retain-recall/      Thread-local context management
  facts/              Test framework

doc/
  libbf.md            libbf C API reference
  libbf++.md          libbf++ C++ API reference
```

## License

MIT License. Both the C++ wrapper and the vendored [libbf](https://bellard.org/libbf/) library (Copyright (c) 2017-2025 Fabrice Bellard) are MIT-licensed.
