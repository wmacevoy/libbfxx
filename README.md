# libbf++

A modern C++17 wrapper around Fabrice Bellard's [libbf](https://bellard.org/libbf/) — a small, fast arbitrary precision floating point library.

Provides two main types:

- **`BF`** — binary floating point (base 2), with transcendental math functions
- **`BFDec`** — decimal floating point (base 10), exact for values like `0.1`

## Quick Example

```cpp
#include <iostream>
#include "bf.hpp"

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

    // Change precision on the fly
    bf_dprec(50);
    BFDec third = BFDec(1) / BFDec(3);
    std::cout << "1/3 (50 digits) = " << third << std::endl;
}
```

## Building

```bash
cmake -B build -S .
cmake --build build
./build/round
```

Requires CMake 3.16+ and a C++17 compiler.

## API Overview

### Context

All operations use an implicit thread-local context managed via the `retain`/`recall` pattern:

```cpp
BFContext context(prec, dprec, rnd);
retain<BFContext> use(&context);
```

Free functions to query/adjust the active context:

| Function | Description |
|---|---|
| `bf_prec()` / `bf_prec(p)` | Get/set binary precision (bits) |
| `bf_dprec()` / `bf_dprec(p)` | Get/set decimal precision (digits) |
| `bf_rnd()` / `bf_rnd(mode)` | Get/set rounding mode |

Rounding modes: `BF_RNDN` (nearest, ties to even), `BF_RNDZ` (toward zero), `BF_RNDD` (down), `BF_RNDU` (up), `BF_RNDA` (away from zero), `BF_RNDF` (faithful).

### BF (binary)

```cpp
BF a(42);                     // from int
BF b(3.14);                   // from double
BF c("123.456", 10);          // from string (radix 2–36)
BF d = sqrt(a);               // math functions
double v = a.to_double();     // convert out
std::string s = a.to_string();
```

**Arithmetic:** `+`, `-`, `*`, `/` (and `+=`, `-=`, `*=`, `/=`)
**Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=`
**Math:** `sqrt`, `exp`, `log`, `pow`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `abs`, `pi`, `ln2`
**Queries:** `is_nan()`, `is_zero()`, `is_finite()`, `is_inf()`

### BFDec (decimal)

```cpp
BFDec a("19.99");             // exact decimal
BFDec b(42);                  // from int
BFDec c = a + b;
```

Same operators and queries as `BF`. Math functions: `sqrt`, `abs`, `pow` (integer exponent).

### Conversions

```cpp
BFDec d("0.1");
BF b(d);          // decimal → binary
BFDec d2(b);      // binary → decimal
```

## Project Structure

```
include/bf.hpp           C++ header (single-file API)
src/round.cpp            Example program
vendor/libbf/            Bellard's libbf (vendored from bellard.org/libbf/)
vendor/retain-recall/    Thread-local context management
doc/                     API reference for libbf and libbf++
```

## License

libbf is Copyright (c) 2017-2021 Fabrice Bellard — see `vendor/libbf/` for license details. The C++ wrapper is provided under the same terms.
