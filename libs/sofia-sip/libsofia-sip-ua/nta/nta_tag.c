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

tag_typedef_t ntatag_mclass = PTRTAG_TYPEDEF(mclass);

tag_typedef_t ntatag_bad_req_mask = UINTTAG_TYPEDEF(bad_req_mask);
tag_typedef_t ntatag_bad_resp_mask = UINTTAG_TYPEDEF(bad_resp_mask);

tag_typedef_t ntatag_default_proxy = URLTAG_TYPEDEF(default_proxy);
tag_typedef_t ntatag_contact = SIPHDRTAG_NAMED_TYPEDEF(contact, contact);
tag_typedef_t ntatag_target = SIPHDRTAG_NAMED_TYPEDEF(target, contact);
tag_typedef_t ntatag_aliases = SIPHDRTAG_NAMED_TYPEDEF(aliases, contact);

tag_typedef_t ntatag_method = STRTAG_TYPEDEF(method);
tag_typedef_t ntatag_branch_key = STRTAG_TYPEDEF(branch_key);
tag_typedef_t ntatag_ack_branch = STRTAG_TYPEDEF(ack_branch);
tag_typedef_t ntatag_comp = CSTRTAG_TYPEDEF(comp);
tag_typedef_t ntatag_msg = PTRTAG_TYPEDEF(msg);
tag_typedef_t ntatag_tport = PTRTAG_TYPEDEF(tport);
tag_typedef_t ntatag_smime = PTRTAG_TYPEDEF(smime);
tag_typedef_t ntatag_remote_cseq = UINTTAG_TYPEDEF(remote_cseq);

tag_typedef_t ntatag_maxsize = USIZETAG_TYPEDEF(maxsize);
tag_typedef_t ntatag_udp_mtu = UINTTAG_TYPEDEF(udp_mtu);
tag_typedef_t ntatag_max_forwards = UINTTAG_TYPEDEF(max_forwards);
tag_typedef_t ntatag_sip_t1 = UINTTAG_TYPEDEF(sip_t1);
tag_typedef_t ntatag_sip_t1x64 = UINTTAG_TYPEDEF(sip_t1x64);
tag_typedef_t ntatag_sip_t2 = UINTTAG_TYPEDEF(sip_t2);
tag_typedef_t ntatag_sip_t4 = UINTTAG_TYPEDEF(sip_t4);
tag_typedef_t ntatag_progress = UINTTAG_TYPEDEF(progress);
tag_typedef_t ntatag_blacklist = UINTTAG_TYPEDEF(blacklist);
tag_typedef_t ntatag_debug_drop_prob = UINTTAG_TYPEDEF(debug_drop_prob);

tag_typedef_t ntatag_sigcomp_options = STRTAG_TYPEDEF(sigcomp_options);
tag_typedef_t ntatag_sigcomp_close = BOOLTAG_TYPEDEF(sigcomp_close);
tag_typedef_t ntatag_sigcomp_aware = BOOLTAG_TYPEDEF(sigcomp_aware);
tag_typedef_t ntatag_sigcomp_algorithm = STRTAG_TYPEDEF(sigcomp_algorithm);

tag_typedef_t ntatag_ua = BOOLTAG_TYPEDEF(ua);
tag_typedef_t ntatag_stateless = BOOLTAG_TYPEDEF(stateless);
tag_typedef_t ntatag_user_via = BOOLTAG_TYPEDEF(user_via);
tag_typedef_t ntatag_pass_100 = BOOLTAG_TYPEDEF(pass_100);
tag_typedef_t ntatag_extra_100 = BOOLTAG_TYPEDEF(extra_100);
tag_typedef_t ntatag_timeout_408 = BOOLTAG_TYPEDEF(timeout_408);
tag_typedef_t ntatag_pass_408 = BOOLTAG_TYPEDEF(pass_408);
tag_typedef_t ntatag_merge_482 = BOOLTAG_TYPEDEF(merge_482);
tag_typedef_t ntatag_cancel_2543 = BOOLTAG_TYPEDEF(cancel_2543);
tag_typedef_t ntatag_cancel_408 = BOOLTAG_TYPEDEF(cancel_408);
tag_typedef_t ntatag_cancel_487 = BOOLTAG_TYPEDEF(cancel_487);
tag_typedef_t ntatag_tag_3261 = BOOLTAG_TYPEDEF(tag_3261);
tag_typedef_t ntatag_rel100 = BOOLTAG_TYPEDEF(rel100);
tag_typedef_t ntatag_no_dialog = BOOLTAG_TYPEDEF(no_dialog);
tag_typedef_t ntatag_use_timestamp = BOOLTAG_TYPEDEF(use_timestamp);
tag_typedef_t ntatag_sipflags = UINTTAG_TYPEDEF(sipflags);
tag_typedef_t ntatag_client_rport = BOOLTAG_TYPEDEF(client_rport);
tag_typedef_t ntatag_server_rport = BOOLTAG_TYPEDEF(server_rport);
tag_typedef_t ntatag_tcp_rport = BOOLTAG_TYPEDEF(tcp_rport);
tag_typedef_t ntatag_preload = UINTTAG_TYPEDEF(preload);
tag_typedef_t ntatag_use_naptr = BOOLTAG_TYPEDEF(naptr);
tag_typedef_t ntatag_use_srv = BOOLTAG_TYPEDEF(srv);
tag_typedef_t ntatag_rseq = UINTTAG_TYPEDEF(rseq);

/* Status */

tag_typedef_t ntatag_s_irq_hash =         USIZETAG_TYPEDEF(s_irq_hash);
tag_typedef_t ntatag_s_orq_hash =         USIZETAG_TYPEDEF(s_orq_hash);
tag_typedef_t ntatag_s_leg_hash =         USIZETAG_TYPEDEF(s_leg_hash);
tag_typedef_t ntatag_s_irq_hash_used =    USIZETAG_TYPEDEF(s_irq_hash_used);
tag_typedef_t ntatag_s_orq_hash_used =    USIZETAG_TYPEDEF(s_orq_hash_used);
tag_typedef_t ntatag_s_leg_hash_used =    USIZETAG_TYPEDEF(s_leg_hash_used);
tag_typedef_t ntatag_s_recv_msg =         USIZETAG_TYPEDEF(s_recv_msg);
tag_typedef_t ntatag_s_recv_request =     USIZETAG_TYPEDEF(s_recv_request);
tag_typedef_t ntatag_s_recv_response =    USIZETAG_TYPEDEF(s_recv_response);
tag_typedef_t ntatag_s_bad_message =      USIZETAG_TYPEDEF(s_bad_message);
tag_typedef_t ntatag_s_bad_request =      USIZETAG_TYPEDEF(s_bad_request);
tag_typedef_t ntatag_s_bad_response =     USIZETAG_TYPEDEF(s_bad_response);
tag_typedef_t ntatag_s_drop_request =     USIZETAG_TYPEDEF(s_drop_request);
tag_typedef_t ntatag_s_drop_response =    USIZETAG_TYPEDEF(s_drop_response);
tag_typedef_t ntatag_s_client_tr =        USIZETAG_TYPEDEF(s_client_tr);
tag_typedef_t ntatag_s_server_tr =        USIZETAG_TYPEDEF(s_server_tr);
tag_typedef_t ntatag_s_dialog_tr =        USIZETAG_TYPEDEF(s_dialog_tr);
tag_typedef_t ntatag_s_acked_tr =         USIZETAG_TYPEDEF(s_acked_tr);
tag_typedef_t ntatag_s_canceled_tr =      USIZETAG_TYPEDEF(s_canceled_tr);
tag_typedef_t ntatag_s_trless_request =   USIZETAG_TYPEDEF(s_trless_request);
tag_typedef_t ntatag_s_trless_to_tr =     USIZETAG_TYPEDEF(s_trless_to_tr);
tag_typedef_t ntatag_s_trless_response =  USIZETAG_TYPEDEF(s_trless_response);
tag_typedef_t ntatag_s_trless_200 =       USIZETAG_TYPEDEF(s_trless_200);
tag_typedef_t ntatag_s_merged_request =   USIZETAG_TYPEDEF(s_merged_request);
tag_typedef_t ntatag_s_sent_msg =      	  USIZETAG_TYPEDEF(s_sent_msg);
tag_typedef_t ntatag_s_sent_request =  	  USIZETAG_TYPEDEF(s_sent_request);
tag_typedef_t ntatag_s_sent_response = 	  USIZETAG_TYPEDEF(s_sent_response);
tag_typedef_t ntatag_s_retry_request = 	  USIZETAG_TYPEDEF(s_retry_request);
tag_typedef_t ntatag_s_retry_response =   USIZETAG_TYPEDEF(s_retry_response);
tag_typedef_t ntatag_s_recv_retry =       USIZETAG_TYPEDEF(s_recv_retry);
tag_typedef_t ntatag_s_tout_request =     USIZETAG_TYPEDEF(s_tout_request);
tag_typedef_t ntatag_s_tout_response =    USIZETAG_TYPEDEF(s_tout_response);

/* Internal */
tag_typedef_t ntatag_delay_sending = BOOLTAG_TYPEDEF(delay_sending);
tag_typedef_t ntatag_incomplete = BOOLTAG_TYPEDEF(incomplete);
