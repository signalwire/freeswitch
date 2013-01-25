/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef NUA_TAG_H
/** Defined when <sofia-sip/nua_tag.h> has been included. */
#define NUA_TAG_H

/**@file sofia-sip/nua_tag.h
 * @brief Tags for Sofia-SIP User Agent Library
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Mon Feb 19 18:54:26 EET 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SDP_TAG_H
#include <sofia-sip/sdp_tag.h>
#endif
#ifndef URL_TAG_H
#include <sofia-sip/url_tag.h>
#endif
#ifndef SIP_TAG_H
#include <sofia-sip/sip_tag.h>
#endif
#ifndef NTA_TAG_H
#include <sofia-sip/nta_tag.h>
#endif
#ifndef NEA_TAG_H
#include <sofia-sip/nea_tag.h>
#endif
#ifndef SOA_TAG_H
#include <sofia-sip/soa_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** NUA agent. */
typedef struct nua_s nua_t;

/** NUA transaction handle. */
typedef struct nua_handle_s nua_handle_t;

/** List of all NUA tags. */
SOFIAPUBVAR tag_type_t nua_tag_list[];

/** Filter tag matching any nua tag. */
#define NUTAG_ANY()          nutag_any, ((tag_value_t)0)
SOFIAPUBVAR tag_typedef_t nutag_any;

#define NUTAG_URL(x)            nutag_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_url;
#define NUTAG_URL_REF(x)        nutag_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_url_ref;

#define NUTAG_ADDRESS(x)        nutag_address, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_address;
#define NUTAG_ADDRESS_REF(x)    nutag_address_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_address_ref;

#define NUTAG_WITH(x)         nutag_with, tag_ptr_v(x)
SOFIAPUBVAR tag_typedef_t nutag_with;

#define NUTAG_WITH_THIS(nua) nutag_with, tag_ptr_v(nua_current_request((nua)))
#define NUTAG_WITH_CURRENT(nua) \
   nutag_with, tag_ptr_v(nua_current_request((nua)))
#define NUTAG_WITH_SAVED(e) nutag_with, tag_ptr_v(nua_saved_event_request((e)))

#define NUTAG_DIALOG(b) nutag_dialog, tag_uint_v((b))
SOFIAPUBVAR tag_typedef_t nutag_dialog;

#define NUTAG_METHOD(x)            nutag_method, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_method;
#define NUTAG_METHOD_REF(x)        nutag_method_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_method_ref;

#define NUTAG_MAX_SUBSCRIPTIONS(x)      nutag_max_subscriptions, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_max_subscriptions;
#define NUTAG_MAX_SUBSCRIPTIONS_REF(x) \
nutag_max_subscriptions_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_max_subscriptions_ref;

#define NUTAG_UICC(x)  nutag_uicc, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_uicc;
#define NUTAG_UICC_REF(x) nutag_uicc_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_uicc_ref;

#define NUTAG_USE_DIALOG(x)        nutag_use_dialog, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_use_dialog;
#define NUTAG_USE_DIALOG_REF(x)    nutag_use_dialog_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_use_dialog_ref;


/* Protocol engine parameters,
 * set by nua_set_params(), get by nua_get_params() */

#define NUTAG_RETRY_COUNT(x)      nutag_retry_count, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_retry_count;
#define NUTAG_RETRY_COUNT_REF(x)  nutag_retry_count_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_retry_count_ref;

#define NUTAG_SOA_NAME(x)  nutag_soa_name, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_soa_name;
#define NUTAG_SOA_NAME_REF(x) \
 nutag_soa_name_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_soa_name_ref;

#define NUTAG_EARLY_MEDIA(x)    nutag_early_media, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_early_media;
#define NUTAG_EARLY_MEDIA_REF(x) nutag_early_media_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_early_media_ref;

#define NUTAG_ONLY183_100REL(x)    nutag_only183_100rel, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_only183_100rel;
#define NUTAG_ONLY183_100REL_REF(x) nutag_only183_100rel_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_only183_100rel_ref;

#define NUTAG_EARLY_ANSWER(x)    nutag_early_answer, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_early_answer;
#define NUTAG_EARLY_ANSWER_REF(x) nutag_early_answer_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_early_answer_ref;

#define NUTAG_INCLUDE_EXTRA_SDP(x)    nutag_include_extra_sdp, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_include_extra_sdp;
#define NUTAG_INCLUDE_EXTRA_SDP_REF(x) \
   nutag_include_extra_sdp_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_include_extra_sdp_ref;

#define NUTAG_INVITE_TIMER(x)  nutag_invite_timer, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_invite_timer;
#define NUTAG_INVITE_TIMER_REF(x) nutag_invite_timer_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_invite_timer_ref;

#define NUTAG_SESSION_TIMER(x)  nutag_session_timer, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_session_timer;
#define NUTAG_SESSION_TIMER_REF(x) nutag_session_timer_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_session_timer_ref;

#define NUTAG_MIN_SE(x)         nutag_min_se, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_min_se;
#define NUTAG_MIN_SE_REF(x)     nutag_min_se_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_min_se_ref;

/** Enumeration type of NUTAG_SESSION_REFRESHER(). */
enum nua_session_refresher {
  nua_no_refresher,		/**< Disable session timer. */
  nua_local_refresher,		/**< Session refresh by local end. */
  nua_remote_refresher,		/**< Session refresh by remote end. */
  nua_any_refresher		/**< No preference (default). */
};

#define NUTAG_SESSION_REFRESHER(x)  nutag_session_refresher, tag_int_v((x))
SOFIAPUBVAR tag_typedef_t nutag_session_refresher;
#define NUTAG_SESSION_REFRESHER_REF(x) nutag_session_refresher_ref, tag_int_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_session_refresher_ref;

#define NUTAG_UPDATE_REFRESH(x)  nutag_update_refresh, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t nutag_update_refresh;
#define NUTAG_UPDATE_REFRESH_REF(x) nutag_update_refresh_ref, tag_bool_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_update_refresh_ref;

#define NUTAG_REFRESH_WITHOUT_SDP(x) nutag_refresh_without_sdp, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t nutag_refresh_without_sdp;
#define NUTAG_REFRESH_WITHOUT_SDP_REF(x) \
  nutag_refresh_without_sdp_ref, tag_bool_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_refresh_without_sdp_ref;

#define NUTAG_AUTOALERT(x)      nutag_autoalert, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoalert;
#define NUTAG_AUTOALERT_REF(x)  nutag_autoalert_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoalert_ref;

#define NUTAG_AUTOACK(x)        nutag_autoack, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoack;
#define NUTAG_AUTOACK_REF(x)    nutag_autoack_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoack_ref;

#define NUTAG_TIMER_AUTOREQUIRE(x)        nutag_timer_autorequire, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_timer_autorequire;
#define NUTAG_TIMER_AUTOREQUIRE_REF(x)    nutag_timer_autorequire_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_timer_autorequire_ref;

#define NUTAG_AUTOANSWER(x)     nutag_autoanswer, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoanswer;
#define NUTAG_AUTOANSWER_REF(x) nutag_autoanswer_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoanswer_ref;

#define NUTAG_ENABLEINVITE(x)   nutag_enableinvite, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enableinvite;
#define NUTAG_ENABLEINVITE_REF(x) nutag_enableinvite_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enableinvite_ref;

#define NUTAG_ENABLEMESSAGE(x)  nutag_enablemessage, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enablemessage;
#define NUTAG_ENABLEMESSAGE_REF(x) nutag_enablemessage_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enablemessage_ref;

#define NUTAG_ENABLEMESSENGER(x)  nutag_enablemessenger, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enablemessenger;
#define NUTAG_ENABLEMESSENGER_REF(x) \
  nutag_enablemessenger_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enablemessenger_ref;

/* Start NRC Boston */

#define NUTAG_SMIME_ENABLE(x)  nutag_smime_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_enable;
#define NUTAG_SMIME_ENABLE_REF(x) nutag_smime_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_enable_ref;

#define NUTAG_SMIME_OPT(x)  nutag_smime_opt, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_opt;
#define NUTAG_SMIME_OPT_REF(x) nutag_smime_opt_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_opt_ref;

#define NUTAG_SMIME_PROTECTION_MODE(x) nutag_smime_protection_mode, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_protection_mode;
#define NUTAG_SMIME_PROTECTION_MODE_REF(x) \
           nutag_smime_protection_mode_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_protection_mode_ref;

#define NUTAG_SMIME_MESSAGE_DIGEST(x) nutag_smime_message_digest, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_message_digest;
#define NUTAG_SMIME_MESSAGE_DIGEST_REF(x) \
            nutag_smime_message_digest_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_message_digest_ref;

#define NUTAG_SMIME_SIGNATURE(x) nutag_smime_signature, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_signature;
#define NUTAG_SMIME_SIGNATURE_REF(x) \
            nutag_smime_signature_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_signature_ref;

#define NUTAG_SMIME_KEY_ENCRYPTION(x) nutag_smime_key_encryption, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_key_encryption;
#define NUTAG_SMIME_KEY_ENCRYPTION_REF(x) \
          nutag_smime_key_encryption_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_key_encryption_ref;

#define NUTAG_SMIME_MESSAGE_ENCRYPTION(x) nutag_smime_message_encryption, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_message_encryption;
#define NUTAG_SMIME_MESSAGE_ENCRYPTION_REF(x) \
           nutag_smime_message_encryption_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_message_encryption_ref;

/* End NRC Boston */

#define NUTAG_CERTIFICATE_DIR(x) nutag_certificate_dir, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_certificate_dir;
#define NUTAG_CERTIFICATE_DIR_REF(x) \
          nutag_certificate_dir_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_certificate_dir_ref;

#define NUTAG_CERTIFICATE_PHRASE(x) nutag_certificate_phrase, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_certificate_phrase;
#define NUTAG_CERTIFICATE_PHRASE_REF(x) \
          nutag_certificate_phrase_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_certificate_phrase_ref;

#define NUTAG_SIPS_URL(x)       nutag_sips_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_sips_url;
#define NUTAG_SIPS_URL_REF(x)   nutag_sips_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_sips_url_ref;

#define NUTAG_WS_URL(x)       nutag_ws_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_ws_url;
#define NUTAG_WS_URL_REF(x)   nutag_ws_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_ws_url_ref;

#define NUTAG_WSS_URL(x)       nutag_wss_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_wss_url;
#define NUTAG_WSS_URL_REF(x)   nutag_wss_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_wss_url_ref;

#define NUTAG_PROXY(x)          NTATAG_DEFAULT_PROXY(x)
#define NUTAG_PROXY_REF(x)      NTATAG_DEFAULT_PROXY_REF(x)
#define nutag_proxy             ntatag_default_proxy

#define NUTAG_INITIAL_ROUTE(x)     nutag_initial_route, siptag_route_v(x)
SOFIAPUBVAR tag_typedef_t nutag_initial_route;
#define NUTAG_INITIAL_ROUTE_REF(x) nutag_initial_route_ref, siptag_route_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_initial_route_ref;

#define NUTAG_INITIAL_ROUTE_STR(x)     nutag_initial_route_str, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_initial_route_str;
#define NUTAG_INITIAL_ROUTE_STR_REF(x) nutag_initial_route_str_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_initial_route_str_ref;

#define NUTAG_REGISTRAR(x)      nutag_registrar, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_registrar;
#define NUTAG_REGISTRAR_REF(x)  nutag_registrar_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_registrar_ref;

#define NUTAG_OUTBOUND(x)      nutag_outbound, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound;
#define NUTAG_OUTBOUND_REF(x)  nutag_outbound_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_ref;

#if notyet

#define NUTAG_OUTBOUND_SET1(x)      nutag_outbound_set1, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set1;
#define NUTAG_OUTBOUND_SET1_REF(x)  nutag_outbound_set1_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set1_ref;

#define NUTAG_OUTBOUND_SET2(x)      nutag_outbound_set2, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set2;
#define NUTAG_OUTBOUND_SET2_REF(x)  nutag_outbound_set2_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set2_ref;

#define NUTAG_OUTBOUND_SET3(x)      nutag_outbound_set3, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set3;
#define NUTAG_OUTBOUND_SET3_REF(x)  nutag_outbound_set3_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set3_ref;

#define NUTAG_OUTBOUND_SET4(x)      nutag_outbound_set4, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set4;
#define NUTAG_OUTBOUND_SET4_REF(x)  nutag_outbound_set4_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set4_ref;

#endif	/* ...notyet */

#define NUTAG_SIP_PARSER(x)     NTATAG_MCLASS(x)
#define NUTAG_SIP_PARSER_REF(x) NTATAG_MCLASS_REF(x)

#define NUTAG_AUTH(x)		nutag_auth, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_auth;
#define NUTAG_AUTH_REF(x)	    nutag_auth_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_auth_ref;

#define NUTAG_AUTH_CACHE(x)   nutag_auth_cache, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_auth_cache;
#define NUTAG_AUTH_CACHE_REF(x) nutag_auth_cache_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_auth_cache_ref;

/** Authentication caching policy. @NEW_1_12_6. */
enum nua_auth_cache {
  /** Include credentials within dialog (default) */
  nua_auth_cache_dialog = 0,
  /** Include credentials only when challenged */
  nua_auth_cache_challenged = 1,
  _nua_auth_cache_invalid
};

#define NUTAG_KEEPALIVE(x) nutag_keepalive, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_keepalive;
#define NUTAG_KEEPALIVE_REF(x) nutag_keepalive_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_keepalive_ref;

#define NUTAG_KEEPALIVE_STREAM(x) nutag_keepalive_stream, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_keepalive_stream;
#define NUTAG_KEEPALIVE_STREAM_REF(x) \
nutag_keepalive_stream_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_keepalive_stream_ref;

#define NUTAG_AUTHTIME(x)	nutag_authtime, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_authtime;
#define NUTAG_AUTHTIME_REF(x)	nutag_authtime_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_authtime_ref;

#define NUTAG_M_DISPLAY(x)   nutag_m_display, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_display;
#define NUTAG_M_DISPLAY_REF(x) nutag_m_display_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_display_ref;

#define NUTAG_M_USERNAME(x)   nutag_m_username, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_username;
#define NUTAG_M_USERNAME_REF(x) nutag_m_username_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_username_ref;

#define NUTAG_M_PARAMS(x)   nutag_m_params, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_params;
#define NUTAG_M_PARAMS_REF(x) nutag_m_params_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_params_ref;

#define NUTAG_M_FEATURES(x)   nutag_m_features, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_features;
#define NUTAG_M_FEATURES_REF(x) nutag_m_features_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_features_ref;

#define NUTAG_EVENT(x)          nutag_event, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_event;
#define NUTAG_EVENT_REF(x)      nutag_event_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_event_ref;

#define NUTAG_STATUS(x)         nutag_status, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_status;
#define NUTAG_STATUS_REF(x)     nutag_status_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_status_ref;

#define NUTAG_PHRASE(x)         nutag_phrase, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_phrase;
#define NUTAG_PHRASE_REF(x)     nutag_phrase_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_phrase_ref;

#define NUTAG_HANDLE(x)         nutag_handle, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_handle;
#define NUTAG_HANDLE_REF(x)     nutag_handle_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_handle_ref;

#define NUTAG_IDENTITY(x)   nutag_identity, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_identity;
#define NUTAG_IDENTITY_REF(x) nutag_identity_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_identity_ref;

#define NUTAG_INSTANCE(x)        nutag_instance, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_instance;
#define NUTAG_INSTANCE_REF(x)    nutag_instance_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_instance_ref;

#define NUTAG_NOTIFY_REFER(x)   nutag_notify_refer, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_notify_refer;
#define NUTAG_NOTIFY_REFER_REF(x) nutag_notify_refer_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_notify_refer_ref;

#define NUTAG_REFER_EVENT(x)   nutag_refer_event, siptag_event_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_event;
#define NUTAG_REFER_EVENT_REF(x) nutag_refer_event_ref, siptag_event_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_event_ref;

#define NUTAG_REFER_PAUSE(x)   nutag_refer_pause, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_pause;
#define NUTAG_REFER_PAUSE_REF(x) nutag_refer_pause_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_pause_ref;

#define NUTAG_USER_AGENT(x)     nutag_user_agent, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_user_agent;
#define NUTAG_USER_AGENT_REF(x) nutag_user_agent_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_user_agent_ref;

#define NUTAG_VIA(x)     nutag_via, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_via;
#define NUTAG_VIA_REF(x) nutag_via_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_via_ref;

#define NUTAG_ALLOW(x)     nutag_allow, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_allow;
#define NUTAG_ALLOW_REF(x) nutag_allow_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_allow_ref;


#define NUTAG_APPL_METHOD(x)     nutag_appl_method, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_appl_method;
#define NUTAG_APPL_METHOD_REF(x) nutag_appl_method_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_appl_method_ref;


#define NUTAG_SUPPORTED(x)     nutag_supported, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_supported;
#define NUTAG_SUPPORTED_REF(x) nutag_supported_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_supported_ref;

#define NUTAG_ALLOW_EVENTS(x)     nutag_allow_events, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_allow_events;
#define NUTAG_ALLOW_EVENTS_REF(x) nutag_allow_events_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_allow_events_ref;

#define NUTAG_CALLSTATE(x) nutag_callstate, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_callstate;
#define NUTAG_CALLSTATE_REF(x) nutag_callstate_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_callstate_ref;

enum nua_callstate {
  nua_callstate_init,		/**< Initial state */
  nua_callstate_authenticating, /**< 401/407 received */
  nua_callstate_calling,	/**< INVITE sent */
  nua_callstate_proceeding,	/**< 18X received */
  nua_callstate_completing,	/**< 2XX received */
  nua_callstate_received,	/**< INVITE received */
  nua_callstate_early,		/**< 18X sent (w/SDP) */
  nua_callstate_completed,	/**< 2XX sent */
  nua_callstate_ready,		/**< 2XX received, ACK sent, or vice versa */
  nua_callstate_terminating,	/**< BYE sent */
  nua_callstate_terminated	/**< BYE complete */
};

/** Get name for NUA call state */
SOFIAPUBFUN char const *nua_callstate_name(enum nua_callstate state);

#define NUTAG_SUBSTATE(x) nutag_substate, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_substate;
#define NUTAG_SUBSTATE_REF(x) nutag_substate_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_substate_ref;

/** Parameter type of NUTAG_SUBSTATE() */
enum nua_substate {
  /** Extended state, considered as active. */
  nua_substate_extended = nea_extended,
  /** Embryonic subscription: SUBSCRIBE sent */
  nua_substate_embryonic = nea_embryonic,
  nua_substate_pending = nea_pending,   /**< Pending subscription */
  nua_substate_active = nea_active,	/**< Active subscription */
  nua_substate_terminated = nea_terminated /**< Terminated subscription */
};

/** Return name of subscription state. @NEW_1_12_5. */
SOFIAPUBFUN char const *nua_substate_name(enum nua_substate substate);

/** Convert string to enum nua_substate. @NEW_1_12_5. */
SOFIAPUBFUN enum nua_substate nua_substate_make(char const *sip_substate);

#define NUTAG_SUB_EXPIRES(x) nutag_sub_expires, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_sub_expires;
#define NUTAG_SUB_EXPIRES_REF(x) nutag_sub_expires_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_sub_expires_ref;

#define NUTAG_NEWSUB(x)   nutag_newsub, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_newsub;
#define NUTAG_NEWSUB_REF(x) nutag_newsub_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_newsub_ref;

#define NUTAG_REFER_EXPIRES(x)  nutag_refer_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_refer_expires;
#define NUTAG_REFER_EXPIRES_REF(x) nutag_refer_expires_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_refer_expires_ref;

#define NUTAG_REFER_WITH_ID(x)   nutag_refer_with_id, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_with_id;
#define NUTAG_REFER_WITH_ID_REF(x) nutag_refer_with_id_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_with_id_ref;

#define NUTAG_MEDIA_FEATURES(x) nutag_media_features, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_media_features;
#define NUTAG_MEDIA_FEATURES_REF(x) \
          nutag_media_features_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_media_features_ref;

#define NUTAG_CALLEE_CAPS(x) nutag_callee_caps, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_callee_caps;
#define NUTAG_CALLEE_CAPS_REF(x) \
          nutag_callee_caps_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_callee_caps_ref;

#define NUTAG_PATH_ENABLE(x)   nutag_path_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_path_enable;
#define NUTAG_PATH_ENABLE_REF(x) nutag_path_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_path_enable_ref;

#define NUTAG_RETRY_AFTER_ENABLE(x)   nutag_retry_after_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_retry_after_enable;
#define NUTAG_RETRY_AFTER_ENABLE_REF(x) nutag_retry_after_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_retry_after_enable_ref;

#define NUTAG_SERVICE_ROUTE_ENABLE(x) nutag_service_route_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_service_route_enable;
#define NUTAG_SERVICE_ROUTE_ENABLE_REF(x) \
          nutag_service_route_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_service_route_enable_ref;

#define NUTAG_MEDIA_ENABLE(x) nutag_media_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_media_enable;
#define NUTAG_MEDIA_ENABLE_REF(x) \
          nutag_media_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_media_enable_ref;

#define NUTAG_OFFER_RECV(x) nutag_offer_recv, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_offer_recv;
#define NUTAG_OFFER_RECV_REF(x) nutag_offer_recv_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_offer_recv_ref;

#define NUTAG_ANSWER_RECV(x) nutag_answer_recv, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_answer_recv;
#define NUTAG_ANSWER_RECV_REF(x) nutag_answer_recv_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_answer_recv_ref;

#define NUTAG_OFFER_SENT(x) nutag_offer_sent, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_offer_sent;
#define NUTAG_OFFER_SENT_REF(x) nutag_offer_sent_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_offer_sent_ref;

#define NUTAG_ANSWER_SENT(x) nutag_answer_sent, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_answer_sent;
#define NUTAG_ANSWER_SENT_REF(x) nutag_answer_sent_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_answer_sent_ref;

#define NUTAG_DETECT_NETWORK_UPDATES(x) \
          nutag_detect_network_updates, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_detect_network_updates;
#define NUTAG_DETECT_NETWORK_UPDATES_REF(x) \
          nutag_detect_network_updates_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_detect_network_updates_ref;

#define NUTAG_SHUTDOWN_EVENTS(x) \
  nutag_shutdown_events, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_shutdown_events;
#define NUTAG_SHUTDOWN_EVENTS_REF(x) \
  nutag_shutdown_events_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_shutdown_events_ref;

/* Pass nua handle as tagged argument */
#if SU_INLINE_TAG_CAST
su_inline tag_value_t nutag_handle_v(nua_handle_t *v) { return (tag_value_t)v; }
su_inline tag_value_t nutag_handle_vr(nua_handle_t **vp) {return(tag_value_t)vp;}
#else
#define nutag_handle_v(v)   (tag_value_t)(v)
#define nutag_handle_vr(v)  (tag_value_t)(v)
#endif

/* Tags for compatibility */

#define NUTAG_USE_LEG(x) NUTAG_USE_DIALOG(x)
#define NUTAG_USE_LEG_REF(x) NUTAG_USE_DIALOG_REF(x)

#define NUTAG_AF(x) SOATAG_AF((x))
#define NUTAG_AF_REF(x) SOATAG_AF_REF((x))

enum nua_af {
  nutag_af_any = SOA_AF_ANY,
  nutag_af_ip4_only = SOA_AF_IP4_ONLY,
  nutag_af_ip6_only = SOA_AF_IP6_ONLY,
  nutag_af_ip4_ip6 = SOA_AF_IP4_IP6,
  nutag_af_ip6_ip4 = SOA_AF_IP6_IP4
};

#define NUTAG_AF_ANY      nutag_af_any
#define NUTAG_AF_IP4_ONLY nutag_af_ip4_only
#define NUTAG_AF_IP6_ONLY nutag_af_ip6_only
#define NUTAG_AF_IP4_IP6  nutag_af_ip4_ip6
#define NUTAG_AF_IP6_IP4  nutag_af_ip6_ip4

#define NUTAG_MEDIA_ADDRESS(x)  SOATAG_ADDRESS((x))
#define NUTAG_MEDIA_ADDRESS_REF(x)   SOATAG_ADDRESS_REF((x))

#define NUTAG_HOLD(x) SOATAG_HOLD((x) ? "*" : NULL)

#define NUTAG_ACTIVE_AUDIO(x) SOATAG_ACTIVE_AUDIO((x))
#define NUTAG_ACTIVE_AUDIO_REF(x) SOATAG_ACTIVE_AUDIO_REF((x))
#define NUTAG_ACTIVE_VIDEO(x) SOATAG_ACTIVE_VIDEO((x))
#define NUTAG_ACTIVE_VIDEO_REF(x) SOATAG_ACTIVE_VIDEO_REF((x))
#define NUTAG_ACTIVE_IMAGE(x) SOATAG_ACTIVE_IMAGE((x))
#define NUTAG_ACTIVE_IMAGE_REF(x) SOATAG_ACTIVE_IMAGE_REF((x))
#define NUTAG_ACTIVE_CHAT(x) SOATAG_ACTIVE_CHAT((x))
#define NUTAG_ACTIVE_CHAT_REF(x) SOATAG_ACTIVE_CHAT_REF((x))

enum {
  nua_active_rejected = SOA_ACTIVE_REJECTED,
  nua_active_disabled = SOA_ACTIVE_DISABLED,
  nua_active_inactive = SOA_ACTIVE_INACTIVE,
  nua_active_sendonly = SOA_ACTIVE_SENDONLY,
  nua_active_recvonly = SOA_ACTIVE_RECVONLY,
  nua_active_sendrecv = SOA_ACTIVE_SENDRECV
};

#define NUTAG_SRTP_ENABLE(x)  SOATAG_SRTP_ENABLE((x))
#define NUTAG_SRTP_ENABLE_REF(x) SOATAG_SRTP_ENABLE_REF((x))
#define NUTAG_SRTP_CONFIDENTIALITY(x)  SOATAG_SRTP_CONFIDENTIALITY((x))
#define NUTAG_SRTP_CONFIDENTIALITY_REF(x) SOATAG_SRTP_CONFIDENTIALITY_REF((x))
#define NUTAG_SRTP_INTEGRITY_PROTECTION(x)  SOATAG_SRTP_INTEGRITY((x))
#define NUTAG_SRTP_INTEGRITY_PROTECTION_REF(x) SOATAG_SRTP_INTEGRITY_REF((x))

SOFIA_END_DECLS

#endif
