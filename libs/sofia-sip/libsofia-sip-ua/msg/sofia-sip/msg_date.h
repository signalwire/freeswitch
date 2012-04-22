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

#ifndef MSG_DATE_H
/** Defined when <sofia-sip/msg_date.h> has been included. */
#define MSG_DATE_H

/**@ingroup msg_parser
 * @file sofia-sip/msg_date.h
 * @brief Types and functions for handling dates and times.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun  8 19:28:55 2000 ppessi
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

#ifndef MSG_TIME_T_DEFINED
#define MSG_TIME_T_DEFINED
/** Time in seconds since epoch (1900-Jan-01 00:00:00). */
typedef unsigned long msg_time_t;
#endif

#ifndef MSG_TIME_MAX
/** Latest time that can be expressed with msg_time_t. @HIDE */
#define MSG_TIME_MAX ((msg_time_t)ULONG_MAX)
#endif

/* Current time. */
SOFIAPUBFUN msg_time_t msg_now(void);

SOFIAPUBFUN issize_t msg_date_delta_d(char const **inout_string,
				      msg_time_t *return_date,
				      msg_time_t *return_delta);

SOFIAPUBFUN issize_t msg_delta_d(char const **ss, msg_time_t *return_delta);
SOFIAPUBFUN issize_t msg_delta_e(char b[], isize_t bsiz, msg_time_t delta);

/** Decode RFC1123-date, RFC822-date or asctime-date. */
SOFIAPUBFUN issize_t msg_date_d(char const **ss, msg_time_t *date);

/** Encode RFC1123-date. */
SOFIAPUBFUN issize_t msg_date_e(char b[], isize_t bsiz, msg_time_t date);

enum { msg_date_string_size = 29 };

SOFIA_END_DECLS

#endif /* !defined(MSG_DATE_H) */
