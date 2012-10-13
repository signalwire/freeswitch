#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "casprintf.h"
#include "girstring.h"

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/string_int.h"

#include "testtool.h"

#include "value_datetime.h"



static const char *
truncateFracSec(const char * const datestring) {
/*----------------------------------------------------------------------------
   Return 'datestring', but with any fractional seconds chopped off.
   E.g. if 'datestring' is "20000301T00:00:00.654321",
   we return "20000301T00:00:00".
-----------------------------------------------------------------------------*/
    char * buffer;
    unsigned int i;

    buffer = strdup(datestring);

    for (i = 0; i < strlen(buffer); ++i) {
        if (buffer[i] == '.')
            buffer[i] = '\0';
    }
    return buffer;
}



#if XMLRPC_HAVE_TIMEVAL

static struct timeval
makeTv(time_t       const secs,
       unsigned int const usecs) {

    struct timeval retval;

    retval.tv_sec  = secs;
    retval.tv_usec = usecs;

    return retval;
}

static bool
tvIsEqual(struct timeval const comparand,
          struct timeval const comparator) {
    return
        comparand.tv_sec  == comparator.tv_sec &&
        comparand.tv_usec == comparator.tv_usec;
}
#endif



#if XMLRPC_HAVE_TIMESPEC

static struct timespec
makeTs(time_t       const secs,
       unsigned int const usecs) {

    struct timespec retval;

    retval.tv_sec  = secs;
    retval.tv_nsec = usecs * 1000;

    return retval;
}

static bool
tsIsEqual(struct timespec const comparand,
          struct timespec const comparator) {
    return
        comparand.tv_sec  == comparator.tv_sec &&
        comparand.tv_nsec == comparator.tv_nsec;
}
#endif



static void
test_value_datetime_varytime(const char * const datestring,
                             time_t       const datetime,
                             unsigned int const usec) {

    xmlrpc_value * v;
    xmlrpc_env env;
    const char * readBackString;
    time_t readBackDt;
    unsigned int readBackUsec;
    const char * datestringSec;
#if XMLRPC_HAVE_TIMEVAL
    struct timeval const dtTimeval = makeTv(datetime, usec);
    struct timeval readBackTv;
#endif
#if XMLRPC_HAVE_TIMESPEC
    struct timespec const dtTimespec = makeTs(datetime, usec);
    struct timespec readBackTs;
#endif

    datestringSec = truncateFracSec(datestring);

    xmlrpc_env_init(&env);

    /* Test xmlrpc_datetime_new_str and time read functions*/
    v = xmlrpc_datetime_new_str(&env, datestring);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_sec(&env, v, &readBackDt);
    TEST_NO_FAULT(&env);
    TEST(readBackDt == datetime);

    xmlrpc_read_datetime_usec(&env, v, &readBackDt, &readBackUsec);
    TEST_NO_FAULT(&env);
    TEST(readBackDt == datetime);
    TEST(readBackUsec == usec);

#if XMLRPC_HAVE_TIMEVAL
    xmlrpc_read_datetime_timeval(&env, v, &readBackTv);
    TEST_NO_FAULT(&env);
    TEST(tvIsEqual(dtTimeval, readBackTv));
#endif

#if XMLRPC_HAVE_TIMESPEC
    xmlrpc_read_datetime_timespec(&env, v, &readBackTs);
    TEST_NO_FAULT(&env);
    TEST(tsIsEqual(dtTimespec, readBackTs));
#endif

    xmlrpc_DECREF(v);

    /* Test xmlrpc_datetime_new_sec */
    v = xmlrpc_datetime_new_sec(&env, datetime);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &readBackString);
    TEST_NO_FAULT(&env);
    TEST(streq(readBackString, datestringSec));
    strfree(readBackString);

    xmlrpc_DECREF(v);

    /* Test xmlrpc_datetime_new_usec */
    v = xmlrpc_datetime_new_usec(&env, datetime, usec);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &readBackString);
    TEST_NO_FAULT(&env);
    TEST(streq(readBackString, datestring));
    strfree(readBackString);

    xmlrpc_DECREF(v);

#if XMLRPC_HAVE_TIMEVAL
    /* Test xmlrpc_datetime_new_timeval */
    v = xmlrpc_datetime_new_timeval(&env, dtTimeval);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &readBackString);
    TEST_NO_FAULT(&env);
    TEST(streq(readBackString, datestring));
    strfree(readBackString);

    xmlrpc_DECREF(v);
#endif

#if XMLRPC_HAVE_TIMESPEC
    /* Test xmlrpc_datetime_new_timespec */
    v = xmlrpc_datetime_new_timespec(&env, dtTimespec);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &readBackString);
    TEST_NO_FAULT(&env);
    TEST(streq(readBackString, datestring));
    strfree(readBackString);

    xmlrpc_DECREF(v);
#endif

    xmlrpc_env_clean(&env);
    strfree(datestringSec);
}



static void
test_value_datetime_not_unix(const char * const datestring) {

    xmlrpc_value * v;
    xmlrpc_env env;
    time_t dt;

    xmlrpc_env_init(&env);

    v = xmlrpc_datetime_new_str(&env, datestring);
    TEST_NO_FAULT(&env);

    xmlrpc_read_datetime_sec(&env, v, &dt);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_datetime_str_invalid1(const char * const datestring) {

    /* Ideally, xmlrpc_datetime_new_str() would fail on these, but
       the code doesn't implement that today.  However,
       xmlrpc_read_datetime_sec() does catch many cases, so we
       use that.

       Note that xmlrpc_read_datetime_sec() doesn't catch them all.
       Sometimes it just returns garbage, e.g. returns July 1 for
       June 31.
    */

    xmlrpc_value * v;
    xmlrpc_env env;
    time_t dt;

    xmlrpc_env_init(&env);

    v = xmlrpc_datetime_new_str(&env, datestring);
    TEST_NO_FAULT(&env);

    xmlrpc_read_datetime_sec(&env, v, &dt);
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR);

    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_datetime_str_invalid2(const char * const datestring) {

    xmlrpc_value * v;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    v = xmlrpc_datetime_new_str(&env, datestring);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_build_decomp_datetime(void) {

    const char * datestring = "19980717T14:08:55";
    time_t const datetime = 900684535;

    xmlrpc_env env;
    xmlrpc_value * v;
    time_t dt;
    const char * ds;

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "t", datetime);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_DATETIME);

    dt = 0;
    xmlrpc_read_datetime_sec(&env, v, &dt);
    TEST(dt == datetime);

    dt = 0;
    xmlrpc_decompose_value(&env, v, "t", &dt);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(dt == datetime);

    v = xmlrpc_int_new(&env, 9);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, v, "t", &dt);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);
    xmlrpc_decompose_value(&env, v, "8", &ds);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "8", datestring);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_DATETIME);
    xmlrpc_decompose_value(&env, v, "8", &ds);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(streq(ds, datestring));
    strfree(ds);

    xmlrpc_env_clean(&env);
}




static void
test_value_datetime_basic(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_datetime dt;
    xmlrpc_datetime readBackDt;

    xmlrpc_env_init(&env);

    dt.Y = 2001;
    dt.M = 12;
    dt.D = 25;
    dt.h = 1;
    dt.m = 2;
    dt.s = 3;
    dt.u = 4;

    v = xmlrpc_datetime_new(&env, dt);

    xmlrpc_read_datetime(&env, v, &readBackDt);
    TEST_NO_FAULT(&env);
    TEST(readBackDt.Y = dt.Y);
    TEST(readBackDt.M = dt.M);
    TEST(readBackDt.D = dt.D);
    TEST(readBackDt.h = dt.h);
    TEST(readBackDt.m = dt.m);
    TEST(readBackDt.s = dt.s);
    TEST(readBackDt.u = dt.u);

    xmlrpc_env_clean(&env);
}



void
test_value_datetime(void) {

    const char * datestring = "19980717T14:08:55";
    time_t const datetime = 900684535;

    xmlrpc_env env;

    printf("\n  Running datetime value tests");

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_DATETIME), "DATETIME"));

    test_value_datetime_basic();

    /* Valid datetime, generated from XML-RPC string, time_t, and
       time_t + microseconds
    */

    test_value_datetime_varytime(datestring, datetime, 0);

    /* test microseconds */
    test_value_datetime_varytime("20000301T00:00:00.654321",
                                 951868800,  654321);
    test_value_datetime_varytime("20040229T23:59:59.123000",
                                 1078099199, 123000);
    test_value_datetime_varytime("20000229T23:59:59.000123",
                                 951868799,  123);

    /* Leap years */
    test_value_datetime_varytime("20000229T23:59:59",  951868799, 0);
    test_value_datetime_varytime("20000301T00:00:00",  951868800, 0);
    test_value_datetime_varytime("20010228T23:59:59",  983404799, 0);
    test_value_datetime_varytime("20010301T00:00:00",  983404800, 0);
    test_value_datetime_varytime("20040229T23:59:59", 1078099199, 0);
    test_value_datetime_varytime("20040301T00:00:00", 1078099200, 0);

    /* Datetimes that can't be represented as time_t */
    test_value_datetime_not_unix("19691231T23:59:59");

    /* Invalid datetimes */
    /* Note that the code today does a pretty weak job of validating datetimes,
       so we test only the validation that we know is implemented.
    */
    test_value_datetime_str_invalid1("19700101T25:00:00");
    test_value_datetime_str_invalid1("19700101T10:61:01");
    test_value_datetime_str_invalid1("19700101T10:59:61");
    test_value_datetime_str_invalid1("19700001T10:00:00");
    test_value_datetime_str_invalid1("19701301T10:00:00");
    test_value_datetime_str_invalid1("19700132T10:00:00");
    test_value_datetime_str_invalid2("19700132T10:00:00.");
    test_value_datetime_str_invalid2("19700132T10:00:00,123");

    test_build_decomp_datetime();

    xmlrpc_env_clean(&env);

    printf("\n");
    printf("  datetime value tests done.\n");
}
