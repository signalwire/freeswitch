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

#ifndef SU_DEBUG_H
/** Defined when <sofia-sip/su_debug.h> has been included */
#define SU_DEBUG_H

/**@ingroup su_log
 * @file sofia-sip/su_debug.h
 * @brief SU debugging macros
 *
 * The logging levels and macros to use are defined as follows:
 *  - SU_DEBUG_0()  fatal errors, panic
 *  - SU_DEBUG_1()  critical errors, minimal progress at subsystem level
 *  - SU_DEBUG_2()  non-critical errors
 *  - SU_DEBUG_3()  warnings, progress messages
 *  - SU_DEBUG_5()  signaling protocol actions (incoming packets, etc.)
 *  - SU_DEBUG_7()  media protocol actions (incoming packets, etc.)
 *  - SU_DEBUG_9()  entering/exiting functions, very verbatim progress
 *
 * These macros are used to log with module-specific levels. The SU_LOG
 * macro is redefined with a pointer to a module-specific #su_log_t
 * structure, e.g., "iptsec_debug.h".
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb  8 10:06:33 2000 ppessi
 *
 * @sa @ref debug_logs, su_llog(), su_vllog(), #su_log_t,
 */

#ifndef SU_LOG_H
#include <sofia-sip/su_log.h>
#endif

SOFIA_BEGIN_DECLS

#ifndef SU_DEBUG_MAX
/** The maximum debugging level. */
#define SU_DEBUG_MAX 9
#endif

#define SU_LOG_LEVEL \
((SU_LOG != NULL && SU_LOG->log_init) == 0 ? 9 : \
((SU_LOG != NULL && SU_LOG->log_init > 1) ? \
  SU_LOG->log_level : su_log_default->log_level))

#if SU_DEBUG_MAX >= 0
#ifndef SU_LOG
#define SU_LOG       (su_log_default)
#else
SOFIAPUBVAR su_log_t SU_LOG[];
#endif

#define SU_DEBUG_DEF(level) \
  su_inline void su_debug_##level(char const *fmt, ...) \
    __attribute__ ((__format__ (printf, 1, 2))); \
  su_inline void su_debug_##level(char const *fmt, ...) \
    { va_list ap; va_start(ap, fmt); su_vllog(SU_LOG, level, fmt, ap); va_end(ap); }

SU_DEBUG_DEF(0)
/** Log messages at level 0.
 *
 * Fatal errors and panic messages should be logged at level 0.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_0(x) (SU_LOG_LEVEL >= 0 ? (su_debug_0 x) : (void)0)

/** Log C library errors. */
#define SU_LERROR(s) (su_llog(SU_LOG, 1, "%s: %s\n", (s), strerror(errno)))
/** Log socket errors. */
#define SU_LSERROR(s) \
  (su_llog(SU_LOG, 1, "%s: %s\n", (s), su_strerror(su_errno())))
#else
#define SU_DEBUG_0(x) ((void)0)
#define SU_LERROR(s)  ((void)0)
#define SU_LSERROR(s) ((void)0)
#endif

#if SU_DEBUG_MAX >= 1
SU_DEBUG_DEF(1)
/**Log messages at level 1.
 *
 * Critical errors and minimal progress at subsystem level should be logged
 * at level 1.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_1(x) (SU_LOG_LEVEL >= 1 ? (su_debug_1 x) : (void)0)
#else
#define SU_DEBUG_1(x) (void)1
#endif

#if SU_DEBUG_MAX >= 2
SU_DEBUG_DEF(2)
/**Log messages at level 2.
 *
 * Non-critical errors should be logged at level 2.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_2(x) (SU_LOG_LEVEL >= 2 ? (su_debug_2 x) : (void)0)
#else
#define SU_DEBUG_2(x) (void)2
#endif

#if SU_DEBUG_MAX >= 3
SU_DEBUG_DEF(3)
/** Log messages at level 3.
 *
 * Warnings and progress messages should be logged at level 3.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_3(x) (SU_LOG_LEVEL >= 3 ? (su_debug_3 x) : (void)0)
#else
#define SU_DEBUG_3(x) (void)3
#endif

#if SU_DEBUG_MAX >= 4
SU_DEBUG_DEF(4)
/** Log messages at level 4. */
#define SU_DEBUG_4(x) (SU_LOG_LEVEL >= 4 ? (su_debug_4 x) : (void)0)
#else
#define SU_DEBUG_4(x) (void)4
#endif

#if SU_DEBUG_MAX >= 5
SU_DEBUG_DEF(5)
/** Log messages at level 5.
 *
 * Signaling protocol actions (incoming packets, etc.) should be logged
 * at level 5.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_5(x) (SU_LOG_LEVEL >= 5 ? (su_debug_5 x) : (void)0)
#else
#define SU_DEBUG_5(x) (void)5
#endif

#if SU_DEBUG_MAX >= 6
SU_DEBUG_DEF(6)
/** Log messages at level 6. */
#define SU_DEBUG_6(x) (SU_LOG_LEVEL >= 6 ? (su_debug_6 x) : (void)0)
#else
#define SU_DEBUG_6(x) (void)6
#endif

#if SU_DEBUG_MAX >= 7
SU_DEBUG_DEF(7)
/** Log messages at level 7.
 *
 * Media protocol actions (incoming packets, etc) should be logged at level 7.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_7(x) (SU_LOG_LEVEL >= 7 ? (su_debug_7 x) : (void)0)
#else
#define SU_DEBUG_7(x) (void)7
#endif

#if SU_DEBUG_MAX >= 8
SU_DEBUG_DEF(8)
/** Log messages at level 8. */
#define SU_DEBUG_8(x) (SU_LOG_LEVEL >= 8 ? (su_debug_8 x) : (void)0)
#else
#define SU_DEBUG_8(x) (void)8
#endif

#if SU_DEBUG_MAX >= 9
SU_DEBUG_DEF(9)
/** Log messages at level 9.
 *
 * Entering/exiting functions, very verbatim progress should be logged at
 * level 9.
 *
 * @sa su_llog(), su_vllog(), #su_log_t, @ref debug_logs
 */
#define SU_DEBUG_9(x) (SU_LOG_LEVEL >= 9 ? (su_debug_9 x) : (void)0)
#else
#define SU_DEBUG_9(x) (void)9
#endif

SOFIA_END_DECLS

#endif /* SU_DEBUG_H */
