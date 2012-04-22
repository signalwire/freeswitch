/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@file sofia-sip/tstdef.h Macros for unit tests
 *
 * The macros defined here can be used by unit test programs. When a test
 * fails, the TEST macros print the offending file name and line number.
 * They use format that is accepted by Emacs and other fine editors so you
 * can directly go to the source code of the failed test with next-error.
 *
 * @note There is no protection agains multiple inclusion.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Aug 22 13:53:24 2001 ppessi
 *
 * @par Example Program
 *
 * You should define the macro TSTFLAGS to a int variable possibly
 * containing a flag #tst_verbatim. As a convention, the int variable should
 * be set when your test program is run with @c -v or @c --verbatim command
 * line option. If the (TSTFLAGS & tst_verbatim) is true, the test macros
 * log the test before executing it and the result of successful tests, too.
 *
 * You should typedef longlong to integer type at least 64 bit wide before
 * including <sofia-sip/tstdef.h>, too.
 *
 * As an example, we provide a test program making sure that inet_ntop() and
 * inet_pton() behave as expected and that we can create UDP/IPv4 sockets
 * with @b su library:
 *
 * @code
 * #include "config.h"
 *
 * #include <stdio.h>
 * #include <limits.h>
 *
 * #include <sofia-sip/su.h>
 *
 * #define TSTFLAGS tstflags
 *
 * #include <stdlib.h>
 * #include <sofia-sip/tstdef.h>
 *
 * static int tstflags = 0;
 *
 * void usage(void)
 * {
 *   fprintf(stderr, "usage: %s [-v|--verbatim]\n", name);
 *   exit(2);
 * }
 *
 * static int socket_test(void);
 *
 * int main(int argc, char *argv[])
 * {
 *   int retval = 0, i;
 *
 *   for (i = 1; argv[i]; i++) {
 *     if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbatim") == 0)
 *       tstflags |= tst_verbatim;
 *     else
 *       usage();
 *   }
 *
 *   retval |= socket_test(); fflush(stdout);
 *
 *   return retval;
 * }
 *
 * double max_bandwidth()
 *
 * int socket_test(void)
 * {
 *   su_socket_t s;
 *   char buf[64];
 *   unsigned long localhost = htonl(0x7f000001);
 *   unsigned long addr;
 *
 *   BEGIN();
 *
 *   // Check inet_ntop() return value (Tests equality of integral values)
 *   TEST(inet_ntop(AF_INET, &localhost, buf, sizeof buf), buf);
 *
 *   // Check inet_ntop() result (Tests equality of strings)
 *   TEST_S(buf, "127.0.0.1");
 *
 *   // Check inet_pton() argument validation (Tests equality of ints)
 *   TEST(inet_pton(0, buf, &addr), -1);
 *
 *   // Check inet_pton() return value (Tests for true value (non-zero))
 *   TEST_1(inet_pton(AF_INET, buf, &addr) > 0);
 *
 *   // Check inet_pton() result (Tests equality of memory areas)
 *   TEST_M(&addr, &localhost, sizeof(addr));
 *
 *   // Test to create UDP socket (Test for true value)
 *   TEST_1((s = su_socket(AF_INET, SOCK_DGRAM, 0)) != INVALID_SOCKET);
 *
 *   // Check max bandwidth
 *   TEST_D(max_bandwidth(), DBL_MAX);
 *
 *   END();
 * }
 * @endcode
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

#if HAVE_FUNC
#define TSTNAME name, __func__, "() "
#elif HAVE_FUNCTION
#define TSTNAME name, __FUNCTION__, "() "
#else
#define TSTNAME name, "", ""
#endif

enum {
  /** If (TSTFLAGS & tst_verbatim) is non-zero, be verbatim. */
  tst_verbatim = 1,
  /** If (TSTFLAGS & tst_abort) is non-zero, abort() when failed. */
  tst_abort = 2,
  /** If (TSTFLAGS & tst_log) is non-zero, log intermediate results. */
  tst_log = 4
};

#ifndef TSTFLAGS
#error <TSTFLAGS is not defined>
#endif

/** Begin a test function. @HIDE */
#define BEGIN() BEGIN_(TSTFLAGS); { extern int tstdef_dummy
/** End a test function. @HIDE */
#define END() (void) tstdef_dummy; } END_(TSTFLAGS)
/**Test that @a suite returns a nonzero value.
 * @deprecated Use TEST_1()
 * @HIDE */
#define TEST0(suite) TEST_1_(TSTFLAGS, suite)
/** Test that @a suite returns a nonzero value. @HIDE */
#define TEST_1(suite) TEST_1_(TSTFLAGS, suite)
/** Test a void suite. @HIDE */
#define TEST_VOID(suite) TEST_VOID_(TSTFLAGS, suite)
/** Test that @a suite is equal to @a expected. @HIDE */
#define TEST(suite, expected) TEST_(TSTFLAGS, suite, expected)
/** Test that @a suite is same pointer as @a expected. @HIDE */
#define TEST_P(suite, expected) TEST_P_(TSTFLAGS, suite, expected)
/** Test that 64-bit @a suite is equal to @a expect. @HIDE */
#define TEST64(suite, expected) TEST64_(TSTFLAGS, suite, expected)
/** Test that @a suite is same double as @a expected. @HIDE */
#define TEST_D(suite, expected) TEST_D_(TSTFLAGS, suite, expected)
/** Test that @a suite is same string as @a expected. @HIDE */
#define TEST_S(suite, expected) TEST_S_(TSTFLAGS, suite, expected)
/** Test that @a suite is results as identical memory as @a expected. @HIDE */
#define TEST_M(suite, expected, len) TEST_M_(TSTFLAGS, suite, expected, len)
/** Test that @a suite has same size as @a expected. @HIDE */
#define TEST_SIZE(suite, expected) TEST_SIZE_(TSTFLAGS, suite, expected)

/** Print in torture test with -l option */
#define TEST_LOG(x)		       \
  do {				       \
    if (tstflags & tst_log)	       \
      printf x;			       \
  } while(0)

#define TEST_FAILED(flags) \
  ((flags) & tst_abort) ? abort() : (void)0; return 1

/** @HIDE */
#define TEST_1_(flags, suite) do { \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s\n", TSTNAME, #suite); \
    fflush(stdout); } \
  if ((suite)) { if (flags & tst_verbatim) \
  printf("%s: %s%sok: (%s)\n", TSTNAME, #suite); break ; } \
  fprintf(stderr, "%s:%u: %s %s%sFAILED: (%s)\n", \
          __FILE__, __LINE__, TSTNAME, #suite); fflush(stderr); \
  TEST_FAILED(flags); }						\
  while(0)

/** @HIDE */
#define TEST_VOID_(flags, suite) do { \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s\n", TSTNAME, #suite); \
    fflush(stdout); } \
  (suite); } while(0)

/** @HIDE */
#define TEST_(flags, suite, expect) do {				\
    uintptr_t _value, _expect;						\
    if (flags & tst_verbatim) {						\
      printf("%s: %s%stesting %s == %s\n", TSTNAME, #suite, #expect);	\
      fflush(stdout); }							\
    _value = (uintptr_t)(suite);					\
    _expect = (uintptr_t)(expect);					\
    if (_value == _expect) {						\
      if (flags & tst_verbatim)						\
	printf("%s: %s%sok: %s == %s \n",				\
	       TSTNAME, #suite, #expect);				\
      break;								\
    }									\
    fprintf(stderr, "%s:%u: %s %s%sFAILED: "				\
	    "%s != %s or "MOD_ZU" != "MOD_ZU"\n",			\
	    __FILE__, __LINE__, TSTNAME,				\
	    #suite, #expect, (size_t)_value, (size_t)_expect);		\
    fflush(stderr);							\
    TEST_FAILED(flags);							\
  } while(0)

/** @HIDE */
#define TEST_P_(flags, suite, expect) do {				\
    void const * _value, * _expect;					\
  if (flags & tst_verbatim) {						\
    printf("%s: %s%stesting %s == %s\n", TSTNAME, #suite, #expect);	\
    fflush(stdout); }							\
  if ((_value = (suite)) == (_expect = (expect))) {			\
    if (flags & tst_verbatim)						\
      printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect);	\
    break;								\
  }									\
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s or %p != %p\n",	\
	  __FILE__, __LINE__, TSTNAME,					\
	  #suite, #expect, _value, _expect); fflush(stderr);		\
  TEST_FAILED(flags);							\
  } while(0)

/** @HIDE */
#define TEST_SIZE_(flags, suite, expect) do {	\
  size_t _value, _expect; \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s == %s\n", TSTNAME, #suite, #expect); \
    fflush(stdout); } \
  if ((_value = (size_t)(suite)) == \
      (_expect = (size_t)(expect))) \
  { if (flags & tst_verbatim) \
  printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect); break; } \
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s or "MOD_ZU" != "MOD_ZU"\n", \
	 __FILE__, __LINE__, TSTNAME, \
	  #suite, #expect, _value, _expect); fflush(stderr);		\
  TEST_FAILED(flags);							\
  } while(0)


/** @HIDE */
#define TEST64_(flags, suite, expect) do { \
  uint64_t _value, _expect; \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s == %s\n", TSTNAME, #suite, #expect); \
    fflush(stdout); } \
  if ((_value = (uint64_t)(suite)) == (_expect = (uint64_t)(expect))) \
  { if (flags & tst_verbatim) \
  printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect); break; } \
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s or "LLU" != "LLU"\n", \
	 __FILE__, __LINE__, TSTNAME, \
	  #suite, #expect, (unsigned longlong)_value,	\
	 (unsigned longlong)_expect); fflush(stderr);	\
  TEST_FAILED(flags);					\
  } while(0)

/** @HIDE */
#define TEST_D_(flags, suite, expect) do { \
  double _value, _expect; \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s == %s\n", TSTNAME, #suite, #expect); \
    fflush(stdout); } \
  if ((_value = (double)(suite)) == (_expect = (double)(expect))) \
  { if (flags & tst_verbatim) \
  printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect); break; } \
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s or %g != %g\n", \
	 __FILE__, __LINE__, TSTNAME, \
         #suite, #expect, _value, _expect); fflush(stderr); \
  TEST_FAILED(flags);					    \
  } while(0)

/** @HIDE */
#define TEST_S_(flags, suite, expect) do { \
  char const * _value, * _expect; \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s is %s\n", TSTNAME, #suite, #expect); \
    fflush(stdout); } \
  _value = (suite); \
  _expect = (expect); \
  if (((_value == NULL || _expect == NULL) && _value == _expect) || \
      (_value != NULL && _expect != NULL && strcmp(_value, _expect) == 0)) \
  { if (flags & tst_verbatim) \
  printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect);break;}\
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s or %s%s%s != \"%s\"\n", \
	 __FILE__, __LINE__, TSTNAME, \
	  #suite, #expect, \
	  _value ? "\"" : "", _value ? _value : "NULL", _value ? "\"" : "", \
	  _expect); fflush(stderr);					\
  TEST_FAILED(flags);							\
  } while(0)

/** @HIDE */
#define TEST_M_(flags, suite, expect, len) do { \
  void const * _value, * _expect; \
  size_t _len; \
  if (flags & tst_verbatim) { \
    printf("%s: %s%stesting %s is %s\n", TSTNAME, #suite, #expect); \
    fflush(stdout); } \
  _value = (suite); \
  _expect = (expect); \
  _len = (size_t)(len); \
  if (((_value == NULL || _expect == NULL) && _value == _expect) || \
      memcmp(_value, _expect, _len) == 0) \
  { if (flags & tst_verbatim) \
  printf("%s: %s%sok: %s == %s \n", TSTNAME, #suite, #expect);break;}\
  fprintf(stderr, "%s:%u: %s %s%sFAILED: %s != %s "\
                  "or \"%.*s\" != \"%.*s\"\n", \
	 __FILE__, __LINE__, TSTNAME, \
	  #suite, #expect, (int)_len, \
          (char *)_value, (int)_len, (char *)_expect); \
  fflush(stderr);							\
  TEST_FAILED(flags);							\
  } while(0)

/** @HIDE */
#define BEGIN_(flags) \
  if (flags & tst_verbatim) printf("%s: %s%sstarting\n", TSTNAME)

/** @HIDE */
#define END_(flags) \
  if (flags & tst_verbatim) \
    printf("%s: %s%sfinished fully successful\n", TSTNAME); \
  return 0

SOFIA_END_DECLS
