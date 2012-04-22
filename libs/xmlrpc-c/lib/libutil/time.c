#include "xmlrpc_config.h"
#include <assert.h>
#include <time.h>

#if !MSVCRT
#include <sys/time.h>
#endif

#if MSVCRT
#include <windows.h>
#endif

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"


/* A note about struct timeval and Windows: There is a 'struct
   timeval' type in Windows, but it is just an argument to select(),
   which is just part of the sockets interface.  It's defined
   identically to the POSIX type of the same name, but not meant for
   general timekeeping as the POSIX type is.
*/

#if HAVE_GETTIMEOFDAY
static void
gettimeofdayPosix(xmlrpc_timespec * const todP) {

    struct timeval tv;

    gettimeofday(&tv, NULL);

    todP->tv_sec  = tv.tv_sec;
    todP->tv_nsec = tv.tv_usec * 1000;
}
#endif



#if MSVCRT
static void
gettimeofdayWindows(xmlrpc_timespec * const todP) {

    __int64 const epochOffset = 116444736000000000i64;
        /* Number of 100-nanosecond units between the beginning of the
           Windows epoch (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
        */
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;

    GetSystemTimeAsFileTime(&ft);
    li.LowPart  = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    t  = (li.QuadPart - epochOffset) * 100;  /* nanoseconds */
    todP->tv_sec  = (long)(t / 1E9);
    todP->tv_nsec = (long)(t - (__int64)todP->tv_sec * 1E9);
}
#endif



void
xmlrpc_gettimeofday(xmlrpc_timespec * const todP) {

    assert(todP);

#if HAVE_GETTIMEOFDAY
    gettimeofdayPosix(todP);
#else
#if MSVCRT
    gettimeofdayWindows(todP);
#else
  #error "We don't know how to get the time of day on this system"
#endif
#endif /* HAVE_GETTIMEOFDAY */
}



static bool
isLeapYear(unsigned int const yearOfAd) {

    return
        (yearOfAd % 4) == 0 &&
        ((yearOfAd % 100) != 0 || (yearOfAd % 400) == 0);
}



void
xmlrpc_timegm(const struct tm  * const tmP,
              time_t *           const timeValueP,
              const char **      const errorP) {
/*----------------------------------------------------------------------------
   This does what GNU libc's timegm() does.
-----------------------------------------------------------------------------*/
    if (tmP->tm_year < 70 ||
        tmP->tm_mon  > 11 ||
        tmP->tm_mon  <  0 ||
        tmP->tm_mday > 31 ||
        tmP->tm_min  > 60 ||
        tmP->tm_sec  > 60 ||
        tmP->tm_hour > 24) {

        xmlrpc_asprintf(errorP, "Invalid time specification; a member "
                        "of struct tm is out of range");
    } else {
        static unsigned int const monthDaysNonLeap[12] = 
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        unsigned int totalDays;
        unsigned int year;
        unsigned int month;

        totalDays = 0;  /* initial value */
    
        for (year = 70; year < (unsigned int)tmP->tm_year; ++year)
            totalDays += isLeapYear(1900 + year) ? 366 : 365;
    
        for (month = 0; month < (unsigned int)tmP->tm_mon; ++month)
            totalDays += monthDaysNonLeap[month];

        if (tmP->tm_mon > 1 && isLeapYear(1900 + tmP->tm_year))
            totalDays += 1;

        totalDays += tmP->tm_mday - 1;

        *errorP = NULL;

        *timeValueP = ((totalDays * 24 +
                        tmP->tm_hour) * 60 +
                        tmP->tm_min) * 60 +
                        tmP->tm_sec;
    }
}



void
xmlrpc_localtime(time_t      const datetime,
                 struct tm * const tmP) {
/*----------------------------------------------------------------------------
   Convert datetime from standard to broken-down format in the local
   time zone.

   For Windows, this is not thread-safe.  If you run a version of Abyss
   with multiple threads, you can get arbitrary results here.
-----------------------------------------------------------------------------*/
#if HAVE_LOCALTIME_R
  localtime_r(&datetime, tmP);
#else
  *tmP = *localtime(&datetime);
#endif
}



void
xmlrpc_gmtime(time_t      const datetime,
              struct tm * const resultP) {
/*----------------------------------------------------------------------------
   Convert datetime from standard to broken-down UTC format.

   For Windows, this is not thread-safe.  If you run a version of Abyss
   with multiple threads, you can get arbitrary results here.
-----------------------------------------------------------------------------*/

#if HAVE_GMTIME_R
    gmtime_r(&datetime, resultP);
#else
    *resultP = *gmtime(&datetime);
#endif
}
