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

#ifndef SOA_H
/** Defined when <sofia-sip/soa.h> has been included. */
#define SOA_H
/**@file sofia-sip/soa.h  SDP Offer/Answer (RFC 3264) Interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Fri Jul 15 15:43:53 EEST 2005 ppessi
 */

#ifndef SU_WAIT_H
#include <sofia-sip/su_wait.h>
#endif
#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct soa_session soa_session_t;

struct sdp_session_s;

#ifndef SOA_MAGIC_T
#define SOA_MAGIC_T void
#endif

typedef SOA_MAGIC_T soa_magic_t;

typedef int soa_callback_f(soa_magic_t *arg, soa_session_t *session);

SOFIAPUBFUN soa_session_t *soa_create(char const *name, su_root_t *, soa_magic_t *);

SOFIAPUBFUN soa_session_t *soa_clone(soa_session_t *, su_root_t *, soa_magic_t *);

SOFIAPUBFUN void soa_destroy(soa_session_t *);

SOFIAPUBFUN int soa_set_params(soa_session_t *ss,
			       tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN int soa_get_params(soa_session_t const *ss,
			       tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN tagi_t *soa_get_paramlist(soa_session_t const *ss,
				      tag_type_t tag, tag_value_t value, ...);

SOFIAPUBFUN int soa_error_as_sip_response(soa_session_t *soa,
					  char const **return_phrase);

SOFIAPUBFUN char const *soa_error_as_sip_reason(soa_session_t *soa);

SOFIAPUBFUN int soa_get_warning(soa_session_t *ss, char const **return_phrase);

SOFIAPUBFUN int soa_set_capability_sdp(soa_session_t *ss,
				       struct sdp_session_s const *sdp,
				       char const *str, issize_t len);

SOFIAPUBFUN int soa_get_capability_sdp(soa_session_t const *ss,
				       struct sdp_session_s const **return_sdp,
				       char const **return_sdp_str,
				       isize_t *return_len);

SOFIAPUBFUN int soa_set_remote_sdp(soa_session_t *ss,
				   struct sdp_session_s const *sdp,
				   char const *str, issize_t len);

SOFIAPUBFUN int soa_get_remote_sdp(soa_session_t const *ss,
				   struct sdp_session_s const **return_sdp,
				   char const **return_sdp_str,
				   isize_t *return_len);

SOFIAPUBFUN int soa_clear_remote_sdp(soa_session_t *ss);

SOFIAPUBFUN int soa_get_remote_version(soa_session_t const *ss);

SOFIAPUBFUN int soa_set_user_sdp(soa_session_t *ss,
				 struct sdp_session_s const *sdp,
				 char const *str, issize_t len);

SOFIAPUBFUN int soa_get_user_sdp(soa_session_t const *ss,
				 struct sdp_session_s const **return_sdp,
				 char const **return_sdp_str,
				 isize_t *return_len);

SOFIAPUBFUN int soa_get_user_version(soa_session_t const *ss);

SOFIAPUBFUN int soa_get_local_sdp(soa_session_t const *ss,
				  struct sdp_session_s const **return_sdp,
				  char const **return_sdp_str,
				  isize_t *return_len);

SOFIAPUBFUN char const * const * soa_sip_require(soa_session_t const *ss);
SOFIAPUBFUN char const * const * soa_sip_supported(soa_session_t const *ss);

SOFIAPUBFUN int soa_remote_sip_features(soa_session_t *ss,
					char const * const * support,
					char const * const * required);

SOFIAPUBFUN char **soa_media_features(soa_session_t *ss, int live, su_home_t *home);

SOFIAPUBFUN int soa_generate_offer(soa_session_t *, int always, soa_callback_f *);
SOFIAPUBFUN int soa_generate_answer(soa_session_t *, soa_callback_f *);
SOFIAPUBFUN int soa_process_answer(soa_session_t *, soa_callback_f *);
SOFIAPUBFUN int soa_process_reject(soa_session_t *, soa_callback_f *);

SOFIAPUBFUN int soa_activate(soa_session_t *, char const *option);
SOFIAPUBFUN int soa_deactivate(soa_session_t *, char const *option);

SOFIAPUBFUN void soa_terminate(soa_session_t *, char const *option);

SOFIAPUBFUN int soa_is_complete(soa_session_t const *ss);

SOFIAPUBFUN int soa_init_offer_answer(soa_session_t *ss);

SOFIAPUBFUN int soa_is_audio_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_video_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_image_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_chat_active(soa_session_t const *ss);

SOFIAPUBFUN int soa_is_remote_audio_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_remote_video_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_remote_image_active(soa_session_t const *ss);
SOFIAPUBFUN int soa_is_remote_chat_active(soa_session_t const *ss);

SOFIAPUBFUN int soa_tag_filter(tagi_t const *f, tagi_t const *t);

SOFIA_END_DECLS

#endif
