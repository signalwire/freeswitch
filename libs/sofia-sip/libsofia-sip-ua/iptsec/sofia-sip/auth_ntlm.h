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

#ifndef AUTH_NTLM_H
/** Defined when <sofia-sip/auth_ntlm.h> has been included. */
#define AUTH_NTLM_H

/**@file sofia-sip/auth_ntlm.h
 * Datatypes and functions for Ntlm authentication.
 *
 * The structures and functions here follow the RFC 2617.
 *
 * @sa
 * <a href="ftp://ftp.ietf.org/rfc/rfc2617.txt">RFC 2617</a>,
 * <i>"HTTP Authentication: Basic and Ntlm Access Authentication"</i>,
 * J. Franks et al,
 * June 1999.
 *
 * @sa Section 19 from
 * <a href="ftp://ftp.ietf.org/internet-drafts/draft-ietf-sip-rfc2543bis-04.txt>draft-ietf-sip-rfc2543bis-04</a>.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Feb 22 12:25:55 2001 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#ifndef AUTH_PLUGIN_H
#include <sofia-sip/auth_plugin.h>
#endif

SOFIA_BEGIN_DECLS

issize_t auth_ntlm_challenge_get(su_home_t *, auth_challenge_t *,
				 char const * const params[]);
issize_t auth_ntlm_response_get(su_home_t *, auth_response_t *,
				char const * const params[]);

int auth_ntlm_a1(auth_response_t *ar,
		   auth_hexmd5_t ha1,
		   char const *secret);

int auth_ntlm_a1sess(auth_response_t *ar,
		       auth_hexmd5_t ha1sess,
		       char const *ha1);

int auth_ntlm_sessionkey(auth_response_t *, auth_hexmd5_t ha1,
			   char const *secret);
int auth_ntlm_response(auth_response_t *, auth_hexmd5_t response,
		       auth_hexmd5_t const ha1,
		       char const *method_name, void const *data, issize_t dlen);

/** NTLM scheme */
msg_auth_t *auth_ntlm_credentials(msg_auth_t *auth,
				  char const *realm,
				  char const *opaque,
				  char const *gssapidata,
				  char const *targetname);

void auth_challenge_ntlm(auth_mod_t *am,
			 auth_status_t *as,
			 auth_challenger_t const *ach);


void auth_method_ntlm(auth_mod_t *am,
		      auth_status_t *as,
		      msg_auth_t *au,
		      auth_challenger_t const *ach);


void auth_check_ntlm(auth_mod_t *am,
		     auth_status_t *as,
		     auth_response_t *ar,
		     auth_challenger_t const *ach);

int auth_generate_ntlm_nonce(auth_mod_t *am,
			     char buffer[],
			     size_t buffer_len,
			     int nextnonce,
			     msg_time_t now);

int auth_validate_ntlm_nonce(auth_mod_t *am,
			     auth_status_t *as,
			     auth_response_t *ar,
			     msg_time_t now);

void auth_info_ntlm(auth_mod_t *am,
		    auth_status_t *as,
		    auth_challenger_t const *ach);

SOFIA_END_DECLS

#endif
