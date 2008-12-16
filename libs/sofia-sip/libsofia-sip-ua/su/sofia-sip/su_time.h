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

#ifndef SU_TIME_H
/** Defined when <sofia-sip/su_time.h> has been included. */
#define SU_TIME_H
/**@ingroup su_time
 * @file sofia-sip/su_time.h
 * @brief Time types and functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 *
 */

#ifndef SU_TYPES_H
#include "sofia-sip/su_types.h"
#endif

SOFIA_BEGIN_DECLS

/** Time in seconds and microsecondcs.
 *
 * The structure su_time_t contains time in seconds and microseconds since
 * epoch (January 1, 1900).
 */
struct su_time_s {
  unsigned long tv_sec;		/**< Seconds */
  unsigned long tv_usec;	/**< Microseconds  */
};
/** Time in seconds and microsecondcs. */
typedef struct su_time_s su_time_t;

/** Time difference in microseconds.
 *
 * The type su_duration_t is used to present small time differences (24
 * days), usually calculated between two su_time_t timestamps.  Note that
 * the su_duration_t is signed.
 */
typedef long su_duration_t;

enum {
  /** Maximum duration in milliseconds. */
  SU_DURATION_MAX = 0x7fffffffL
};
#define SU_DURATION_MAX SU_DURATION_MAX

/** NTP timestamp.
 *
 * NTP timestamp is defined as microseconds since epoch (1-Jan-1900)
 * with 64-bit resolution.
 */
typedef uint64_t su_ntp_t;

/** Represent NTP constant */
#define SU_NTP_C(x) SU_U64_C(x)

#define SU_TIME_CMP(t1, t2) su_time_cmp(t1, t2)

/** Seconds from 1.1.1900 to 1.1.1970. @NEW_1_12_4. */
#define SU_TIME_EPOCH 2208988800UL

typedef uint64_t su_nanotime_t;

#define SU_E9 (1000000000U)

SOFIAPUBFUN su_nanotime_t su_nanotime(su_nanotime_t *return_time);
SOFIAPUBFUN su_nanotime_t su_monotime(su_nanotime_t *return_time);

SOFIAPUBFUN su_time_t su_now(void);
SOFIAPUBFUN void su_time(su_time_t *tv);
SOFIAPUBFUN long su_time_cmp(su_time_t const t1, su_time_t const t2);
SOFIAPUBFUN double su_time_diff(su_time_t const t1, su_time_t const t2);
SOFIAPUBFUN su_duration_t su_duration(su_time_t const t1, su_time_t const t2);

SOFIAPUBFUN su_time_t su_time_add(su_time_t t, su_duration_t dur);
SOFIAPUBFUN su_time_t su_time_dadd(su_time_t t, double dur);

SOFIAPUBFUN int su_time_print(char *s, int n, su_time_t const *tv);

#define SU_SEC_TO_DURATION(sec) ((su_duration_t)(1000 * (sec)))

SOFIAPUBFUN su_ntp_t su_ntp_now(void);
SOFIAPUBFUN uint32_t su_ntp_sec(void);
SOFIAPUBFUN uint32_t su_ntp_hi(su_ntp_t);
SOFIAPUBFUN uint32_t su_ntp_lo(su_ntp_t);
SOFIAPUBFUN uint32_t su_ntp_mw(su_ntp_t ntp);

#if !SU_HAVE_INLINE
SOFIAPUBFUN uint32_t su_ntp_fraq(su_time_t t);
SOFIAPUBFUN uint32_t su_time_ms(su_time_t t);
#else
su_inline uint32_t su_ntp_fraq(su_time_t t);
su_inline uint32_t su_time_ms(su_time_t t);
#endif

SOFIAPUBFUN su_ntp_t su_ntp_hilo(uint32_t hi, uint32_t lo);

SOFIAPUBFUN uint64_t su_counter(void);

SOFIAPUBFUN uint64_t su_nanocounter(void);

SOFIAPUBFUN uint32_t su_random(void);

#if SU_HAVE_INLINE
/** Middle 32 bit of NTP timestamp. */
su_inline uint32_t su_ntp_fraq(su_time_t t)
{
  /*
   * Multiply usec by 0.065536 (ie. 2**16 / 1E6)
   *
   * Utilize fact that 0.065536 == 1024 / 15625
   */
  return (t.tv_sec << 16) + (1024 * t.tv_usec + 7812) / 15625;
}

/** Time as milliseconds. */
su_inline uint32_t su_time_ms(su_time_t t)
{
  return t.tv_sec * 1000 + (t.tv_usec + 500) / 1000;
}
#endif

SOFIA_END_DECLS

#endif /* !defined(SU_TIME_H) */
