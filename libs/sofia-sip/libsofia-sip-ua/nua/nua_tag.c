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

/**@CFILE nua_tag.c  Tags and tag lists for NUA
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Wed Feb 21 10:13:29 2001 ppessi
 */

#include "config.h"

#define TAG_NAMESPACE "nua"

#include "sofia-sip/nua_tag.h"

#include <sofia-sip/msg_header.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/url_tag_class.h>
#include <sofia-sip/sip_tag_class.h>
#include <sofia-sip/sip_hclasses.h>

/** @page nua_api_overview NUA API Overview
 *
 * This page shortly overviews the NUA API: different functions, tags, and
 * where and how they affect the working of NUA engine.
 *
 * The application and the NUA engine can pass various parameters between
 * them using tagged arguments. Tagged arguments can be used like named
 * arguments in higher-lever language.
 *
 * @par NUA Agent
 *
 * The NUA agent object is created with nua_create(). The nua_create() also
 * creates the transports and binds the transport sockets used by the SIP
 * stack.
 *
 * The special tags controlling the transports are
 * - NUTAG_URL(), NUTAG_SIPS_URL(),  NUTAG_CERTIFICATE_DIR(), NUTAG_SIP_PARSER()
 *
 * See nta_agent_add_tport() for discussion about magic URIs used to
 * initialize transports.
 *
 * The agent-wide parameter can be later modified or obtained with
 * nua_set_params() and nua_get_params(), respectively.
 *
 * The #su_root_t mainloop integration uses
 * - su_root_create(), su_root_threading(),
 *   su_root_poll(), su_root_run(), su_root_break()
 *
 * @par NUA Handles
 * - nua_handle(), nua_get_hparams(), nua_set_hparams()
 * - nua_handle_home(), nua_handle_has_invite(), nua_handle_has_subscribe(),
 *   nua_handle_has_register(), nua_handle_has_active_call(),
 *   nua_handle_has_call_on_hold(), nua_handle_has_events(),
 *   nua_handle_has_registrations(), nua_handle_remote(), and
 *   nua_handle_local().
 * - Settings:
 *   See nua_set_hparams(). There are a few "sticky" headers that are used
 *   on subsequent requests if given on any handle-specific call:
 *   - @Contact, @UserAgent, @Supported, @Allow, @Organization
 *
 * @par Client Generating SIP Requests
 * - nua_register(), nua_unregister(), nua_invite(), nua_cancel(),
 *   nua_ack(), nua_bye(), nua_options(), nua_refer(), nua_publish(),
 *   nua_unpublish(), nua_prack(), nua_info(), nua_update(), nua_message(),
 *   nua_subscribe(), nua_unsubscribe(), nua_notify(), nua_method()
 * - NUTAG_URL()
 *  Settings:
 * - NUTAG_RETRY_COUNT(), NUTAG_PROXY(),
 *   NUTAG_INITIAL_ROUTE() and NUTAG_INITIAL_ROUTE_STR()
 * - NUTAG_ALLOW(), SIPTAG_ALLOW(), and SIPTAG_ALLOW_STR()
 * - NUTAG_SUPPORTED(), SIPTAG_SUPPORTED(), and SIPTAG_SUPPORTED_STR()
 * - NUTAG_USER_AGENT(), SIPTAG_USER_AGENT() and SIPTAG_USER_AGENT_STR()
 * - SIPTAG_ORGANIZATION() and SIPTAG_ORGANIZATION_STR()
 *
 * @par Client Authenticating Requests
 * - nua_authenticate(), #nua_r_authenticate
 * - NUTAG_AUTH(), NUTAG_AUTH_CACHE()
 *
 * @par Server Processing Received SIP Requests
 * - nua_respond(), NUTAG_WITH_THIS(), NUTAG_WITH_SAVED(), NUTAG_WITH()
 * - #nua_i_invite, #nua_i_cancel, #nua_i_ack, #nua_i_bye,
 *   #nua_i_options, #nua_i_refer, #nua_i_publish, #nua_i_prack,
 *   #nua_i_info, #nua_i_update, #nua_i_message, #nua_i_subscribe,
 *   #nua_i_notify, #nua_i_method, #nua_i_register
 *  Settings:
 * - NUTAG_APPL_METHOD(), NUTAG_PROXY()
 * - NUTAG_ALLOW(), SIPTAG_ALLOW(), and SIPTAG_ALLOW_STR()
 * - NUTAG_SUPPORTED(), SIPTAG_SUPPORTED(), and SIPTAG_SUPPORTED_STR()
 *
 * @par Registrations and Contact Header Generation
 * - nua_register(), #nua_r_register(), #nua_i_outbound,
 *   nua_unregister(), and #nua_r_unregister
 * Settings:
 * - NUTAG_CALLEE_CAPS()
 * - NUTAG_DETECT_NETWORK_UPDATES()
 * - NUTAG_INSTANCE()
 * - NUTAG_KEEPALIVE()
 * - NUTAG_KEEPALIVE_STREAM()
 * - NUTAG_M_DISPLAY()
 * - NUTAG_M_FEATURES()
 * - NUTAG_M_PARAMS()
 * - NUTAG_M_USERNAME()
 * - NUTAG_OUTBOUND()
 * - NUTAG_PATH_ENABLE()
 * - NUTAG_SERVICE_ROUTE_ENABLE()
 * Specifications:
 * - @RFC3261 section 10, @RFC3327, @RFC3608, @RFC3680, @RFC3840,
 *   draft-ietf-sip-outbound, draft-ietf-sip-gruu-14
 *
 * @par INVITE Sessions and Call Model
 * - nua_invite(), #nua_r_invite, #nua_i_invite
 * - nua_handle_has_active_call(), nua_handle_has_call_on_hold(),
 *   nua_handle_has_invite()
 * - nua_cancel(), #nua_r_cancel, #nua_i_cancel
 * - nua_ack(), #nua_i_ack
 * - nua_bye(), #nua_r_bye, #nua_i_bye
 * - #nua_i_state, NUTAG_CALLSTATE(),
 *   NUTAG_OFFER_SENT(), NUTAG_OFFER_RECV(), NUTAG_ANSWER_RECV(), and
 *   NUTAG_ANSWER_SENT(), SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR(),
 *   SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR()
 *  Settings:
 * - NUTAG_AUTOACK(), NUTAG_AUTOALERT(), NUTAG_AUTOANSWER(),
 *   NUTAG_ENABLEINVITE(), NUTAG_INVITE_TIMER(), NUTAG_MEDIA_ENABLE(),
 *   SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(), SOATAG_CAPS_SDP(),
 *   SOATAG_CAPS_SDP_STR()
 * Specifications:
 * - @RFC3261, @RFC3264
 *
 * @par In-Session Information requests
 * - nua_info() #nua_r_info, #nua_i_info
 * Settings:
 * - NUTAG_ALLOW("INFO"), NUTAG_APPL_METHOD("INFO")
 *
 * @par SDP Processing
 * - #nua_i_state, SOATAG_ACTIVE_AUDIO(), SOATAG_ACTIVE_VIDEO(),
 *   SOATAG_ACTIVE_IMAGE(), SOATAG_ACTIVE_CHAT(),
 *   SOATAG_REMOTE_SDP(), SOATAG_REMOTE_SDP_STR(),
 *   SOATAG_LOCAL_SDP(), SOATAG_LOCAL_SDP_STR()
 * Settings:
 * - NUTAG_MEDIA_ENABLE(), NUTAG_SOA_NAME(), NUTAG_EARLY_ANSWER(),
 *   SOATAG_USER_SDP(), SOATAG_USER_SDP_STR(), SOATAG_CAPS_SDP(),
 *   SOATAG_CAPS_SDP_STR()
 * Specifications:
 * - @RFC3264
 *
 * @par Call Model Extensions ("100rel" and "precondition")
 * Early
 * - nua_prack(), #nua_r_prack, #nua_i_prack
 * - nua_update() #nua_r_update, #nua_i_update
 * Settings:
 * - NUTAG_EARLY_MEDIA(), NUTAG_ONLY183_100REL()
 * - "100rel" or "precondition" in NUTAG_SUPPORTED()/SIPTAG_SUPPORTED()
 * Specifications:
 * - @RFC3262, @RFC3311, @RFC3312
 *
 * @par SIP Session Timers ("timer")
 * Periodic refresh of SIP Session initiated with INVITE with re-INVITE or
 * UPDATE requests.
 * Settings:
 * - NUTAG_MIN_SE(), NUTAG_SESSION_REFRESHER(),
 *   NUTAG_SESSION_TIMER(), NUTAG_UPDATE_REFRESH(),
 *   NUTAG_REFRESH_WITHOUT_SDP(),
 * - "timer" in NUTAG_SUPPORTED()/SIPTAG_SUPPORTED()
 * Specifications:
 * - @RFC4028
 *
 * @par Caller Preferences and Callee Caps
 * - Caller preferences in @AcceptContact header in a INVITE requests
 * - Callee caps contained in @Contact header in a REGISTER request
 * Settings:
 * - NUTAG_CALLEE_CAPS(), NUTAG_MEDIA_FEATURES(),
 *   NUTAG_M_FEATURES()
 * Specifications:
 * - @RFC3840, @RFC3841
 *
 * @par Instant Messaging
 * - nua_message(), #nua_r_message, #nua_i_message
 * Settings:
 * - NUTAG_APPL_METHOD("MESSAGE"),
 *   NUTAG_ENABLEMESSAGE(), NUTAG_ENABLEMESSENGER()
 * Specifications:
 * - @RFC3428
 *
 * @par Call Transfer
 * - nua_refer(), #nua_r_refer, #nua_i_notify, SIPTAG_EVENT(),
 *   @ReferTo, SIPTAG_REFER_TO(), @ReferredBy, SIPTAG_REFERRED_BY(),
 *   nua_handle_make_replaces(), @Replaces, SIPTAG_REPLACES(),
 *   @ReferSub, SIPTAG_REFER_SUB()
 * - #nua_i_refer, nua_notify(), #nua_r_notify,
 *   nua_handle_by_replaces()
 * - nua_invite() with NUTAG_NOTIFY_REFER() and NUTAG_REFER_EVENT()
 * Settings:
 * - NUTAG_REFER_EXPIRES(), NUTAG_REFER_WITH_ID()
 * Specifications:
 * - @RFC3515 (@ReferTo), @RFC3892 (@ReferredBy), @RFC3891 (@Replaces),
 *   @RFC4488, @ReferSub
 *
 * @par Internal SIP Event Server
 * - nua_notifier(), #nua_r_notifier, #nua_i_subscription,
 *   nua_authorize(), #nua_r_authorize, nua_terminate(), #nua_r_terminate
 * - SIPTAG_EVENT(), SIPTAG_CONTENT_TYPE(), SIPTAG_PAYLOAD(),
 *   NUTAG_SUBSTATE()
 * @par Settings
 * - NUTAG_ALLOW_EVENTS(), SIPTAG_ALLOW_EVENTS(), and
 *                          SIPTAG_ALLOW_EVENTS_STR()
 * - NUTAG_MAX_SUBSCRIPTIONS()
 * - NUTAG_SUBSTATE(), NUTAG_SUB_EXPIRES()
 * @par Specifications
 * - @RFC3265
 *
 * @par SIP Event Subscriber
 * - nua_subscribe(), #nua_r_subscribe, #nua_i_notify, NUTAG_SUBSTATE(),
 *   SIPTAG_EVENT(), SIPTAG_EXPIRES()
 * - nua_unsubscribe(), #nua_r_unsubscribe()
 * @par Specifications
 * - @RFC3265
 *
 * @par SIP Event Notifier
 * - #nua_i_subscribe(), nua_notify(), #nua_r_notify,
 *   NUTAG_SUBSTATE(), NUTAG_SUB_EXPIRES(), SIPTAG_EVENT()
 * Settings:
 * - NUTAG_SUB_EXPIRES()
 * - NUTAG_ALLOW_EVENTS(), SIPTAG_ALLOW_EVENTS(), and
 *                          SIPTAG_ALLOW_EVENTS_STR()
 * - NUTAG_ALLOW("SUBSCRIBE"), NUTAG_APPL_METHOD("SUBSCRIBE")
 * @par Specifications
 * - @RFC3265
 *
 * @par SIP Event Publisher
 * - nua_publish(), #nua_r_publish(), nua_unpublish(), nua_r_unpublish()
 * - @SIPETag, SIPTAG_ETAG(), @SIPIfMatch, SIPTAG_IF_MATCH()
 * @par Specifications
 * - @RFC3903
 *
 * @par SIP Event State Compositor (PUBLISH Server)
 * - #nua_i_publish, @SIPETag, @SIPIfMatch
 * @par Settings
 * - NUTAG_ALLOW("PUBLISH"), NUTAG_APPL_METHOD("PUBLISH")
 * @par Specifications
 * - @RFC3903
 *
 * @par Non-Standard Extension Methods
 * - nua_method(), NUTAG_METHOD(), #nua_r_method, NUTAG_DIALOG()
 * - #nua_i_method, nua_respond()
 * Settings:
 * - NUTAG_ALLOW(x), NUTAG_APPL_METHOD(x)
 *
 * @par Server Shutdown
 * - nua_shutdown(), NUTAG_SHUTDOWN_EVENTS(), nua_destroy().
 */

/* @par S/MIME
 * - NUTAG_SMIME_ENABLE()
 * - NUTAG_SMIME_KEY_ENCRYPTION()
 * - NUTAG_SMIME_MESSAGE_DIGEST()
 * - NUTAG_SMIME_MESSAGE_ENCRYPTION()
 * - NUTAG_SMIME_OPT()
 * - NUTAG_SMIME_PROTECTION_MODE()
 * - NUTAG_SMIME_SIGNATURE()
 */

tag_typedef_t nutag_any = NSTAG_TYPEDEF(*);

/**@def NUTAG_URL()
 *
 * URL address from application to NUA
 *
 * @par Used with
 *    any function that create SIP request or nua_handle() \n
 *    nua_create() \n
 *    nua_set_params() \n
 *    nua_get_params() \n
 *
 * @par Parameter type
 *    char const *   or   url_t *  or url_string_t *
 *
 * @par Values
 *    #url_string_t, which is either a pointer to #url_t or NULL terminated
 *    character string representing URL
 *
 * For normal nua calls, this tag is used as request target, which is usually
 * stored as request-URI.
 *
 * It is used to set stack's own address with nua_create(), nua_set_params()
 * and nua_get_params(). It can be specified multiple times when used with
 * nua_create().
 *
 * @sa SIPTAG_TO()
 *
 * Corresponding tag taking reference parameter is NUTAG_URL_REF()
 */
tag_typedef_t nutag_url = URLTAG_TYPEDEF(url);


/**@def NUTAG_METHOD(x)
 *
 * Extension method name.
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
tag_typedef_t nutag_method = STRTAG_TYPEDEF(method);

/**@def NUTAG_METHOD_REF(x)
 * Reference tag for NUTAG_METHOD().
 */


/*#@def NUTAG_UICC(x)
 *
 * Intentionally undocumented.
 */
tag_typedef_t nutag_uicc = STRTAG_TYPEDEF(uicc);

/*#@def NUTAG_UICC_REF(x)
 * Reference tag for NUTAG_UICC().
 */


/**@def NUTAG_MEDIA_FEATURES()
 *
 * Add media tags from our offer to Accept-Contact headers.
 *
 * Automatically generate @AcceptContact headers for caller
 * preference processing according to the media capabilities in @a soa.
 *
 * @par Used with
 * - nua_create(), nua_set_params(), nua_get_params()
 * - nua_handle(), nua_set_hparams(), nua_get_hparams()
 * - nua_invite()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - Do not add @AcceptContact
 *    - 1 (true) - Add @AcceptContact with media tags
 *
 * Corresponding tag taking reference parameter is NUTAG_MEDIA_FEATURES_REF()
 *
 * @sa nua_invite(), @AcceptContact, @RFC3841, @RFC3840, SOATAG_USER_SDP(),
 * SIPTAG_ACCEPT_CONTACT(), NUTAG_CALLEE_CAPS()
 */
tag_typedef_t nutag_media_features = BOOLTAG_TYPEDEF(media_features);

/**@def NUTAG_MEDIA_FEATURES_REF(x)
 * Reference tag for NUTAG_MEDIA_FEATURES().
 */


/**@def NUTAG_CALLEE_CAPS(x)
 *
 * Add methods parameter and media feature parameter to the @Contact headers
 * generated for REGISTER request.
 *
 * @par Used with
 * - nua_create(), nua_set_params(), nua_get_params()
 * - nua_handle(), nua_set_hparams(), nua_get_hparams()
 * - nua_register()
 *
 * @par Parameter type
 *    int
 *
 * @par Values
 *    - 0 (false) - Do not include methods and media feature parameters
 *    - 1 (true) - Include media tags in @Contact
 *
 * Corresponding tag taking reference parameter is NUTAG_MEDIA_FEATURES_REF().
 *
 * @sa nua_register(), @Contact, NUTAG_M_FEATURES(), @RFC3840, @RFC3841,
 * SOATAG_USER_SDP(), NUTAG_MEDIA_FEATURES()
 */
tag_typedef_t nutag_callee_caps = BOOLTAG_TYPEDEF(callee_caps);

/**@def NUTAG_CALLEE_CAPS_REF(x)
 * Reference tag for NUTAG_CALLEE_CAPS().
 */


/**@def NUTAG_EARLY_MEDIA(x)
 *
 * Establish early media session using 100rel, 183 responses and PRACK.
 *
 * @par Used with
 * - nua_create(), nua_set_params(), nua_get_params()
 * - nua_handle(), nua_set_hparams(), nua_get_hparams()
 * - nua_invite(), nua_respond()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) -  do not try to use early media
 *    - 1 (true) - try to use early media
 *
 * @sa NUTAG_EARLY_ANSWER()
 *
 * Corresponding tag taking reference parameter is NUTAG_EARLY_MEDIA_REF().
 */
tag_typedef_t nutag_early_media = BOOLTAG_TYPEDEF(early_media);

/**@def NUTAG_EARLY_MEDIA_REF(x)
 * Reference tag for NUTAG_EARLY_MEDIA().
 */


/**@def NUTAG_ONLY183_100REL(x)
 *
 * Require 100rel extension and PRACK only with 183 response.
 *
 * When NUTAG_EARLY_MEDIA() is set, and if this parameter is set, stack
 * includes feature tag "100rel" in the @Require header only with 183:
 * otherwise, all 1XX responses (except <i>100 Trying</i>) require 100rel.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_handle() \n
 *    nua_set_hparams() \n
 *    nua_get_hparams() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - include 100rel in all preliminary responses
 *    - 1 (true) - include 100rel only in 183 responses
 *
 * @note
 * This tag takes only effect when NUTAG_EARLY_MEDIA(1) has been used, too.
 *
 * Corresponding tag taking reference parameter is NUTAG_ONLY183_100REL_REF().
 *
 * @sa
 */
tag_typedef_t nutag_only183_100rel = BOOLTAG_TYPEDEF(only183_100rel);

/**@def NUTAG_ONLY183_100REL_REF(x)
 * Reference tag for NUTAG_ONLY183_100REL().
 */


/**@def NUTAG_EARLY_ANSWER(x)
 *
 * Establish early media session by including SDP answer in 1XX response.
 *
 * @par Used with
 *    nua_respond(), nua_set_params(), nua_set_hparams()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - do not include SDP in non-100rel 1XX responses
 *    - 1 (true) - try to include SDP in preliminary responses
 *
 * Corresponding tag taking reference parameter is NUTAG_EARLY_ANSWER_REF().
 *
 * @note Requires that @soa is enabled with NUTAG_MEDIA_ENABLE(1).
 *
 * @sa NUTAG_EARLY_MEDIA(), NUTAG_AUTOALERT(), NUTAG_MEDIA_ENABLE()
 *
 * @since New in @VERSION_1_12_2.
 */
tag_typedef_t nutag_early_answer = BOOLTAG_TYPEDEF(early_answer);

/**@def NUTAG_EARLY_ANSWER_REF(x)
 * Reference tag for NUTAG_EARLY_ANSWER().
 */


/**@def NUTAG_INCLUDE_EXTRA_SDP(x)
 *
 * Include an extra copy of SDP answer in the response.
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - do not include extra SDP on 200 OK
 *    - 1 (true) - include SDP in 200 OK even if it has been sent
 *                 a 100rel response, too
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
tag_typedef_t nutag_include_extra_sdp = BOOLTAG_TYPEDEF(include_extra_sdp);

/**@def NUTAG_INCLUDE_EXTRA_SDP_REF(x)
 * Reference tag for NUTAG_INCLUDE_EXTRA_SDP().
 */


/**@def NUTAG_MEDIA_ENABLE()
 *
 * Enable built-in media session handling
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
 *    - 0 (false) - do not use soa
 *    - 1 (true) - use soa with SDP O/A
 *
 * Corresponding tag taking reference parameter is NUTAG_MEDIA_ENABLE_REF()
 */
tag_typedef_t nutag_media_enable = BOOLTAG_TYPEDEF(media_enable);

/**@def NUTAG_MEDIA_ENABLE_REF(x)
 * Reference tag for NUTAG_MEDIA_ENABLE().
 */



/**@def NUTAG_SOA_NAME(x)
 *
 * Name for SDP Offer-Answer session object.
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
 * Corresponding tag taking reference parameter is NUTAG_SOA_NAME_REF().
 */
tag_typedef_t nutag_soa_name = STRTAG_TYPEDEF(soa_name);

/**@def NUTAG_SOA_NAME_REF(x)
 * Reference tag for NUTAG_SOA_NAME().
 */


/**@def NUTAG_RETRY_COUNT(x)
 *
 * Set request retry count.
 *
 * Retry count determines how many times stack will automatically retry
 * after an recoverable error response, like 302, 401 or 407.
 *
 * Note that the first request does not count as retry.
 *
 * @par Used with
 *    nua_create(), nua_set_params(), nua_handle(), nua_set_hparams(),
 *    nua_get_params(), nua_get_hparams(),
 *    nua_register(), nua_unregister(),
 *    nua_options(), nua_invite(), nua_ack(), nua_cancel(), nua_bye(),
 *    nua_prack(), nua_update(), nua_info(),
 *    nua_message(), nua_publish(), nua_unpublish(), nua_notifier(),
 *    nua_subscribe(), nua_unsubscribe(), nua_notify(), nua_refer(),
 *    nua_method(), nua_respond()
 *    nua_authenticate().
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 * - 0 - Never retry automatically
 * - Otherwise, number of extra transactions initiated after initial
 *   transaction failed with recoverable error response
 *
 * @NEW_1_12_4.
 *
 * Corresponding tag taking reference parameter is NUTAG_RETRY_COUNT_REF().
 */
tag_typedef_t nutag_retry_count = UINTTAG_TYPEDEF(retry_count);

/**@def NUTAG_RETRY_COUNT_REF(x)
 *
 * Reference tag for NUTAG_RETRY_COUNT().
 */


/**@def NUTAG_MAX_SUBSCRIPTIONS(x)
 *
 * Set maximum number of simultaneous subscribers per single event server.
 *
 * Determines how many subscribers can simultaneously subscribe to a single
 * event.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - 0 (zero) - do not allow any subscriptions
 *
 * @sa nua_notifier(), nua_authorize()
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_MAX_SUBSCRIPTIONS_REF().
 */
tag_typedef_t nutag_max_subscriptions = UINTTAG_TYPEDEF(max_subscriptions);

/**@def NUTAG_MAX_SUBSCRIPTIONS_REF(x)
 * Reference tag for NUTAG_MAX_SUBSCRIPTIONS().
 */


/**@def NUTAG_CALLSTATE()
 *
 * Call state
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
 * Corresponding tag taking reference parameter is NUTAG_CALLSTATE_REF().
 */
tag_typedef_t nutag_callstate = INTTAG_TYPEDEF(callstate);

/**@def NUTAG_CALLSTATE_REF(x)
 * Reference tag for NUTAG_CALLSTATE().
 */


/**@def NUTAG_OFFER_RECV()
 *
 * Indicate that SDP offer has been received.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * Corresponding tag taking reference parameter is NUTAG_OFFER_RECV_REF().
 */
tag_typedef_t nutag_offer_recv = BOOLTAG_TYPEDEF(offer_recv);

/**@def NUTAG_OFFER_RECV_REF(x)
 * Reference tag for NUTAG_OFFER_RECV().
 */


/**@def NUTAG_ANSWER_RECV()
 *
 * Indicate that SDP answer has been received.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * Corresponding tag taking reference parameter is NUTAG_ANSWER_RECV_REF().
 */
tag_typedef_t nutag_answer_recv = BOOLTAG_TYPEDEF(answer_recv);

/**@def NUTAG_ANSWER_RECV_REF(x)
 * Reference tag for NUTAG_ANSWER_RECV().
 */


/**@def NUTAG_OFFER_SENT()
 *
 * Indicate that SDP offer has been sent.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * Corresponding tag taking reference parameter is NUTAG_OFFER_SENT_REF().
 */
tag_typedef_t nutag_offer_sent = BOOLTAG_TYPEDEF(offer_sent);

/**@def NUTAG_OFFER_SENT_REF(x)
 * Reference tag for NUTAG_OFFER_SENT().
 */


/**@def NUTAG_ANSWER_SENT()
 *
 * Indicate that SDP answer has been sent.
 *
 * @par Used with
 *    #nua_i_state
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * Corresponding tag taking reference parameter is NUTAG_ANSWER_SENT_REF().
 */
tag_typedef_t nutag_answer_sent = BOOLTAG_TYPEDEF(answer_sent);

/**@def NUTAG_ANSWER_SENT_REF(x)
 * Reference tag for NUTAG_ANSWER_SENT().
 */


/**@def NUTAG_SUBSTATE()
 *
 * Subscription state.
 *
 * @par Used with
 * - with nua_create(), nua_set_params(), nua_get_params(),
 *   nua_handle(), nua_set_hparams(), nua_get_hparams(), and
 *   nua_notifier() to change the default subscription state returned by
 *   the internal event server
 * - with nua_notify() and nua_respond() to SUBSCRIBE to determine the
 *   subscription state (if application include @SubscriptionState
 *   header in the tag list, the NUTAG_SUBSTATE() value is ignored)
 * - with #nua_r_subscribe, #nua_i_notify, #nua_i_subscribe, and #nua_r_notify
 *   to indicate the current subscription state
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
 * application with the nua_notify() or nua_respond() to SUBSCRIBE overrides
 * the subscription state specified by NUTAG_SUBSTATE().
 * Application can terminate subscription by including
 * NUTAG_SUBSTATE(nua_substate_terminated), @SubscriptionState with value
 * "terminated" or @Expires header with value 0 in the NOTIFY request sent
 * by nua_notify().
 *
 * @sa @RFC3265, @SubscriptionState, SIPTAG_SUBSCRIPTION_STATE(),
 * SIPTAG_SUBSCRIPTION_STATE_STR(), nua_notifier(), #nua_r_subscribe,
 * #nua_i_subscribe, #nua_i_refer, #nua_r_notify, #nua_i_notify.
 *
 * Corresponding tag taking reference parameter is NUTAG_SUBSTATE_REF().
 */
tag_typedef_t nutag_substate = INTTAG_TYPEDEF(substate);

/**@def NUTAG_SUBSTATE_REF(x)
 * Reference tag for NUTAG_SUBSTATE().
 */


/**@def NUTAG_SUB_EXPIRES()
 *
 * Default expiration time of subscriptions.
 *
 * @par Used with
 * - with nua_create(), nua_set_params(), nua_get_params(), nua_handle(),
 *   nua_set_hparams(), nua_get_hparams(), nua_respond(), nua_notify(), and
 *   nua_notifier() to change the default expiration time of subscriptions
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *   - default expiration time in seconds
 *
 * Note that the expires parameter in @SubscriptionState or @Expires header
 * in the nua_response() to the SUBSCRIBE overrides the default subscription
 * expiration specified by NUTAG_SUB_EXPIRES().
 *
 * @sa @RFC3265, NUTAG_REFER_EXPIRES(), @Expires, SIPTAG_EXPIRES(),
 * SIPTAG_EXPIRES_STR(), @SubscriptionState, nua_respond(), nua_notifier(),
 * #nua_r_subscribe, #nua_i_subscribe, #nua_r_refer, #nua_r_notify,
 * #nua_i_notify.
 *
 * Corresponding tag taking reference parameter is NUTAG_SUB_EXPIRES_REF().
 *
 * @NEW_1_12_9.
 */
tag_typedef_t nutag_sub_expires = UINTTAG_TYPEDEF(substate);

/**@def NUTAG_SUB_EXPIRES_REF(x)
 * Reference tag for NUTAG_SUB_EXPIRES().
 */


/**@def NUTAG_NEWSUB()
 *
 * Send unsolicited NOTIFY request.
 *
 * Some applications may require sending unsolicited NOTIFY requests, that
 * is, NOTIFY without SUBSCRIBE or REFER request sent by event watcher.
 * However, sending NOTIFY request requires an existing dialog usage by
 * default. If the nua_notify() tags include NUTAG_NEWSUB(1), the usage
 * is created by nua_notify() itself.
 *
 * If you want to create a subscription that does not terminate immediately
 * include SIPTAG_SUBSCRIPTION_STATE()/SIPTAG_SUBSCRIPTION_STATE_STR() with
 * an "expires" parameter in the argument list, too.
 *
 * @par Used with
 *    nua_notify()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *   - 0 - false (default) - do not create new subscription
 *         but reject NOTIFY with 481 locally
 *   - 1 - true - create a subscription if it does not exist
 *
 * Corresponding tag taking reference parameter is NUTAG_NEWSUB_REF().
 *
 * @NEW_1_12_5.
 */
tag_typedef_t nutag_newsub = BOOLTAG_TYPEDEF(newsub);

/**@def NUTAG_NEWSUB_REF(x)
 * Reference tag for NUTAG_NEWSUB().
 */


/**@def NUTAG_INVITE_TIMER(x)
 *
 * Timer for outstanding INVITE in seconds.
 *
 * INVITE will be canceled if no answer is received before timer expires.
 *
 * @par Used with
 *    nua_invite() \n
 *    nua_set_params(), nua_set_hparams(),
 *    nua_get_params(), nua_get_hparams()
 *
 * @par Parameter type
 *    int (enum nua_af)
 *
 * @par Values
 *    - 0  no timer
 *    - >0 timer in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_INVITE_TIMER_REF().
 */
tag_typedef_t nutag_invite_timer = UINTTAG_TYPEDEF(invite_timer);

/**@def NUTAG_INVITE_TIMER_REF(x)
 * Reference tag for NUTAG_INVITE_TIMER().
 */


/**@def NUTAG_SESSION_TIMER(x)
 *
 * Default session timer in seconds.
 *
 * Set default value for session timer in seconds when the session timer
 * extension is used. The tag value is the proposed session expiration time
 * in seconds, the session is refreshed twice during the expiration time.
 *
 * @par Sending INVITE and UPDATE Requests
 *
 * If NUTAG_SESSION_TIMER() is used with non-zero value, the value is used
 * in the @SessionExpires header included in the INVITE or UPDATE requests.
 * The intermediate proxies or the ultimate destination can lower the
 * interval in @SessionExpires header. If the value is too low, they can
 * reject the request with the status code <i>422 Session Timer Too
 * Small</i>. In that case, @b nua increases the value of @SessionExpires
 * header and retries the request automatically.
 *
 * @par Returning a Response to the INVITE and UPDATE Requests
 *
 * The NUTAG_SESSION_TIMER() value is also used when sending the final
 * response to the INVITE or UPDATE requests. If the NUTAG_SESSION_TIMER()
 * value is 0 or the value in the @SessionExpires header of the request is
 * lower than the value in NUTAG_SESSION_TIMER(), the value from the
 * incoming @SessionExpires header is used. However, if the value in
 * @SessionExpires is lower than the minimal acceptable session expiration
 * interval specified with the tag NUTAG_MIN_SE() the request is
 * automatically rejected with <i>422 Session Timer Too Small</i>.
 *
 * @par Refreshes
 *
 * After the initial INVITE request, the SIP session is refreshed at the
 * intervals indicated by the @SessionExpires header returned in the 2XX
 * response. The party indicated with the "refresher" parameter of the
 * @SessionExpires header sends a re-INVITE requests (or an UPDATE
 * request if NUTAG_UPDATE_REFRESH(1) parameter tag has been set).
 *
 * Some SIP user-agents use INVITE without SDP offer to refresh session.
 * By default, NUA sends an offer in 200 OK to such an INVITE and expects
 * an answer back in ACK. If NUTAG_REFRESH_WITHOUT_SDP(1) tag is used,
 * no SDP offer is sent in 200 OK if re-INVITE was received without SDP.
 *
 * @par When to Use NUTAG_SESSION_TIMER()?
 *
 * The session time extension is enabled ("timer" feature tag is included in
 * @Supported header) but not activated by default (no @SessionExpires
 * header is included in the requests or responses by default). Using
 * non-zero value with NUTAG_SESSION_TIMER() or NUTAG_SESSION_REFRESHER()
 * activates it. When the extension is activated, @nua refreshes the call
 * state by sending periodic re-INVITE or UPDATE requests unless the remote
 * end indicated that it will take care of refreshes.
 *
 * The session timer extension is mainly useful for proxies or back-to-back
 * user agents that keep call state. The call state is "soft" meaning that
 * if no call-related SIP messages are processed for certain time the state
 * will be destroyed. An ordinary user-agent can also make use of session
 * timer if it cannot get any activity feedback from RTP or other media.
 *
 * @note The session timer extension is used only if the feature
 * tag "timer" is listed in the @Supported header, set by NUTAG_SUPPORTED(),
 * SIPTAG_SUPPORTED(), or SIPTAG_SUPPORTED_STR() tags.
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
 *    - 0  disable
 *    - >0 interval in seconds
 *
 * Corresponding tag taking reference parameter is NUTAG_SESSION_TIMER_REF().
 *
 * @sa NUTAG_SUPPORTED(), NUTAG_MIN_SE(), NUTAG_SESSION_REFRESHER(),
 * nua_invite(), #nua_r_invite, #nua_i_invite, nua_respond(),
 * nua_update(), #nua_r_update, #nua_i_update,
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @SessionExpires, @MinSE
 */
tag_typedef_t nutag_session_timer = UINTTAG_TYPEDEF(session_timer);

/**@def NUTAG_SESSION_TIMER_REF(x)
 * Reference tag for NUTAG_SESSION_TIMER().
 */


/**@def NUTAG_MIN_SE(x)
 *
 * Minimum acceptable refresh interval for session.
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
 * Corresponding tag taking reference parameter is NUTAG_MIN_SE_REF().
 *
 * @sa NUTAG_SESSION_TIMER(), NUTAG_SESSION_REFRESHER(),
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @MinSE, @SessionExpires
 */
tag_typedef_t nutag_min_se = UINTTAG_TYPEDEF(min_se);

/**@def NUTAG_MIN_SE_REF(x)
 * Reference tag for NUTAG_MIN_SE().
 */


/**@def NUTAG_SESSION_REFRESHER(x)
 *
 * Specify the preferred refresher.
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
 *    - nua_no_refresher (session timers are disabled)
 *    - nua_local_refresher
 *    - nua_remote_refresher
 *    - nua_any_refresher (default)
 *
 * Corresponding tag taking reference parameter is
 * NUTAG_SESSION_REFRESHER_REF().
 *
 * @sa NUTAG_SESSION_TIMER(), NUTAG_MIN_SE_REF(),
 * NUTAG_UPDATE_REFRESH(), @RFC4028, @SessionExpires, @MinSE
 */
tag_typedef_t nutag_session_refresher = INTTAG_TYPEDEF(session_refresher);

/**@def NUTAG_SESSION_REFRESHER_REF(x)
 * Reference tag for NUTAG_SESSION_REFRESHER().
 */




/**@def NUTAG_UPDATE_REFRESH(x)
 *
 * Use UPDATE as refresh method.
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 1 (true, use UPDATE)
 *    - 0 (false, use INVITE)
 *
 * Corresponding tag taking reference parameter is NUTAG_UPDATE_REFRESH_REF().
 *
 * @sa #nua_r_update, NUTAG_SESSION_TIMER(), NUTAG_MIN_SE_REF(),
 * NUTAG_SESSION_REFRESHER(), @RFC4028, @SessionExpires, @MinSE
 */
tag_typedef_t nutag_update_refresh = BOOLTAG_TYPEDEF(update_refresh);

/**@def NUTAG_UPDATE_REFRESH_REF(x)
 * Reference tag for NUTAG_UPDATE_REFRESH().
 */


/**@def NUTAG_REFRESH_WITHOUT_SDP(x)
 *
 * Do not send offer in response if re-INVITE was received without SDP.
 *
 * Some SIP user-agents use INVITE without SDP offer to refresh session.
 * By default, NUA sends an offer in 200 OK to such an INVITE and expects
 * an answer back in ACK.
 *
 * If NUTAG_REFRESH_WITHOUT_SDP(1) tag is used, no SDP offer is sent in 200
 * OK if re-INVITE was received without SDP.
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 1 (true, do not try to send offer in response to re-INVITE)
 *    - 0 (false, always use SDP offer-answer in re-INVITEs)
 *
 * Corresponding tag taking reference parameter is NUTAG_REFRESH_WITHOUT_SDP_REF().
 *
 * @sa #nua_r_update, NUTAG_SESSION_TIMER(), NUTAG_MIN_SE_REF(),
 * NUTAG_SESSION_REFRESHER(), NUTAG_UPDATE_REFRESH(), @RFC4028,
 * @SessionExpires, @MinSE
 *
 * @NEW_1_12_10
 */
tag_typedef_t nutag_refresh_without_sdp = BOOLTAG_TYPEDEF(refresh_without_sdp);

/**@def NUTAG_REFRESH_WITHOUT_SDP_REF(x)
 * Reference tag for NUTAG_REFRESH_WITHOUT_SDP_REF().
 */


/**@def NUTAG_REFER_EXPIRES()
 *
 * Default lifetime for implicit subscriptions created by REFER.
 *
 * Default expiration time in seconds for implicit subscriptions created by
 * REFER.
 *
 * @par Used with
 *    nua_handle(), nua_respond() \n
 *    nua_set_params() or nua_set_hparams() \n
 *    nua_get_params() or nua_get_hparams()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - default interval in seconds
 *
 * @sa NUTAG_SUB_EXPIRES()
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_EXPIRES_REF().
 */
tag_typedef_t nutag_refer_expires = UINTTAG_TYPEDEF(refer_expires);

/**@def NUTAG_REFER_EXPIRES_REF(x)
 * Reference tag for NUTAG_REFER_EXPIRES().
 */


/**@def NUTAG_REFER_WITH_ID()
 *
 * Always use id parameter with refer event.
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *   - 0 (false, do not use id with subscription created with first REFER request)
 *   - 1 (true, use id with all subscriptions created with REFER request)
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_WITH_ID_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
tag_typedef_t nutag_refer_with_id = BOOLTAG_TYPEDEF(refer_with_id);

/**@def NUTAG_REFER_WITH_ID_REF(x)
 * Reference tag for NUTAG_REFER_WITH_ID().
 */

/**@def NUTAG_AUTOALERT(x)
 *
 * Send alerting (180 Ringing) automatically (instead of 100 Trying). If the
 * early media has been enabled with NUTAG_EARLY_MEDIA(1), the stack will
 * send 183, wait for PRACK and then return 180 Ringing.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - no automatic sending of "180 Ringing"
 *    - 1 (true) - "180 Ringing" sent automatically
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTOALERT_REF().
 */
tag_typedef_t nutag_autoalert = BOOLTAG_TYPEDEF(autoAlert);

/**@def NUTAG_AUTOALERT_REF(x)
 * Reference tag for NUTAG_AUTOALERT().
 */


/**@def NUTAG_AUTOANSWER(x)
 *
 * Answer (with 200 Ok) automatically to incoming call.
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams() \n
 *    nua_get_params(), nua_get_hparams() \n
 *    nua_invite() \n
 *    nua_respond()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - No automatic sending of "200 Ok"
 *    - 1 (true) - "200 Ok" sent automatically
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTOANSWER_REF().
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
tag_typedef_t nutag_autoanswer = BOOLTAG_TYPEDEF(autoAnswer);

/**@def NUTAG_AUTOANSWER_REF(x)
 * Reference tag for NUTAG_AUTOANSWER().
 */


/**@def NUTAG_AUTOACK(x)
 *
 * ACK automatically
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - No automatic sending of ACK
 *    - 1 (true) - ACK sent automatically
 *
 * Default value is NUTAG_AUTOACK(1).
 *
 * @par Auto ACK with Re-INVITE requests
 * By default, NUA tries to auto-ACK the final response to re-INVITE used to
 * refresh the session when the media is enabled. Set NUTAG_AUTOACK(0) on
 * the call handle (e.g., include the tag with nua_invite() or
 * nua_respond()) in order to disable the auto ACK with re-INVITE.
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTOACK_REF().
 */
tag_typedef_t nutag_autoack = BOOLTAG_TYPEDEF(autoACK);

/**@def NUTAG_AUTOACK_REF(x)
 * Reference tag for NUTAG_AUTOACK().
 */


/**@def NUTAG_ENABLEINVITE(x)
 *
 * Enable incoming INVITE.
 *
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - Incoming INVITE not enabled. NUA answers 403 Forbidden
 *    - 1 (true) - Incoming INVITE enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEINVITE_REF().
 */
tag_typedef_t nutag_enableinvite = BOOLTAG_TYPEDEF(enableInvite);

/**@def NUTAG_ENABLEINVITE_REF(x)
 * Reference tag for NUTAG_ENABLEINVITE().
 */



/**@def NUTAG_ENABLEMESSAGE(x)
 *
 * Enable incoming MESSAGE
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - Incoming MESSAGE not enabled. NUA answers 403 Forbidden
 *    - 1 (true) - Incoming MESSAGE enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEMESSAGE_REF().
 */
tag_typedef_t nutag_enablemessage = BOOLTAG_TYPEDEF(enableMessage);

/**@def NUTAG_ENABLEMESSAGE_REF(x)
 * Reference tag for NUTAG_ENABLEMESSAGE().
 */



/**@def NUTAG_ENABLEMESSENGER(x)
 *
 * Enable incoming MESSAGE with To tag.
 *
 * Set this parameter true if you want to chat with Windows Messenger. When
 * it is set, stack will accept MESSAGE requests with To tag outside
 * existing dialogs.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - disable Windows-Messenger-specific features
 *    - 1 (true) - enable Windows-Messenger-specific features
 *
 * Corresponding tag taking reference parameter is NUTAG_ENABLEMESSENGER_REF().
 */
tag_typedef_t nutag_enablemessenger = BOOLTAG_TYPEDEF(enableMessenger);

/**@def NUTAG_ENABLEMESSENGER_REF(x)
 * Reference tag for NUTAG_ENABLEMESSENGER().
 */


/**@def NUTAG_SMIME_ENABLE(x)
 *
 * Enable S/MIME
 *
 * @par Used with
 *    nua_create() \n
 *    nua_set_params() \n
 *    nua_get_params()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - S/MIME is Disabled
 *    - 1 (true) - S/MIME is Enabled
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_ENABLE_REF().
 */
tag_typedef_t nutag_smime_enable = BOOLTAG_TYPEDEF(smime_enable);

/**@def NUTAG_SMIME_ENABLE_REF(x)
 * Reference tag for NUTAG_SMIME_ENABLE().
 */


/**@def NUTAG_SMIME_OPT(x)
 *
 * S/MIME Options
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
 *   - -1 (SM_ID_NULL) No security service needed
 *   -  0 (SM_ID_CLEAR_SIGN) Clear signing
 *   -  1 (SM_ID_SIGN) S/MIME signing
 *   -  2 (SM_ID_ENCRYPT) S/MIME encryption
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_OPT_REF().
 */
tag_typedef_t nutag_smime_opt = INTTAG_TYPEDEF(smime_opt);

/**@def NUTAG_SMIME_OPT_REF(x)
 * Reference tag for NUTAG_SMIME_OPT().
 */


/**@def NUTAG_SMIME_PROTECTION_MODE(x)
 *
 * S/MIME protection mode
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
 *   - -1 (SM_MODE_NULL) Unspecified
 *   -  0 (SM_MODE_PAYLOAD_ONLY) SIP payload only
 *   -  1 (SM_MODE_TUNNEL) SIP tunneling mode
 *   -  2 (SM_MODE_SIPFRAG) SIPfrag protection
 *
 * Corresponding tag taking reference parameter is NUTAG_SMIME_PROTECTION_MODE_REF().
 */
tag_typedef_t nutag_smime_protection_mode =
  INTTAG_TYPEDEF(smime_protection_mode);

/**@def NUTAG_SMIME_PROTECTION_MODE_REF(x)
 * Reference tag for NUTAG_SMIME_PROTECTION_MODE().
 */


/**@def NUTAG_SMIME_MESSAGE_DIGEST(x)
 *
 * S/MIME digest algorithm
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
 * Corresponding tag taking reference parameter is NUTAG_SMIME_MESSAGE_DIGEST_REF().
 */
tag_typedef_t nutag_smime_message_digest =
  STRTAG_TYPEDEF(smime_message_digest);

/**@def NUTAG_SMIME_MESSAGE_DIGEST_REF(x)
 * Reference tag for NUTAG_SMIME_MESSAGE_DIGEST().
 */


/**@def NUTAG_SMIME_SIGNATURE(x)
 *
 * S/MIME signature algorithm
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
 * Corresponding tag taking reference parameter is NUTAG_SMIME_SIGNATURE_REF().
 */
tag_typedef_t nutag_smime_signature =
  STRTAG_TYPEDEF(smime_signature);

/**@def NUTAG_SMIME_SIGNATURE_REF(x)
 * Reference tag for NUTAG_SMIME_SIGNATURE().
 */


/**@def NUTAG_SMIME_KEY_ENCRYPTION(x)
 *
 * S/MIME key encryption algorithm
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
 * Corresponding tag taking reference parameter is NUTAG_SMIME_KEY_ENCRYPTION_REF().
 */
tag_typedef_t nutag_smime_key_encryption =
  STRTAG_TYPEDEF(smime_key_encryption);

/**@def NUTAG_SMIME_KEY_ENCRYPTION_REF(x)
 * Reference tag for NUTAG_SMIME_KEY_ENCRYPTION().
 */


/**@def NUTAG_SMIME_MESSAGE_ENCRYPTION(x)
 *
 * S/MIME message encryption algorithm
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
 * Corresponding tag taking reference parameter is NUTAG_SMIME_MESSAGE_ENCRYPTION_REF().
 */
tag_typedef_t nutag_smime_message_encryption =
  STRTAG_TYPEDEF(smime_message_encryption);

/**@def NUTAG_SMIME_MESSAGE_ENCRYPTION_REF(x)
 * Reference tag for NUTAG_SMIME_MESSAGE_ENCRYPTION().
 */


/**@def NUTAG_SIPS_URL(x)
 *
 * Local SIPS url.
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
 * Corresponding tag taking reference parameter is NUTAG_SIPS_URL_REF().
 */
tag_typedef_t nutag_sips_url = URLTAG_TYPEDEF(sips_url);

/**@def NUTAG_SIPS_URL_REF(x)
 * Reference tag for NUTAG_SIPS_URL().
 */


/**@def NUTAG_CERTIFICATE_DIR(x)
 *
 * X.500 certificate directory
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
 * Corresponding tag taking reference parameter is NUTAG_CERTIFICATE_DIR_REF().
 */
tag_typedef_t nutag_certificate_dir = STRTAG_TYPEDEF(certificate_dir);

/**@def NUTAG_CERTIFICATE_DIR_REF(x)
 * Reference tag for NUTAG_CERTIFICATE_DIR().
 */


/**@def NUTAG_CERTIFICATE_PHRASE(x)
 *
 * Certificate phrase
 *
 * @par Used with
 *    Currently not processed by NUA
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_CERTIFICATE_PHRASE_REF().
 */
tag_typedef_t nutag_certificate_phrase = STRTAG_TYPEDEF(certificate_phrase);

/**@def NUTAG_CERTIFICATE_PHRASE_REF(x)
 * Reference tag for NUTAG_CERTIFICATE_PHRASE().
 */

extern msg_hclass_t sip_route_class[];

/**@def NUTAG_INITIAL_ROUTE(x)
 *
 * Specify initial route set.
 *
 * The initial route set is used instead or or in addition to the outbound
 * proxy URL given by NUTAG_PROXY(). The NUTAG_INITIAL_ROUTE() accepts a
 * list of parsed @Route header structures, NUTAG_INITIAL_ROUTE_STR() an
 * unparsed string.
 *
 * If a tag list contains multiple NUTAG_INITIAL_ROUTE() or
 * NUTAG_INITIAL_ROUTE_STR() tags, the route set is constructed from them
 * all.
 *
 * The initial route is inserted into request message before the route
 * entries set with SIPTAG_ROUTE() or SIPTAG_ROUTE_STR().
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    sip_route_t const *
 *
 * @par Values
 *    Linked list of #sip_route_t structures
 *
 * Corresponding tag taking reference parameter is NUTAG_INITIAL_ROUTE_REF().
 *
 * @NEW_1_12_7.
 */
tag_typedef_t nutag_initial_route = SIPEXTHDRTAG_TYPEDEF(initial_route, route);

/**@def NUTAG_INITIAL_ROUTE_REF(x)
 * Reference tag for NUTAG_INITIAL_ROUTE().
 */


/**@def NUTAG_INITIAL_ROUTE_STR(x)
 *
 * Specify initial route set.
 *
 * The initial route set is used instead or or in addition to the outbound
 * proxy URL given by NUTAG_PROXY(). The NUTAG_INITIAL_ROUTE() accepts a
 * list of parsed @Route header structures, NUTAG_INITIAL_ROUTE_STR() a
 * unparsed string containing route URIs, quoted with <> and separated by
 * commas.
 *
 * Please note that the syntax requires <> around the @Route URIs if they
 * contain parameters, e.g., "lr".
 *
 * If a tag list contains multiple NUTAG_INITIAL_ROUTE() or
 * NUTAG_INITIAL_ROUTE_STR() tags, the route set is constructed from them
 * all.
 *
 * The initial route set can be reset with NUTAG_INITIAL_ROUTE(NULL).
 *
 * If a tag list of a request contains SIPTAG_ROUTE() or
 * SIPTAG_ROUTE_STR() tags, the resulting route set will contain first the
 * initial route entries followed by the route URIs given with the
 * SIPTAG_ROUTE()/SIPTAG_ROUTE_STR() tags.
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_set_hparams() \n
 *    any handle-specific nua call
 *
 * @par Parameter type
 *    sip_route_t const *
 *
 * @par Values
 *    Linked list of #sip_route_t structures
 *
 * Corresponding tag taking reference parameter is NUTAG_INITIAL_ROUTE_STR_REF().
 *
 * @NEW_1_12_7.
 */
tag_typedef_t nutag_initial_route_str = STRTAG_TYPEDEF(inital_route_str);

/**@def NUTAG_INITIAL_ROUTE_STR_REF(x)
 * Reference tag for NUTAG_INITIAL_ROUTE_STR().
 */


/**@def NUTAG_REGISTRAR(x)
 *
 * Registrar URL
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
 * Corresponding tag taking reference parameter is NUTAG_REGISTRAR_REF().
 */
tag_typedef_t nutag_registrar = URLTAG_TYPEDEF(registrar);

/**@def NUTAG_REGISTRAR_REF(x)
 * Reference tag for NUTAG_REGISTRAR().
 */


/**@def NUTAG_IDENTITY(x)
 *
 * Registration handle (used with requests and nua_respond()) (NOT YET IMPLEMENTED)
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
 * Corresponding tag taking reference parameter is NUTAG_IDENTITY_REF().
 */
tag_typedef_t nutag_identity = PTRTAG_TYPEDEF(identity);

/**@def NUTAG_IDENTITY_REF(x)
 * Reference tag for NUTAG_IDENTITY().
 */


/**@def NUTAG_M_DISPLAY(x)
 *
 * Display name for @Contact.
 *
 * Specify display name for the Contact header URI generated for
 * registration request and dialog-creating requests/responses.
 *
 * Note that the display name is not included the request-URI when proxy
 * forwards the request towards the user-agent.
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
tag_typedef_t nutag_m_display = STRTAG_TYPEDEF(m_display);

/**@def NUTAG_M_DISPLAY_REF(x)
 * Reference tag for NUTAG_M_DISPLAY().
 */


/**@def NUTAG_M_USERNAME(x)
 *
 * Username prefix for @Contact.
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
tag_typedef_t nutag_m_username = STRTAG_TYPEDEF(m_username);

/**@def NUTAG_M_USERNAME_REF(x)
 * Reference tag for NUTAG_M_USERNAME().
 */


/**@def NUTAG_M_PARAMS(x)
 *
 * URL parameters for @Contact.
 *
 * Specify URL parameters for the @Contact header URI generated for
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
tag_typedef_t nutag_m_params = STRTAG_TYPEDEF(m_params);

/**@def NUTAG_M_PARAMS_REF(x)
 * Reference tag for NUTAG_M_PARAMS().
 */


/**@def NUTAG_M_FEATURES(x)
 *
 * Header parameters for @Contact used in registration.
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
tag_typedef_t nutag_m_features = STRTAG_TYPEDEF(m_features);

/**@def NUTAG_M_FEATURES_REF(x)
 * Reference tag for NUTAG_M_FEATURES().
 */


/**@def NUTAG_INSTANCE(x)
 *
 * Intance identifier.
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
 * Corresponding tag taking reference parameter is NUTAG_INSTANCE_REF().
 */
tag_typedef_t nutag_instance = STRTAG_TYPEDEF(instance);

/**@def NUTAG_INSTANCE_REF(x)
 * Reference tag for NUTAG_INSTANCE().
 */


/**@def NUTAG_OUTBOUND(x)
 *
 * Outbound option string.
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
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_REF().
 */
tag_typedef_t nutag_outbound = STRTAG_TYPEDEF(outbound);

/**@def NUTAG_OUTBOUND_REF(x)
 * Reference tag for NUTAG_OUTBOUND().
 */


/*#@def NUTAG_OUTBOUND_SET1(x)
 *
 * Outbound proxy set 1.
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
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET1_REF().
 */
tag_typedef_t nutag_outbound_set1 = STRTAG_TYPEDEF(outbound_set1);

/*#@def NUTAG_OUTBOUND_SET1_REF(x)
 * Reference tag for NUTAG_OUTBOUND_SET1().
 */


/*#@def NUTAG_OUTBOUND_SET2(x)
 *
 * Outbound proxy set 2.
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
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET2_REF().
 */
tag_typedef_t nutag_outbound_set2 = STRTAG_TYPEDEF(outbound_set2);

/*#@def NUTAG_OUTBOUND_SET2_REF(x)
 * Reference tag for NUTAG_OUTBOUND_SET2().
 */


/*#@def NUTAG_OUTBOUND_SET3(x)
 *
 * Outbound proxy set 3.
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
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET3_REF().
 */
tag_typedef_t nutag_outbound_set3 = STRTAG_TYPEDEF(outbound_set3);

/*#@def NUTAG_OUTBOUND_SET3_REF(x)
 * Reference tag for NUTAG_OUTBOUND_SET3().
 */


/*#@def NUTAG_OUTBOUND_SET4(x)
 *
 * Outbound proxy set 4.
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
 * Corresponding tag taking reference parameter is NUTAG_OUTBOUND_SET4_REF().
 */
tag_typedef_t nutag_outbound_set4 = STRTAG_TYPEDEF(outbound_set4);

/*#@def NUTAG_OUTBOUND_SET4_REF(x)
 * Reference tag for NUTAG_OUTBOUND_SET4().
 */



/**@def NUTAG_KEEPALIVE(x)
 *
 * Keepalive interval in milliseconds.
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
 * NUTAG_KEEPALIVE_REF().
 */
tag_typedef_t nutag_keepalive = UINTTAG_TYPEDEF(keepalive);

/**@def NUTAG_KEEPALIVE_REF(x)
 * Reference tag for NUTAG_KEEPALIVE().
 */


/**@def NUTAG_KEEPALIVE_STREAM(x)
 *
 * Transport-level keepalive interval for streams.
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
 * NUTAG_KEEPALIVE_STREAM_REF().
 *
 * @todo Actually pass NUTAG_KEEPALIVE_STREAM() to transport layer.
 */
tag_typedef_t nutag_keepalive_stream = UINTTAG_TYPEDEF(keepalive_stream);

/**@def NUTAG_KEEPALIVE_STREAM_REF(x)
 * Reference tag for NUTAG_KEEPALIVE_STREAM().
 */



/**@def NUTAG_USE_DIALOG(x)
 *
 * Ask NUA to create dialog for this handle
 *
 * @par Used with nua calls that send a SIP request
 *
 * @par Parameter type
 *   int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - do not create a dialog
 *    - 1 (true) - store dialog info
 *
 * Corresponding tag taking reference parameter is NUTAG_USE_DIALOG_REF().
 */
tag_typedef_t nutag_use_dialog = BOOLTAG_TYPEDEF(use_dialog);

/**@def NUTAG_USE_DIALOG_REF(x)
 * Reference tag for NUTAG_USE_DIALOG().
 */


/**@def NUTAG_AUTH(x)
 *
 * Authentication data ("scheme" "realm" "user" "password")
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
 * Corresponding tag taking reference parameter is NUTAG_AUTH_REF().
 */
tag_typedef_t nutag_auth = STRTAG_TYPEDEF(auth);

/**@def NUTAG_AUTH_REF(x)
 * Reference tag for NUTAG_AUTH().
 */


/**@def NUTAG_AUTHTIME(x)
 *
 * Lifetime of authentication data in seconds.
 *
 * @par Used with
 *    Currently not processed by NUA
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - 0 (zero) - Use authentication data only for this handle
 *    - nonzero - Lifetime of authentication data in seconds
 *
 * @todo
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTHTIME_REF().
 */
tag_typedef_t nutag_authtime = INTTAG_TYPEDEF(authtime);

/**@def NUTAG_AUTHTIME_REF(x)
 * Reference tag for NUTAG_AUTHTIME().
 */



/**@def NUTAG_EVENT(x)
 *
 * NUA event.
 *
 * @deprecated
 *
 * @par Parameter type
 *    enum nua_event_e
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_EVENT_REF().
 */
tag_typedef_t nutag_event = INTTAG_TYPEDEF(event);

/**@def NUTAG_EVENT_REF(x)
 * Reference tag for NUTAG_EVENT().
 */


/**@def NUTAG_STATUS(x)
 *
 * Response status code
 *
 * @deprecated
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 * - 100 - preliminary response, request is being processed by next hop
 * - 1XX - preliminary response, request is being processed by UAS
 * - 2XX - successful final response
 * - 3XX - redirection error response
 * - 4XX - client error response
 * - 5XX - server error response
 * - 6XX - global error response
 *
 * Corresponding tag taking reference parameter is NUTAG_STATUS_REF().
 */
tag_typedef_t nutag_status = INTTAG_TYPEDEF(status);

/**@def NUTAG_STATUS_REF(x)
 * Reference tag for NUTAG_STATUS().
 */


/**@def NUTAG_PHRASE(x)
 *
 * Response phrase
 *
 * @deprecated
 *
 * @par Parameter type
 *    char const *
 *
 * @par Values.
 *
 * Corresponding tag taking reference parameter is NUTAG_PHRASE_REF().
 */
tag_typedef_t nutag_phrase = STRTAG_TYPEDEF(phrase);

/**@def NUTAG_PHRASE_REF(x)
 * Reference tag for NUTAG_PHRASE().
 */


/**@def NUTAG_HANDLE(x)
 *
 * NUA Handle
 *
 * @deprecated
 *
 * @par Parameter type
 *    nua_handle_t *
 *
 * @par Values
 *
 * Corresponding tag taking reference parameter is NUTAG_HANDLE_REF().
 */
tag_typedef_t nutag_handle = PTRTAG_TYPEDEF(handle);

/**@def NUTAG_HANDLE_REF(x)
 * Reference tag for NUTAG_HANDLE().
 */


/**@def NUTAG_NOTIFY_REFER(x)
 *
 * Refer reply handle (used with refer)
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
 * Corresponding tag taking reference parameter is NUTAG_NOTIFY_REFER_REF().
 */
tag_typedef_t nutag_notify_refer = PTRTAG_TYPEDEF(notify_refer);

/**@def NUTAG_NOTIFY_REFER_REF(x)
 * Reference tag for NUTAG_NOTIFY_REFER().
 */

/**@def NUTAG_REFER_EVENT(x)
 *
 * Event used with automatic refer notifications.
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
 * Corresponding tag taking reference parameter is NUTAG_REFER_EVENT_REF().
 */
tag_typedef_t nutag_refer_event = SIPHDRTAG_NAMED_TYPEDEF(refer_event, event);

/**@def NUTAG_REFER_EVENT_REF(x)
 * Reference tag for NUTAG_REFER_EVENT().
 */


/**@def NUTAG_REFER_PAUSE()
 *
 * Invite pauses referrer's handle.
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
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - do not pause referring call
 *    - 1 (true) - pause referring call
 *
 * Corresponding tag taking reference parameter is NUTAG_REFER_PAUSE_REF().
 *
 * @deprecated Not implemented.
 */
tag_typedef_t nutag_refer_pause = BOOLTAG_TYPEDEF(refer_pause);

/**@def NUTAG_REFER_PAUSE_REF(x)
 * Reference tag for NUTAG_REFER_PAUSE().
 */


/**@def NUTAG_USER_AGENT()
 *
 * User-Agent string.
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
 * Corresponding tag taking reference parameter is NUTAG_USER_AGENT_REF().
 */
tag_typedef_t nutag_user_agent = STRTAG_TYPEDEF(user_agent);

/**@def NUTAG_USER_AGENT_REF(x)
 * Reference tag for NUTAG_USER_AGENT().
 */


/**@def NUTAG_ALLOW()
 *
 * Allow a method (or methods).
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
 * Corresponding tag taking reference parameter is NUTAG_ALLOW_REF().
 */
tag_typedef_t nutag_allow = STRTAG_TYPEDEF(allow);

/**@def NUTAG_ALLOW_REF(x)
 * Reference tag for NUTAG_ALLOW().
 */


/**@def NUTAG_ALLOW_EVENTS()
 *
 * Allow an event or events.
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
 * Corresponding tag taking reference parameter is NUTAG_ALLOW_EVENTS_REF().
 */
tag_typedef_t nutag_allow_events = STRTAG_TYPEDEF(allow_events);

/**@def NUTAG_ALLOW_EVENTS_REF(x)
 * Reference tag for NUTAG_ALLOW_EVENTS().
 */


/**@def NUTAG_APPL_METHOD()
 *
 * Indicate that a method (or methods) are handled by application.
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
 * Corresponding tag taking reference parameter is NUTAG_APPL_METHOD_REF().
 *
 * @since Working since @VERSION_1_12_5. Handling of client-side PRACK and
 * UPDATE was fixed in @VERSION_1_12_6.
 */
tag_typedef_t nutag_appl_method = STRTAG_TYPEDEF(appl_method);

/**@def NUTAG_APPL_METHOD_REF(x)
 * Reference tag for NUTAG_APPL_METHOD().
 */


/**@def NUTAG_SUPPORTED()
 *
 * Support a feature.
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
 * Corresponding tag taking reference parameter is NUTAG_SUPPORTED_REF().
 *
 * @since New in @VERSION_1_12_2.
 */
tag_typedef_t nutag_supported = STRTAG_TYPEDEF(supported);

/**@def NUTAG_SUPPORTED_REF(x)
 * Reference tag for NUTAG_SUPPORTED().
 */


/**@def NUTAG_PATH_ENABLE(x)
 *
 * If true, add "path" to @Supported in REGISTER.
 *
 * @par Used with
 * - nua_create(), nua_set_params(), nua_get_params()
 * - nua_handle(), nua_set_hparams(), nua_get_hparams()
 * - nua_register()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - Do not include "path" to @Supported header
 *    - 1 (true) - Include "path" to @Supported header
 *
 * @sa NUTAG_SERVICE_ROUTE_ENABLE(), NUTAG_SUPPORTED(),
 * NUTAG_INITIAL_ROUTE(), NUTAG_INITIAL_ROUTE_STR(), @RFC3327
 * <i>"SIP Extension Header Field for Registering Non-Adjacent Contacts"</i>,
 * D. Willis, B. Hoeneisen,
 * December 2002.
 */
tag_typedef_t nutag_path_enable = BOOLTAG_TYPEDEF(path_enable);

/**@def NUTAG_PATH_ENABLE_REF(x)
 * Reference tag for NUTAG_PATH_ENABLE().
 */



/**@def NUTAG_SERVICE_ROUTE_ENABLE(x)
 *
 * Use route taken from the @ServiceRoute header in the 200 class response
 * to REGISTER.
 *
 * @par Used with
 * - nua_create(), nua_set_params(), nua_get_params()
 * - nua_handle(), nua_set_hparams(), nua_get_hparams()
 * - nua_register()
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - Do not use @ServiceRoute
 *    - 1 (true) - Use @ServiceRoute
 *
 * Corresponding tag taking reference parameter is NUTAG_SERVICE_ROUTE_ENABLE_REF().
 *
 * @sa NUTAG_INITIAL_ROUTE(), NUTAG_INITIAL_ROUTE_STR(), @RFC3608
 *
 * @todo
 */
tag_typedef_t nutag_service_route_enable =
  BOOLTAG_TYPEDEF(service_route_enable);

/**@def NUTAG_SERVICE_ROUTE_ENABLE_REF(x)
 * Reference tag for NUTAG_SERVICE_ROUTE_ENABLE().
 */


/**@def NUTAG_AUTH_CACHE(x)
 *
 * Authentication caching policy
 *
 * @par Used with
 *    nua_set_params(), nua_set_hparams() \n
 *    nua_get_params(), nua_get_hparams() \n
 *    @NUA_HPARAM_CALLS
 *
 * @par Parameter type
 *    enum nua_auth_cache
 *
 * @par Values
 *    - nua_auth_cache_dialog (0) - include credentials within dialog
 *    - nua_auth_cache_challenged (1) - include credentials only when
 *                                      challenged
 *
 * Corresponding tag taking reference parameter is NUTAG_AUTH_CACHE_REF().
 *
 * @NEW_1_12_6.
 */
tag_typedef_t nutag_auth_cache = INTTAG_TYPEDEF(auth_cache);

/**@def NUTAG_AUTH_CACHE_REF(x)
 * Reference tag for NUTAG_AUTH_CACHE().
 */


/**@def NUTAG_DETECT_NETWORK_UPDATES(x)
 *
 * Enable detection of local IP address updates.
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
tag_typedef_t nutag_detect_network_updates = UINTTAG_TYPEDEF(detect_network_updates);

/**@def NUTAG_DETECT_NETWORK_UPDATES_REF(x)
 * Reference tag for NUTAG_DETECT_NETWORK_UPDATES().
 */


/**@def NUTAG_WITH_THIS(nua)
 *
 * Specify request to respond to.
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
 * @sa NUTAG_WITH(), NUTAG_WITH_SAVED()
 */

/**@def NUTAG_WITH(msg)
 *
 * Specify request to respond to.
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
 * @sa nua_save_event(), NUTAG_WITH_THIS(), NUTAG_WITH_SAVED()
 */

/**@def NUTAG_WITH_SAVED(event)
 *
 * Specify request to respond to.
 *
 * @par Used with
 *    nua_respond()
 *
 * @par Parameter type
 *    nua_saved_event_t *
 *
 * @par Values
 *   Pointer to a saved event.
 *
 * @NEW_1_12_4.
 *
 * @sa nua_save_event(), NUTAG_WITH(), NUTAG_WITH_THIS()
 */

tag_typedef_t nutag_with = PTRTAG_TYPEDEF(with);


/**@def NUTAG_DIALOG(x)
 *
 * An (extension) method is used to create dialog or refresh target.
 *
 * @par Used with
 *    nua_method()
 *
 * @par Parameter type
 *    unsigned int (0, 1, 2)
 *
 * @par Values
 *   - 0 if dialog target is not refreshed
 *   - 1 if dialog target is refreshed
 *   - > 1 if dialog is to be created
 *
 * @NEW_1_12_6.
 *
 * @sa nua_method(), #nua_i_method
 */
tag_typedef_t nutag_dialog = UINTTAG_TYPEDEF(dialog);


/**@def NUTAG_PROXY(x)
 *
 * Outbound proxy URL.
 *
 * Same tag as NTATAG_DEFAULT_PROXY()
 *
 * @par Used with
 *    nua_set_params() \n
 *    nua_get_params() \n
 *    nua_create()
 * @note
 * Since @VERSION_1_12_9, NUTAG_PROXY()/NTATAG_DEFAULT_PROXY() can be used
 * to set handle-specific outbound route. The route is set with \n
 *    nua_set_hparams(), \n
 *    nua_invite(), nua_prack(), nua_ack(), nua_update(), nua_respond(), \n
 *    nua_info(), nua_cancel(), nua_bye(), \n
 *    nua_register(), nua_unregister(), nua_publish(), nua_unpublish(), \n
 *    nua_subscribe(), nua_unsubscribe(), nua_refer(), nua_notify(), \n
 *    nua_options(), nua_message(), nua_method()
 *
 * @par Parameter type
 *    url_string_t const * (either char const * or url_t *)
 *
 * @par Values
 *    NULL implies routing based on request-URI or Route header.
 *    Non-NULL is used as invisible routing URI, ie., routing URI that is
 *    not included in the request.
 *
 * Corresponding tag taking reference parameter is NUTAG_PROXY_REF().
 */

/**@def NUTAG_PROXY_REF(x)
 * Reference tag for NUTAG_PROXY().
 */

/**@def NUTAG_SIP_PARSER(x)
 *
 * Pointer to SIP parser structure
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

/**@def NUTAG_SIP_PARSER_REF(x)
 * Reference tag for NUTAG_SIP_PARSER().
 */


/**@def NUTAG_SHUTDOWN_EVENTS(x)
 *
 * Allow passing of normal events when stack is being shut down.
 *
 * By default, only #nua_r_shutdown events are passed to application after
 * calling nua_shutdown(). If application is interested in nua events during
 * shutdown, it should give NUTAG_SHUTDOWN_EVENTS(1) to nua_create() or
 * nua_set_params() called before nua_shutdown().
 *
 * @par Used with
 *    nua_create(), nua_set_params().
 *
 * @par Parameter type
 *    int (boolean: nonzero is true, zero is false)
 *
 * @par Values
 *    - 0 (false) - pass only #nua_r_shutdown events to application during shutdown
 *    - 1 (true) - pass all events to application during shutdown
 *
 * Corresponding tag taking reference parameter is NUTAG_SHUTDOWN_EVENTS_REF().
 *
 * @sa nua_shutdown(), nua_destroy().
 *
 * @NEW_1_12_9.
 */
tag_typedef_t nutag_shutdown_events = BOOLTAG_TYPEDEF(shutdown_events);

/**@def NUTAG_SHUTDOWN_EVENTS_REF(x)
 * Reference tag for NUTAG_SHUTDOWN_EVENTS().
 */


/* ---------------------------------------------------------------------- */

tag_typedef_t nutag_soa_session = PTRTAG_TYPEDEF(soa_session);
tag_typedef_t nutag_hold = BOOLTAG_TYPEDEF(hold);
tag_typedef_t nutag_address = STRTAG_TYPEDEF(address);
