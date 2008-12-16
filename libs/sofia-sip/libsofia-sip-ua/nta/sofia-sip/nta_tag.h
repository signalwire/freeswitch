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
#define NTATAG_MCLASS(x) ntatag_mclass, tag_cptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_mclass_ref;
#define NTATAG_MCLASS_REF(x) ntatag_mclass_ref, tag_cptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask;
#define NTATAG_BAD_REQ_MASK(x) ntatag_bad_req_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_req_mask_ref;
#define NTATAG_BAD_REQ_MASK_REF(x) ntatag_bad_req_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask;
#define NTATAG_BAD_RESP_MASK(x) ntatag_bad_resp_mask, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_bad_resp_mask_ref;
#define NTATAG_BAD_RESP_MASK_REF(x) ntatag_bad_resp_mask_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy;
#define NTATAG_DEFAULT_PROXY(x) ntatag_default_proxy, urltag_url_v((x))

NTA_DLL extern tag_typedef_t ntatag_default_proxy_ref;
#define NTATAG_DEFAULT_PROXY_REF(x) \
ntatag_default_proxy_ref, urltag_url_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_contact;
#define NTATAG_CONTACT(x) ntatag_contact, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_contact_ref;
#define NTATAG_CONTACT_REF(x) ntatag_contact_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_target;
#define NTATAG_TARGET(x) ntatag_target, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_target_ref;
#define NTATAG_TARGET_REF(x) ntatag_target_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_aliases;
#define NTATAG_ALIASES(x) ntatag_aliases, siptag_contact_v((x))

NTA_DLL extern tag_typedef_t ntatag_aliases_ref;
#define NTATAG_ALIASES_REF(x) ntatag_aliases_ref, siptag_contact_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_branch_key;
#define NTATAG_BRANCH_KEY(x) ntatag_branch_key, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_branch_key_ref;
#define NTATAG_BRANCH_KEY_REF(x) \
ntatag_branch_key_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_ack_branch;
#define NTATAG_ACK_BRANCH(x) ntatag_ack_branch, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_ack_branch_ref;
#define NTATAG_ACK_BRANCH_REF(x) ntatag_ack_branch_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_comp;
#define NTATAG_COMP(x) ntatag_comp, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_comp_ref;
#define NTATAG_COMP_REF(x) ntatag_comp_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_msg;
#define NTATAG_MSG(x)     ntatag_msg, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_msg_ref;
#define NTATAG_MSG_REF(x) ntatag_msg_ref, tag_ptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_tport;
#define NTATAG_TPORT(x)     ntatag_tport, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_tport_ref;
#define NTATAG_TPORT_REF(x) ntatag_tport_ref, tag_ptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_remote_cseq;
#define NTATAG_REMOTE_CSEQ(x) ntatag_remote_cseq, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_remote_cseq_ref;
#define NTATAG_REMOTE_CSEQ_REF(x) ntatag_remote_cseq_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_smime;
#define NTATAG_SMIME(x)     ntatag_smime, tag_ptr_v((x))

NTA_DLL extern tag_typedef_t ntatag_smime_ref;
#define NTATAG_SMIME_REF(x) ntatag_smime_ref, tag_ptr_vr(&(x), (x))

NTA_DLL extern tag_typedef_t ntatag_maxsize;
#define NTATAG_MAXSIZE(x) ntatag_maxsize, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_maxsize_ref;
#define NTATAG_MAXSIZE_REF(x) ntatag_maxsize_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu;
#define NTATAG_UDP_MTU(x) ntatag_udp_mtu, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_udp_mtu_ref;
#define NTATAG_UDP_MTU_REF(x) ntatag_udp_mtu_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_max_proceeding;
#define NTATAG_MAX_PROCEEDING(x) ntatag_max_proceeding, tag_usize_v((x))

NTA_DLL extern tag_typedef_t ntatag_max_proceeding_ref;
#define NTATAG_MAX_PROCEEDING_REF(x) ntatag_max_proceeding_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards;
#define NTATAG_MAX_FORWARDS(x) ntatag_max_forwards, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_max_forwards_ref;
#define NTATAG_MAX_FORWARDS_REF(x) ntatag_max_forwards_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1;
#define NTATAG_SIP_T1(x) ntatag_sip_t1, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1_ref;
#define NTATAG_SIP_T1_REF(x) ntatag_sip_t1_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64;
#define NTATAG_SIP_T1X64(x) ntatag_sip_t1x64, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t1x64_ref;
#define NTATAG_SIP_T1X64_REF(x) ntatag_sip_t1x64_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2;
#define NTATAG_SIP_T2(x) ntatag_sip_t2, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t2_ref;
#define NTATAG_SIP_T2_REF(x) ntatag_sip_t2_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4;
#define NTATAG_SIP_T4(x)    ntatag_sip_t4, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sip_t4_ref;
#define NTATAG_SIP_T4_REF(x) ntatag_sip_t4_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_progress;
#define NTATAG_PROGRESS(x)    ntatag_progress, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_progress_ref;
#define NTATAG_PROGRESS_REF(x) ntatag_progress_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_timer_c;
#define NTATAG_TIMER_C(x)    ntatag_timer_c, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_timer_c_ref;
#define NTATAG_TIMER_C_REF(x) ntatag_timer_c_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_graylist;
#define NTATAG_GRAYLIST(x)  ntatag_graylist, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_graylist_ref;
#define NTATAG_GRAYLIST_REF(x) ntatag_graylist_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_blacklist;
#define NTATAG_BLACKLIST(x)  ntatag_blacklist, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_blacklist_ref;
#define NTATAG_BLACKLIST_REF(x) ntatag_blacklist_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob;
#define NTATAG_DEBUG_DROP_PROB(x) ntatag_debug_drop_prob, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_debug_drop_prob_ref;
#define NTATAG_DEBUG_DROP_PROB_REF(x) ntatag_debug_drop_prob_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options;
#define NTATAG_SIGCOMP_OPTIONS(x)    ntatag_sigcomp_options, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_options_ref;
#define NTATAG_SIGCOMP_OPTIONS_REF(x) ntatag_sigcomp_options_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close;
#define NTATAG_SIGCOMP_CLOSE(x)  ntatag_sigcomp_close, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_close_ref;
#define NTATAG_SIGCOMP_CLOSE_REF(x) ntatag_sigcomp_close_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware;
#define NTATAG_SIGCOMP_AWARE(x) ntatag_sigcomp_aware, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_aware_ref;
#define NTATAG_SIGCOMP_AWARE_REF(x) ntatag_sigcomp_aware_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm;
#define NTATAG_SIGCOMP_ALGORITHM(x) ntatag_sigcomp_algorithm, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_sigcomp_algorithm_ref;
#define NTATAG_SIGCOMP_ALGORITHM_REF(x) \
ntatag_sigcomp_algorithm_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_ua;
#define NTATAG_UA(x) ntatag_ua, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_ua_ref;
#define NTATAG_UA_REF(x) ntatag_ua_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_stateless;
#define NTATAG_STATELESS(x) ntatag_stateless, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_stateless_ref;
#define NTATAG_STATELESS_REF(x) ntatag_stateless_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_user_via;
#define NTATAG_USER_VIA(x) ntatag_user_via, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_user_via_ref;
#define NTATAG_USER_VIA_REF(x) ntatag_user_via_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_extra_100;
#define NTATAG_EXTRA_100(x)    ntatag_extra_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_extra_100_ref;
#define NTATAG_EXTRA_100_REF(x) ntatag_extra_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_100;
#define NTATAG_PASS_100(x) ntatag_pass_100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_100_ref;
#define NTATAG_PASS_100_REF(x) ntatag_pass_100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408;
#define NTATAG_TIMEOUT_408(x)  ntatag_timeout_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_timeout_408_ref;
#define NTATAG_TIMEOUT_408_REF(x) ntatag_timeout_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_pass_408;
#define NTATAG_PASS_408(x)  ntatag_pass_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_pass_408_ref;
#define NTATAG_PASS_408_REF(x) ntatag_pass_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog;
#define NTATAG_NO_DIALOG(x)       ntatag_no_dialog, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_no_dialog_ref;
#define NTATAG_NO_DIALOG_REF(x)   ntatag_no_dialog_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_merge_482;
#define NTATAG_MERGE_482(x)       ntatag_merge_482, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_merge_482_ref;
#define NTATAG_MERGE_482_REF(x)   ntatag_merge_482_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543;
#define NTATAG_CANCEL_2543(x)     ntatag_cancel_2543, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_2543_ref;
#define NTATAG_CANCEL_2543_REF(x) ntatag_cancel_2543_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408;
#define NTATAG_CANCEL_408(x)     ntatag_cancel_408, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_408_ref;
#define NTATAG_CANCEL_408_REF(x) ntatag_cancel_408_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261;
#define NTATAG_TAG_3261(x)        ntatag_tag_3261, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tag_3261_ref;
#define NTATAG_TAG_3261_REF(x)    ntatag_tag_3261_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp;
#define NTATAG_USE_TIMESTAMP(x) ntatag_use_timestamp, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_timestamp_ref;
#define NTATAG_USE_TIMESTAMP_REF(x) ntatag_use_timestamp_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_method;
#define NTATAG_METHOD(x)          ntatag_method, tag_str_v((x))

NTA_DLL extern tag_typedef_t ntatag_method_ref;
#define NTATAG_METHOD_REF(x)      ntatag_method_ref, tag_str_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487;
#define NTATAG_CANCEL_487(x)     ntatag_cancel_487, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_cancel_487_ref;
#define NTATAG_CANCEL_487_REF(x) ntatag_cancel_487_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rel100;
#define NTATAG_REL100(x)     ntatag_rel100, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_rel100_ref;
#define NTATAG_REL100_REF(x) ntatag_rel100_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_sipflags;
#define NTATAG_SIPFLAGS(x)     ntatag_sipflags, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_sipflags_ref;
#define NTATAG_SIPFLAGS_REF(x) ntatag_sipflags_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_client_rport;
#define NTATAG_CLIENT_RPORT(x) ntatag_client_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_client_rport_ref;
#define NTATAG_CLIENT_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

#define NTATAG_RPORT(x) ntatag_client_rport, tag_bool_v((x))
#define NTATAG_RPORT_REF(x) ntatag_client_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_server_rport;
#define NTATAG_SERVER_RPORT(x) ntatag_server_rport, tag_int_v((x))

NTA_DLL extern tag_typedef_t ntatag_server_rport_ref;
#define NTATAG_SERVER_RPORT_REF(x) ntatag_server_rport_ref, tag_int_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport;
#define NTATAG_TCP_RPORT(x) ntatag_tcp_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tcp_rport_ref;
#define NTATAG_TCP_RPORT_REF(x) ntatag_tcp_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_tls_rport;
#define NTATAG_TLS_RPORT(x) ntatag_tls_rport, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_tls_rport_ref;
#define NTATAG_TLS_RPORT_REF(x) ntatag_tls_rport_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_preload;
#define NTATAG_PRELOAD(x) ntatag_preload, tag_uint_v((x))

NTA_DLL extern tag_typedef_t ntatag_preload_ref;
#define NTATAG_PRELOAD_REF(x) ntatag_preload_ref, tag_uint_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr;
#define NTATAG_USE_NAPTR(x) ntatag_use_naptr, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_naptr_ref;
#define NTATAG_USE_NAPTR_REF(x) ntatag_use_naptr_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_use_srv;
#define NTATAG_USE_SRV(x) ntatag_use_srv, tag_bool_v((x))

NTA_DLL extern tag_typedef_t ntatag_use_srv_ref;
#define NTATAG_USE_SRV_REF(x) ntatag_use_srv_ref, tag_bool_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_rseq;
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
#define NTATAG_S_IRQ_HASH_USED_REF(x) ntatag_s_irq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used;
#define NTATAG_S_ORQ_HASH_USED(x) ntatag_s_orq_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_orq_hash_used_ref;
#define NTATAG_S_ORQ_HASH_USED_REF(x) ntatag_s_orq_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used;
#define NTATAG_S_LEG_HASH_USED(x) ntatag_s_leg_hash_used, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_leg_hash_used_ref;
#define NTATAG_S_LEG_HASH_USED_REF(x) ntatag_s_leg_hash_used_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg;
#define NTATAG_S_RECV_MSG(x) ntatag_s_recv_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_msg_ref;
#define NTATAG_S_RECV_MSG_REF(x) ntatag_s_recv_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_request;
#define NTATAG_S_RECV_REQUEST(x) ntatag_s_recv_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_request_ref;
#define NTATAG_S_RECV_REQUEST_REF(x) ntatag_s_recv_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_response;
#define NTATAG_S_RECV_RESPONSE(x) ntatag_s_recv_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_response_ref;
#define NTATAG_S_RECV_RESPONSE_REF(x) ntatag_s_recv_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_message;
#define NTATAG_S_BAD_MESSAGE(x) ntatag_s_bad_message, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_message_ref;
#define NTATAG_S_BAD_MESSAGE_REF(x) ntatag_s_bad_message_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_request;
#define NTATAG_S_BAD_REQUEST(x) ntatag_s_bad_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_request_ref;
#define NTATAG_S_BAD_REQUEST_REF(x) ntatag_s_bad_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_bad_response;
#define NTATAG_S_BAD_RESPONSE(x) ntatag_s_bad_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_bad_response_ref;
#define NTATAG_S_BAD_RESPONSE_REF(x) ntatag_s_bad_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_request;
#define NTATAG_S_DROP_REQUEST(x) ntatag_s_drop_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_request_ref;
#define NTATAG_S_DROP_REQUEST_REF(x) ntatag_s_drop_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_drop_response;
#define NTATAG_S_DROP_RESPONSE(x) ntatag_s_drop_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_drop_response_ref;
#define NTATAG_S_DROP_RESPONSE_REF(x) ntatag_s_drop_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_client_tr;
#define NTATAG_S_CLIENT_TR(x) ntatag_s_client_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_client_tr_ref;
#define NTATAG_S_CLIENT_TR_REF(x) ntatag_s_client_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_server_tr;
#define NTATAG_S_SERVER_TR(x) ntatag_s_server_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_server_tr_ref;
#define NTATAG_S_SERVER_TR_REF(x) ntatag_s_server_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr;
#define NTATAG_S_DIALOG_TR(x) ntatag_s_dialog_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_dialog_tr_ref;
#define NTATAG_S_DIALOG_TR_REF(x) ntatag_s_dialog_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr;
#define NTATAG_S_ACKED_TR(x) ntatag_s_acked_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_acked_tr_ref;
#define NTATAG_S_ACKED_TR_REF(x) ntatag_s_acked_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr;
#define NTATAG_S_CANCELED_TR(x) ntatag_s_canceled_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_canceled_tr_ref;
#define NTATAG_S_CANCELED_TR_REF(x)   ntatag_s_canceled_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_request;
#define NTATAG_S_TRLESS_REQUEST(x) ntatag_s_trless_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_request_ref;
#define NTATAG_S_TRLESS_REQUEST_REF(x) ntatag_s_trless_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr;
#define NTATAG_S_TRLESS_TO_TR(x) ntatag_s_trless_to_tr, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_to_tr_ref;
#define NTATAG_S_TRLESS_TO_TR_REF(x) ntatag_s_trless_to_tr_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_response;
#define NTATAG_S_TRLESS_RESPONSE(x) ntatag_s_trless_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_response_ref;
#define NTATAG_S_TRLESS_RESPONSE_REF(x) ntatag_s_trless_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_trless_200;
#define NTATAG_S_TRLESS_200(x) ntatag_s_trless_200, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_trless_200_ref;
#define NTATAG_S_TRLESS_200_REF(x) ntatag_s_trless_200_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_merged_request;
#define NTATAG_S_MERGED_REQUEST(x) ntatag_s_merged_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_merged_request_ref;
#define NTATAG_S_MERGED_REQUEST_REF(x) ntatag_s_merged_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg;
#define NTATAG_S_SENT_MSG(x) ntatag_s_sent_msg, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_msg_ref;
#define NTATAG_S_SENT_MSG_REF(x) ntatag_s_sent_msg_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_request;
#define NTATAG_S_SENT_REQUEST(x) ntatag_s_sent_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_request_ref;
#define NTATAG_S_SENT_REQUEST_REF(x) ntatag_s_sent_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_sent_response;
#define NTATAG_S_SENT_RESPONSE(x) ntatag_s_sent_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_sent_response_ref;
#define NTATAG_S_SENT_RESPONSE_REF(x) ntatag_s_sent_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_request;
#define NTATAG_S_RETRY_REQUEST(x) ntatag_s_retry_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_request_ref;
#define NTATAG_S_RETRY_REQUEST_REF(x) ntatag_s_retry_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_retry_response;
#define NTATAG_S_RETRY_RESPONSE(x) ntatag_s_retry_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_retry_response_ref;
#define NTATAG_S_RETRY_RESPONSE_REF(x) ntatag_s_retry_response_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry;
#define NTATAG_S_RECV_RETRY(x) ntatag_s_recv_retry, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_recv_retry_ref;
#define NTATAG_S_RECV_RETRY_REF(x) ntatag_s_recv_retry_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_request;
#define NTATAG_S_TOUT_REQUEST(x) ntatag_s_tout_request, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_request_ref;
#define NTATAG_S_TOUT_REQUEST_REF(x) ntatag_s_tout_request_ref, tag_usize_vr(&(x))

NTA_DLL extern tag_typedef_t ntatag_s_tout_response;
#define NTATAG_S_TOUT_RESPONSE(x) ntatag_s_tout_response, tag_usize_v(x)

NTA_DLL extern tag_typedef_t ntatag_s_tout_response_ref;
#define NTATAG_S_TOUT_RESPONSE_REF(x) ntatag_s_tout_response_ref, tag_usize_vr(&(x))

SOFIA_END_DECLS

#endif /* !defined(nta_tag_h) */
