#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include "numeric.hpp"
#include "facts.h"

using namespace bellard;

struct Bob { bool bobness; Bob() : bobness(true) {} };
std::ostream& operator<<(std::ostream &out, const Bob &bob) { return out << "bob"; }

// ── Unbound ─────────────────────────────────────────────────────────

FACTS(AboutUniversalUnbound) {
  FACT(unbound().universal(), ==, true);
  FACT(unbound("").universal(), ==, true);
  FACT(unbound("x").universal(), ==, false);

  // Universal vs universal: all relations true
  FACT(unbound(), <, unbound());
  FACT(unbound(), <=, unbound());
  FACT(unbound(), ==, unbound());
  FACT(unbound(), !=, unbound());
  FACT(unbound(), >, unbound());
  FACT(unbound(), >=, unbound());

  // Universal vs concrete type: all relations true
  FACT(unbound(), <, Bob());
  FACT(unbound(), <=, Bob());
  FACT(unbound(), ==, Bob());
  FACT(unbound(), !=, Bob());
  FACT(unbound(), >, Bob());
  FACT(unbound(), >=, Bob());
}

FACTS(AboutNamedUnbound) {
  // Universal vs named: all true
  FACT(unbound(), <, unbound("x"));
  FACT(unbound(), <=, unbound("x"));
  FACT(unbound(), ==, unbound("x"));
  FACT(unbound(), !=, unbound("x"));
  FACT(unbound(), >, unbound("x"));
  FACT(unbound(), >=, unbound("x"));

  // Named vs universal: all true
  FACT(unbound("x"), <, unbound());
  FACT(unbound("x"), <=, unbound());
  FACT(unbound("x"), ==, unbound());
  FACT(unbound("x"), !=, unbound());
  FACT(unbound("x"), >, unbound());
  FACT(unbound("x"), >=, unbound());

  // Different names: all true
  FACT(unbound("x"), <, unbound("y"));
  FACT(unbound("x"), <=, unbound("y"));
  FACT(unbound("x"), ==, unbound("y"));
  FACT(unbound("x"), !=, unbound("y"));
  FACT(unbound("x"), >, unbound("y"));
  FACT(unbound("x"), >=, unbound("y"));

  // Same name: identity semantics
  FACT(unbound("x") < unbound("x"), ==, false);
  FACT(unbound("x") <= unbound("x"), ==, true);
  FACT(unbound("x") == unbound("x"), ==, true);
  FACT(unbound("x") != unbound("x"), ==, false);
  FACT(unbound("x") > unbound("x"), ==, false);
  FACT(unbound("x") >= unbound("x"), ==, true);
}

// ── NumericVariant cross-type comparison ─────────────────────────────

FACTS(NumericVariant_CrossType) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // double vs BF: same value
  NumericVariant dv = double(0.5);
  NumericVariant bv = BF(0.5);
  FACT(dv == bv, ==, true);
  FACT(dv != bv, ==, false);

  // double vs BFDec: same value
  NumericVariant decv = BFDec("0.5");
  FACT(dv == decv, ==, true);

  // BF vs BFDec: same value
  FACT(bv == decv, ==, true);

  // Ordering
  NumericVariant one = double(1.0);
  NumericVariant two_bf = BF(2);
  NumericVariant three_dec = BFDec("3");

  FACT(one < two_bf, ==, true);
  FACT(two_bf < three_dec, ==, true);
  FACT(one < three_dec, ==, true);
  FACT(three_dec > one, ==, true);

  // Unbound in variant: always satisfies
  NumericVariant uv = unbound("x");
  FACT(uv < one, ==, true);
  FACT(one < uv, ==, true);
  FACT(uv == one, ==, true);
}

// ── numeric construction ────────────────────────────────────────────

FACTS(Numeric_Construction) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // From double
  numeric nd(42.0);
  FACT(nd.type, ==, numeric::DOUBLE);
  FACT(nd.lo, ==, 42.0);
  FACT(nd.hi, ==, 42.0);

  // From int64_t — small fits in double
  numeric ni(int64_t(100));
  FACT(ni.type, ==, numeric::DOUBLE);
  FACT(ni.lo, ==, 100.0);

  // From int64_t — large exceeds double precision
  int64_t big = (int64_t(1) << 53) + 1;
  numeric nbig(big);
  FACT(nbig.type, ==, numeric::FLOAT);

  // From BF — exact in double
  numeric nbf_exact(BF(0.5));
  FACT(nbf_exact.type, ==, numeric::FLOAT);
  FACT(nbf_exact.lo, ==, 0.5);
  FACT(nbf_exact.hi, ==, 0.5);

  // From BF — not exact in double
  numeric nbf(sqrt(BF(2)));
  FACT(nbf.type, ==, numeric::FLOAT);
  FACT(nbf.lo < nbf.hi, ==, true);

  // From BFDec — exact in double
  numeric ndec_exact(BFDec("0.5"));
  FACT(ndec_exact.type, ==, numeric::DECIMAL);
  FACT(ndec_exact.lo, ==, 0.5);

  // From BFDec — not exact in double
  numeric ndec(BFDec("0.1"));
  FACT(ndec.type, ==, numeric::DECIMAL);
  FACT(ndec.lo < ndec.hi, ==, true);

  // From unbound
  numeric nu(unbound("x"));
  FACT(nu.type, ==, numeric::UNBOUND);

  numeric nu_anon{unbound()};
  FACT(nu_anon.type, ==, numeric::UNBOUND);
}

// ── numeric assignment ──────────────────────────────────────────────

FACTS(Numeric_Assignment) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  numeric n(0.0);
  FACT(n.type, ==, numeric::DOUBLE);

  n = 42.0;
  FACT(n.type, ==, numeric::DOUBLE);
  FACT(n.lo, ==, 42.0);

  n = BF(3);
  FACT(n.type, ==, numeric::FLOAT);

  n = BFDec("0.1");
  FACT(n.type, ==, numeric::DECIMAL);

  n = unbound("y");
  FACT(n.type, ==, numeric::UNBOUND);

  n = int64_t(7);
  FACT(n.type, ==, numeric::DOUBLE);
  FACT(n.lo, ==, 7.0);
}

// ── numeric comparison ──────────────────────────────────────────────

FACTS(Numeric_Comparison) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // Same type, same value
  numeric a(1.0), b(1.0);
  FACT(a == b, ==, true);
  FACT(a != b, ==, false);

  // Same type, different value
  numeric c(2.0);
  FACT(a < c, ==, true);
  FACT(c > a, ==, true);
  FACT(a <= c, ==, true);
  FACT(c >= a, ==, true);
  FACT(a != c, ==, true);
  FACT(a == c, ==, false);

  // Cross-type: BF(0.5) and double(0.5) — same value, bracket collapses
  numeric bf_half(BF(0.5));
  numeric d_half(0.5);
  // Both have lo==hi==0.5, comparison via doubles
  FACT(bf_half.lo, ==, d_half.lo);
  FACT(bf_half.hi, ==, d_half.hi);

  // Ordering with BFDec
  numeric dec_small(BFDec("0.1"));
  numeric dec_big(BFDec("0.9"));
  FACT(dec_small < dec_big, ==, true);
  FACT(dec_big > dec_small, ==, true);

  // Unbound: always satisfies
  numeric u{unbound()};
  numeric v(1.0);
  FACT(u < v, ==, true);
  FACT(u > v, ==, true);
  FACT(u == v, ==, true);
  FACT(u <= v, ==, true);
  FACT(u >= v, ==, true);
}

// ── numeric sorting ─────────────────────────────────────────────────

FACTS(Numeric_Sorting) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  std::vector<numeric> nums;
  nums.push_back(numeric(3.0));
  nums.push_back(numeric(1.0));
  nums.push_back(numeric(2.0));

  std::sort(nums.begin(), nums.end());

  FACT(nums[0].lo, ==, 1.0);
  FACT(nums[1].lo, ==, 2.0);
  FACT(nums[2].lo, ==, 3.0);

  // Mixed types
  std::vector<numeric> mixed;
  mixed.push_back(numeric(BFDec("2.5")));
  mixed.push_back(numeric(1.0));
  mixed.push_back(numeric(BF(3)));

  std::sort(mixed.begin(), mixed.end());

  FACT(mixed[0].lo, ==, 1.0);
  FACT(mixed[1].lo, ==, 2.5);
  FACT(mixed[2].lo, ==, 3.0);
}

// ── numeric output (<<) ─────────────────────────────────────────────

FACTS(Numeric_Output) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  std::ostringstream oss;

  // Double
  oss.str(""); oss << numeric(42.0);
  FACT(oss.str(), ==, std::string("42"));

  oss.str(""); oss << numeric(0.5);
  FACT(oss.str(), ==, std::string("0.5"));

  // BigFloat suffix
  oss.str(""); oss << numeric(BF(42));
  FACT(oss.str(), ==, std::string("42L"));

  // BigDecimal suffix
  oss.str(""); oss << numeric(BFDec("0.1"));
  FACT(oss.str(), ==, std::string("0.1M"));

  oss.str(""); oss << numeric(BFDec(42));
  FACT(oss.str(), ==, std::string("42M"));

  // Anonymous unbound
  oss.str(""); oss << numeric(unbound());
  FACT(oss.str(), ==, std::string("?"));

  // Named unbound — bare identifier
  oss.str(""); oss << numeric(unbound("x"));
  FACT(oss.str(), ==, std::string("?x"));

  oss.str(""); oss << numeric(unbound("myVar_1"));
  FACT(oss.str(), ==, std::string("?myVar_1"));

  // Named unbound — needs quoting
  oss.str(""); oss << numeric(unbound("has space"));
  FACT(oss.str(), ==, std::string("?\"has space\""));

  oss.str(""); oss << numeric(unbound("Bob's"));
  FACT(oss.str(), ==, std::string("?\"Bob's\""));
}

// ── numeric input (>>) ──────────────────────────────────────────────

FACTS(Numeric_Input) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  numeric n(0.0);

  // Plain double
  { std::istringstream iss("42"); iss >> n;
    FACT(n.type, ==, numeric::DOUBLE);
    FACT(n.lo, ==, 42.0); }

  { std::istringstream iss("-3.14"); iss >> n;
    FACT(n.type, ==, numeric::DOUBLE);
    FACT(n.lo, ==, -3.14); }

  { std::istringstream iss("1e10"); iss >> n;
    FACT(n.type, ==, numeric::DOUBLE);
    FACT(n.lo, ==, 1e10); }

  // BigFloat
  { std::istringstream iss("3.14L"); iss >> n;
    FACT(n.type, ==, numeric::FLOAT); }

  { std::istringstream iss("42l"); iss >> n;
    FACT(n.type, ==, numeric::FLOAT); }

  // BigDecimal
  { std::istringstream iss("0.1M"); iss >> n;
    FACT(n.type, ==, numeric::DECIMAL); }

  { std::istringstream iss("67432.50m"); iss >> n;
    FACT(n.type, ==, numeric::DECIMAL); }

  // BigInt (N suffix -> stored as BF)
  { std::istringstream iss("42N"); iss >> n;
    FACT(n.type, ==, numeric::FLOAT); }

  // Anonymous unbound
  { std::istringstream iss("?"); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(n.rep == nullptr, ==, true); }

  // Named unbound — bare
  { std::istringstream iss("?x"); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(n.rep != nullptr, ==, true);
    FACT(*n.rep, ==, std::string("x")); }

  { std::istringstream iss("?myVar_1"); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("myVar_1")); }

  // Named unbound — quoted
  { std::istringstream iss("?\"hello world\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("hello world")); }

  // Quoted with escapes
  { std::istringstream iss("?\"line\\none\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("line\none")); }

  { std::istringstream iss("?\"tab\\there\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("tab\there")); }

  { std::istringstream iss("?\"q\\\"uote\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("q\"uote")); }

  // Unicode escape \uXXXX
  { std::istringstream iss("?\"\\u0041\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    FACT(*n.rep, ==, std::string("A")); }

  // Unicode escape \u{...}
  { std::istringstream iss("?\"\\u{1F600}\""); iss >> n;
    FACT(n.type, ==, numeric::UNBOUND);
    // U+1F600 = F0 9F 98 80
    FACT(n.rep->size(), ==, size_t(4)); }
}

// ── numeric round-trip (output then input) ──────────────────────────

FACTS(Numeric_RoundTrip) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  auto round_trip = [](const numeric &orig) -> numeric {
    std::ostringstream oss;
    oss << orig;
    std::istringstream iss(oss.str());
    numeric result(0.0);
    iss >> result;
    return result;
  };

  // Double round-trip
  numeric d(42.0);
  numeric d_rt = round_trip(d);
  FACT(d_rt.type, ==, numeric::DOUBLE);
  FACT(d_rt.lo, ==, 42.0);

  // BFDec round-trip
  numeric dec(BFDec("0.1"));
  numeric dec_rt = round_trip(dec);
  FACT(dec_rt.type, ==, numeric::DECIMAL);

  // BF round-trip
  numeric bf(BF(42));
  numeric bf_rt = round_trip(bf);
  FACT(bf_rt.type, ==, numeric::FLOAT);

  // Unbound round-trips
  numeric u_anon{unbound()};
  numeric u_anon_rt = round_trip(u_anon);
  FACT(u_anon_rt.type, ==, numeric::UNBOUND);
  FACT(u_anon_rt.rep == nullptr, ==, true);

  numeric u_named(unbound("xyz"));
  numeric u_named_rt = round_trip(u_named);
  FACT(u_named_rt.type, ==, numeric::UNBOUND);
  FACT(*u_named_rt.rep, ==, std::string("xyz"));
}

// ── Widening correctness ────────────────────────────────────────────

FACTS(Widen_Correctness) {
  BFContext context(256, 34);
  retain<BFContext> use(&context);

  // BF -> BFDec widen preserves value
  BF bf_half(0.5);
  BFDec dec_half = widen<BFDec>(bf_half);
  FACT(dec_half == BFDec("0.5"), ==, true);

  // double -> BF widen
  BF bf_from_d = widen<BF>(0.5);
  FACT(bf_from_d == BF(0.5), ==, true);

  // double -> BFDec widen
  BFDec dec_from_d = widen<BFDec>(0.5);
  FACT(dec_from_d.to_double(), ==, 0.5);

  // Same-type widen is identity
  BF bf_id = widen<BF>(BF(42));
  FACT(bf_id == BF(42), ==, true);
}

FACTS_REGISTER_ALL() {
  FACTS_REGISTER(AboutUniversalUnbound);
  FACTS_REGISTER(AboutNamedUnbound);
  FACTS_REGISTER(NumericVariant_CrossType);
  FACTS_REGISTER(Numeric_Construction);
  FACTS_REGISTER(Numeric_Assignment);
  FACTS_REGISTER(Numeric_Comparison);
  FACTS_REGISTER(Numeric_Sorting);
  FACTS_REGISTER(Numeric_Output);
  FACTS_REGISTER(Numeric_Input);
  FACTS_REGISTER(Numeric_RoundTrip);
  FACTS_REGISTER(Widen_Correctness);
}

FACTS_MAIN
