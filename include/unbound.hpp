#pragma once

#include <string>
#include <iostream>

//  unbound: 
//  - An unnamed (empty string) unbound is a universal unknown: ?
//  - ? relation ? is true for any relation {==,!=,<,<=,>,>=}
//  - ?x relation ?x is false for {!=,<,>}
//  - any other comparision is true.
//
//  BF: BigFloat (lightweight wrapper around Febrice Ballard's bf_t in libbf).
//
//  BFDec: BigDecimal (lightweight wrapper around Febrice Ballard's bf_dec_t in libbf).
//
//  double: IEEE 754 8 byte double.
//

namespace bellard {

struct unbound {
  const std::string name;
  unbound(const std::string &_name = "") : name(_name) {}
  bool universal() const { return name.size() == 0; }
  bool unequal(const unbound &to) const {
    return universal() || to.universal() || name != to.name;
  }
};

// non-universal unbounds of the same name are equal,
// so <, > and != might be false, all other relations are universally true.
  
inline bool operator!=(const unbound &a, const unbound &b) { return a.unequal(b); }
inline bool operator<(const unbound &a, const unbound &b) { return a.unequal(b); }
inline bool operator>(const unbound &a, const unbound &b) { return a.unequal(b); }

inline bool operator==(const unbound &a, const unbound &b) { return true; }
inline bool operator<=(const unbound &a, const unbound &b) { return true; }
inline bool operator>=(const unbound &a, const unbound &b) { return true; }

inline std::ostream &operator<<(std::ostream &os, const unbound &u) {
  return os << "?" << u.name;
}

template <typename T>
inline bool operator<(const unbound &, const T &) { return true; }

template <typename T>
inline bool operator<(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator<=(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator<=(const unbound &, const T &) { return true; }

template <typename T>
inline bool operator!=(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator!=(const unbound &, const T &) { return true; }

template <typename T>
inline bool operator==(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator==(const unbound &, const T &) { return true; }

template <typename T>
inline bool operator>=(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator>=(const unbound &, const T &) { return true; }

template <typename T>
inline bool operator>(const T &, const unbound &) { return true; }

template <typename T>
inline bool operator>(const unbound &, const T &) { return true; }

} // namespace bellard
