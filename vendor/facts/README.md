![CI](https://github.com/wmacevoy/facts/actions/workflows/c-cpp.yml/badge.svg)

# Facts

Facts is a tiny, beginner‑friendly C/C++ testing library. It aims to be easy to drop into small programs without build system friction.

## What you get

- **C core**: `FACTS(name)` groups and `FACT(a,op,b)` checks with readable failure messages.
- **C++ extras**: optional tracing (`FACTS_TRACE`, `FACTS_WATCH*`) to help debug failures.
- **No heavy deps**: just compile the provided `.c`/`.cpp` files with your code. An installable shared library + `pkg-config` file is provided too.

## Quick start

1) Add the files beside your sources

- C only: download `include/facts.h` and `src/facts.c`.
- C++: also add `src/facts.cpp` for tracing helpers.

2) Include the header

```C
#include "facts.h"
```

3) Write facts

```C
int inc(int x) { return x + 1; }

FACTS(AboutInc) {
  int a = 5, b = 6;
  FACT(inc(a),==,b);
}

FACTS_MAIN
```

4) Build and run

```sh
# C — build and test in one line
cc -std=c11 -g -o check your.c facts.c && ./check --facts_find your.c

# C++
c++ -std=c++11 -g -o check your.cpp facts.cpp && ./check --facts_find your.cpp
```

`--facts_find` scans source files for `FACTS()` declarations, registers them at runtime via `dlsym`, and runs the tests. No `FACTS_REGISTER_ALL` block needed.

Tip: after `make install`, you can also use `pkg-config`:

```sh
c++ -std=c++11 -g $(pkg-config --cflags facts) -o check your.cpp $(pkg-config --libs facts)
```

## Writing facts

- `FACTS(Name) { ... }` declares a group. Each group compiles to `void facts_Name_function(Facts*)`.
- `FACT(a,op,b)` checks a relation (e.g., `==`, `!=`, `<`, `>=`). On failure it prints the expressions and values and returns from the current `FACTS` group.
- `FACT_CHECK(a,op,b)` behaves like `FACT` but does not early‑return; it evaluates to 1 on success, 0 on failure.

Floating‑point checks: use the helpers for clarity.

```C
FACTS(AboutFloats) {
  double a = 0.1 * 7, b = 0.7;
  FACT(FactsAbsErr(a,b),<,1e-12);
  FACT(FactsRelErr(a,b),<,1e-12);
}
```

Important: on a failure the values of `a` and `b` are printed, which means each side is evaluated twice in that case. Avoid side effects (like `++x`) in `FACT(...)` expressions.

## Registration and main()

The simplest approach: use `--facts_find` at runtime to discover and run tests from source files. Just add `FACTS_MAIN` to your file — no registration block needed:

```sh
./check --facts_find your.c
```

For baked-in registration (no source files needed at runtime), add a `FACTS_REGISTER_ALL` block:

```C
FACTS_REGISTER_ALL() {
  FACTS_REGISTER(AboutInc);
  FACTS_REGISTER(AnotherGroup);
}

FACTS_MAIN
```

Use `--facts_register_all` to generate this block from whatever is currently registered:

```sh
./check --facts_find src/*.c \; --facts_register_all
```

You can also gate running facts behind a command‑line switch:

```C
FACTS_REGISTER_ALL() { FACTS_REGISTER(MyTest); }

FACTS_MAIN_IF(--facts) {
  /* your program's main */
  return 0;
}
```

## Command‑line options

Printed by `--facts_help`:

```
default is to check all registered facts
    --facts_include="*pattern*"   include matching groups
    --facts_exclude="*pattern*"   exclude matching groups
    --facts_find files... [\;]    register facts from source files
    --facts_register_all          print FACTS_REGISTER_ALL block
    --facts_skip                  do not run facts
    --facts_junit                 emit JUnit XML to stdout
    --facts_plain                 disable ANSI colors
    --facts_unsafe                run directly (skip fork/exec wrapper)
    --facts_timeout=N             timeout in seconds (default 10)
```

Exit code: 0 if no failures, 1 if any `FACT` failed. (If no facts ran, the exit code is 0.)

## Safe execution

By default, facts fork/exec's itself as a child process before running tests. This provides:

- **Crash isolation** — segfaults, aborts, and other signals in tests are caught and reported cleanly instead of crashing your terminal.
- **Timeout** — tests that hang are killed after 10 seconds (configurable with `--facts_timeout=N`).
- **Niceness** — the child process runs at lower priority (`nice 10`) so runaway tests don't starve your system.

Pass `--facts_unsafe` to skip the wrapper and run tests directly in the current process (useful for debugging under gdb/lldb).

## C++ extras

Use tracing to annotate failures with breadcrumbs (last‑in, first‑out):

```C++
FACTS(ExampleTrace) {
  for (int i=0; i<3; ++i) {
    FACTS_TRACE("i=" << i);
    // ...
  }
}
```

Conditional breakpoints via gdb hints:

```C++
FACTS(Scan) {
  int i=1, j=2;
  FACTS_WATCH(i,j);
  // Failure text includes:
  // (gdb) break facts_Scan_function:facts_trace_0 if i==1 && j==2
}
```

## Examples

- `examples/hello_c/` shows a pure‑C setup (`facts.c` is required).
- `examples/hello_cpp/` shows C++ with tracing (`facts.cpp` plus `facts.c`).

## Notes and limitations

- ANSI colors are used by default; pass `--facts_plain` to disable.

Enjoy your fact checking!
