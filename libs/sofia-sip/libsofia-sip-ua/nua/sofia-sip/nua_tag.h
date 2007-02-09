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

/** URL address from application to NUA
 *
 * @par Used with
 *    any function that create SIP request or nua_handle() \n
 *    nua_create() \n
 *    nua_set_params() \n
 *    nua_get_params() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    #url_string_t, which is either a pointer to #url_t or NULL terminated
 *    character string representing URL \n
 *
 * For normal nua calls, this tag is used as request target, which is usually
 * stored as request-URI.
 *
 * It is used to set stack's own address with nua_create(), nua_set_params()
 * and nua_get_params().
 *
 * @sa SIPTAG_TO()
 *
 * Corresponding tag taking reference parameter is NUTAG_URL_REF()
 */
#define NUTAG_URL(x)            nutag_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_url;

#define NUTAG_URL_REF(x)        nutag_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_url_ref;

/** Address as a string
 *
 * @deprecated Not used.
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    String in form "name <url>"
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_ADDRESS_REF()
 */
#define NUTAG_ADDRESS(x)        nutag_address, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_address;

#define NUTAG_ADDRESS_REF(x)    nutag_address_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_address_ref;

/**Specify request to respond to.
 *
 * @par Used with
 *    nua_respond()
 *
 * @par Parameter type
 *    msg_t *
 *
 * @par Values
 *   Pointer to a request message.
 *
 * @NEW_1_12_4.
 *
 * @sa NUTAG_WITH_THIS(), NUTAG_WITH_SAVED()
 */
#define NUTAG_WITH(x)         nutag_with, tag_ptr_v(x)
SOFIAPUBVAR tag_typedef_t nutag_with;

/**Specify request to respond to.
 *
 * @par Used with
 *    nua_respond()
 *
 * @par Parameter type
 *    nua_t *
 *
 * @par Values
 *   Pointer to the nua agent instance object.
 *
 * @NEW_1_12_4.
 *
 * @sa nua_save_event(), NUTAG_WITH(), NUTAG_WITH_SAVED()
 */
#define NUTAG_WITH_THIS(nua) nutag_with, tag_ptr_v(nua_current_request((nua)))

/**Specify request to respond to.
 *
 * @par Used with
 *    nua_respond()
 *
 * @par Parameter type
 *    msg_t *
 *
 * @par Values
 *   Pointer to a saved event.
 *
 * @NEW_1_12_4.
 *
 * @sa nua_save_event(), NUTAG_WITH(), NUTAG_WITH_THIS()
 */
#define NUTAG_WITH_SAVED(e) nutag_with, tag_ptr_v(nua_saved_event_request((e)))

/**Set request retry count.
 *
 * Retry count determines how many times stack will automatically retry
 * after an recoverable error response, like 302, 401 or 407.
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams() \n
 *    nua_get_params(), nua_get_hparams() \n
 *    nua_invite(), nua_ack()
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    @c 0   Never retry automatically \n
 *
 * @NEW_1_12_4.
 *
 * Corresponding tag taking reference parameter is NUTAG_RETRY_COUNT_REF()
 */
#define NUTAG_RETRY_COUNT(x)      nutag_retry_count, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_retry_count;

#define NUTAG_RETRY_COUNT_REF(x)  nutag_retry_count_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_retry_count_ref;

/** Extension method name.
 *
 * Specify extension method name with nua_method() function.
 *
 * @par Used with
 *    nua_method() \n
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    Extension method name (e.g., "SERVICE")
 *
 * Corresponding tag taking reference parameter is NUTAG_METHOD_REF()
 *
 * @sa nua_method(), SIP_METHOD_UNKNOWN()
 *
 * @since New in @VERSION_1_12_4.
 */
#define NUTAG_METHOD(x)            nutag_method, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_method;

#define NUTAG_METHOD_REF(x)        nutag_method_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_method_ref;

/**Set maximum number of simultaneous subscribers per single event server.
 *
 * Determines how many subscribers can simultaneously subscribe to a single
 * event.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    @c 0   Do not allow any subscriptions \n
 *
 * @sa nua_notifier(), nua_authorize()
 *
 * Corresponding tag taking reference parameter is 
 * NUTAG_MAX_SUBSCRIPTIONS_REF()
 */
#define NUTAG_MAX_SUBSCRIPTIONS(x)      nutag_max_subscriptions, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_max_subscriptions;

#define NUTAG_MAX_SUBSCRIPTIONS_REF(x) \
nutag_max_subscriptions_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_max_subscriptions_ref;

/** Intentionally undocumented. */
#define NUTAG_UICC(x)  nutag_uicc, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_uicc;

#define NUTAG_UICC_REF(x) nutag_uicc_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_uicc_ref;

/** Ask NUA to create dialog for this handle
 *
 * @par Used with nua calls that send a SIP request
 *
 * @par Parameter type
 *   int
 *
 * @par Values
 *    @c False (zero) \n
 *    @c True (nonzero)
 *
 * Corresponding tag taking reference parameter is NUTAG_USE_DIALOG_REF()
 */
#define NUTAG_USE_DIALOG(x)        nutag_use_dialog, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_use_dialog;

#define NUTAG_USE_DIALOG_REF(x)    nutag_use_dialog_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_use_dialog_ref;


/* Protocol engine parameters,
 * set by nua_set_params(), get by nua_get_params() */

#if 0

/**Pointer to a SDP Offer-Answer session object.
 *
 * Pointer to the media session object.
 *
 * @par Used with nua_create(), nua_handle().
 *
 * @par Parameter type
 *    void * (actually soa_session_t *)
 *
 * @par Values
 *    Pointer to MSS media session.
 *
 * Corresponding tag taking reference parameter is NUTAG_SOA_SESSION_REF.
 */
#define NUTAG_SOA_SESSION(x)  nutag_soa_session, tag_ptr_v(x)
SOFIAPUBVAR tag_typedef_t nutag_soa_session;

#define NUTAG_SOA_SESSION_REF(x) \
 nutag_soa_session_ref, tag_ptr_vr(&(x),(x))
SOFIAPUBVAR tag_typedef_t nutag_soa_session_ref;

#endif

/**Name for SDP Offer-Answer session object.
 *
 * SDP Offer-Answer session object name.
 *
 * @par Used with nua_create(), nua_handle().
 *
 * @par Parameter type
 *    void * (actually soa_session_t *)
 *
 * @par Values
 *    Pointer to MSS media session.
 *
 * Corresponding tag taking reference parameter is NUTAG_SOA_SESSION_REF.
 */
#define NUTAG_SOA_NAME(x)  nutag_soa_name, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_soa_name;

#define NUTAG_SOA_NAME_REF(x) \
 nutag_soa_name_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_soa_name_ref;

/**Establish early media session using 100rel, 183 responses and PRACK.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_set_hparams() \n
 *    nua_get_hparams() \n
 *    nua_invite() \n
 *    nua_respond() \n
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * @sa NUTAG_EARLY_ANWER()
 *
 * Corresponding tag taking reference parameter is NUTAG_EARLY_MEDIA_REF()
 */
#define NUTAG_EARLY_MEDIA(x)    nutag_early_media, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_early_media;

#define NUTAG_EARLY_MEDIA_REF(x) nutag_early_media_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_early_media_ref;

/**Respond only 183 with 100rel.
 *
 * If this parameter is set, stack uses 100rel only with 183: otherwise, all
 * 1XX responses (except <i>100 Trying</i>) uses 100rel.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_set_hparams() \n
 *    nua_get_hparams() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is NUTAG_ONLY183_100REL_REF()
*/
#define NUTAG_ONLY183_100REL(x)    nutag_only183_100rel, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_only183_100rel;

#define NUTAG_ONLY183_100REL_REF(x) nutag_only183_100rel_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_only183_100rel_ref;

/**Establish early media session by including SDP answer in 1XX response.
 *
 * @par Used with
 *    nua_respond(), nua_set_params(), nua_set_hparams()
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is NUTAG_EARLY_ANSWER_REF().
 *
 * @note Requires that @soa is enabled with NUTAG_MEDIA_ENABLE(1).
 *
 * @sa NUTAG_EARLY_MEDIA(), NUTAG_AUTOALERT(), NUTAG_MEDIA_ENABLE()
 * 
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_EARLY_ANSWER(x)    nutag_early_answer, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_early_answer;

#define NUTAG_EARLY_ANSWER_REF(x) nutag_early_answer_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_early_answer_ref;

/**Include an extra copy of SDP answer in the response.
 *
 * When NUTAG_INCLUDE_EXTRA_SDP(1) is included in nua_respond() tags, stack
 * will include in the response a copy of the SDP offer/answer that was last
 * sent to the client. This tag should be used only when you know that the
 * remote end requires the extra SDP, for example, some versions of Cisco
 * SIPGateway need a copy of answer in 200 OK even when they indicate
 * support for 100rel.
 *
 * @par Used with
 *    nua_respond()
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_INCLUDE_EXTRA_SDP_REF().
 *
 * @note Requires that @soa is enabled with NUTAG_MEDIA_ENABLE(1).
 *
 * @sa NUTAG_EARLY_ANSWER(), NUTAG_EARLY_MEDIA(), NUTAG_AUTOALERT(),
 * NUTAG_MEDIA_ENABLE(), @RFC3264, @RFC3264
 * 
 * @since New in @VERSION_1_12_4.
 */
#define NUTAG_INCLUDE_EXTRA_SDP(x)    nutag_include_extra_sdp, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_include_extra_sdp;

#define NUTAG_INCLUDE_EXTRA_SDP_REF(x) \
   nutag_include_extra_sdp_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_include_extra_sdp_ref;

/** Timer for outstanding INVITE in seconds.
 *
 * INVITE will be canceled if no answer is received before timer expires.
 *
 * @par Used with
 *    nua_invite() \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (enum nua_af)
 *
 * @par Values
 *    @c 0  no timer \n
 *    @c >0 timer in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_INVITE_TIMER_REF()
 */
#define NUTAG_INVITE_TIMER(x)  nutag_invite_timer, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_invite_timer;

#define NUTAG_INVITE_TIMER_REF(x) nutag_invite_timer_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_invite_timer_ref;

/**Default session timer in seconds.
 *
 * Set default session timer in seconds when using session timer extension. 
 * The value given here is the proposed session expiration time in seconds.
 * Note that the session timer extension is ponly used 
 *
 * @par Sending INVITE and UPDATE Requests 
 *
 * If NUTAG_SESSION_TIMER() is used with non-zero value, the value is
 * used in the @SessionExpires header included in the INVITE or UPDATE
 * requests. The intermediate proxies or the ultimate destination can lower
 * the interval in @SessionExpires header. If the value is too low, they can
 * reject the request with the status code <i>422 Session Timer Too
 * Small</i>. When Re-INVITE will be sent in given intervals. In that case,
 * @b nua retries the request automatically.
 * 
 * @par Returning Response to the INVITE and UPDATE Requests 
 *
 * The NUTAG_SESSION_TIMER() value is also used when sending the final
 * response to the INVITE or UPDATE requests. If the NUTAG_SESSION_TIMER()
 * value is 0 or the value in the @SessionExpires header of the requeast is
 * lower than the value in NUTAG_SESSION_TIMER(), the value from the
 * incoming @SessionExpires header is used. However, if the value in
 * @SessionExpires is lower than the minimal acceptable session expiration
 * interval specified with the tag NUTAG_MIN_SE() the request is
 * automatically rejected with <i>422 Session Timer Too Small</i>.
 *
 * @par When to Use NUTAG_SESSION_TIMER()?
 *
 * The session time extension is enabled ("timer" feature tag is included in
 * @Supported header) but not activated by default (no @SessionExpires
 * header is included in the requests or responses by default). Using
 * non-zero value with NUTAG_SESSION_TIMER() activates it. When the
 * extension is activated, @nua refreshes the call state by sending periodic
 * re-INVITE or UPDATE requests unless the remote end indicated that it will
 * take care of refreshes.
 *
 * The session timer extension is mainly useful for proxies or back-to-back
 * user agents that keep call state. The call state is "soft" meaning that
 * if no call-related SIP messages are processed for certain time the state
 * will be destroyed. An ordinary user-agent can also make use of session
 * timer if it cannot get any activity feedback from RTP or other media.
 *
 * @par Used with
 *    nua_invite(), nua_update(), nua_respond() \n
 *    nua_set_params() or nua_set_hparams() \n
 *    nua_get_params() or nua_get_hparams()
 *
 * See nua_set_hparams() for a complete list of the the nua operations that
 * accept this tag.
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    @c 0  disable \n
 *    @c >0 interval in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_SESSION_TIMER_REF()
 *
 * @sa NUTAG_SUPPORTED(), NUTAG_MIN_SE(), NUTAG_SESSION_REFRESHER(),
 * nua_invite(), #nua_r_invite, #nua_i_invite, nua_update(), #nua_r_update,
 * #nua_i_update, 
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @SessionExpires, @MinSE
 */
#define NUTAG_SESSION_TIMER(x)  nutag_session_timer, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_session_timer;

#define NUTAG_SESSION_TIMER_REF(x) nutag_session_timer_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_session_timer_ref;

/** Minimum acceptable refresh interval for session.
 *
 * Specifies the value of @MinSE header in seconds. The @b Min-SE header is
 * used to specify minimum acceptable refresh interval for session timer
 * extension.
 *
 * @par Used with
 *    nua_handle(), nua_invite(), nua_update(), nua_respond() \n
 *    nua_set_params() or nua_set_hparams() \n
 *    nua_get_params() or nua_get_hparams()
 *
 * See nua_set_hparams() for a complete list of the nua operations that
 * accept this tag.
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    interval in seconds.
 *
 * Corresponding tag taking reference parameter is NUTAG_MIN_SE_REF()
 *
 * @sa NUTAG_SESSION_TIMER(), NUTAG_SESSION_REFRESHER(),
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @MinSE, @SessionExpires
 */
#define NUTAG_MIN_SE(x)         nutag_min_se, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_min_se;

#define NUTAG_MIN_SE_REF(x)     nutag_min_se_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_min_se_ref;

enum nua_session_refresher {
  nua_no_refresher,		/**< Disable session timer. */
  nua_local_refresher,		/**< Session refresh by local end. */
  nua_remote_refresher,		/**< Session refresh by remote end. */
  nua_any_refresher		/**< No preference (default). */
};

/**Specify the preferred refresher.
 *
 * Specify for session timer extension which party is the preferred refresher.
 *
 * @par Used with
 *    nua_handle(), nua_invite(), nua_update(), nua_respond() \n
 *    nua_set_params() or nua_set_hparams() \n
 *    nua_get_params() or nua_get_hparams()
 *
 * See nua_set_hparams() for a complete list of all the nua operations that
 * accept this tag.
 *
 * @par Parameter type
 *   enum { #nua_no_refresher,  #nua_local_refresher, #nua_remote_refresher,
 *          #nua_any_refresher }
 *
 * @par Values
 *    @c nua_no_refresher (session timers are disabled) \n
 *    @c nua_local_refresher \n
 *    @c nua_remote_refresher \n
 *    @c nua_any_refresher (default) \n
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_SESSION_REFRESHER_REF()
 *
 * @sa NUTAG_SESSION_TIMER(), NUTAG_MIN_SE_REF(),
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @SessionExpires, @MinSE
 */
#define NUTAG_SESSION_REFRESHER(x)  nutag_session_refresher, tag_int_v((x))
SOFIAPUBVAR tag_typedef_t nutag_session_refresher;

#define NUTAG_SESSION_REFRESHER_REF(x) nutag_session_refresher_ref, tag_int_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_session_refresher_ref;

/** Use UPDATE as refresh method.
 *
 * If this parameter is true and the remote endpoint has included UPDATE in
 * Allow header, the nua stack uses UPDATE instead of INVITE to refresh the 
 * session when using the session timer extension.
 *
 * Note that the session timer headers @SessionExpires and @MinSE are always
 * included in the UPDATE request and responses regardless of the value of
 * this tag.
 *
 * @par Used with
 *    nua_handle(), nua_invite(), nua_update(), nua_respond() \n
 *    nua_set_params() or nua_set_hparams() \n
 *    nua_get_params() or nua_get_hparams()
 *
 * See nua_set_hparams() for a complete list of all the nua operations that
 * accept this tag.
 *
 * @par Parameter type
 *    boolean
 *
 * @par Values
 *    @c 1 Use UPDATE \n
 *    @c 0 Use INVITE
 *
 * Corresponding tag taking reference parameter is NUTAG_UPDATE_REFRESH_REF()
 *
 * @sa #nua_r_update, NUTAG_SESSION_TIMER(), NUTAG_MIN_SE_REF(),
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @SessionExpires, @MinSE
 */
#define NUTAG_UPDATE_REFRESH(x)  nutag_update_refresh, tag_bool_v((x))
SOFIAPUBVAR tag_typedef_t nutag_update_refresh;

#define NUTAG_UPDATE_REFRESH_REF(x) nutag_update_refresh_ref, tag_bool_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_update_refresh_ref;

/** Send alerting (180 Ringing) automatically
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   No automatic sending of "180 Ringing" \n
 *    @c !=0 "180 Ringing" sent automatically
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTOALERT_REF()
 */
#define NUTAG_AUTOALERT(x)      nutag_autoalert, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoalert;

#define NUTAG_AUTOALERT_REF(x)  nutag_autoalert_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoalert_ref;

/** ACK automatically
 *
 * If this parameter is true, ACK is sent automatically after receiving 2XX
 * series response to INVITE. Note that ACK is always sent automatically by
 * lower layers of the stack after receiving an error response 3XX, 4XX, 5XX
 * or 6XX.
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams(), \n
 *    nua_get_params(), nua_get_hparams(), \n
 *    nua_invite(), nua_ack(), nua_respond(), nua_update() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0    No automatic sending of ACK \n
 *    @c !=0 ACK sent automatically
 *
 * Default value is NUTAG_AUTOACK(1).
 * 
 * Corresponding tag taking reference parameter is NUTAG_AUTOACK_REF()
 */
#define NUTAG_AUTOACK(x)        nutag_autoack, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoack;

#define NUTAG_AUTOACK_REF(x)    nutag_autoack_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoack_ref;

/** Answer (with 200 Ok) automatically to incoming call.
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams() \n
 *    nua_get_params(), nua_get_hparams() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *    @c 0    No automatic sending of "200 Ok" \n
 *    @c !=0 "200 Ok" sent automatically
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTOANSWER_REF()
 *
 * @note Requires that @soa is enabled with NUTAG_MEDIA_ENABLE(1).
 * 
 * @par Auto-Answer to Re-INVITE requests
 * By default, NUA tries to auto answer the re-INVITEs used to refresh the
 * session when the media is enabled. Set NUTAG_AUTOANSWER(0) on the call
 * handle (e.g., include the tag with nua_invite(), nua_respond()) in order
 * to disable the auto answer on re-INVITEs.
 *
 * @bug If the re-INVITE modifies the session (e.g., SDP contains offer that
 * adds video stream to the session), NUA auto-answers it if
 * NUTAG_AUTOANSWER(0) has not been set on the handle. It accepts or rejects
 * media based on the existing user SDP (set with SOATAG_USER_SDP(), for
 * example). It should auto-answer only session refresh request and let
 * application decide how to handle requests to modify the session.
 *
 * @sa NUTAG_MEDIA_ENABLE(), NUTAG_AUTOALERT(), NUTAG_AUTOACK().
 */
#define NUTAG_AUTOANSWER(x)     nutag_autoanswer, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_autoanswer;

#define NUTAG_AUTOANSWER_REF(x) nutag_autoanswer_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_autoanswer_ref;

/** Enable incoming INVITE
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   Incoming INVITE not enabled. NUA answers 403 Forbidden \n
 *    @c !=0 Incoming INVITE enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEINVITE_REF()
 */
#define NUTAG_ENABLEINVITE(x)   nutag_enableinvite, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enableinvite;

#define NUTAG_ENABLEINVITE_REF(x) nutag_enableinvite_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enableinvite_ref;

/** Enable incoming MESSAGE
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   Incoming MESSAGE not enabled. NUA answers 403 Forbidden \n
 *    @c !=0 Incoming MESSAGE enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEMESSAGE_REF()
 */
#define NUTAG_ENABLEMESSAGE(x)  nutag_enablemessage, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enablemessage;

#define NUTAG_ENABLEMESSAGE_REF(x) nutag_enablemessage_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enablemessage_ref;

/** Enable incoming MESSAGE with To tag.
 *
 * Set this parameter if you want to chat with Windows Messenger.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEMESSENGER_REF()
 */
#define NUTAG_ENABLEMESSENGER(x)  nutag_enablemessenger, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_enablemessenger;

#define NUTAG_ENABLEMESSENGER_REF(x) \
  nutag_enablemessenger_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_enablemessenger_ref;

/* Start NRC Boston */

/** Enable S/MIME
 *
 * @par Used with
 *    nua_create() \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    boolean
 *
 * @par Values
 *    @c 0   S/MIME is Disabled \n
 *    @c !=0 S/MIME is Enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_ENABLE_REF()
 */
#define NUTAG_SMIME_ENABLE(x)  nutag_smime_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_enable;

#define NUTAG_SMIME_ENABLE_REF(x) nutag_smime_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_enable_ref;

/** S/MIME Options
 *
 * This tag specifies the type of S/MIME security services requested
 * by the user.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_message()
 *
 * @par Parameter type
 *   int
 *
 * @par Values
 *   @c -1 (SM_ID_NULL) No security service needed \n
 *   @c  0 (SM_ID_CLEAR_SIGN) Clear signing \n
 *   @c  1 (SM_ID_SIGN) S/MIME signing \n
 *   @c  2 (SM_ID_ENCRYPT) S/MIME encryption
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_OPT_REF()
 */
#define NUTAG_SMIME_OPT(x)  nutag_smime_opt, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_opt;

#define NUTAG_SMIME_OPT_REF(x) nutag_smime_opt_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_opt_ref;

/* End NRC Boston */

/** S/MIME protection mode
 *
 * This tag specifies the protection mode of the SIP message by
 * S/MIME as requested by the user
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *   unsigned int
 *
 * @par Values
 *   @c -1 (SM_MODE_NULL) Unspecified \n
 *   @c  0 (SM_MODE_PAYLOAD_ONLY) SIP payload only \n
 *   @c  1 (SM_MODE_TUNNEL) SIP tunneling mode \n
 *   @c  2 (SM_MODE_SIPFRAG) SIPfrag protection
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_PROTECTION_MODE_REF()
 */
#define NUTAG_SMIME_PROTECTION_MODE(x) nutag_smime_protection_mode, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_protection_mode;

#define NUTAG_SMIME_PROTECTION_MODE_REF(x) \
           nutag_smime_protection_mode_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_smime_protection_mode_ref;

/** S/MIME digest algorithm
 *
 * This tag specifies the message digest algorithm to be used in S/MIME.
 *
 * @par Used with
 *    To be implemented
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_MESSAGE_DIGEST_REF()
 */
#define NUTAG_SMIME_MESSAGE_DIGEST(x) nutag_smime_message_digest, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_message_digest;

#define NUTAG_SMIME_MESSAGE_DIGEST_REF(x) \
            nutag_smime_message_digest_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_message_digest_ref;

/** S/MIME signature algorithm
 *
 * This tag specifies the signature algorithm to be used in S/MIME.
 *
 * @par Used with
 *    To be implemented.
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_SIGNATURE_REF()
 */
#define NUTAG_SMIME_SIGNATURE(x) nutag_smime_signature, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_signature;

#define NUTAG_SMIME_SIGNATURE_REF(x) \
            nutag_smime_signature_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_signature_ref;

/** S/MIME key encryption algorithm
 *
 * This tag specifies the key encryption algorithm to be used by S/MIME.
 *
 * @par Used with
 *    To be implemented
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_KEY_ENCRYPTION_REF()
 */
#define NUTAG_SMIME_KEY_ENCRYPTION(x) nutag_smime_key_encryption, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_key_encryption;

#define NUTAG_SMIME_KEY_ENCRYPTION_REF(x) \
          nutag_smime_key_encryption_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_key_encryption_ref;

/** S/MIME message encryption algorithm
 *
 * This tag specifies the message encryption algorithm to be used in S/MIME.
 *
 * @par Used with
 *    To be implemented.
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_MESSAGE_ENCRYPTION_REF()
 */
#define NUTAG_SMIME_MESSAGE_ENCRYPTION(x) nutag_smime_message_encryption, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_smime_message_encryption;

#define NUTAG_SMIME_MESSAGE_ENCRYPTION_REF(x) \
           nutag_smime_message_encryption_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_smime_message_encryption_ref;

/** x.500 certificate directory
 *
 * @par Used with
 *    nua_create()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    NULL terminated pathname of directory containing agent.pem and cafile.pem files.
 *
 * Corresponding tag taking reference parameter is NUTAG_CERTIFICATE_DIR_REF()
 */
#define NUTAG_CERTIFICATE_DIR(x) nutag_certificate_dir, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_certificate_dir;

#define NUTAG_CERTIFICATE_DIR_REF(x) \
          nutag_certificate_dir_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_certificate_dir_ref;

/** Certificate phrase
 *
 * @par Used with
 *    Currently not processed by NUA
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_CERTIFICATE_PHRASE_REF()
 */
#define NUTAG_CERTIFICATE_PHRASE(x) nutag_certificate_phrase, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_certificate_phrase;

#define NUTAG_CERTIFICATE_PHRASE_REF(x) \
          nutag_certificate_phrase_ref, tag_str_vr((&x))
SOFIAPUBVAR tag_typedef_t nutag_certificate_phrase_ref;

/** Local SIPS url.
 *
 * The application can specify an alternative local address for
 * NUA user agent engine. Usually the alternative address is a
 * secure SIP URI (SIPS) used with TLS transport.
 *
 * @par Used with
 *    nua_create()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_SIPS_URL_REF()
 */
#define NUTAG_SIPS_URL(x)       nutag_sips_url, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_sips_url;

#define NUTAG_SIPS_URL_REF(x)   nutag_sips_url_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_sips_url_ref;

/** Outbound proxy URL
 *
 * Same tag as NTATAG_DEFAULT_PROXY()
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_create()
 *
 * @par Parameter type
 *    url_string_t const * (either char const * or url_t *)
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_PROXY_REF()
 */
#define NUTAG_PROXY(x)          NTATAG_DEFAULT_PROXY(x)
#define NUTAG_PROXY_REF(x)      NTATAG_DEFAULT_PROXY_REF(x)
#define nutag_proxy             ntatag_default_proxy

/** Registrar URL
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    url_string_t const * (either char const * or url_t *)
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_REGISTRAR_REF()
 */
#define NUTAG_REGISTRAR(x)      nutag_registrar, urltag_url_v(x)
SOFIAPUBVAR tag_typedef_t nutag_registrar;

#define NUTAG_REGISTRAR_REF(x)  nutag_registrar_ref, urltag_url_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_registrar_ref;

/** Outbound option string.
 *
 * The outbound option string can specify how the NAT traversal is handled.
 * The option tokens are as follows:
 * - "gruuize": try to generate a GRUU contact from REGISTER response
 * - "outbound": use SIP outbound extension (off by default)
 * - "validate": validate registration behind a NAT by sending OPTIONS to self
 * - "natify": try to traverse NAT
 * - "use-rport": use rport to traverse NAT
 * - "options-keepalive": send periodic OPTIONS requests as keepalive messages
 *
 * An option token with "no-" or "not-" prefix turns the option off. For
 * example, if you want to try to traverse NATs but not to use OPTIONS
 * keepalive, use NUTAG_OUTBOUND("natify no-options-keepalive").
 * 
 * An empty string can be passed to let the stack choose the
 * default values for outbound usage (in the 1.12.5 release, the
 * defaults are: "gruuize no-outbound validate use-port options-keepalive").
 *
 * @note
 * Options string is used so that no new tags need to be added when the
 * outbound functionality changes.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_REF()
 */
#define NUTAG_OUTBOUND(x)      nutag_outbound, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound;

#define NUTAG_OUTBOUND_REF(x)  nutag_outbound_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_ref;

#if notyet

/** Outbound proxy set 1.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET1_REF()
 */
#define NUTAG_OUTBOUND_SET1(x)      nutag_outbound_set1, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set1;

#define NUTAG_OUTBOUND_SET1_REF(x)  nutag_outbound_set1_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set1_ref;

/** Outbound proxy set 2.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET2_REF()
 */
#define NUTAG_OUTBOUND_SET2(x)      nutag_outbound_set2, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set2;

#define NUTAG_OUTBOUND_SET2_REF(x)  nutag_outbound_set2_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set2_ref;

/** Outbound proxy set 3.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET3_REF()
 */
#define NUTAG_OUTBOUND_SET3(x)      nutag_outbound_set3, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set3;

#define NUTAG_OUTBOUND_SET3_REF(x)  nutag_outbound_set3_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set3_ref;

/** Outbound proxy set 4.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET4_REF()
 */
#define NUTAG_OUTBOUND_SET4(x)      nutag_outbound_set4, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_outbound_set4;

#define NUTAG_OUTBOUND_SET4_REF(x)  nutag_outbound_set4_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_outbound_set4_ref;

#endif	/* ...notyet */

/** Pointer to SIP parser structure
 *
 * @par Used with
 *    nua_create()
 *
 * @par Parameter type
 *    msg_mclass_t const *
 *
 * @par Values
 *    Pointer to an extended SIP parser.
 *
 * @sa msg_mclass_clone(), msg_mclass_insert_header()
 *
 * Corresponding tag taking reference parameter is NUTAG_SIP_PARSER_REF().
 */
#define NUTAG_SIP_PARSER(x)     NTATAG_MCLASS(x)
#define NUTAG_SIP_PARSER_REF(x) NTATAG_MCLASS_REF(x)

/** Authentication data ("scheme" "realm" "user" "password")
 *
 * @par Used with
 *    nua_authenticate()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    NULL terminated string of format: \n
 *    basic digest scheme:"realm":user:password  \n
 *    @b NOTE the double quotes around realm!
 *    For example: \n
 *	\code Digest:"nokia proxy":xyz:secret \endcode
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTH_REF()
 */
#define NUTAG_AUTH(x)		nutag_auth, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_auth;

#define NUTAG_AUTH_REF(x)	    nutag_auth_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_auth_ref;

/** Keepalive interval in milliseconds.
 *
 * This setting applies to OPTIONS/STUN keepalives. See documentation 
 * for nua_register() for more detailed information.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values 
 *   - 0 - disable keepalives
 *   - 120000 - default value (120000 milliseconds, 120 seconds)
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_KEEPALIVE_REF()
 */
#define NUTAG_KEEPALIVE(x) nutag_keepalive, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_keepalive;

#define NUTAG_KEEPALIVE_REF(x) nutag_keepalive_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_keepalive_ref;

/** Transport-level keepalive interval for streams.
 *
 * See documentation for nua_register() for more detailed information.
 *
 * @par Used with
 *    nua_register()   \n
 *    nua_set_params() \n
 *    nua_get_params()
 *    nua_set_hparams() \n
 *    nua_get_hparams()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values 
 *
 * Transport-level keepalive interval for streams in milliseconds. If this
 * parameter specified, it takes presedence over value given in
 * NUTAG_KEEPALIVE().
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_KEEPALIVE_STREAM_REF()
 *
 * @todo Actually pass NUTAG_KEEPALIVE_STREAM() to transport layer.
 */
#define NUTAG_KEEPALIVE_STREAM(x) nutag_keepalive_stream, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_keepalive_stream;

#define NUTAG_KEEPALIVE_STREAM_REF(x) \
nutag_keepalive_stream_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_keepalive_stream_ref;

/** Lifetime of authentication data in seconds.
 *
 * @par Used with
 *    Currently not processed by NUA
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    @c 0   Use authentication data only for this handle \n
 *    @c !=0 Lifetime in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTHTIME_REF()
 */
#define NUTAG_AUTHTIME(x)	nutag_authtime, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_authtime;

#define NUTAG_AUTHTIME_REF(x)	nutag_authtime_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_authtime_ref;

/**Display name for @Contact.
 *
 * Specify display name for the Contact header URI generated for
 * registration request and dialog-creating requests/responses.
 *
 * Note that display name is not included the request-URI when proxy
 * forwards the request towards user-agent.
 *
 * @par Used with
 *    nua_register(), nua_set_hparams(), nua_set_params().
 *    nua_invite(), nua_respond(), nua_subscribe(), nua_notify()
 *
 * @par Parameter type
 *    string (char *)
 *
 * @par Values
 *    Valid display name.
 *
 * @sa NUTAG_M_USERNAME(), NUTAG_M_PARAMS(), NUTAG_M_FEATURES(),
 * NUTAG_CALLEE_CAPS().
 *
 * Corresponding tag taking reference parameter is NUTAG_M_DISPLAY_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_M_DISPLAY(x)   nutag_m_display, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_display;

#define NUTAG_M_DISPLAY_REF(x) nutag_m_display_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_display_ref;

/**Username prefix for @Contact.
 *
 * Specify username part for the Contact header URI generated for
 * registration request and dialog-creating requests/responses.
 *
 * Using username, application can make multiple registrations using
 * multiple identities, or it can distinguish between different logical
 * destinations.
 *
 * @par Used with
 *    nua_register(), nua_set_hparams(), nua_set_params().
 *    nua_invite(), nua_respond(), nua_subscribe(), nua_notify()
 *
 * @par Parameter type
 *    string (char *)
 *
 * @par Values
 *    Valid SIP username.
 *
 * @sa NUTAG_M_DISPLAY(), NUTAG_M_PARAMS(), NUTAG_M_FEATURES(),
 * NUTAG_CALLEE_CAPS().
 *
 * Corresponding tag taking reference parameter is NUTAG_M_USERNAME_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_M_USERNAME(x)   nutag_m_username, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_username;

#define NUTAG_M_USERNAME_REF(x) nutag_m_username_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_username_ref;

/**URL parameters for @Contact.
 *
 * Specify URL parameters for the Contact header URI generated for
 * registration request and dialog-creating requests/responses.
 *
 * Please note that some proxies may remove even the non-transport
 * parameters from the request-URI when they forward the request towards
 * user-agent.
 *
 * @par Used with
 *    nua_register(), nua_set_hparams(), nua_set_params(), 
 *    nua_invite(), nua_respond(), nua_subscribe(), nua_notify()
 *
 * @par Parameter type
 *    string (char *)
 *
 * @par Values
 *    Semicolon-separated URL parameters.
 *
 * @sa NUTAG_M_DISPLAY(), NUTAG_M_USERNAME(), NUTAG_M_FEATURES(),
 * NUTAG_CALLEE_CAPS().
 *
 * Corresponding tag taking reference parameter is NUTAG_M_PARAMS_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_M_PARAMS(x)   nutag_m_params, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_params;

#define NUTAG_M_PARAMS_REF(x) nutag_m_params_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_params_ref;

/**Header parameters for registration @Contact.
 *
 * Specify header parameters for the @Contact header generated for
 * registration request and dialog-creating requests/responses. Such header
 * parameters include "q", indicating preference for the @Contact URI, and
 * "expires", indicating the desired expiration time for the registration.
 *
 * Additional header parameters are typically media feature tags, specified in
 * @RFC3840. If NUTAG_CALLEE_CAPS(1) is specified, additional @Contact header
 * parameters are generated based on SDP capabilities and SIP @Allow header.
 *
 * When using the "outbound" extension option, the stack will also add
 * "+sip.instance" and "reg-id" header parameters to the @Contact.
 *
 * @par Used with
 *    nua_register(), nua_set_hparams(), nua_set_params()
 *
 * @par Parameter type
 *    string (char *)
 *
 * @par Values
 *    Semicolon-separated SIP header parameters.
 *
 * @sa NUTAG_M_DISPLAY(), NUTAG_M_USERNAME(), NUTAG_M_PARAMS(),
 * NUTAG_CALLEE_CAPS(), NUTAG_IDENTITY().
 *
 * Corresponding tag taking reference parameter is NUTAG_M_FEATURES_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_M_FEATURES(x)   nutag_m_features, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_m_features;

#define NUTAG_M_FEATURES_REF(x) nutag_m_features_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_m_features_ref;

/**NUA event.
 *
 * @deprecated
 *
 * @par Parameter type
 *    enum nua_event_e
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_EVENT_REF()
 */
#define NUTAG_EVENT(x)          nutag_event, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_event;

#define NUTAG_EVENT_REF(x)      nutag_event_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_event_ref;

/** Response status code
 *
 * @deprecated
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 * 100 - preliminary response, request is being processed by next hop \n
 * 1XX - preliminary response, request is being processed by UAS \n
 * 2XX - successful final response \n
 * 3XX - redirection error response \n
 * 4XX - client error response \n
 * 5XX - server error response \n
 * 6XX - global error response \n
 *
 * Corresponding tag taking reference parameter is NUTAG_STATUS_REF()
 */
#define NUTAG_STATUS(x)         nutag_status, tag_uint_v(x)
SOFIAPUBVAR tag_typedef_t nutag_status;

#define NUTAG_STATUS_REF(x)     nutag_status_ref, tag_uint_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_status_ref;

/** Response phrase
 *
 * @deprecated
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values.
 *
 * Corresponding tag taking reference parameter is NUTAG_PHRASE_REF()
 */
#define NUTAG_PHRASE(x)         nutag_phrase, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_phrase;

#define NUTAG_PHRASE_REF(x)     nutag_phrase_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_phrase_ref;

/** NUA Handle
 *
 * @deprecated
 *
 * @par Parameter type
 *    nua_handle_t *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_HANDLE_REF()
 */
#define NUTAG_HANDLE(x)         nutag_handle, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_handle;

#define NUTAG_HANDLE_REF(x)     nutag_handle_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_handle_ref;

/** Registration handle (used with requests and nua_respond()) (NOT YET IMPLEMENTED)
 *
 * When a new request is made or new call is responded, a new identity can
 * be selected with NUTAG_IDENTITY(). The identity comprises of @b From
 * header, initial route set, local contact header and media tags associated
 * with it, soa handle and so on. User can make multiple registrations using
 * multiple identities.
 *
 * @par Used with
 *    nua_invite()
 *
 * @par Parameter type
 *    nua_handle_t *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_IDENTITY_REF()
*/
#define NUTAG_IDENTITY(x)   nutag_identity, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_identity;

#define NUTAG_IDENTITY_REF(x) nutag_identity_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_identity_ref;

/**Intance identifier.
 *
 * @par Used with
 *    nua_create(), nua_set_params(), nua_get_params(), 
 *    nua_register()
 *
 * @par Parameter type
 *    char const *
 *
 * @par Value
 *    urn:uuid string, a globally unique identifier for this user-agent
 *    instance.
 *
 * Corresponding tag taking reference parameter is NUTAG_INSTANCE_REF()
 */
#define NUTAG_INSTANCE(x)        nutag_instance, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_instance;

#define NUTAG_INSTANCE_REF(x)    nutag_instance_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_instance_ref;

/** Refer reply handle (used with refer)
 *
 * When making a call in response to a REFER request [RFC3515] with
 * nua_invite(), the application can ask NUA to automatically generate
 * notifications about the call progress to the referrer. In order to
 * do that the application should pass to the stack the handle, which
 * it used to receive the REFER request. It should also pass the event
 * header object along with the handle using NUTAG_REFER_EVENT().
 *
 * @par Used with
 *    nua_invite()
 *
 * @par Parameter type
 *    nua_handle_t *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_NOTIFY_REFER_REF()
*/
#define NUTAG_NOTIFY_REFER(x)   nutag_notify_refer, nutag_handle_v(x)
SOFIAPUBVAR tag_typedef_t nutag_notify_refer;

#define NUTAG_NOTIFY_REFER_REF(x) nutag_notify_refer_ref, nutag_handle_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_notify_refer_ref;

/** Event used with automatic refer notifications.
 *
 * When creating a call in response to a REFER request [RFC3515]
 * the application can ask NUA to automatically generate notifications
 * about the call progress to the referrer. The #nua_i_refer event will
 * contain a suitable SIP event header for the notifications in the
 * NUTAG_REFER_EVENT() tag. The application should store the SIP event
 * header and when it makes the referred call, it should pass it back
 * to the stack again using the NUTAG_REFER_EVENT() tag.
 *
 * @par Used with
 *
 * @par Parameter type
 *    sip_event_t *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_EVENT_REF()
 */
#define NUTAG_REFER_EVENT(x)   nutag_refer_event, siptag_event_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_event;

#define NUTAG_REFER_EVENT_REF(x) nutag_refer_event_ref, siptag_event_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_event_ref;

/** Invite pauses referrer's handle.
 *
 * When creating a call in response to a REFER [RFC3515] request,
 * the application can ask that the original call will be muted
 * when the new call is connected by specifying NUTAG_REFER_PAUSE()
 * along with NUTAG_NOTIFY_REFER() as a parameter to nua_invite() call.
 *
 * @par Used with
 *    nua_invite()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_PAUSE_REF()
 *
 * @deprecated Not implemented.
 */
#define NUTAG_REFER_PAUSE(x)   nutag_refer_pause, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_pause;

#define NUTAG_REFER_PAUSE_REF(x) nutag_refer_pause_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_pause_ref;

/**User-Agent string.
 *
 * Indicate the User-Agent header used by the stack. The value set with this
 * tag is concatenated with the value indicating the stack name and version,
 * e.g., "sofia-sip/1.12.1" unless the stack name "sofia-sip" followed by
 * slash is already included in the string. The concatenated value is
 * returned in SIPTAG_USER_AGENT_STR() and NUTAG_USER_AGENT() when
 * nua_get_params() is called.
 *
 * If you want to set the complete string, use SIPTAG_USER_AGENT_STR() or
 * SIPTAG_USER_AGENT().
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams() \n
 *    nua_get_params(), nua_get_hparams(), #nua_r_get_params \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    See @RFC3261 \n
 *    If NULL, stack uses default string which of format "sofia-sip/1.12".
 *
 * Corresponding tag taking reference parameter is NUTAG_USER_AGENT_REF()
 */
#define NUTAG_USER_AGENT(x)     nutag_user_agent, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_user_agent;

#define NUTAG_USER_AGENT_REF(x) nutag_user_agent_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_user_agent_ref;

/** Allow a method (or methods).
 *
 * This tag is used to add a new method to the already existing set of
 * allowed methods. If you want to ignore the existing set of allowed
 * methods, use SIPTAG_ALLOW_STR() or SIPTAG_ALLOW().
 *
 * The set of allowed methods is added to the @Allow header in the response
 * or request messages. For incoming request, an error response <i>405
 * Method Not Allowed</i> is automatically returned if the incoming method
 * is not included in the set.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    Valid method name, or comma-separated list of them.
 *
 * Corresponding tag taking reference parameter is NUTAG_ALLOW_REF()
 */
#define NUTAG_ALLOW(x)     nutag_allow, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_allow;

#define NUTAG_ALLOW_REF(x) nutag_allow_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_allow_ref;


/** Indicate that a method (or methods) are handled by application.
 *
 * This tag is used to add a new method to the already existing set of
 * methods handled by application, or clear the set. If you want to
 * determine the set explicitly, include NUTAG_APPL_METHOD() twice,
 * first with NULL and then with your supported set.
 *
 * The default set of application methods now include INVITE, REGISTER,
 * PUBLISH and SUBSCRIBE.
 *
 * If the request method is in the set of methods handled by application,
 * the nua stack does not automatically respond to the incoming request nor
 * it will automatically send such a request. Note if the application adds
 * the PRACK and UPDATE requests to the set of application methods it must
 * also take care for sending the PRACK and UPDATE requests during the call
 * setup when necessary.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    Valid method name, or comma-separated list of them.
 *
 * Corresponding tag taking reference parameter is NUTAG_APPL_METHOD_REF()
 *
 * @since Working since @VERSION_1_12_5. 
 */
#define NUTAG_APPL_METHOD(x)     nutag_appl_method, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_appl_method;

#define NUTAG_APPL_METHOD_REF(x) nutag_appl_method_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_appl_method_ref;


/** Support a feature.
 *
 * This tag is used to add a new feature to the existing set of supported
 * SIP features. If you want to ignore the existing set of supported
 * features, use SIPTAG_SUPPORTED_STR() or SIPTAG_SUPPORTED().
 *
 * The set of supported features is added to the @Supported header in the
 * response or request messages. For incoming requests, an error response
 * <i>420 Bad Extension </i> is automatically returned if the request
 * requires features that are not included in the supported feature set.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    Feature name, or comma-separated list of them.
 *
 * Corresponding tag taking reference parameter is NUTAG_SUPPORTED_REF()
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_SUPPORTED(x)     nutag_supported, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_supported;

#define NUTAG_SUPPORTED_REF(x) nutag_supported_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_supported_ref;

/** Allow an event or events.
 *
 * This tag is used to add a new event to the already existing set of
 * allowed events. If you want to ignore the existing set of allowed events,
 * set the allowed event set with SIPTAG_ALLOW_EVENTS_STR() or
 * SIPTAG_ALLOW_EVENTS().
 *
 * The set of allowed methods is added to the @AllowEvents header in the
 * response to the SUBSCRIBE or PUBLISH requests. For incoming SUBSCRIBE or
 * PUBLISH request, an error response <i>489 Bad Event</i> is automatically
 * returned if the incoming method is not included in the set.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *    Valid event name, or comma-separated list of them.
 *
 * @sa @AllowEvents, @RFC3265, @RFC3903, #nua_i_subscribe, #nua_i_publish,
 * nua_subscribe(), nua_publish(), SIPTAG_ALLOW_EVENTS(),
 * SIPTAG_ALLOW_EVENTS_STR()
 *
 * @NEW_1_12_4.
 *
 * Corresponding tag taking reference parameter is NUTAG_ALLOW_EVENTS_REF()
 */
#define NUTAG_ALLOW_EVENTS(x)     nutag_allow_events, tag_str_v(x)
SOFIAPUBVAR tag_typedef_t nutag_allow_events;

#define NUTAG_ALLOW_EVENTS_REF(x) nutag_allow_events_ref, tag_str_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_allow_events_ref;

/** Call state
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 * - #nua_callstate_init - Initial state
 * - #nua_callstate_authenticating - 401/407 received
 * - #nua_callstate_calling - INVITE sent
 * - #nua_callstate_proceeding - 18X received
 * - #nua_callstate_completing   - 2XX received
 * - #nua_callstate_received - INVITE received (and 100 Trying sent)
 * - #nua_callstate_early       - 18X sent
 * - #nua_callstate_completed   - 2XX sent
 * - #nua_callstate_ready       - 2XX and ACK received/sent
 * - #nua_callstate_terminating - BYE sent
 * - #nua_callstate_terminated  - BYE complete
 *
 * Corresponding tag taking reference parameter is NUTAG_CALLSTATE_REF()
 */
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

/**Subscription state.
 *
 * @par Used with
 *    #nua_notify() \n
 *    #nua_r_subscribe \n
 *    #nua_i_notify \n
 *    #nua_i_subscribe \n
 *    #nua_r_notify \n
 *    nua_notify() \n
 *    nua_respond() to SUBSCRIBE
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *   - #nua_substate_embryonic (0)
 *   - #nua_substate_pending (1)
 *   - #nua_substate_active (2)
 *   - #nua_substate_terminated (3)
 *
 * Note that the @SubscriptionState or @Expires headers specified by
 * application overrides the subscription state specified by
 * NUTAG_SUBSTATE(). Application can terminate subscription by including
 * NUTAG_SUBSTATE(nua_substate_terminated), @SubscriptionState with value
 * "terminated" or @Expires header with value 0 in the NOTIFY request sent
 * by nua_notify().
 *
 * @sa @RFC3265, @SubscriptionState, SIPTAG_SUBSCRIPTION_STATE(),
 * SIPTAG_SUBSCRIPTION_STATE_STR(), #nua_r_subscribe, #nua_i_subscribe,
 * #nua_i_refer, #nua_r_notify, #nua_i_notify.
 *
 * Corresponding tag taking reference parameter is NUTAG_SUBSTATE_REF()
 */
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

/**Send unsolicited NOTIFY request.
 *
 * Some applications may require sending unsolicited NOTIFY requests, that
 * is, NOTIFY without SUBSCRIBE or REFER request sent by event watcher. 
 * However, sending NOTIFY request requires an existing dialog usage by
 * default. If NUTAG_NEWSUB(1) is included in the nua_notify() the usage
 * is create the usage by itself.
 *
 * If you want to create a subscription that does not terminate immediately
 * include SIPTAG_SUBSCRIPTION_STATE_STR() with an "expires" parameter in
 * the argument list, too.
 *
 * @par Used with
 *    nua_notify()
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *   - 0 - false (default) - do not create new subscription 
 *         but reject NOTIFY with 481 locally \n
 *   - 1 - true - create a subscription if it does not exist \n
 *
 * Corresponding tag taking reference parameter is NUTAG_NEWSUB().
 *
 * @since @NEW_1_12_5.
 */
#define NUTAG_NEWSUB(x)   nutag_newsub, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_newsub;

#define NUTAG_NEWSUB_REF(x) nutag_newsub_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_newsub_ref;


/**Default lifetime for implicit subscriptions created by REFER.
 *
 * Default expiration time in seconds for implicit subscriptions created by
 * REFER.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_set_hparams() \n
 *    nua_get_hparams() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    @c 0  disable \n
 *    @c >0 interval in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_EXPIRES()
 */
#define NUTAG_REFER_EXPIRES(x)  nutag_refer_expires, tag_uint_v((x))
SOFIAPUBVAR tag_typedef_t nutag_refer_expires;

#define NUTAG_REFER_EXPIRES_REF(x) nutag_refer_expires_ref, tag_uint_vr((&(x)))
SOFIAPUBVAR tag_typedef_t nutag_refer_expires_ref;

/**Always use id parameter with refer event.
 *
 * When an incoming REFER creates an implicit subscription, the event header
 * in the NOTIFY request may have an id parameter. The id parameter can be
 * either always included (default behavior), or the parameter can be used
 * only for the second and subsequent REFER requests received in a given
 * dialog.
 *
 * Note that once the subscription is created, the event header should not
 * be modified. Therefore this tag has no effect on already established
 * subscriptions, and its use makes sense largely on nua_set_params() only.
 *
 * @par Used with
 *    nua_set_params() (nua_set_hparams(), nua_invite(), nua_respond(),
 *    nua_update()).
 *
 * @par Parameter type
 *    int (boolean)
 *
 * @par Values
 *   0 (false, do not use id with subscription created with first REFER request) \n
 *   1 (true, use id with all subscriptions created with REFER request) \n
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_WITH_ID_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_REFER_WITH_ID(x)   nutag_refer_with_id, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_refer_with_id;

#define NUTAG_REFER_WITH_ID_REF(x) nutag_refer_with_id_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_refer_with_id_ref;

/**Add media tags from our offer to Accept-Contact headers.
 *
 * Automatically generate @AcceptContact headers for caller
 * preference processing according to our the media capabilities in @a soa.
 *
 * @par Used with
 *    nua_invite()  \n
 *    nua_update()  \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   Do not add media tags \n
 *    @c !=0 Add media tags
 *
 * Corresponding tag taking reference parameter is NUTAG_MEDIA_FEATURES_REF()
 *
 * @sa nua_invite(), SOATAG_USER_SDP(), SIPTAG_ACCEPT_CONTACT(),
 *     NUTAG_CALLEE_CAPS()
 */
#define NUTAG_MEDIA_FEATURES(x) nutag_media_features, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_media_features;

#define NUTAG_MEDIA_FEATURES_REF(x) \
          nutag_media_features_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_media_features_ref;

/** Add methods and media tags to Contact headers. */
#define NUTAG_CALLEE_CAPS(x) nutag_callee_caps, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_callee_caps;

#define NUTAG_CALLEE_CAPS_REF(x) \
          nutag_callee_caps_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_callee_caps_ref;

/** If true, add "path" to Supported in REGISTER.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc3327.txt">RFC 3327</a>,
 * <i>"SIP Extension Header Field for Registering Non-Adjacent Contacts"</i>,
 * D. Willis, B. Hoeneisen,
 * December 2002.
 */
#define NUTAG_PATH_ENABLE(x)   nutag_path_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_path_enable;

#define NUTAG_PATH_ENABLE_REF(x) nutag_path_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_path_enable_ref;

/** Use route from Service-Route header in response to REGISTER.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc3327.txt">RFC 3327</a>,
 * <i>"SIP Extension Header Field for Registering Non-Adjacent Contacts"</i>,
 * D. Willis, B. Hoeneisen,
 * December 2002.
 */
#define NUTAG_SERVICE_ROUTE_ENABLE(x) nutag_service_route_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_service_route_enable;

#define NUTAG_SERVICE_ROUTE_ENABLE_REF(x) \
          nutag_service_route_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_service_route_enable_ref;

/** Enable built-in media session handling
 *
 * The built-in media session object @soa takes care of most details
 * of offer-answer negotiation. 
 *
 * @par Used with
 *    nua_create()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    @c 0   False \n
 *    @c !=0 True
 *
 * Corresponding tag taking reference parameter is NUTAG_MEDIA_ENABLE_REF()
*/
#define NUTAG_MEDIA_ENABLE(x) nutag_media_enable, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_media_enable;

#define NUTAG_MEDIA_ENABLE_REF(x) \
          nutag_media_enable_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_media_enable_ref;

/** Indicate that SDP offer has been received.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    boolean
 *
 * Corresponding tag taking reference parameter is NUTAG_OFFER_RECV_REF()
 */
#define NUTAG_OFFER_RECV(x) nutag_offer_recv, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_offer_recv;

#define NUTAG_OFFER_RECV_REF(x) nutag_offer_recv_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_offer_recv_ref;

/** Indicate that SDP answer has been received.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    boolean
 *
 * Corresponding tag taking reference parameter is NUTAG_ANSWER_RECV_REF()
 */
#define NUTAG_ANSWER_RECV(x) nutag_answer_recv, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_answer_recv;

#define NUTAG_ANSWER_RECV_REF(x) nutag_answer_recv_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_answer_recv_ref;

/** Indicate that SDP offer has been sent.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    boolean
 *
 * Corresponding tag taking reference parameter is NUTAG_OFFER_SENT_REF()
 */
#define NUTAG_OFFER_SENT(x) nutag_offer_sent, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_offer_sent;

#define NUTAG_OFFER_SENT_REF(x) nutag_offer_sent_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_offer_sent_ref;

/** Indicate that SDP answer has been sent.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * Corresponding tag taking reference parameter is NUTAG_ANSWER_SENT_REF()
 */
#define NUTAG_ANSWER_SENT(x) nutag_answer_sent, tag_bool_v(x)
SOFIAPUBVAR tag_typedef_t nutag_answer_sent;

#define NUTAG_ANSWER_SENT_REF(x) nutag_answer_sent_ref, tag_bool_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_answer_sent_ref;

/**Enable detection of local IP address updates.
 *
 * @par Used with
 *    nua_create() \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (enum nua_nw_detector_e aka #nua_nw_detector_t)
 *
 * @sa #nua_i_network_changed, #nua_nw_detector_t
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_DETECT_NETWORK_UPDATES_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
#define NUTAG_DETECT_NETWORK_UPDATES(x) \
          nutag_detect_network_updates, tag_int_v(x)
SOFIAPUBVAR tag_typedef_t nutag_detect_network_updates;

#define NUTAG_DETECT_NETWORK_UPDATES_REF(x) \
          nutag_detect_network_updates_ref, tag_int_vr(&(x))
SOFIAPUBVAR tag_typedef_t nutag_detect_network_updates_ref;

/* Pass nua handle as tagged argument */
#if SU_HAVE_INLINE
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
