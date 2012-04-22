#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "bool.h"
#include "int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"
#include "xmlrpc-c/abyss.h"

#include "date.h"

/*********************************************************************
** Date
*********************************************************************/

static char *_DateDay[7]=
{
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static char *_DateMonth[12]=
{
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};



void
DateToString(time_t        const datetime,
             const char ** const dateStringP) {

    struct tm brokenTime;

    xmlrpc_gmtime(datetime, &brokenTime);

    if (mktime(&brokenTime) == (time_t)-1)
        *dateStringP = NULL;
    else
        xmlrpc_asprintf(dateStringP, "%s, %02u %s %04u %02u:%02u:%02u UTC",
                        _DateDay[brokenTime.tm_wday],
                        brokenTime.tm_mday,
                        _DateMonth[brokenTime.tm_mon],
                        1900 + brokenTime.tm_year,
                        brokenTime.tm_hour,
                        brokenTime.tm_min,
                        brokenTime.tm_sec);
}



static const char *
tzOffsetStr(struct tm const tm,
            time_t    const datetime) {

    const char * retval;
    time_t timeIfUtc;
    const char * error;
    
    xmlrpc_timegm(&tm, &timeIfUtc, &error);

    if (error) {
        xmlrpc_strfree(error);
        xmlrpc_asprintf(&retval, "%s", "+????");
    } else {
        int const tzOffset = (int)(datetime - timeIfUtc);

        assert(tzOffset == datetime - timeIfUtc);

        xmlrpc_asprintf(&retval, "%+03d%02d",
                        tzOffset/3600, abs(tzOffset % 3600)/60);
    }
    return retval;
}



void
DateToLogString(time_t        const datetime,
                const char ** const dateStringP) {

    const char * tzo;
    struct tm tm;

    xmlrpc_localtime(datetime, &tm);

    tzo = tzOffsetStr(tm, datetime);

    xmlrpc_asprintf(dateStringP, "%02d/%s/%04d:%02d:%02d:%02d %s",
                    tm.tm_mday, _DateMonth[tm.tm_mon],
                    1900 + tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec,
                    tzo);

    xmlrpc_strfree(tzo);
}



void
DateDecode(const char * const dateString,
           bool *       const validP,
           time_t *     const datetimeP) {
/*----------------------------------------------------------------------------
   Return the datetime represented by 'dateString', which is in the
   format used in an HTTP header.

   We assume that format is always UTC-based; I don't know if HTTP
   actually requires that -- maybe it could be some local time.
-----------------------------------------------------------------------------*/
    int rc;
    const char * s;
    unsigned int monthOff;
    struct tm tm;
    bool error;

    s = &dateString[0];

    /* Ignore spaces, day name and spaces */
    while ((*s==' ') || (*s=='\t'))
        ++s;

    while ((*s!=' ') && (*s!='\t'))
        ++s;

    while ((*s==' ') || (*s=='\t'))
        ++s;

    error = false;  /* initial value */

    /* try to recognize the date format */
    rc = sscanf(s, "%*s %d %d:%d:%d %d%*s",
                &tm.tm_mday, &tm.tm_hour,
                &tm.tm_min, &tm.tm_sec, &tm.tm_year);
    if (rc == 5)
        monthOff = 0;
    else {
        int rc;
        rc = sscanf(s, "%d %n%*s %d %d:%d:%d GMT%*s",
                    &tm.tm_mday, &monthOff, &tm.tm_year,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        if (rc != 5) {
            int rc;
            rc = sscanf(s, "%d-%n%*[A-Za-z]-%d %d:%d:%d GMT%*s",
                        &tm.tm_mday, &monthOff, &tm.tm_year,
                        &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
            if (rc != 5)
                error = true;
        }
    }    
    if (!error) {
        const char * monthName = s + monthOff;
            /* This is actually just the point in 'dateString' where
               the month name begins -- it's not a nul-terminated string
            */

        unsigned int i;
        bool gotMonth;

        for (i = 0, gotMonth = false; i < 12; ++i) {
            const char * p;

            p =_DateMonth[i];

            if (tolower(*p++) == tolower(monthName[0]))
                if (*p++ == tolower(monthName[1]))
                    if (*p == tolower(monthName[2])) {
                        gotMonth = true;
                        tm.tm_mon = i;
                    }
        }

        if (!gotMonth)
            error = true;
        else {
            if (tm.tm_year > 1900)
                tm.tm_year -= 1900;
            else {
                if (tm.tm_year < 70)
                    tm.tm_year += 100;
            }
            tm.tm_isdst = 0;

            {
                const char * timeError;
                xmlrpc_timegm(&tm, datetimeP, &timeError);
                
                if (timeError) {
                    error = TRUE;
                    xmlrpc_strfree(timeError);
                }
            }
        }
    }
    *validP = !error;
}



abyss_bool
DateInit(void) {

    return true;
}
