#pragma once

#include <variant>
#include <memory>
#include <limits>
#include <string>
#include <iostream>
#include <functional>
#include <iomanip>
#include "bf.hpp"
#include "unbound.hpp"

//
// Group double, BF, BFDec and unbound as one numeric type.
//

namespace bellard {

// ── Widening: common type for cross-type operations ─────────────────

template <typename TA, typename TB> struct NumericWiden {
    typedef TA Widest;
};

// too subtle --- will be a problem somewhere
//template <typename TA> struct NumericWiden<TA,unbound> {
//    typedef unbound Widest;
//};

template <> struct NumericWiden<double, BF>    { typedef BF Widest; };
template <> struct NumericWiden<BF, double>    { typedef BF Widest; };
template <> struct NumericWiden<double, BFDec> { typedef BFDec Widest; };
template <> struct NumericWiden<BFDec, double> { typedef BFDec Widest; };
template <> struct NumericWiden<BF, BFDec>     { typedef BFDec Widest; };
template <> struct NumericWiden<BFDec, BF>     { typedef BFDec Widest; };

template <typename To, typename From>
To widen(const From &from) { return To(from); }

//template <>
//unbound widen<unbound,double>(const double &from) { return unbound(); }

//template <>
//unbound widen<unbound,BF>(const BF &from) { return unbound(); }

//template <>
//unbound widen<unbound,BFDec>(const BFDec &from) { return unbound(); }

template <>
inline BFDec widen<BFDec, BF>(const BF &from) {
    limb_t bits = from.c_bf()->len * LIMB_BITS;
    // 1/2^n needs n decimal digits, so use bits as decimal precision
    BFContext wide(bf_alloc(), bf_prec(), bits, bf_rnd());
    retain<BFContext> use(&wide);
    return BFDec(from);
}

// ── numeric_op: type-aware comparison via Op functor ────────────────

// General case: widen both operands to common type, then apply Op
template <typename Op, typename A, typename B>
bool numeric_op(const A &a, const B &b) {
    using W = typename NumericWiden<A, B>::Widest;
    return Op{}(widen<W>(a), widen<W>(b));
}

template<typename Op, typename A>
bool numeric_op(const A &a, const unbound &b) {
  return Op{}(a,b);
}

template<typename Op, typename B>
bool numeric_op(const unbound &a, const B &b) {
  return Op{}(a,b);
}

template<typename Op>
bool numeric_op(const unbound &a, const unbound &b) {
  return Op{}(a,b);
}
// ── Numeric variant ─────────────────────────────────────────────────

using NumericVariant = std::variant<BF, BFDec, double, unbound>;


// Variant dispatch
template <typename Op>
bool numeric_op(const NumericVariant &a, const NumericVariant &b) {
    return std::visit([](const auto &x, const auto &y) -> bool {
        return numeric_op<Op>(x, y);
    }, a, b);
}

inline bool operator<(const NumericVariant &a, const NumericVariant &b)  { return numeric_op<std::less<>>(a, b); }
inline bool operator<=(const NumericVariant &a, const NumericVariant &b) { return numeric_op<std::less_equal<>>(a, b); }
inline bool operator==(const NumericVariant &a, const NumericVariant &b) { return numeric_op<std::equal_to<>>(a, b); }
inline bool operator!=(const NumericVariant &a, const NumericVariant &b) { return numeric_op<std::not_equal_to<>>(a, b); }
inline bool operator>=(const NumericVariant &a, const NumericVariant &b) { return numeric_op<std::greater_equal<>>(a, b); }
inline bool operator>(const NumericVariant &a, const NumericVariant &b)  { return numeric_op<std::greater<>>(a, b); }

inline std::ostream &operator<<(std::ostream &os, const NumericVariant &n) {
    std::visit([&os](const auto &v) { os << v; }, n);
    return os;
}

// ── QJSON type IDs (bitmask layout) ─────────────────────────────────
// Group bits: 0x10=BOOLEAN, 0x20=NUMERIC, 0x40=BLOB, 0x80=STRING,
//             0x100=CONTAINER.  UNBOUND has all bits set.
enum qjson_type : uint16_t {
    QJSON_NULL       = 0x000,  // 0b0'0000'0000
    QJSON_FALSE      = 0x010,  // 0b0'0001'0000
    QJSON_TRUE       = 0x011,  // 0b0'0001'0001
    QJSON_NUMBER     = 0x021,  // 0b0'0010'0001
    QJSON_BIGINT     = 0x022,  // 0b0'0010'0010
    QJSON_BIGFLOAT   = 0x024,  // 0b0'0010'0100
    QJSON_BIGDECIMAL = 0x028,  // 0b0'0010'1000
    QJSON_BLOB       = 0x040,  // 0b0'0100'0000
    QJSON_STRING     = 0x081,  // 0b0'1000'0001
    QJSON_ARRAY      = 0x101,  // 0b1'0000'0001
    QJSON_OBJECT     = 0x102,  // 0b1'0000'0010
    QJSON_UNBOUND    = 0x1FF,  // 0b1'1111'1111

    // Group masks
    QJSON_BOOLEAN    = 0x010,
    QJSON_NUMERIC    = 0x020,
    QJSON_CONTAINER  = 0x100,
};

// ── Bracket: double-interval projection of a numeric ────────────────
// Fast double-precision comparison first; falls back to full-precision
// string reconstruction only when double brackets overlap.

struct numeric {
    // Type aliases for the numeric subset + UNBOUND
    static constexpr qjson_type NUMBER     = QJSON_NUMBER;
    static constexpr qjson_type BIGINT     = QJSON_BIGINT;
    static constexpr qjson_type BIGDECIMAL = QJSON_BIGDECIMAL;
    static constexpr qjson_type BIGFLOAT   = QJSON_BIGFLOAT;
    static constexpr qjson_type UNBOUND    = QJSON_UNBOUND;

    qjson_type type;

    double lo;
    double hi;
    std::unique_ptr<std::string> rep;

    NumericVariant variant() const {
        switch (type) {
        case NUMBER:     return NumericVariant(lo);
        case BIGINT:     return NumericVariant((rep == nullptr) ? BF(lo) : BF(*rep));
        case BIGDECIMAL: return NumericVariant((rep == nullptr) ? BFDec(lo) : BFDec(*rep));
        case BIGFLOAT:   return NumericVariant((rep == nullptr) ? BF(lo) : BF(*rep));
        case UNBOUND:    return NumericVariant((rep == nullptr) ? unbound() : unbound(*rep));
        default:         return NumericVariant(lo); // unreachable
        }
    }

  numeric& set(const BF &bf, qjson_type t = BIGFLOAT) {
    type = t;
    lo = bf.to_double(BF_RNDD);
    hi = bf.to_double(BF_RNDU);
    rep = (lo == hi) ? nullptr : std::make_unique<std::string>(bf.to_string());
    return *this;
  }
  explicit numeric(const BF &bf) { set(bf); }
  numeric& operator=(const BF &bf) { return set(bf); }

  numeric& set(const BFDec &dec) {
    type = BIGDECIMAL;
    lo = dec.to_double(BF_RNDD);
    hi = dec.to_double(BF_RNDU);
    rep = (lo == hi) ? nullptr : std::make_unique<std::string>(dec.to_string());
    return *this;
  }
  explicit numeric(const BFDec &dec) { set(dec); }
  numeric& operator=(const BFDec &dec) { return set(dec); }

  numeric& set(double d) {
    type = NUMBER;
    lo = hi = d;
    rep = nullptr;
    return *this;
  }
  explicit numeric(double d) { set(d); }
  numeric& operator=(double d) { return set(d); }

  numeric& set(int64_t i) {
    if (-(int64_t(1)<<53) <= i && i <= (int64_t(1)<<53)) {
      set(double(i));
    } else {
      set(BF(i));
    }
    return *this;
  }
  explicit numeric(int64_t i) { set(i); }
  numeric& operator=(int64_t i) { return set(i); }

  numeric& set(uint64_t u) {
    if (u <= (uint64_t(1)<<53)) {
      set(double(u));
    } else {
      set(BF(u));
    }
    return *this;
  }

  explicit numeric(uint64_t u) { set(u); }
  numeric& operator=(uint64_t u) { return set(u); }

  numeric& set(const unbound &u) {
    type = UNBOUND;
    lo=-std::numeric_limits<double>::infinity();
    hi= std::numeric_limits<double>::infinity();
    rep = u.universal() ? nullptr : std::make_unique<std::string>(u.name);
    return *this;
  }
  explicit numeric(const unbound &u) { set(u); }
  numeric& operator=(const unbound &u) { return set(u); }

  // QJSON unbound name needs quoting?
  static bool needs_quoting(const std::string &name) {
    if (name.empty()) return false;
    char c = name[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'))
      return true;
    for (size_t i = 1; i < name.size(); ++i) {
      c = name[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '$'))
        return true;
    }
    return false;
  }

  static void write_quoted(std::ostream &out, const std::string &s) {
    out << '"';
    for (unsigned char c : s) {
      if (c == '"') out << "\\\"";
      else if (c == '\\') out << "\\\\";
      else if (c < 0x20) {
        out << "\\u" << std::hex << std::setfill('0')
            << std::setw(4) << (int)c
            << std::dec << std::setfill(' ');
      } else {
        out << c;
      }
    }
    out << '"';
  }

  std::ostream& print(std::ostream &out) const {
    switch (type) {
    case NUMBER:
      out << lo;
      break;
    case BIGINT:
      if (rep == nullptr) { out << lo << "N"; } else { out << *rep << "N"; }
      break;
    case BIGDECIMAL:
      if (rep == nullptr) { out << lo << "M"; } else { out << *rep << "M"; }
      break;
    case BIGFLOAT:
      if (rep == nullptr) { out << lo << "L"; } else { out << *rep << "L"; }
      break;
    case UNBOUND:
      out << '?';
      if (rep != nullptr) {
        if (needs_quoting(*rep)) write_quoted(out, *rep);
        else out << *rep;
      }
      break;
    default:
      break;
    }
    return out;
  }

  // Parse a QJSON numeric literal from a stream
  static bool parse(std::istream &in, numeric &result) {
    in >> std::ws;
    int ch = in.peek();
    if (ch == std::char_traits<char>::eof()) return false;

    // Unbound: ? followed by optional ident or "quoted"
    if (ch == '?') {
      in.get(); // consume '?'
      ch = in.peek();
      if (ch == '"') {
        // quoted name
        in.get(); // consume opening "
        std::string name;
        while (true) {
          ch = in.get();
          if (ch == std::char_traits<char>::eof() || ch == '"') break;
          if (ch == '\\') {
            ch = in.get();
            if (ch == '"') name += '"';
            else if (ch == '\\') name += '\\';
            else if (ch == 'n') name += '\n';
            else if (ch == 'r') name += '\r';
            else if (ch == 't') name += '\t';
            else if (ch == 'b') name += '\b';
            else if (ch == 'f') name += '\f';
            else if (ch == 'u') {
              // \uXXXX or \u{X+}
              ch = in.peek();
              unsigned codepoint = 0;
              if (ch == '{') {
                in.get();
                while (true) {
                  ch = in.get();
                  if (ch == '}' || ch == std::char_traits<char>::eof()) break;
                  codepoint = codepoint * 16;
                  if (ch >= '0' && ch <= '9') codepoint += ch - '0';
                  else if (ch >= 'a' && ch <= 'f') codepoint += 10 + ch - 'a';
                  else if (ch >= 'A' && ch <= 'F') codepoint += 10 + ch - 'A';
                }
              } else {
                for (int i = 0; i < 4; ++i) {
                  ch = in.get();
                  codepoint = codepoint * 16;
                  if (ch >= '0' && ch <= '9') codepoint += ch - '0';
                  else if (ch >= 'a' && ch <= 'f') codepoint += 10 + ch - 'a';
                  else if (ch >= 'A' && ch <= 'F') codepoint += 10 + ch - 'A';
                }
              }
              // encode as UTF-8
              if (codepoint < 0x80) {
                name += (char)codepoint;
              } else if (codepoint < 0x800) {
                name += (char)(0xC0 | (codepoint >> 6));
                name += (char)(0x80 | (codepoint & 0x3F));
              } else if (codepoint < 0x10000) {
                name += (char)(0xE0 | (codepoint >> 12));
                name += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                name += (char)(0x80 | (codepoint & 0x3F));
              } else {
                name += (char)(0xF0 | (codepoint >> 18));
                name += (char)(0x80 | ((codepoint >> 12) & 0x3F));
                name += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                name += (char)(0x80 | (codepoint & 0x3F));
              }
            }
            else name += (char)ch; // unknown escape, pass through
          } else {
            name += (char)ch;
          }
        }
        result.set(unbound(name));
      } else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                 ch == '_' || ch == '$') {
        // bare identifier
        std::string name;
        while (true) {
          ch = in.peek();
          if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '$') {
            name += (char)in.get();
          } else break;
        }
        result.set(unbound(name));
      } else {
        // anonymous: just ?
        result.set(unbound());
      }
      return true;
    }

    // Number: optional '-', digits, optional '.', optional exponent, optional suffix
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
      std::string num;
      if (ch == '-') { num += (char)in.get(); ch = in.peek(); }

      // integer part
      while (ch >= '0' && ch <= '9') { num += (char)in.get(); ch = in.peek(); }

      // fractional part
      if (ch == '.') {
        num += (char)in.get(); ch = in.peek();
        while (ch >= '0' && ch <= '9') { num += (char)in.get(); ch = in.peek(); }
      }

      // exponent
      if (ch == 'e' || ch == 'E') {
        num += (char)in.get(); ch = in.peek();
        if (ch == '+' || ch == '-') { num += (char)in.get(); ch = in.peek(); }
        while (ch >= '0' && ch <= '9') { num += (char)in.get(); ch = in.peek(); }
      }

      // suffix
      ch = in.peek();
      if (ch == 'L' || ch == 'l') {
        in.get();
        result.set(BF(num), BIGFLOAT);
      } else if (ch == 'M' || ch == 'm') {
        in.get();
        result.set(BFDec(num));
      } else if (ch == 'N' || ch == 'n') {
        in.get();
        result.set(BF(num), BIGINT);
      } else {
        // plain double
        char *end;
        double d = std::strtod(num.c_str(), &end);
        result.set(d);
      }
      return true;
    }

    return false; // not a numeric
  }

};

inline std::istream& operator>>(std::istream &in, numeric &x) {
  numeric::parse(in, x);
  return in;
}

inline bool operator<(const numeric &x, const numeric &y) {
    return (x.lo < y.hi) && (x.hi < y.lo || x.variant() < y.variant());
}

inline bool operator>(const numeric &x, const numeric &y) {
    return (x.hi > y.lo) && (x.lo > y.hi || x.variant() > y.variant());
}

inline bool operator<=(const numeric &x, const numeric &y) {
    return (x.lo <= y.hi) && (x.hi <= y.lo || x.variant() <= y.variant());
}

inline bool operator>=(const numeric &x, const numeric &y) {
    return (x.hi >= y.lo) && (x.lo >= y.hi || x.variant() >= y.variant());
}

inline bool operator!=(const numeric &x, const numeric &y) {
  return (x.lo < y.hi || y.lo < x.hi) && ((x.rep == nullptr && y.rep == nullptr) || x.variant() != y.variant());
}

inline bool operator==(const numeric &x, const numeric &y) {
  return (x.lo <= y.hi && y.lo <= x.hi) && ((x.rep == nullptr && y.rep == nullptr) || x.variant() == y.variant());
}


inline std::ostream& operator<<(std::ostream &out, const numeric &x) {
  return x.print(out);
}

} // namespace bellard
