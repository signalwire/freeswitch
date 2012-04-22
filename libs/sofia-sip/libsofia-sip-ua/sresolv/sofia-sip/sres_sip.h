/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifndef SOFIA_SIP_SRES_SIP_H
/** Defined when <sofia-sip/sres_sip.h> has been included. */
#define SOFIA_SIP_SRES_SIP_H

/**@file sofia-sip/sres_sip.h
 * @brief Asynchronous resolver for SIP
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#ifndef SU_ADDRINFO_H
#include <sofia-sip/su_addrinfo.h>
#endif

#ifndef URL_H
#include <sofia-sip/url.h>
#endif

#ifndef SOFIA_RESOLV_SRES_H
#include <sofia-resolv/sres.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct sres_sip_s   sres_sip_t;

#ifndef SRES_SIP_MAGIC_T
/** Default type of application context for NTA resolver.
 *
 * Application may define this to appropriate type before including
 * <sofia-sip/sres_sip.h>. */
#define SRES_SIP_MAGIC_T struct sres_sip_magic_s
#endif

/** Application context for NTA resolver. */
typedef SRES_SIP_MAGIC_T     sres_sip_magic_t;

typedef void sres_sip_notify_f(sres_sip_magic_t *context,
			       sres_sip_t *resolver,
			       int error);

SOFIAPUBFUN sres_sip_t *sres_sip_new(
  sres_resolver_t *sres,
  url_string_t const *url,
  su_addrinfo_t const *hints,
  int naptr, int srv,
  sres_sip_notify_f *callback,
  sres_sip_magic_t *magic);

SOFIAPUBFUN sres_sip_t *sres_sip_ref(sres_sip_t *);
SOFIAPUBFUN void sres_sip_unref(sres_sip_t *);

SOFIAPUBFUN su_addrinfo_t const *sres_sip_results(sres_sip_t *);

SOFIAPUBFUN su_addrinfo_t const *sres_sip_next(sres_sip_t *);

SOFIAPUBFUN int sres_sip_next_step(sres_sip_t *nr);

SOFIAPUBFUN int sres_sip_error(sres_sip_t *nr);

/* Errors */
enum {
  SRES_SIP_ERR_FAULT = -1,	/* Invalid pointers */
  SRES_SIP_ERR_BAD_URI = -2,	/* Invalid URI */
  SRES_SIP_ERR_BAD_HINTS = -3,	/* Invalid hints */
  SRES_SIP_ERR_NO_NAME = -4,	/* No domain found */
  SRES_SIP_ERR_NO_DATA = -5,	/* No matching records */
  SRES_SIP_ERR_NO_TPORT = -6,	/* Unknown transport */
  SRES_SIP_ERR_FAIL = -7,	/* Permanent resolving error */
  SRES_SIP_ERR_AGAIN = -8,	/* Temporary resolving error */
  SRES_SIP_ERR_INTERNAL = -9,	/* Internal error */
  _SRES_SIP_ERR_LAST
};

/* Well-known transport numbers */
enum {
  TPPROTO_TCP =  6,
  TPPROTO_UDP = 17,
  TPPROTO_SCTP = 132,
  TPPROTO_SECURE = 256,
  TPPROTO_TLS = TPPROTO_SECURE | TPPROTO_TCP,
  TPPROTO_NONE = 0
};

#define TPPROTO_TCP  TPPROTO_TCP
#define TPPROTO_UDP  TPPROTO_UDP
#define TPPROTO_SCTP TPPROTO_SCTP
#define TPPROTO_TLS  TPPROTO_TLS
#define TPPROTO_NONE TPPROTO_NONE

SOFIA_END_DECLS

#endif	/* SOFIA_SIP_SRES_SIP_H */
