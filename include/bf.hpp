#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <new>
#include <cstdlib>

extern "C" {
#include "libbf/libbf.h"
}

#include "retain.hpp"

namespace bellard {

class BFDec;

// ── Ref-counted allocator wrapping bf_context_t ─────────────────────

struct BFAlloc {
    bf_context_t ctx;

    static void *realloc_wrapper(void *, void *ptr, size_t size) {
        if (size == 0) { free(ptr); return nullptr; }
        return realloc(ptr, size);
    }

    BFAlloc() { bf_context_init(&ctx, realloc_wrapper, nullptr); }
    ~BFAlloc() { bf_context_end(&ctx); }

    BFAlloc(const BFAlloc &) = delete;
    BFAlloc &operator=(const BFAlloc &) = delete;
};

// ── Context: allocator + precision/rounding settings ────────────────

struct BFContext {
    std::shared_ptr<BFAlloc> alloc;
    limb_t prec;    // binary precision in bits (for BF)
    limb_t dprec;   // decimal precision in digits (for BFDec)
    bf_flags_t flags;

    BFContext(limb_t prec = 128, limb_t dprec = 34, bf_rnd_t rnd = BF_RNDN)
        : alloc(std::make_shared<BFAlloc>()),
          prec(prec), dprec(dprec),
          flags(bf_set_exp_bits(BF_EXP_BITS_MAX) | rnd)
    {}

    // Create a context sharing the same allocator but different settings
    BFContext(const std::shared_ptr<BFAlloc> &a, limb_t prec, limb_t dprec, bf_rnd_t rnd = BF_RNDN)
        : alloc(a), prec(prec), dprec(dprec),
          flags(bf_set_exp_bits(BF_EXP_BITS_MAX) | rnd)
    {}

    bf_context_t *ctx() { return &alloc->ctx; }

    BFContext(const BFContext &) = delete;
    BFContext &operator=(const BFContext &) = delete;
};

// ── Context accessors via retain/recall ─────────────────────────────

inline std::shared_ptr<BFAlloc> bf_alloc() { return recall<BFContext>()->alloc; }
inline bf_context_t *bf_ctx() { return recall<BFContext>()->ctx(); }

inline limb_t bf_prec() { return recall<BFContext>()->prec; }
inline void bf_prec(limb_t p) { recall<BFContext>()->prec = p; }

inline limb_t bf_dprec() { return recall<BFContext>()->dprec; }
inline void bf_dprec(limb_t p) { recall<BFContext>()->dprec = p; }

inline bf_flags_t bf_flags() { return recall<BFContext>()->flags; }

inline bf_rnd_t bf_rnd() {
    return static_cast<bf_rnd_t>(recall<BFContext>()->flags & BF_RND_MASK);
}
inline void bf_rnd(bf_rnd_t rnd) {
    auto &f = recall<BFContext>()->flags;
    f = (f & ~BF_RND_MASK) | (rnd & BF_RND_MASK);
}

// ── Binary floating point (base 2) ──────────────────────────────────

class BF {
    bf_t val;
    std::shared_ptr<BFAlloc> alloc;  // ref-counted allocator

public:
    BF() : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
    }

    BF(int v) : BF(static_cast<int64_t>(v)) {}

    BF(int64_t v) : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
        bf_set_si(&val, v);
    }

    BF(uint64_t v) : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
        bf_set_ui(&val, v);
    }

    BF(double v) : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
        bf_set_float64(&val, v);
    }

    BF(const char *str, int radix = 10) : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
        bf_atof(&val, str, nullptr, radix, bf_prec(), bf_flags());
    }

    BF(const std::string &str, int radix = 10) : BF(str.c_str(), radix) {}

    BF(const BF &other) : alloc(bf_alloc()) {
        bf_init(&alloc->ctx, &val);
        bf_set(&val, &other.val);
    }

    BF(BF &&other) noexcept : alloc(std::move(other.alloc)) {
        // bf_move does *r = *a, so val.ctx becomes other's allocator
        // which alloc (moved from other) keeps alive
        bf_init(&alloc->ctx, &val);
        bf_move(&val, &other.val);
        other.val.tab = nullptr;
        other.val.len = 0;
        other.val.expn = BF_EXP_ZERO;
    }

    // Convert from decimal float
    inline BF(const BFDec &d);

    ~BF() { bf_delete(&val); }

    BF &operator=(const BF &other) {
        if (this != &other) {
            // Keep our allocator — bf_set allocates via val.ctx (ours)
            bf_set(&val, &other.val);
        }
        return *this;
    }

    BF &operator=(BF &&other) noexcept {
        if (this != &other) {
            // bf_move sets val.ctx = other's allocator, so take its ref
            alloc = std::move(other.alloc);
            bf_move(&val, &other.val);
            other.val.tab = nullptr;
            other.val.len = 0;
            other.val.expn = BF_EXP_ZERO;
        }
        return *this;
    }

    BF &operator=(int64_t v) { bf_set_si(&val, v); return *this; }
    BF &operator=(double v) { bf_set_float64(&val, v); return *this; }

    // Arithmetic operators
    BF operator+(const BF &b) const {
        BF r;
        bf_add(&r.val, &val, &b.val, bf_prec(), bf_flags());
        return r;
    }

    BF operator-(const BF &b) const {
        BF r;
        bf_sub(&r.val, &val, &b.val, bf_prec(), bf_flags());
        return r;
    }

    BF operator*(const BF &b) const {
        BF r;
        bf_mul(&r.val, &val, &b.val, bf_prec(), bf_flags());
        return r;
    }

    BF operator/(const BF &b) const {
        BF r;
        bf_div(&r.val, &val, &b.val, bf_prec(), bf_flags());
        return r;
    }

    BF operator-() const {
        BF r(*this);
        bf_neg(&r.val);
        return r;
    }

    BF &operator+=(const BF &b) { bf_add(&val, &val, &b.val, bf_prec(), bf_flags()); return *this; }
    BF &operator-=(const BF &b) { bf_sub(&val, &val, &b.val, bf_prec(), bf_flags()); return *this; }
    BF &operator*=(const BF &b) { bf_mul(&val, &val, &b.val, bf_prec(), bf_flags()); return *this; }
    BF &operator/=(const BF &b) { bf_div(&val, &val, &b.val, bf_prec(), bf_flags()); return *this; }

    // Comparison operators
    bool operator==(const BF &b) const { return bf_cmp_eq(&val, &b.val); }
    bool operator!=(const BF &b) const { return !bf_cmp_eq(&val, &b.val); }
    bool operator<(const BF &b) const { return bf_cmp_lt(&val, &b.val); }
    bool operator<=(const BF &b) const { return bf_cmp_le(&val, &b.val); }
    bool operator>(const BF &b) const { return bf_cmp_lt(&b.val, &val); }
    bool operator>=(const BF &b) const { return bf_cmp_le(&b.val, &val); }

    // State queries
    bool is_nan() const { return bf_is_nan(&val); }
    bool is_zero() const { return bf_is_zero(&val); }
    bool is_finite() const { return bf_is_finite(&val); }
    bool is_inf() const { return !bf_is_finite(&val) && !bf_is_nan(&val); }

    // Conversions
    double to_double() const {
        double d;
        bf_get_float64(&val, &d, bf_rnd());
        return d;
    }

    double to_double(bf_rnd_t rnd) const {
        double d;
        bf_get_float64(&val, &d, rnd);
        return d;
    }

    int64_t to_int64() const {
        int64_t v;
        bf_get_int64(&v, &val, 0);
        return v;
    }

    // Precision actually carried by this value (in bits)
    limb_t precision() const { return val.len * LIMB_BITS; }

    std::string to_string(int radix = 10, limb_t digits = 0) const {
        bf_flags_t fmt_flags;
        limb_t p;
        if (digits > 0) {
            fmt_flags = BF_FTOA_FORMAT_FIXED | BF_RNDN;
            p = digits;
        } else {
            // Use value's actual precision for lossless round-trip
            fmt_flags = BF_FTOA_FORMAT_FREE | bf_flags();
            p = precision();
        }
        size_t len;
        char *s = bf_ftoa(&len, &val, radix, p, fmt_flags);
        if (!s) throw std::bad_alloc();
        std::string result(s, len);
        free(s);
        return result;
    }

    // Access to underlying C type
    const bf_t *c_bf() const { return &val; }
    bf_t *c_bf() { return &val; }

    // Stream output
    friend std::ostream &operator<<(std::ostream &os, const BF &a) {
        return os << a.to_string();
    }

    // Friends
    friend class BFDec;
    friend BF sqrt(const BF &a);
    friend BF exp(const BF &a);
    friend BF log(const BF &a);
    friend BF pow(const BF &a, const BF &b);
    friend BF sin(const BF &a);
    friend BF cos(const BF &a);
    friend BF tan(const BF &a);
    friend BF atan(const BF &a);
    friend BF atan2(const BF &y, const BF &x);
    friend BF asin(const BF &a);
    friend BF acos(const BF &a);
    friend BF abs(const BF &a);
    friend BF pi();
    friend BF ln2();
};

// ── Decimal floating point (base 10) ────────────────────────────────

class BFDec {
    bfdec_t val;
    std::shared_ptr<BFAlloc> alloc;  // ref-counted allocator

public:
    BFDec() : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
    }

    BFDec(int v) : BFDec(static_cast<int64_t>(v)) {}

    BFDec(int64_t v) : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_set_si(&val, v);
    }

    BFDec(uint64_t v) : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_set_ui(&val, v);
    }

    BFDec(double v) : BFDec(BF(v)) {}

    BFDec(const char *str) : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_atof(&val, str, nullptr, bf_dprec(), bf_flags());
    }

    BFDec(const std::string &str) : BFDec(str.c_str()) {}

    BFDec(const BFDec &other) : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_set(&val, &other.val);
    }

    BFDec(BFDec &&other) noexcept : alloc(std::move(other.alloc)) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_move(&val, &other.val);
        other.val.tab = nullptr;
        other.val.len = 0;
        other.val.expn = BF_EXP_ZERO;
    }

    // Convert from binary float
    BFDec(const BF &b) : alloc(bf_alloc()) {
        bfdec_init(&alloc->ctx, &val);
        bfdec_from_f(&val, b.c_bf(), bf_dprec(), bf_flags());
    }

    ~BFDec() { bfdec_delete(&val); }

    BFDec &operator=(const BFDec &other) {
        if (this != &other) bfdec_set(&val, &other.val);
        return *this;
    }

    BFDec &operator=(BFDec &&other) noexcept {
        if (this != &other) {
            alloc = std::move(other.alloc);
            bfdec_move(&val, &other.val);
            other.val.tab = nullptr;
            other.val.len = 0;
            other.val.expn = BF_EXP_ZERO;
        }
        return *this;
    }

    BFDec &operator=(int64_t v) { bfdec_set_si(&val, v); return *this; }

    // Arithmetic operators
    BFDec operator+(const BFDec &b) const {
        BFDec r;
        bfdec_add(&r.val, &val, &b.val, bf_dprec(), bf_flags());
        return r;
    }

    BFDec operator-(const BFDec &b) const {
        BFDec r;
        bfdec_sub(&r.val, &val, &b.val, bf_dprec(), bf_flags());
        return r;
    }

    BFDec operator*(const BFDec &b) const {
        BFDec r;
        bfdec_mul(&r.val, &val, &b.val, bf_dprec(), bf_flags());
        return r;
    }

    BFDec operator/(const BFDec &b) const {
        BFDec r;
        bfdec_div(&r.val, &val, &b.val, bf_dprec(), bf_flags());
        return r;
    }

    BFDec operator-() const {
        BFDec r(*this);
        bfdec_neg(&r.val);
        return r;
    }

    BFDec &operator+=(const BFDec &b) { bfdec_add(&val, &val, &b.val, bf_dprec(), bf_flags()); return *this; }
    BFDec &operator-=(const BFDec &b) { bfdec_sub(&val, &val, &b.val, bf_dprec(), bf_flags()); return *this; }
    BFDec &operator*=(const BFDec &b) { bfdec_mul(&val, &val, &b.val, bf_dprec(), bf_flags()); return *this; }
    BFDec &operator/=(const BFDec &b) { bfdec_div(&val, &val, &b.val, bf_dprec(), bf_flags()); return *this; }

    // Comparison operators
    bool operator==(const BFDec &b) const { return bfdec_cmp_eq(&val, &b.val); }
    bool operator!=(const BFDec &b) const { return !bfdec_cmp_eq(&val, &b.val); }
    bool operator<(const BFDec &b) const { return bfdec_cmp_lt(&val, &b.val); }
    bool operator<=(const BFDec &b) const { return bfdec_cmp_le(&val, &b.val); }
    bool operator>(const BFDec &b) const { return bfdec_cmp_lt(&b.val, &val); }
    bool operator>=(const BFDec &b) const { return bfdec_cmp_le(&b.val, &val); }

    // State queries
    bool is_nan() const { return bfdec_is_nan(&val); }
    bool is_zero() const { return bfdec_is_zero(&val); }
    bool is_finite() const { return bfdec_is_finite(&val); }
    bool is_inf() const { return !bfdec_is_finite(&val) && !bfdec_is_nan(&val); }

    // Conversions
    double to_double() const {
        return BF(*this).to_double();
    }

    double to_double(bf_rnd_t rnd) const {
        return BF(*this).to_double(rnd);
    }

    int64_t to_int64() const {
        int32_t v;
        bfdec_get_int32(&v, &val);
        return v;
    }

    // Precision actually carried by this value (in decimal digits)
    limb_t precision() const {
        constexpr limb_t digits_per_limb = (LIMB_BITS == 64) ? 19 : 9;
        return val.len * digits_per_limb;
    }

    std::string to_string(limb_t digits = 0) const {
        bf_flags_t fmt_flags;
        limb_t p;
        if (digits > 0) {
            fmt_flags = BF_FTOA_FORMAT_FIXED | BF_RNDN;
            p = digits;
        } else {
            // Use value's actual precision for lossless round-trip
            fmt_flags = BF_FTOA_FORMAT_FREE | bf_flags();
            p = precision();
        }
        size_t len;
        char *s = bfdec_ftoa(&len, &val, p, fmt_flags);
        if (!s) throw std::bad_alloc();
        std::string result(s, len);
        free(s);
        return result;
    }

    // Access to underlying C type
    const bfdec_t *c_bfdec() const { return &val; }
    bfdec_t *c_bfdec() { return &val; }

    // Stream output
    friend std::ostream &operator<<(std::ostream &os, const BFDec &a) {
        return os << a.to_string();
    }

    // Friends
    friend class BF;
    friend BFDec sqrt(const BFDec &a);
    friend BFDec abs(const BFDec &a);
    friend BFDec pow(const BFDec &a, limb_t b);
};

// ── Deferred inline: BF from BFDec ─────────────────────────────────

inline BF::BF(const BFDec &d) : alloc(bf_alloc()) {
    bf_init(&alloc->ctx, &val);
    bfdec_to_f(&val, d.c_bfdec(), bf_prec(), bf_flags());
}

// ── BF math functions ───────────────────────────────────────────────

inline BF sqrt(const BF &a) {
    BF r;
    bf_sqrt(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF exp(const BF &a) {
    BF r;
    bf_exp(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF log(const BF &a) {
    BF r;
    bf_log(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF pow(const BF &a, const BF &b) {
    BF r;
    bf_pow(r.c_bf(), a.c_bf(), b.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF sin(const BF &a) {
    BF r;
    bf_sin(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF cos(const BF &a) {
    BF r;
    bf_cos(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF tan(const BF &a) {
    BF r;
    bf_tan(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF atan(const BF &a) {
    BF r;
    bf_atan(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF atan2(const BF &y, const BF &x) {
    BF r;
    bf_atan2(r.c_bf(), y.c_bf(), x.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF asin(const BF &a) {
    BF r;
    bf_asin(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF acos(const BF &a) {
    BF r;
    bf_acos(r.c_bf(), a.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF abs(const BF &a) {
    BF r(a);
    r.c_bf()->sign = 0;
    return r;
}

inline BF pi() {
    BF r;
    bf_const_pi(r.c_bf(), bf_prec(), bf_flags());
    return r;
}

inline BF ln2() {
    BF r;
    bf_const_log2(r.c_bf(), bf_prec(), bf_flags());
    return r;
}

// ── BFDec math functions ────────────────────────────────────────────

inline BFDec sqrt(const BFDec &a) {
    BFDec r;
    bfdec_sqrt(&r.val, &a.val, bf_dprec(), bf_flags());
    return r;
}

inline BFDec abs(const BFDec &a) {
    BFDec r(a);
    r.c_bfdec()->sign = 0;
    return r;
}

inline BFDec pow(const BFDec &a, limb_t b) {
    BFDec r;
    bfdec_pow_ui(&r.val, &a.val, b);
    return r;
}

} // namespace bellard
