#include <iostream>
#include <sstream>
#include <cmath>
#include "bf.hpp"
#include "facts.h"

using namespace bellard;

// ── Constructors ────────────────────────────────────────────────────

FACTS(BF_Constructors) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // Default is zero
  FACT(BF().is_zero(), ==, true);

  // From int
  FACT(BF(0).is_zero(), ==, true);
  FACT(BF(42).to_double(), ==, 42.0);
  FACT(BF(-1).to_double(), ==, -1.0);

  // From int64_t
  FACT(BF(int64_t(1) << 53).to_double(), ==, 9007199254740992.0);

  // From uint64_t
  FACT(BF(uint64_t(0)).is_zero(), ==, true);
  FACT(BF(uint64_t(100)).to_double(), ==, 100.0);

  // From double
  FACT(BF(3.14).to_double(), ==, 3.14);
  FACT(BF(0.5).to_double(), ==, 0.5);
  FACT(BF(-0.0).is_zero(), ==, true);

  // From string
  FACT(BF("42").to_double(), ==, 42.0);
  FACT(BF("3.14", 10).to_double(), ==, 3.14);
  FACT(BF("ff", 16).to_double(), ==, 255.0);
  FACT(BF("10", 2).to_double(), ==, 2.0);

  // Copy
  BF a(42);
  BF b(a);
  FACT(b.to_double(), ==, 42.0);

  // Move
  BF c(std::move(b));
  FACT(c.to_double(), ==, 42.0);
}

// ── Arithmetic ──────────────────────────────────────────────────────

FACTS(BF_Arithmetic) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  BF a(10), b(3);

  // Basic ops
  FACT((a + b).to_double(), ==, 13.0);
  FACT((a - b).to_double(), ==, 7.0);
  FACT((a * b).to_double(), ==, 30.0);

  // Division
  BF quot = a / b;
  FACT(quot.is_finite(), ==, true);
  // 10/3 is not exactly representable, but at 256 bits 10/3 * 3 rounds back to 10
  FACT((quot * b).to_double(), ==, 10.0);

  // Unary negation
  FACT((-a).to_double(), ==, -10.0);
  FACT((-(-a)).to_double(), ==, 10.0);

  // Compound assignment
  BF x(5);
  x += BF(3); FACT(x.to_double(), ==, 8.0);
  x -= BF(2); FACT(x.to_double(), ==, 6.0);
  x *= BF(4); FACT(x.to_double(), ==, 24.0);
  x /= BF(6); FACT(x.to_double(), ==, 4.0);
}

// ── Comparison ──────────────────────────────────────────────────────

FACTS(BF_Comparison) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  BF a(1), b(2), c(1);

  FACT(a == c, ==, true);
  FACT(a != b, ==, true);
  FACT(a < b, ==, true);
  FACT(b > a, ==, true);
  FACT(a <= c, ==, true);
  FACT(a <= b, ==, true);
  FACT(b >= a, ==, true);
  FACT(a >= c, ==, true);

  // Negative
  FACT(a == b, ==, false);
  FACT(a != c, ==, false);
  FACT(b < a, ==, false);
  FACT(a > b, ==, false);
}

// ── State queries ───────────────────────────────────────────────────

FACTS(BF_StateQueries) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  FACT(BF(0).is_zero(), ==, true);
  FACT(BF(1).is_zero(), ==, false);
  FACT(BF(1).is_finite(), ==, true);
  FACT(BF(0).is_finite(), ==, true);
  FACT(BF(1).is_nan(), ==, false);
  FACT(BF(1).is_inf(), ==, false);

  // Infinity via 1/0
  BF inf = BF(1) / BF(0);
  FACT(inf.is_inf(), ==, true);
  FACT(inf.is_finite(), ==, false);
  FACT(inf.is_nan(), ==, false);

  // NaN via 0/0
  BF nan = BF(0) / BF(0);
  FACT(nan.is_nan(), ==, true);
  FACT(nan.is_finite(), ==, false);
}

// ── Math functions ──────────────────────────────────────────────────

FACTS(BF_Math) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // sqrt
  BF s = sqrt(BF(4));
  FACT(s.to_double(), ==, 2.0);

  BF s2 = sqrt(BF(2));
  FACT(s2 * s2 == BF(2), ==, false); // not exact, but close
  FACT((s2 * s2 - BF(2)).to_double() < 1e-70, ==, true);

  // exp/log round-trip
  BF e = exp(BF(1));
  BF l = log(e);
  FACT((l - BF(1)).to_double() < 1e-70, ==, true);

  // sin/cos identity
  BF angle = pi() / BF(4);
  BF s_val = sin(angle);
  BF c_val = cos(angle);
  BF identity = s_val * s_val + c_val * c_val;
  FACT((identity - BF(1)).to_double() < 1e-70, ==, true);

  // abs
  FACT(abs(BF(-5)).to_double(), ==, 5.0);
  FACT(abs(BF(5)).to_double(), ==, 5.0);

  // pi
  BF p = pi();
  FACT(p.to_double() > 3.14159265, ==, true);
  FACT(p.to_double() < 3.14159266, ==, true);

  // pow
  FACT(pow(BF(2), BF(10)).to_double(), ==, 1024.0);
}

// ── Conversions ─────────────────────────────────────────────────────

FACTS(BF_Conversions) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // to_double with rounding modes
  BF tenth = BF(1) / BF(10);
  double dn = tenth.to_double(BF_RNDN);
  double dd = tenth.to_double(BF_RNDD);
  double du = tenth.to_double(BF_RNDU);
  FACT(dd <= dn, ==, true);
  FACT(dn <= du, ==, true);

  // to_int64
  FACT(BF(42).to_int64(), ==, 42);
  FACT(BF(-7).to_int64(), ==, -7);

  // to_string — lossless round-trip (may include trailing zeros for precision)
  std::string s42 = BF(42).to_string();
  FACT(BF(s42).to_double(), ==, 42.0);  // round-trips correctly

  std::string s0 = BF(0).to_string();
  FACT(BF(s0).is_zero(), ==, true);

  // to_string with explicit digits (significant digits for FIXED format)
  FACT(BF(42).to_string(10, 5), ==, std::string("42.000"));

  // to_string radix — round-trip
  std::string hex = BF(255).to_string(16);
  FACT(BF(hex, 16).to_double(), ==, 255.0);

  // BF <-> BFDec round-trip
  BFDec d("0.5");
  BF b(d);
  FACT(b.to_double(), ==, 0.5);
  BFDec d2(b);
  FACT(d2.to_double(), ==, 0.5);
}

// ── Precision ───────────────────────────────────────────────────────

FACTS(BF_Precision) {
  // Higher precision gives more accurate sqrt(2)
  BFContext lo_ctx(64, 34);
  retain<BFContext> use_lo(&lo_ctx);
  BF lo_s2 = sqrt(BF(2));
  std::string lo_str = lo_s2.to_string();

  BFContext hi_ctx(512, 34);
  retain<BFContext> use_hi(&hi_ctx);
  BF hi_s2 = sqrt(BF(2));
  std::string hi_str = hi_s2.to_string();

  FACT(hi_str.size() > lo_str.size(), ==, true);

  // Dynamic precision change
  bf_prec(1024);
  BF very_hi = sqrt(BF(2));
  std::string very_hi_str = very_hi.to_string();
  FACT(very_hi_str.size() > hi_str.size(), ==, true);
}

// ── Ref-counted allocator ───────────────────────────────────────────

FACTS(BF_RefCounting) {
  BFContext outer(64, 10);
  retain<BFContext> use_outer(&outer);

  BF escaped;
  {
    BFContext inner(256, 34);
    retain<BFContext> use(&inner);
    escaped = sqrt(BF(2));
  }
  // escaped's allocator is still alive via shared_ptr
  // this should not crash
  FACT(escaped.to_double() > 1.414, ==, true);
  FACT(escaped.to_double() < 1.415, ==, true);
  FACT(escaped.is_finite(), ==, true);
}

// ── Stream output ───────────────────────────────────────────────────

FACTS(BF_Stream) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  std::ostringstream oss;
  oss << BF(42);
  // Round-trips correctly through stream
  FACT(BF(oss.str()).to_double(), ==, 42.0);

  oss.str("");
  oss << BF(0);
  FACT(BF(oss.str()).is_zero(), ==, true);
}

FACTS_REGISTER_ALL() {
  FACTS_REGISTER(BF_Constructors);
  FACTS_REGISTER(BF_Arithmetic);
  FACTS_REGISTER(BF_Comparison);
  FACTS_REGISTER(BF_StateQueries);
  FACTS_REGISTER(BF_Math);
  FACTS_REGISTER(BF_Conversions);
  FACTS_REGISTER(BF_Precision);
  FACTS_REGISTER(BF_RefCounting);
  FACTS_REGISTER(BF_Stream);
}

FACTS_MAIN
