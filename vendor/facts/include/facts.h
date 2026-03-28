#pragma once

#ifdef __cplusplus
#include <iostream>
extern "C"
{
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>

  struct FactsStruct;
  typedef struct FactsStruct Facts;

#define FACTS_GREEN ((FACTS_COLOR) ? "\033[1;32m" : "" )
#define FACTS_RED ((FACTS_COLOR) ? "\033[1;31m" : "")
#define FACTS_RESET ((FACTS_COLOR) ? "\033[0m" : "")

#define FACTS_STATE_EXCLUDE -2
#define FACTS_STATE_FAIL -1
#define FACTS_STATE_INCLUDE 0
#define FACTS_STATE_PASS 1

#define FACTS_CONSOLE (((facts_format) & (1<<0)) == 0) 
#define FACTS_JUNIT   (((facts_format) & (1<<0)) != 0) 
#define FACTS_COLOR   (((facts_format) & 3) == 0)

  struct FactsStruct
  {
    const char *file;
    int line;
    const char *name;
    void (*function)(Facts *facts);
    int status;
    Facts *next;
    Facts *prev;
  };

  void FactsPrint(const char *fmt, ...);
  void FactsInclude(const char *pattern);
  void FactsExclude(const char *pattern);
  void FactsRegister(Facts *facts);
  void FactsCheck();
  void FactsFiction(const char *file, int line, Facts *facts,
		    const char *a, const char *op, const char *b);

  double FactsAbsErr(double a, double b);
  double FactsRelErr(double a, double b);

  int FactsMain(int argc, const char *argv[]);

// https://stackoverflow.com/questions/24844970/how-to-print-types-of-unknown-size-like-ino-t
#define FACTS_PRINT_FORMAT(X) _Generic((X),                    \
                                       _Bool                   \
                                       : "%d",                 \
                                         char                  \
                                       : "%c",                 \
                                         unsigned char         \
                                       : "%hhu",               \
                                         unsigned short        \
                                       : "%hu",                \
                                         unsigned int          \
                                       : "%u",                 \
                                         unsigned long         \
                                       : "%lu",                \
                                         unsigned long long    \
                                       : "%llu",               \
                                         signed char           \
                                       : "%hhd",               \
                                         short                 \
                                       : "%hd",                \
                                         int                   \
                                       : "%d",                 \
                                         long                  \
                                       : "%ld",                \
                                         long long             \
                                       : "%lld",               \
                                         float                 \
                                       : "%g",                 \
                                         double                \
                                       : "%g",                 \
                                         long double           \
                                       : "%Lg",                \
                                         const char *          \
                                       : "%s",                 \
                                         const unsigned char * \
                                       : "%s",                 \
                                         const void *          \
                                       : "%p",                 \
                                         char *                \
                                       : "%s",                 \
                                         unsigned char *       \
                                       : "%s",                 \
                                         void *                \
                                       : "%p")

#if !(defined(FACTS_C) || defined(FACTS_CPP))
  extern uint64_t facts_fictions;
  extern uint64_t facts_truths;
  extern int facts_format;
#endif

#define FACT_CHECK_PRINT(a, op, b, fmt) (((a)op(b)) ? (++facts_truths, 1) : (FactsPrint("%s/%s %d: %s%s {=%?} " #op " %s {=%?} is fiction%s\n", fmt, fmt, __FILE__, facts->name, __LINE__, FACTS_RED, #a, (a), #b, (b), FACTS_RESET), FactsFiction(__FILE__, __LINE__, facts, #a, #op, #b), facts->status = -1, ++facts_fictions, 0))
#define FACT_PRINT(a, op, b, fmt)  \
  if (!FACT_CHECK_PRINT(a, op, b, fmt)) \
  return

#define FACT_CHECK_CERR(a, op, b) (((a)op(b)) ? (++facts_truths, 1) : (std::cout  << __FILE__ << "/" << facts->name << " " << __LINE__ << ": " << FACTS_RED << #a << " {=" << (a) << "} " << #op << " " << #b << " {=" << (b) << "} is fiction" << FACTS_RESET << std::endl, FactsFiction(__FILE__, __LINE__, facts, #a, #op, #b), facts->status = -1, ++facts_fictions, 0))
#define FACT_CERR(a, op, b)  \
  if (!FACT_CHECK_CERR(a, op, b)) \
  return

#ifdef __cplusplus
#define FACT(a, op, b) FACT_CERR(a, op, b)
#define FACT_CHECK(a, op, b) FACT_CHECK_CERR(a, op, b)
#define FACTS_EXTERN extern "C"
#else
#define FACT(a, op, b) FACT_PRINT(a, op, b, FACTS_PRINT_FORMAT(a))
#define FACT_CHECK(a, op, b) FACT_CHECK_PRINT(a, op, b, FACTS_PRINT_FORMAT(a))
#define FACTS_EXTERN
#endif

#define FACTS_DECLARE(name, state)                                                                    \
  void facts_##name##_function(Facts *facts);                                                           \
  Facts facts_##name##_data = {__FILE__, __LINE__, #name, &facts_##name##_function, state, NULL, NULL}; \
  void facts_##name##_function(Facts *facts)

#define FACTS_INCLUDE(name) FACTS_DECLARE(name, FACTS_STATE_INCLUDE)
#define FACTS_EXCLUDE(name) FACTS_DECLARE(name, FACTS_STATE_EXCLUDE)
#define FACTS(name) FACTS_INCLUDE(name)

#define FACTS_REGISTER(name) FactsRegister(&facts_##name##_data)

#define FACTS_REGISTER_ALL                \
  FACTS_EXTERN void FactsRegisterAll

#define FACTS_MAIN 						\
  int main(int argc, const char *argv[]) { return FactsMain(argc, argv); }

#define FACTS_MAIN_IF(check)						\
  int FactsMainElse(int argc, const char *argv[]);			\
  int main(int argc, const char *argv[]) {				\
    const char *facts=#check;						\
    for (int argi=1; argi<argc; ++argi) {				\
      const char *arg=argv[argi];					\
      for (int c=0; arg[c] == facts[c]; ++c) {				\
        if (arg[c] == 0) return FactsMain(argc, argv);			\
      }									\
    }									\
    return FactsMainElse(argc,argv);					\
  }									\
  int FactsMainElse(int argc, const char *argv[])
    
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <functional>
#include <iostream>
 
  class FactsTrace
  {
    private: static FactsTrace *top;
    private: static Facts *facts;
    private: FactsTrace *up;
    private: std::function<void(Facts *facts, std::ostream&)> note;

    public: static void reset(Facts *_facts);
    public: static void notes();

    FactsTrace(std::function<void(Facts *facts, std::ostream&)> _note);
    ~FactsTrace();
  };

  #define FACTS_TRACE_SUFFIX(note,suffix) \
    FactsTrace facts_trace##suffix([=](Facts *facts, std::ostream&out) { out << __FILE__ << "/" << facts->name << ":" << __LINE__ << " " << note << std::endl; }); \
    facts_trace_##suffix:
  #define FACTS_TRACE_COUNT(note,counter) FACTS_TRACE_SUFFIX(note,counter)
  #define FACTS_TRACE(note) FACTS_TRACE_COUNT(note,__COUNTER__)

  #define FACTS_WATCH_SUFFIX(note,suffix) FACTS_TRACE_SUFFIX(FACTS_GREEN << "(gdb)" << FACTS_RESET << " break facts_" << facts->name << "_function:facts_trace_" << suffix << " if " << note, suffix)
  #define FACTS_WATCH_COUNT(note,counter) FACTS_WATCH_SUFFIX(note,counter)
  #define FACTS_WATCH_IF(watch) FACTS_WATCH_COUNT(watch,__COUNTER__)

  // internal: ostream chain — do not call directly
  #define FACTS_OSTREAM1(x)       #x << "==" << (x)
  #define FACTS_OSTREAM2(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM1(__VA_ARGS__)
  #define FACTS_OSTREAM3(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM2(__VA_ARGS__)
  #define FACTS_OSTREAM4(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM3(__VA_ARGS__)
  #define FACTS_OSTREAM5(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM4(__VA_ARGS__)
  #define FACTS_OSTREAM6(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM5(__VA_ARGS__)
  #define FACTS_OSTREAM7(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM6(__VA_ARGS__)
  #define FACTS_OSTREAM8(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM7(__VA_ARGS__)
  #define FACTS_OSTREAM9(x, ...)  #x << "==" << (x) << " && " << FACTS_OSTREAM8(__VA_ARGS__)
  #define FACTS_OSTREAM10(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM9(__VA_ARGS__)
  #define FACTS_OSTREAM11(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM10(__VA_ARGS__)
  #define FACTS_OSTREAM12(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM11(__VA_ARGS__)
  #define FACTS_OSTREAM13(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM12(__VA_ARGS__)
  #define FACTS_OSTREAM14(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM13(__VA_ARGS__)
  #define FACTS_OSTREAM15(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM14(__VA_ARGS__)
  #define FACTS_OSTREAM16(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM15(__VA_ARGS__)
  #define FACTS_OSTREAM17(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM16(__VA_ARGS__)
  #define FACTS_OSTREAM18(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM17(__VA_ARGS__)
  #define FACTS_OSTREAM19(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM18(__VA_ARGS__)
  #define FACTS_OSTREAM20(x, ...) #x << "==" << (x) << " && " << FACTS_OSTREAM19(__VA_ARGS__)

  // internal: argument count dispatch
  #define FACTS_ARGC_(                                                   \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,                                     \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N
  #define FACTS_ARGC(...) FACTS_ARGC_(__VA_ARGS__,                       \
    20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
  #define FACTS_PASTE_(a,b) a##b
  #define FACTS_PASTE(a,b)  FACTS_PASTE_(a,b)

  // Conditional gdb breakpoint hint — accepts 1 to 20 variables.
  #define FACTS_WATCH(...)                                               \
    FACTS_WATCH_IF(FACTS_PASTE(FACTS_OSTREAM,FACTS_ARGC(__VA_ARGS__))(__VA_ARGS__))

#endif
