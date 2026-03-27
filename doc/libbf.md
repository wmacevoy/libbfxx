# libbf -- Tiny Arbitrary Precision Floating Point Library

Copyright (c) 2017-2025 Fabrice Bellard. MIT License.

libbf is a small C library for arbitrary precision binary and decimal floating
point arithmetic.  It supports configurable precision, IEEE 754 rounding modes,
subnormal numbers, and a full set of transcendental functions.

## Building

libbf is compiled as a static library by the project CMakeLists.txt:

```
vendor/libbf/libbf.c   -- core library
vendor/libbf/cutils.c  -- utility functions
vendor/libbf/libbf.h   -- public API (only header you need)
```

> **Note for C++ users:** the `vendor/libbf/` directory contains a file named
> `version` (no extension) that shadows the C++ `<version>` header.  Do not add
> `vendor/libbf/` directly to C++ include paths.  The project CMakeLists.txt
> handles this by keeping that path PRIVATE to the C target and exposing
> `vendor/` instead so C++ code can `#include "libbf/libbf.h"`.

## Platform types

libbf selects a limb size at compile time based on the platform:

| Platform          | `LIMB_BITS` | `limb_t`   | `slimb_t`  |
|-------------------|-------------|------------|------------|
| 64-bit + __int128 | 64          | `uint64_t` | `int64_t`  |
| everything else   | 32          | `uint32_t` | `int32_t`  |

## Core types

### `bf_t` -- Binary floating point number

```c
typedef struct {
    struct bf_context_t *ctx;   // owning context
    int sign;                   // 0 = positive, 1 = negative
    slimb_t expn;               // exponent (or special value)
    limb_t len;                 // number of limbs in mantissa
    limb_t *tab;                // mantissa digits (allocated)
} bf_t;
```

Special values are encoded via `expn`:

| Value      | `expn`       | `len` | `sign`  |
|------------|--------------|-------|---------|
| +/- zero   | `BF_EXP_ZERO`| 0     | 0 or 1  |
| +/- inf    | `BF_EXP_INF` | 0     | 0 or 1  |
| NaN        | `BF_EXP_NAN` | 0     | ignored |

### `bfdec_t` -- Decimal floating point number

Identical layout to `bf_t`.  Mantissa limbs store base-10 digit groups
(`BF_DEC_BASE` = 10^19 on 64-bit, 10^9 on 32-bit).

### `bf_context_t` -- Allocation context

```c
typedef struct bf_context_t {
    void *realloc_opaque;
    bf_realloc_func_t *realloc_func;
    BFConstCache log2_cache;    // cached ln(2)
    BFConstCache pi_cache;      // cached pi
    struct BFNTTState *ntt_state;
} bf_context_t;
```

All numbers allocated from a context share its allocator and constant caches.

## Precision and flags

Every arithmetic operation takes a `limb_t prec` and `bf_flags_t flags`.

### Precision

- **For `bf_t`:** precision is in *bits*.
- **For `bfdec_t`:** precision is in *decimal digits*.
- `BF_PREC_INF` enables exact (infinite precision) integer arithmetic.
- `BF_PREC_MIN` = 2.

### Flags

`bf_flags_t` is a `uint32_t` packing the rounding mode and exponent range:

```c
flags = rounding_mode | bf_set_exp_bits(n)
```

| Helper                     | Purpose                                        |
|----------------------------|------------------------------------------------|
| `bf_set_exp_bits(n)`       | Encode exponent bit width into flags            |
| `bf_get_exp_bits(flags)`   | Decode exponent bit width from flags            |
| `BF_FLAG_EXT_EXP`          | Shortcut for maximum extended exponent range    |

Optional flag bits:

| Flag                  | Effect                                                         |
|-----------------------|----------------------------------------------------------------|
| `BF_FLAG_SUBNORMAL`  | Allow subnormal results (requires exponent bits set)           |
| `BF_FLAG_RADPNT_PREC`| Precision counts digits *after* the radix point, not total     |

## Rounding modes

```c
typedef enum {
    BF_RNDN,   // round to nearest, ties to even (IEEE default)
    BF_RNDZ,   // round toward zero
    BF_RNDD,   // round toward -infinity (floor)
    BF_RNDU,   // round toward +infinity (ceil)
    BF_RNDNA,  // round to nearest, ties away from zero
    BF_RNDA,   // round away from zero
    BF_RNDF,   // faithful rounding (nondeterministic, RNDD or RNDU)
} bf_rnd_t;
```

## Status flags

Arithmetic operations return a bitmask of status flags (OR of):

| Flag                | Meaning                |
|---------------------|------------------------|
| `BF_ST_INVALID_OP`  | Invalid operation       |
| `BF_ST_DIVIDE_ZERO` | Division by zero        |
| `BF_ST_OVERFLOW`    | Overflow occurred       |
| `BF_ST_UNDERFLOW`   | Underflow occurred      |
| `BF_ST_INEXACT`     | Result was rounded      |
| `BF_ST_MEM_ERROR`   | Allocation failed (NaN) |

## API reference

### Context lifecycle

```c
void bf_context_init(bf_context_t *s, bf_realloc_func_t *realloc_func,
                     void *realloc_opaque);
void bf_context_end(bf_context_t *s);
void bf_clear_cache(bf_context_t *s);
```

The `realloc_func` signature is:

```c
typedef void *bf_realloc_func_t(void *opaque, void *ptr, size_t size);
```

When `size == 0`, it must free `ptr`.  When `ptr == NULL`, it must allocate.
Passing `NULL` for `realloc_func` to `bf_context_init` will crash on any
allocation -- always provide one.

### Number lifecycle

```c
void bf_init(bf_context_t *s, bf_t *r);      // initialize (zero)
void bf_delete(bf_t *r);                      // free mantissa
int  bf_set(bf_t *r, const bf_t *a);          // deep copy
void bf_move(bf_t *r, bf_t *a);               // transfer ownership
```

> **Caution:** `bf_move` does not null the source's `tab` pointer.  After
> `bf_move(r, a)`, both `r` and `a` point to the same mantissa.  The caller
> must null `a->tab` to prevent double-free.

### Setting values

```c
int  bf_set_ui(bf_t *r, uint64_t a);
int  bf_set_si(bf_t *r, int64_t a);
int  bf_set_float64(bf_t *a, double d);
void bf_set_nan(bf_t *r);
void bf_set_zero(bf_t *r, int is_neg);
void bf_set_inf(bf_t *r, int is_neg);
```

### Querying state

```c
void bf_neg(bf_t *r);                         // negate in place
int  bf_is_finite(const bf_t *a);
int  bf_is_nan(const bf_t *a);
int  bf_is_zero(const bf_t *a);
void bf_memcpy(bf_t *r, const bf_t *a);       // shallow copy (aliased tab!)
```

### Comparison

```c
int bf_cmp(const bf_t *a, const bf_t *b);     // <0, 0, >0
int bf_cmpu(const bf_t *a, const bf_t *b);    // unsigned (magnitude)
int bf_cmp_full(const bf_t *a, const bf_t *b);// handles NaN
int bf_cmp_eq(const bf_t *a, const bf_t *b);  // == (0 or 1)
int bf_cmp_lt(const bf_t *a, const bf_t *b);  // <
int bf_cmp_le(const bf_t *a, const bf_t *b);  // <=
```

### Binary arithmetic

All return status flags.  `r` may alias `a` or `b`.

```c
int bf_add(bf_t *r, const bf_t *a, const bf_t *b, limb_t prec, bf_flags_t flags);
int bf_sub(bf_t *r, const bf_t *a, const bf_t *b, limb_t prec, bf_flags_t flags);
int bf_mul(bf_t *r, const bf_t *a, const bf_t *b, limb_t prec, bf_flags_t flags);
int bf_div(bf_t *r, const bf_t *a, const bf_t *b, limb_t prec, bf_flags_t flags);

int bf_add_si(bf_t *r, const bf_t *a, int64_t b1, limb_t prec, bf_flags_t flags);
int bf_mul_ui(bf_t *r, const bf_t *a, uint64_t b1, limb_t prec, bf_flags_t flags);
int bf_mul_si(bf_t *r, const bf_t *a, int64_t b1, limb_t prec, bf_flags_t flags);
int bf_mul_2exp(bf_t *r, slimb_t e, limb_t prec, bf_flags_t flags);  // r *= 2^e
```

### Division and remainder

```c
int bf_divrem(bf_t *q, bf_t *r, const bf_t *a, const bf_t *b,
              limb_t prec, bf_flags_t flags, int rnd_mode);
int bf_rem(bf_t *r, const bf_t *a, const bf_t *b, limb_t prec,
           bf_flags_t flags, int rnd_mode);
int bf_remquo(slimb_t *pq, bf_t *r, const bf_t *a, const bf_t *b,
              limb_t prec, bf_flags_t flags, int rnd_mode);
```

`rnd_mode` for the quotient can be any `bf_rnd_t` or
`BF_DIVREM_EUCLIDIAN` (alias for `BF_RNDF`).

### Rounding and square root

```c
int bf_round(bf_t *r, limb_t prec, bf_flags_t flags);
int bf_rint(bf_t *r, int rnd_mode);                       // round to integer
int bf_sqrt(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_sqrtrem(bf_t *r, bf_t *rem, const bf_t *a);        // exact sqrt + remainder
```

### Bitwise operations (integer)

```c
int bf_logic_or(bf_t *r, const bf_t *a, const bf_t *b);
int bf_logic_xor(bf_t *r, const bf_t *a, const bf_t *b);
int bf_logic_and(bf_t *r, const bf_t *a, const bf_t *b);
```

### Transcendental functions

```c
int bf_const_log2(bf_t *T, limb_t prec, bf_flags_t flags);  // ln(2)
int bf_const_pi(bf_t *T, limb_t prec, bf_flags_t flags);    // pi

int bf_exp(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_log(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_pow(bf_t *r, const bf_t *x, const bf_t *y, limb_t prec, bf_flags_t flags);

int bf_cos(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_sin(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_tan(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);

int bf_atan(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_atan2(bf_t *r, const bf_t *y, const bf_t *x, limb_t prec, bf_flags_t flags);
int bf_asin(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
int bf_acos(bf_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
```

### Conversion to/from integers

```c
int bf_get_int32(int *pres, const bf_t *a, int flags);
int bf_get_int64(int64_t *pres, const bf_t *a, int flags);
int bf_get_uint64(uint64_t *pres, const bf_t *a);
int bf_get_float64(const bf_t *a, double *pres, bf_rnd_t rnd_mode);
```

`flags` for `bf_get_int32/64` can include `BF_GET_INT_MOD` (modulo 2^n
instead of saturation).

### String conversion

#### Parsing (`bf_atof`)

```c
int bf_atof(bf_t *a, const char *str, const char **pnext, int radix,
            limb_t prec, bf_flags_t flags);
```

- `radix` = 0 auto-detects from prefix (0x, 0o, 0b); 2-36 for explicit.
- `pnext` receives pointer to first unconsumed character (NULL to ignore).

Additional `flags` bits for parsing:

| Flag                  | Effect                                  |
|-----------------------|-----------------------------------------|
| `BF_ATOF_NO_HEX`     | Reject 0x prefix (radix 0 or 16)       |
| `BF_ATOF_BIN_OCT`    | Accept 0b and 0o prefixes (radix 0)    |
| `BF_ATOF_NO_NAN_INF` | Reject NaN and Inf strings              |
| `BF_ATOF_EXPONENT`   | Return exponent separately (with atof2) |

#### Formatting (`bf_ftoa`)

```c
char *bf_ftoa(size_t *plen, const bf_t *a, int radix, limb_t prec,
              bf_flags_t flags);
```

Returns a `malloc`'d string (caller must `free`).

Format is selected via flags:

| Flag                       | Meaning                                              |
|----------------------------|------------------------------------------------------|
| `BF_FTOA_FORMAT_FIXED`    | `prec` significant digits                            |
| `BF_FTOA_FORMAT_FRAC`     | `prec` digits after decimal point                    |
| `BF_FTOA_FORMAT_FREE`     | minimum digits for round-trip at given precision      |
| `BF_FTOA_FORMAT_FREE_MIN` | absolute minimum digits (slower)                     |
| `BF_FTOA_FORCE_EXP`       | force exponential notation                           |
| `BF_FTOA_ADD_PREFIX`      | add 0x/0o/0b prefix                                  |
| `BF_FTOA_JS_QUIRKS`       | JavaScript compatibility ("Infinity", "+" exponents) |

## Decimal floating point (`bfdec_*`)

The `bfdec_t` API mirrors `bf_t` for arithmetic, comparison, and string I/O.
Key differences:

- Precision is in **decimal digits**, not bits.
- String functions (`bfdec_atof`, `bfdec_ftoa`) have no radix parameter
  (always base 10).
- No transcendental functions (no sin, cos, exp, log, etc.).
- `bfdec_pow_ui(r, a, b)` -- integer exponent power.
- Conversion between binary and decimal:

```c
int bfdec_to_f(bf_t *r, const bfdec_t *a, limb_t prec, bf_flags_t flags);
int bfdec_from_f(bfdec_t *r, const bf_t *a, limb_t prec, bf_flags_t flags);
```
