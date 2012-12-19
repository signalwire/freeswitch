/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * sofia_media.c -- SOFIA SIP Endpoint (sofia media code)
 *
 */
#include "mod_sofia.h"


switch_status_t sofia_glue_get_offered_pt(private_object_t *tech_pvt, const switch_codec_implementation_t *mimp, switch_payload_t *pt)
{
	int i = 0;

	for (i = 0; i < tech_pvt->num_codecs; i++) {
		const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

		if (!strcasecmp(imp->iananame, mimp->iananame)) {
			*pt = tech_pvt->ianacodes[i];

			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}


void sofia_glue_tech_absorb_sdp(private_object_t *tech_pvt)
{
	const char *sdp_str;

	if ((sdp_str = switch_channel_get_variable(tech_pvt->channel, SWITCH_B_SDP_VARIABLE))) {
		sdp_parser_t *parser;
		sdp_session_t *sdp;
		sdp_media_t *m;
		sdp_connection_t *connection;

		if ((parser = sdp_parse(NULL, sdp_str, (int) strlen(sdp_str), 0))) {
			if ((sdp = sdp_session(parser))) {
				for (m = sdp->sdp_media; m; m = m->m_next) {
					if (m->m_type != sdp_media_audio || !m->m_port) {
						continue;
					}

					connection = sdp->sdp_connection;
					if (m->m_connections) {
						connection = m->m_connections;
					}

					if (connection) {
						tech_pvt->proxy_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, connection->c_address);
					}
					tech_pvt->proxy_sdp_audio_port = (switch_port_t) m->m_port;
					if (tech_pvt->proxy_sdp_audio_ip && tech_pvt->proxy_sdp_audio_port) {
						break;
					}
				}
			}
			sdp_parser_free(parser);
		}
		sofia_glue_tech_set_local_sdp(tech_pvt, sdp_str, SWITCH_TRUE);
	}
}



switch_status_t sofia_glue_sdp_map(const char *r_sdp, switch_event_t **fmtp, switch_event_t **pt)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return SWITCH_STATUS_FALSE;
	}

	switch_event_create(&(*fmtp), SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_create(&(*pt), SWITCH_EVENT_REQUEST_PARAMS);

	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (m->m_proto == sdp_proto_rtp) {
			sdp_rtpmap_t *map;
			
			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				if (map->rm_encoding) {
					char buf[25] = "";
					char key[128] = "";
					char *br = NULL;

					if (map->rm_fmtp) {
						if ((br = strstr(map->rm_fmtp, "bitrate="))) {
							br += 8;
						}
					}

					switch_snprintf(buf, sizeof(buf), "%d", map->rm_pt);

					if (br) {
						switch_snprintf(key, sizeof(key), "%s:%s", map->rm_encoding, br);
					} else {
						switch_snprintf(key, sizeof(key), "%s", map->rm_encoding);
					}
					
					switch_event_add_header_string(*pt, SWITCH_STACK_BOTTOM, key, buf);

					if (map->rm_fmtp) {
						switch_event_add_header_string(*fmtp, SWITCH_STACK_BOTTOM, key, map->rm_fmtp);
					}
				}
			}
		}
	}
	
	sdp_parser_free(parser);

	return SWITCH_STATUS_SUCCESS;
	
}


void sofia_glue_proxy_codec(switch_core_session_t *session, const char *r_sdp)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return;
	}

	switch_assert(tech_pvt != NULL);


	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (zstr(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
		}
	}


	for (m = sdp->sdp_media; m; m = m->m_next) {

		ptime = dptime;
		//maxptime = dmaxptime;

		if (m->m_proto == sdp_proto_rtp) {
			sdp_rtpmap_t *map;
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
					//maxptime = atoi(attr->a_value);		
				}
			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				tech_pvt->iananame = switch_core_session_strdup(tech_pvt->session, map->rm_encoding);
				tech_pvt->rm_rate = map->rm_rate;
				tech_pvt->codec_ms = ptime;
				sofia_glue_tech_set_codec(tech_pvt, 0);
				break;
			}

			break;
		}
	}

	sdp_parser_free(parser);

}

switch_t38_options_t *tech_process_udptl(private_object_t *tech_pvt, sdp_session_t *sdp, sdp_media_t *m)
{
	switch_t38_options_t *t38_options = switch_channel_get_private(tech_pvt->channel, "t38_options");
	sdp_attribute_t *attr;

	if (!t38_options) {
		t38_options = switch_core_session_alloc(tech_pvt->session, sizeof(switch_t38_options_t));

		// set some default value
		t38_options->T38FaxVersion = 0;
		t38_options->T38MaxBitRate = 14400;
		t38_options->T38FaxRateManagement = switch_core_session_strdup(tech_pvt->session, "transferredTCF");
		t38_options->T38FaxUdpEC = switch_core_session_strdup(tech_pvt->session, "t38UDPRedundancy");
		t38_options->T38FaxMaxBuffer = 500;
		t38_options->T38FaxMaxDatagram = 500;
	}

	t38_options->remote_port = (switch_port_t)m->m_port;

	if (sdp->sdp_origin) {
		t38_options->sdp_o_line = switch_core_session_strdup(tech_pvt->session, sdp->sdp_origin->o_username);
	} else {
		t38_options->sdp_o_line = "unknown";
	}
	
	if (m->m_connections && m->m_connections->c_address) {
		t38_options->remote_ip = switch_core_session_strdup(tech_pvt->session, m->m_connections->c_address);
	} else if (sdp && sdp->sdp_connection && sdp->sdp_connection->c_address) {
		t38_options->remote_ip = switch_core_session_strdup(tech_pvt->session, sdp->sdp_connection->c_address);
	}

	for (attr = m->m_attributes; attr; attr = attr->a_next) {
		if (!strcasecmp(attr->a_name, "T38FaxVersion") && attr->a_value) {
			t38_options->T38FaxVersion = (uint16_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38MaxBitRate") && attr->a_value) {
			t38_options->T38MaxBitRate = (uint32_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxFillBitRemoval")) {
			t38_options->T38FaxFillBitRemoval = switch_safe_atoi(attr->a_value, 1);
		} else if (!strcasecmp(attr->a_name, "T38FaxTranscodingMMR")) {
			t38_options->T38FaxTranscodingMMR = switch_safe_atoi(attr->a_value, 1);
		} else if (!strcasecmp(attr->a_name, "T38FaxTranscodingJBIG")) {
			t38_options->T38FaxTranscodingJBIG = switch_safe_atoi(attr->a_value, 1);
		} else if (!strcasecmp(attr->a_name, "T38FaxRateManagement") && attr->a_value) {
			t38_options->T38FaxRateManagement = switch_core_session_strdup(tech_pvt->session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxMaxBuffer") && attr->a_value) {
			t38_options->T38FaxMaxBuffer = (uint32_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxMaxDatagram") && attr->a_value) {
			t38_options->T38FaxMaxDatagram = (uint32_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxUdpEC") && attr->a_value) {
			t38_options->T38FaxUdpEC = switch_core_session_strdup(tech_pvt->session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38VendorInfo") && attr->a_value) {
			t38_options->T38VendorInfo = switch_core_session_strdup(tech_pvt->session, attr->a_value);
		}
	}

	switch_channel_set_variable(tech_pvt->channel, "has_t38", "true");
	switch_channel_set_private(tech_pvt->channel, "t38_options", t38_options);
	switch_channel_set_app_flag_key("T38", tech_pvt->channel, CF_APP_T38);

	switch_channel_execute_on(tech_pvt->channel, "sip_execute_on_image");
	switch_channel_api_on(tech_pvt->channel, "sip_api_on_image");

	return t38_options;
}




switch_t38_options_t *sofia_glue_extract_t38_options(switch_core_session_t *session, const char *r_sdp)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_t38_options_t *t38_options = NULL;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	switch_assert(tech_pvt != NULL);

	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (m->m_proto == sdp_proto_udptl && m->m_type == sdp_media_image && m->m_port) {
			t38_options = tech_process_udptl(tech_pvt, sdp, m);
			break;
		}
	}

	sdp_parser_free(parser);

	return t38_options;

}


uint8_t sofia_glue_negotiate_sdp(switch_core_session_t *session, const char *r_sdp)
{
	uint8_t match = 0;
	switch_payload_t best_te = 0, te = 0, cng_pt = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int first = 0, last = 0;
	int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
	int sendonly = 0, recvonly = 0;
	int greedy = 0, x = 0, skip = 0, mine = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *val;
	const char *crypto = NULL;
	int got_crypto = 0, got_video_crypto = 0, got_audio = 0, got_avp = 0, got_video_avp = 0, got_video_savp = 0, got_savp = 0, got_udptl = 0;
	int scrooge = 0;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	int reneg = 1;
	const switch_codec_implementation_t **codec_array;
	int total_codecs;


	codec_array = tech_pvt->codecs;
	total_codecs = tech_pvt->num_codecs;


	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	switch_assert(tech_pvt != NULL);

	greedy = !!sofia_test_pflag(tech_pvt->profile, PFLAG_GREEDY);
	scrooge = !!sofia_test_pflag(tech_pvt->profile, PFLAG_SCROOGE);

	if ((val = switch_channel_get_variable(channel, "sip_codec_negotiation"))) {
		if (!strcasecmp(val, "generous")) {
			greedy = 0;
			scrooge = 0;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sip_codec_negotiation overriding sofia inbound-codec-negotiation : generous\n" );
		} else if (!strcasecmp(val, "greedy")) {
			greedy = 1;
			scrooge = 0;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sip_codec_negotiation overriding sofia inbound-codec-negotiation : greedy\n" );
		} else if (!strcasecmp(val, "scrooge")) {
			scrooge = 1;
			greedy = 1;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sip_codec_negotiation overriding sofia inbound-codec-negotiation : scrooge\n" );
		} else {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sip_codec_negotiation ignored invalid value : '%s' \n", val );	
		}		
	}

	if ((tech_pvt->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {

		if (tech_pvt->profile->auto_rtp_bugs & RTP_BUG_CISCO_SKIP_MARK_BIT_2833) {

			if (strstr(tech_pvt->origin, "CiscoSystemsSIP-GW-UserAgent")) {
				tech_pvt->rtp_bugs |= RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
			}
		}

		if (tech_pvt->profile->auto_rtp_bugs & RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833) {
			if (strstr(tech_pvt->origin, "Sonus_UAC")) {
				tech_pvt->rtp_bugs |= RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "Hello,\nI see you have a Sonus!\n"
								  "FYI, Sonus cannot follow the RFC on the proper way to send DTMF.\n"
								  "Sadly, my creator had to spend several hours figuring this out so I thought you'd like to know that!\n"
								  "Don't worry, DTMF will work but you may want to ask them to fix it......\n");
			}
		}
	}

	if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_liberal_dtmf")) && switch_true(val)) {
		sofia_set_flag_locked(tech_pvt, TFLAG_LIBERAL_DTMF);
	}

	if ((m = sdp->sdp_media) && 
		(m->m_mode == sdp_sendonly || m->m_mode == sdp_inactive || 
		 (m->m_connections && m->m_connections->c_address && !strcmp(m->m_connections->c_address, "0.0.0.0")))) {
		sendonly = 2;			/* global sendonly always wins */
	}

	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (zstr(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "sendonly")) {
			sendonly = 1;
			switch_channel_set_variable(tech_pvt->channel, "media_audio_mode", "recvonly");
		} else if (!strcasecmp(attr->a_name, "inactive")) {
			sendonly = 1;
			switch_channel_set_variable(tech_pvt->channel, "media_audio_mode", "inactive");
		} else if (!strcasecmp(attr->a_name, "recvonly")) {
			switch_channel_set_variable(tech_pvt->channel, "media_audio_mode", "sendonly");
			recvonly = 1;

			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, 0);
				tech_pvt->max_missed_hold_packets = 0;
				tech_pvt->max_missed_packets = 0;
			} else {
				switch_channel_set_variable(tech_pvt->channel, "rtp_timeout_sec", "0");
				switch_channel_set_variable(tech_pvt->channel, "rtp_hold_timeout_sec", "0");
			}
		} else if (sendonly < 2 && !strcasecmp(attr->a_name, "sendrecv")) {
			sendonly = 0;
		} else if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "maxptime")) {
			dmaxptime = atoi(attr->a_value);
		}
	}

	if (sendonly != 1 && recvonly != 1) {
		switch_channel_set_variable(tech_pvt->channel, "media_audio_mode", NULL);
	}


	if (sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_HOLD) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "sip_disable_hold")) && switch_true(val))) {
		sendonly = 0;
	} else {

		if (!tech_pvt->hold_laps) {
			tech_pvt->hold_laps++;
			if (sofia_glue_toggle_hold(tech_pvt, sendonly)) {
				reneg = sofia_test_pflag(tech_pvt->profile, PFLAG_RENEG_ON_HOLD);
				
				if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_renegotiate_codec_on_hold"))) {
					reneg = switch_true(val);
				}
			}
			
		}
	}

	if (reneg) {
		reneg = sofia_test_pflag(tech_pvt->profile, PFLAG_RENEG_ON_REINVITE);
		
		if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_renegotiate_codec_on_reinvite"))) {
			reneg = switch_true(val);
		}
	}

	if (!reneg && tech_pvt->num_negotiated_codecs) {
		codec_array = tech_pvt->negotiated_codecs;
		total_codecs = tech_pvt->num_negotiated_codecs;
	} else if (reneg) {
		tech_pvt->num_codecs = 0;
		sofia_glue_tech_prepare_codecs(tech_pvt);
		codec_array = tech_pvt->codecs;
		total_codecs = tech_pvt->num_codecs;
	}

	if (switch_stristr("T38FaxFillBitRemoval:", r_sdp) || switch_stristr("T38FaxTranscodingMMR:", r_sdp) || 
		switch_stristr("T38FaxTranscodingJBIG:", r_sdp)) {
		switch_channel_set_variable(tech_pvt->channel, "t38_broken_boolean", "true");
	}

	find_zrtp_hash(session, sdp);
	sofia_glue_pass_zrtp_hash(session);

	for (m = sdp->sdp_media; m; m = m->m_next) {
		sdp_connection_t *connection;
		switch_core_session_t *other_session;

		ptime = dptime;
		maxptime = dmaxptime;

		if (m->m_proto == sdp_proto_srtp) {
			if (m->m_type == sdp_media_audio) {
				got_savp++;
			} else {
				got_video_savp++;
			}
		} else if (m->m_proto == sdp_proto_rtp) {
			if (m->m_type == sdp_media_audio) {
				got_avp++;
			} else {
				got_video_avp++;
			}
		} else if (m->m_proto == sdp_proto_udptl) {
			got_udptl++;
		}

		if (got_udptl && m->m_type == sdp_media_image && m->m_port) {
			switch_t38_options_t *t38_options = tech_process_udptl(tech_pvt, sdp, m);

			if (switch_channel_test_app_flag_key("T38", tech_pvt->channel, CF_APP_T38_NEGOTIATED)) {
				match = 1;
				goto done;
			}

			if (switch_true(switch_channel_get_variable(channel, "refuse_t38"))) {
				switch_channel_clear_app_flag_key("T38", tech_pvt->channel, CF_APP_T38);
				match = 0;
				goto done;
			} else {
				const char *var = switch_channel_get_variable(channel, "t38_passthru");
				int pass = sofia_test_pflag(tech_pvt->profile, PFLAG_T38_PASSTHRU);


				if (switch_channel_test_app_flag_key("T38", tech_pvt->channel, CF_APP_T38)) {
					sofia_set_flag(tech_pvt, TFLAG_NOREPLY);
				}

				if (var) {
					if (!(pass = switch_true(var))) {
						if (!strcasecmp(var, "once")) {
							pass = 2;
						}
					}
				}

				if ((pass == 2 && sofia_test_flag(tech_pvt, TFLAG_T38_PASSTHRU)) || 
					!sofia_test_flag(tech_pvt, TFLAG_REINVITE) ||
					switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || 
					switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || 
					!switch_rtp_ready(tech_pvt->rtp_session)) {
					pass = 0;
				}
				
				if (pass && switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					private_object_t *other_tech_pvt = switch_core_session_get_private(other_session);
					switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
					switch_core_session_message_t *msg;
					char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
					switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
					char tmp[32] = "";

					if (switch_true(switch_channel_get_variable(tech_pvt->channel, "t38_broken_boolean")) && 
						switch_true(switch_channel_get_variable(tech_pvt->channel, "t38_pass_broken_boolean"))) {
						switch_channel_set_variable(other_channel, "t38_broken_boolean", "true");
					}
					
					tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, t38_options->remote_ip);
					tech_pvt->remote_sdp_audio_port = t38_options->remote_port;

					if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n",
										  switch_channel_get_name(tech_pvt->channel));
					} else {
						const char *err = NULL;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n",
										  switch_channel_get_name(tech_pvt->channel),
										  remote_host, remote_port, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
						
						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);

						if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip,
														  tech_pvt->remote_sdp_audio_port, 0, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
							switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
						}
						
					}

					

					sofia_glue_copy_t38_options(t38_options, other_session);

					sofia_set_flag(tech_pvt, TFLAG_T38_PASSTHRU);
					sofia_set_flag(other_tech_pvt, TFLAG_T38_PASSTHRU);

					msg = switch_core_session_alloc(other_session, sizeof(*msg));
					msg->message_id = SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA;
					msg->from = __FILE__;
					msg->string_arg = switch_core_session_strdup(other_session, r_sdp);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Passing T38 req to other leg.\n%s\n", r_sdp);
					switch_core_session_queue_message(other_session, msg);
					switch_core_session_rwunlock(other_session);
				}
			}


			/* do nothing here, mod_fax will trigger a response (if it's listening =/) */
			match = 1;
			goto done;
		} else if (m->m_type == sdp_media_audio && m->m_port && !got_audio) {
			sdp_rtpmap_t *map;

			for (attr = m->m_attributes; attr; attr = attr->a_next) {

				if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value) {
					switch_channel_set_variable(tech_pvt->channel, "sip_remote_audio_rtcp_port", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
					maxptime = atoi(attr->a_value);
				} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
					int crypto_tag;

					if (!(tech_pvt->profile->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
						!switch_true(switch_channel_get_variable(tech_pvt->channel, "sip_allow_crypto_in_avp"))) {
						if (m->m_proto != sdp_proto_srtp) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
							match = 0;
							goto done;
						}
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);

					got_crypto = switch_core_session_check_incoming_crypto(tech_pvt->session, 
																		   SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_MEDIA_TYPE_AUDIO, crypto, crypto_tag);

				}
			}

			if (got_crypto && !got_avp) {
				switch_channel_set_variable(tech_pvt->channel, SOFIA_CRYPTO_MANDATORY_VARIABLE, "true");
				switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE, "true");
			}

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

		greed:
			x = 0;

			if (tech_pvt->rm_encoding && !(sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || sofia_test_flag(tech_pvt, TFLAG_LIBERAL_DTMF))) {	// && !sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
				char *remote_host = tech_pvt->remote_sdp_audio_ip;
				switch_port_t remote_port = tech_pvt->remote_sdp_audio_port;
				int same = 0;

				if (switch_rtp_ready(tech_pvt->rtp_session)) {
					remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
					remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
				}

				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == tech_pvt->pt) ? 1 : 0;
					} else {
						match = strcasecmp(switch_str_nil(map->rm_encoding), tech_pvt->iananame) ? 0 : 1;
					}

					if (match && connection->c_address && remote_host && !strcmp(connection->c_address, remote_host) && m->m_port == remote_port) {
						same = 1;
					} else {
						same = 0;
						break;
					}
				}

				if (same) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "Our existing sdp is still good [%s %s:%d], let's keep it.\n",
									  tech_pvt->rm_encoding, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
					got_audio = 1;
				} else {
					match = 0;
					got_audio = 0;
				}
			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				uint32_t near_rate = 0;
				const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
				const char *rm_encoding;
				uint32_t map_bit_rate = 0;
				int codec_ms = 0;
				switch_codec_fmtp_t codec_fmtp = { 0 };

				if (x++ < skip) {
					continue;
				}

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				if (!strcasecmp(rm_encoding, "telephone-event")) {
					if (!best_te || map->rm_rate == tech_pvt->rm_rate) {
						best_te = (switch_payload_t) map->rm_pt;
					}
				}
				
				if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (tech_pvt->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
					}
				}

				if (match) {
					continue;
				}

				if (greedy) {
					first = mine;
					last = first + 1;
				} else {
					first = 0;
					last = tech_pvt->num_codecs;
				}

				codec_ms = ptime;

				if (maxptime && (!codec_ms || codec_ms > maxptime)) {
					codec_ms = maxptime;
				}

				if (!codec_ms) {
					codec_ms = switch_default_ptime(rm_encoding, map->rm_pt);
				}

				map_bit_rate = switch_known_bitrate((switch_payload_t)map->rm_pt);
				
				if (!ptime && !strcasecmp(map->rm_encoding, "g723")) {
					codec_ms = 30;
				}
				
				if (zstr(map->rm_fmtp)) {
					if (!strcasecmp(map->rm_encoding, "ilbc")) {
						codec_ms = 30;
						map_bit_rate = 13330;
					}
				} else {
					if ((switch_core_codec_parse_fmtp(map->rm_encoding, map->rm_fmtp, map->rm_rate, &codec_fmtp)) == SWITCH_STATUS_SUCCESS) {
						if (codec_fmtp.bits_per_second) {
							map_bit_rate = codec_fmtp.bits_per_second;
						}
						if (codec_fmtp.microseconds_per_packet) {
							codec_ms = (codec_fmtp.microseconds_per_packet / 1000);
						}
					}
				}

				
				for (i = first; i < last && i < total_codecs; i++) {
					const switch_codec_implementation_t *imp = codec_array[i];
					uint32_t bit_rate = imp->bits_per_second;
					uint32_t codec_rate = imp->samples_per_second;
					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Compare [%s:%d:%u:%d:%u]/[%s:%d:%u:%d:%u]\n",
									  rm_encoding, map->rm_pt, (int) map->rm_rate, codec_ms, map_bit_rate,
									  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000, bit_rate);
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}

					if (match && bit_rate && map_bit_rate && map_bit_rate != bit_rate && strcasecmp(map->rm_encoding, "ilbc")) {
						/* if a bit rate is specified and doesn't match, this is not a codec match, except for ILBC */
						match = 0;
					}

					if (match && map->rm_rate && codec_rate && map->rm_rate != codec_rate && (!strcasecmp(map->rm_encoding, "pcma") || !strcasecmp(map->rm_encoding, "pcmu"))) {
						/* if the sampling rate is specified and doesn't match, this is not a codec match for G.711 */
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sampling rates have to match for G.711\n");
						match = 0;
					}
					
					if (match) {
						if (scrooge) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
											  "Bah HUMBUG! Sticking with %s@%uh@%ui\n",
											  imp->iananame, imp->samples_per_second, imp->microseconds_per_packet / 1000);
						} else {
							if ((ptime && codec_ms && codec_ms * 1000 != imp->microseconds_per_packet) || map->rm_rate != codec_rate) {
								near_rate = map->rm_rate;
								near_match = imp;
								match = 0;
								continue;
							}
						}
						mimp = imp;
						break;
					}
				}

				if (!match && near_match) {
					const switch_codec_implementation_t *search[1];
					char *prefs[1];
					char tmp[80];
					int num;

					switch_snprintf(tmp, sizeof(tmp), "%s@%uh@%ui", near_match->iananame, near_rate ? near_rate : near_match->samples_per_second,
									codec_ms);

					prefs[0] = tmp;
					num = switch_loadable_module_get_codecs_sorted(search, 1, prefs, 1);

					if (num) {
						mimp = search[0];
					} else {
						mimp = near_match;
					}

					if (!maxptime || mimp->microseconds_per_packet / 1000 <= maxptime) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Substituting codec %s@%ui@%uh\n",
										  mimp->iananame, mimp->microseconds_per_packet / 1000, mimp->samples_per_second);
						match = 1;
					} else {
						mimp = NULL;
						match = 0;
					}

				}

				if (!match && greedy) {
					skip++;
					continue;
				}

				if (mimp) {
					char tmp[50];
					const char *mirror = switch_channel_get_variable(tech_pvt->channel, "sip_mirror_remote_audio_codec_payload");

					tech_pvt->rm_encoding = switch_core_session_strdup(session, (char *) map->rm_encoding);
					tech_pvt->iananame = switch_core_session_strdup(session, (char *) mimp->iananame);
					tech_pvt->pt = (switch_payload_t) map->rm_pt;
					tech_pvt->rm_rate = mimp->samples_per_second;
					tech_pvt->codec_ms = mimp->microseconds_per_packet / 1000;
					tech_pvt->bitrate = mimp->bits_per_second;
					tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					tech_pvt->rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
					tech_pvt->remote_sdp_audio_port = (switch_port_t) m->m_port;
					tech_pvt->agreed_pt = (switch_payload_t) map->rm_pt;
					tech_pvt->num_negotiated_codecs = 0;
					tech_pvt->negotiated_codecs[tech_pvt->num_negotiated_codecs++] = mimp;
					switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
					tech_pvt->audio_recv_pt = (switch_payload_t)map->rm_pt;
					
					if (!switch_true(mirror) && 
						switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND && 
						(!sofia_test_flag(tech_pvt, TFLAG_REINVITE) || sofia_test_pflag(tech_pvt->profile, PFLAG_RENEG_ON_REINVITE))) {
						sofia_glue_get_offered_pt(tech_pvt, mimp, &tech_pvt->audio_recv_pt);
					}
					
					switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->audio_recv_pt);
					switch_channel_set_variable(tech_pvt->channel, "sip_audio_recv_pt", tmp);
					
				}
				
				if (match) {
					if (sofia_glue_tech_set_codec(tech_pvt, 1) == SWITCH_STATUS_SUCCESS) {
						got_audio = 1;
					} else {
						match = 0;
					}
				}
			}

			if (!best_te && (sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || sofia_test_flag(tech_pvt, TFLAG_LIBERAL_DTMF))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
								  "No 2833 in SDP. Liberal DTMF mode adding %d as telephone-event.\n", tech_pvt->profile->te);
				best_te = tech_pvt->profile->te;
			}

			if (best_te) {
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
					te = tech_pvt->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send payload to %u\n", best_te);
					if (tech_pvt->rtp_session) {
						switch_rtp_set_telephony_event(tech_pvt->rtp_session, (switch_payload_t) best_te);
						switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_send_payload", "%d", best_te);
					}
				} else {
					te = tech_pvt->recv_te = tech_pvt->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send/recv payload to %u\n", te);
					if (tech_pvt->rtp_session) {
						switch_rtp_set_telephony_event(tech_pvt->rtp_session, te);
						switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_send_payload", "%d", te);
						switch_rtp_set_telephony_recv_event(tech_pvt->rtp_session, te);
						switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_recv_payload", "%d", te);
					}
				}
			} else {
				/* by default, use SIP INFO if 2833 is not in the SDP */
				if (!switch_false(switch_channel_get_variable(channel, "sip_info_when_no_2833"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No 2833 in SDP.  Disable 2833 dtmf and switch to INFO\n");
					switch_channel_set_variable(tech_pvt->channel, "dtmf_type", "info");
					tech_pvt->dtmf_type = DTMF_INFO;
					te = tech_pvt->recv_te = tech_pvt->te = 0;
				} else {
					switch_channel_set_variable(tech_pvt->channel, "dtmf_type", "none");
					tech_pvt->dtmf_type = DTMF_NONE;
					te = tech_pvt->recv_te = tech_pvt->te = 0;
				}
			}

			
			if (!match && greedy && mine < total_codecs) {
				mine++;
				skip = 0;
				goto greed;
			}

		} else if (m->m_type == sdp_media_video && m->m_port) {
			sdp_rtpmap_t *map;
			const char *rm_encoding;
			const switch_codec_implementation_t *mimp = NULL;
			int vmatch = 0, i;
			switch_channel_set_variable(tech_pvt->channel, "video_possible", "true");

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {

				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
						//framerate = atoi(attr->a_value);
					}
					if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value) {
						switch_channel_set_variable(tech_pvt->channel, "sip_remote_video_rtcp_port", attr->a_value);

					} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
						int crypto_tag;
						
						if (!(tech_pvt->profile->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
							!switch_true(switch_channel_get_variable(tech_pvt->channel, "sip_allow_crypto_in_avp"))) {
							if (m->m_proto != sdp_proto_srtp) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
								match = 0;
								goto done;
							}
						}
						
						crypto = attr->a_value;
						crypto_tag = atoi(crypto);
						
						got_video_crypto = switch_core_session_check_incoming_crypto(tech_pvt->session, 
																					 SOFIA_HAS_VIDEO_CRYPTO_VARIABLE, 
																					 SWITCH_MEDIA_TYPE_VIDEO, crypto, crypto_tag);
					
					}
				}

				if (got_video_crypto && !got_video_avp) {
					switch_channel_set_variable(tech_pvt->channel, SOFIA_CRYPTO_MANDATORY_VARIABLE, "true");
					switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE, "true");
				}

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				for (i = 0; i < total_codecs; i++) {
					const switch_codec_implementation_t *imp = codec_array[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d]/[%s:%d]\n",
									  rm_encoding, map->rm_pt, imp->iananame, imp->ianacode);
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						vmatch = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						vmatch = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}


					if (vmatch && (map->rm_rate == imp->samples_per_second)) {
						mimp = imp;
						break;
					} else {
						vmatch = 0;
					}
				}

				if (mimp) {
					if ((tech_pvt->video_rm_encoding = switch_core_session_strdup(session, (char *) rm_encoding))) {
						char tmp[50];
						const char *mirror = switch_channel_get_variable(tech_pvt->channel, "sip_mirror_remote_video_codec_payload");

						tech_pvt->video_pt = (switch_payload_t) map->rm_pt;
						tech_pvt->video_rm_rate = map->rm_rate;
						tech_pvt->video_codec_ms = mimp->microseconds_per_packet / 1000;
						tech_pvt->remote_sdp_video_ip = switch_core_session_strdup(session, (char *) connection->c_address);
						tech_pvt->video_rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
						tech_pvt->remote_sdp_video_port = (switch_port_t) m->m_port;
						tech_pvt->video_agreed_pt = (switch_payload_t) map->rm_pt;
						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_video_port);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
						switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE, tmp);
						switch_channel_set_variable(tech_pvt->channel, "sip_video_fmtp", tech_pvt->video_rm_fmtp);
						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->video_agreed_pt);
						switch_channel_set_variable(tech_pvt->channel, "sip_video_pt", tmp);
						sofia_glue_check_video_codecs(tech_pvt);

						tech_pvt->video_recv_pt = (switch_payload_t)map->rm_pt;
						
						if (!switch_true(mirror) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
							sofia_glue_get_offered_pt(tech_pvt, mimp, &tech_pvt->video_recv_pt);
						}

						switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->video_recv_pt);
						switch_channel_set_variable(tech_pvt->channel, "sip_video_recv_pt", tmp);
						if (!match && vmatch) match = 1;

						break;
					} else {
						vmatch = 0;
					}
				}
			}
		}
	}

 done:

	if (parser) {
		sdp_parser_free(parser);
	}

	tech_pvt->cng_pt = cng_pt;
	sofia_set_flag_locked(tech_pvt, TFLAG_SDP);

	return match;
}


switch_status_t sofia_media_activate_rtp(private_object_t *tech_pvt)
{
	const char *err = NULL;
	const char *val = NULL;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char tmp[50];
	uint32_t rtp_timeout_sec = tech_pvt->profile->rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec = tech_pvt->profile->rtp_hold_timeout_sec;
	char *timer_name = NULL;
	const char *var;
	uint32_t delay = tech_pvt->profile->rtp_digit_delay;

	switch_assert(tech_pvt != NULL);

	if (switch_channel_down(tech_pvt->channel) || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(tech_pvt->sofia_mutex);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_reset_media_timer(tech_pvt->rtp_session);
	}

	if ((var = switch_channel_get_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE)) && switch_true(var)) {
		switch_channel_set_flag(tech_pvt->channel, CF_SECURE);
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}


	if (!sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			if (sofia_test_flag(tech_pvt, TFLAG_VIDEO) && !switch_rtp_ready(tech_pvt->video_rtp_session)) {
				goto video;
			}

			status = SWITCH_STATUS_SUCCESS;
			goto end;
		} 
	}

	if ((status = sofia_glue_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	memset(flags, 0, sizeof(flags));
	flags[SWITCH_RTP_FLAG_DATAWAIT]++;

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
		!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
		flags[SWITCH_RTP_FLAG_AUTOADJ]++;
	}


	if (sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_RFC2833)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "pass_rfc2833")) && switch_true(val))) {
		sofia_set_flag(tech_pvt, TFLAG_PASS_RFC2833);
	}


	if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFLUSH)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_autoflush")) && switch_true(val))) {
		flags[SWITCH_RTP_FLAG_AUTOFLUSH]++;
	}

	if (!(sofia_test_pflag(tech_pvt->profile, PFLAG_REWRITE_TIMESTAMPS) ||
		  ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_rewrite_timestamps")) && switch_true(val)))) {
		flags[SWITCH_RTP_FLAG_RAW_WRITE]++;
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
		tech_pvt->cng_pt = 0;
	} else if (tech_pvt->cng_pt) {
		flags[SWITCH_RTP_FLAG_AUTO_CNG]++;
	}

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if (!strcasecmp(tech_pvt->read_impl.iananame, "L16")) {
		flags[SWITCH_RTP_FLAG_BYTESWAP]++;
	}
#endif
	
	if ((flags[SWITCH_RTP_FLAG_BYTESWAP]) && (val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_byteswap")) && switch_true(val)) {
		flags[SWITCH_RTP_FLAG_BYTESWAP] = 0;
	}

	if (tech_pvt->rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		//const char *ip = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
		//const char *port = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);

		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n",
							  switch_channel_get_name(tech_pvt->channel));
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				if (tech_pvt->audio_recv_pt != tech_pvt->agreed_pt) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
								  "%s Set audio receive payload in Re-INVITE for non-matching dynamic PT to %u\n", 
									  switch_channel_get_name(tech_pvt->channel), tech_pvt->audio_recv_pt);
				
					switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->audio_recv_pt);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
								  "%s Setting audio receive payload in Re-INVITE to %u\n", 
									  switch_channel_get_name(tech_pvt->channel), tech_pvt->audio_recv_pt);
					switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->agreed_pt);
				}

			}
			goto video;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n",
							  switch_channel_get_name(tech_pvt->channel),
							  remote_host, remote_port, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);

			switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
			switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
			switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
		}
	}

	if (!switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);

		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->agreed_pt);

			if (tech_pvt->audio_recv_pt != tech_pvt->agreed_pt) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
								  "%s Set audio receive payload to %u\n", switch_channel_get_name(tech_pvt->channel), tech_pvt->audio_recv_pt);
				
				switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->audio_recv_pt);
			} else {
				switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->agreed_pt);
			}

		}
	}

	switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->local_sdp_audio_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->local_sdp_audio_ip);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);

	if (tech_pvt->rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
		const char *rport = NULL;
		switch_port_t remote_rtcp_port = 0;

		

		if ((rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_audio_rtcp_port"))) {
			remote_rtcp_port = (switch_port_t)atoi(rport);
		}

		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port,
										  remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}
		goto video;
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		sofia_glue_tech_proxy_remote_addr(tech_pvt, NULL);

		memset(flags, 0, sizeof(flags));
		flags[SWITCH_RTP_FLAG_DATAWAIT]++;
		flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
			!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
			flags[SWITCH_RTP_FLAG_AUTOADJ]++;
		}
		timer_name = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
						  "PROXY AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);

		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->agreed_pt);
		}

	} else {
		timer_name = tech_pvt->profile->timer_name;

		if ((var = switch_channel_get_variable(tech_pvt->channel, "rtp_timer_name"))) {
			timer_name = (char *) var;
		}
	}

	if (switch_channel_up(tech_pvt->channel) && !sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
											   tech_pvt->local_sdp_audio_port,
											   tech_pvt->remote_sdp_audio_ip,
											   tech_pvt->remote_sdp_audio_port,
											   tech_pvt->agreed_pt,
											   tech_pvt->read_impl.samples_per_packet,
											   tech_pvt->codec_ms * 1000,
											   flags, timer_name, &err, switch_core_session_get_pool(tech_pvt->session));
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = sofia_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = sofia_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = sofia_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;
		uint32_t stun_ping = 0;
		const char *ssrc;

		switch_core_media_set_rtp_session(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, tech_pvt->rtp_session);


		if ((ssrc = switch_channel_get_variable(tech_pvt->channel, "rtp_use_ssrc"))) {
			uint32_t ssrc_ul = (uint32_t) strtoul(ssrc, NULL, 10);
			switch_rtp_set_ssrc(tech_pvt->rtp_session, ssrc_ul);
		}


		switch_channel_set_flag(tech_pvt->channel, CF_FS_RTP);

		switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_pt", "%d", tech_pvt->agreed_pt);

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_enable_vad_in")) && switch_true(val)) {
			vad_in = 1;
		}
		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_enable_vad_out")) && switch_true(val)) {
			vad_out = 1;
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_vad_in")) && switch_true(val)) {
			vad_in = 0;
		}
		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_vad_out")) && switch_true(val)) {
			vad_out = 0;
		}

		if ((tech_pvt->stun_flags & STUN_FLAG_SET) && (val = switch_channel_get_variable(tech_pvt->channel, "rtp_stun_ping"))) {
			int ival = atoi(val);

			if (ival <= 0) {
				if (switch_true(val)) {
					ival = 6;
				}
			}

			stun_ping = (ival * tech_pvt->read_impl.samples_per_second) / tech_pvt->read_impl.samples_per_packet;
		}

		tech_pvt->ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
		switch_channel_set_variable_printf(tech_pvt->channel, "rtp_use_ssrc", "%u", tech_pvt->ssrc);

		sofia_set_flag(tech_pvt, TFLAG_RTP);
		sofia_set_flag(tech_pvt, TFLAG_IO);

		if (tech_pvt->profile->auto_rtp_bugs & RTP_BUG_IGNORE_MARK_BIT) {
			tech_pvt->rtp_bugs |= RTP_BUG_IGNORE_MARK_BIT;
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_manual_rtp_bugs"))) {
			sofia_glue_parse_rtp_bugs(&tech_pvt->rtp_bugs, val);
		}

		switch_rtp_intentional_bugs(tech_pvt->rtp_session, tech_pvt->rtp_bugs | tech_pvt->profile->manual_rtp_bugs);

		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING | SWITCH_VAD_FLAG_EVENTS_TALK | SWITCH_VAD_FLAG_EVENTS_NOTALK);
			sofia_set_flag(tech_pvt, TFLAG_VAD);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP Engage VAD for %s ( %s %s )\n",
							  switch_channel_get_name(switch_core_session_get_channel(tech_pvt->session)), vad_in ? "in" : "", vad_out ? "out" : "");
		}

		if (stun_ping) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Setting stun ping to %s:%d\n", tech_pvt->stun_ip,
							  stun_ping);
			switch_rtp_activate_stun_ping(tech_pvt->rtp_session, tech_pvt->stun_ip, tech_pvt->stun_port, stun_ping,
										  (tech_pvt->stun_flags & STUN_FLAG_FUNNY) ? 1 : 0);
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtcp_audio_interval_msec")) || (val = tech_pvt->profile->rtcp_audio_interval_msec)) {
			const char *rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_audio_rtcp_port");
			switch_port_t remote_port = 0;
			if (rport) {
				remote_port = (switch_port_t)atoi(rport);
			}
			if (!strcasecmp(val, "passthru")) {
				switch_rtp_activate_rtcp(tech_pvt->rtp_session, -1, remote_port);
			} else {
				int interval = atoi(val);
				if (interval < 100 || interval > 5000) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR,
									  "Invalid rtcp interval spec [%d] must be between 100 and 5000\n", interval);
				} else {
					switch_rtp_activate_rtcp(tech_pvt->rtp_session, interval, remote_port);
				}
			}
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "jitterbuffer_msec")) || (val = tech_pvt->profile->jb_msec)) {
			int jb_msec = atoi(val);
			int maxlen = 0, max_drift = 0;
			char *p, *q;
			
			if ((p = strchr(val, ':'))) {
				p++;
				maxlen = atoi(p);
				if ((q = strchr(p, ':'))) {
					q++;
					max_drift = abs(atoi(q));
				}
			}

			if (jb_msec < 20 || jb_msec > 10000) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR,
								  "Invalid Jitterbuffer spec [%d] must be between 20 and 10000\n", jb_msec);
			} else {
				int qlen, maxqlen = 50;
				
				qlen = jb_msec / (tech_pvt->read_impl.microseconds_per_packet / 1000);

				if (qlen < 1) {
					qlen = 3;
				}

				if (maxlen) {
					maxqlen = maxlen / (tech_pvt->read_impl.microseconds_per_packet / 1000);
				}

				if (maxqlen < qlen) {
					maxqlen = qlen * 5;
				}
				if (switch_rtp_activate_jitter_buffer(tech_pvt->rtp_session, qlen, maxqlen,
													  tech_pvt->read_impl.samples_per_packet, 
													  tech_pvt->read_impl.samples_per_second, max_drift) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), 
									  SWITCH_LOG_DEBUG, "Setting Jitterbuffer to %dms (%d frames)\n", jb_msec, qlen);
					switch_channel_set_flag(tech_pvt->channel, CF_JITTERBUFFER);
					if (!switch_false(switch_channel_get_variable(tech_pvt->channel, "sip_jitter_buffer_plc"))) {
						switch_channel_set_flag(tech_pvt->channel, CF_JITTERBUFFER_PLC);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), 
									  SWITCH_LOG_WARNING, "Error Setting Jitterbuffer to %dms (%d frames)\n", jb_msec, qlen);
				}
				
			}
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				rtp_timeout_sec = v;
			}
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_hold_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				rtp_hold_timeout_sec = v;
			}
		}

		if (rtp_timeout_sec) {
			tech_pvt->max_missed_packets = (tech_pvt->read_impl.samples_per_second * rtp_timeout_sec) / tech_pvt->read_impl.samples_per_packet;

			switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			if (!rtp_hold_timeout_sec) {
				rtp_hold_timeout_sec = rtp_timeout_sec * 10;
			}
		}

		if (rtp_hold_timeout_sec) {
			tech_pvt->max_missed_hold_packets = (tech_pvt->read_impl.samples_per_second * rtp_hold_timeout_sec) / tech_pvt->read_impl.samples_per_packet;
		}

		if (tech_pvt->te) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send payload to %u\n", tech_pvt->te);
			switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
			switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_send_payload", "%d", tech_pvt->te);
		}

		if (tech_pvt->recv_te) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set 2833 dtmf receive payload to %u\n", tech_pvt->recv_te);
			switch_rtp_set_telephony_recv_event(tech_pvt->rtp_session, tech_pvt->recv_te);
			switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_recv_payload", "%d", tech_pvt->recv_te);
		}

		if (tech_pvt->audio_recv_pt != tech_pvt->agreed_pt) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
							  "%s Set audio receive payload to %u\n", switch_channel_get_name(tech_pvt->channel), tech_pvt->audio_recv_pt);

			switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->audio_recv_pt);
		}

		if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
			tech_pvt->cng_pt = 0;
		}

		if (((val = switch_channel_get_variable(tech_pvt->channel, "rtp_digit_delay")))) {
			int delayi = atoi(val);
			if (delayi < 0) delayi = 0;
			delay = (uint32_t) delayi;
		}


		if (delay) {
			switch_rtp_set_interdigit_delay(tech_pvt->rtp_session, delay);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
							  "%s Set rtp dtmf delay to %u\n", switch_channel_get_name(tech_pvt->channel), delay);
			
		}

		if (tech_pvt->cng_pt && !sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", tech_pvt->cng_pt);
			switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
		}

		switch_core_session_apply_crypto(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, SOFIA_SECURE_MEDIA_CONFIRMED_VARIABLE);

		switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
		switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
		switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);


		if (switch_channel_test_flag(tech_pvt->channel, CF_ZRTP_PASSTHRU)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_INFO, "Activating ZRTP PROXY MODE\n");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Disable NOTIMER_DURING_BRIDGE\n");
			sofia_clear_flag(tech_pvt, TFLAG_NOTIMER_DURING_BRIDGE);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Activating audio UDPTL mode\n");
			switch_rtp_udptl_mode(tech_pvt->rtp_session);
		}


	video:
		
		sofia_glue_check_video_codecs(tech_pvt);

		if (sofia_test_flag(tech_pvt, TFLAG_VIDEO) && tech_pvt->video_rm_encoding && tech_pvt->remote_sdp_video_port) {
			/******************************************************************************************/
			if (tech_pvt->video_rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
				//const char *ip = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
				//const char *port = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
				char *remote_host = switch_rtp_get_remote_host(tech_pvt->video_rtp_session);
				switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->video_rtp_session);
				


				if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_video_ip) && remote_port == tech_pvt->remote_sdp_video_port) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Video params are unchanged for %s.\n",
									  switch_channel_get_name(tech_pvt->channel));
					goto video_up;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Video params changed for %s from %s:%d to %s:%d\n",
									  switch_channel_get_name(tech_pvt->channel),
									  remote_host, remote_port, tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port);
				}
			}

			if (!switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
								  "VIDEO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n", switch_channel_get_name(tech_pvt->channel),
								  tech_pvt->local_sdp_audio_ip, tech_pvt->local_sdp_video_port, tech_pvt->remote_sdp_video_ip,
								  tech_pvt->remote_sdp_video_port, tech_pvt->video_agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);

				if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
					switch_rtp_set_default_payload(tech_pvt->video_rtp_session, tech_pvt->video_agreed_pt);
				}
			}
			
			switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->local_sdp_video_port);
			switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
			switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, tmp);


			if (tech_pvt->video_rtp_session && sofia_test_flag(tech_pvt, TFLAG_REINVITE)) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = 0;

				sofia_clear_flag_locked(tech_pvt, TFLAG_REINVITE);

				if ((rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_video_rtcp_port"))) {
					remote_rtcp_port = (switch_port_t)atoi(rport);
				}
				
				if (switch_rtp_set_remote_address
					(tech_pvt->video_rtp_session, tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port, remote_rtcp_port, SWITCH_TRUE,
					 &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port);
					if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
						!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(tech_pvt->video_rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}

				}
				goto video_up;
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				sofia_glue_tech_proxy_remote_addr(tech_pvt, NULL);

				memset(flags, 0, sizeof(flags));
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
				flags[SWITCH_RTP_FLAG_DATAWAIT]++;

				if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
					!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
					flags[SWITCH_RTP_FLAG_AUTOADJ]++;
				}
				timer_name = NULL;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
								  "PROXY VIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
								  switch_channel_get_name(tech_pvt->channel),
								  tech_pvt->local_sdp_audio_ip,
								  tech_pvt->local_sdp_video_port,
								  tech_pvt->remote_sdp_video_ip,
								  tech_pvt->remote_sdp_video_port, tech_pvt->video_agreed_pt, tech_pvt->read_impl.microseconds_per_packet / 1000);

				if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
					switch_rtp_set_default_payload(tech_pvt->video_rtp_session, tech_pvt->video_agreed_pt);
				}
			} else {
				timer_name = tech_pvt->profile->timer_name;

				if ((var = switch_channel_get_variable(tech_pvt->channel, "rtp_timer_name"))) {
					timer_name = (char *) var;
				}
			}

			/******************************************************************************************/

			if (tech_pvt->video_rtp_session) {
				goto video_up;
			}


			if (!tech_pvt->local_sdp_video_port) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
			}

			memset(flags, 0, sizeof(flags));
			flags[SWITCH_RTP_FLAG_DATAWAIT]++;
			flags[SWITCH_RTP_FLAG_RAW_WRITE]++;

			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				flags[SWITCH_RTP_FLAG_AUTOADJ]++;				
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
			}
			sofia_glue_tech_set_video_codec(tech_pvt, 0);

			flags[SWITCH_RTP_FLAG_USE_TIMER] = 0;
			flags[SWITCH_RTP_FLAG_NOBLOCK] = 0;
			flags[SWITCH_RTP_FLAG_VIDEO]++;

			tech_pvt->video_rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
														 tech_pvt->local_sdp_video_port,
														 tech_pvt->remote_sdp_video_ip,
														 tech_pvt->remote_sdp_video_port,
														 tech_pvt->video_agreed_pt,
														 1, 90000, flags, NULL, &err, switch_core_session_get_pool(tech_pvt->session));

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%sVIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(tech_pvt->channel),
							  tech_pvt->local_sdp_audio_ip,
							  tech_pvt->local_sdp_video_port,
							  tech_pvt->remote_sdp_video_ip,
							  tech_pvt->remote_sdp_video_port, tech_pvt->video_agreed_pt,
							  0, switch_rtp_ready(tech_pvt->video_rtp_session) ? "SUCCESS" : err);


			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				switch_rtp_set_default_payload(tech_pvt->video_rtp_session, tech_pvt->video_agreed_pt);
				switch_core_media_set_rtp_session(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO, tech_pvt->video_rtp_session);
			}

			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				const char *ssrc;
				switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
				if ((ssrc = switch_channel_get_variable(tech_pvt->channel, "rtp_use_video_ssrc"))) {
					uint32_t ssrc_ul = (uint32_t) strtoul(ssrc, NULL, 10);
					switch_rtp_set_ssrc(tech_pvt->video_rtp_session, ssrc_ul);
				}



				if ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_manual_video_rtp_bugs"))) {
					sofia_glue_parse_rtp_bugs(&tech_pvt->video_rtp_bugs, val);
				}
				
				switch_rtp_intentional_bugs(tech_pvt->video_rtp_session, tech_pvt->video_rtp_bugs | tech_pvt->profile->manual_video_rtp_bugs);

				if (tech_pvt->video_recv_pt != tech_pvt->video_agreed_pt) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
									  "%s Set video receive payload to %u\n", switch_channel_get_name(tech_pvt->channel), tech_pvt->video_recv_pt);
					switch_rtp_set_recv_pt(tech_pvt->video_rtp_session, tech_pvt->video_recv_pt);
				}

				switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_video_pt", "%d", tech_pvt->video_agreed_pt);
				tech_pvt->video_ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
				switch_channel_set_variable_printf(tech_pvt->channel, "rtp_use_video_ssrc", "%u", tech_pvt->ssrc);

				switch_core_session_apply_crypto(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO, SOFIA_SECURE_VIDEO_CONFIRMED_VARIABLE);


				if ((val = switch_channel_get_variable(tech_pvt->channel, "rtcp_audio_interval_msec"))
					|| (val = tech_pvt->profile->rtcp_audio_interval_msec)) {
					const char *rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_video_rtcp_port");
					switch_port_t remote_port = 0;
					if (rport) {
						remote_port = (switch_port_t)atoi(rport);
					}
					if (!strcasecmp(val, "passthru")) {
						switch_rtp_activate_rtcp(tech_pvt->rtp_session, -1, remote_port);
					} else {
						int interval = atoi(val);
						if (interval < 100 || interval > 5000) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR,
											  "Invalid rtcp interval spec [%d] must be between 100 and 5000\n", interval);
						} else {
							switch_rtp_activate_rtcp(tech_pvt->rtp_session, interval, remote_port);
						}
					}
				}
				if (switch_channel_test_flag(tech_pvt->channel, CF_ZRTP_PASSTHRU)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Activating video UDPTL mode\n");
					switch_rtp_udptl_mode(tech_pvt->video_rtp_session);
				}

			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end;
			}
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		sofia_clear_flag_locked(tech_pvt, TFLAG_IO);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

 video_up:

	sofia_set_flag(tech_pvt, TFLAG_IO);
	status = SWITCH_STATUS_SUCCESS;

 end:

	sofia_clear_flag_locked(tech_pvt, TFLAG_REINVITE);
	switch_core_recovery_track(tech_pvt->session);

	switch_mutex_unlock(tech_pvt->sofia_mutex);

	return status;

}



void sofia_media_set_sdp_codec_string(switch_core_session_t *session, const char *r_sdp)
{
	sdp_parser_t *parser;
	sdp_session_t *sdp;
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {

		if ((sdp = sdp_session(parser))) {
			sofia_glue_set_r_sdp_codec_string(session, sofia_glue_get_codec_string(tech_pvt), sdp);
		}

		sdp_parser_free(parser);
	}

}

switch_status_t sofia_glue_tech_set_video_codec(private_object_t *tech_pvt, int force)
{

	if (!tech_pvt->video_rm_encoding) {
		return SWITCH_STATUS_FALSE;
	}

	if (tech_pvt->video_read_codec.implementation && switch_core_codec_ready(&tech_pvt->video_read_codec)) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding) ||
			tech_pvt->video_read_codec.implementation->samples_per_second != tech_pvt->video_rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding);
			switch_core_codec_destroy(&tech_pvt->video_read_codec);
			switch_core_codec_destroy(&tech_pvt->video_write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Already using %s\n",
							  tech_pvt->video_read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}



	if (switch_core_codec_init(&tech_pvt->video_read_codec,
							   tech_pvt->video_rm_encoding,
							   tech_pvt->video_rm_fmtp,
							   tech_pvt->video_rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&tech_pvt->video_write_codec,
								   tech_pvt->video_rm_encoding,
								   tech_pvt->video_rm_fmtp,
								   tech_pvt->video_rm_rate,
								   0,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			tech_pvt->video_read_frame.rate = tech_pvt->video_rm_rate;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(tech_pvt->channel), tech_pvt->video_rm_encoding, tech_pvt->video_rm_rate, tech_pvt->video_codec_ms);
			tech_pvt->video_read_frame.codec = &tech_pvt->video_read_codec;

			tech_pvt->video_fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->video_write_codec.fmtp_out);

			tech_pvt->video_write_codec.agreed_pt = tech_pvt->video_agreed_pt;
			tech_pvt->video_read_codec.agreed_pt = tech_pvt->video_agreed_pt;
			switch_core_session_set_video_read_codec(tech_pvt->session, &tech_pvt->video_read_codec);
			switch_core_session_set_video_write_codec(tech_pvt->session, &tech_pvt->video_write_codec);


			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				switch_core_session_message_t msg = { 0 };

				msg.from = __FILE__;
				msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;

				switch_rtp_set_default_payload(tech_pvt->video_rtp_session, tech_pvt->video_agreed_pt);
				
				if (tech_pvt->video_recv_pt != tech_pvt->video_agreed_pt) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
									  "%s Set video receive payload to %u\n", switch_channel_get_name(tech_pvt->channel), tech_pvt->video_recv_pt);
					
					switch_rtp_set_recv_pt(tech_pvt->video_rtp_session, tech_pvt->video_recv_pt);
				} else {
					switch_rtp_set_recv_pt(tech_pvt->video_rtp_session, tech_pvt->video_agreed_pt);
				}

				switch_core_session_receive_message(tech_pvt->session, &msg);


			}

			switch_channel_set_variable(tech_pvt->channel, "sip_use_video_codec_name", tech_pvt->video_rm_encoding);
			switch_channel_set_variable(tech_pvt->channel, "sip_use_video_codec_fmtp", tech_pvt->video_rm_fmtp);
			switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_video_codec_rate", "%d", tech_pvt->video_rm_rate);
			switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_video_codec_ptime", "%d", 0);

		}
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_tech_set_codec(private_object_t *tech_pvt, int force)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int resetting = 0;

	if (!tech_pvt->iananame) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "No audio codec available\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_ready(&tech_pvt->read_codec)) {
		if (!force) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
		if (strcasecmp(tech_pvt->read_impl.iananame, tech_pvt->iananame) ||
			tech_pvt->read_impl.samples_per_second != tech_pvt->rm_rate ||
			tech_pvt->codec_ms != (uint32_t) tech_pvt->read_impl.microseconds_per_packet / 1000) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, 
							  "Changing Codec from %s@%dms@%dhz to %s@%dms@%luhz\n",
							  tech_pvt->read_impl.iananame, tech_pvt->read_impl.microseconds_per_packet / 1000,
							  tech_pvt->read_impl.samples_per_second,
							  tech_pvt->rm_encoding, 
							  tech_pvt->codec_ms,
							  tech_pvt->rm_rate);
			
			switch_yield(tech_pvt->read_impl.microseconds_per_packet);
			switch_core_session_lock_codec_write(tech_pvt->session);
			switch_core_session_lock_codec_read(tech_pvt->session);
			resetting = 1;
			switch_yield(tech_pvt->read_impl.microseconds_per_packet);
			switch_core_codec_destroy(&tech_pvt->read_codec);
			switch_core_codec_destroy(&tech_pvt->write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->read_impl.iananame);
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
	}

	if (switch_core_codec_init_with_bitrate(&tech_pvt->read_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   tech_pvt->bitrate,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	
	tech_pvt->read_codec.session = tech_pvt->session;


	if (switch_core_codec_init_with_bitrate(&tech_pvt->write_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   tech_pvt->bitrate,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	tech_pvt->write_codec.session = tech_pvt->session;

	switch_channel_set_variable(tech_pvt->channel, "sip_use_codec_name", tech_pvt->iananame);
	switch_channel_set_variable(tech_pvt->channel, "sip_use_codec_fmtp", tech_pvt->rm_fmtp);
	switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_codec_rate", "%d", tech_pvt->rm_rate);
	switch_channel_set_variable_printf(tech_pvt->channel, "sip_use_codec_ptime", "%d", tech_pvt->codec_ms);


	switch_assert(tech_pvt->read_codec.implementation);
	switch_assert(tech_pvt->write_codec.implementation);

	tech_pvt->read_impl = *tech_pvt->read_codec.implementation;
	tech_pvt->write_impl = *tech_pvt->write_codec.implementation;

	switch_core_session_set_read_impl(tech_pvt->session, tech_pvt->read_codec.implementation);
	switch_core_session_set_write_impl(tech_pvt->session, tech_pvt->write_codec.implementation);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_assert(tech_pvt->read_codec.implementation);
		
		if (switch_rtp_change_interval(tech_pvt->rtp_session,
									   tech_pvt->read_impl.microseconds_per_packet, 
									   tech_pvt->read_impl.samples_per_packet) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	tech_pvt->read_frame.rate = tech_pvt->rm_rate;

	if (!switch_core_codec_ready(&tech_pvt->read_codec)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples %d bits\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->iananame, tech_pvt->rm_rate, tech_pvt->codec_ms,
					  tech_pvt->read_impl.samples_per_packet, tech_pvt->read_impl.bits_per_second);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	tech_pvt->write_codec.agreed_pt = tech_pvt->agreed_pt;
	tech_pvt->read_codec.agreed_pt = tech_pvt->agreed_pt;

	if (force != 2) {
		switch_core_session_set_real_read_codec(tech_pvt->session, &tech_pvt->read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	}

	tech_pvt->fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->write_codec.fmtp_out);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->pt);
	}

 end:
	if (resetting) {
		switch_core_session_unlock_codec_write(tech_pvt->session);
		switch_core_session_unlock_codec_read(tech_pvt->session);
	}

	sofia_glue_tech_set_video_codec(tech_pvt, force);

	return status;
}



static void add_audio_codec(sdp_rtpmap_t *map, int ptime, char *buf, switch_size_t buflen)
{
	int codec_ms = ptime;
	uint32_t map_bit_rate = 0;
	char ptstr[20] = "";
	char ratestr[20] = "";
	char bitstr[20] = "";
	switch_codec_fmtp_t codec_fmtp = { 0 };
						
	if (!codec_ms) {
		codec_ms = switch_default_ptime(map->rm_encoding, map->rm_pt);
	}

	map_bit_rate = switch_known_bitrate((switch_payload_t)map->rm_pt);
				
	if (!ptime && !strcasecmp(map->rm_encoding, "g723")) {
		ptime = codec_ms = 30;
	}
				
	if (zstr(map->rm_fmtp)) {
		if (!strcasecmp(map->rm_encoding, "ilbc")) {
			ptime = codec_ms = 30;
			map_bit_rate = 13330;
		}
	} else {
		if ((switch_core_codec_parse_fmtp(map->rm_encoding, map->rm_fmtp, map->rm_rate, &codec_fmtp)) == SWITCH_STATUS_SUCCESS) {
			if (codec_fmtp.bits_per_second) {
				map_bit_rate = codec_fmtp.bits_per_second;
			}
			if (codec_fmtp.microseconds_per_packet) {
				codec_ms = (codec_fmtp.microseconds_per_packet / 1000);
			}
		}
	}

	if (map->rm_rate) {
		switch_snprintf(ratestr, sizeof(ratestr), "@%uh", (unsigned int) map->rm_rate);
	}

	if (codec_ms) {
		switch_snprintf(ptstr, sizeof(ptstr), "@%di", codec_ms);
	}

	if (map_bit_rate) {
		switch_snprintf(bitstr, sizeof(bitstr), "@%db", map_bit_rate);
	}

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), ",%s%s%s%s", map->rm_encoding, ratestr, ptstr, bitstr);

}


void sofia_glue_set_r_sdp_codec_string(switch_core_session_t *session, const char *codec_string, sdp_session_t *sdp)
{
	char buf[1024] = { 0 };
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0;
	sdp_connection_t *connection;
	sdp_rtpmap_t *map;
	short int match = 0;
	int i;
	int already_did[128] = { 0 };
	int num_codecs = 0;
	char *codec_order[SWITCH_MAX_CODECS];
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS] = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	int prefer_sdp = 0;
	const char *var;

	if ((var = switch_channel_get_variable(channel, "ep_codec_prefer_sdp")) && switch_true(var)) {
		prefer_sdp = 1;
	}
		
	if (!zstr(codec_string)) {
		char *tmp_codec_string;
		if ((tmp_codec_string = strdup(codec_string))) {
			num_codecs = switch_separate_string(tmp_codec_string, ',', codec_order, SWITCH_MAX_CODECS);
			num_codecs = switch_loadable_module_get_codecs_sorted(codecs, SWITCH_MAX_CODECS, codec_order, num_codecs);
			switch_safe_free(tmp_codec_string);
		}
	} else {
		num_codecs = switch_loadable_module_get_codecs(codecs, SWITCH_MAX_CODECS);
	}

	if (!channel || !num_codecs) {
		return;
	}

	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (zstr(attr->a_name)) {
			continue;
		}
		if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
			break;
		}
	}

	find_zrtp_hash(session, sdp);
	sofia_glue_pass_zrtp_hash(session);

	for (m = sdp->sdp_media; m; m = m->m_next) {
		ptime = dptime;
		if (m->m_type == sdp_media_image && m->m_port) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",t38");
		} else if (m->m_type == sdp_media_audio && m->m_port) {
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (zstr(attr->a_name)) {
					continue;
				}
				if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
					break;
				}
			}
			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				break;
			}

			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND || prefer_sdp) {
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if (map->rm_pt > 127 || already_did[map->rm_pt]) {
						continue;
					}

					for (i = 0; i < num_codecs; i++) {
						const switch_codec_implementation_t *imp = codecs[i];

						if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
							match = (map->rm_pt == imp->ianacode) ? 1 : 0;
						} else {
							if (map->rm_encoding) {
								match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
							} else {
								match = 0;
							}
						}

						if (match) {
							add_audio_codec(map, ptime, buf, sizeof(buf));
							break;
						}
					
					}
				}

			} else {
				for (i = 0; i < num_codecs; i++) {
					const switch_codec_implementation_t *imp = codecs[i];
					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO || imp->ianacode > 127 || already_did[imp->ianacode]) {
						continue;
					}
					for (map = m->m_rtpmaps; map; map = map->rm_next) {
						if (map->rm_pt > 127 || already_did[map->rm_pt]) {
							continue;
						}

						if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
							match = (map->rm_pt == imp->ianacode) ? 1 : 0;
						} else {
							if (map->rm_encoding) {
								match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
							} else {
								match = 0;
							}
						}

						if (match) {
							add_audio_codec(map, ptime, buf, sizeof(buf));
							break;
						}
					}
				}
			}

		} else if (m->m_type == sdp_media_video && m->m_port) {
			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				break;
			}
			for (i = 0; i < num_codecs; i++) {
				const switch_codec_implementation_t *imp = codecs[i];
				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO || imp->ianacode > 127 || already_did[imp->ianacode]) {
					continue;
				}
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if (map->rm_pt > 127 || already_did[map->rm_pt]) {
						continue;
					}

					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						if (map->rm_encoding) {
							match = strcasecmp(map->rm_encoding, imp->iananame) ? 0 : 1;
						} else {
							match = 0;
						}
					}

					if (match) {
						if (ptime > 0) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh@%di", imp->iananame, (unsigned int) map->rm_rate,
											ptime);
						} else {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh", imp->iananame, (unsigned int) map->rm_rate);
						}
						already_did[imp->ianacode] = 1;
						break;
					}
				}
			}
		}
	}
	if (buf[0] == ',') {
		switch_channel_set_variable(channel, "ep_codec_string", buf + 1);
	}
}

switch_status_t sofia_glue_tech_media(private_object_t *tech_pvt, const char *r_sdp)
{
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (zstr(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((match = sofia_glue_negotiate_sdp(tech_pvt->session, r_sdp))) {
		if (sofia_glue_tech_choose_port(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		if (sofia_media_activate_rtp(tech_pvt) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "EARLY MEDIA");
		sofia_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
		switch_channel_mark_pre_answered(tech_pvt->channel);
		return SWITCH_STATUS_SUCCESS;
	}


	return SWITCH_STATUS_FALSE;
}

int sofia_glue_toggle_hold(private_object_t *tech_pvt, int sendonly)
{
	int changed = 0;

	if (sofia_test_flag(tech_pvt, TFLAG_SLA_BARGE) || sofia_test_flag(tech_pvt, TFLAG_SLA_BARGING)) {
		switch_channel_mark_hold(tech_pvt->channel, sendonly);
		return 0;
	}

	if (sendonly && switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
		if (!sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *stream;
			const char *msg = "hold";

			if (sofia_test_pflag(tech_pvt->profile, PFLAG_MANAGE_SHARED_APPEARANCE)) {
				const char *info = switch_channel_get_variable(tech_pvt->channel, "presence_call_info");
				if (info) {
					if (switch_stristr("private", info)) {
						msg = "hold-private";
					}
				}
			}

			sofia_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_mark_hold(tech_pvt->channel, SWITCH_TRUE);
			switch_channel_presence(tech_pvt->channel, "unknown", msg, NULL);
			changed = 1;

			if (tech_pvt->max_missed_hold_packets) {
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_hold_packets);
			}

			if (!(stream = switch_channel_get_hold_music(tech_pvt->channel))) {
				stream = tech_pvt->profile->hold_music;
			}

			if (stream && strcasecmp(stream, "silence")) {
				if (!strcasecmp(stream, "indicate_hold")) {
					switch_channel_set_flag(tech_pvt->channel, CF_SUSPEND);
					switch_channel_set_flag(tech_pvt->channel, CF_HOLD);
					switch_ivr_hold_uuid(switch_channel_get_partner_uuid(tech_pvt->channel), NULL, 0);
				} else {
					switch_ivr_broadcast(switch_channel_get_partner_uuid(tech_pvt->channel), stream,
										 SMF_ECHO_ALEG | SMF_LOOP | SMF_PRIORITY);
					switch_yield(250000);
				}
			}
		}
	} else {
		if (sofia_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
			sofia_set_flag(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_mark_hold(tech_pvt->channel, SWITCH_TRUE);
			changed = 1;
		}

		sofia_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);

		if (sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *uuid;
			switch_core_session_t *b_session;

			switch_yield(250000);

			if (tech_pvt->max_missed_packets) {
				switch_rtp_reset_media_timer(tech_pvt->rtp_session);
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			}

			if ((uuid = switch_channel_get_partner_uuid(tech_pvt->channel)) && (b_session = switch_core_session_locate(uuid))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);

				if (switch_channel_test_flag(tech_pvt->channel, CF_HOLD)) {
					switch_ivr_unhold(b_session);
					switch_channel_clear_flag(tech_pvt->channel, CF_SUSPEND);
					switch_channel_clear_flag(tech_pvt->channel, CF_HOLD);
				} else {
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
				}
				switch_core_session_rwunlock(b_session);
			}

			sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_mark_hold(tech_pvt->channel, SWITCH_FALSE);
			switch_channel_presence(tech_pvt->channel, "unknown", "unhold", NULL);
			changed = 1;
		}
	}

	return changed;
}

void sofia_glue_copy_t38_options(switch_t38_options_t *t38_options, switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_t38_options_t *local_t38_options = switch_channel_get_private(channel, "t38_options");

	switch_assert(t38_options);
	
	if (!local_t38_options) {
		local_t38_options = switch_core_session_alloc(session, sizeof(switch_t38_options_t));
	}

	local_t38_options->T38MaxBitRate = t38_options->T38MaxBitRate;
	local_t38_options->T38FaxFillBitRemoval = t38_options->T38FaxFillBitRemoval;
	local_t38_options->T38FaxTranscodingMMR = t38_options->T38FaxTranscodingMMR;
	local_t38_options->T38FaxTranscodingJBIG = t38_options->T38FaxTranscodingJBIG;
	local_t38_options->T38FaxRateManagement = switch_core_session_strdup(session, t38_options->T38FaxRateManagement);
	local_t38_options->T38FaxMaxBuffer = t38_options->T38FaxMaxBuffer;
	local_t38_options->T38FaxMaxDatagram = t38_options->T38FaxMaxDatagram;
	local_t38_options->T38FaxUdpEC = switch_core_session_strdup(session, t38_options->T38FaxUdpEC);
	local_t38_options->T38VendorInfo = switch_core_session_strdup(session, t38_options->T38VendorInfo);
	local_t38_options->remote_ip = switch_core_session_strdup(session, t38_options->remote_ip);
	local_t38_options->remote_port = t38_options->remote_port;


	switch_channel_set_private(channel, "t38_options", local_t38_options);

}

static void generate_m(private_object_t *tech_pvt, char *buf, size_t buflen, 
					   switch_port_t port,
					   int cur_ptime, const char *append_audio, const char *sr, int use_cng, int cng_type, switch_event_t *map, int verbose_sdp, int secure)
{
	int i = 0;
	int rate;
	int already_did[128] = { 0 };
	int ptime = 0, noptime = 0;
	const char *local_audio_crypto_key = switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);


	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "m=audio %d RTP/%sAVP", 
					port, secure ? "S" : "");
				
	

	for (i = 0; i < tech_pvt->num_codecs; i++) {
		const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
		int this_ptime = (imp->microseconds_per_packet / 1000);

		if (!strcasecmp(imp->iananame, "ilbc")) {
			this_ptime = 20;
		}

		if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
			continue;
		}

		if (!noptime) {
			if (!cur_ptime) {
#if 0
				if (ptime) {
					if (ptime != this_ptime) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
										  "Codec %s payload %d added to sdp wanting ptime %d but it's already %d (%s:%d:%d), disabling ptime.\n", 
										  imp->iananame,
										  tech_pvt->ianacodes[i],
										  this_ptime,
										  ptime,
										  tech_pvt->codecs[0]->iananame,
										  tech_pvt->codecs[0]->ianacode,
										  ptime);
						ptime = 0;
						noptime = 1;
					}
				} else {
					ptime = this_ptime;
				}
#else
				if (!ptime) {
					ptime = this_ptime;
				}
#endif
			} else {
				if (this_ptime != cur_ptime) {
					continue;
				}
			}
		}

		if (tech_pvt->ianacodes[i] < 128) {
			if (already_did[tech_pvt->ianacodes[i]]) {
				continue;
			}

			already_did[tech_pvt->ianacodes[i]] = 1;
		}

		
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", tech_pvt->ianacodes[i]);
	}

	if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", tech_pvt->te);
	}
		
	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && cng_type && use_cng) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", cng_type);
	}
		
	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "\n");


	memset(already_did, 0, sizeof(already_did));
		
	for (i = 0; i < tech_pvt->num_codecs; i++) {
		const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
		char *fmtp = imp->fmtp;
		int this_ptime = imp->microseconds_per_packet / 1000;

		if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
			continue;
		}

		if (!strcasecmp(imp->iananame, "ilbc")) {
			this_ptime = 20;
		}

		if (!noptime) {
			if (!cur_ptime) {
				if (!ptime) {
					ptime = this_ptime;
				}
			} else {
				if (this_ptime != cur_ptime) {
					continue;
				}
			}
		}
		
		if (tech_pvt->ianacodes[i] < 128) {
			if (already_did[tech_pvt->ianacodes[i]]) {
				continue;
			}
			
			already_did[tech_pvt->ianacodes[i]] = 1;
		}

		
		rate = imp->samples_per_second;

		if (map) {
			char key[128] = "";
			char *check = NULL;
			switch_snprintf(key, sizeof(key), "%s:%u", imp->iananame, imp->bits_per_second);

			if ((check = switch_event_get_header(map, key)) || (check = switch_event_get_header(map, imp->iananame))) {
				fmtp = check;
			}
		}
		
		if (tech_pvt->ianacodes[i] > 95 || verbose_sdp) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d %s/%d\n", tech_pvt->ianacodes[i], imp->iananame, rate);
		}

		if (fmtp) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->ianacodes[i], fmtp);
		}
	}


	if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || sofia_test_flag(tech_pvt, TFLAG_LIBERAL_DTMF)) 
		&& tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", tech_pvt->te, tech_pvt->te);
	}

	if (secure) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=crypto:%s\n", local_audio_crypto_key);
		//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
	}

	if (!cng_type) {
		//switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d CN/8000\n", cng_type);
		//} else {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=silenceSupp:off - - - -\n");
	}

	if (append_audio) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "%s%s", append_audio, end_of(append_audio) == '\n' ? "" : "\n");
	}

	if (!cur_ptime) {
		cur_ptime = ptime;
	}
	
	if (!noptime && cur_ptime) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ptime:%d\n", cur_ptime);
	}

	if (tech_pvt->local_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n",
						  tech_pvt->local_sdp_audio_zrtp_hash);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=zrtp-hash:%s\n",
						tech_pvt->local_sdp_audio_zrtp_hash);
	}

	if (!zstr(sr)) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=%s\n", sr);
	}
}


#define SDPBUFLEN 65536
void sofia_glue_set_local_sdp(private_object_t *tech_pvt, const char *ip, switch_port_t port, const char *sr, int force)
{
	char *buf;
	int ptime = 0;
	uint32_t rate = 0;
	uint32_t v_port;
	int use_cng = 1;
	const char *val;
	const char *family;
	const char *pass_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_video_fmtp");
	const char *ov_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_force_video_fmtp");
	const char *append_audio = switch_channel_get_variable(tech_pvt->channel, "sip_append_audio_sdp");
	char srbuf[128] = "";
	const char *var_val;
	const char *username = tech_pvt->profile->username;
	const char *fmtp_out = tech_pvt->fmtp_out;
	const char *fmtp_out_var = switch_channel_get_variable(tech_pvt->channel, "sip_force_audio_fmtp");
	switch_event_t *map = NULL, *ptmap = NULL;
	const char *b_sdp = NULL;
	int verbose_sdp = 0;
	const char *local_audio_crypto_key = switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);

	switch_zmalloc(buf, SDPBUFLEN);
	
	sofia_glue_check_dtmf_type(tech_pvt);

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
		use_cng = 0;
		tech_pvt->cng_pt = 0;
	}

	if (!tech_pvt->payload_space) {
		int i;

		tech_pvt->payload_space = 98;

		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

			tech_pvt->ianacodes[i] = imp->ianacode;
			
			if (tech_pvt->ianacodes[i] > 64) {
				if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95 && tech_pvt->te == tech_pvt->payload_space) {
					tech_pvt->payload_space++;
				}
				if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) &&
					tech_pvt->cng_pt && use_cng  && tech_pvt->cng_pt == tech_pvt->payload_space) {
					tech_pvt->payload_space++;
				}
				tech_pvt->ianacodes[i] = tech_pvt->payload_space++;
			}
		}
	}

	if (fmtp_out_var) {
		fmtp_out = fmtp_out_var;
	}

	if ((val = switch_channel_get_variable(tech_pvt->channel, "verbose_sdp")) && switch_true(val)) {
		verbose_sdp = 1;
	}

	if (!force && !ip && zstr(sr)
		&& (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA))) {
		switch_safe_free(buf);
		return;
	}

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}

	if (!ip) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO IP!\n", switch_channel_get_name(tech_pvt->channel));
		switch_safe_free(buf);
		return;
	}

	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}

	if (!port) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO PORT!\n", switch_channel_get_name(tech_pvt->channel));
		switch_safe_free(buf);
		return;
	}

	if (!tech_pvt->rm_encoding && (b_sdp = switch_channel_get_variable(tech_pvt->channel, SWITCH_B_SDP_VARIABLE))) {
		sofia_glue_sdp_map(b_sdp, &map, &ptmap);
	}

	if (zstr(sr)) {
		if ((var_val = switch_channel_get_variable(tech_pvt->channel, "media_audio_mode"))) {
			sr = var_val;
		} else {
			sr = "sendrecv";
		}
	}

	if (!tech_pvt->owner_id) {
		tech_pvt->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!tech_pvt->session_id) {
		tech_pvt->session_id = tech_pvt->owner_id;
	}

	if (switch_true(switch_channel_get_variable_dup(tech_pvt->channel, "drop_dtmf", SWITCH_FALSE, -1))) {
		sofia_set_flag(tech_pvt, TFLAG_DROP_DTMF);
	}

	tech_pvt->session_id++;

	if ((tech_pvt->profile->ndlb & PFLAG_NDLB_SENDRECV_IN_SESSION) ||
		((var_val = switch_channel_get_variable(tech_pvt->channel, "ndlb_sendrecv_in_session")) && switch_true(var_val))) {
		if (!zstr(sr)) {
			switch_snprintf(srbuf, sizeof(srbuf), "a=%s\n", sr);
		}
		sr = NULL;
	}

	family = strchr(ip, ':') ? "IP6" : "IP4";
	switch_snprintf(buf, SDPBUFLEN,
					"v=0\n"
					"o=%s %010u %010u IN %s %s\n"
					"s=%s\n"
					"c=IN %s %s\n" "t=0 0\n"
					"%s",
					username, tech_pvt->owner_id, tech_pvt->session_id, family, ip, username, family, ip, srbuf);

	if (tech_pvt->rm_encoding) {
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=audio %d RTP/%sAVP", 
						port, (!zstr(local_audio_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE)) ? "S" : "");

		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", tech_pvt->pt);

		if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || sofia_test_flag(tech_pvt, TFLAG_LIBERAL_DTMF)) && tech_pvt->te > 95) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", tech_pvt->te);
		}
		
		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", tech_pvt->cng_pt);
		}
		
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\n");

		rate = tech_pvt->rm_rate;
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\n", tech_pvt->agreed_pt, tech_pvt->rm_encoding, rate);
		if (fmtp_out) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->agreed_pt, fmtp_out);
		}

		if (tech_pvt->read_codec.implementation && !ptime) {
			ptime = tech_pvt->read_codec.implementation->microseconds_per_packet / 1000;
		}


		if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || sofia_test_flag(tech_pvt, TFLAG_LIBERAL_DTMF))
			&& tech_pvt->te > 95) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", tech_pvt->te, tech_pvt->te);
		}
		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d CN/8000\n", tech_pvt->cng_pt);
			if (!tech_pvt->rm_encoding) {
				tech_pvt->cng_pt = 0;
			}
		} else {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=silenceSupp:off - - - -\n");
		}

		if (append_audio) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s%s", append_audio, end_of(append_audio) == '\n' ? "" : "\n");
		}

		if (ptime) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ptime:%d\n", ptime);
		}


		if (tech_pvt->local_sdp_audio_zrtp_hash) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n",
							  tech_pvt->local_sdp_audio_zrtp_hash);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n",
							tech_pvt->local_sdp_audio_zrtp_hash);
		}

		if (!zstr(sr)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=%s\n", sr);
		}
	
		if (!zstr(local_audio_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\n", local_audio_crypto_key);
			//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=encryption:optional\n");
		}

	} else if (tech_pvt->num_codecs) {
		int i;
		int cur_ptime = 0, this_ptime = 0, cng_type = 0;
		const char *mult;

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
			cng_type = tech_pvt->cng_pt;

			if (!tech_pvt->rm_encoding) {
				tech_pvt->cng_pt = 0;
			}
		}
		
		mult = switch_channel_get_variable(tech_pvt->channel, "sdp_m_per_ptime");
		
		if (mult && switch_false(mult)) {
			char *bp = buf;
			int both = 1;

			if ((!zstr(local_audio_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE))) {
				generate_m(tech_pvt, buf, SDPBUFLEN, port, 0, append_audio, sr, use_cng, cng_type, map, verbose_sdp, 1);
				bp = (buf + strlen(buf));

				/* asterisk can't handle AVP and SAVP in sep streams, way to blow off the spec....*/
				if (switch_true(switch_channel_get_variable(tech_pvt->channel, "sdp_secure_savp_only"))) {
					both = 0;
				}

			}

			if (both) {
				generate_m(tech_pvt, bp, SDPBUFLEN - strlen(buf), port, 0, append_audio, sr, use_cng, cng_type, map, verbose_sdp, 0);
			}

		} else {

			for (i = 0; i < tech_pvt->num_codecs; i++) {
				const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
				
				if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
					continue;
				}
				
				this_ptime = imp->microseconds_per_packet / 1000;
				
				if (!strcasecmp(imp->iananame, "ilbc")) {
					this_ptime = 20;
				}
				
				if (cur_ptime != this_ptime) {
					char *bp = buf;
					int both = 1;

					cur_ptime = this_ptime;			
					
					if ((!zstr(local_audio_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE))) {
						generate_m(tech_pvt, bp, SDPBUFLEN - strlen(buf), port, cur_ptime, append_audio, sr, use_cng, cng_type, map, verbose_sdp, 1);
						bp = (buf + strlen(buf));

						/* asterisk can't handle AVP and SAVP in sep streams, way to blow off the spec....*/
						if (switch_true(switch_channel_get_variable(tech_pvt->channel, "sdp_secure_savp_only"))) {
							both = 0;
						}
					}

					if (both) {
						generate_m(tech_pvt, bp, SDPBUFLEN - strlen(buf), port, cur_ptime, append_audio, sr, use_cng, cng_type, map, verbose_sdp, 0);
					}
				}
				
			}
		}

	}
	
	if (sofia_test_flag(tech_pvt, TFLAG_VIDEO)) {
		const char *local_video_crypto_key = switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
		
		if (!tech_pvt->local_sdp_video_port) {
			sofia_glue_tech_choose_video_port(tech_pvt, 0);
		}

		if ((v_port = tech_pvt->adv_sdp_video_port)) {

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=video %d RTP/%sAVP", 
							v_port, (!zstr(local_video_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE)) ? "S" : "");


			/*****************************/
			if (tech_pvt->video_rm_encoding) {
				sofia_glue_tech_set_video_codec(tech_pvt, 0);
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", tech_pvt->video_agreed_pt);
			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };
				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (tech_pvt->ianacodes[i] < 128) {
						if (already_did[tech_pvt->ianacodes[i]]) {
							continue;
						}
						already_did[tech_pvt->ianacodes[i]] = 1;
					}

					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", tech_pvt->ianacodes[i]);
					if (!ptime) {
						ptime = imp->microseconds_per_packet / 1000;
					}
				}
			}

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\n");

			if (tech_pvt->video_rm_encoding) {
				const char *of;
				rate = tech_pvt->video_rm_rate;
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\n", tech_pvt->video_pt, tech_pvt->video_rm_encoding,
								tech_pvt->video_rm_rate);

				if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
					pass_fmtp = tech_pvt->video_rm_fmtp;
				} else {

					pass_fmtp = NULL;

					if (switch_channel_get_partner_uuid(tech_pvt->channel)) {
						if ((of = switch_channel_get_variable_partner(tech_pvt->channel, "sip_video_fmtp"))) {
							pass_fmtp = of;
						}
					}

					if (ov_fmtp) {
						pass_fmtp = ov_fmtp;
					}// else { // seems to break eyebeam at least...
						//pass_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_video_fmtp");
					//}
				}

				if (pass_fmtp) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->video_pt, pass_fmtp);
				}

			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
					char *fmtp = NULL;
					uint32_t ianacode = tech_pvt->ianacodes[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (ianacode < 128) {
						if (already_did[ianacode]) {
							continue;
						}
						already_did[ianacode] = 1;
					}

					if (!rate) {
						rate = imp->samples_per_second;
					}
					
					
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\n", ianacode, imp->iananame,
									imp->samples_per_second);
					
					if (!zstr(ov_fmtp)) {
						fmtp = (char *) ov_fmtp;
					} else {
					
						if (map) {
							fmtp = switch_event_get_header(map, imp->iananame);
						}
						
						if (zstr(fmtp)) fmtp = imp->fmtp;

						if (zstr(fmtp)) fmtp = (char *) pass_fmtp;
					}
					
					if (!zstr(fmtp) && strcasecmp(fmtp, "_blank_")) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", ianacode, fmtp);
					}
				}
				
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_SECURE)) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\n", local_video_crypto_key);
				//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
			}			


			if (tech_pvt->local_sdp_video_zrtp_hash) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding video a=zrtp-hash:%s\n",
								  tech_pvt->local_sdp_video_zrtp_hash);
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n",
								tech_pvt->local_sdp_video_zrtp_hash);
			}
		}
	}


	if (map) {
		switch_event_destroy(&map);
	}
	
	if (ptmap) {
		switch_event_destroy(&ptmap);
	}

	sofia_glue_tech_set_local_sdp(tech_pvt, buf, SWITCH_TRUE);

	switch_safe_free(buf);
}

const char *sofia_glue_get_codec_string(private_object_t *tech_pvt)
{
	const char *preferred = NULL, *fallback = NULL;
	
	if (!(preferred = switch_channel_get_variable(tech_pvt->channel, "absolute_codec_string"))) {
		preferred = switch_channel_get_variable(tech_pvt->channel, "codec_string");
	}
	
	if (!preferred) {
		if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			preferred = tech_pvt->profile->outbound_codec_string;
			fallback = tech_pvt->profile->inbound_codec_string;
		} else {
			preferred = tech_pvt->profile->inbound_codec_string;
			fallback = tech_pvt->profile->outbound_codec_string;
		}
	}

	return !zstr(preferred) ? preferred : fallback;
}

void sofia_glue_tech_prepare_codecs(private_object_t *tech_pvt)
{
	const char *abs, *codec_string = NULL;
	const char *ocodec = NULL;

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		return;
	}

	if (tech_pvt->num_codecs) {
		return;
	}

	tech_pvt->payload_space = 0;

	switch_assert(tech_pvt->session != NULL);

	if ((abs = switch_channel_get_variable(tech_pvt->channel, "absolute_codec_string"))) {
		/* inherit_codec == true will implicitly clear the absolute_codec_string 
		   variable if used since it was the reason it was set in the first place and is no longer needed */
		if (switch_true(switch_channel_get_variable(tech_pvt->channel, "inherit_codec"))) {
			switch_channel_set_variable(tech_pvt->channel, "absolute_codec_string", NULL);
		}
		codec_string = abs;
		goto ready;
	}

	if (!(codec_string = switch_channel_get_variable(tech_pvt->channel, "codec_string"))) {
		codec_string = sofia_glue_get_codec_string(tech_pvt);
	}

	if (codec_string && *codec_string == '=') {
		codec_string++;
		goto ready;
	}


	if ((ocodec = switch_channel_get_variable(tech_pvt->channel, SWITCH_ORIGINATOR_CODEC_VARIABLE))) {
		if (!codec_string || sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_TRANSCODING)) {
			codec_string = ocodec;
		} else {
			if (!(codec_string = switch_core_session_sprintf(tech_pvt->session, "%s,%s", ocodec, codec_string))) {
				codec_string = ocodec;
			}
		}
	}

 ready:

	if (codec_string) {
		char *tmp_codec_string;
		if ((tmp_codec_string = switch_core_session_strdup(tech_pvt->session, codec_string))) {
			tech_pvt->codec_order_last = switch_separate_string(tmp_codec_string, ',', tech_pvt->codec_order, SWITCH_MAX_CODECS);
			tech_pvt->num_codecs =
				switch_loadable_module_get_codecs_sorted(tech_pvt->codecs, SWITCH_MAX_CODECS, tech_pvt->codec_order, tech_pvt->codec_order_last);
		}
	} else {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs(tech_pvt->codecs, sizeof(tech_pvt->codecs) / sizeof(tech_pvt->codecs[0]));
	}


}

void sofia_glue_check_video_codecs(private_object_t *tech_pvt)
{
	if (tech_pvt->num_codecs && !sofia_test_flag(tech_pvt, TFLAG_VIDEO)) {
		int i;
		tech_pvt->video_count = 0;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			
			if (tech_pvt->codecs[i]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				tech_pvt->video_count++;
			}
		}
		if (tech_pvt->video_count) {
			sofia_set_flag_locked(tech_pvt, TFLAG_VIDEO);
		}
	}
}


void sofia_glue_tech_patch_sdp(private_object_t *tech_pvt)
{
	switch_size_t len;
	char *p, *q, *pe, *qe;
	int has_video = 0, has_audio = 0, has_ip = 0;
	char port_buf[25] = "";
	char vport_buf[25] = "";
	char *new_sdp;
	int bad = 0;

	if (zstr(tech_pvt->local_sdp_str)) {
		return;
	}

	len = strlen(tech_pvt->local_sdp_str) * 2;

	if (switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) &&
		(switch_stristr("sendonly", tech_pvt->local_sdp_str) || switch_stristr("0.0.0.0", tech_pvt->local_sdp_str))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Skip patch on hold SDP\n");
		return;
	}

	if (zstr(tech_pvt->adv_sdp_audio_ip) || !tech_pvt->adv_sdp_audio_port) {
		if (sofia_glue_tech_choose_port(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "%s I/O Error\n",
							  switch_channel_get_name(tech_pvt->channel));
			return;
		}
		tech_pvt->iananame = switch_core_session_strdup(tech_pvt->session, "PROXY");
		tech_pvt->rm_rate = 8000;
		tech_pvt->codec_ms = 20;
	}

	new_sdp = switch_core_session_alloc(tech_pvt->session, len);
	switch_snprintf(port_buf, sizeof(port_buf), "%u", tech_pvt->adv_sdp_audio_port);


	p = tech_pvt->local_sdp_str;
	q = new_sdp;
	pe = p + strlen(p);
	qe = q + len - 1;


	while (p && *p) {
		if (p >= pe) {
			bad = 1;
			goto end;
		}

		if (q >= qe) {
			bad = 2;
			goto end;
		}

		if (tech_pvt->adv_sdp_audio_ip && !strncmp("c=IN IP", p, 7)) {
			strncpy(q, p, 7);
			p += 7;
			q += 7;
			strncpy(q, strchr(tech_pvt->adv_sdp_audio_ip, ':') ? "6 " : "4 ", 2);
			p +=2;
			q +=2;			
			strncpy(q, tech_pvt->adv_sdp_audio_ip, strlen(tech_pvt->adv_sdp_audio_ip));
			q += strlen(tech_pvt->adv_sdp_audio_ip);

			while (p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) {
				if (p >= pe) {
					bad = 3;
					goto end;
				}
				p++;
			}

			has_ip++;

		} else if (!strncmp("o=", p, 2)) {
			char *oe = strchr(p, '\n');
			switch_size_t len;

			if (oe) {
				const char *family = "IP4";
				char o_line[1024] = "";

				if (oe >= pe) {
					bad = 5;
					goto end;
				}

				len = (oe - p);
				p += len;


				family = strchr(tech_pvt->profile->sipip, ':') ? "IP6" : "IP4";

				if (!tech_pvt->owner_id) {
					tech_pvt->owner_id = (uint32_t) switch_epoch_time_now(NULL) * 31821U + 13849U;
				}

				if (!tech_pvt->session_id) {
					tech_pvt->session_id = tech_pvt->owner_id;
				}

				tech_pvt->session_id++;


				snprintf(o_line, sizeof(o_line), "o=%s %010u %010u IN %s %s\n",
						 tech_pvt->profile->username, tech_pvt->owner_id, tech_pvt->session_id, family, tech_pvt->profile->sipip);

				strncpy(q, o_line, strlen(o_line));
				q += strlen(o_line) - 1;

			}

		} else if (!strncmp("s=", p, 2)) {
			char *se = strchr(p, '\n');
			switch_size_t len;

			if (se) {
				char s_line[1024] = "";

				if (se >= pe) {
					bad = 5;
					goto end;
				}

				len = (se - p);
				p += len;

				snprintf(s_line, sizeof(s_line), "s=%s\n", tech_pvt->profile->username);

				strncpy(q, s_line, strlen(s_line));
				q += strlen(s_line) - 1;

			}

		} else if ((!strncmp("m=audio ", p, 8) && *(p + 8) != '0') || (!strncmp("m=image ", p, 8) && *(p + 8) != '0')) {
			strncpy(q, p, 8);
			p += 8;

			if (p >= pe) {
				bad = 4;
				goto end;
			}


			q += 8;

			if (q >= qe) {
				bad = 5;
				goto end;
			}


			strncpy(q, port_buf, strlen(port_buf));
			q += strlen(port_buf);

			if (q >= qe) {
				bad = 6;
				goto end;
			}

			while (p && *p && (*p >= '0' && *p <= '9')) {
				if (p >= pe) {
					bad = 7;
					goto end;
				}
				p++;
			}

			has_audio++;

		} else if (!strncmp("m=video ", p, 8) && *(p + 8) != '0') {
			if (!has_video) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
				tech_pvt->video_rm_encoding = "PROXY-VID";
				tech_pvt->video_rm_rate = 90000;
				tech_pvt->video_codec_ms = 0;
				switch_snprintf(vport_buf, sizeof(vport_buf), "%u", tech_pvt->adv_sdp_video_port);
				if (switch_channel_media_ready(tech_pvt->channel) && !switch_rtp_ready(tech_pvt->video_rtp_session)) {
					sofia_set_flag(tech_pvt, TFLAG_VIDEO);
					sofia_set_flag(tech_pvt, TFLAG_REINVITE);
					sofia_media_activate_rtp(tech_pvt);
				}
			}

			strncpy(q, p, 8);
			p += 8;

			if (p >= pe) {
				bad = 8;
				goto end;
			}

			q += 8;

			if (q >= qe) {
				bad = 9;
				goto end;
			}

			strncpy(q, vport_buf, strlen(vport_buf));
			q += strlen(vport_buf);

			if (q >= qe) {
				bad = 10;
				goto end;
			}

			while (p && *p && (*p >= '0' && *p <= '9')) {

				if (p >= pe) {
					bad = 11;
					goto end;
				}

				p++;
			}

			has_video++;
		}

		while (p && *p && *p != '\n') {

			if (p >= pe) {
				bad = 12;
				goto end;
			}

			if (q >= qe) {
				bad = 13;
				goto end;
			}

			*q++ = *p++;
		}

		if (p >= pe) {
			bad = 14;
			goto end;
		}

		if (q >= qe) {
			bad = 15;
			goto end;
		}

		*q++ = *p++;

	}

 end:

	if (bad) {
		return;
	}


	if (switch_channel_down(tech_pvt->channel) || sofia_test_flag(tech_pvt, TFLAG_BYE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s too late.\n", switch_channel_get_name(tech_pvt->channel));
		return;
	}


	if (!has_ip && !has_audio) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s SDP has no audio in it.\n%s\n",
						  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str);
		return;
	}


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s Patched SDP\n---\n%s\n+++\n%s\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str, new_sdp);

	sofia_glue_tech_set_local_sdp(tech_pvt, new_sdp, SWITCH_FALSE);

}


void sofia_glue_tech_set_local_sdp(private_object_t *tech_pvt, const char *sdp_str, switch_bool_t dup)
{
	switch_mutex_lock(tech_pvt->sofia_mutex);
	tech_pvt->local_sdp_str = dup ? switch_core_session_strdup(tech_pvt->session, sdp_str) : (char *) sdp_str;
	switch_channel_set_variable(tech_pvt->channel, "sip_local_sdp_str", tech_pvt->local_sdp_str);
	switch_mutex_unlock(tech_pvt->sofia_mutex);
}


char *sofia_glue_get_multipart(switch_core_session_t *session, const char *prefix, const char *sdp, char **mp_type)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi = NULL;
	int x = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *boundary = switch_core_session_get_uuid(session);

	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;

			if (!strncasecmp(name, prefix, strlen(prefix))) {
				const char *hname = name + strlen(prefix);
				if (*value == '~') {
					stream.write_function(&stream, "--%s\nContent-Type: %s\nContent-Length: %d\n%s\n", boundary, hname, strlen(value), value + 1);
				} else {
					stream.write_function(&stream, "--%s\nContent-Type: %s\nContent-Length: %d\n\n%s\n", boundary, hname, strlen(value) + 1, value);
				}
				x++;
			}
		}
		switch_channel_variable_last(channel);
	}

	if (x) {
		*mp_type = switch_core_session_sprintf(session, "multipart/mixed; boundary=%s", boundary);
		if (sdp) {
			stream.write_function(&stream, "--%s\nContent-Type: application/sdp\nContent-Length: %d\n\n%s\n", boundary, strlen(sdp) + 1, sdp);
		}
		stream.write_function(&stream, "--%s--\n", boundary);
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
}





/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

