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

tag_typedef_t nutag_any = NSTAG_TYPEDEF(*);

tag_typedef_t nutag_url = URLTAG_TYPEDEF(url);
tag_typedef_t nutag_address = STRTAG_TYPEDEF(address);
tag_typedef_t nutag_method = STRTAG_TYPEDEF(method);
tag_typedef_t nutag_uicc = STRTAG_TYPEDEF(uicc);
tag_typedef_t nutag_media_features = BOOLTAG_TYPEDEF(media_features);
tag_typedef_t nutag_callee_caps = BOOLTAG_TYPEDEF(callee_caps);
tag_typedef_t nutag_early_media = BOOLTAG_TYPEDEF(early_media);
tag_typedef_t nutag_only183_100rel = BOOLTAG_TYPEDEF(only183_100rel);
tag_typedef_t nutag_early_answer = BOOLTAG_TYPEDEF(early_answer);
tag_typedef_t nutag_include_extra_sdp = BOOLTAG_TYPEDEF(include_extra_sdp);
tag_typedef_t nutag_media_enable = BOOLTAG_TYPEDEF(media_enable);

tag_typedef_t nutag_soa_session = PTRTAG_TYPEDEF(soa_session);
tag_typedef_t nutag_soa_name = STRTAG_TYPEDEF(soa_name);

tag_typedef_t nutag_retry_count = UINTTAG_TYPEDEF(retry_count);
tag_typedef_t nutag_max_subscriptions = UINTTAG_TYPEDEF(max_subscriptions);

tag_typedef_t nutag_callstate = INTTAG_TYPEDEF(callstate);
tag_typedef_t nutag_offer_recv = BOOLTAG_TYPEDEF(offer_recv);
tag_typedef_t nutag_answer_recv = BOOLTAG_TYPEDEF(answer_recv);
tag_typedef_t nutag_offer_sent = BOOLTAG_TYPEDEF(offer_sent);
tag_typedef_t nutag_answer_sent = BOOLTAG_TYPEDEF(answer_sent);
tag_typedef_t nutag_substate = INTTAG_TYPEDEF(substate);
tag_typedef_t nutag_newsub = BOOLTAG_TYPEDEF(newsub);
tag_typedef_t nutag_invite_timer = UINTTAG_TYPEDEF(invite_timer);
tag_typedef_t nutag_session_timer = UINTTAG_TYPEDEF(session_timer);
tag_typedef_t nutag_min_se = UINTTAG_TYPEDEF(min_se);
tag_typedef_t nutag_session_refresher = INTTAG_TYPEDEF(session_refresher);
tag_typedef_t nutag_update_refresh = BOOLTAG_TYPEDEF(update_refresh);
tag_typedef_t nutag_refer_expires = UINTTAG_TYPEDEF(refer_expires);
tag_typedef_t nutag_refer_with_id = BOOLTAG_TYPEDEF(refer_with_id);
tag_typedef_t nutag_autoalert = BOOLTAG_TYPEDEF(autoAlert);
tag_typedef_t nutag_autoanswer = BOOLTAG_TYPEDEF(autoAnswer);
tag_typedef_t nutag_autoack = BOOLTAG_TYPEDEF(autoACK);
tag_typedef_t nutag_enableinvite = BOOLTAG_TYPEDEF(enableInvite);
tag_typedef_t nutag_enablemessage = BOOLTAG_TYPEDEF(enableMessage);
tag_typedef_t nutag_enablemessenger = BOOLTAG_TYPEDEF(enableMessenger);


/* Start NRC Boston */
tag_typedef_t nutag_smime_enable = BOOLTAG_TYPEDEF(smime_enable);
tag_typedef_t nutag_smime_opt = INTTAG_TYPEDEF(smime_opt);
tag_typedef_t nutag_smime_protection_mode = 
  INTTAG_TYPEDEF(smime_protection_mode);
tag_typedef_t nutag_smime_message_digest = 
  STRTAG_TYPEDEF(smime_message_digest);
tag_typedef_t nutag_smime_signature = 
  STRTAG_TYPEDEF(smime_signature);
tag_typedef_t nutag_smime_key_encryption = 
  STRTAG_TYPEDEF(smime_key_encryption);
tag_typedef_t nutag_smime_message_encryption = 
  STRTAG_TYPEDEF(smime_message_encryption);
/* End NRC Boston */

tag_typedef_t nutag_sips_url = URLTAG_TYPEDEF(sips_url);
tag_typedef_t nutag_certificate_dir = STRTAG_TYPEDEF(certificate_dir);
tag_typedef_t nutag_certificate_phrase = STRTAG_TYPEDEF(certificate_phrase);

tag_typedef_t nutag_registrar = URLTAG_TYPEDEF(registrar);
tag_typedef_t nutag_identity = PTRTAG_TYPEDEF(identity);
tag_typedef_t nutag_m_display = STRTAG_TYPEDEF(m_display);
tag_typedef_t nutag_m_username = STRTAG_TYPEDEF(m_username);
tag_typedef_t nutag_m_params = STRTAG_TYPEDEF(m_params);
tag_typedef_t nutag_m_features = STRTAG_TYPEDEF(m_features);
tag_typedef_t nutag_instance = STRTAG_TYPEDEF(instance);
tag_typedef_t nutag_outbound = STRTAG_TYPEDEF(outbound);

tag_typedef_t nutag_outbound_set1 = STRTAG_TYPEDEF(outbound_set1);
tag_typedef_t nutag_outbound_set2 = STRTAG_TYPEDEF(outbound_set2);
tag_typedef_t nutag_outbound_set3 = STRTAG_TYPEDEF(outbound_set3);
tag_typedef_t nutag_outbound_set4 = STRTAG_TYPEDEF(outbound_set4);

tag_typedef_t nutag_keepalive = UINTTAG_TYPEDEF(keepalive);
tag_typedef_t nutag_keepalive_stream = UINTTAG_TYPEDEF(keepalive_stream);

tag_typedef_t nutag_use_dialog = BOOLTAG_TYPEDEF(use_dialog);

tag_typedef_t nutag_auth = STRTAG_TYPEDEF(auth);
tag_typedef_t nutag_authtime = INTTAG_TYPEDEF(authtime);

tag_typedef_t nutag_event = INTTAG_TYPEDEF(event);
tag_typedef_t nutag_status = INTTAG_TYPEDEF(status);
tag_typedef_t nutag_phrase = STRTAG_TYPEDEF(phrase);

tag_typedef_t nutag_handle = PTRTAG_TYPEDEF(handle);

tag_typedef_t nutag_hold = BOOLTAG_TYPEDEF(hold);

tag_typedef_t nutag_notify_refer = PTRTAG_TYPEDEF(notify_refer);
tag_typedef_t nutag_refer_event = SIPHDRTAG_NAMED_TYPEDEF(refer_event, event);
tag_typedef_t nutag_refer_pause = BOOLTAG_TYPEDEF(refer_pause);
tag_typedef_t nutag_user_agent = STRTAG_TYPEDEF(user_agent);
tag_typedef_t nutag_allow = STRTAG_TYPEDEF(allow);
tag_typedef_t nutag_allow_events = STRTAG_TYPEDEF(allow_events);
tag_typedef_t nutag_appl_method = STRTAG_TYPEDEF(appl_method);
tag_typedef_t nutag_supported = STRTAG_TYPEDEF(supported);
tag_typedef_t nutag_path_enable = BOOLTAG_TYPEDEF(path_enable);
tag_typedef_t nutag_service_route_enable = 
  BOOLTAG_TYPEDEF(service_route_enable);

tag_typedef_t nutag_detect_network_updates = UINTTAG_TYPEDEF(detect_network_updates);

tag_typedef_t nutag_with = PTRTAG_TYPEDEF(with);
