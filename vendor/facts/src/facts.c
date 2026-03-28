// _GNU_SOURCE must come before any includes (needed for RTLD_DEFAULT on glibc)
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define dup    _dup
#define dup2   _dup2
#define close  _close
#define lseek  _lseek
#ifndef fileno
#define fileno _fileno
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
#else
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/wait.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)0)
#endif
#endif

#define FACTS_C 1
#include "facts.h"
static Facts *head = NULL, *tail = NULL;

// These must have C linkage to match the extern declarations in facts.h
// (which are inside extern "C"). Without this, MSVC mangles the names
// differently when facts.c is compiled as C++ via facts.cpp.
#ifdef __cplusplus
extern "C" {
#endif
uint64_t facts_fictions = 0;
uint64_t facts_truths = 0;
int facts_format = 0;
#ifdef __cplusplus
}
#endif

// Weak default so the shared library links without user code.
// User-provided strong definition (from FACTS_REGISTER_ALL)
// overrides at link time.
#if defined(__GNUC__) || defined(__clang__)
FACTS_EXTERN __attribute__((weak)) void FactsRegisterAll() {}
#elif defined(_MSC_VER)
FACTS_EXTERN void FactsRegisterAll_default() {}
#pragma comment(linker, "/alternatename:FactsRegisterAll=FactsRegisterAll_default")
FACTS_EXTERN void FactsRegisterAll();
#else
FACTS_EXTERN void FactsRegisterAll();
#endif

// Write len bytes to stdout with XML entity escaping.
static void FactsXmlEscape(const char *s, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    switch (s[i]) {
      case '<':  printf("&lt;");   break;
      case '>':  printf("&gt;");   break;
      case '&':  printf("&amp;");  break;
      case '"':  printf("&quot;"); break;
      default:   putchar(s[i]);    break;
    }
  }
}

static void FactsXmlEscapeStr(const char *s) {
  FactsXmlEscape(s, strlen(s));
}

// Wildcard pattern matcher.
//
// Desiged to work on small systems with possibly no regex library.
//
//
// 5+2*sizeof(void*)+strlen(pattern)/4
//
// bytes of (stack) memory.
FACTS_EXTERN int FactsMatches(const char *str, const char *pattern)
{
  size_t np = strlen(pattern);

  // fast exit for match-all pattern
  if (np == 1 && pattern[0] == '*')
    return 1;

  size_t ns = strlen(str);

  size_t nb = (np + 2) / 8 + ((np + 2) % 8 != 0);
  size_t k;

  uint8_t *state0 = (uint8_t *)calloc(nb, 1);
  uint8_t *state1 = (uint8_t *)calloc(nb, 1);

  if (!state0 || !state1) { free(state0); free(state1); return 0; }
  state0[0] = 1;
  for (k = 1; pattern[k - 1] == '*'; ++k)
    state0[k / 8] |= (1 << (k % 8));

  for (size_t i = 0; i <= ns; ++i)
  {
    unsigned char c = str[i];

    memset(state1, 0, nb);
    for (size_t j = 0; j <= np; ++j)
    {
      if (state0[j / 8] & (1 << (j % 8)))
      {
        if (pattern[j] == '*')
        {
          k = j;
          state1[k / 8] |= (1 << (k % 8));
          ++k;
          state1[k / 8] |= (1 << (k % 8));
        }
        else if (pattern[j] == c)
        {
          k = j + 1;
          state1[k / 8] |= (1 << (k % 8));
          while (pattern[k] == '*')
          {
            ++k;
            state1[k / 8] |= (1 << (k % 8));
          }
        }
      }
    }

    uint8_t *tmp = state0;
    state0 = state1;
    state1 = tmp;
  }

  k = np + 1;
  int result = (state0[k / 8] & (1 << (k % 8))) != 0;
  free(state0);
  free(state1);
  return result;
}

// Fiction break point.
//
// This function is called whenever a FACT is actually
// fiction.  It counts the number of calls mostly so
// the optimizer will not remove it.
//
// It is really provided as an easy debug break point when
// tracing a fact check that fails.

FACTS_EXTERN void FactsFiction(const char *file, int line, Facts *facts,
			       const char *a, const char *op, const char *b)
{
  printf("%s(gdb)%s break facts_%s_function\n",FACTS_GREEN,FACTS_RESET,facts->name);
  #ifdef __cplusplus
      FactsTrace::notes();
  #endif
  printf("%s(gdb)%s run --facts_include=%s\n",FACTS_GREEN,FACTS_RESET,facts->name);
}

// Include FACTS to check with wildncard pattern.

FACTS_EXTERN void FactsInclude(const char *pattern)
{
  if (head == NULL)
  {
    FactsRegisterAll();
    for (Facts *facts = head; facts != NULL; facts = facts->next)
    {
      if (facts->status == FACTS_STATE_INCLUDE)
      {
        facts->status = FACTS_STATE_EXCLUDE;
      }
    }
  }
  for (Facts *facts = head; facts != NULL; facts = facts->next)
  {
    if (facts->status == FACTS_STATE_EXCLUDE && FactsMatches(facts->name, pattern))
    {
      facts->status = FACTS_STATE_INCLUDE;
    }
  }
}

// Exclude facts with wildcard pattern.
// Normally all FACTS are checked.
FACTS_EXTERN void FactsExclude(const char *pattern)
{
  if (head == NULL)
  {
    FactsRegisterAll();
  }
  for (Facts *facts = head; facts != NULL; facts = facts->next)
  {
    if (facts->status == FACTS_STATE_INCLUDE && FactsMatches(facts->name, pattern))
    {
      facts->status = FACTS_STATE_EXCLUDE;
    }
  }
}

FACTS_EXTERN void FactsRegister(Facts *facts)
{
  if (facts->prev == NULL && facts->next == NULL && facts != head)
  {
    facts->prev = tail;
    facts->next = NULL;
    if (tail != NULL)
    {
      tail->next = facts;
    }
    if (head == NULL)
    {
      head = facts;
    }
    tail = facts;
  }
}

// Fact find (internals).
//
// C does not provide a way to initialize a list of
// FACTS checks.  So we search memory for them by the
// Symmetric relative error.
FACTS_EXTERN double FactsRelErr(double a, double b)
{
  double abserr = a >= b ? a - b : b - a;
  a = a >= 0 ? a : -a;
  b = b >= 0 ? b : -b;
  double maxabs = a >= b ? a : b;
  if (maxabs == 0.0) return abserr;
  return abserr / maxabs;
}

// Absolute error.
FACTS_EXTERN double FactsAbsErr(double a, double b)
{
  double abserr = a >= b ? a - b : b - a;
  return abserr;
}

// Print with dynamic formats.
// format can contain %? patterns.  These
// are replaced with strings in the va-args
// list FIRST.  After the %? substitution,
// the remaing argmuments are processed with
// the modified format string is passed to
// vfprintf.
FACTS_EXTERN void FactsPrint(const char *format, ...)
{
  int i, j;
  int reformatSize = strlen(format) + 1;

  va_list ap;
  va_start(ap, format);
  for (i = 0, j = 0; format[i] != 0;)
  {
    if (format[i] == '%' && format[i + 1] == '?')
    {
      const char *fs = va_arg(ap, const char *);
      reformatSize += strlen(fs);
      i += 2;
    }
    else
    {
      i += 1;
    }
  }

  char *reformat = (char *)malloc(reformatSize);
  if (!reformat) { va_end(ap); return; }
  va_end(ap);
  va_start(ap, format);
  for (i = 0, j = 0; format[i] != 0;)
  {
    if (format[i] == '%' && format[i + 1] == '?')
    {
      const char *fs = va_arg(ap, const char *);
      strncpy(reformat + j, fs, reformatSize - j);
      j += strlen(fs);
      i += 2;
    }
    else
    {
      reformat[j] = format[i];
      ++j;
      ++i;
    }
  }
  reformat[j] = 0;
  vprintf(reformat, ap);
  va_end(ap);
  free(reformat);
}

#define FACTS_BLOCKSIZE 1024
// Execute facts checks.
//
// You can preceed this with FactsInclude and FactsExlude to pick out
// a particular set.
//
//
FACTS_EXTERN void FactsCheck()
{
  if (head == NULL)
  {
    FactsRegisterAll();
  }

  FILE *tmpout = NULL, *tmperr = NULL;
  int oldout = -1, olderr = -1;
  if (FACTS_JUNIT)
  {
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    printf("<testsuite name=\"facts\">\n");
  }
  for (Facts *facts = head; facts != NULL; facts = facts->next)
  {
    if (FACTS_JUNIT)
    {
      printf("<testcase name=\"");
      FactsXmlEscapeStr(facts->name);
      printf("\">\n");
      fflush(stdout);
      fflush(stderr);
      tmpout = tmpfile();
      tmperr = tmpfile();
      if (tmpout != NULL && tmperr != NULL) {
        oldout = dup(STDOUT_FILENO);
        olderr = dup(STDERR_FILENO);
        if (oldout >= 0 && olderr >= 0) {
          dup2(fileno(tmpout), STDOUT_FILENO);
          dup2(fileno(tmperr), STDERR_FILENO);
        }
      }
    }
    if (facts->status == FACTS_STATE_INCLUDE)
    {
      printf("%s %d: %s facts check started\n",
             facts->file, facts->line, facts->name);
#ifdef __cplusplus
      FactsTrace::reset(facts);
#endif
      facts->function(facts);
      if (facts->status == FACTS_STATE_INCLUDE)
      {
        facts->status = FACTS_STATE_PASS;
      }
      if (facts->status == FACTS_STATE_FAIL) {
        printf("%s %d: %s facts check ended %sbadly%s\n",
               facts->file, facts->line, facts->name, FACTS_RED, FACTS_RESET);
      } else {
        printf("%s %d: %s facts check ended\n",
               facts->file, facts->line, facts->name);
      }
    }
    else
    {
      printf("%s %d: %s facts check %sexcluded%s.\n",
             facts->file, facts->line, facts->name, FACTS_RED, FACTS_RESET);
    }
    if (FACTS_JUNIT)
    {
      fflush(stdout);
      fflush(stderr);
      if (tmpout != NULL && tmperr != NULL && oldout >= 0 && olderr >= 0) {
        int64_t outlen = lseek(STDOUT_FILENO, 0, SEEK_CUR);
        int64_t errlen = lseek(STDERR_FILENO, 0, SEEK_CUR);
        dup2(oldout, STDOUT_FILENO);
        dup2(olderr, STDERR_FILENO);
        close(oldout);
        close(olderr);
        oldout = olderr = -1;
        if (outlen > 0) {
          fseek(tmpout, 0L, SEEK_SET);
          printf("<system-out>");
          for (int64_t p = 0; p < outlen; p += FACTS_BLOCKSIZE)
          {
            char data[FACTS_BLOCKSIZE];
            int n = (int)(outlen - p);
            if (n > FACTS_BLOCKSIZE) n = FACTS_BLOCKSIZE;
            if (fread(data, n, 1, tmpout) == 1)
              FactsXmlEscape(data, n);
          }
          printf("</system-out>\n");
        }
        if (errlen > 0) {
          fseek(tmperr, 0L, SEEK_SET);
          printf("<system-err>");
          for (int64_t p = 0; p < errlen; p += FACTS_BLOCKSIZE)
          {
            char data[FACTS_BLOCKSIZE];
            int n = (int)(errlen - p);
            if (n > FACTS_BLOCKSIZE) n = FACTS_BLOCKSIZE;
            if (fread(data, n, 1, tmperr) == 1)
              FactsXmlEscape(data, n);
          }
          printf("</system-err>\n");
        }
      }
      if (facts->status == FACTS_STATE_EXCLUDE)
      {
        printf("<skipped />\n");
      }
      if (facts->status == FACTS_STATE_FAIL)
      {
        printf("<failure>See stdout</failure>\n");
      }
      printf("</testcase>\n\n");
      if (tmpout != NULL) fclose(tmpout);
      if (tmperr != NULL) fclose(tmperr);
      tmpout = tmperr = NULL;
    }
  }

  if (FACTS_CONSOLE)
  {
    printf("facts summary.\n");
    for (Facts *facts = head; facts != NULL; facts = facts->next)
    {
      if (facts->status == FACTS_STATE_PASS)
      {
        printf("facts check %s %spassed%s\n", facts->name, FACTS_GREEN, FACTS_RESET);
      }
    }
    for (Facts *facts = head; facts != NULL; facts = facts->next)
    {
      if (facts->status == FACTS_STATE_FAIL)
      {
        printf("facts check %s %sfailed%s\n", facts->name, FACTS_RED, FACTS_RESET);
      }
    }
    for (Facts *facts = head; facts != NULL; facts = facts->next)
    {
      if (facts->status == FACTS_STATE_EXCLUDE)
      {
        printf("facts check %s %sexcluded%s\n", facts->name, FACTS_RED, FACTS_RESET);
      }
    }
    for (Facts *facts = head; facts != NULL; facts = facts->next)
    {
      if (facts->status != FACTS_STATE_PASS &&
          facts->status != FACTS_STATE_FAIL &&
          facts->status != FACTS_STATE_EXCLUDE)
      {
        printf("facts check %s status %d\n", facts->name, facts->status);
      }
    }
    double checks = ((double)facts_truths) + ((double)facts_fictions);
    double rate = 100.0 / (checks > 0.0 ? checks : 1.0);
    printf("%" PRIu64 " (%1.1f%%) truths and %" PRIu64 " (%1.1f%%) fictions checked.\n",
           facts_truths, facts_truths * rate, facts_fictions, facts_fictions * rate);
  }

  if (FACTS_JUNIT)
  {
    printf("</testsuite>\n");
  }
}

//
// You can call this from your main to process facts
// checks.  Return status 0 is good, it means a fact check was
// called and passed all (at least one) FACT check, 1 means at
// least one FACT failed, and 2 means no facts were checked.
//
//
// Look up a Facts struct by name using dynamic symbol lookup.
// Returns NULL if not found (e.g. not compiled into this binary).
static Facts *FactsLookup(const char *name) {
  char sym[256];
  snprintf(sym, sizeof(sym), "facts_%s_data", name);
#ifdef _WIN32
  return (Facts *)GetProcAddress(GetModuleHandle(NULL), sym);
#else
  return (Facts *)dlsym(RTLD_DEFAULT, sym);
#endif
}

// Scan a source file for FACTS(Name) declarations.
// If register_mode, look up and register each test found.
// Otherwise, print FACTS_REGISTER(Name) lines to stdout.
static void FactsScanFile(const char *filename, int register_mode) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "facts: cannot open %s\n", filename);
    return;
  }

  char line[4096];
  while (fgets(line, sizeof(line), f)) {
    // skip preprocessor directives (#define, #include, etc.)
    const char *trimmed = line;
    while (*trimmed == ' ' || *trimmed == '\t') ++trimmed;
    if (*trimmed == '#') continue;

    const char *p = line;
    while ((p = strstr(p, "FACTS(")) != NULL) {
      // word boundary: previous char must not be identifier char
      if (p > line) {
        char c = p[-1];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_') {
          p += 6;
          continue;
        }
      }

      // extract identifier
      const char *start = p + 6;
      const char *end = start;
      while ((*end >= 'A' && *end <= 'Z') || (*end >= 'a' && *end <= 'z') ||
             (*end >= '0' && *end <= '9') || *end == '_')
        ++end;

      if (*end == ')' && end > start) {
        int len = (int)(end - start);
        if (register_mode) {
          char name[256];
          if (len < (int)sizeof(name)) {
            memcpy(name, start, len);
            name[len] = '\0';
            Facts *facts = FactsLookup(name);
            if (facts) {
              FactsRegister(facts);
            } else {
              fprintf(stderr, "facts: %s not found in executable\n", name);
            }
          }
        } else {
          printf("    FACTS_REGISTER(%.*s);\n", len, start);
        }
      }

      p = (end > p) ? end : p + 1;
    }
  }

  fclose(f);
}

// Re-exec self as a child process with --facts_unsafe, monitoring for
// crashes and timeouts.  Returns the exit code to propagate.
static int FactsExecSafe(int argc, const char *argv[], int timeout) {
  // Build new argv: argv[0] --facts_unsafe [rest of argv...]
  const char **child_argv = (const char **)malloc((argc + 2) * sizeof(const char *));
  if (!child_argv) return 2;
  child_argv[0] = argv[0];
  child_argv[1] = "--facts_unsafe";
  for (int i = 1; i < argc; ++i)
    child_argv[i + 1] = argv[i];
  child_argv[argc + 1] = NULL;

#ifdef _WIN32
  // Build command line string for CreateProcess
  char cmdline[32768];
  int pos = 0;
  for (int i = 0; child_argv[i] && pos < (int)sizeof(cmdline) - 2; ++i) {
    if (i > 0) cmdline[pos++] = ' ';
    // Quote each argument
    cmdline[pos++] = '"';
    for (const char *p = child_argv[i]; *p && pos < (int)sizeof(cmdline) - 3; ++p) {
      if (*p == '"') cmdline[pos++] = '\\';
      cmdline[pos++] = *p;
    }
    cmdline[pos++] = '"';
  }
  cmdline[pos] = '\0';

  STARTUPINFOA si = {0};
  PROCESS_INFORMATION pi = {0};
  si.cb = sizeof(si);

  if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    fprintf(stderr, "facts: CreateProcess failed (%lu)\n", GetLastError());
    free(child_argv);
    return 2;
  }
  free(child_argv);

  DWORD wait = WaitForSingleObject(pi.hProcess, timeout * 1000);
  if (wait == WAIT_TIMEOUT) {
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 1000);
    fprintf(stderr, "\nfacts: TIMEOUT after %d seconds\n", timeout);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 2;
  }

  DWORD exit_code = 0;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (exit_code > 1) {
    fprintf(stderr, "\nfacts: child exited with code %lu\n", (unsigned long)exit_code);
  }
  return (int)exit_code;

#else // POSIX
  pid_t pid = fork();
  if (pid < 0) {
    perror("facts: fork");
    free(child_argv);
    return 2;
  }

  if (pid == 0) {
    // Child: lower priority and exec
    nice(10);
    execv(argv[0], (char *const *)child_argv);
    // If exec fails, try /proc/self/exe (Linux)
    execv("/proc/self/exe", (char *const *)child_argv);
    perror("facts: exec");
    _exit(2);
  }

  // Parent: wait with timeout
  free(child_argv);

  int remaining = timeout;
  int wstatus;
  pid_t result;

  while (remaining > 0) {
    result = waitpid(pid, &wstatus, WNOHANG);
    if (result != 0) break;
    sleep(1);
    --remaining;
  }

  if (remaining <= 0) {
    // Timeout: kill child
    kill(pid, SIGKILL);
    waitpid(pid, &wstatus, 0);
    fprintf(stderr, "\nfacts: TIMEOUT after %d seconds\n", timeout);
    return 2;
  }

  if (WIFSIGNALED(wstatus)) {
    int sig = WTERMSIG(wstatus);
    const char *signame = "unknown";
    if (sig == SIGSEGV) signame = "SIGSEGV";
    else if (sig == SIGABRT) signame = "SIGABRT";
    else if (sig == SIGFPE) signame = "SIGFPE";
    else if (sig == SIGBUS) signame = "SIGBUS";
    fprintf(stderr, "\nfacts: child killed by signal %d (%s)\n", sig, signame);
    return 2;
  }

  return WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 2;
#endif
}

FACTS_EXTERN int FactsMain(int argc, const char *argv[])
{
  // Check for --facts_unsafe and --facts_timeout before anything else.
  // If --facts_unsafe is absent, re-exec under a safe wrapper.
  int unsafe = 0;
  int timeout = 10;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--facts_unsafe") == 0) unsafe = 1;
    else if (strncmp(argv[i], "--facts_timeout=", 16) == 0) timeout = atoi(argv[i] + 16);
  }

  if (!unsafe) {
    return FactsExecSafe(argc, argv, timeout);
  }

  int status = 0;
  int check = 1;
  for (int argi = 1; argi < argc; ++argi)
  {
    const char *arg = (argi < argc) ? argv[argi] : NULL;
    if (strcmp(arg, "--facts_unsafe") == 0 ||
        strncmp(arg, "--facts_timeout=", 16) == 0)
      continue;
    {
      const char *op = "--facts_include=";
      if (strncmp(arg, op, strlen(op)) == 0)
      {
        FactsInclude(arg + strlen(op));
        continue;
      }
    }
    {
      const char *op = "--facts_exclude=";
      if (strncmp(arg, op, strlen(op)) == 0)
      {
        FactsExclude(arg + strlen(op));
        continue;
      }
    }
    {
      const char *op = "--facts_find";
      if (strcmp(arg, op) == 0)
      {
        for (++argi; argi < argc; ++argi) {
          if (strcmp(argv[argi], ";") == 0) break;
          FactsScanFile(argv[argi], 1);
        }
        continue;
      }
    }
    {
      const char *op = "--facts_register_all";
      if (strcmp(arg, op) == 0)
      {
        check = 0;
        if (head == NULL) FactsRegisterAll();
        printf("FACTS_REGISTER_ALL() {\n");
        for (Facts *f = head; f != NULL; f = f->next)
          printf("    FACTS_REGISTER(%s);\n", f->name);
        printf("}\n");
        continue;
      }
    }
    {
      const char *op = "--facts_skip";
      if (strcmp(arg, op) == 0)
      {
        check = 0;
        continue;
      }
    }
    {
      const char *op = "--facts_junit";
      if (strcmp(arg, op) == 0)
      {
        facts_format |= 1; // FACTS_JUNIT FLAG
        continue;
      }
    }
    {
      const char *op = "--facts_plain";
      if (strcmp(arg, op) == 0)
      {
        facts_format |= 2; // FACTS NO COLORS FLAG
        continue;
      }
    }
    {
      const char *op = "--facts_help";
      if (strcmp(arg, op) == 0)
      {
        check = 0;
        printf("default is to check all registered facts\n");
        printf("    --facts_include=\"*pattern*\" --- include certain facts\n");
        printf("    --facts_exclude=\"*pattern*\" --- exclude certain facts\n");
        printf("    --facts_find files... [\\;] --- register facts from source\n");
        printf("    --facts_register_all --- print FACTS_REGISTER_ALL block\n");
        printf("    --facts_skip --- don't fact check\n");
        printf("    --facts_help --- this help\n");
        printf("    --facts_junit --- use junit format\n");
        printf("    --facts_plain --- no tty colors\n");
        printf("    --facts_unsafe --- run directly (skip fork/exec wrapper)\n");
        printf("    --facts_timeout=N --- timeout in seconds (default 10)\n");
        continue;
      }
    }
  }

  if (check)
  {
    FactsCheck();
  }

  return (facts_fictions == 0) ? 0 : 1;
}
