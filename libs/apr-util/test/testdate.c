/* This program tests the date_parse_http routine in ../main/util_date.c.
 *
 * It is only semiautomated in that I would run it, modify the code to
 * use a different algorithm or seed, recompile and run again, etc.
 * Obviously it should use an argument for that, but I never got around
 * to changing the implementation.
 * 
 *     gcc -g -O2 -I../main -o test_date ../main/util_date.o test_date.c
 *     test_date | egrep '^No '
 * 
 * Roy Fielding, 1996
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "apr_date.h"

#ifndef srand48
#define srand48 srandom
#endif

#ifndef mrand48
#define mrand48 random
#endif

void gm_timestr_822(char *ts, apr_time_t sec);
void gm_timestr_850(char *ts, apr_time_t sec);
void gm_timestr_ccc(char *ts, apr_time_t sec);

static const apr_time_t year2secs[] = {
             0LL,    /* 1970 */
      31536000LL,    /* 1971 */
      63072000LL,    /* 1972 */
      94694400LL,    /* 1973 */
     126230400LL,    /* 1974 */
     157766400LL,    /* 1975 */
     189302400LL,    /* 1976 */
     220924800LL,    /* 1977 */
     252460800LL,    /* 1978 */
     283996800LL,    /* 1979 */
     315532800LL,    /* 1980 */
     347155200LL,    /* 1981 */
     378691200LL,    /* 1982 */
     410227200LL,    /* 1983 */
     441763200LL,    /* 1984 */
     473385600LL,    /* 1985 */
     504921600LL,    /* 1986 */
     536457600LL,    /* 1987 */
     567993600LL,    /* 1988 */
     599616000LL,    /* 1989 */
     631152000LL,    /* 1990 */
     662688000LL,    /* 1991 */
     694224000LL,    /* 1992 */
     725846400LL,    /* 1993 */
     757382400LL,    /* 1994 */
     788918400LL,    /* 1995 */
     820454400LL,    /* 1996 */
     852076800LL,    /* 1997 */
     883612800LL,    /* 1998 */
     915148800LL,    /* 1999 */
     946684800LL,    /* 2000 */
     978307200LL,    /* 2001 */
    1009843200LL,    /* 2002 */
    1041379200LL,    /* 2003 */
    1072915200LL,    /* 2004 */
    1104537600LL,    /* 2005 */
    1136073600LL,    /* 2006 */
    1167609600LL,    /* 2007 */
    1199145600LL,    /* 2008 */
    1230768000LL,    /* 2009 */
    1262304000LL,    /* 2010 */
    1293840000LL,    /* 2011 */
    1325376000LL,    /* 2012 */
    1356998400LL,    /* 2013 */
    1388534400LL,    /* 2014 */
    1420070400LL,    /* 2015 */
    1451606400LL,    /* 2016 */
    1483228800LL,    /* 2017 */
    1514764800LL,    /* 2018 */
    1546300800LL,    /* 2019 */
    1577836800LL,    /* 2020 */
    1609459200LL,    /* 2021 */
    1640995200LL,    /* 2022 */
    1672531200LL,    /* 2023 */
    1704067200LL,    /* 2024 */
    1735689600LL,    /* 2025 */
    1767225600LL,    /* 2026 */
    1798761600LL,    /* 2027 */
    1830297600LL,    /* 2028 */
    1861920000LL,    /* 2029 */
    1893456000LL,    /* 2030 */
    1924992000LL,    /* 2031 */
    1956528000LL,    /* 2032 */
    1988150400LL,    /* 2033 */
    2019686400LL,    /* 2034 */
    2051222400LL,    /* 2035 */
    2082758400LL,    /* 2036 */
    2114380800LL,    /* 2037 */
    2145916800LL     /* 2038 */
};

const char month_snames[12][4] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

void gm_timestr_822(char *ts, apr_time_t sec)
{
    static const char *const days[7]=
        {"Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm *tms;
    time_t ls = (time_t)sec;

    tms = gmtime(&ls);
 
    sprintf(ts, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT", days[tms->tm_wday],
            tms->tm_mday, month_snames[tms->tm_mon], tms->tm_year + 1900,
            tms->tm_hour, tms->tm_min, tms->tm_sec);
}

void gm_timestr_850(char *ts, apr_time_t sec)
{
    static const char *const days[7]=
           {"Sunday","Monday", "Tuesday", "Wednesday", "Thursday", "Friday", 
            "Saturday"};
    struct tm *tms;
    int year;
    time_t ls = (time_t)sec;
 
    tms = gmtime(&ls);

    year = tms->tm_year;
    if (year >= 100) year -= 100;
 
    sprintf(ts, "%s, %.2d-%s-%.2d %.2d:%.2d:%.2d GMT", days[tms->tm_wday],
            tms->tm_mday, month_snames[tms->tm_mon], year,
            tms->tm_hour, tms->tm_min, tms->tm_sec);
}

void gm_timestr_ccc(char *ts, apr_time_t sec)
{
    static const char *const days[7]=
       {"Sun","Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm *tms;
    time_t ls = (time_t)sec;
 
    tms = gmtime(&ls);
 
    sprintf(ts, "%s %s %2d %.2d:%.2d:%.2d %d", days[tms->tm_wday],
            month_snames[tms->tm_mon], tms->tm_mday, 
            tms->tm_hour, tms->tm_min, tms->tm_sec, tms->tm_year + 1900);
}

int main (void)
{
    int year, i;
    apr_time_t guess;
    apr_time_t offset = 0;
 /* apr_time_t offset = 0; */
 /* apr_time_t offset = ((31 + 28) * 24 * 3600) - 1; */
    apr_time_t secstodate, newsecs;
    char datestr[50];

    for (year = 1970; year < 2038; ++year) {
        secstodate = year2secs[year - 1970] + offset;
        gm_timestr_822(datestr, secstodate);
        secstodate *= APR_USEC_PER_SEC;
        newsecs = apr_date_parse_http(datestr);
        if (secstodate == newsecs)
            printf("Yes %4d %19" APR_TIME_T_FMT " %s\n", year, secstodate, datestr);
        else if (newsecs == APR_DATE_BAD)
            printf("No  %4d %19" APR_TIME_T_FMT " %19" APR_TIME_T_FMT " %s\n",
                   year, secstodate, newsecs, datestr);
        else
            printf("No* %4d %19" APR_TIME_T_FMT " %19" APR_TIME_T_FMT " %s\n",
                   year, secstodate, newsecs, datestr);
    }
    
    srand48(978245L);

    for (i = 0; i < 10000; ++i) {
        guess = (time_t)mrand48();
        if (guess < 0) guess *= -1;
        secstodate = guess + offset;
        gm_timestr_822(datestr, secstodate);
        secstodate *= APR_USEC_PER_SEC;
        newsecs = apr_date_parse_http(datestr);
        if (secstodate == newsecs)
            printf("Yes %" APR_TIME_T_FMT " %s\n", secstodate, datestr);
        else if (newsecs == APR_DATE_BAD)
            printf("No  %" APR_TIME_T_FMT " %" APR_TIME_T_FMT " %s\n", 
                   secstodate, newsecs, datestr);
        else
            printf("No* %" APR_TIME_T_FMT " %" APR_TIME_T_FMT " %s\n", 
                   secstodate, newsecs, datestr);
    }
    exit(0);
}
