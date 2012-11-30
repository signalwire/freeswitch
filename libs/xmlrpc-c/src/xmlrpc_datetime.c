#include "xmlrpc_config.h"

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#if MSVCRT
#include <windows.h>
#endif

#include "bool.h"
#include "mallocvar.h"

#include "xmlrpc-c/c_util.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"


#if HAVE_REGEX
#include <regex.h>
#endif

#if MSVCRT

static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
static const __int64 SECS_TO_100NS = 10000000; /* 10^7 */


void
UnixTimeToFileTime(time_t     const t,
                   LPFILETIME const pft) {

    int64_t const ll =
        Int32x32To64(t, SECS_TO_100NS) + SECS_BETWEEN_EPOCHS * SECS_TO_100NS;

    pft->dwLowDateTime  = (DWORD)ll;
    pft->dwHighDateTime = (DWORD)(ll >> 32);
}



void
UnixTimeToSystemTime(time_t const t,
                     LPSYSTEMTIME const pst) {
    FILETIME ft;

    UnixTimeToFileTime(t, &ft);
    FileTimeToSystemTime(&ft, pst);
}



static void
UnixTimeFromFileTime(xmlrpc_env *  const envP,
                     LPFILETIME    const pft,
                     time_t *      const timeValueP) { 

    int64_t const WinEpoch100Ns =
        ((int64_t)pft->dwHighDateTime << 32) + pft->dwLowDateTime;
    int64_t const unixEpoch100Ns =
        WinEpoch100Ns - (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);
    int64_t const unixEpochSeconds =
        unixEpoch100Ns / SECS_TO_100NS; 

    if ((time_t)unixEpochSeconds != unixEpochSeconds) {
        /* Value is too big for a time_t; fail. */
        xmlrpc_faultf(envP, "Does not indicate a valid date");
        *timeValueP = (time_t)(-1);
    } else
        *timeValueP = (time_t)unixEpochSeconds;
}



static void
UnixTimeFromSystemTime(xmlrpc_env * const envP,
                       LPSYSTEMTIME const pst,
                       time_t *     const timeValueP) {
    FILETIME filetime;

    SystemTimeToFileTime(pst, &filetime); 
    UnixTimeFromFileTime(envP, &filetime, timeValueP); 
}

#endif  /* MSVCRT */



static void
validateDatetimeType(xmlrpc_env *         const envP,
                     const xmlrpc_value * const valueP) {

    if (valueP->_type != XMLRPC_TYPE_DATETIME) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value of type %s supplied where "
            "type %s was expected.", 
            xmlrpc_type_name(valueP->_type), 
            xmlrpc_type_name(XMLRPC_TYPE_DATETIME));
    }
}



void
xmlrpc_read_datetime(xmlrpc_env *         const envP,
                     const xmlrpc_value * const valueP,
                     xmlrpc_datetime *    const dtP) {

    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred) {
        *dtP = valueP->_value.dt;
    }
}



void
xmlrpc_read_datetime_str(xmlrpc_env *         const envP,
                         const xmlrpc_value * const valueP,
                         const char **        const stringValueP) {
/*----------------------------------------------------------------------------
   This exists for backward compatibility.  No normal modern program would
   want to see a datetime value in this format.  Note that the format isn't
   even ISO 8601 -- it's a bizarre hybrid of two ISO 8601 formats.

   Do not extend this.

   This exists because Xmlrpc-c was at one time lazy and this was the only way
   to extract the value.  An xmlrpc_value in those days represented a datetime
   with the actual XML-RPC wire format of a datetime, and this function simply
   returned a copy of it.
-----------------------------------------------------------------------------*/
    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred) {
        time_t secs;
        unsigned int usecs;

        xmlrpc_read_datetime_usec(envP, valueP, &secs, &usecs);

        if (!envP->fault_occurred) {
            struct tm brokenTime;
            char dtString[64];

            xmlrpc_gmtime(secs, &brokenTime);

            /* Note that this format is NOT ISO 8601 -- it's a bizarre
               hybrid of two ISO 8601 formats.
            */
            strftime(dtString, sizeof(dtString), "%Y%m%dT%H:%M:%S", 
                     &brokenTime);

            if (usecs != 0) {
                char usecString[64];
                assert(usecs < 1000000);
                snprintf(usecString, sizeof(usecString), ".%06u", usecs);
                STRSCAT(dtString, usecString);
            }

            *stringValueP = strdup(dtString);
            if (*stringValueP == NULL)
                xmlrpc_faultf(envP,
                              "Unable to allocate memory for datetime string");
        }
    }
}



void
xmlrpc_read_datetime_str_old(xmlrpc_env *         const envP,
                             const xmlrpc_value * const valueP,
                             const char **        const stringValueP) {

    assert(valueP->_cache);
    
    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred) {
        const char ** const readBufferP = valueP->_cache;

        if (!*readBufferP)
            /* Nobody's asked for the internal buffer before.  Set it up. */
            xmlrpc_read_datetime_str(envP, valueP, readBufferP);

        *stringValueP = *readBufferP;
    }
}



void
xmlrpc_read_datetime_usec(xmlrpc_env *         const envP,
                          const xmlrpc_value * const valueP,
                          time_t *             const secsP,
                          unsigned int *       const usecsP) {
    
    validateDatetimeType(envP, valueP);

    if (!envP->fault_occurred) {
        if (valueP->_value.dt.Y < 1970)
            xmlrpc_faultf(envP, "Year (%u) is too early to represent as "
                          "a standard Unix time",
                          valueP->_value.dt.Y);
        else {
            struct tm brokenTime;
            const char * error;
                
            brokenTime.tm_sec  = valueP->_value.dt.s;
            brokenTime.tm_min  = valueP->_value.dt.m;
            brokenTime.tm_hour = valueP->_value.dt.h;
            brokenTime.tm_mday = valueP->_value.dt.D;
            brokenTime.tm_mon  = valueP->_value.dt.M - 1;
            brokenTime.tm_year = valueP->_value.dt.Y - 1900;
                
            xmlrpc_timegm(&brokenTime, secsP, &error);

            if (error) {
                /* Ideally, this wouldn't be possible - it wouldn't be
                   possible to create an xmlrpc_value that doesn't actually
                   represent a real datetime.  But today, we're lazy and
                   don't fully validate incoming XML-RPC <dateTime.iso8601>
                   elements, and we also have the legacy
                   xmlrpc_datetime_new_str() constructor to which the user
                   may feed garbage.

                   We should tighten that up and then simply assert here that
                   xmlrpc_timegm() succeeded.
                */
                xmlrpc_env_set_fault_formatted(envP, XMLRPC_PARSE_ERROR,
                              "A datetime received in an XML-RPC message "
                              "or generated with legacy Xmlrpc-c facilities "
                              "does not validly describe a datetime.  %s",
                              error);
                xmlrpc_strfree(error);
            } else
                *usecsP = valueP->_value.dt.u;
        }
    }
}



void
xmlrpc_read_datetime_sec(xmlrpc_env *         const envP,
                         const xmlrpc_value * const valueP,
                         time_t *             const timeValueP) {
    
    unsigned int usecs;

    xmlrpc_read_datetime_usec(envP, valueP, timeValueP, &usecs);
}



#if XMLRPC_HAVE_TIMEVAL

void
xmlrpc_read_datetime_timeval(xmlrpc_env *         const envP,
                             const xmlrpc_value * const valueP,
                             struct timeval *     const timeValueP) {
    
    time_t secs;
    unsigned int usecs;

    xmlrpc_read_datetime_usec(envP, valueP, &secs, &usecs);

    timeValueP->tv_sec  = secs;
    timeValueP->tv_usec = usecs;
}
#endif



#if XMLRPC_HAVE_TIMESPEC

void
xmlrpc_read_datetime_timespec(xmlrpc_env *         const envP,
                              const xmlrpc_value * const valueP,
                              struct timespec *    const timeValueP) {
    
    time_t secs;
    unsigned int usecs;

    xmlrpc_read_datetime_usec(envP, valueP, &secs, &usecs);

    timeValueP->tv_sec  = secs;
    timeValueP->tv_nsec = usecs * 1000;
}
#endif



xmlrpc_value *
xmlrpc_datetime_new(xmlrpc_env *    const envP, 
                    xmlrpc_datetime const dt) {

    xmlrpc_value * valP;

    const char ** readBufferP;
        
    MALLOCVAR(readBufferP);

    if (!readBufferP)
        xmlrpc_faultf(envP, "Couldn't get memory for the cache part of the "
                      "XML-RPC datetime value object");

    else {
        *readBufferP = NULL;

        xmlrpc_createXmlrpcValue(envP, &valP);

        if (!envP->fault_occurred) {
            valP->_type = XMLRPC_TYPE_DATETIME;
            
            valP->_value.dt = dt;

            valP->_cache = readBufferP;
        }
        if (envP->fault_occurred)
            free(readBufferP);
    }
    return valP;
}



static void
parseDatetimeString(const char *      const datetimeString,
                    xmlrpc_datetime * const dtP) {

    size_t const dtStrlen = strlen(datetimeString);

    char year[4+1];
    char month[2+1];
    char day[2+1];
    char hour[2+1];
    char minute[2+1];
    char second[2+1];

    /* Because we require input to be valid: */
    assert(dtStrlen >= 17 && dtStrlen != 18 && dtStrlen <= 24);

    year[0]   = datetimeString[ 0];
    year[1]   = datetimeString[ 1];
    year[2]   = datetimeString[ 2];
    year[3]   = datetimeString[ 3];
    year[4]   = '\0';

    month[0]  = datetimeString[ 4];
    month[1]  = datetimeString[ 5];
    month[2]  = '\0';

    day[0]    = datetimeString[ 6];
    day[1]    = datetimeString[ 7];
    day[2]    = '\0';

    assert(datetimeString[ 8] == 'T');

    hour[0]   = datetimeString[ 9];
    hour[1]   = datetimeString[10];
    hour[2]   = '\0';

    assert(datetimeString[11] == ':');

    minute[0] = datetimeString[12];
    minute[1] = datetimeString[13];
    minute[2] = '\0';

    assert(datetimeString[14] == ':');

    second[0] = datetimeString[15];
    second[1] = datetimeString[16];
    second[2] = '\0';

    if (dtStrlen > 17) {
        size_t const pad = 24 - dtStrlen;
        size_t i;

        dtP->u = atoi(&datetimeString[18]);
        for (i = 0; i < pad; ++i)
            dtP->u *= 10;
    } else
        dtP->u = 0;

    dtP->Y = atoi(year);
    dtP->M = atoi(month);
    dtP->D = atoi(day);
    dtP->h = atoi(hour);
    dtP->m = atoi(minute);
    dtP->s = atoi(second);
}



static void
validateFirst17(xmlrpc_env * const envP,
                const char * const dt) {
/*----------------------------------------------------------------------------
   Assuming 'dt' is at least 17 characters long, validate that the first
   17 characters are a valid XML-RPC datetime, e.g.
   "20080628T16:35:02"
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < 8 && !envP->fault_occurred; ++i)
        if (!isdigit(dt[i]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", dt[i]);

    if (dt[8] != 'T')
        xmlrpc_faultf(envP, "9th character is '%c', not 'T'", dt[8]);
    if (!isdigit(dt[9]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[9]);
    if (!isdigit(dt[10]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[10]);
    if (dt[11] != ':')
        xmlrpc_faultf(envP, "Not a colon: '%c'", dt[11]);
    if (!isdigit(dt[12]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[12]);
    if (!isdigit(dt[13]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[13]);
    if (dt[14] != ':')
        xmlrpc_faultf(envP, "Not a colon: '%c'", dt[14]);
    if (!isdigit(dt[15]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[15]);
    if (!isdigit(dt[16]))
        xmlrpc_faultf(envP, "Not a digit: '%c'", dt[16]);
}



static void
validateFractionalSeconds(xmlrpc_env * const envP,
                          const char * const dt) {
/*----------------------------------------------------------------------------
   Validate the fractional seconds part of the XML-RPC datetime string
   'dt', if any.  That's the decimal point and everything following
   it.
-----------------------------------------------------------------------------*/
    if (strlen(dt) > 17) {
        if (dt[17] != '.') {
            xmlrpc_faultf(envP, "'%c' where only a period is valid", dt[17]);
        } else {
            if (dt[18] == '\0')
                xmlrpc_faultf(envP, "Nothing after decimal point");
            else {
                unsigned int i;
                for (i = 18; dt[i] != '\0' && !envP->fault_occurred; ++i) {
                    if (!isdigit(dt[i]))
                        xmlrpc_faultf(envP,
                                      "Non-digit in fractional seconds: '%c'",
                                      dt[i]);
                }
            }
        }
    }
}



static void
validateFormat(xmlrpc_env * const envP,
               const char * const dt) {

    if (strlen(dt) < 17)
        xmlrpc_faultf(envP,
                      "Invalid length of %u of datetime string.  "
                      "Must be at least 17 characters",
                      (unsigned)strlen(dt));
    else {
        validateFirst17(envP, dt);

        if (!envP->fault_occurred)
            validateFractionalSeconds(envP, dt);
    }
}



/* Microsoft Visual C in debug mode produces code that complains about
   returning an undefined value from xmlrpc_datetime_new_str().  It's a bogus
   complaint, because this function is defined to return nothing meaningful
   those cases.  So we disable the check.
*/
#pragma runtime_checks("u", off)



xmlrpc_value *
xmlrpc_datetime_new_str(xmlrpc_env * const envP, 
                        const char * const datetimeString) {
/*----------------------------------------------------------------------------
   This exists only for backward compatibility.  Originally, this was the
   only way to create a datetime XML-RPC value, because we had a really
   lazy implementation of XML-RPC serialization and parsing (basically, the
   user did it!).

   Do not extend this.  The user should use more normal C representations
   of datetimes.
-----------------------------------------------------------------------------*/
    xmlrpc_value * retval;

    validateFormat(envP, datetimeString);
    if (!envP->fault_occurred) {
        xmlrpc_datetime dt;

        parseDatetimeString(datetimeString, &dt);

        /* Note that parseDatetimeString() can generate an invalid datetime
           value, e.g. Hour 25 or February 30.  Ideally, we would catch that
           here, but due to laziness, we simply accept the possibility of
           invalid xmlrpc_datetime in xmlrpc_value and whoever uses the the
           xmlrpc_value has to deal with it.
        */
        retval = xmlrpc_datetime_new(envP, dt);
    }

    return retval;
}



#pragma runtime_checks("u", restore)



xmlrpc_value *
xmlrpc_datetime_new_usec(xmlrpc_env * const envP,
                         time_t       const secs,
                         unsigned int const usecs) {

    xmlrpc_value * valueP;

    if (usecs >= 1000000)
        xmlrpc_faultf(envP, "Number of fractional microseconds must be less "
                      "than one million.  You specified %u", usecs);
    else {
        struct tm brokenTime;
        xmlrpc_datetime dt;

        xmlrpc_gmtime(secs, &brokenTime);

        dt.s = brokenTime.tm_sec;
        dt.m = brokenTime.tm_min;
        dt.h = brokenTime.tm_hour;
        dt.D = brokenTime.tm_mday;
        dt.M = brokenTime.tm_mon + 1;
        dt.Y = 1900 + brokenTime.tm_year;
        dt.u = usecs;

        valueP = xmlrpc_datetime_new(envP, dt);
    }
    return valueP;
}



xmlrpc_value *
xmlrpc_datetime_new_sec(xmlrpc_env * const envP, 
                        time_t       const value) {

    return xmlrpc_datetime_new_usec(envP, value, 0);
}



#if XMLRPC_HAVE_TIMEVAL

xmlrpc_value *
xmlrpc_datetime_new_timeval(xmlrpc_env *   const envP, 
                            struct timeval const value) {

    return xmlrpc_datetime_new_usec(envP, value.tv_sec, value.tv_usec);
}
#endif



#if XMLRPC_HAVE_TIMESPEC

xmlrpc_value *
xmlrpc_datetime_new_timespec(xmlrpc_env *    const envP, 
                             struct timespec const value) {

    return xmlrpc_datetime_new_usec(envP, value.tv_sec, value.tv_nsec/1000);
}
#endif



void
xmlrpc_destroyDatetime(xmlrpc_value * const datetimeP) {

    const char ** const readBufferP = datetimeP->_cache;

    if (*readBufferP)
        xmlrpc_strfree(*readBufferP);

    free(datetimeP->_cache);
}
