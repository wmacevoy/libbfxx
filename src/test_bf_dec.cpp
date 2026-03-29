#include <iostream>
#include <sstream>
#include <cmath>
#include "bf.hpp"
#include "facts.h"

using namespace bellard;

// ── Constructors ────────────────────────────────────────────────────

FACTS(BFDec_Constructors) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // Default is zero
  FACT(BFDec().is_zero(), ==, true);

  // From int
  FACT(BFDec(0).is_zero(), ==, true);
  FACT(BFDec(42).to_double(), ==, 42.0);
  FACT(BFDec(-1).to_double(), ==, -1.0);

  // From int64_t
  FACT(BFDec(int64_t(1000000)).to_double(), ==, 1000000.0);

  // From uint64_t
  FACT(BFDec(uint64_t(0)).is_zero(), ==, true);
  FACT(BFDec(uint64_t(100)).to_double(), ==, 100.0);

  // From double
  FACT(BFDec(0.5).to_double(), ==, 0.5);
  FACT(BFDec(1.0).to_double(), ==, 1.0);

  // From string — exact decimal
  FACT(BFDec("42").to_double(), ==, 42.0);
  FACT(BFDec("0.1").to_string(), ==, std::string("0.1"));
  FACT(BFDec("0.5").to_double(), ==, 0.5);
  FACT(BFDec("-3.14").to_double(), ==, -3.14);

  // Copy
  BFDec a("123.456");
  BFDec b(a);
  FACT(b.to_string(), ==, a.to_string());

  // Move
  BFDec c(std::move(b));
  FACT(c.to_string(), ==, a.to_string());
}

// ── Decimal exactness ───────────────────────────────────────────────

FACTS(BFDec_Exactness) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // 0.1 is exact in decimal, not in binary
  BFDec d("0.1");
  FACT(d.to_string(), ==, std::string("0.1"));

  // 0.1 + 0.2 == 0.3 exactly in decimal
  BFDec sum = BFDec("0.1") + BFDec("0.2");
  FACT(sum == BFDec("0.3"), ==, true);

  // Financial: $19.99 + 7.25% tax
  BFDec price("19.99");
  BFDec tax("0.0725");
  BFDec total = price + price * tax;
  FACT(total == BFDec("21.439275"), ==, true);

  // Exact representation preserved through arithmetic
  BFDec a("1.23");
  BFDec b("4.56");
  FACT((a + b) == BFDec("5.79"), ==, true);
  FACT((b - a) == BFDec("3.33"), ==, true);
}

// ── Arithmetic ──────────────────────────────────────────────────────

FACTS(BFDec_Arithmetic) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  BFDec a(10), b(3);

  FACT((a + b).to_double(), ==, 13.0);
  FACT((a - b).to_double(), ==, 7.0);
  FACT((a * b).to_double(), ==, 30.0);

  // Division
  BFDec q = a / b;
  FACT(q.is_finite(), ==, true);

  // Unary negation
  FACT((-a).to_double(), ==, -10.0);

  // Compound assignment
  BFDec x(5);
  x += BFDec(3); FACT(x.to_double(), ==, 8.0);
  x -= BFDec(2); FACT(x.to_double(), ==, 6.0);
  x *= BFDec(4); FACT(x.to_double(), ==, 24.0);
  x /= BFDec(6); FACT(x.to_double(), ==, 4.0);
}

// ── Comparison ──────────────────────────────────────────────────────

FACTS(BFDec_Comparison) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  BFDec a("1.1"), b("2.2"), c("1.1");

  FACT(a == c, ==, true);
  FACT(a != b, ==, true);
  FACT(a < b, ==, true);
  FACT(b > a, ==, true);
  FACT(a <= c, ==, true);
  FACT(a <= b, ==, true);
  FACT(b >= a, ==, true);

  FACT(a == b, ==, false);
  FACT(a != c, ==, false);
  FACT(b < a, ==, false);
}

// ── State queries ───────────────────────────────────────────────────

FACTS(BFDec_StateQueries) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  FACT(BFDec(0).is_zero(), ==, true);
  FACT(BFDec(1).is_zero(), ==, false);
  FACT(BFDec(1).is_finite(), ==, true);
  FACT(BFDec(1).is_nan(), ==, false);
  FACT(BFDec(1).is_inf(), ==, false);
}

// ── Math ────────────────────────────────────────────────────────────

FACTS(BFDec_Math) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // sqrt
  BFDec s = sqrt(BFDec(4));
  FACT(s == BFDec(2), ==, true);

  // sqrt(2) close to known value
  BFDec s2 = sqrt(BFDec(2));
  FACT(s2.to_double() > 1.414, ==, true);
  FACT(s2.to_double() < 1.415, ==, true);

  // abs
  FACT(abs(BFDec(-5)).to_double(), ==, 5.0);
  FACT(abs(BFDec(5)).to_double(), ==, 5.0);

  // pow (integer exponent)
  FACT(pow(BFDec(2), 10).to_double(), ==, 1024.0);
  FACT(pow(BFDec(10), 3).to_double(), ==, 1000.0);
}

// ── Conversions ─────────────────────────────────────────────────────

FACTS(BFDec_Conversions) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // to_double with rounding modes
  BFDec tenth("0.1");
  double dn = tenth.to_double(BF_RNDN);
  double dd = tenth.to_double(BF_RNDD);
  double du = tenth.to_double(BF_RNDU);
  FACT(dd < du, ==, true);     // 0.1 not exact in double
  FACT(dd <= dn, ==, true);
  FACT(dn <= du, ==, true);

  // to_double exact cases
  FACT(BFDec("0.5").to_double(), ==, 0.5);
  FACT(BFDec("0.25").to_double(), ==, 0.25);
  FACT(BFDec("1024").to_double(), ==, 1024.0);

  // to_int64
  FACT(BFDec(42).to_int64(), ==, 42);

  // to_string
  FACT(BFDec("0.1").to_string(), ==, std::string("0.1"));
  FACT(BFDec(42).to_string(), ==, std::string("42"));

  // BFDec -> BF -> BFDec for exact values
  BFDec half("0.5");
  BF bf_half(half);
  BFDec back(bf_half);
  FACT(back == half, ==, true);
}

// ── Precision ───────────────────────────────────────────────────────

FACTS(BFDec_Precision) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // 1/3 at 34 digits
  BFDec third = BFDec(1) / BFDec(3);
  std::string s34 = third.to_string();

  // 1/3 at 50 digits
  bf_dprec(50);
  BFDec third50 = BFDec(1) / BFDec(3);
  std::string s50 = third50.to_string();
  FACT(s50.size() > s34.size(), ==, true);

  // Restore and verify
  bf_dprec(34);
  BFDec again = BFDec(1) / BFDec(3);
  FACT(again.to_string(), ==, s34);
}

// ── Ref-counted allocator ───────────────────────────────────────────

FACTS(BFDec_RefCounting) {
  BFContext outer(64, 10);
  retain<BFContext> use_outer(&outer);

  BFDec escaped;
  {
    BFContext inner(256, 34);
    retain<BFContext> use(&inner);
    escaped = BFDec("3.14159265358979323846");
  }
  // escaped's allocator is still alive via ref-count
  FACT(escaped.to_double() > 3.14, ==, true);
  FACT(escaped.to_double() < 3.15, ==, true);
  FACT(escaped.is_finite(), ==, true);
}

// ── Stream output ───────────────────────────────────────────────────

FACTS(BFDec_Stream) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  std::ostringstream oss;
  oss << BFDec(42);
  FACT(oss.str(), ==, std::string("42"));

  oss.str("");
  oss << BFDec("0.1");
  FACT(oss.str(), ==, std::string("0.1"));
}

FACTS_REGISTER_ALL() {
  FACTS_REGISTER(BFDec_Constructors);
  FACTS_REGISTER(BFDec_Exactness);
  FACTS_REGISTER(BFDec_Arithmetic);
  FACTS_REGISTER(BFDec_Comparison);
  FACTS_REGISTER(BFDec_StateQueries);
  FACTS_REGISTER(BFDec_Math);
  FACTS_REGISTER(BFDec_Conversions);
  FACTS_REGISTER(BFDec_Precision);
  FACTS_REGISTER(BFDec_RefCounting);
  FACTS_REGISTER(BFDec_Stream);
}

FACTS_MAIN
