/*
 * SpanDSP - a series of DSP components for telephony
 *
 * timezone_tests.c - Timezone handling for time interpretation
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \page timezone_tests_page Timezone handling tests
\section timezone_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    struct tm tms;
    struct tm *tmp = &tms;
    time_t ltime;
    tz_t *tz;

    /* Get the current time */
    ltime = time(NULL);

    /* Compute the local current time now for several localities, based on Posix tz strings */

    tz = tz_init(NULL, "GMT0GMT0,M10.5.0,M3.5.0");
    tz_localtime(tz, tmp, ltime);
    printf("Local time is %02d:%02d:%02d\n", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    printf("Time zone is %s\n", tz_tzname(tz, tmp->tm_isdst));

    tz_init(tz, "CST-8CST-8,M10.5.0,M3.5.0");
    tz_localtime(tz, tmp, ltime);
    printf("Local time is %02d:%02d:%02d\n", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    printf("Time zone is %s\n", tz_tzname(tz, tmp->tm_isdst));

    tz_init(tz, "AEST-10AEDT-11,M10.5.0,M3.5.0");
    tz_localtime(tz, tmp, ltime);
    printf("Local time is %02d:%02d:%02d\n", tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    printf("Time zone is %s\n", tz_tzname(tz, tmp->tm_isdst));

    tz_free(tz);

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
