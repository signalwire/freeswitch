#include <stdlib.h>
#include <stdio.h>

extern int total_tests;
extern int total_failures;


static __inline void
strfree(const char * const string) {
    free((char*)string);
}



/* This is a good place to set a breakpoint. */
static __inline void
test_failure(const char * const file,
             unsigned int const line,
             const char * const label,
             const char * const statement) {

    ++total_failures;
    printf("\n%s:%u: test failure: %s (%s)\n", file, line, label, statement);
    abort();
}



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

#define TEST_FAULT(env, code) \
do { \
    ++total_tests; \
    if ((env)->fault_occurred && (env)->fault_code == (code)) { \
        printf("."); \
    } else { \
            test_failure(__FILE__, __LINE__, "wrong/no fault occurred", \
            (env)->fault_string); \
    } \
   } while (0)



#define TEST_ERROR(reason) \
do { \
    printf("Unable to test at %s/%u.  %s", __FILE__, __LINE__, reason); \
    abort(); \
   } while (0)
