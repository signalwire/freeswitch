/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "win32/fspr_arch_atime.h"
#include "fspr_portable.h"
#include "fspr_strings.h"

APR_DECLARE_DATA const char fspr_month_snames[12][4] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
APR_DECLARE_DATA const char fspr_day_snames[7][4] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

APR_DECLARE(fspr_status_t) fspr_rfc822_date(char *date_str, fspr_time_t t)
{
    fspr_time_exp_t xt;
    const char *s;
    int real_year;

    fspr_time_exp_gmt(&xt, t);

    /* example: "Sat, 08 Jan 2000 18:31:41 GMT" */
    /*           12345678901234567890123456789  */

    s = &fspr_day_snames[xt.tm_wday][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ',';
    *date_str++ = ' ';
    *date_str++ = xt.tm_mday / 10 + '0';
    *date_str++ = xt.tm_mday % 10 + '0';
    *date_str++ = ' ';
    s = &fspr_month_snames[xt.tm_mon][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    real_year = 1900 + xt.tm_year;
    /* This routine isn't y10k ready. */
    *date_str++ = real_year / 1000 + '0';
    *date_str++ = real_year % 1000 / 100 + '0';
    *date_str++ = real_year % 100 / 10 + '0';
    *date_str++ = real_year % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = xt.tm_hour / 10 + '0';
    *date_str++ = xt.tm_hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_min / 10 + '0';
    *date_str++ = xt.tm_min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_sec / 10 + '0';
    *date_str++ = xt.tm_sec % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = 'G';
    *date_str++ = 'M';
    *date_str++ = 'T';
    *date_str++ = 0;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_ctime(char *date_str, fspr_time_t t)
{
    fspr_time_exp_t xt;
    const char *s;
    int real_year;

    /* example: "Wed Jun 30 21:49:08 1993" */
    /*           123456789012345678901234  */

    fspr_time_exp_lt(&xt, t);
    s = &fspr_day_snames[xt.tm_wday][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    s = &fspr_month_snames[xt.tm_mon][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    *date_str++ = xt.tm_mday / 10 + '0';
    *date_str++ = xt.tm_mday % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = xt.tm_hour / 10 + '0';
    *date_str++ = xt.tm_hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_min / 10 + '0';
    *date_str++ = xt.tm_min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_sec / 10 + '0';
    *date_str++ = xt.tm_sec % 10 + '0';
    *date_str++ = ' ';
    real_year = 1900 + xt.tm_year;
    *date_str++ = real_year / 1000 + '0';
    *date_str++ = real_year % 1000 / 100 + '0';
    *date_str++ = real_year % 100 / 10 + '0';
    *date_str++ = real_year % 10 + '0';
    *date_str++ = 0;

    return APR_SUCCESS;
}


#ifndef _WIN32_WCE

fspr_size_t win32_strftime_extra(char *s, size_t max, const char *format,
                                const struct tm *tm) 
{
   /* If the new format string is bigger than max, the result string won't fit
    * anyway. If format strings are added, made sure the padding below is
    * enough */
    char *new_format = (char *) malloc(max + 11);
    size_t i, j, format_length = strlen(format);
    fspr_size_t return_value;
    int length_written;

    for (i = 0, j = 0; (i < format_length && j < max);) {
        if (format[i] != '%') {
            new_format[j++] = format[i++];
            continue;
        }
        switch (format[i+1]) {
            case 'C':
                length_written = fspr_snprintf(new_format + j, max - j, "%2d",
                    (tm->tm_year + 1970)/100);
                j = (length_written == -1) ? max : (j + length_written);
                i += 2;
                break;
            case 'D':
                /* Is this locale dependent? Shouldn't be...
                   Also note the year 2000 exposure here */
                memcpy(new_format + j, "%m/%d/%y", 8);
                i += 2;
                j += 8;
                break;
            case 'r':
                memcpy(new_format + j, "%I:%M:%S %p", 11);
                i += 2;
                j += 11;
                break;
            case 'R':
                memcpy(new_format + j, "%H:%M", 5);
                i += 2;
                j += 5;
                break;
            case 'T':
                memcpy(new_format + j, "%H:%M:%S", 8);
                i += 2;
                j += 8;
                break;
            case 'e':
                length_written = fspr_snprintf(new_format + j, max - j, "%2d",
                    tm->tm_mday);
                j = (length_written == -1) ? max : (j + length_written);
                i += 2;
                break;
            default:
                /* We know we can advance two characters forward here. Also
                 * makes sure that %% is preserved. */
                new_format[j++] = format[i++];
                new_format[j++] = format[i++];
        }
    }
    if (j >= max) {
        *s = '\0';  /* Defensive programming, okay since output is undefined*/
        return_value = 0;
    } else {
        new_format[j] = '\0';
        return_value = strftime(s, max, new_format, tm);
    }
    free(new_format);
    return return_value;
}

#endif


APR_DECLARE(fspr_status_t) fspr_strftime(char *s, fspr_size_t *retsize,
                                       fspr_size_t max, const char *format,
                                       fspr_time_exp_t *xt)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_sec  = xt->tm_sec;
    tm.tm_min  = xt->tm_min;
    tm.tm_hour = xt->tm_hour;
    tm.tm_mday = xt->tm_mday;
    tm.tm_mon  = xt->tm_mon;
    tm.tm_year = xt->tm_year;
    tm.tm_wday = xt->tm_wday;
    tm.tm_yday = xt->tm_yday;
    tm.tm_isdst = xt->tm_isdst;
    (*retsize) = win32_strftime_extra(s, max, format, &tm);
    return APR_SUCCESS;
#endif
}
