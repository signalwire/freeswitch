#ifdef _MSC_VER
#pragma warning(disable:4100)
#endif

#include "windows.h"

const unsigned long long int DELTA_EPOCH_IN_MICROSECS = 11644473600000000;

void gettimeofday(struct timeval *tv, void *tz)
{
    FILETIME ft;
    unsigned long long int highResolutionTime;
    TIME_ZONE_INFORMATION tz_winapi;
    int result_tz;
    long long int timezone_time_bias_in_minutes;

    ZeroMemory(&ft, sizeof(ft));
    ZeroMemory(&tz_winapi, sizeof(tz_winapi));

    GetSystemTimeAsFileTime(&ft);
    result_tz = GetTimeZoneInformation(&tz_winapi);
    timezone_time_bias_in_minutes = tz_winapi.Bias + ((result_tz == TIME_ZONE_ID_DAYLIGHT)  ?  tz_winapi.DaylightBias  :  0);

    highResolutionTime = ft.dwHighDateTime;
    highResolutionTime <<= 32;
    highResolutionTime |= ft.dwLowDateTime;

    /* Converting file time to unix epoch */
    /* Convert to microseconds */
    highResolutionTime /= 10;
    /* Add timezone bias conververt from minutes to microsecond */
    highResolutionTime -= timezone_time_bias_in_minutes*60*1000000;
    highResolutionTime -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (long int) (highResolutionTime/1000000);
    tv->tv_usec = (highResolutionTime%1000000);
}
