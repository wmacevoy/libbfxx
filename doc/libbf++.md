# libbf++ -- C++ Wrapper for libbf

Header-only C++ wrapper around Fabrice Bellard's libbf arbitrary precision
floating point library.  Provides RAII lifetime management, operator
overloads, and implicit context via the retain/recall pattern.

```
#include "bf.hpp"    // single header, includes libbf.h and retain.hpp
```

## Quick start

```cpp
#include <iostream>
#include "bf.hpp"

int main() {
    BFContext context(256, 34);   // 256 binary bits, 34 decimal digits
    retain<BFContext> use(&context);

    // Binary float (base 2)
    BF a = BF(1) / BF(3);
    std::cout << a << std::endl;
    // 0.333333333333...  (256 bits of precision)

    // Decimal float (base 10)
    BFDec price("19.99");
    BFDec tax("0.0725");
    std::cout << price + price * tax << std::endl;
    // 21.439275  (exact in decimal)

    // Round to double with controlled rounding
    bf_rnd(BF_RNDD);
    double lo = a.to_double();   // rounds toward -inf
    bf_rnd(BF_RNDU);
    double hi = a.to_double();   // rounds toward +inf
}
```

## Context

### `BFContext`

Owns the libbf allocation context, binary precision, decimal precision, and
flags (rounding mode + exponent range).

```cpp
BFContext(limb_t prec = 128, limb_t dprec = 34, bf_rnd_t rnd = BF_RNDN);
```

| Parameter | Meaning                                       | Default    |
|-----------|-----------------------------------------------|------------|
| `prec`    | Binary precision in bits (used by `BF`)       | 128        |
| `dprec`   | Decimal precision in digits (used by `BFDec`) | 34         |
| `rnd`     | IEEE rounding mode                            | `BF_RNDN`  |

`BFContext` is non-copyable.  Make it the active context with retain:

```cpp
BFContext context(512, 50, BF_RNDN);
retain<BFContext> use(&context);
```

Retain has stack semantics -- nested retains shadow outer ones and restore
on destruction:

```cpp
BFContext outer(128);
retain<BFContext> r1(&outer);
// bf_prec() == 128

{
    BFContext inner(1024);
    retain<BFContext> r2(&inner);
    // bf_prec() == 1024
}
// bf_prec() == 128 again
```

### Free-function accessors

These read/write the currently retained `BFContext` via `recall<BFContext>()`.

| Getter         | Setter            | Controls                    |
|----------------|-------------------|-----------------------------|
| `bf_prec()`    | `bf_prec(p)`      | Binary precision (bits)     |
| `bf_dprec()`   | `bf_dprec(p)`     | Decimal precision (digits)  |
| `bf_rnd()`     | `bf_rnd(mode)`    | Rounding mode               |
| `bf_flags()`   | --                | Raw flags (rnd + exp bits)  |

```cpp
bf_prec(512);          // change binary precision on the fly
bf_rnd(BF_RNDD);      // switch to round-toward-minus-infinity
```

### Rounding modes

| Constant   | Meaning                             |
|------------|-------------------------------------|
| `BF_RNDN`  | Round to nearest, ties to even      |
| `BF_RNDZ`  | Round toward zero                   |
| `BF_RNDD`  | Round toward -infinity (floor)      |
| `BF_RNDU`  | Round toward +infinity (ceil)       |
| `BF_RNDNA` | Round to nearest, ties away from 0  |
| `BF_RNDA`  | Round away from zero                |
| `BF_RNDF`  | Faithful rounding (nondeterministic)|

## `BF` -- Binary floating point (base 2)

Wraps `bf_t`.  All arithmetic uses `bf_prec()` bits and `bf_flags()`.

### Constructors

| Constructor                       | Source                                  |
|-----------------------------------|-----------------------------------------|
| `BF()`                            | Zero                                    |
| `BF(int v)`                       | Integer                                 |
| `BF(int64_t v)`                   | Signed 64-bit integer                   |
| `BF(uint64_t v)`                  | Unsigned 64-bit integer                 |
| `BF(double v)`                    | IEEE 754 double (exact representation)  |
| `BF(const char *s, int radix=10)` | Parse string in given base (2-36)       |
| `BF(const std::string &s, int radix=10)` | Same, from std::string           |
| `BF(const BFDec &d)`              | Convert from decimal float              |
| `BF(const BF &other)`             | Copy                                    |
| `BF(BF &&other)`                  | Move                                    |

### Assignment

```cpp
BF &operator=(const BF &);
BF &operator=(BF &&);
BF &operator=(int64_t);
BF &operator=(double);
```

### Arithmetic

```cpp
BF operator+(const BF &) const;    BF &operator+=(const BF &);
BF operator-(const BF &) const;    BF &operator-=(const BF &);
BF operator*(const BF &) const;    BF &operator*=(const BF &);
BF operator/(const BF &) const;    BF &operator/=(const BF &);
BF operator-() const;              // unary negation
```

### Comparison

```cpp
bool operator==(const BF &) const;
bool operator!=(const BF &) const;
bool operator<(const BF &) const;
bool operator<=(const BF &) const;
bool operator>(const BF &) const;
bool operator>=(const BF &) const;
```

### State queries

```cpp
bool is_nan() const;
bool is_zero() const;
bool is_finite() const;
bool is_inf() const;
```

### Conversions

```cpp
double      to_double() const;            // uses bf_rnd()
double      to_double(bf_rnd_t) const;    // explicit rounding mode
int64_t     to_int64() const;
std::string to_string(int radix=10, limb_t digits=0) const;
```

`to_string()` with `digits=0` uses `BF_FTOA_FORMAT_FREE` (minimum digits for
round-trip fidelity at the current precision).  With `digits > 0` it uses
`BF_FTOA_FORMAT_FIXED` (that many significant digits).

### Stream output

```cpp
std::cout << my_bf;    // calls to_string()
```

### C escape hatch

```cpp
const bf_t *c_bf() const;
bf_t       *c_bf();
```

### Math functions

All use `bf_prec()` and `bf_flags()` from the retained context.

```cpp
BF sqrt(const BF &a);
BF exp(const BF &a);
BF log(const BF &a);
BF pow(const BF &a, const BF &b);
BF sin(const BF &a);
BF cos(const BF &a);
BF tan(const BF &a);
BF atan(const BF &a);
BF atan2(const BF &y, const BF &x);
BF asin(const BF &a);
BF acos(const BF &a);
BF abs(const BF &a);
BF pi();       // pi constant
BF ln2();      // ln(2) constant
```

## `BFDec` -- Decimal floating point (base 10)

Wraps `bfdec_t`.  All arithmetic uses `bf_dprec()` digits and `bf_flags()`.

Decimal floats represent values exactly in base 10.  `BFDec("0.1")` is exact,
while `BF(1)/BF(10)` has binary rounding noise.  Use `BFDec` for financial
arithmetic or any domain where base-10 exactness matters.

### Constructors

| Constructor                | Source                                  |
|----------------------------|-----------------------------------------|
| `BFDec()`                  | Zero                                    |
| `BFDec(int v)`             | Integer                                 |
| `BFDec(int64_t v)`         | Signed 64-bit integer                   |
| `BFDec(uint64_t v)`        | Unsigned 64-bit integer                 |
| `BFDec(const char *s)`     | Parse decimal string                    |
| `BFDec(const std::string &s)` | Same, from std::string               |
| `BFDec(const BF &b)`       | Convert from binary float               |
| `BFDec(const BFDec &other)`| Copy                                    |
| `BFDec(BFDec &&other)`     | Move                                    |

No `double` constructor -- use `BFDec(BF(d))` for explicit conversion path.

### Assignment

```cpp
BFDec &operator=(const BFDec &);
BFDec &operator=(BFDec &&);
BFDec &operator=(int64_t);
```

### Arithmetic

```cpp
BFDec operator+(const BFDec &) const;    BFDec &operator+=(const BFDec &);
BFDec operator-(const BFDec &) const;    BFDec &operator-=(const BFDec &);
BFDec operator*(const BFDec &) const;    BFDec &operator*=(const BFDec &);
BFDec operator/(const BFDec &) const;    BFDec &operator/=(const BFDec &);
BFDec operator-() const;                // unary negation
```

### Comparison

```cpp
bool operator==(const BFDec &) const;
bool operator!=(const BFDec &) const;
bool operator<(const BFDec &) const;
bool operator<=(const BFDec &) const;
bool operator>(const BFDec &) const;
bool operator>=(const BFDec &) const;
```

### State queries

```cpp
bool is_nan() const;
bool is_zero() const;
bool is_finite() const;
bool is_inf() const;
```

### Conversions

```cpp
double      to_double() const;           // converts via BF, uses bf_rnd()
double      to_double(bf_rnd_t) const;   // explicit rounding mode
int64_t     to_int64() const;
std::string to_string(limb_t digits=0) const;
```

`to_string()` with `digits=0` uses `BF_FTOA_FORMAT_FREE`.  With `digits > 0`
uses `BF_FTOA_FORMAT_FIXED`.

### Stream output

```cpp
std::cout << my_bfdec;    // calls to_string()
```

### C escape hatch

```cpp
const bfdec_t *c_bfdec() const;
bfdec_t       *c_bfdec();
```

### Math functions

`BFDec` has no transcendental functions.  Convert to `BF` first for those.

```cpp
BFDec sqrt(const BFDec &a);
BFDec abs(const BFDec &a);
BFDec pow(const BFDec &a, limb_t b);    // integer exponent only
```

## Converting between `BF` and `BFDec`

```cpp
BFDec d("0.1");
BF    b(d);         // decimal -> binary  (uses bf_prec() bits)
BFDec d2(b);        // binary -> decimal  (uses bf_dprec() digits)
```

Conversions round using the current context precision and rounding mode.

## Building

CMakeLists.txt at the project root:

```cmake
cmake_minimum_required(VERSION 3.16)
project(bf LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(libbf STATIC vendor/libbf/libbf.c vendor/libbf/cutils.c)
target_include_directories(libbf PRIVATE vendor/libbf)

add_executable(myapp src/main.cpp)
target_include_directories(myapp PRIVATE
    include                      # bf.hpp
    vendor/retain-recall/include # retain.hpp
    vendor                       # so "libbf/libbf.h" resolves
)
target_link_libraries(myapp PRIVATE libbf)
```

The `vendor/` (not `vendor/libbf/`) include path is important -- the
`vendor/libbf/version` file collides with the C++ `<version>` header.
