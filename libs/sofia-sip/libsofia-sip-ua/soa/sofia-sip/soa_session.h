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

#ifndef SOA_SESSION_H
#define SOA_SESSION_H
/**@file sofia-sip/soa_session.h
 *
 * Internal API for SDP Offer/Answer Interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Aug 1 15:43:53 EEST 2005 ppessi
 */

#ifndef SOA_H
#include "sofia-sip/soa.h"
#endif
#ifndef SOA_TAG_H
#include "sofia-sip/soa_tag.h"
#endif
#ifndef SDP_H
#include <sofia-sip/sdp.h>
#endif
#ifndef SU_STRLST_H
#include <sofia-sip/su_strlst.h>
#endif

SOFIA_BEGIN_DECLS

struct soa_session_actions
{
  int sizeof_soa_session_actions;
  int sizeof_soa_session;
  char const *soa_name;
  int (*soa_init)(char const *name, soa_session_t *ss, soa_session_t *parent);
  void (*soa_deinit)(soa_session_t *ss);
  int (*soa_set_params)(soa_session_t *ss, tagi_t const *tags);
  int (*soa_get_params)(soa_session_t const *ss, tagi_t *tags);
  tagi_t *(*soa_get_paramlist)(soa_session_t const *ss,
			       tag_type_t, tag_value_t, ...);
  char **(*soa_media_features)(soa_session_t *, int live, su_home_t *);
  char const * const *(*soa_sip_require)(soa_session_t const *ss);
  char const * const *(*soa_sip_supported)(soa_session_t const *ss);
  int (*soa_remote_sip_features)(soa_session_t *ss,
				 char const * const * support,
				 char const * const * required);
  int (*soa_set_capability_sdp)(soa_session_t *, sdp_session_t *,
				char const *, isize_t);
  int (*soa_set_remote_sdp)(soa_session_t *, int new_version,
			    sdp_session_t *, char const *, isize_t);
  int (*soa_set_user_sdp)(soa_session_t *, sdp_session_t *,
			  char const *, isize_t);
  int (*soa_generate_offer)(soa_session_t *ss, soa_callback_f *completed);
  int (*soa_generate_answer)(soa_session_t *ss, soa_callback_f *completed);
  int (*soa_process_answer)(soa_session_t *ss, soa_callback_f *completed);
  int (*soa_process_reject)(soa_session_t *ss, soa_callback_f *completed);
  int (*soa_activate_session)(soa_session_t *ss, char const *option);
  int (*soa_deactivate_session)(soa_session_t *ss, char const *option);
  void (*soa_terminate_session)(soa_session_t *ss, char const *option);
};

SOFIAPUBFUN soa_session_t *soa_session_ref(soa_session_t *ss);
SOFIAPUBFUN void soa_session_unref(soa_session_t *ss);

SOFIAPUBFUN int soa_base_init(char const *name, soa_session_t *,
			      soa_session_t *parent);
SOFIAPUBFUN void soa_base_deinit(soa_session_t *ss);
SOFIAPUBFUN int soa_base_set_params(soa_session_t *ss, tagi_t const *tags);
SOFIAPUBFUN int soa_base_get_params(soa_session_t const *ss, tagi_t *tags);
SOFIAPUBFUN tagi_t *soa_base_get_paramlist(soa_session_t const *ss,
					   tag_type_t, tag_value_t, ...);
SOFIAPUBFUN char **soa_base_media_features(soa_session_t *,
					   int live, su_home_t *);
SOFIAPUBFUN char const * const *soa_base_sip_require(soa_session_t const *ss);
SOFIAPUBFUN char const * const *soa_base_sip_supported(soa_session_t const *ss);

SOFIAPUBFUN int soa_base_remote_sip_features(soa_session_t *ss,
					     char const * const *support,
					     char const * const *required);
SOFIAPUBFUN int soa_base_set_capability_sdp(soa_session_t *ss,
					    sdp_session_t *sdp,
					    char const *, isize_t);
SOFIAPUBFUN int soa_base_set_remote_sdp(soa_session_t *ss,
					int new_version,
					sdp_session_t *sdp, char const *, isize_t);
SOFIAPUBFUN int soa_base_set_user_sdp(soa_session_t *ss,
				      sdp_session_t *sdp, char const *, isize_t);

SOFIAPUBFUN int soa_base_generate_offer(soa_session_t *ss,
					soa_callback_f *completed);
SOFIAPUBFUN int soa_base_generate_answer(soa_session_t *ss,
					 soa_callback_f *completed);
SOFIAPUBFUN int soa_base_process_answer(soa_session_t *ss,
					soa_callback_f *completed);
SOFIAPUBFUN int soa_base_process_reject(soa_session_t *ss,
					soa_callback_f *completed);

SOFIAPUBFUN int soa_base_activate(soa_session_t *ss, char const *option);
SOFIAPUBFUN int soa_base_deactivate(soa_session_t *ss, char const *option);
SOFIAPUBFUN void soa_base_terminate(soa_session_t *ss, char const *option);

struct soa_description
{
  sdp_session_t  *ssd_sdp;	/**< Session description  */
  char const     *ssd_unparsed;	/**< Original session description as string */
  char const     *ssd_str;	/**< Session description as string */
  sdp_printer_t  *ssd_printer;	/**< SDP printer object */
};

struct soa_session
{
  su_home_t ss_home[1];

  struct soa_session_actions const *ss_actions;
  char const *ss_name;		/**< Our name */

  su_root_t *ss_root;
  soa_magic_t *ss_magic;	/**< Application data */

  soa_callback_f *ss_in_progress;/**< Operation in progress */

  /** Incremented once each time session is terminated */
  unsigned  ss_terminated;

  /* XXX - this is part of public API. we should have no bitfields here */

  unsigned  ss_active:1;	/**< Session has been activated */

  /* Current Offer-Answer status */
  unsigned  ss_complete:1;	/**< Completed SDP offer-answer */

  unsigned  ss_unprocessed_remote:1; /**< We have received remote SDP */

  unsigned  ss_offer_sent:2;	/**< We have offered SDP */
  unsigned  ss_answer_recv:2;	/**< We have received SDP answer */

  unsigned  ss_offer_recv:2;	/**< We have received an offer */
  unsigned  ss_answer_sent:2;	/**< We have answered (reliably, if >1) */
  unsigned  :0;			/* Pad */

  unsigned  ss_oa_rounds;	/**< Number of O/A rounds completed */

  struct soa_media_activity
  {
    unsigned ma_audio:4; /**< Audio activity (send/recv) */
    unsigned ma_video:4; /**< Video activity (send/recv) */
    unsigned ma_image:4; /**< Image activity (send/recv) for JPIP */
    unsigned ma_chat:4;  /**< Chat activity (send/recv) */
  } ss_local_activity[1], ss_remote_activity[1];

  /** Capabilities as specified by application */
  struct soa_description ss_caps[1];

  /** Session description provided by user */
  struct soa_description ss_user[1];
  unsigned ss_user_version;  /**< Version incremented at each change */

  /** Remote session description */
  struct soa_description ss_remote[1];
  unsigned ss_remote_version;  /**< Version incremented at each change */

  /** Local session description */
  struct soa_description ss_local[1];
  unsigned ss_local_user_version, ss_local_remote_version;
  char const *ss_hold_local;

  /** Previous session description (return to this if offer is rejected) */
  struct soa_description ss_previous[1];
  unsigned ss_previous_user_version, ss_previous_remote_version;
  char const *ss_hold_previous;

  sdp_session_t *ss_rsession;	/**< Processed remote SDP */

  /** SIP features required */
  char const * const *ss_local_required;
  /** SIP features supported */
  char const * const *ss_local_support;

  /** SIP features required by remote */
  char const **ss_remote_required;
  /** SIP features supported */
  char const **ss_remote_support;

  int             ss_status;	/**< Status from last media operation */
  char const     *ss_phrase;	/**< Phrase from last media operation */
  char           *ss_reason;	/**< Reason generated by media operation */


  /* Media parameters */
  char const     *ss_address;
  enum soa_af     ss_af;
  char const     *ss_hold;	/**< Media on hold locally */

  char const     *ss_cname;

  /* XXX - this is part of public API. we should have no bitfields here */

  /* Codec handling during negotiation */
  unsigned  ss_rtp_select:2;
  unsigned  ss_rtp_sort:2;
  unsigned  ss_rtp_mismatch:1;

  unsigned ss_srtp_enable:1,
    ss_srtp_confidentiality:1,
    ss_srtp_integrity:1;

  unsigned :0;			/* Pad */

  int             ss_wcode;	/**< Warning code from last media operation */
  char const     *ss_warning;	/**< Warnings text from last media operation */
};

/* ====================================================================== */

SOFIAPUBFUN int soa_has_received_sdp(soa_session_t const *ss);

SOFIAPUBFUN int soa_set_status(soa_session_t *ss,
			       int status, char const *phrase);

enum soa_activity {
  soa_activity_local,
  soa_activity_remote,
  soa_activity_session
};

SOFIAPUBFUN void soa_set_activity(soa_session_t *ss,
				  sdp_media_t const *m,
				  enum soa_activity activity);

SOFIAPUBFUN int soa_description_set(soa_session_t *ss,
				    struct soa_description *ssd,
				    sdp_session_t *sdp,
				    char const *sdp_str,
				    isize_t sdp_len);

SOFIAPUBFUN void soa_description_free(soa_session_t *,
				      struct soa_description *ssd);

SOFIAPUBFUN int soa_description_dup(su_home_t *,
				    struct soa_description *ssd,
				    struct soa_description const *ssd0);

SOFIAPUBFUN int soa_init_sdp_origin(soa_session_t *ss,
				    sdp_origin_t *o, char buf[64]);
SOFIAPUBFUN int soa_init_sdp_origin_with_session(soa_session_t *ss,
						 sdp_origin_t *o,
						 char buffer[64],
						 sdp_session_t const *sdp);
SOFIAPUBFUN int soa_check_sdp_connection(sdp_connection_t const *c);
SOFIAPUBFUN int soa_init_sdp_connection(soa_session_t *,
					sdp_connection_t *, char buf[64]);
SOFIAPUBFUN int soa_init_sdp_connection_with_session(soa_session_t *,
						     sdp_connection_t *, char buf[64],
						     sdp_session_t const *sdp);

SOFIAPUBFUN sdp_connection_t *soa_find_local_sdp_connection(sdp_session_t const*);

/* ====================================================================== */
/* Debug log settings */

#define SU_LOG   soa_log

#ifdef SU_DEBUG_H
#error <su_debug.h> included directly.
#endif
#include <sofia-sip/su_debug.h>
SOFIAPUBVAR su_log_t soa_log[];

SOFIA_END_DECLS

#endif
