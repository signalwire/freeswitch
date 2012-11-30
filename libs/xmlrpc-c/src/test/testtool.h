#ifndef TESTTOOL_H_INCLUDED
#define TESTTOOL_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "xmlrpc-c/util.h"
#include "xmlrpc-c/util_int.h"

extern int total_tests;
extern int total_failures;


void
test_failure(const char * const file,
             unsigned int const line,
             const char * const label,
             const char * const statement);

void
test_fault(xmlrpc_env * const envP,
           int          const expectedCode,
           const char * const fileName,
           unsigned int const lineNumber);

void
test_null_string(const char * const string,
                 const char * const fileName,
                 unsigned int const lineNumber);

#define TEST(statement) \
do { \
    ++total_tests; \
    if ((statement)) { \
        printf("."); \
    } else { \
        test_failure(__FILE__, __LINE__, "expected", #statement); \
    } \
   } while (0)

#define TEST_NO_FAULT(env) \
    do { \
        ++total_tests; \
        if (!(env)->fault_occurred) { \
            printf("."); \
        } else { \
            test_failure(__FILE__, __LINE__, "fault occurred", \
            (env)->fault_string); \
        } \
       } while (0)

#define TEST_EPSILON 1E-5

#define FORCENONZERO(x) (MAX(fabs(x), TEST_EPSILON))

#define FLOATEQUAL(comparand, comparator) \
    ((fabs((comparand)-(comparator)))/FORCENONZERO(comparand) < TEST_EPSILON)
#define TESTFLOATEQUAL(comparand, comparator) \
    TEST(FLOATEQUAL(comparand, comparator))

#define TEST_FAULT(envP, code) \
    do { test_fault(envP, code, __FILE__, __LINE__); } while(0)

;

#define TEST_NULL_STRING(string) \
    do { test_null_string(string, __FILE__, __LINE__); } while(0)

;

#define TEST_ERROR(reason) \
do { \
    printf("Unable to test at %s/%u.  %s", __FILE__, __LINE__, reason); \
    abort(); \
   } while (0)

;

#endif
