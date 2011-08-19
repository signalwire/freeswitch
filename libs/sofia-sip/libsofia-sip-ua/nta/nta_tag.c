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

/**@CFILE nta_tag.c
 * @brief Tags for Nokia SIP Transaction API
 *
 * @note This file is used to automatically generate
 * nta_tag_ref.c and nta_tag_dll.c
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jul 24 22:28:34 2001 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#define TAG_NAMESPACE "nta"

#include "sofia-sip/nta_tag.h"
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/sip_tag_class.h>
#include <sofia-sip/url_tag_class.h>

#include <sofia-sip/sip_protos.h>

tag_typedef_t ntatag_any = NSTAG_TYPEDEF(*);

/**@def NTATAG_MCLASS(x)
 *
 * Message class used by NTA.
 *
 * The nta can use a custom or extended parser created with
 * msg_mclass_clone().
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    pointer to #msg_mclass_t.
 *
 * @par Values
 *    - custom or extended parser created with msg_mclass_clone()
 *    - NULL - use default parser
 *
 * @par Default Value
 *    - Value returned by sip_default_mclass()
 *
 * @sa NTATAG_SIPFLAGS()
 */
tag_typedef_t ntatag_mclass = PTRTAG_TYPEDEF(mclass);

/**@def NTATAG_BAD_REQ_MASK(x)
 *
 * Mask for bad request messages.
 *
 * If an incoming request has erroneous headers matching with the mask, nta
 * automatically returns a 400 Bad Message response to them.
 *
 * If mask ~0U (all bits set) is specified, all requests with any bad header
 * are dropped. By default only the requests with bad headers essential for
 * request processing or proxying are dropped.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - bitwise or of enum #sip_bad_mask values
 *
 * @par Default Value
 * - <code>sip_mask_response | sip_mask_ua | sip_mask_100rel | </code><br>
 *   <code>sip_mask_events | sip_mask_timer | sip_mask_publish</code>
 * The following headers are considered essential by default:
 * - @ref sip_request \"request line\"", @From, @To, @CSeq, @CallID,
 *   @ContentLength, @Via, @ContentType, @ContentDisposition,
 *   @ContentEncoding, @Supported, @Contact, @Require, @RecordRoute, @RAck,
 *   @RSeq, @Event, @Expires, @SubscriptionState, @SessionExpires,
 *   @MinSE, @SIPETag, and @SIPIfMatch.
 *
 * @sa enum #sip_bad_mask, NTATAG_BAD_RESP_MASK()
 */
tag_typedef_t ntatag_bad_req_mask = UINTTAG_TYPEDEF(bad_req_mask);

/**@def NTATAG_BAD_RESP_MASK(x)
 *
 * Mask for bad response messages.
 *
 * If an incoming response has erroneous headers matching with the mask, nta
 * drops the response message.
 *
 * If mask ~0U (all bits set) is specified, all responses with any bad header
 * are dropped. By default only the responses with bad headers essential for
 * response processing or proxying are dropped.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - bitwise or of enum #sip_bad_mask values
 *
 * @sa enum #sip_bad_mask, NTATAG_BAD_REQ_MASK()
 *
 * @par Default Value
 * - <code>sip_mask_response | sip_mask_ua | sip_mask_100rel | </code><br>
 *   <code>sip_mask_events | sip_mask_timer | sip_mask_publish</code>
 * The following headers are considered essential by default:
 * - @ref sip_status \"status line\"", @From, @To, @CSeq, @CallID,
 *   @ContentLength, @Via, @ContentType, @ContentDisposition,
 *   @ContentEncoding, @Supported, @Contact, @Require, @RecordRoute, @RAck,
 *   @RSeq, @Event, @Expires, @SubscriptionState, @SessionExpires,
 *   @MinSE, @SIPETag, and @SIPIfMatch.
 */
tag_typedef_t ntatag_bad_resp_mask = UINTTAG_TYPEDEF(bad_resp_mask);

/**@def NTATAG_DEFAULT_PROXY(x)
 *
 * URL for (default) proxy.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par Parameter type
 *    Pointer to a url_t structure or a string containg a SIP or SIPS URI
 *
 * @par Values
 *    - Valid SIP or SIPS URI
 */
tag_typedef_t ntatag_default_proxy = URLTAG_TYPEDEF(default_proxy);

/**@def NTATAG_CONTACT(x)
 *
 * Contact used by NTA.
 */
tag_typedef_t ntatag_contact = SIPHDRTAG_NAMED_TYPEDEF(contact, contact);

/** @def NTATAG_TARGET(x)
 *
 * Dialog target (contact) used by NTA.
 */
tag_typedef_t ntatag_target = SIPHDRTAG_NAMED_TYPEDEF(target, contact);

/** @def NTATAG_ALIASES(x)
 *
 * Aliases used by NTA.
 * @deprecated
 */
tag_typedef_t ntatag_aliases = SIPHDRTAG_NAMED_TYPEDEF(aliases, contact);

/**@def NTATAG_METHOD(x)
 *
 * Method name.
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
 *    - A SIP method name (e.g., "SUBSCRIBE").
 *
 * @par Default Value
 *    - None (i.e., all requests methods match with the leg)
 *
 */
tag_typedef_t ntatag_method = STRTAG_TYPEDEF(method);

/**@def NTATAG_BRANCH_KEY(x)
 *
 * Branch ID to the topmost @Via header.
 *
 * The NTA generates a random branch ID for the topmost @Via header by default.
 * The application can the branch by itself, for intance, if it wants to
 * create a @RFC2543-era transaction.
 *
 * Note that according to @RFC3261 the branch ID must start with "z9hG4bK".
 *
 * @par Used with
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par Parameter type
 *    string
 *
 * @par Value
 * - The "branch" ID to to insert into topmost @Via header of the
 *   request to be sent
 *
 * @par Default Value
 *  - A token is generated, either by random when a client transaction is
 *    created or by hashing the headers and contents of the request when
 *    request is sent statelessly
 *
 * @sa @RFC3261 section 8.1.1.7
 */
tag_typedef_t ntatag_branch_key = STRTAG_TYPEDEF(branch_key);

/**@def NTATAG_ACK_BRANCH(x)
 *
 * Branch of the transaction to ACK.
 *
 * When creating a ACK transaction, the application should provide the
 * branch parameter from the original transaction to the stack. The ACK
 * transaction object then receives all the retransmitted 2XX responses to
 * the original INVITE transaction.
 *
 * @par Used with
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate()
 *
 * @par Parameter type
 *    string
 *
 * @par Value
 *    - "branch" ID used to store the ACK transaction in the nta hash
 *      table for outgoing client transaction
 *
 * @par Default Value
 *  - The INVITE transaction is looked from the hash table using the @CallID,
 *    @CSeq, @From and @To tags and its branch ID is used
 */
tag_typedef_t ntatag_ack_branch = STRTAG_TYPEDEF(ack_branch);

/**@def NTATAG_COMP(x)
 *
 * Compression algorithm.
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
 *    - name of the compression algorithm
 *
 * @par Default Value
 *    - "sigcomp"
 *
 * @sa @RFC3320, @RFC3486, TPTAG_COMPARTMENT(),
 * NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_AWARE(),
 * NTATAG_SIGCOMP_CLOSE(), NTATAG_SIGCOMP_OPTIONS()
 */
tag_typedef_t ntatag_comp = CSTRTAG_TYPEDEF(comp);

/**@def NTATAG_MSG(x)
 *
 * Pass a SIP message to treply()/tcreate() functions.
 *
 * @par Used with
 *    nta_outgoing_tcreate(), nta_incoming_treply()
 *
 * @par Parameter type
 *    #msg_t
 *
 * @par Values
 * - A message object which will be completed, serialized and encoded.
 *   Note that the functions modify directly the message.
 *
 * @par Default Value
 * - A new  message object is created and populated by the function call.
 *
 * @sa msg_copy(), msg_dup(), msg_create(), sip_default_mclass()
 */
tag_typedef_t ntatag_msg = PTRTAG_TYPEDEF(msg);

/**@def NTATAG_TPORT(x)
 *
 * Pass a transport object. The transport object is used to send the request
 * or response message(s).
 *
 * @par Used with
 *    nta_outgoing_tcreate(), nta_outgoing_mcreate(), nta_outgoing_tcancel(),
 *    nta_incoming_create(), nta_msg_tsend(), nta_msg_mreply()
 *
 * @par Parameter type
 *  - #tport_t
 *
 * @par Values
 * - A pointer to the transport object. Note that a new reference to the transport
 *   is created.
 *
 * @par Default Value
 * - The transport is selected by resolving the outbound URI (specified with
 *   NTATAG_DEFAULT_PROXY(), the topmost @Route URI or Request-URI.
 */
tag_typedef_t ntatag_tport = PTRTAG_TYPEDEF(tport);

/**@def NTATAG_SMIME(x)
 *
 * Provide S/MIME context to NTA.
 *
 * @todo S/MIME is not implemented.
 */
tag_typedef_t ntatag_smime = PTRTAG_TYPEDEF(smime);

/**@def NTATAG_REMOTE_CSEQ(x)
 *
 * Remote CSeq number.
 *
 * Specify remote command sequence number for a #nta_leg_t dialog object. If
 * an request is received matching with the dialog but with @CSeq number
 * less than the remote sequence number associated with the dialog, a <i>500
 * Internal Server Error</i> response is automatically returned to the client.
 *
 * @par Used with
 *   nta_leg_tcreate()
 *
 * @par Parameter type
 *   - uint32_t
 *
 * @par Values
 *    - Remote command sequence number
 *
 * @par Default Value
 *    - Initially 0, then determined by the received requests
 *
 */
tag_typedef_t ntatag_remote_cseq = UINTTAG_TYPEDEF(remote_cseq);

/**@def NTATAG_MAXSIZE(x)
 *
 * Maximum size of incoming message.
 *
 * If the size of an incoming request message would exceed the
 * given limit, the stack will automatically respond with <i>413 Request
 * Entity Too Large</i>.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    - #usize_t
 *
 * @par Values
 *    - Maximum acceptable size of an incoming request message.
 *
 * @par Default Value
 *    - 2097152 (bytes or 2 megabytes)
 *
 * @sa msg_maxsize(), NTATAG_UDP_MTU()
 */
tag_typedef_t ntatag_maxsize = USIZETAG_TYPEDEF(maxsize);

/**@def NTATAG_MAX_PROCEEDING(x)
 *
 * Maximum size of proceeding queue.
 *
 * If the size of the proceedng message queue would exceed the
 * given limit, the stack will automatically respond with <i>503
 * Service Unavailable</i>.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    - #usize_t
 *
 * @par Values
 *    - Maximum acceptable size of a queue (size_t).
 *
 * @NEW_1_12_9
 */
tag_typedef_t ntatag_max_proceeding = USIZETAG_TYPEDEF(max_proceeding);

/**@def NTATAG_UDP_MTU(x)
 *
 * Maximum size of outgoing UDP request.
 *
 * The maximum UDP request size is used to control use of UDP with overtly
 * large messages. The IETF requires that the SIP requests over 1300 bytes
 * are sent over congestion-controlled transport such as TCP. If a SIP
 * message size exceeds the UDP MTU, the TCP is tried instead of UDP. (If
 * the TCP connection is refused, the stack reverts back to UDP).
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    - unsigned
 *
 * @par Values
 *    - Maximum size of an outgoing UDP request
 *
 * @par Default Value
 *    - 1300 (bytes)
 *
 * @sa @RFC3261 section 18.1.1, NTATAG_MAXSIZE()
 */
tag_typedef_t ntatag_udp_mtu = UINTTAG_TYPEDEF(udp_mtu);

/**@def NTATAG_MAX_FORWARDS(x)
 *
 * Default value for @MaxForwards header.
 *
 * The default value of @MaxForwards header added to the requests. The
 * initial value recommended by @RFC3261 is 70, but usually SIP proxies use
 * much lower default value, such as 24.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    - Default value added to the @MaxForwards header in the sent requests
 *
 * @par Default Value
 *    - 70 (hops)
 *
 * @since New in @VERSION_1_12_2.
 */
tag_typedef_t ntatag_max_forwards = UINTTAG_TYPEDEF(max_forwards);

/**@def NTATAG_SIP_T1(x)
 *
 * Initial retransmission interval (in milliseconds)
 *
 * Set the T1 retransmission interval used by the SIP transaction engine. The
 * T1 is the initial duration used by request retransmission timers A and E
 * (UDP) as well as response retransmission timer G.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T1 in milliseconds
 *
 * @par Default Value
 *    - #NTA_SIP_T1 or 500 (milliseconds)
 *
 * @sa @RFC3261 appendix A, #NTA_SIP_T1, NTATAG_SIP_T1X4(), NTATAG_SIP_T1(), NTATAG_SIP_T4()
 */
tag_typedef_t ntatag_sip_t1 = UINTTAG_TYPEDEF(sip_t1);

/**@def NTATAG_SIP_T1X64(x)
 *
 * Transaction timeout (defaults to T1 * 64).
 *
 * Set the T1x64  timeout value used by the SIP transaction engine. The T1x64 is
 * duration used for timers B, F, H, and J (UDP) by the SIP transaction engine.
 * The timeout value T1x64 can be adjusted separately from the initial
 * retransmission interval T1, which is set with NTATAG_SIP_T1().
 *
 * The default value for T1x64 is 64 times value of T1, or 32000 milliseconds.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of T1x64 in milliseconds
 *
 * @par Default Value
 *    - 64 * #NTA_SIP_T1 or 32000 (milliseconds)
 *
 * @sa @RFC3261 appendix A, #NTA_SIP_T1, NTATAG_SIP_T1(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 *
 */
tag_typedef_t ntatag_sip_t1x64 = UINTTAG_TYPEDEF(sip_t1x64);

/**@def NTATAG_SIP_T2(x)
 *
 * Maximum retransmission interval (in milliseconds)
 *
 * Set the maximum retransmission interval used by the SIP transaction
 * engine. The T2 is the maximum duration used for the timers E (UDP) and G
 * by the SIP transaction engine. Note that the timer A is not capped by T2.
 * Retransmission interval of INVITE requests grows exponentially until the
 * timer B fires.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T2 in milliseconds
 *
 * @par Default Value
 *    - #NTA_SIP_T2 or 4000 (milliseconds)
 *
 * @sa @RFC3261 appendix A, #NTA_SIP_T2, NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T4()
 */
tag_typedef_t ntatag_sip_t2 = UINTTAG_TYPEDEF(sip_t2);

/**@def NTATAG_SIP_T4(x)
 *
 * Transaction lifetime (in milliseconds)
 *
 * Set the lifetime for completed transactions used by the SIP transaction
 * engine. A completed transaction is kept around for the duration of T4 in
 * order to catch late responses. The T4 is the maximum duration for the
 * messages to stay in the network and the duration of SIP timer K.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Value of SIP T4 in milliseconds
 *
 * @par Default Value
 *    - #NTA_SIP_T4 or 4000 (milliseconds)
 *
 * @sa @RFC3261 appendix A, #NTA_SIP_T4, NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2()
 */
tag_typedef_t ntatag_sip_t4 = UINTTAG_TYPEDEF(sip_t4);

/**@def NTATAG_PROGRESS(x)
 *
 * Progress timer for User-Agents (interval for retranmitting 1XXs).
 *
 * The UAS should retransmit preliminary responses to the INVITE
 * transactions every minute in order to re-set the timer C within the
 * intermediate proxies.
 *
 * The default value for the progress timer is 60000.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    Value of progress timer in milliseconds.
 *
 * @par Default Value
 *   - 90000 (milliseconds, 1.5 minutes)
 *
 * @sa @RFC3261 sections 13.3.1.1, 16.7 and 16.8, NTATAG_TIMER_C(),
 * NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 */
tag_typedef_t ntatag_progress = UINTTAG_TYPEDEF(progress);

/**@def NTATAG_TIMER_C(x)
 *
 * Value for timer C in milliseconds.
 *
 * By default the INVITE transaction will not timeout after a preliminary
 * response has been received. However, an intermediate proxy can timeout
 * the transaction using timer C. Timer C is reset every time a response
 * belonging to the transaction is received.
 *
 * The default value for the timer C is 185000 milliseconds (3 minutes and 5
 * seconds). By default, timer C is not run on user agents (if NTATAG_UA(1)
 * without NTATAG_TIMER_C() is given).
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    Value of SIP timer C in milliseconds. The default value is used
 *    instead if NTATAG_TIMER_C(0) is given.
 *
 * @par Default Value
 *   - 185000 (milliseconds, 3 minutes)
 *
 * @sa @RFC3261 sections 13.3.1.1, 16.7 and 16.8,
 * NTATAG_UA(1), NTATAG_TIMER_C(),
 * NTATAG_SIP_T1(), NTATAG_SIP_T1X4(), NTATAG_SIP_T2(), NTATAG_SIP_T4()
 *
 * @NEW_1_12_7.
 */
tag_typedef_t ntatag_timer_c = UINTTAG_TYPEDEF(timer_c);

/**@def NTATAG_GRAYLIST(x)
 *
 * Avoid failed servers.
 *
 * The NTATAG_GRAYLIST() provides the time that the servers are avoided
 * after a request sent to them has been failed. Avoiding means that if a
 * domain provides multiple servers, the failed servers are tried last.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *     unsigned int
 *
 * @par Values
 *    - Number of seconds that server is kept in graylist, from 0 to 86400.
 *
 * @par Default Value
 *    - 600 (graylist server for 10 minutes)
 *
 * @sa NTATAG_BLACKLIST(), NTATAG_TIMEOUT_408()
 *
 * @NEW_1_12_8
 */
tag_typedef_t ntatag_graylist = UINTTAG_TYPEDEF(graylist);

/**@def NTATAG_BLACKLIST(x)
 *
 * Add Retry-After header to error responses returned to application.
 *
 * The NTATAG_BLACKLIST() provides a default value for @RetryAfter header
 * added to the internally generated responses such as <i>503 DNS Error</i>
 * or <i>408 Timeout</i>. The idea is that the application can retain its
 * current state and retry the operation after a while.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *     unsigned int
 *
 * @par Values
 *    - Value of @a delta-seconds in @RetryAfter header, from 0 to 86400
 *
 * @par Default Value
 *    - 0 (no @RetryAfter header is included)
 *
 * @sa NTATAG_TIMEOUT_408()
 */
tag_typedef_t ntatag_blacklist = UINTTAG_TYPEDEF(blacklist);

/**@def NTATAG_DEBUG_DROP_PROB(x)
 *
 * Packet drop probability for debugging.
 *
 * The packet drop probability parameter is useful mainly for debugging
 * purposes. The stack drops an incoming message received over an unreliable
 * transport (such as UDP) with the given probability. The range is in 0 ..
 * 1000, 500 means p=0.5.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned integer
 *
 * @par Values
 *    - Valid values are in range 0 ... 1000
 *    - Probablity to drop a given message is value / 1000.
 *
 * @par Default Value
 *    - 0 (no packets are dropped)
 *
 * @sa TPTAG_DEBUG_DROP()
 */
tag_typedef_t ntatag_debug_drop_prob = UINTTAG_TYPEDEF(debug_drop_prob);

/**@def NTATAG_SIGCOMP_OPTIONS(x)
 *
 * Semicolon-separated SigComp options.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
 *    nta_agent_add_tport()
 *
 * @par Parameter type
 *    string
 *
 * @par Values
 *    - semicolon-separated parameter-value pairs, passed to the SigComp plugin
 *
 * @sa NTATAG_COMP(), NTATAG_SIGCOMP_ALGORITHM(), NTATAG_SIGCOMP_AWARE(),
 * NTATAG_SIGCOMP_CLOSE(), @RFC3320
 */
tag_typedef_t ntatag_sigcomp_options = STRTAG_TYPEDEF(sigcomp_options);

/**@def NTATAG_SIGCOMP_CLOSE(x)
 *
 * Close SigComp compartment after completing transaction.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nta_incoming_set_params(), nta_incoming_treply()
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tmcreate(), nta_outgoing_tcancel(),
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
 */
tag_typedef_t ntatag_sigcomp_close = BOOLTAG_TYPEDEF(sigcomp_close);

/**@def NTATAG_SIGCOMP_AWARE(x)
 *
 * Indicate that the application is SigComp-aware.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
 */
tag_typedef_t ntatag_sigcomp_aware = BOOLTAG_TYPEDEF(sigcomp_aware);

/**@def NTATAG_SIGCOMP_ALGORITHM(x)
 *
 * Specify SigComp algorithm.
 *
 * @note This tag is has no effect without a SigComp plugin.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
 *    nta_agent_add_tport()
 *
 * @par Parameter type
 *    string
 *
 * @par Values
 *    - opaque string passed to the SigComp plugin
 *
 * @sa NTATAG_COMP(), NTATAG_SIGCOMP_AWARE(), NTATAG_SIGCOMP_CLOSE(),
 * NTATAG_SIGCOMP_OPTIONS(), @RFC3320
 */
tag_typedef_t ntatag_sigcomp_algorithm = STRTAG_TYPEDEF(sigcomp_algorithm);

/**@def NTATAG_UA(x)
 *
 * If true, NTA acts as User Agent Server or Client by default.
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
 * @par Default Value
 *    - 0 (false)
 *
 * @sa NTATAG_MERGE_482()
 */
tag_typedef_t ntatag_ua = BOOLTAG_TYPEDEF(ua);

/**@def NTATAG_STATELESS(x)
 *
 * Enable stateless processing.
 *
 * @par Server side
 * The incoming requests are processed statefully if there is a default leg
 * (created with nta_leg_default()). This option is provided for proxies or
 * other server elements that process requests statelessly.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Values
 *    - true - do not pass incoming requests to default leg
 *    - false - pass incoming requests to default leg, if it exists
 *
 * @par Default Value
 *    - 0 (false,  pass incoming requests to default leg)
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
 * @par Default Value
 *    - 0 (false, create client transaction)
 *
 * @sa NTATAG_IS_UA(), nta_incoming_default(), nta_outgoing_default(),
 * nta_leg_default()
 */
tag_typedef_t ntatag_stateless = BOOLTAG_TYPEDEF(stateless);

/**@def NTATAG_USER_VIA(x)
 *
 * Allow application to insert Via headers.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
 *    nta_outgoing_mcreate(), nta_outgoing_tcreate(),
 *    nta_outgoing_tcancel(), nta_outgoing_prack(), nta_msg_tsend()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - do not add @Via header to the request (if it has one)
 *    - false - always add a @Via header
 *
 * @par Default Value
 *    - 0 (false, always add a @Via header)
 *
 * @sa NTATAG_BRANCH(), NTATAG_TPORT()
 */
tag_typedef_t ntatag_user_via = BOOLTAG_TYPEDEF(user_via);

/**@def NTATAG_PASS_100(x)
 *
 * Pass "<i>100 Trying</i>" provisional answers to the application.
 *
 * By default, the stack silently processes the <i>100 Trying</i> responses
 * from the server. Usually the <i>100 Trying</i> responses are not
 * important to the application but rather sent by the outgoing proxy
 * immediately after it has received the request. However, the application
 * can ask nta for them by setting NTATAG_PASS_100(1) if, for instance, the
 * <i>100 Trying</i> responses are needed for user feedback.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
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
 * @par Default Value
 *    - 0 (false, save application from seeing 100 Trying)
 *
 * @sa NTATAG_EXTRA_100(), NTATAG_DEFAULT_PROXY()
 */
tag_typedef_t ntatag_pass_100 = BOOLTAG_TYPEDEF(pass_100);

/**@def NTATAG_EXTRA_100(x)
 *
 * Respond with "100 Trying" if application has not responded.
 *
 * As per recommended by @RFC4320, the stack can generate a 100 Trying
 * response to the non-INVITE requests if the application has not responded
 * to a request within half of the SIP T2 (the default value for T2 is 4000
 * milliseconds, so the extra <i>100 Trying</i> would be sent after 2 seconds).
 *
 * At agent level, this option applies to retransmissions of both non-INVITE
 * and INVITE transactions.
 *
 * At incoming request level, this option can disable sending the 100 Trying for
 * both retransmissions (if set at agent level) and N1 firings, for just a given
 * incoming request.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params(),
 *    nta_incoming_set_params()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - send extra 100 Trying if application does not respond
 *    - false - do not send 100 Trying
 *
 * @par Default Value at Agent level
 *    - 0 (false, do not respond with 100 Trying to retransmissions)
 *
 * @par Default Value at incoming transaction level
 *    - 1 (true, respond with 100 Trying to retransmissions and when N1 fired)
 *
 * @sa @RFC4320, NTATAG_PASS_408(), NTATAG_TIMEOUT_408()
 */
tag_typedef_t ntatag_extra_100 = BOOLTAG_TYPEDEF(extra_100);

/**@def NTATAG_TIMEOUT_408(x)
 *
 * Generate "408 Request Timeout" response when request times out.
 *
 * This tag is used to prevent stack from generating extra 408 response
 * messages to non-INVITE requests upon timeout. As per recommended by
 * @RFC4320, the <i>408 Request Timeout</i> responses to non-INVITE
 * transaction are not sent over the network to the client by default. The
 * application can ask stack to pass the 408 responses with
 * NTATAG_PASS_408(1).
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_timeout_408 = BOOLTAG_TYPEDEF(timeout_408);

/**@def NTATAG_PASS_408(x)
 *
 * Pass "408 Request Timeout" responses to the client.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_pass_408 = BOOLTAG_TYPEDEF(pass_408);

/**@def NTATAG_MERGE_482(x)
 *
 * Merge requests, send 482 to other requests.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_merge_482 = BOOLTAG_TYPEDEF(merge_482);

/**@def NTATAG_CANCEL_2543(x)
 *
 *Follow @RFC2543 semantics with CANCEL.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
 */
tag_typedef_t ntatag_cancel_2543 = BOOLTAG_TYPEDEF(cancel_2543);

/**@def NTATAG_CANCEL_408(x)
 *
 * Do not send a CANCEL but just timeout the request.
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
tag_typedef_t ntatag_cancel_408 = BOOLTAG_TYPEDEF(cancel_408);

/**@def NTATAG_CANCEL_487(x)
 *
 * When a CANCEL is received, automatically return 487 response to original request.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
 */
tag_typedef_t ntatag_cancel_487 = BOOLTAG_TYPEDEF(cancel_487);

/**@def NTATAG_TAG_3261(x)
 *
 * When responding to requests, use unique tags.
 *
 * If set the UA would generate an unique @From/@To tag for all dialogs. If
 * unset UA would reuse same tag in order to make it easier to re-establish
 * dialog state after a reboot.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_tag_3261 = BOOLTAG_TYPEDEF(tag_3261);

/**@def NTATAG_REL100(x)
 *
 * Include rel100 in INVITE requests.
 *
 * Include feature tag "100rel" in @Supported header of the INVITE requests.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_rel100 = BOOLTAG_TYPEDEF(rel100);

/**@def NTATAG_NO_DIALOG(x)
 *
 * Create a leg without dialog. */
tag_typedef_t ntatag_no_dialog = BOOLTAG_TYPEDEF(no_dialog);

/**@def NTATAG_USE_TIMESTAMP(x)
 *
 * Use @Timestamp header.
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
tag_typedef_t ntatag_use_timestamp = BOOLTAG_TYPEDEF(use_timestamp);

/**@def NTATAG_SIPFLAGS(x)
 *
 * Set SIP parser flags.
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
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned int
 *
 * @par Values
 *    - Bitwise OR of SIP parser flags (enum #msg_flg_user)
 *
 * @sa NTATAG_PRELOAD(), enum #msg_flg_user, sip_s::sip_flags
 */
tag_typedef_t ntatag_sipflags = UINTTAG_TYPEDEF(sipflags);

/**@def NTATAG_CLIENT_RPORT(x)
 *
 * Enable client-side "rport".
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
 * @sa @RFC3581, NTATAG_SERVER_RPORT(), NTATAG_TCP_RPORT(), NTATAG_TLS_RPORT(), @Via
 */
tag_typedef_t ntatag_client_rport = BOOLTAG_TYPEDEF(client_rport);

/**@def NTATAG_SERVER_RPORT(x)
 *
 * Use rport parameter at server.
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
 * Note that on server-side the port number is stored regardless of the
 * transport protocol. (It is assumed that client supports rport if it
 * includes "rport" parameter in @Via field).
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - 2 - add "rport" parameter even if was not present in request
 *    - 1 - use "rport" parameter (default)
 *    - 0 - do not use "rport" parameter
 *
 * @sa @RFC3581, NTATAG_CLIENT_RPORT(), NTATAG_TCP_RPORT(), NTATAG_TLS_RPORT(), @Via
 *
 * @since Tag type and NTATAG_SERVER_RPORT(2) was added in @VERSION_1_12_9.
 */
tag_typedef_t ntatag_server_rport = INTTAG_TYPEDEF(server_rport);


/**@def NTATAG_TCP_RPORT(x)
 *
 * Use rport with TCP, too.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - include rport parameter in the TCP via line on client side
 *    - false - do not include rport parameter in the TCP via line on client side
 *
 * @sa @RFC3581, NTATAG_CLIENT_RPORT(), NTATAG_SERVER_RPORT(), @Via
 */
tag_typedef_t ntatag_tcp_rport = BOOLTAG_TYPEDEF(tcp_rport);

/**@def NTATAG_TLS_RPORT(x)
 *
 * Use rport with TLS, too.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - include rport parameter in the TLS via line on client side
 *    - false - do not include rport parameter in the TLS via line
 *      on client side
 *
 * @sa @RFC3581, NTATAG_CLIENT_RPORT(), NTATAG_SERVER_RPORT(), @Via
 *
 * @NEW_1_12_10
 */
tag_typedef_t ntatag_tls_rport = BOOLTAG_TYPEDEF(tls_rport);

/**@def NTATAG_PRELOAD(x)
 *
 * Preload by N bytes.
 *
 * When the memory block is allocated for an incoming request by the stack,
 * the stack can allocate some extra memory for the parser in addition to
 * the memory used by the actual message contents.
 *
 * While wasting some memory, this can speed up parsing considerably.
 * Recommended amount of preloading per packet is 1500 bytes.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    unsigned
 *
 * @par Values
 *    Amount of extra per-message memory allocated for parser.
 *
 * @sa NTATAG_SIPFLAGS() and #MSG_FLG_EXTRACT_COPY
 */
tag_typedef_t ntatag_preload = UINTTAG_TYPEDEF(preload);

/**@def NTATAG_USE_NAPTR(x)
 *
 * If true, try to use NAPTR records when resolving.
 *
 * The application can disable NTA from using NAPTR records when resolving
 * SIP URIs.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_use_naptr = BOOLTAG_TYPEDEF(naptr);

/**@def NTATAG_USE_SRV(x)
 *
 * If true, try to use SRV records when resolving.
 *
 * The application can disable NTA from using SRV records when resolving
 * SIP URIs.
 *
 * @par Used with
 *    nua_create(), nua_set_params(),
 *    nta_agent_create(), nta_agent_set_params()
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
tag_typedef_t ntatag_use_srv = BOOLTAG_TYPEDEF(srv);

/**@def NTATAG_SRV_503(x)
 *
 * If true, try to use another destination from SRV records on 503 response. RFC3263
 *
 * The application can disable NTA from using a new route after 503
 *
 * @par Used with
 *    nua_create(), nua_set_params(), agent_recv_response(),
 *    nta_agent_create(), nta_agent_set_params()
 *
 * @par Parameter type
 *    boolean: true (non-zero or non-NULL pointer)
 *          or false (zero or NULL pointer)
 *
 * @par Values
 *    - true - enable new destination on 503
 *    - false - still use the same destination after timeout
 *
 * @sa @RFC3263
 */
tag_typedef_t ntatag_srv_503 = BOOLTAG_TYPEDEF(srv_503);


/**@def NTATAG_RSEQ(x)
 *
 * @RSeq value for nta_outgoing_prack().
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
tag_typedef_t ntatag_rseq = UINTTAG_TYPEDEF(rseq);

/* Status */

/**@def NTATAG_S_IRQ_HASH_REF(x)
 *
 * Get size of hash table for server-side transactions.
 *
 * Return number of transactions that fit in the hash table for server-side
 * transactions.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_IRQ_HASH_USED_REF(),
 * NTATAG_S_ORQ_HASH_REFxs(), NTATAG_S_LEG_HASH_REF()
 */
tag_typedef_t ntatag_s_irq_hash =         USIZETAG_TYPEDEF(s_irq_hash);

/**@def NTATAG_S_ORQ_HASH_REF(x)
 *
 * Get size of hash table for client-side transactions.
 *
 * Return number of transactions that fit in the hash table for client-side
 * transactions.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_ORQ_HASH_USED_REF(),
 * NTATAG_S_IRQ_HASH_REF(), NTATAG_S_LEG_HASH_REF()
 */
tag_typedef_t ntatag_s_orq_hash =         USIZETAG_TYPEDEF(s_orq_hash);

/**@def NTATAG_S_LEG_HASH_REF(x)
 *
 * Get size of hash table for dialogs.
 *
 * Return number of dialog objects that fit in the hash table for dialogs.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_LEG_HASH_USED_REF(),
 * NTATAG_S_IRQ_HASH_REF(), NTATAG_S_ORQ_HASH_REF()
 */
tag_typedef_t ntatag_s_leg_hash =         USIZETAG_TYPEDEF(s_leg_hash);

/**@def NTATAG_S_IRQ_HASH_USED_REF(x)
 *
 * Get number of server-side transactions in the hash table.
 *
 * Return number of server-side transactions objects in the hash table. The
 * number includes all transactions destroyed by the application which have
 * not expired yet.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_IRQ_HASH_REF(),
 * NTATAG_S_ORQ_HASH_USED_REF(), NTATAG_S_LEG_HASH_USED_REF()
 */
/**@def NTATAG_S_IRQ_HASH_USED_REF(x)
 *
 * Get number of server-side transactions in the hash table.
 *
 * Return number of server-side transactions objects in the hash table. The
 * number includes all transactions destroyed by the application which have
 * not expired yet.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_IRQ_HASH_REF(),
 * NTATAG_S_ORQ_HASH_USED_REF(), NTATAG_S_LEG_HASH_USED_REF()
 */
tag_typedef_t ntatag_s_irq_hash_used =    USIZETAG_TYPEDEF(s_irq_hash_used);

/**@def NTATAG_S_ORQ_HASH_USED_REF(x)
 *
 * Get number of client-side transactions in the hash table.
 *
 * Return number of client-side transactions objects in the hash table. The
 * number includes all transactions destroyed by the application which have
 * not expired yet.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_ORQ_HASH_REF(),
 * NTATAG_S_IRQ_HASH_USED_REF(), NTATAG_S_LEG_HASH_USED_REF()
 */
tag_typedef_t ntatag_s_orq_hash_used =    USIZETAG_TYPEDEF(s_orq_hash_used);

/**@def NTATAG_S_LEG_HASH_USED_REF(x)
 *
 * Get number of dialogs in the hash table.
 *
 * Return number of dialog objects in the hash table. Note that the
 * nta_leg_t objects created with NTATAG_NO_DIALOG(1) and this not
 * corresponding to a dialog are not included in the number.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_LEG_HASH_REF(),
 * NTATAG_S_IRQ_HASH_USED_REF(), NTATAG_S_ORQ_HASH_USED_REF()
 */
tag_typedef_t ntatag_s_leg_hash_used =    USIZETAG_TYPEDEF(s_leg_hash_used);

/**@def NTATAG_S_RECV_MSG_REF(x)
 *
 * Get number of SIP messages received.
 *
 * Return number SIP messages that has been received.  The number includes
 * also bad and unparsable messages.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_RECV_REQUEST_REF(), NTATAG_S_RECV_RESPONSE_REF()
 */
tag_typedef_t ntatag_s_recv_msg =         USIZETAG_TYPEDEF(s_recv_msg);

/**@def NTATAG_S_RECV_REQUEST_REF(x)
 *
 * Get number of SIP requests received.
 *
 * Return number SIP requests that has been received. The number includes
 * also number of bad requests available with NTATAG_S_BAD_REQUEST_REF().
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_REQUEST_REF(),
 * NTATAG_S_RECV_MSG_REF(), NTATAG_S_RECV_RESPONSE_REF()
 */
tag_typedef_t ntatag_s_recv_request =     USIZETAG_TYPEDEF(s_recv_request);

/**@def NTATAG_S_RECV_RESPONSE_REF(x)
 *
 * Get number of SIP responses received.
 *
 * Return number SIP responses that has been received. The number includes
 * also number of bad and unusable responses available with
 * NTATAG_S_BAD_RESPONSE_REF().
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_RESPONSE_REF(),
 * NTATAG_S_RECV_MSG_REF(), NTATAG_S_RECV_REQUEST_REF()
 */
tag_typedef_t ntatag_s_recv_response =    USIZETAG_TYPEDEF(s_recv_response);

/**@def NTATAG_S_BAD_MESSAGE_REF(x)
 *
 * Get number of bad SIP messages received.
 *
 * Return number of bad SIP messages that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_MSG_REF(),
 * NTATAG_S_BAD_REQUEST_REF(), NTATAG_S_BAD_RESPONSE_REF().
 */
tag_typedef_t ntatag_s_bad_message =      USIZETAG_TYPEDEF(s_bad_message);

/**@def NTATAG_S_BAD_REQUEST_REF(x)
 *
 * Get number of bad SIP requests received.
 *
 * Return number of bad SIP requests that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_BAD_RESPONSE_REF().
 */
tag_typedef_t ntatag_s_bad_request =      USIZETAG_TYPEDEF(s_bad_request);

/**@def NTATAG_S_BAD_RESPONSE_REF(x)
 *
 * Get number of bad SIP responses received.
 *
 * Return number of bad SIP responses that has been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_BAD_MESSAGE_REF(),
 * NTATAG_S_BAD_REQUEST_REF()
 */
tag_typedef_t ntatag_s_bad_response =     USIZETAG_TYPEDEF(s_bad_response);

/**@def NTATAG_S_DROP_REQUEST_REF(x)
 *
 * Get number of SIP requests dropped.
 *
 * Return number of SIP requests that has been randomly dropped after
 * receiving them because of NTATAG_DEBUG_DROP_PROB() has been set.
 *
 * @sa nta_agent_get_stats(), NTATAG_DEBUG_DROP_PROB(),
 * NTATAG_S_DROP_RESPONSE_REF()
 *
 * @note The value was not calculated before @VERSION_1_12_7.
 */
tag_typedef_t ntatag_s_drop_request =     USIZETAG_TYPEDEF(s_drop_request);

/**@def NTATAG_S_DROP_RESPONSE_REF(x)
 *
 * Get number of SIP responses dropped.
 *
 * Return number of SIP responses that has been randomly dropped after
 * receiving them because of NTATAG_DEBUG_DROP_PROB() has been set.
 *
 * @sa nta_agent_get_stats(), NTATAG_DEBUG_DROP_PROB(),
 * NTATAG_S_DROP_REQUEST_REF()
 *
 * @note The value was not calculated before @VERSION_1_12_7.
 */
tag_typedef_t ntatag_s_drop_response =    USIZETAG_TYPEDEF(s_drop_response);

/**@def NTATAG_S_CLIENT_TR_REF(x)
 *
 * Get number of client transactions created.
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
tag_typedef_t ntatag_s_client_tr =        USIZETAG_TYPEDEF(s_client_tr);

/**@def NTATAG_S_SERVER_TR_REF(x)
 *
 * Get number of server transactions created.
 *
 * Return number of server transactions created.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_RESPONSE_REF(),
 * NTATAG_S_CLIENT_TR_REF(), NTATAG_S_DIALOG_TR_REF(),
 */
tag_typedef_t ntatag_s_server_tr =        USIZETAG_TYPEDEF(s_server_tr);

/**@def NTATAG_S_DIALOG_TR_REF(x)
 *
 * Get number of in-dialog server transactions created.
 *
 * Return number of in-dialog server transactions created. The number
 * includes only those transactions that were correlated with a dialog
 * object.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_CLIENT_TR_REF(), NTATAG_S_RECV_RESPONSE_REF().
 */
tag_typedef_t ntatag_s_dialog_tr =        USIZETAG_TYPEDEF(s_dialog_tr);

/**@def NTATAG_S_ACKED_TR_REF(x)
 *
 * Get number of server transactions that have received ACK.
 *
 * Return number of INVITE server transactions for which an ACK request has
 * been received.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_CANCELED_TR_REF()
 */
tag_typedef_t ntatag_s_acked_tr =         USIZETAG_TYPEDEF(s_acked_tr);

/**@def NTATAG_S_CANCELED_TR_REF(x)
 *
 * Get number of server transactions that have been CANCELed.
 *
 * Return number of server transactions for which an CANCEL request has been
 * received. Currently, the count includes only INVITE server transactions
 * that have been CANCELed.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SERVER_TR_REF(),
 * NTATAG_S_ACKED_TR_REF().
 */
tag_typedef_t ntatag_s_canceled_tr =      USIZETAG_TYPEDEF(s_canceled_tr);

/**@def NTATAG_S_TRLESS_REQUEST_REF(x)
 *
 * Get number of requests that were processed stateless.
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
tag_typedef_t ntatag_s_trless_request =   USIZETAG_TYPEDEF(s_trless_request);

/**@def NTATAG_S_TRLESS_TO_TR_REF(x)
 *
 * Get number of requests converted to transactions by message callback.
 *
 * Return number of requests that were converted to a server transaction
 * with nta_incoming_create().
 *
 * @sa nta_agent_get_stats(), nta_incoming_create(), nta_agent_create(),
 * #nta_message_f, NTATAG_S_TRLESS_REQUEST_REF()
 */
tag_typedef_t ntatag_s_trless_to_tr =     USIZETAG_TYPEDEF(s_trless_to_tr);

/**@def NTATAG_S_TRLESS_RESPONSE_REF(x)
 *
 * Get number of responses without matching request.
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
tag_typedef_t ntatag_s_trless_response =  USIZETAG_TYPEDEF(s_trless_response);

/**@def NTATAG_S_TRLESS_200_REF(x)
 *
 * Get number of successful responses missing INVITE client transaction.
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
tag_typedef_t ntatag_s_trless_200 =       USIZETAG_TYPEDEF(s_trless_200);

/**@def NTATAG_S_MERGED_REQUEST_REF(x)
 *
 * Get number of requests merged by UAS.
 *
 * Return number of requests for which UAS already has returned a response
 * and which were merged (that is, returned a <i>482 Request Merged</i>
 * response).
 *
 * @sa nta_agent_get_stats(), NTATAG_UA(1), @RFC3261 section 8.2.2.2
 */
tag_typedef_t ntatag_s_merged_request =   USIZETAG_TYPEDEF(s_merged_request);

/**@def NTATAG_S_SENT_MSG_REF(x)
 *
 * Get number of SIP messages sent by stack.
 *
 * Return number of SIP messages given to the transport layer for
 * transmission by the SIP stack. The number includes also messages which
 * the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_SENT_RESPONSE_REF()
 */
tag_typedef_t ntatag_s_sent_msg =      	  USIZETAG_TYPEDEF(s_sent_msg);

/**@def NTATAG_S_SENT_REQUEST_REF(x)
 *
 * Get number of SIP requests sent by stack.
 *
 * Return number of SIP requests given to the transport layer for
 * transmission by the SIP stack. The number includes retransmissions and
 * messages which the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_REQUEST_REF(),
 * NTATAG_S_SENT_MSG_REF(), NTATAG_S_SENT_RESPONSE_REF()
 */
tag_typedef_t ntatag_s_sent_request =  	  USIZETAG_TYPEDEF(s_sent_request);

/**@def NTATAG_S_SENT_RESPONSE_REF(x)
 *
 * Get number of SIP responses sent by stack.
 *
 * Return number of SIP responses given to the transport layer for
 * transmission by the SIP stack. The number includes retransmissions and
 * messages which the transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RECV_RESPONSE_REF(),
 * NTATAG_S_SENT_MSG_REF(), NTATAG_S_SENT_REQUEST_REF()
 */
tag_typedef_t ntatag_s_sent_response = 	  USIZETAG_TYPEDEF(s_sent_response);

/**@def NTATAG_S_RETRY_REQUEST_REF(x)
 *
 * Get number of SIP requests retransmitted by stack.
 *
 * Return number of SIP requests given to the transport layer for
 * retransmission by the SIP stack. The number includes messages which the
 * transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SENT_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_RETRY_RESPONSE_REF()
 */
tag_typedef_t ntatag_s_retry_request = 	  USIZETAG_TYPEDEF(s_retry_request);

/**@def NTATAG_S_RETRY_RESPONSE_REF(x)
 *
 * Get number of SIP responses retransmitted by stack.
 *
 * Return number of SIP responses given to the transport layer for
 * retransmission by the SIP stack. The number includes messages which the
 * transport layer failed to send for different reasons.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_SENT_MSG_REF(),
 * NTATAG_S_SENT_REQUEST_REF(), NTATAG_S_RETRY_REQUEST_REF()
 */
tag_typedef_t ntatag_s_retry_response =   USIZETAG_TYPEDEF(s_retry_response);

/**@def NTATAG_S_RECV_RETRY_REF(x)
 *
 * Get number of retransmitted SIP requests received by stack.
 *
 * Return number of SIP requests received by the stack. This number only
 * includes retransmission for which a matching server transaction object
 * was found.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_RETRY_REQUEST_REF().
 */
tag_typedef_t ntatag_s_recv_retry =       USIZETAG_TYPEDEF(s_recv_retry);

/**@def NTATAG_S_TOUT_REQUEST_REF(x)
 *
 * Get number of SIP client transactions that has timeout.
 *
 * Return number of SIP client transactions that has timeout.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_TOUT_RESPONSE_REF().
 */
tag_typedef_t ntatag_s_tout_request =     USIZETAG_TYPEDEF(s_tout_request);

/**@def NTATAG_S_TOUT_RESPONSE_REF(x)
 *
 * Get number of SIP server transactions that has timeout.
 *
 * Return number of SIP server transactions that has timeout. The number
 * includes only the INVITE transactions for which the stack has received no
 * ACK requests.
 *
 * @sa nta_agent_get_stats(), NTATAG_S_TOUT_REQUEST_REF().
 */
tag_typedef_t ntatag_s_tout_response =    USIZETAG_TYPEDEF(s_tout_response);

/* Internal */
tag_typedef_t ntatag_delay_sending = BOOLTAG_TYPEDEF(delay_sending);
tag_typedef_t ntatag_incomplete = BOOLTAG_TYPEDEF(incomplete);
