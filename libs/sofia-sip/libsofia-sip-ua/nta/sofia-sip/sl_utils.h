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

#ifndef SL_UTILS_H
/** Defined when <sofia-sip/sl_utils.h> has been included. */
#define SL_UTILS_H

/**@ingroup sl_utils
 *
 * @file sofia-sip/sl_utils.h @brief Prototypes for SIP helper functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Oct  5 15:38:39 2000 ppessi
 */

#include <stdio.h>

#ifndef STRING0_H
#include <sofia-sip/su_string.h>
#endif

#ifndef SIP_H
#include <sofia-sip/sip.h>
#endif

SOFIA_BEGIN_DECLS

#ifndef SU_LOG_T
#define SU_LOG_T
typedef struct su_log_s su_log_t;
#endif

/* Read from file */
SOFIAPUBFUN sip_payload_t *sl_read_payload(su_home_t *home, char const *fname);
SOFIAPUBFUN sip_payload_t *sl_fread_payload(su_home_t *home, FILE *);

/* Printing functions */
SOFIAPUBFUN void
sl_message_log(FILE *, char const *prefix, sip_t const *, int details);
SOFIAPUBFUN issize_t
sl_header_print(FILE *, char const *fmt, sip_header_t const *h),
sl_from_print(FILE *, char const *fmt, sip_from_t const *from),
sl_to_print(FILE *, char const *fmt, sip_to_t const *to),
sl_contact_print(FILE *, char const *fmt, sip_contact_t const *m),
sl_allow_print(FILE *, char const *fmt, sip_allow_t const *g),
sl_payload_print(FILE *, char const *prefix, sip_payload_t const *pl),
sl_via_print(FILE *, char const *fmt, sip_via_t const *v);

/* Logging functions */
SOFIAPUBFUN void
sl_sip_log(su_log_t*, int lvl, char const *, sip_t const *, int details),
sl_header_log(su_log_t *, int lvl, char const *, sip_header_t const *h),
sl_from_log(su_log_t *, int lvl, char const *, sip_from_t const *from),
sl_to_log(su_log_t *, int lvl, char const *, sip_to_t const *to),
sl_contact_log(su_log_t *, int lvl, char const *, sip_contact_t const *m),
sl_allow_log(su_log_t *, int, char const *, sip_allow_t const *g),
sl_via_log(su_log_t *, int, char const *, sip_via_t const *v),
sl_payload_log(su_log_t *, int, char const *, sip_payload_t const *pl);

SOFIA_END_DECLS

#endif
