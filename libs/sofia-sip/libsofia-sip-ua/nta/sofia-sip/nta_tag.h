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
/** Message class used by NTA. @HI */
#define NTATAG_MCLASS(x) ntatag_mclass, tag_cptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_mclass_ref;
#define NTATAG_MCLASS_REF(x) ntatag_mclass_ref, tag_cptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask;
/** Mask for bad request messages. 
 * 
 * If an incoming request has erroneous headers matching with the mask, nta
 * automatically returns a 400 Bad Message response to them. If no mask is
 * specified, all requests with any bad header are dropped.
 * 
 */
#define NTATAG_BAD_REQ_MASK(x) ntatag_bad_req_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask_ref;
#define NTATAG_BAD_REQ_MASK_REF(x) ntatag_bad_req_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask;
/** Mask for bad response messages. 
 * 
 * If an incoming response has erroneous headers matching with the mask, nta
 * drops the response message. If no mask is specified, all responses with
 * any bad header are dropped.
 */
#define NTATAG_BAD_RESP_MASK(x) ntatag_bad_resp_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask_ref;
#define NTATAG_BAD_RESP_MASK_REF(x) ntatag_bad_resp_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy;
/** URL for (default) proxy. @HI */
#define NTATAG_DEFAULT_PROXY(x) \
ntatag_default_proxy, urltag_url_v((x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy_ref;
#define NTATAG_DEFAULT_PROXY_REF(x) \
ntatag_default_proxy_ref, urltag_url_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_contact;
/** Contact used by NTA. @HI */
#define NTATAG_CONTACT(x) \
ntatag_contact, siptag_contact_v((x))

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
#define NTATAG_ACK_BRANCH(x) \
ntatag_ack_branch, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_ack_branch_ref;
#define NTATAG_ACK_BRANCH_REF(x) \
ntatag_ack_branch_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_comp;
/** Compression algorithm. @HI */
#define NTATAG_COMP(x) \
ntatag_comp, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_comp_ref;
#define NTATAG_COMP_REF(x) \
ntatag_comp_ref, tag_str_vr(&(x))

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
/** Maximum size of incoming message. @HI */
#define NTATAG_MAXSIZE(x) ntatag_maxsize, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_maxsize_ref;
#define NTATAG_MAXSIZE_REF(x) ntatag_maxsize_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu;
/** Maximum size of outgoing UDP request. @HI */
#define NTATAG_UDP_MTU(x) ntatag_udp_mtu, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu_ref;
#define NTATAG_UDP_MTU_REF(x) ntatag_udp_mtu_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards;
/** Default value for @MaxForwards header. 
 *
 * @since New in @VERSION_1_12_2.
 * @hideinitializer 
 */
#define NTATAG_MAX_FORWARDS(x) ntatag_max_forwards, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards_ref;
#define NTATAG_MAX_FORWARDS_REF(x) ntatag_max_forwards_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1;
/** Initial retransmission interval (in milliseconds) @HI */
#define NTATAG_SIP_T1(x) ntatag_sip_t1, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1_ref;
#define NTATAG_SIP_T1_REF(x) ntatag_sip_t1_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64;
/** Transaction timeout (defaults to T1 * 64). @HI */
#define NTATAG_SIP_T1X64(x) ntatag_sip_t1x64, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64_ref;
#define NTATAG_SIP_T1X64_REF(x) ntatag_sip_t1x64_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2;
/** Maximum retransmission interval (in milliseconds) @HI */
#define NTATAG_SIP_T2(x) ntatag_sip_t2, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2_ref;
#define NTATAG_SIP_T2_REF(x) ntatag_sip_t2_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4;
/** Transaction lifetime (in milliseconds) @HI */
#define NTATAG_SIP_T4(x)    ntatag_sip_t4, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4_ref;
#define NTATAG_SIP_T4_REF(x) ntatag_sip_t4_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_progress;
/** Progress timer for User-Agents (interval for retranmitting 1XXs) @HI */
#define NTATAG_PROGRESS(x)    ntatag_progress, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_progress_ref;
#define NTATAG_PROGRESS_REF(x) ntatag_progress_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_blacklist;
/** Add Retry-After header to internally-generated error messages. @HI */
#define NTATAG_BLACKLIST(x)  ntatag_blacklist, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_blacklist_ref;
#define NTATAG_BLACKLIST_REF(x) ntatag_blacklist_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob;
/** Packet drop probability for debugging. 
 *
 * The packet drop probability parameter is useful mainly in proxies for
 * debugging purposes. The stack drops an incoming message with the given
 * probability. The range is in 0 .. 1000, 500 means p=0.5.
 @HI */
#define NTATAG_DEBUG_DROP_PROB(x) ntatag_debug_drop_prob, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob_ref;
#define NTATAG_DEBUG_DROP_PROB_REF(x) ntatag_debug_drop_prob_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options;
/** Semicolon-separate SigComp options. @HI */
#define NTATAG_SIGCOMP_OPTIONS(x)    ntatag_sigcomp_options, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options_ref;
#define NTATAG_SIGCOMP_OPTIONS_REF(x) ntatag_sigcomp_options_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close;
/** Close SigComp compartment after completing transaction. @HI */
#define NTATAG_SIGCOMP_CLOSE(x)  ntatag_sigcomp_close, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close_ref;
#define NTATAG_SIGCOMP_CLOSE_REF(x) ntatag_sigcomp_close_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware;
/** Indicate that the application is SigComp-aware. */
#define NTATAG_SIGCOMP_AWARE(x) ntatag_sigcomp_aware, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware_ref;
#define NTATAG_SIGCOMP_AWARE_REF(x) ntatag_sigcomp_aware_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm;
/** Specify SigComp algorithm. For example, NULL, LZSS, or LZSS-POC.
 */
#define NTATAG_SIGCOMP_ALGORITHM(x) ntatag_sigcomp_algorithm, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm_ref;
#define NTATAG_SIGCOMP_ALGORITHM_REF(x) \
ntatag_sigcomp_algorithm_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_ua;
/** If true, NTA acts as User Agent Server or Client by default. @HI */
#define NTATAG_UA(x) ntatag_ua, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_ua_ref;
#define NTATAG_UA_REF(x) ntatag_ua_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_stateless;
/** If true, agent processes incoming requests statelessly by default. @HI */
#define NTATAG_STATELESS(x) ntatag_stateless, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_stateless_ref;
#define NTATAG_STATELESS_REF(x) ntatag_stateless_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_user_via;
/** Allow application to insert Via headers. @HI */
#define NTATAG_USER_VIA(x) ntatag_user_via, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_user_via_ref;
#define NTATAG_USER_VIA_REF(x) ntatag_user_via_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_extra_100;
/** Respond with "100 Trying" if application has not responded. @HI */
#define NTATAG_EXTRA_100(x)    ntatag_extra_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_extra_100_ref;
#define NTATAG_EXTRA_100_REF(x) ntatag_extra_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_100;
/** Pass "100 Trying" provisional answers to the application. @HI */
#define NTATAG_PASS_100(x) ntatag_pass_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_100_ref;
#define NTATAG_PASS_100_REF(x) ntatag_pass_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408;
/** Generate "408 Request Timeout" response when request times out. @HI */
#define NTATAG_TIMEOUT_408(x)  ntatag_timeout_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408_ref;
#define NTATAG_TIMEOUT_408_REF(x) ntatag_timeout_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_408;
/** Pass "408 Request Timeout" responses to client. @HI */
#define NTATAG_PASS_408(x)  ntatag_pass_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_408_ref;
#define NTATAG_PASS_408_REF(x) ntatag_pass_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog;
/** Create a leg without dialog. @HI */
#define NTATAG_NO_DIALOG(x)       ntatag_no_dialog, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog_ref;
#define NTATAG_NO_DIALOG_REF(x)   ntatag_no_dialog_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_merge_482;
/** Merge requests, send 482 to other requests. @HI */
#define NTATAG_MERGE_482(x)       ntatag_merge_482, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_merge_482_ref;
#define NTATAG_MERGE_482_REF(x)   ntatag_merge_482_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543;
/** Send a CANCEL to an INVITE without an provisional response. @HI */
#define NTATAG_CANCEL_2543(x)     ntatag_cancel_2543, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543_ref;
#define NTATAG_CANCEL_2543_REF(x) ntatag_cancel_2543_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408;
/** Do not send a CANCEL but just timeout the request. @HI */
#define NTATAG_CANCEL_408(x)     ntatag_cancel_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408_ref;
#define NTATAG_CANCEL_408_REF(x) ntatag_cancel_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261;
/** When responding to requests, use unique tags. @HI */
#define NTATAG_TAG_3261(x)        ntatag_tag_3261, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261_ref;
#define NTATAG_TAG_3261_REF(x)    ntatag_tag_3261_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp;
/** Use Timestamp header. @HI */
#define NTATAG_USE_TIMESTAMP(x) ntatag_use_timestamp, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp_ref;
#define NTATAG_USE_TIMESTAMP_REF(x) ntatag_use_timestamp_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_method;
/** Method name. @HI */
#define NTATAG_METHOD(x)          ntatag_method, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_method_ref;
#define NTATAG_METHOD_REF(x)      ntatag_method_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487;
/** When a CANCEL is received, reply with 487 response. True by default. @HI */
#define NTATAG_CANCEL_487(x)     ntatag_cancel_487, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487_ref;
#define NTATAG_CANCEL_487_REF(x) ntatag_cancel_487_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rel100;
/** Include rel100 in INVITE requests. @HI */
#define NTATAG_REL100(x)     ntatag_rel100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_rel100_ref;
#define NTATAG_REL100_REF(x) ntatag_rel100_ref, tag_bool_vr(&(x))
 
NTA_DLL extern tag_typedef_t ntatag_sipflags;
/** Set SIP parser flags. @HI */
#define NTATAG_SIPFLAGS(x)     ntatag_sipflags, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sipflags_ref;
#define NTATAG_SIPFLAGS_REF(x) ntatag_sipflags_ref, tag_uint_vr(&(x))
 
NTA_DLL extern tag_typedef_t ntatag_client_rport;
/** Add rport at client. @HI */
#define NTATAG_CLIENT_RPORT(x) ntatag_client_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_client_rport_ref;
#define NTATAG_CLIENT_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

#define NTATAG_RPORT(x) ntatag_client_rport, tag_bool_v((x))
#define NTATAG_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_server_rport;
/** Use rport at server. @HI */
#define NTATAG_SERVER_RPORT(x) ntatag_server_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_server_rport_ref;
#define NTATAG_SERVER_RPORT_REF(x) ntatag_server_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport;
/** Use rport with TCP, too. @HI */
#define NTATAG_TCP_RPORT(x) ntatag_tcp_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport_ref;
#define NTATAG_TCP_RPORT_REF(x) ntatag_tcp_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_preload;
/** Preload by N bytes. @HI */
#define NTATAG_PRELOAD(x) ntatag_preload, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_preload_ref;
#define NTATAG_PRELOAD_REF(x) ntatag_preload_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr;
/** If true, try to use NAPTR records when resolving. @HI */
#define NTATAG_USE_NAPTR(x) ntatag_use_naptr, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr_ref;
#define NTATAG_USE_NAPTR_REF(x) ntatag_use_naptr_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_srv;
/** If true, try to use SRV records when resolving. @HI */
#define NTATAG_USE_SRV(x) ntatag_use_srv, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_srv_ref;
#define NTATAG_USE_SRV_REF(x) ntatag_use_srv_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rseq;
/** RSeq value for nta_outgoing_prack(), @HI */
#define NTATAG_RSEQ(x)    ntatag_rseq, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_rseq_ref;
#define NTATAG_RSEQ_REF(x) ntatag_rseq_ref, tag_uint_vr(&(x))

/* ====================================================================== */
/* Tags for statistics. */

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash;
#define NTATAG_S_IRQ_HASH(x) ntatag_s_irq_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_ref;
#define NTATAG_S_IRQ_HASH_REF(x) ntatag_s_irq_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash;
#define NTATAG_S_ORQ_HASH(x) ntatag_s_orq_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_ref;
#define NTATAG_S_ORQ_HASH_REF(x) ntatag_s_orq_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash;
#define NTATAG_S_LEG_HASH(x) ntatag_s_leg_hash, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_ref;
#define NTATAG_S_LEG_HASH_REF(x) ntatag_s_leg_hash_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_used;
#define NTATAG_S_IRQ_HASH_USED(x) ntatag_s_irq_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_irq_hash_used_ref;
#define NTATAG_S_IRQ_HASH_USED_REF(x) \
ntatag_s_irq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used;
#define NTATAG_S_ORQ_HASH_USED(x) ntatag_s_orq_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used_ref;
#define NTATAG_S_ORQ_HASH_USED_REF(x) \
ntatag_s_orq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used;
#define NTATAG_S_LEG_HASH_USED(x) ntatag_s_leg_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used_ref;
#define NTATAG_S_LEG_HASH_USED_REF(x) \
ntatag_s_leg_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg;
#define NTATAG_S_RECV_MSG(x) ntatag_s_recv_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg_ref;
#define NTATAG_S_RECV_MSG_REF(x) ntatag_s_recv_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_request;
#define NTATAG_S_RECV_REQUEST(x) ntatag_s_recv_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_request_ref;
#define NTATAG_S_RECV_REQUEST_REF(x)\
 ntatag_s_recv_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_response;
#define NTATAG_S_RECV_RESPONSE(x) ntatag_s_recv_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_response_ref;
#define NTATAG_S_RECV_RESPONSE_REF(x)\
 ntatag_s_recv_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_message;
#define NTATAG_S_BAD_MESSAGE(x) ntatag_s_bad_message, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_message_ref;
#define NTATAG_S_BAD_MESSAGE_REF(x)\
 ntatag_s_bad_message_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_request;
#define NTATAG_S_BAD_REQUEST(x) ntatag_s_bad_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_request_ref;
#define NTATAG_S_BAD_REQUEST_REF(x)\
 ntatag_s_bad_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_response;
#define NTATAG_S_BAD_RESPONSE(x) ntatag_s_bad_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_response_ref;
#define NTATAG_S_BAD_RESPONSE_REF(x)\
 ntatag_s_bad_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_request;
#define NTATAG_S_DROP_REQUEST(x) ntatag_s_drop_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_request_ref;
#define NTATAG_S_DROP_REQUEST_REF(x)\
 ntatag_s_drop_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_response;
#define NTATAG_S_DROP_RESPONSE(x) ntatag_s_drop_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_response_ref;
#define NTATAG_S_DROP_RESPONSE_REF(x)\
 ntatag_s_drop_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_client_tr;
#define NTATAG_S_CLIENT_TR(x) ntatag_s_client_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_client_tr_ref;
#define NTATAG_S_CLIENT_TR_REF(x)\
 ntatag_s_client_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_server_tr;
#define NTATAG_S_SERVER_TR(x) ntatag_s_server_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_server_tr_ref;
#define NTATAG_S_SERVER_TR_REF(x)\
 ntatag_s_server_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr;
#define NTATAG_S_DIALOG_TR(x) ntatag_s_dialog_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr_ref;
#define NTATAG_S_DIALOG_TR_REF(x)\
 ntatag_s_dialog_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr;
#define NTATAG_S_ACKED_TR(x) ntatag_s_acked_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr_ref;
#define NTATAG_S_ACKED_TR_REF(x) ntatag_s_acked_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr;
#define NTATAG_S_CANCELED_TR(x) ntatag_s_canceled_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr_ref;
#define NTATAG_S_CANCELED_TR_REF(x)  \
 ntatag_s_canceled_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_request;
#define NTATAG_S_TRLESS_REQUEST(x) ntatag_s_trless_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_request_ref;
#define NTATAG_S_TRLESS_REQUEST_REF(x)\
 ntatag_s_trless_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr;
#define NTATAG_S_TRLESS_TO_TR(x) ntatag_s_trless_to_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr_ref;
#define NTATAG_S_TRLESS_TO_TR_REF(x)\
 ntatag_s_trless_to_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_response;
#define NTATAG_S_TRLESS_RESPONSE(x) ntatag_s_trless_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_response_ref;
#define NTATAG_S_TRLESS_RESPONSE_REF(x)\
 ntatag_s_trless_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_200;
#define NTATAG_S_TRLESS_200(x) ntatag_s_trless_200, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_200_ref;
#define NTATAG_S_TRLESS_200_REF(x)\
 ntatag_s_trless_200_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_merged_request;
#define NTATAG_S_MERGED_REQUEST(x) ntatag_s_merged_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_merged_request_ref;
#define NTATAG_S_MERGED_REQUEST_REF(x)\
 ntatag_s_merged_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg;
#define NTATAG_S_SENT_MSG(x) ntatag_s_sent_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg_ref;
#define NTATAG_S_SENT_MSG_REF(x)\
 ntatag_s_sent_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_request;
#define NTATAG_S_SENT_REQUEST(x) ntatag_s_sent_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_request_ref;
#define NTATAG_S_SENT_REQUEST_REF(x)\
 ntatag_s_sent_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_response;
#define NTATAG_S_SENT_RESPONSE(x) ntatag_s_sent_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_response_ref;
#define NTATAG_S_SENT_RESPONSE_REF(x)\
 ntatag_s_sent_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_request;
#define NTATAG_S_RETRY_REQUEST(x) ntatag_s_retry_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_request_ref;
#define NTATAG_S_RETRY_REQUEST_REF(x)\
 ntatag_s_retry_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_response;
#define NTATAG_S_RETRY_RESPONSE(x) ntatag_s_retry_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_response_ref;
#define NTATAG_S_RETRY_RESPONSE_REF(x)\
 ntatag_s_retry_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry;
#define NTATAG_S_RECV_RETRY(x) ntatag_s_recv_retry, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry_ref;
#define NTATAG_S_RECV_RETRY_REF(x)\
 ntatag_s_recv_retry_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_request;
#define NTATAG_S_TOUT_REQUEST(x) ntatag_s_tout_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_request_ref;
#define NTATAG_S_TOUT_REQUEST_REF(x)\
 ntatag_s_tout_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_response;
#define NTATAG_S_TOUT_RESPONSE(x) ntatag_s_tout_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_response_ref;
#define NTATAG_S_TOUT_RESPONSE_REF(x)\
 ntatag_s_tout_response_ref, tag_usize_vr(&(x))

SOFIA_END_DECLS

#endif /* !defined(nta_tag_h) */
