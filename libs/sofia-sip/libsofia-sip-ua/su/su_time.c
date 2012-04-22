/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2006 Nokia Corporation.
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
 * @CFILE su_time.c
 *
 * @brief Implementation of OS-independent time functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Jari Selin <Jari.Selin@nokia.com>
 * @author Kai Vehmanen <first.surname@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <time.h>

#include "sofia-sip/su_types.h"
#include "su_module_debug.h"

/* Include bodies of inlined functions */
#undef su_inline
#define su_inline
#undef SU_HAVE_INLINE
#define SU_HAVE_INLINE 1

#include "sofia-sip/su_time.h"

#if HAVE_SYS_TIME_H
#include <sys/time.h> /* Get struct timeval */
#endif

/**@defgroup su_time Time Handling
 *
 * OS-independent timing functions and types for the @b su library.
 *
 * The @b su library provides three different time formats with different
 * ranges and epochs in <sofia-sip/su_time.h>:
 *
 *   - #su_time_t, second and microsecond as 32-bit values since 1900,
 *   - #su_duration_t, milliseconds between two times, and
 *   - #su_ntp_t, standard NTP timestamp (seconds since 1900 as
 *     a fixed-point 64-bit value with 32 bits representing subsecond
 *     value).
 */

/**
 * Compare two timestamps.
 *
 * The function su_time_cmp() compares two su_time_t timestamps.
 *
 * @param t1 first NTP timestamp in su_time_t structure
 * @param t2 second NTP timestamp in su_time_t structure
 *
 * @retval  Negative, if @a t1 is before @a t2,
 * @retval  Zero, if @a t1 is same as @a t2, or
 * @retval  Positive, if @a t1 is after @a t2.
 *
 */
long su_time_cmp(su_time_t const t1, su_time_t const t2)
{
  long retval = 0;

  if (t1.tv_sec > t2.tv_sec)
    retval = 1;
  else if (t1.tv_sec < t2.tv_sec)
    retval = -1;
  else {
    if (t1.tv_usec > t2.tv_usec)
      retval = 1;
    else if (t1.tv_usec < t2.tv_usec)
      retval = -1;
  }

  return retval;
}

/**@def SU_TIME_CMP(t1, t2)
 *
 * Compare two timestamps.
 *
 * The macro SU_TIME_CMP() compares two su_time_t timestamps.
 *
 * @param t1   first NTP timestamp in su_time_t structure
 * @param t2   second NTP timestamp in su_time_t  structure
 *
 * @retval negative, if t1 is before t2,
 * @retval zero,     if t1 is same as t2, or
 * @retval positive, if t1 is after t2.
 *
 * @hideinitializer
 */

/** Difference between two timestamps.
 *
 * The function returns difference between two timestamps
 * in seconds (t1 - t2).
 *
 * @param t1   first timeval
 * @param t2   second timeval
 *
 * @return
 *    The difference between two timestamps in seconds as a double.
 */
double su_time_diff(su_time_t const t1, su_time_t const t2)
{
  return
    ((double)t1.tv_sec - (double)t2.tv_sec)
    + (double)((long)t1.tv_usec - (long)t2.tv_usec) / 1000000.0;
}

/** Get current time.
 *
 *   Return the current timestamp in su_time_t structure.
 *
 * @return
 *   The structure containing the current NTP timestamp.
 */
su_time_t su_now(void)
{
  su_time_t retval;
  su_time(&retval);
  return retval;
}

/** Print su_time_t timestamp.
 *
 *   This function prints a su_time_t timestamp as a decimal number to the
 *   given buffer.
 *
 * @param s    pointer to buffer
 * @param n    buffer size
 * @param tv   pointer to the timeval object
 *
 * @return
 *   The number of characters printed, excluding the final @c NUL.
 */
int su_time_print(char *s, int n, su_time_t const *tv)
{
#ifdef _WIN32
  return _snprintf(s, n, "%lu.%06lu", tv->tv_sec, tv->tv_usec);
#else
  return snprintf(s, n, "%lu.%06lu", tv->tv_sec, tv->tv_usec);
#endif
}

/** Time difference in milliseconds.
 *
 * Calculates the duration from t2 to t1 in milliseconds.
 *
 * @param t1   after time
 * @param t2   before time
 *
 * @return The duration in milliseconds between the two times.
 * If the difference is bigger than #SU_DURATION_MAX, return #SU_DURATION_MAX
 * instead.
 * If the difference is smaller than -#SU_DURATION_MAX, return
 * -#SU_DURATION_MAX.
 */
su_duration_t su_duration(su_time_t const t1, su_time_t const t2)
{
  su_duration_t diff, udiff, tdiff;

  diff = t1.tv_sec - t2.tv_sec;
  udiff = t1.tv_usec - t2.tv_usec;

  tdiff = diff * 1000 + udiff / 1000;

  if (diff > (SU_DURATION_MAX / 1000) || (diff > 0 && diff > tdiff))
    return SU_DURATION_MAX;
  if (diff < (-SU_DURATION_MAX / 1000) || (diff < 0 && diff < tdiff))
    return -SU_DURATION_MAX;

  return tdiff;
}

typedef uint64_t su_t64_t;	/* time with 64 bits */

static su_time_t su_t64_to_time(su_t64_t const us);

const uint32_t su_res32 = 1000000UL;
const su_t64_t su_res64 = (su_t64_t)1000000UL;

#define SU_TIME_TO_T64(tv) \
  (su_res64 * (su_t64_t)(tv).tv_sec + (su_t64_t)(tv).tv_usec)
#define SU_DUR_TO_T64(d) (1000 * (int64_t)(d))

#define E9 (1000000000)

/** Get NTP timestamp.
 *
 * The function su_ntp_now() returns the current NTP timestamp.  NTP
 * timestamp is seconds elapsed since January 1st, 1900.
 *
 * @return
 * The current time as NTP timestamp is returned.
 */
su_ntp_t su_ntp_now(void)
{
  su_nanotime_t now;

  su_nanotime(&now);

  if (sizeof(long) == sizeof(now)) {
    return ((now / E9) << 32) | ((((now % E9) << 32) + E9/2) / E9);
  }
  else {
    su_nanotime_t usec;
    int nsec;
    unsigned rem;

    usec = now / 1000;
    nsec = now % 1000;

    /*
     * Multiply usec by 4294.967296 (ie. 2**32 / 1E6)
     *
     * Utilize fact that 4294.967296 == 4295 - 511 / 15625
     */
    now = 4295 * usec - 511 * usec / 15625;
    rem = (511U * usec) % 15625U;

    /* Multiply nsec by 4.294967296 */
    nsec = 4295 * 125 * nsec - (511 * nsec) / 125;
    nsec -= 8 * rem; /* usec rounding */
    nsec = (nsec + (nsec < 0 ? -62499 : +62499)) / 125000;

    return now + nsec;
  }
}

/** Get NTP seconds.
 *
 * The function su_ntp_sec() returns the seconds elapsed since January 1st,
 * 1900.
 *
 * @return
 * The current time as NTP seconds is returned.
 */
uint32_t su_ntp_sec(void)
{
  su_time_t t;
  su_time(&t);
  return t.tv_sec;
}

/** High 32 bit of NTP timestamp.
 *
 * @param ntp 64bit NTP timestamp.
 *
 * @return The function su_ntp_hi() returns high 32 bits of NTP timestamp.
 */
uint32_t su_ntp_hi(su_ntp_t ntp)
{
  return (uint32_t) (ntp >> 32) & 0xffffffffLU;
}

su_ntp_t su_ntp_hilo(uint32_t hi, uint32_t lo)
{
  return ((((su_ntp_t)hi) << 32)) | lo;
}

/** Low 32 bit of NTP timestamp.
 *
 * @param ntp 64bit NTP timestamp.
 *
 * @return The function su_ntp_hi() returns low 32 bits of NTP timestamp.
 */
uint32_t su_ntp_lo(su_ntp_t ntp)
{
  return (uint32_t) ntp & 0xffffffffLU;
}

/** Middle 32 bit of NTP timestamp.
 *
 * @param ntp 64bit NTP timestamp.
 *
 * @return The function su_ntp_mw() returns bits 48..16 (middle word) of NTP
 * timestamp.
 */
uint32_t su_ntp_mw(su_ntp_t ntp)
{
  return (uint32_t) (ntp >> 16) & 0xffffffffLU;
}

static
su_time_t su_t64_to_time(su_t64_t const us)
{
  su_time_t tv;

  tv.tv_sec = (unsigned long) (us / su_res64);
  tv.tv_usec = (unsigned long) (us % su_res64);

  return tv;
}

/**
 * Add milliseconds to the time.
 *
 * @param t0  time in seconds and microseconds as @c su_time_t
 * @param dur milliseconds to be added
 */
su_time_t su_time_add(su_time_t t0, su_duration_t dur)
{
  return su_t64_to_time(SU_TIME_TO_T64(t0) + SU_DUR_TO_T64(dur));
}

/**
 * Add seconds to the time.
 *
 * @param t0  time in seconds and microseconds as @c su_time_t
 * @param sec seconds to be added
 *
 * @return
 * New time as @c su_time_t.
 */
su_time_t su_time_dadd(su_time_t t0, double sec)
{
  return su_t64_to_time(SU_TIME_TO_T64(t0) + (int64_t)(su_res32 * sec));
}

#ifdef WIN32

#include <windows.h>

uint64_t su_nanocounter(void)
{
  static ULONGLONG counterfreq = 0;  /*performance counter frequency*/
  LARGE_INTEGER LargeIntCount = { 0, 0 };
  ULONGLONG count;

  if(!counterfreq) {
    LARGE_INTEGER Freq = { 0, 0 };
    if (!QueryPerformanceFrequency(&Freq)) {
      SU_DEBUG_1(("su_counter: QueryPerformanceFrequency failed\n"));
      return 0;
    }
    counterfreq = Freq.QuadPart;
  }

  if (!QueryPerformanceCounter(&LargeIntCount)) {
     SU_DEBUG_1(("su_counter: QueryPeformanceCounter failed\n"));
     return 0;
  }
  count = (ULONGLONG) LargeIntCount.QuadPart;

  /* return value is in ns */
  return  ((count * 1000 * 1000 * 1000) / counterfreq) ;
}

uint64_t su_counter(void)
{
  return su_nanocounter() / 1000U;
}

#elif HAVE_CLOCK_GETTIME

/** Return CPU counter value in nanoseconds.
 *
 * The function su_nanocounter() returns a CPU counter value in nanoseconds
 * for timing purposes.
 *
 * Parameters:
 *
 * @return
 *   The current CPU counter value in nanoseconds
 */
uint64_t su_nanocounter(void)
{
  struct timespec tp;
  struct timeval tv;
  static int init = 0;
  static clockid_t cpu = CLOCK_REALTIME;

# define CLOCK_GETTIMEOFDAY 0xdedbeefUL

  if (init == 0) {
    init = 1;

    if (0)
      ;
#if HAVE_CLOCK_GETCPUCLOCKID
    else if (clock_getcpuclockid(0, &cpu) != -1 &&
	     clock_gettime(cpu, &tp) != -1)
      ;
#endif
#ifdef _POSIX_CPUTIME
    else if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp) >= 0)
      cpu = CLOCK_PROCESS_CPUTIME_ID;
#endif
    else if (clock_gettime(CLOCK_REALTIME, &tp) >= 0)
      cpu = CLOCK_REALTIME;
    else
      cpu = CLOCK_GETTIMEOFDAY;
  }

  if (cpu == CLOCK_GETTIMEOFDAY) {
    gettimeofday(&tv, NULL);
    tp.tv_sec = tv.tv_sec, tp.tv_nsec = tv.tv_usec * 1000;
  }
  else if (clock_gettime(cpu, &tp) < 0)
    perror("clock_gettime");

  /* return value is in nanoseconds */
  return
    (uint64_t)((unsigned long)tp.tv_nsec) +
    (uint64_t)((unsigned long)tp.tv_sec) * 1000000000ULL;
}

/** Return CPU counter value in microseconds.
 *
 * The function su_counter() returns the CPU counter value in microseconds
 * for timing purposes.
 *
 * Parameters:
 *
 * @return
 *   The current CPU counter value in microseconds.
 */
uint64_t su_counter(void)
{
  return su_nanocounter() / 1000U;
}

#elif HAVE_GETTIMEOFDAY
uint64_t su_counter(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  /* return value is in microseconds */
  return
    (uint64_t)((unsigned long)tv.tv_usec) +
    (uint64_t)((unsigned long)tv.tv_sec) * 1000000ULL;
}

uint64_t su_nanocounter(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  /* return value is in nanoseconds */
  return
    (uint64_t)((unsigned long)tv.tv_usec) * 1000ULL +
    (uint64_t)((unsigned long)tv.tv_sec) * 1000000000ULL;
}

#else

uint64_t su_counter(void)
{
  return (uint64_t)clock();
}

uint64_t su_nanocounter(void)
{
  return (uint64_t)clock() * 1000;
}

#endif /* WIN32 */
