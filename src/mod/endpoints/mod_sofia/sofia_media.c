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




void sofia_media_tech_absorb_sdp(private_object_t *tech_pvt)
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
		sofia_media_tech_set_local_sdp(tech_pvt, sdp_str, SWITCH_TRUE);
	}
}



switch_status_t sofia_media_sdp_map(const char *r_sdp, switch_event_t **fmtp, switch_event_t **pt)
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


void sofia_media_proxy_codec(switch_core_session_t *session, const char *r_sdp)
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
				switch_core_media_set_codec(tech_pvt->session, 0, tech_pvt->profile->codec_flags);
				break;
			}

			break;
		}
	}

	sdp_parser_free(parser);

}

uint8_t sofia_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp)
{
	uint8_t t, p = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if ((t = switch_core_media_negotiate_sdp(session, r_sdp, &p, sofia_test_flag(tech_pvt, TFLAG_REINVITE), 
									   tech_pvt->profile->codec_flags, tech_pvt->profile->te))) {
		sofia_set_flag_locked(tech_pvt, TFLAG_SDP);
	}

	if (!p) {
		sofia_set_flag(tech_pvt, TFLAG_NOREPLY);
	}

	return t;
}

switch_status_t sofia_media_activate_rtp(private_object_t *tech_pvt)
{
	int ok;

	switch_mutex_lock(tech_pvt->sofia_mutex);
	switch_mutex_unlock(tech_pvt->sofia_mutex);

	if (ok) {
		sofia_set_flag(tech_pvt, TFLAG_RTP);
		sofia_set_flag(tech_pvt, TFLAG_IO);
	}

}



void sofia_media_set_sdp_codec_string(switch_core_session_t *session, const char *r_sdp)
{
	sdp_parser_t *parser;
	sdp_session_t *sdp;
	private_object_t *tech_pvt = switch_core_session_get_private(session);

	if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {

		if ((sdp = sdp_session(parser))) {
			sofia_media_set_r_sdp_codec_string(session, switch_core_media_get_codec_string(tech_pvt->session), sdp);
		}

		sdp_parser_free(parser);
	}

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


void sofia_media_set_r_sdp_codec_string(switch_core_session_t *session, const char *codec_string, sdp_session_t *sdp)
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

	switch_core_media_find_zrtp_hash(session, sdp);
	switch_core_media_pass_zrtp_hash(session);

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

switch_status_t sofia_media_tech_media(private_object_t *tech_pvt, const char *r_sdp)
{
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (zstr(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((match = sofia_media_negotiate_sdp(tech_pvt->session, r_sdp))) {
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


static void generate_m(private_object_t *tech_pvt, char *buf, size_t buflen, 
					   switch_port_t port,
					   int cur_ptime, const char *append_audio, const char *sr, int use_cng, int cng_type, switch_event_t *map, int verbose_sdp, int secure)
{
	int i = 0;
	int rate;
	int already_did[128] = { 0 };
	int ptime = 0, noptime = 0;
	const char *local_audio_crypto_key = switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO);
	const char *local_sdp_audio_zrtp_hash;

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


	if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || 
		 switch_channel_test_flag(tech_pvt->channel, CF_LIBERAL_DTMF)) && tech_pvt->te > 95) {
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

	local_sdp_audio_zrtp_hash = switch_core_media_get_zrtp_hash(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_TRUE);

	if (local_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n", local_sdp_audio_zrtp_hash);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=zrtp-hash:%s\n", local_sdp_audio_zrtp_hash);
	}

	if (!zstr(sr)) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=%s\n", sr);
	}
}


#define SDPBUFLEN 65536
void sofia_media_set_local_sdp(private_object_t *tech_pvt, const char *ip, switch_port_t port, const char *sr, int force)
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
	const char *local_sdp_audio_zrtp_hash = switch_core_media_get_zrtp_hash(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_TRUE);
	const char *local_sdp_video_zrtp_hash = switch_core_media_get_zrtp_hash(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_TRUE);

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
		sofia_media_sdp_map(b_sdp, &map, &ptmap);
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

		if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || 
			 switch_channel_test_flag(tech_pvt->channel, CF_LIBERAL_DTMF)) && tech_pvt->te > 95) {
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


		if ((tech_pvt->dtmf_type == DTMF_2833 || sofia_test_pflag(tech_pvt->profile, PFLAG_LIBERAL_DTMF) || 
			 switch_channel_test_flag(tech_pvt->channel, CF_LIBERAL_DTMF))
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


		if (local_sdp_audio_zrtp_hash) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n",
							  local_sdp_audio_zrtp_hash);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n",
							local_sdp_audio_zrtp_hash);
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
	
	if (switch_channel_test_flag(tech_pvt->channel, CF_VIDEO_POSSIBLE)) {
		const char *local_video_crypto_key = switch_core_session_local_crypto_key(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO);
		
		if (!tech_pvt->local_sdp_video_port) {
			sofia_glue_tech_choose_video_port(tech_pvt, 0);
		}

		if ((v_port = tech_pvt->adv_sdp_video_port)) {

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=video %d RTP/%sAVP", 
							v_port, (!zstr(local_video_crypto_key) && switch_channel_test_flag(tech_pvt->channel, CF_SECURE)) ? "S" : "");


			/*****************************/
			if (tech_pvt->video_rm_encoding) {
				switch_core_media_set_video_codec(tech_pvt->session, 0);
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



			if (local_sdp_video_zrtp_hash) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Adding video a=zrtp-hash:%s\n", local_sdp_video_zrtp_hash);
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n", local_sdp_video_zrtp_hash);
			}
		}
	}


	if (map) {
		switch_event_destroy(&map);
	}
	
	if (ptmap) {
		switch_event_destroy(&ptmap);
	}

	sofia_media_tech_set_local_sdp(tech_pvt, buf, SWITCH_TRUE);

	switch_safe_free(buf);
}

void sofia_media_tech_prepare_codecs(private_object_t *tech_pvt)
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
		codec_string = switch_core_media_get_codec_string(tech_pvt->session);
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
		char *tmp_codec_string = switch_core_session_strdup(tech_pvt->session, codec_string);
		tech_pvt->codec_order_last = switch_separate_string(tmp_codec_string, ',', tech_pvt->codec_order, SWITCH_MAX_CODECS);
		tech_pvt->num_codecs =
			switch_loadable_module_get_codecs_sorted(tech_pvt->codecs, SWITCH_MAX_CODECS, tech_pvt->codec_order, tech_pvt->codec_order_last);
		
	} else {
		tech_pvt->num_codecs = switch_loadable_module_get_codecs(tech_pvt->codecs, sizeof(tech_pvt->codecs) / sizeof(tech_pvt->codecs[0]));
	}


}


void sofia_media_tech_patch_sdp(private_object_t *tech_pvt)
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
					switch_channel_set_flag(tech_pvt->channel, CF_VIDEO_POSSIBLE);
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

	sofia_media_tech_set_local_sdp(tech_pvt, new_sdp, SWITCH_FALSE);

}


void sofia_media_tech_set_local_sdp(private_object_t *tech_pvt, const char *sdp_str, switch_bool_t dup)
{
	switch_mutex_lock(tech_pvt->sofia_mutex);
	tech_pvt->local_sdp_str = dup ? switch_core_session_strdup(tech_pvt->session, sdp_str) : (char *) sdp_str;
	switch_channel_set_variable(tech_pvt->channel, "sip_local_sdp_str", tech_pvt->local_sdp_str);
	switch_mutex_unlock(tech_pvt->sofia_mutex);
}


char *sofia_media_get_multipart(switch_core_session_t *session, const char *prefix, const char *sdp, char **mp_type)
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

