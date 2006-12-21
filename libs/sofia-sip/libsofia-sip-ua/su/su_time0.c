/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup su_time
 * @CFILE su_time0.c
 * @brief su_time() implementation
 *
 * The file su_time0.c contains implementation of OS-independent wallclock
 * time with microsecond resolution.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Jari Selin <Jari.Selin@nokia.com>
 * 
 * @date Created: Fri May 10 18:13:19 2002 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "sofia-sip/su_types.h"
#include "sofia-sip/su_time.h"
#include "su_module_debug.h"

#if HAVE_SYS_TIME_H
#include <sys/time.h> /* Get struct timeval */
#endif

#if defined(__MINGW32__)
#define HAVE_FILETIME 1
#include <windows.h>
#endif

#if HAVE_FILETIME
#define HAVE_FILETIME 1
#include <windows.h>
#endif

/** Seconds from 1.1.1900 to 1.1.1970 */
#define NTP_EPOCH 2208988800UL 

/** Get current time.
 *
 * The function @c su_time() fills its argument with the current NTP
 * timestamp expressed as a su_time_t structure.
 *
 * @param tv pointer to the timeval object
 */
void su_time(su_time_t *tv)
{
#if HAVE_GETTIMEOFDAY
  if (tv) {
    gettimeofday((struct timeval *)tv, NULL);
    tv->tv_sec += NTP_EPOCH;
  }
#elif HAVE_FILETIME
  union {
    FILETIME       ft[1];
    ULARGE_INTEGER ull[1];
  } date;

  GetSystemTimeAsFileTime(date.ft);

  tv->tv_usec = (unsigned long) ((date.ull->QuadPart % 10000000U) / 10);
  tv->tv_sec = (unsigned long) ((date.ull->QuadPart / 10000000U) - 
    /* 1900-Jan-01 - 1601-Jan-01: 299 years, 72 leap years */
    (299 * 365 + 72) * 24 * 60 * (uint64_t)60);
#else
#error no su_time() implementation
#endif
}
