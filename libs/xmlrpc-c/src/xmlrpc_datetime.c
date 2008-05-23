#include "xmlrpc_config.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#if MSVCRT
#include <windows.h>
#endif

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"


/* Future work: the XMLRPC_TYPE_DATETIME xmlrpc_value should store the
   datetime as something computation-friendly, not as a string.  The
   XML-RPC XML parser should parse the string value and reject the XML if
   it isn't valid.

   But this file should remain the authority on datetimes, so the XML
   parser and builder should call on routines in here to do that.

   time_t won't work because it can't represent times before 1970 or
   after 2038.  We need to figure out something better.
*/


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
xmlrpc_read_datetime_str(xmlrpc_env *         const envP,
                         const xmlrpc_value * const valueP,
                         const char **        const stringValueP) {
    
    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred) {
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
        *stringValueP = strdup(contents);
        if (*stringValueP == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Unable to allocate space "
                "for datetime string");
    }
}



void
xmlrpc_read_datetime_str_old(xmlrpc_env *         const envP,
                             const xmlrpc_value * const valueP,
                             const char **        const stringValueP) {
    
    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred) {
        *stringValueP = XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
    }
}



static void
parseDateNumbers(const char * const t,
                 unsigned int * const YP,
                 unsigned int * const MP,
                 unsigned int * const DP,
                 unsigned int * const hP,
                 unsigned int * const mP,
                 unsigned int * const sP) {

    char year[4+1];
    char month[2+1];
    char day[2+1];
    char hour[2+1];
    char minute[2+1];
    char second[2+1];

    assert(strlen(t) == 17);

    year[0]   = t[ 0];
    year[1]   = t[ 1];
    year[2]   = t[ 2];
    year[3]   = t[ 3];
    year[4]   = '\0';

    month[0]  = t[ 4];
    month[1]  = t[ 5];
    month[2]  = '\0';

    day[0]    = t[ 6];
    day[1]    = t[ 7];
    day[2]    = '\0';

    assert(t[ 8] == 'T');

    hour[0]   = t[ 9];
    hour[1]   = t[10];
    hour[2]   = '\0';

    assert(t[11] == ':');

    minute[0] = t[12];
    minute[1] = t[13];
    minute[2] = '\0';

    assert(t[14] == ':');

    second[0] = t[15];
    second[1] = t[16];
    second[2] = '\0';

    *YP = atoi(year);
    *MP = atoi(month);
    *DP = atoi(day);
    *hP = atoi(hour);
    *mP = atoi(minute);
    *sP = atoi(second);
}



static void
validateFormat(xmlrpc_env * const envP,
               const char * const t) {

    if (strlen(t) != 17)
        xmlrpc_faultf(envP, "%u characters instead of 15.", strlen(t));
    else if (t[8] != 'T')
        xmlrpc_faultf(envP, "9th character is '%c', not 'T'", t[8]);
    else {
        unsigned int i;

        for (i = 0; i < 8 && !envP->fault_occurred; ++i)
            if (!isdigit(t[i]))
                xmlrpc_faultf(envP, "Not a digit: '%c'", t[i]);

        if (!isdigit(t[9]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[9]);
        if (!isdigit(t[10]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[10]);
        if (t[11] != ':')
            xmlrpc_faultf(envP, "Not a colon: '%c'", t[11]);
        if (!isdigit(t[12]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[12]);
        if (!isdigit(t[13]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[13]);
        if (t[14] != ':')
            xmlrpc_faultf(envP, "Not a colon: '%c'", t[14]);
        if (!isdigit(t[15]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[15]);
        if (!isdigit(t[16]))
            xmlrpc_faultf(envP, "Not a digit: '%c'", t[16]);
    }
}        



static void
parseDatetime(xmlrpc_env * const envP,
              const char * const t,
              time_t *     const timeValueP) {
/*----------------------------------------------------------------------------
   Parse a time in the format stored in an xmlrpc_value and return the
   time that it represents.

   t[] is the input time string.  We return the result as *timeValueP.

   Example of the format we parse: "19980717T14:08:55"
   Note that this is not quite ISO 8601.  It's a bizarre combination of
   two ISO 8601 formats.

   The input is capable of representing datetimes that cannot be expressed
   as a time_t.  In that case, we fail, with fault code
   XMLRPC_INTERNAL_ERROR.

   And of course the input may not validly represent a datetime at all.
   In that case too, we fail with fault code XMLRPC_PARSE_ERROR.
-----------------------------------------------------------------------------*/
    validateFormat(envP, t);

    if (!envP->fault_occurred) {
        unsigned int Y, M, D, h, m, s;
        
        parseDateNumbers(t, &Y, &M, &D, &h, &m, &s);
        
        if (Y < 1970)
            xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                                 "Year is too early to represent as "
                                 "a standard Unix time");
        else {
            struct tm brokenTime;
            const char * error;
            
            brokenTime.tm_sec  = s;
            brokenTime.tm_min  = m;
            brokenTime.tm_hour = h;
            brokenTime.tm_mday = D;
            brokenTime.tm_mon  = M - 1;
            brokenTime.tm_year = Y - 1900;
            
            xmlrpc_timegm(&brokenTime, timeValueP, &error);

            if (error) {
                xmlrpc_env_set_fault(envP, XMLRPC_PARSE_ERROR, error);
                xmlrpc_strfree(error);
            }
        }
    }
}



void
xmlrpc_read_datetime_sec(xmlrpc_env *         const envP,
                         const xmlrpc_value * const valueP,
                         time_t *             const timeValueP) {
    
    validateDatetimeType(envP, valueP);
    if (!envP->fault_occurred)
        parseDatetime(envP,
                      XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block),
                      timeValueP);
}



xmlrpc_value *
xmlrpc_datetime_new_str(xmlrpc_env * const envP, 
                        const char * const value) {

    xmlrpc_value * valP;

    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_DATETIME;

        XMLRPC_TYPED_MEM_BLOCK_INIT(
            char, envP, &valP->_block, strlen(value) + 1);
        if (!envP->fault_occurred) {
            char * const contents =
                XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &valP->_block);
            strcpy(contents, value);
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}



xmlrpc_value *
xmlrpc_datetime_new_sec(xmlrpc_env * const envP, 
                        time_t       const value) {

    xmlrpc_value * valP;
    
    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        struct tm brokenTime;
        char timeString[64];
        
        valP->_type = XMLRPC_TYPE_DATETIME;

        xmlrpc_gmtime(value, &brokenTime);
        
        /* Note that this format is NOT ISO 8601 -- it's a bizarre
           hybrid of two ISO 8601 formats.
        */
        strftime(timeString, sizeof(timeString), "%Y%m%dT%H:%M:%S", 
                 &brokenTime);
        
        XMLRPC_TYPED_MEM_BLOCK_INIT(
            char, envP, &valP->_block, strlen(timeString) + 1);
        if (!envP->fault_occurred) {
            char * const contents =
                XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &valP->_block);
            
            strcpy(contents, timeString);
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}
