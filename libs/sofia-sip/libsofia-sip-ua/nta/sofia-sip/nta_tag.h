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

#ifndef NTA_TAG_H
/** Defined when <sofia-sip/nta_tag.h> has been included. */
#define NTA_TAG_H

/**@file sofia-sip/nta_tag.h   
 * @brief NTA tags
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep  4 15:54:57 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#ifndef SIP_TAG_H
#include <sofia-sip/sip_tag.h>
#endif

#ifndef URL_TAG_H
#include <sofia-sip/url_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** List of all nta tags. */
NTA_DLL extern tag_type_t nta_tag_list[];

/** Filter tag matching any nta tag. */
#define NTATAG_ANY()         ntatag_any, ((tag_value_t)0)
NTA_DLL extern tag_typedef_t ntatag_any;

/* Tags for parameters */

NTA_DLL extern tag_typedef_t ntatag_mclass;
/** Message class used by NTA. 
 *
 * The nta can use a custom or extended parser created with
 * msg_mclass_clone().
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    pointer to #msg_mclass_t.
 *
 * @par Values
 *    - custom or extended parser created with msg_mclass_clone()
 *    - NULL - use default parser
 *
 * @sa NTATAG_SIPFLAGS()
 */
#define NTATAG_MCLASS(x) ntatag_mclass, tag_cptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_mclass_ref;
#define NTATAG_MCLASS_REF(x) ntatag_mclass_ref, tag_cptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask;
/** Mask for bad request messages. 
 * 
 * If an incoming request has erroneous headers matching with the mask, nta
 * automatically returns a 400 Bad Message response to them. 
 *
 * If mask ~0U (all bits set) is specified, all requests with any bad header
 * are dropped. By default only the requests with bad headers essential for
 * request processing or proxying are dropped.
 * 
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - bitwise or of enum #sip_bad_mask values
 *
 * @sa enum #sip_bad_mask, NTATAG_BAD_RESP_MASK()
 *
 * @note
 * The following headers are considered essential by default:
 * - @ref sip_request \"request line\"", @From, @To, @CSeq, @CallID,
 *   @ContentLength, @Via, @ContentType, @ContentDisposition,
 *   @ContentEncoding, @Supported, @Contact, @Require, @RecordRoute, @RAck,
 *   @RSeq, @Event, @Expires, @SubscriptionState, @SessionExpires,
 *   @MinSE, @SIPEtag, and @SIPIfMatch.
 *  
 */
#define NTATAG_BAD_REQ_MASK(x) ntatag_bad_req_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask_ref;
#define NTATAG_BAD_REQ_MASK_REF(x) ntatag_bad_req_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask;
/** Mask for bad response messages. 
 * 
 * If an incoming response has erroneous headers matching with the mask, nta
 * drops the response message. 
 *
 * If mask ~0U (all bits set) is specified, all responses with any bad header
 * are dropped. By default only the responses with bad headers essential for
 * response processing or proxying are dropped.
 * 
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - bitwise or of enum #sip_bad_mask values
 *
 * @sa enum #sip_bad_mask, NTATAG_BAD_REQ_MASK()
 *
 * @note
 * The following headers are considered essential by default:
 * - @ref sip_status \"status line\"", @From, @To, @CSeq, @CallID,
 *   @ContentLength, @Via, @ContentType, @ContentDisposition,
 *   @ContentEncoding, @Supported, @Contact, @Require, @RecordRoute, @RAck,
 *   @RSeq, @Event, @Expires, @SubscriptionState, @SessionExpires, 
 *   @MinSE, @SIPEtag, and @SIPIfMatch.
 */
#define NTATAG_BAD_RESP_MASK(x) ntatag_bad_resp_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask_ref;
#define NTATAG_BAD_RESP_MASK_REF(x) ntatag_bad_resp_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy;
/** URL for (default) proxy.
 *
 * The requests are sent towards the <i>default outbound proxy</i> regardless
 * the values of request-URI or @Route headers in the request. The URL of
 * the default proxy is not added to the request in the @Route header or in
 * the request-URI (against the recommendation of @RFC3261 section 8.1.2).
 *
 * The outbound proxy set by NTATAG_DEFAULT_PROXY() is used even if the
 * dialog had an established route set or registration provided User-Agent
 * with a @ServiceRoute set.
 *
 * @par Used with
 *    nua_create(), nta_agent_create(), nta_agent_set_params() \n
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par Parameter type
 *    Pointer to a url_t structure or a string containg a SIP or SIPS URI
 *
 * @par Values
 *    - Valid SIP or SIPS URI
 */
#define NTATAG_DEFAULT_PROXY(x) ntatag_default_proxy, urltag_url_v((x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy_ref;
#define NTATAG_DEFAULT_PROXY_REF(x) \
ntatag_default_proxy_ref, urltag_url_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_contact;
/** Contact used by NTA. */
#define NTATAG_CONTACT(x) ntatag_contact, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_contact_ref;
#define NTATAG_CONTACT_REF(x) \
ntatag_contact_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_target;
/** Dialog target (contact) used by NTA. @HI */
#define NTATAG_TARGET(x) \
ntatag_target, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_target_ref;
#define NTATAG_TARGET_REF(x) \
ntatag_target_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_aliases;
/** Aliases used by NTA. @HI @deprecated */
#define NTATAG_ALIASES(x) \
ntatag_aliases, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_aliases_ref;
#define NTATAG_ALIASES_REF(x) \
ntatag_aliases_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_branch_key;
/** Branch key. @HI */
#define NTATAG_BRANCH_KEY(x) \
ntatag_branch_key, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_branch_key_ref;
#define NTATAG_BRANCH_KEY_REF(x) \
ntatag_branch_key_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_ack_branch;
/** Branch for ACKed transaction. @HI */
#define NTATAG_ACK_BRANCH(x) ntatag_ack_branch, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_ack_branch_ref;
#define NTATAG_ACK_BRANCH_REF(x) ntatag_ack_branch_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_comp;
/** Compression algorithm. 
 *
 * Set compression algorithm for request as described in @RFC3486.
 *
 * @note This tag is has no effect without a compression plugin.
 *
 * @par Used with
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par
 * Note that NTATAG_COMP(NULL) can be used with nta_incoming_set_params()
 * and nta_incoming_treply(), too. It indicates that the response is sent
 * uncompressed, no matter what the client has in @a comp parameter of @Via
 * header.
 *
 * @par Parameter type
 *    string
 *
 * @par Values
 *    - name of the compression algorithm ("sigcomp")
 *
 * @sa @RFC3320, @RFC3486, TPTAG_COMPARTMENT(),
 * NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_AWARE(),
 * NTATAG_SIGCOMP_CLOSE(), NTATAG_SIGCOMP_OPTIONS()
 */
#define NTATAG_COMP(x) ntatag_comp, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_comp_ref;
#define NTATAG_COMP_REF(x) ntatag_comp_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_msg;
/** Pass a SIP message to treply()/tcreate() functions. @HI */
#define NTATAG_MSG(x)     ntatag_msg, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_msg_ref;
#define NTATAG_MSG_REF(x) ntatag_msg_ref, tag_ptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_tport;
/** Pass a transport object to msg_tsend/msg_treply. @HI */
#define NTATAG_TPORT(x)     ntatag_tport, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_tport_ref;
#define NTATAG_TPORT_REF(x) ntatag_tport_ref, tag_ptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_remote_cseq;
/** Remote CSeq number. @HI */
#define NTATAG_REMOTE_CSEQ(x) ntatag_remote_cseq, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_remote_cseq_ref;
#define NTATAG_REMOTE_CSEQ_REF(x) ntatag_remote_cseq_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_smime;
/** Provide S/MIME context to NTA. @HI */
#define NTATAG_SMIME(x)     ntatag_smime, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_smime_ref;
#define NTATAG_SMIME_REF(x) ntatag_smime_ref, tag_ptr_vr(&(x), (x))
 
NTA_DLL extern tag_typedef_t ntatag_maxsize;
/** Maximum size of incoming message. @HI 
 *
 * If the size of an incoming request message would exceed the
 * given limit, the stack will automatically respond with <i>413 Request
 * Entity Too Large</i>.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    usize_t 
 *
 * @par Values
 *    - Maximum acceptable size of an incoming request message.
 *      Default value is 2 megabytes (2097152 bytes).
 *
 * @sa msg_maxsize()
 */
#define NTATAG_MAXSIZE(x) ntatag_maxsize, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_maxsize_ref;
#define NTATAG_MAXSIZE_REF(x) ntatag_maxsize_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu;
/** Maximum size of outgoing UDP request. @HI 
 *
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    
 *
 * @par Values
 *    - 
 *
 * @sa
 *
 */
#define NTATAG_UDP_MTU(x) ntatag_udp_mtu, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu_ref;
#define NTATAG_UDP_MTU_REF(x) ntatag_udp_mtu_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards;
/** Default value for @MaxForwards header. 
 *
 * The default value of @MaxForwards header added to the requests. The
 * initial value recommended by @RFC3261 is 70, but usually SIP proxies use
 * much lower default value, such as 24.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    - Default value added to the @MaxForwards header in the sent requests
 *
 * @since New in @VERSION_1_12_2.
 */
#define NTATAG_MAX_FORWARDS(x) ntatag_max_forwards, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards_ref;
#define NTATAG_MAX_FORWARDS_REF(x) ntatag_max_forwards_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1;
/** Initial retransmission interval (in milliseconds) @HI 
 *
 * Set the T1 retransmission interval used by the SIP transaction engine. The
 * T1 is the initial duration used by request retransmission timers A and E
 * (UDP) as well as response retransmission timer G.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T1 in milliseconds 
 *
 * @sa @RFC3261 appendix A, NTATAG_SIP_T1X4(), NTATAG_SIP_T1(), NTATAG_SIP_T4()
 */
#define NTATAG_SIP_T1(x) ntatag_sip_t1, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1_ref;
#define NTATAG_SIP_T1_REF(x) ntatag_sip_t1_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64;
/** Transaction timeout (defaults to T1 * 64). @HI 
 *
 * Set the T1x64  timeout value used by the SIP transaction engine. The T1x64 is
 * duration used for timers B, F, H, and J (UDP) by the SIP transaction engine. 
 * The timeout value T1x64 can be adjusted separately from the initial
 * retransmission interval T1, which is set with NTATAG_SIP_T1().
 * 
 * The default value for T1x64 is 64 times value of T1, or 32000 milliseconds.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of T1x64 in milliseconds
 *
 * @sa @RFC3261 appendix A, NTATAG_SIP_T1(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 *
 */
#define NTATAG_SIP_T1X64(x) ntatag_sip_t1x64, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64_ref;
#define NTATAG_SIP_T1X64_REF(x) ntatag_sip_t1x64_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2;
/** Maximum retransmission interval (in milliseconds) @HI 
 *
 * Set the maximum retransmission interval used by the SIP transaction
 * engine. The T2 is the maximum duration used for the timers E (UDP) and G
 * by the SIP transaction engine. Note that the timer A is not capped by T2. 
 * Retransmission interval of INVITE requests grows exponentially until the
 * timer B fires.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T2 in milliseconds 
 *
 * @sa @RFC3261 appendix A, NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T4()
 */
#define NTATAG_SIP_T2(x) ntatag_sip_t2, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2_ref;
#define NTATAG_SIP_T2_REF(x) ntatag_sip_t2_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4;
/** Transaction lifetime (in milliseconds) @HI 
 *
 * Set the lifetime for completed transactions used by the SIP transaction
 * engine. A completed transaction is kept around for the duration of T4 in
 * order to catch late responses. The T4 is the maximum duration for the
 * messages to stay in the network and the duration of SIP timer K.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T4 in milliseconds
 *
 * @sa @RFC3261 appendix A, NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2()
 */
#define NTATAG_SIP_T4(x)    ntatag_sip_t4, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4_ref;
#define NTATAG_SIP_T4_REF(x) ntatag_sip_t4_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_progress;
/** Progress timer for User-Agents (interval for retranmitting 1XXs) @HI.
 *
 * The UAS should retransmit preliminary responses to the INVITE
 * transactions every minute in order to re-set the timer C within the
 * intermediate proxies.
 *
 * The default value for the progress timer is 60000.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    Value of progress timer in milliseconds.
 *
 * @sa @RFC3261 sections 13.3.1.1, 16.7 and 16.8, NTATAG_TIMER_C(),
 * NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 */
#define NTATAG_PROGRESS(x)    ntatag_progress, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_progress_ref;
#define NTATAG_PROGRESS_REF(x) ntatag_progress_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_timer_c;
/** Value for timer C in milliseconds. @HI
 *
 * By default the INVITE transaction will not timeout after a preliminary
 * response has been received. However, an intermediate proxy can timeout
 * the transaction using timer C. Timer C is reset every time a response
 * belonging to the transaction is received.
 *
 * The default value for the timer C is 185000 milliseconds (3 minutes and 5
 * seconds). By default, timer C is not run on user agents (if NTATAG_UA(1)
 * without NTATAG_TIMER_C() is fgiven).
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    Value of SIP timer C in milliseconds. The default value is used
 *    instead if NTATAG_TIMER_C(0) is given.
 *
 * @sa @RFC3261 sections 13.3.1.1, 16.7 and 16.8,
 * NTATAG_UA(1), NTATAG_TIMER_C(),
 * NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 */
#define NTATAG_TIMER_C(x)    ntatag_timer_c, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_timer_c_ref;
#define NTATAG_TIMER_C_REF(x) ntatag_timer_c_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_blacklist;
/** Add Retry-After header to internally-generated error messages. @HI 
 *
 * The NTATAG_BLACKLIST() provides a default value for @RetryAfter header
 * added to the internally generated responses such as <i>503 DNS Error</i>
 * or <i>408 Timeout</i>. The idea is that the application can retain its
 * current state and retry the operation after a while.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *     unsigned int
 *
 * @par Values
 *    - Value of @RetryAfter header (in seconds)
 *
 * @sa NTATAG_TIMEOUT_408()
 */
#define NTATAG_BLACKLIST(x)  ntatag_blacklist, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_blacklist_ref;
#define NTATAG_BLACKLIST_REF(x) ntatag_blacklist_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob;
/** Packet drop probability for debugging. 
 *
 * The packet drop probability parameter is useful mainly for debugging
 * purposes. The stack drops an incoming message received over an unreliable
 * transport (such as UDP) with the given probability. The range is in 0 .. 
 * 1000, 500 means p=0.5.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned integer
 *
 * @par Values
 *    - Valid values are in range 0 ... 1000
 *    - Probablity to drop a given message is value / 1000.
 *
 * @HI
 */
#define NTATAG_DEBUG_DROP_PROB(x) ntatag_debug_drop_prob, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob_ref;
#define NTATAG_DEBUG_DROP_PROB_REF(x) ntatag_debug_drop_prob_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options;
/** Semicolon-separated SigComp options.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *    nta_agent_add_tport() \n
 *
 * @par Parameter type
 *    string 
 *
 * @par Values
 *    - semicolon-separated parameter-value pairs, passed to the SigComp plugin
 *
 * @sa NTATAG_COMP(), NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_AWARE(),
 * NTATAG_SIGCOMP_CLOSE(), @RFC3320
 * @HI
 */
#define NTATAG_SIGCOMP_OPTIONS(x)    ntatag_sigcomp_options, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options_ref;
#define NTATAG_SIGCOMP_OPTIONS_REF(x) ntatag_sigcomp_options_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close;
/** Close SigComp compartment after completing transaction.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nta_incoming_set_params(), nta_incoming_treply()
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tmcreate(), nta_outgoing_tcancel()
 *    nta_outgoing_prack(), nta_msg_tsend(), nta_msg_treply()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - application takes care of compartment management
 *    - false - stack manages compartments
 *
 * @sa NTATAG_COMP(), TPTAG_COMPARTMENT(),
 * NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_AWARE(),
 * NTATAG_SIGCOMP_OPTIONS(), @RFC3320
 * @HI
 */
#define NTATAG_SIGCOMP_CLOSE(x)  ntatag_sigcomp_close, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close_ref;
#define NTATAG_SIGCOMP_CLOSE_REF(x) ntatag_sigcomp_close_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware;
/** Indicate that the application is SigComp-aware. 
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - application takes care of compartment management
 *    - false - stack manages compartments
 *
 * @sa NTATAG_COMP(), NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_CLOSE(),
 * NTATAG_SIGCOMP_OPTIONS(), @RFC3320
 * 
 * @HI
 */
#define NTATAG_SIGCOMP_AWARE(x) ntatag_sigcomp_aware, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware_ref;
#define NTATAG_SIGCOMP_AWARE_REF(x) ntatag_sigcomp_aware_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm;
/** Specify SigComp algorithm.  
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *    nta_agent_add_tport() \n
 *
 * @par Parameter type
 *    string 
 *
 * @par Values
 *    - opaque string passed to the SigComp plugin
 *
 * @sa NTATAG_COMP(), NTATAG_SIGCOMP_AWARE(), NTATAG_SIGCOMP_CLOSE(),
 * NTATAG_SIGCOMP_OPTIONS(), @RFC3320
 * 
 * @HI
 */
#define NTATAG_SIGCOMP_ALGORITHM(x) ntatag_sigcomp_algorithm, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm_ref;
#define NTATAG_SIGCOMP_ALGORITHM_REF(x) \
ntatag_sigcomp_algorithm_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_ua;
/** If true, NTA acts as User Agent Server or Client by default.
 *
 * When acting as an UA, the NTA stack will
 * - respond with 481 to a PRACK request with no matching "100rel" response
 * - check for out-of-order CSeq headers for each #nta_leg_t dialog object
 * - if NTATAG_MERGE_482(1) is also used, return <i>482 Request Merged</i> to
 *   a duplicate request with same @CallID, @CSeq, @From tag but different
 *   topmost @Via header (see @RFC3261 section 8.2.2.2 Merged Requests)
 * - silently discard duplicate final responses to INVITE
 * - retransmit preliminary responses (101..199) to INVITE request in regular
 *   intervals ("timer N2")
 * - retransmit 2XX response to INVITE request with exponential intervals 
 * - handle ACK sent in 2XX response to an INVITE using the
 *   #nta_ack_cancel_f callback bound to #nta_incoming_t with
 *   nta_incoming_bind()
 * - not use timer C unless its value has been explicitly set
 *
 * @note This NUTAG_UA(1) is set internally by nua_create()
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - act as an UA 
 *    - false - act as an proxy 
 *
 * @sa NTATAG_MERGE_482()
 */
#define NTATAG_UA(x) ntatag_ua, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_ua_ref;
#define NTATAG_UA_REF(x) ntatag_ua_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_stateless;
/** Enable stateless processing. @HI 
 *
 * @par Server side
 * The incoming requests are processed statefully if there is a default leg
 * (created with nta_leg_default()). This option is provided for proxies or
 * other server elements that process requests statelessly.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Values
 *    - true - do not pass incoming requests to default leg
 *    - false - pass incoming requests to default leg, if it exists
 *
 * @par Client side
 * The outgoing requests can be sent statelessly, too, if the
 * NTATAG_STATELESS(1) is included in the tag list of nta_outgoing_tcreate().
 *
 * @par Used with
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - create only a transient #nta_outgoing_t transaction object
 *    - false - create an ordinary client transaction object
 *
 * @sa NTATAG_IS_UA(), nta_incoming_default(), nta_outgoing_default(),
 * nta_leg_default()
 */
#define NTATAG_STATELESS(x) ntatag_stateless, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_stateless_ref;
#define NTATAG_STATELESS_REF(x) ntatag_stateless_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_user_via;
/** Allow application to insert Via headers. @HI 
 *
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - 
 *    - false - 
 *
 * @sa
 *
 */
#define NTATAG_USER_VIA(x) ntatag_user_via, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_user_via_ref;
#define NTATAG_USER_VIA_REF(x) ntatag_user_via_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_extra_100;
/** Respond with "100 Trying" if application has not responded.
 *
 * As per recommended by @RFC4320, the stack can generate a 100 Trying
 * response to the non-INVITE requests if the application has not responded
 * to a request within half of the SIP T2 (the default value for T2 is 4000
 * milliseconds, so the extra <i>100 Trying<i/> would be sent after 2 seconds).
 *
 * @par Used with	
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - send extra 100 Trying if application does not respond
 *    - false - do not send 100 Trying (default)
 *
 * @sa @RFC4320, NTATAG_PASS_408(), NTATAG_TIMEOUT_408()
 */
#define NTATAG_EXTRA_100(x)    ntatag_extra_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_extra_100_ref;
#define NTATAG_EXTRA_100_REF(x) ntatag_extra_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_100;
/** Pass "100 Trying" provisional answers to the application. @HI 
 *
 * By default, the stack silently processes the <i>100 Trying</i> responses
 * from the server. Usually the <i>100 Trying</i> responses are not
 * important to the application but rather sent by the outgoing proxy
 * immediately after it has received the request. However, the application
 * can ask nta for them by setting NTATAG_PASS_100(1) if, for instance, the
 * <i>100 Trying</i> responses are needed for user feedback.
 *
 * @par Used with
 *    nua_create(), nta_agent_create(), nta_agent_set_params() \n
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - pass <i>100 Trying</i> to application
 *    - false - silently process <i>100 Trying</i> responses
 *
 * @sa NTATAG_EXTRA_100(), NTATAG_DEFAULT_PROXY()
 */
#define NTATAG_PASS_100(x) ntatag_pass_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_100_ref;
#define NTATAG_PASS_100_REF(x) ntatag_pass_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408;
/** Generate "408 Request Timeout" response when request times out. @HI 
 *
 * This tag is used to prevent stack from generating extra 408 response
 * messages to non-INVITE requests upon timeout. As per recommended by
 * @RFC4320, the <i>408 Request Timeout</i> responses to non-INVITE
 * transaction are not sent over the network to the client by default. The
 * application can ask stack to pass the 408 responses with
 * NTATAG_PASS_408(1).
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - generate 408 response
 *    - false - invoke #nta_response_f callback with NULL sip pointer
 *              when a non-INVITE transaction times out
 *
 * @sa @RFC4320, NTATAG_PASS_408(), NTATAG_EXTRA_100(),
 */
#define NTATAG_TIMEOUT_408(x)  ntatag_timeout_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408_ref;
#define NTATAG_TIMEOUT_408_REF(x) ntatag_timeout_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_408;
/** Pass "408 Request Timeout" responses to the client. @HI 
 *
 * As per recommended by @RFC4320, the <i>408 Request Timeout</i> responses
 * to non-INVITE transaction are not sent over the network to the client by
 * default. The application can ask stack to pass the 408 responses with
 * NTATAG_PASS_408(1). 
 *
 * Note that unlike NTATAG_PASS_100(), this tags changes the way server side
 * works.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - pass superfluous 408 responses 
 *    - false - discard superfluous 408 responses
 *
 * @sa @RFC4320, NTATAG_EXTRA_100(), NTATAG_TIMEOUT_408()
 *
 */
#define NTATAG_PASS_408(x)  ntatag_pass_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_408_ref;
#define NTATAG_PASS_408_REF(x) ntatag_pass_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog;
/** Create a leg without dialog. @HI */
#define NTATAG_NO_DIALOG(x)       ntatag_no_dialog, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog_ref;
#define NTATAG_NO_DIALOG_REF(x)   ntatag_no_dialog_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_merge_482;
/** Merge requests, send 482 to other requests. @HI 
 *
 * If an User-Agent receives a duplicate request with same @CallID, @CSeq,
 * @From tag but different topmost @Via header (see @RFC3261 section 8.2.2.2
 * Merged Requests), it should return <i>482 Request Merged</i> response to
 * the duplicate request. Such a duplicate request has been originally
 * generated by a forking proxy and usually routed via different route to
 * the User-Agent. The User-Agent should only respond meaningfully to the
 * first request and return the 482 response to the following forked
 * requests.
 *
 * Note that also NTATAG_UA(1) should be set before nta detects merges and
 * responds with 482 to them.
 *
 * @note If your application is an multi-lined user-agent, you may consider
 * disabling request merging. However, you have to somehow handle merging
 * within a single line.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - detect duplicate requests and respond with 482 to them
 *    - false - process duplicate requests separately
 *
 * @sa NTATAG_UA(1)
 */
#define NTATAG_MERGE_482(x)       ntatag_merge_482, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_merge_482_ref;
#define NTATAG_MERGE_482_REF(x)   ntatag_merge_482_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543;
/**Follow @RFC2543 semantics with CANCEL.
 *
 * By default, the nta follows "@RFC3261" semantics when CANCELing a
 * request. The CANCEL does not terminate transaction, rather, it is just a
 * hint to the server that it should respond immediately (with <i>487
 * Request Terminated</i> if it has no better response). Also, if the
 * original request was sent over unreliable transport such as UDP, the
 * CANCEL is delayed until the server has sent a preliminary response to the
 * original request.
 *
 * If NTATAG_CANCEL_2543(1) is given, the transaction is canceled
 * immediately internally (a 487 response is generated locally) and the
 * CANCEL request is sent without waiting for an provisional response.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *    nta_outgoing_tcancel()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - follow "RFC 2543" semantics with CANCEL
 *    - false - follow "RFC 3261" semantics with CANCEL
 *
 * @sa NTATAG_CANCEL_408()
 * 
 * @HI
 */
#define NTATAG_CANCEL_2543(x)     ntatag_cancel_2543, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543_ref;
#define NTATAG_CANCEL_2543_REF(x) ntatag_cancel_2543_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408;
/** Do not send a CANCEL but just timeout the request. 
 *
 * Calling nta_outgoing_tcancel() with this tag set marks request as
 * canceled but does not actually send a CANCEL request. If
 * NTATAG_CANCEL_2543(1) is also included, a 487 response is generated
 * internally.
 *
 * @par Used with
 *    nta_outgoing_tcancel() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - do not send CANCEL
 *    - false - let request to timeout
 *
 * @sa NTATAG_CANCEL_2543()
 */
#define NTATAG_CANCEL_408(x)     ntatag_cancel_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408_ref;
#define NTATAG_CANCEL_408_REF(x) ntatag_cancel_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261;
/** When responding to requests, use unique tags. @HI 
 *
 * If set the UA would generate an unique @From/@To tag for all dialogs. If
 * unset UA would reuse same tag in order to make it easier to re-establish
 * dialog state after a reboot.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - use different tag for each dialog
 *    - false - use same tag for all dialogs
 *
 * @sa @RFC3261 section 12.2.2
 */
#define NTATAG_TAG_3261(x)        ntatag_tag_3261, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261_ref;
#define NTATAG_TAG_3261_REF(x)    ntatag_tag_3261_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp;
/** Use @Timestamp header. @HI 
 *
 * If set, a @Timestamp header would be added to stateful requests. The
 * header can be used to calculate the roundtrip transport latency between
 * client and server.
 *
 * @par Used with
 *    nua_create(), 
 *    nta_agent_create(),
 *    nta_agent_set_params(),
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), and nta_outgoing_prack().
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - Add @Timestamp header
 *    - false - do not add @Timestamp header
 *
 * @sa @RFC3261 section 8.2.6
 */
#define NTATAG_USE_TIMESTAMP(x) ntatag_use_timestamp, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp_ref;
#define NTATAG_USE_TIMESTAMP_REF(x) ntatag_use_timestamp_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_method;
/**Method name. @HI 
 *
 * Create a dialogless #nta_leg_t object matching only requests with
 * the specified method.
 *
 * @par Used with
 *   nta_leg_tcreate()
 *
 * @par Parameter type
 *    String containing method name.
 *
 * @par Values
 *    SIP method name (e.g., "SUBSCRIBE").
 *
 */
#define NTATAG_METHOD(x)          ntatag_method, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_method_ref;
#define NTATAG_METHOD_REF(x)      ntatag_method_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487;
/** When a CANCEL is received, automatically return 487 response to original request.
 *
 * When the CANCEL is received for an ongoing server transaction
 * #nta_incoming_t, the stack will automatically return a <i>487 Request
 * Terminated</i> response to the client after returning from the
 * #nta_incoming_f callback bound to the transaction with
 * nta_incoming_bind()
 * 
 * The application can delay sending the response to the original request
 * when NTATAG_CANCEL_408(0) is used. This is useful, for instance, with a
 * proxy that forwards the CANCEL downstream and the forwards the response
 * back to upstream.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - respond automatically to the CANCELed transaction
 *    - false - application takes care of responding
 *
 * @sa NTATAG_CANCEL_2543(), nta_incoming_bind()
 *
 * @HI
 */
#define NTATAG_CANCEL_487(x)     ntatag_cancel_487, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487_ref;
#define NTATAG_CANCEL_487_REF(x) ntatag_cancel_487_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rel100;
/** Include rel100 in INVITE requests. @HI 
 *
 * Include feature tag "100rel" in @Supported header of the INVITE requests.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - include "100rel"
 *    - false - do not include "100rel"
 *
 * @sa nta_outgoing_prack(), nta_reliable_treply(), nta_reliable_mreply()
 */
#define NTATAG_REL100(x)     ntatag_rel100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_rel100_ref;
#define NTATAG_REL100_REF(x) ntatag_rel100_ref, tag_bool_vr(&(x))
 
NTA_DLL extern tag_typedef_t ntatag_sipflags;
/** Set SIP parser flags. @HI 
 *
 * The SIP parser flags affect how the messages are parsed and the result
 * presented to the application. They also control encoding of messages.
 * The most important flags are as follows:
 * - MSG_FLG_COMPACT - use compact form 
 *                     (single-letter header names, minimum whitespace)
 * - MSG_FLG_EXTRACT_COPY - cache printable copy of headers when parsing. 
 *   Using this flag can speed up proxy processing considerably. It is
 *   implied when the parsed messages are logged (because #TPORT_LOG
 *   environment variable is set, or TPTAG_LOG() is used.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned int 
 *
 * @par Values
 *    - Bitwise OR of SIP parser flags (enum #msg_flg_user)
 *
 * @sa NTATAG_PRELOAD(), enum #msg_flg_user, sip_s::sip_flags
 */
#define NTATAG_SIPFLAGS(x)     ntatag_sipflags, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sipflags_ref;
#define NTATAG_SIPFLAGS_REF(x) ntatag_sipflags_ref, tag_uint_vr(&(x))
 
NTA_DLL extern tag_typedef_t ntatag_client_rport;
/** Enable client-side "rport". @HI 
 *
 * This tag controls @RFC3581 support on client side. The "rport" parameter
 * is used when the response has to be routed symmetrically through a NAT box.
 *
 * The client-side support involves just adding the "rport" parameter to the topmost
 * @Via header before the request is sent. 
 *
 * @note By default, the client "rport" is disabled when nta is used, and
 * enabled when nua is used.
 *
 * @par Used with
 *    nua_create() (nua uses NTATAG_CLIENT_RPORT(1) by default) \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - add "rport" parameter
 *    - false - do not add "rport" parameter
 *
 * @note The NTATAG_RPORT() is a synonym for this.
 *
 * @sa @RFC3581, NTATAG_SERVER_RPORT(), @Via
 */
#define NTATAG_CLIENT_RPORT(x) ntatag_client_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_client_rport_ref;
#define NTATAG_CLIENT_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

#define NTATAG_RPORT(x) ntatag_client_rport, tag_bool_v((x))
#define NTATAG_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_server_rport;
/** Use rport parameter at server.
 *
 * This tag controls @RFC3581 support on server side. The "rport" parameter
 * is used when the response has to be routed symmetrically through a NAT
 * box.
 *
 * If the topmost @Via header has an "rport" parameter, the server stores
 * the port number from which the request was sent in it. When sending the
 * response back to the client, the server uses the port number in the
 * "rport" parameter rather than the client-supplied port number in @Via
 * header.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - use "rport" parameter (default)
 *    - false - do not use "rport" parameterx
 *
 * @sa @RFC3581, NTATAG_CLIENT_RPORT(), @Via
 */
#define NTATAG_SERVER_RPORT(x) ntatag_server_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_server_rport_ref;
#define NTATAG_SERVER_RPORT_REF(x) ntatag_server_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport;
/** Use rport with TCP, too. @HI 
 *
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - 
 *    - false - 
 *
 * @sa
 *
 */
#define NTATAG_TCP_RPORT(x) ntatag_tcp_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport_ref;
#define NTATAG_TCP_RPORT_REF(x) ntatag_tcp_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_preload;
/** Preload by N bytes. @HI 
 *
 * When the memory block is allocated for an incoming request by the stack,
 * the stack can allocate some extra memory for the parser in addition to
 * the memory used by the actual message contents. 
 * 
 * While wasting some memory, this can speed up parsing considerably.
 * Recommended amount of preloading per packet is 1500 bytes.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    Amount of extra per-message memory allocated for parser.
 *
 * @sa NTATAG_SIPFLAGS() and #MSG_FLG_EXTRACT_COPY
 */
#define NTATAG_PRELOAD(x) ntatag_preload, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_preload_ref;
#define NTATAG_PRELOAD_REF(x) ntatag_preload_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr;
/** If true, try to use NAPTR records when resolving. @HI 
 *
 * The application can disable NTA from using NAPTR records when resolving
 * SIP URIs.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - enable NAPTR resolving
 *    - false - disable NAPTR resolving
 *
 * @bug NAPTRs are not used with SIPS URIs in any case.
 *
 * @sa @RFC3263, NTATAG_USE_SRV()
 */
#define NTATAG_USE_NAPTR(x) ntatag_use_naptr, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr_ref;
#define NTATAG_USE_NAPTR_REF(x) ntatag_use_naptr_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_srv;
/** If true, try to use SRV records when resolving.
 *
 * The application can disable NTA from using SRV records when resolving
 * SIP URIs.
 *
 * @par Used with
 *    nua_create() \n
 *    nta_agent_create() \n
 *    nta_agent_set_params() \n
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - enable SRV resolving
 *    - false - disable SRV resolving
 *
 * @sa @RFC3263, NTATAG_USE_NAPTR()
 */
#define NTATAG_USE_SRV(x) ntatag_use_srv, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_srv_ref;
#define NTATAG_USE_SRV_REF(x) ntatag_use_srv_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rseq;

/** @RSeq value for nta_outgoing_prack(). @HI 
 *
 * @par Used with
 *    nta_outgoing_prack()
 *
 * @par Parameter type
 *    @c unsigned @c int
 *
 * @par Values
 *    Response sequence number from the @RSeq header.
*/
#define NTATAG_RSEQ(x)    ntatag_rseq, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_rseq_ref;
#define NTATAG_RSEQ_REF(x) ntatag_rseq_ref, tag_uint_vr(&(x))

/* ====================================================================== */
/* Tags for statistics. */

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash;
#define NTATAG_S_IRQ_HASH(x) ntatag_s_irq_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_ref;
/** Get size of hash table for server-side transactions.
 *
 * Return number of transactions that fit in the hash table for server-side
 * transactions.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_IRQ_HASH_USED_REF(),
 * NTATAG_S_ORQ_HASH_REFxs(), NTATAG_S_LEG_HASH_REF()
 */
#define NTATAG_S_IRQ_HASH_REF(x) ntatag_s_irq_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash;
#define NTATAG_S_ORQ_HASH(x) ntatag_s_orq_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_ref;
/** Get size of hash table for client-side transactions.
 *
 * Return number of transactions that fit in the hash table for client-side
 * transactions.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_ORQ_HASH_USED_REF(),
 * NTATAG_S_IRQ_HASH_REF(), NTATAG_S_LEG_HASH_REF()
 */
#define NTATAG_S_ORQ_HASH_REF(x) ntatag_s_orq_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash;
#define NTATAG_S_LEG_HASH(x) ntatag_s_leg_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_ref;
/** Get size of hash table for dialogs.
 *
 * Return number of dialog objects that fit in the hash table for dialogs.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_LEG_HASH_USED_REF(),
 * NTATAG_S_IRQ_HASH_REF(), NTATAG_S_ORQ_HASH_REF()
 */
#define NTATAG_S_LEG_HASH_REF(x) ntatag_s_leg_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_used;
#define NTATAG_S_IRQ_HASH_USED(x) ntatag_s_irq_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_used_ref;
/** Get number of server-side transactions in the hash table.
 *
 * Return number of server-side transactions objects in the hash table. The
 * number includes all transactions destroyed by the application which have
 * not expired yet.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_IRQ_HASH_REF(),
 * NTATAG_S_ORQ_HASH_USED_REF(), NTATAG_S_LEG_HASH_USED_REF()
 */
#define NTATAG_S_IRQ_HASH_USED_REF(x) \
ntatag_s_irq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used;
#define NTATAG_S_ORQ_HASH_USED(x) ntatag_s_orq_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used_ref;
/** Get number of client-side transactions in the hash table.
 *
 * Return number of client-side transactions objects in the hash table. The
 * number includes all transactions destroyed by the application which have
 * not expired yet.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_ORQ_HASH_REF(),
 * NTATAG_S_IRQ_HASH_USED_REF(), NTATAG_S_LEG_HASH_USED_REF()
 */
#define NTATAG_S_ORQ_HASH_USED_REF(x) \
ntatag_s_orq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used;
#define NTATAG_S_LEG_HASH_USED(x) ntatag_s_leg_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used_ref;
/** Get number of dialogs in the hash table.
 *
 * Return number of dialog objects in the hash table. Note that the
 * nta_leg_t objects created with NTATAG_NO_DIALOG(1) and this not
 * corresponding to a dialog are not included in the number.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_LEG_HASH_REF(),
 * NTATAG_S_IRQ_HASH_USED_REF(), NTATAG_S_ORQ_HASH_USED_REF()
 */
#define NTATAG_S_LEG_HASH_USED_REF(x) \
ntatag_s_leg_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg;
#define NTATAG_S_RECV_MSG(x) ntatag_s_recv_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg_ref;
/** Get number of SIP messages received.
 *
 * Return number SIP messages that has been received.  The number includes
 * also bad and unparsable messages.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_RECV_REQUEST_REF(), NTATAG_S_RECV_RESPONSE_REF()
 */
#define NTATAG_S_RECV_MSG_REF(x) ntatag_s_recv_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_request;
#define NTATAG_S_RECV_REQUEST(x) ntatag_s_recv_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_request_ref;
/** Get number of SIP requests received.
 *
 * Return number SIP requests that has been received. The number includes
 * also number of bad requests available with NTATAG_S_BAD_REQUEST_REF().
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_REQUEST_REF(),
 * NTATAG_S_RECV_MSG_REF(), NTATAG_S_RECV_RESPONSE_REF()
 */
#define NTATAG_S_RECV_REQUEST_REF(x)\
 ntatag_s_recv_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_response;
#define NTATAG_S_RECV_RESPONSE(x) ntatag_s_recv_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_response_ref;
/** Get number of SIP responses received.
 *
 * Return number SIP responses that has been received. The number includes
 * also number of bad and unusable responses available with
 * NTATAG_S_BAD_RESPONSE_REF().
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_RESPONSE_REF(),
 * NTATAG_S_RECV_MSG_REF(), NTATAG_S_RECV_REQUEST_REF()
 */
#define NTATAG_S_RECV_RESPONSE_REF(x)\
 ntatag_s_recv_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_message;
#define NTATAG_S_BAD_MESSAGE(x) ntatag_s_bad_message, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_message_ref;
/** Get number of bad SIP messages received.
 *
 * Return number of bad SIP messages that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_MSG_REF(), 
 * NTATAG_S_BAD_REQUEST_REF(), NTATAG_S_BAD_RESPONSE_REF().
 */
#define NTATAG_S_BAD_MESSAGE_REF(x)\
 ntatag_s_bad_message_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_request;
#define NTATAG_S_BAD_REQUEST(x) ntatag_s_bad_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_request_ref;
/** Get number of bad SIP requests received.
 *
 * Return number of bad SIP requests that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_BAD_RESPONSE_REF().
 */
#define NTATAG_S_BAD_REQUEST_REF(x)\
 ntatag_s_bad_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_response;
#define NTATAG_S_BAD_RESPONSE(x) ntatag_s_bad_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_response_ref;
/** Get number of bad SIP responses received.
 *
 * Return number of bad SIP responses that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_BAD_REQUEST_REF()
 */
#define NTATAG_S_BAD_RESPONSE_REF(x)\
 ntatag_s_bad_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_request;
#define NTATAG_S_DROP_REQUEST(x) ntatag_s_drop_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_request_ref;
/** Get number of SIP requests dropped.
 *
 * Return number of SIP requests that has been randomly dropped after
 * receiving them because of NTATAG_DEBUG_DROP_PROB() has been set.
 *
 * @sa nta_agent_get_stats(), NTATAG_DEBUG_DROP_PROB(),
 * NTATAG_S_DROP_RESPONSE_REF()
 * 
 * @note The value was not calculated before @VERSION_1_12_7.
 */
#define NTATAG_S_DROP_REQUEST_REF(x)\
 ntatag_s_drop_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_response;
#define NTATAG_S_DROP_RESPONSE(x) ntatag_s_drop_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_response_ref;
/** Get number of SIP responses dropped.
 *
 * Return number of SIP responses that has been randomly dropped after
 * receiving them because of NTATAG_DEBUG_DROP_PROB() has been set.
 *
 * @sa nta_agent_get_stats(), NTATAG_DEBUG_DROP_PROB(),
 * NTATAG_S_DROP_REQUEST_REF()
 * 
 * @note The value was not calculated before @VERSION_1_12_7.
 */
#define NTATAG_S_DROP_RESPONSE_REF(x)\
 ntatag_s_drop_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_client_tr;
#define NTATAG_S_CLIENT_TR(x) ntatag_s_client_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_client_tr_ref;
/** Get number of client transactions created.
 *
 * Return number of client transactions created. The number also includes
 * client transactions with which stack failed to send the request because
 * the DNS resolving failed or the transport failed.
 *
 * @note The number include stateless requests sent with nta_msg_tsend(),
 * too.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SENT_REQUEST_REF(),
 * NTATAG_S_SERVER_TR_REF().
 */
#define NTATAG_S_CLIENT_TR_REF(x)\
 ntatag_s_client_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_server_tr;
#define NTATAG_S_SERVER_TR(x) ntatag_s_server_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_server_tr_ref;
/** Get number of server transactions created.
 *
 * Return number of server transactions created. 
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_RESPONSE_REF(),
 * NTATAG_S_CLIENT_TR_REF(), NTATAG_S_DIALOG_TR_REF(),
 */
#define NTATAG_S_SERVER_TR_REF(x)\
 ntatag_s_server_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr;
#define NTATAG_S_DIALOG_TR(x) ntatag_s_dialog_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr_ref;
/** Get number of in-dialog server transactions created.
 *
 * Return number of in-dialog server transactions created. The number
 * includes only those transactions that were correlated with a dialog
 * object.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_CLIENT_TR_REF(), NTATAG_S_RECV_RESPONSE_REF().
 */
#define NTATAG_S_DIALOG_TR_REF(x)\
 ntatag_s_dialog_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr;
#define NTATAG_S_ACKED_TR(x) ntatag_s_acked_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr_ref;
/** Get number of server transactions that have received ACK.
 *
 * Return number of INVITE server transactions for which an ACK request has
 * been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_CANCELED_TR_REF()
 */
#define NTATAG_S_ACKED_TR_REF(x) ntatag_s_acked_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr;
#define NTATAG_S_CANCELED_TR(x) ntatag_s_canceled_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr_ref;
/** Get number of server transactions that have been CANCELed.
 *
 * Return number of server transactions for which an CANCEL request has been
 * received. Currently, the count includes only INVITE server transactions
 * that have been CANCELed.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_ACKED_TR_REF().
 */
#define NTATAG_S_CANCELED_TR_REF(x)  \
 ntatag_s_canceled_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_request;
#define NTATAG_S_TRLESS_REQUEST(x) ntatag_s_trless_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_request_ref;
/** Get number of requests that were processed stateless.
 *
 * Return number of received requests that were processed statelessly,
 * either with #nta_message_f message callback given with the
 * nta_agent_create() or, missing the callback, by returning a <i>501 Not
 * Implemented</i> response to the request.
 *
 * @sa nta_agent_get_stats(), <sofia-sip/nta_stateless.h>,
 * nta_agent_create(), #nta_message_f, NTATAG_S_TRLESS_TO_TR_REF(),
 * NTATAG_S_TRLESS_RESPONSE_REF()
 */
#define NTATAG_S_TRLESS_REQUEST_REF(x)\
 ntatag_s_trless_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr;
#define NTATAG_S_TRLESS_TO_TR(x) ntatag_s_trless_to_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr_ref;
/** Get number of requests converted to transactions by message callback.
 *
 * Return number of requests that were converted to a server transaction
 * with nta_incoming_create().
 *
 * @sa nta_agent_get_stats(), nta_incoming_create(), nta_agent_create(),
 * #nta_message_f, NTATAG_S_TRLESS_REQUEST_REF()
 */
#define NTATAG_S_TRLESS_TO_TR_REF(x)\
 ntatag_s_trless_to_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_response;
#define NTATAG_S_TRLESS_RESPONSE(x) ntatag_s_trless_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_response_ref;
/** Get number of responses without matching request.
 *
 * Return number of received responses for which no matching client
 * transaction was found. Such responses are processed either by the
 * client transaction created with nta_outgoing_default(), the
 * #nta_message_f message callback given to nta_agent_create(), or, missing
 * both the default client transaction and message callback, they are
 * silently discarded.
 *
 * The NTATAG_S_TRLESS_200_REF() counter counts those successful 2XX
 * responses to the INVITE without client transaction which are silently
 * discarded.
 *
 * @sa nta_agent_get_stats(), nta_outgoing_default(), nta_agent_create(),
 * <sofia-sip/nta_stateless.h>, #nta_message_f, nta_msg_ackbye(),
 * NTATAG_S_TRLESS_REQUEST_REF(), NTATAG_S_TRLESS_200_REF().
 */
#define NTATAG_S_TRLESS_RESPONSE_REF(x)\
 ntatag_s_trless_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_200;
#define NTATAG_S_TRLESS_200(x) ntatag_s_trless_200, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_200_ref;
/** Get number of successful responses missing INVITE client transaction.
 *
 * Return number of received 2XX responses to INVITE transaction for which
 * no matching client transaction was found nor which were processed by a
 * default client transaction created with nta_outgoing_default() or
 * #nta_message_f message callback given to nta_agent_create().
 *
 * @sa nta_agent_get_stats(), nta_outgoing_default(), nta_agent_create(),
 * <sofia-sip/nta_stateless.h>, #nta_message_f, nta_msg_ackbye(),
 * NTATAG_S_TRLESS_RESPONSE_REF().
 */
#define NTATAG_S_TRLESS_200_REF(x)\
 ntatag_s_trless_200_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_merged_request;
#define NTATAG_S_MERGED_REQUEST(x) ntatag_s_merged_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_merged_request_ref;
/** Get number of requests merged by UAS.
 *
 * Return number of requests for which UAS already has returned a response
 * and which were merged (that is, returned a <i>482 Request Merged</i>
 * response).
 *
 * @sa nta_agent_get_stats(), NTATAG_UA(1), @RFC3261 section 8.2.2.2
 */
#define NTATAG_S_MERGED_REQUEST_REF(x)\
 ntatag_s_merged_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg;
#define NTATAG_S_SENT_MSG(x) ntatag_s_sent_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg_ref;
/** Get number of SIP messages sent by stack.
 *
 * Return number of SIP messages given to the transport layer for
 * transmission by the SIP stack. The number includes also messages which
 * the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_SENT_RESPONSE_REF()
 */
#define NTATAG_S_SENT_MSG_REF(x)\
 ntatag_s_sent_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_request;
#define NTATAG_S_SENT_REQUEST(x) ntatag_s_sent_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_request_ref;
/** Get number of SIP requests sent by stack.
 *
 * Return number of SIP requests given to the transport layer for
 * transmission by the SIP stack. The number includes retransmissions and
 * messages which the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_REQUEST_REF(),
 * NTATAG_S_SENT_MSG_REF(), NTATAG_S_SENT_RESPONSE_REF()
 */
#define NTATAG_S_SENT_REQUEST_REF(x)\
 ntatag_s_sent_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_response;
#define NTATAG_S_SENT_RESPONSE(x) ntatag_s_sent_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_response_ref;
/** Get number of SIP responses sent by stack.
 *
 * Return number of SIP responses given to the transport layer for
 * transmission by the SIP stack. The number includes retransmissions and
 * messages which the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_RESPONSE_REF(),
 * NTATAG_S_SENT_MSG_REF(), NTATAG_S_SENT_REQUEST_REF()
 */
#define NTATAG_S_SENT_RESPONSE_REF(x)\
 ntatag_s_sent_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_request;
#define NTATAG_S_RETRY_REQUEST(x) ntatag_s_retry_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_request_ref;
/** Get number of SIP requests retransmitted by stack.
 *
 * Return number of SIP requests given to the transport layer for
 * retransmission by the SIP stack. The number includes messages which the
 * transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SENT_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_RETRY_RESPONSE_REF()
 */
#define NTATAG_S_RETRY_REQUEST_REF(x)\
 ntatag_s_retry_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_response;
#define NTATAG_S_RETRY_RESPONSE(x) ntatag_s_retry_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_response_ref;
/** Get number of SIP responses retransmitted by stack.
 *
 * Return number of SIP responses given to the transport layer for
 * retransmission by the SIP stack. The number includes messages which the
 * transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SENT_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_RETRY_REQUEST_REF()
 */
#define NTATAG_S_RETRY_RESPONSE_REF(x)\
 ntatag_s_retry_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry;
#define NTATAG_S_RECV_RETRY(x) ntatag_s_recv_retry, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry_ref;
/** Get number of retransmitted SIP requests received by stack.
 *
 * Return number of SIP requests received by the stack. This number only
 * includes retransmission for which a matching server transaction object
 * was found.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RETRY_REQUEST_REF().
 */
#define NTATAG_S_RECV_RETRY_REF(x)\
 ntatag_s_recv_retry_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_request;
#define NTATAG_S_TOUT_REQUEST(x) ntatag_s_tout_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_request_ref;
/** Get number of SIP client transactions that has timeout.
 *
 * Return number of SIP client transactions that has timeout.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_TOUT_RESPONSE_REF().
 */
#define NTATAG_S_TOUT_REQUEST_REF(x)\
 ntatag_s_tout_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_response;
#define NTATAG_S_TOUT_RESPONSE(x) ntatag_s_tout_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_response_ref;
/** Get number of SIP server transactions that has timeout.
 *
 * Return number of SIP server transactions that has timeout. The number
 * includes only the INVITE transactions for which the stack has received no
 * ACK requests.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_TOUT_REQUEST_REF().
 */
#define NTATAG_S_TOUT_RESPONSE_REF(x)\
 ntatag_s_tout_response_ref, tag_usize_vr(&(x))

SOFIA_END_DECLS

#endif /* !defined(nta_tag_h) */
