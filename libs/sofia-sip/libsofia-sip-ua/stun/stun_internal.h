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

#ifndef STUN_INTERNAL_H
/** Defined when <stun_internal.h> has been included. */
#define STUN_INTERNAL_H
/**@file stun_internal.h STUN client interface
 *
 * @author Martti Mela <martti.mela@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 */

#ifndef SU_DEBUG
#define SU_DEBUG 0
#endif

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#ifndef STUN_H
#include "sofia-sip/stun.h"
#endif

#if defined(HAVE_OPENSSL)
/* avoid krb5-related build failures */
#define OPENSSL_NO_KRB5
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

#ifndef STUN_COMMON_H
#include "sofia-sip/stun_common.h"
#endif



#define SU_LOG (stun_log)
#include <sofia-sip/su_debug.h>

#define enter (void)SU_DEBUG_9(("%s: entering.\n", __func__))

SOFIA_BEGIN_DECLS

#ifdef DOXYGEN
extern char const STUN_DEBUG[]; /* dummy declaration for Doxygen */
#endif

/* XXX -- mela: note that this are 100 times too small */
#if 1
#define STUN_LIFETIME_EST 3500      /**< 6 min? */
#define STUN_LIFETIME_MAX 18000     /**< 30 min? */
#define STUN_LIFETIME_CI  50        /**< 5 sec confidence interval */
#else
#define STUN_LIFETIME_EST 350      /**< 6 min? */
#define STUN_LIFETIME_MAX 1800     /**< 30 min? */
#define STUN_LIFETIME_CI  5        /**< 5 sec confidence interval */
#endif

#define STUN_ERROR(err, what) \
  SU_DEBUG_5(("%s: %s: %s\n", __func__, #what, su_strerror(err))), \
    -1								   \

int stun_is_requested(tag_type_t tag, tag_value_t value, ...);

/* internal functions declaration */
int stun_make_sharedsecret_req(stun_msg_t *msg);

int stun_send_message(su_socket_t s, su_sockaddr_t *srvr,
		      stun_msg_t *msg, stun_buffer_t *pwd);

int stun_make_binding_req(stun_handle_t *se, stun_request_t *req,
			  stun_msg_t *msg,
			  tag_type_t, tag_value_t, ...);
int stun_process_response(stun_msg_t *msg);

int stun_process_binding_response(stun_msg_t *msg);
int stun_process_error_response(stun_msg_t *msg);

int stun_atoaddr(su_home_t *home, int ai_family, su_addrinfo_t *info, char const *in);
int stun_add_response_address(stun_msg_t *req, struct sockaddr_in *mapped_addr);

SOFIA_END_DECLS

#endif /* !defined(STUN_INTERNAL_H) */
