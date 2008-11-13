/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Ken Rice, Asteria Solutions Group, Inc <ken@asteriasgi.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 *
 * sofia_glue.c -- SOFIA SIP Endpoint (code to tie sofia to freeswitch)
 *
 */
#include "mod_sofia.h"
#include <switch_stun.h>


void sofia_glue_set_local_sdp(private_object_t *tech_pvt, const char *ip, uint32_t port, const char *sr, int force)
{
	char buf[2048];
	int ptime = 0;
	uint32_t rate = 0;
	uint32_t v_port;
	int use_cng = 1;
	const char *val;
	const char *family;
	const char *pass_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_video_fmtp");
	const char *ov_fmtp = switch_channel_get_variable(tech_pvt->channel, "sip_force_video_fmtp");

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
		((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
		use_cng = 0;
		tech_pvt->cng_pt = 0;
	}

	if (!force && !ip && !sr
		&& (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA))) {
		return;
	}

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}
	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}

	if (!sr) {
		sr = "sendrecv";
	}

	if (!tech_pvt->owner_id) {
		tech_pvt->owner_id = (uint32_t) switch_timestamp(NULL) - port;
	}

	if (!tech_pvt->session_id) {
		tech_pvt->session_id = tech_pvt->owner_id;
	}

	tech_pvt->session_id++;

	family = strchr(ip, ':') ? "IP6" : "IP4";
	switch_snprintf(buf, sizeof(buf),
					"v=0\n"
					"o=FreeSWITCH %010u %010u IN %s %s\n"
					"s=FreeSWITCH\n"
					"c=IN %s %s\n" "t=0 0\n"
					"m=audio %d RTP/%sAVP", 
					tech_pvt->owner_id, tech_pvt->session_id, family, ip, family, ip, port,
					(!switch_strlen_zero(tech_pvt->local_crypto_key) 
					&& switch_test_flag(tech_pvt,TFLAG_SECURE)) ? "S" : "");

	if (tech_pvt->rm_encoding) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->pt);
	} else if (tech_pvt->num_codecs) {
		int i;
		int already_did[128] = { 0 };
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

			if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
				continue;
			}

			if (imp->ianacode < 128) {
				if (already_did[imp->ianacode]) {
					continue;
				}

				already_did[imp->ianacode] = 1;
			}

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
			if (!ptime) {
				ptime = imp->microseconds_per_packet / 1000;
			}
		}
	}

	if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->te);
	}

	if (!(tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->cng_pt);
	}

	switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");


	if (tech_pvt->rm_encoding) {
		rate = tech_pvt->rm_rate;
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", tech_pvt->agreed_pt, tech_pvt->rm_encoding, rate);
		if (tech_pvt->fmtp_out) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->agreed_pt, tech_pvt->fmtp_out);
		}
		if (tech_pvt->read_codec.implementation && !ptime) {
			ptime = tech_pvt->read_codec.implementation->microseconds_per_packet / 1000;
		}

	} else if (tech_pvt->num_codecs) {
		int i;
		int already_did[128] = { 0 };
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

			if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
				continue;
			}

			if (imp->ianacode < 128) {
				if (already_did[imp->ianacode]) {
					continue;
				}

				already_did[imp->ianacode] = 1;
			}

			rate = imp->samples_per_second;

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame, rate);
			if (imp->fmtp) {
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, imp->fmtp);
			}
		}
	}

	if (tech_pvt->dtmf_type == DTMF_2833 && tech_pvt->te > 95) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", tech_pvt->te, tech_pvt->te);
	}
	if (!(tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG) && tech_pvt->cng_pt && use_cng) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d CN/8000\n", tech_pvt->cng_pt);
		if (!tech_pvt->rm_encoding) {
			tech_pvt->cng_pt = 0;
		}
	} else {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=silenceSupp:off - - - -\n");
	}

	if (ptime) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=ptime:%d\n", ptime);
	}

	if (sr) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=%s\n", sr);
	} 

	if (!switch_strlen_zero(tech_pvt->local_crypto_key) && switch_test_flag(tech_pvt, TFLAG_SECURE)) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=crypto:%s\n", tech_pvt->local_crypto_key);
		//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
#if 0
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=audio %d RTP/AVP", port);

		if (tech_pvt->rm_encoding) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->pt);
		} else if (tech_pvt->num_codecs) {
			int i;
			int already_did[128] = { 0 };
			for (i = 0; i < tech_pvt->num_codecs; i++) {
				const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

				if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
					continue;
				}

				if (imp->ianacode < 128) {
					if (already_did[imp->ianacode]) {
						continue;
					}

					already_did[imp->ianacode] = 1;
				}

				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
			}
		}

		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\na=crypto:%s\n", tech_pvt->local_crypto_key);
#endif
	}

	if (switch_test_flag(tech_pvt, TFLAG_VIDEO)) {
		if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) && !switch_channel_test_flag(tech_pvt->channel, CF_EARLY_MEDIA) &&
			!tech_pvt->local_sdp_video_port) {
			sofia_glue_tech_choose_video_port(tech_pvt, 0);
		}

		if ((v_port = tech_pvt->adv_sdp_video_port)) {
			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=video %d RTP/AVP", v_port);
			
			/*****************************/
			if (tech_pvt->video_rm_encoding) {
				sofia_glue_tech_set_video_codec(tech_pvt, 0);
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", tech_pvt->video_agreed_pt);
			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };
				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (imp->ianacode < 128) {
						if (already_did[imp->ianacode]) {
							continue;
						}
						already_did[imp->ianacode] = 1;
					}

					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %d", imp->ianacode);
					if (!ptime) {
						ptime = imp->microseconds_per_packet / 1000;
					}
				}
			}

			switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");

			if (tech_pvt->video_rm_encoding) {
				const char *of;
				rate = tech_pvt->video_rm_rate;
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%ld\n", tech_pvt->video_pt, tech_pvt->video_rm_encoding,
								tech_pvt->video_rm_rate);

				pass_fmtp = NULL;

				if (switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE)) {
					if ((of = switch_channel_get_variable_partner(tech_pvt->channel, "sip_video_fmtp"))) {
						pass_fmtp = of;
					}
				}

				if (ov_fmtp) {
					pass_fmtp = ov_fmtp;
				}

				if (pass_fmtp) {
					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", tech_pvt->video_pt, pass_fmtp);
				}

			} else if (tech_pvt->num_codecs) {
				int i;
				int already_did[128] = { 0 };

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					if (imp->ianacode < 128) {
						if (already_did[imp->ianacode]) {
							continue;
						}
						already_did[imp->ianacode] = 1;
					}

					if (!rate) {
						rate = imp->samples_per_second;
					}

					switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=rtpmap:%d %s/%d\n", imp->ianacode, imp->iananame,
									imp->samples_per_second);
					if (imp->fmtp) {
						switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, imp->fmtp);
					} else {
						if (pass_fmtp) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=fmtp:%d %s\n", imp->ianacode, pass_fmtp);
						}
					}
				}
			}
		}
	}

	tech_pvt->local_sdp_str = switch_core_session_strdup(tech_pvt->session, buf);
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

	switch_assert(tech_pvt->session != NULL);

	if ((abs = switch_channel_get_variable(tech_pvt->channel, "absolute_codec_string"))) {
		codec_string = abs;
	} else {
		if (!(codec_string = switch_channel_get_variable(tech_pvt->channel, "codec_string"))) {
			if (tech_pvt->profile->codec_string) {
				codec_string = tech_pvt->profile->codec_string;
			}
		}

		if ((ocodec = switch_channel_get_variable(tech_pvt->channel, SWITCH_ORIGINATOR_CODEC_VARIABLE))) {
			if (!codec_string || (tech_pvt->profile->pflags & PFLAG_DISABLE_TRANSCODING)) {
				codec_string = ocodec;
			} else {
				if (!(codec_string = switch_core_session_sprintf(tech_pvt->session, "%s,%s", ocodec, codec_string))) {
					codec_string = ocodec;
				}
			}
		}
	}

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
	if (tech_pvt->num_codecs && !switch_test_flag(tech_pvt, TFLAG_VIDEO)) {
		int i;
		tech_pvt->video_count = 0;
		for (i = 0; i < tech_pvt->num_codecs; i++) {
			if (tech_pvt->codecs[i]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				tech_pvt->video_count++;
			}
		}
		if (tech_pvt->video_count) {
			switch_set_flag_locked(tech_pvt, TFLAG_VIDEO);
		}
	}
}


void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame)
{
	char name[256];

	switch_assert(session != NULL);
	switch_assert(profile != NULL);
	switch_assert(tech_pvt != NULL);

	switch_core_session_add_stream(session, NULL);

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_mutex_lock(profile->flag_mutex);
	tech_pvt->flags = profile->flags;
	tech_pvt->profile = profile;
	profile->inuse++;
	switch_mutex_unlock(profile->flag_mutex);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	if (tech_pvt->bte) {
		tech_pvt->te = tech_pvt->bte;
	} else if (!tech_pvt->te) {
		tech_pvt->te = profile->te;
	}

	tech_pvt->dtmf_type = profile->dtmf_type;

	if (!(tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG)) {
		if (tech_pvt->bcng_pt) {
			tech_pvt->cng_pt = tech_pvt->bcng_pt;
		} else if (!tech_pvt->cng_pt) {
			tech_pvt->cng_pt = profile->cng_pt;
		}
	}

	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);
	switch_core_session_set_private(session, tech_pvt);

	switch_snprintf(name, sizeof(name), "sofia/%s/%s", profile->name, channame);
	switch_channel_set_name(tech_pvt->channel, name);
}

switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, private_object_t *tech_pvt, char **ip, switch_port_t *port, char *sourceip, switch_memory_pool_t *pool)
{
	char *error = "";
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x;
	switch_port_t myport = *port;
	const char *var;
	int funny = 0;
	switch_port_t stun_port = SWITCH_STUN_DEFAULT_PORT;
	char *stun_ip = NULL;

	if (!sourceip) {
		return status;
	}

	if (!strncasecmp(sourceip, "stun:", 5)) {
		char *p;

		if (!(profile->pflags & PFLAG_STUN_ENABLED)) {
			*ip = switch_core_strdup(pool, tech_pvt->profile->rtpip);
			return SWITCH_STATUS_SUCCESS;
		}
		
		stun_ip = strdup(sourceip + 5);

		if ((p = strchr(stun_ip, ':'))) {
			int iport;
			*p++ = '\0';
			iport = atoi(p);
			if (iport > 0 && iport < 0xFFFF) {
				stun_port = (switch_port_t) iport;
			}
		}

		if (switch_strlen_zero(stun_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! NO STUN SERVER\n");
			goto out;
		}

		for (x = 0; x < 5; x++) {
			if ((profile->pflags & PFLAG_FUNNY_STUN) || 
				(tech_pvt && (var = switch_channel_get_variable(tech_pvt->channel, "funny_stun")) && switch_true(var))) {
				error = "funny";
				funny++;
			}
			if ((status = switch_stun_lookup(ip, port, stun_ip, stun_port, &error, pool)) != SWITCH_STATUS_SUCCESS) {
				switch_yield(100000);
			} else {
				break;
			}
		}
		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! %s:%d [%s]\n", stun_ip, stun_port, error);
			goto out;
		}
		if (!*ip) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! No IP returned\n");
			goto out;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Success [%s]:[%d]\n", *ip, *port);
		status = SWITCH_STATUS_SUCCESS;
		if (tech_pvt) {
			if (myport == *port && !strcmp(*ip, tech_pvt->profile->rtpip)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Not Required ip and port match. [%s]:[%d]\n", *ip, *port);
				if (profile->pflags & PFLAG_STUN_AUTO_DISABLE) {
					profile->pflags &= ~PFLAG_STUN_ENABLED;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN completely disabled.\n");
				}
			} else {
				tech_pvt->stun_ip = switch_core_session_strdup(tech_pvt->session, stun_ip);
				tech_pvt->stun_port = stun_port;
				tech_pvt->stun_flags |= STUN_FLAG_SET;
				if (funny) {
					tech_pvt->stun_flags |= STUN_FLAG_FUNNY;
				}
			}
		}
	} else {
		*ip = sourceip;
		status = SWITCH_STATUS_SUCCESS;
	}

 out:

	switch_safe_free(stun_ip);

	return status;
}


const char *sofia_glue_get_unknown_header(sip_t const *sip, const char *name)
{
	sip_unknown_t *un;
	for (un = sip->sip_unknown; un; un = un->un_next) {
		if (!strcasecmp(un->un_name, name)) {
			if (!switch_strlen_zero(un->un_value)) {
				return un->un_value;
			}
		}
	}
	return NULL;
}

switch_status_t sofia_glue_tech_choose_port(private_object_t *tech_pvt, int force)
{
	char *ip = tech_pvt->profile->rtpip;
	switch_port_t sdp_port;
	char tmp[50];
	const char *use_ip = NULL;

	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || tech_pvt->adv_sdp_audio_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_audio_port);
	}


	tech_pvt->local_sdp_audio_ip = ip;
	if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(tech_pvt->profile->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}
	sdp_port = tech_pvt->local_sdp_audio_port;

	if (!(use_ip = switch_channel_get_variable(tech_pvt->channel, "rtp_adv_audio_ip"))) {
		if (tech_pvt->profile->extrtpip) {
			use_ip = tech_pvt->profile->extrtpip;
		}
	}

	if (use_ip) {
		tech_pvt->extrtpip = switch_core_session_strdup(tech_pvt->session, use_ip);
		if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &ip, &sdp_port, 
										  tech_pvt->extrtpip, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}
	

	tech_pvt->adv_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, ip);
	tech_pvt->adv_sdp_audio_port = sdp_port;

	switch_snprintf(tmp, sizeof(tmp), "%d", sdp_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_tech_choose_video_port(private_object_t *tech_pvt, int force)
{
	char *ip = tech_pvt->profile->rtpip;
	switch_port_t sdp_port;
	char tmp[50];

	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) || switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)
			|| tech_pvt->local_sdp_video_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_video_port);
	}

	if (!(tech_pvt->local_sdp_video_port = switch_rtp_request_port(tech_pvt->profile->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}
	sdp_port = tech_pvt->local_sdp_video_port;


	if (tech_pvt->profile->extrtpip) {
		if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &ip, &sdp_port, tech_pvt->profile->extrtpip, switch_core_session_get_pool(tech_pvt->session)) !=
			SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	tech_pvt->adv_sdp_video_port = sdp_port;

	switch_snprintf(tmp, sizeof(tmp), "%d", sdp_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, tmp);

	return SWITCH_STATUS_SUCCESS;
}

sofia_transport_t sofia_glue_str2transport(const char *str)
{
	if (!strncasecmp(str, "udp", 3)) {
		return SOFIA_TRANSPORT_UDP;
	} else if (!strncasecmp(str, "tcp", 3)) {
		return SOFIA_TRANSPORT_TCP;
	} else if (!strncasecmp(str, "sctp", 4)) {
		return SOFIA_TRANSPORT_SCTP;
	} else if (!strncasecmp(str, "tls", 3)) {
		return SOFIA_TRANSPORT_TCP_TLS;
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

char *sofia_glue_find_parameter(const char *str, const char *param)
{
	char *ptr = NULL;

	ptr = (char *) str;
	while (ptr) {
		if (!strncasecmp(ptr, param, strlen(param)))
			return ptr;

		if ((ptr = strchr(ptr, ';')))
			ptr++;
	}

	return NULL;
}

sofia_transport_t sofia_glue_url2transport(const url_t *url)
{
	char *ptr = NULL;
	int tls = 0;

	if (!url)
		return SOFIA_TRANSPORT_UNKNOWN;

	if (url->url_scheme && !strcasecmp(url->url_scheme, "sips")) {
		tls++;
	}

	if ((ptr = sofia_glue_find_parameter(url->url_params, "transport="))) {
		return sofia_glue_str2transport(ptr + 10);
	}

	return (tls) ? SOFIA_TRANSPORT_TCP_TLS : SOFIA_TRANSPORT_UDP;
}

sofia_transport_t sofia_glue_via2transport(const sip_via_t *via)
{
	char *ptr = NULL;

	if (!via || !via->v_protocol)
		return SOFIA_TRANSPORT_UNKNOWN;

	if ((ptr = strrchr(via->v_protocol, '/'))) {
		ptr++;

		if (!strncasecmp(ptr, "udp", 3)) {
			return SOFIA_TRANSPORT_UDP;
		} else if (!strncasecmp(ptr, "tcp", 3)) {
			return SOFIA_TRANSPORT_TCP;
		} else if (!strncasecmp(ptr, "tls", 3)) {
			return SOFIA_TRANSPORT_TCP_TLS;
		} else if (!strncasecmp(ptr, "sctp", 4)) {
			return SOFIA_TRANSPORT_SCTP;
		}
	}

	return SOFIA_TRANSPORT_UNKNOWN;
}

const char *sofia_glue_transport2str(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP:
		return "tcp";

	case SOFIA_TRANSPORT_TCP_TLS:
		return "tls";

	case SOFIA_TRANSPORT_SCTP:
		return "sctp";

	default:
		return "udp";
	}
}

int sofia_glue_transport_has_tls(const sofia_transport_t tp)
{
	switch (tp) {
	case SOFIA_TRANSPORT_TCP_TLS:
		return 1;

	default:
		return 0;
	}
}

char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only,
									  const char *params)
{
	char *stripped = switch_core_session_strdup(session, uri);
	char *new_uri = NULL;
	char *p;

	stripped = sofia_glue_get_url_from_contact(stripped, 0);

	/* remove our params so we don't make any whiny moronic device piss it's pants and forget who it is for a half-hour */
	if ((p = (char *)switch_stristr(";fs_", stripped))) {
		*p = '\0'; 
	}

	if (transport && transport != SOFIA_TRANSPORT_UDP) {

		if (switch_stristr("port=", stripped)) {
			new_uri = switch_core_session_sprintf(session, "%s%s%s", uri_only ? "" : "<", stripped, uri_only ? "" : ">");
		} else {

			if (strchr(stripped, ';')) {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s&transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s&transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			} else {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), uri_only ? "" : ">");
				}
			}
		}
	} else {
		if (params) {
			new_uri = switch_core_session_sprintf(session, "%s%s;%s%s", uri_only ? "" : "<", stripped, params, uri_only ? "" : ">");
		} else {
			if (uri_only) {
				new_uri = stripped;
			} else {
				new_uri = switch_core_session_sprintf(session, "<%s>", stripped);
			}
		}
	}

	return new_uri;
}

switch_status_t sofia_glue_tech_proxy_remote_addr(private_object_t *tech_pvt)
{
	const char *err;
	char rip[128] = "";
	char rp[128] = "";
	char rvp[128] = "";
	char *p, *ip_ptr = NULL, *port_ptr = NULL, *vid_port_ptr = NULL;
	int x;
	const char *val;

	if (switch_strlen_zero(tech_pvt->remote_sdp_str)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((p = (char *) switch_stristr("c=IN IP4 ", tech_pvt->remote_sdp_str)) ||
		(p = (char *) switch_stristr("c=IN IP6 ", tech_pvt->remote_sdp_str))) {
		ip_ptr = p + 9;
	}

	if ((p = (char *) switch_stristr("m=audio ", tech_pvt->remote_sdp_str))) {
		port_ptr = p + 8;
	}

	if ((p = (char *) switch_stristr("m=image ", tech_pvt->remote_sdp_str))) {
		port_ptr = p + 8;
	}

	if ((p = (char *) switch_stristr("m=video ", tech_pvt->remote_sdp_str))) {
		vid_port_ptr = p + 8;
	}

	if (!(ip_ptr && port_ptr)) {
		return SWITCH_STATUS_FALSE;
	}

	p = ip_ptr;
	x = 0;
	while (x < sizeof(rip) && p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
		rip[x++] = *p;
		p++;
	}

	p = port_ptr;
	x = 0;
	while (x < sizeof(rp) && p && *p && (*p >= '0' && *p <= '9')) {
		rp[x++] = *p;
		p++;
	}

	p = vid_port_ptr;
	x = 0;
	while (x < sizeof(rvp) && p && *p && (*p >= '0' && *p <= '9')) {
		rvp[x++] = *p;
		p++;
	}

	if (!(*rip && *rp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid SDP\n");
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, rip);
	tech_pvt->remote_sdp_audio_port = (switch_port_t) atoi(rp);
	
	if (*rvp) {
		tech_pvt->remote_sdp_video_ip = switch_core_session_strdup(tech_pvt->session, rip);
		tech_pvt->remote_sdp_video_port = (switch_port_t) atoi(rvp);
	}

	if (tech_pvt->remote_sdp_video_ip && tech_pvt->remote_sdp_video_port) {
		if (!strcmp(tech_pvt->remote_sdp_video_ip, rip) && atoi(rvp) == tech_pvt->remote_sdp_video_port) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote video address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		} else {
			switch_set_flag_locked(tech_pvt, TFLAG_VIDEO);
			switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				if (switch_rtp_set_remote_address(tech_pvt->video_rtp_session, tech_pvt->remote_sdp_video_ip, 
												  tech_pvt->remote_sdp_video_port, SWITCH_TRUE, &err) !=
					SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port);
					if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
						!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(tech_pvt->video_rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}
				}
			}
		}
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
		
		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			return SWITCH_STATUS_SUCCESS;
		}

	}
	
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port, SWITCH_TRUE, &err) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);	
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


void sofia_glue_tech_patch_sdp(private_object_t *tech_pvt)
{
	switch_size_t len;
	char *p, *q;
	int has_video=0,has_audio=0,has_ip=0;
	char port_buf[25] = "";
	char vport_buf[25] = "";

	if (switch_strlen_zero(tech_pvt->local_sdp_str)) {
		return;
	}

	len = strlen(tech_pvt->local_sdp_str) + 384;

	if (switch_stristr("sendonly", tech_pvt->local_sdp_str) || switch_stristr("0.0.0.0", tech_pvt->local_sdp_str)) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Skip patch on hold SDP\n");
	    return;
	}
	
	if (switch_strlen_zero(tech_pvt->adv_sdp_audio_ip) || !tech_pvt->adv_sdp_audio_port) {
	    if (sofia_glue_tech_choose_port(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s I/O Error\n", switch_channel_get_name(tech_pvt->channel));
			return;
		}
		tech_pvt->iananame = switch_core_session_strdup(tech_pvt->session, "PROXY");
		tech_pvt->rm_rate = 8000;
		tech_pvt->codec_ms = 20;
	}
	
	switch_snprintf(port_buf, sizeof(port_buf), "%u", tech_pvt->adv_sdp_audio_port);
	tech_pvt->orig_local_sdp_str = tech_pvt->local_sdp_str;
	tech_pvt->local_sdp_str = switch_core_session_alloc(tech_pvt->session, len);

	p = tech_pvt->orig_local_sdp_str;
	q = tech_pvt->local_sdp_str;

	while(p && *p) {
	    if (tech_pvt->adv_sdp_audio_ip && !strncmp("c=IN IP", p, 7)) {
			strncpy(q, p, 9);
			p += 9;
			q += 9;
			strncpy(q, tech_pvt->adv_sdp_audio_ip, strlen(tech_pvt->adv_sdp_audio_ip));
			q += strlen(tech_pvt->adv_sdp_audio_ip);

			while(p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) {
				p++;
			}

	    	has_ip++;

		} else if (!strncmp("m=audio ", p, 8) || (!strncmp("m=image ", p, 8))) {
			strncpy(q,p,8);
			p += 8;
			q += 8;
			strncpy(q, port_buf, strlen(port_buf));
			q += strlen(port_buf);

			while (p && *p && (*p >= '0' && *p <= '9')) {
				p++;
			}

			has_audio++;		

	    } else if (!strncmp("m=video ", p, 8)) {
			if (!has_video) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
				tech_pvt->video_rm_encoding = "PROXY-VID";
				tech_pvt->video_rm_rate = 90000;
				tech_pvt->video_codec_ms = 0;
				switch_snprintf(vport_buf, sizeof(vport_buf), "%u", tech_pvt->adv_sdp_video_port);
			}

			strncpy(q, p, 8);
			p += 8;
			q += 8;
			strncpy(q, vport_buf, strlen(vport_buf));
			q += strlen(vport_buf);

			while (p && *p && (*p >= '0' && *p <= '9')) {
				p++;
			}

			has_video++;
	    }
	    
	    while (p && *p && *p != '\n') {
			*q++ = *p++;
	    }

	    *q++ = *p++;
	}

	if (!has_ip && !has_audio) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s SDP has no audio in it.\n%s\n",
						  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str);
		tech_pvt->local_sdp_str = tech_pvt->orig_local_sdp_str;
		return;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Patched SDP\n---\n%s\n+++\n%s\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->orig_local_sdp_str, tech_pvt->local_sdp_str);
}


switch_status_t sofia_glue_do_invite(switch_core_session_t *session)
{
	char *alert_info = NULL;
	const char *max_forwards = NULL;
	const char *alertbuf;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *cid_name, *cid_num;
	char *e_dest = NULL;
	const char *holdstr = "";
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi;
	char *extra_headers = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t session_timeout = 0;
	const char *val;
	const char *rep;
	const char *call_id = NULL;
	char *route = NULL;
	char *route_uri = NULL;

	rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

	switch_assert(tech_pvt != NULL);

	switch_clear_flag_locked(tech_pvt, TFLAG_SDP);

	caller_profile = switch_channel_get_caller_profile(channel);

	cid_name = caller_profile->caller_id_name;
	cid_num = caller_profile->caller_id_number;
	sofia_glue_tech_prepare_codecs(tech_pvt);
	sofia_glue_check_video_codecs(tech_pvt);
	check_decode(cid_name, session);
	check_decode(cid_num, session);

	if (!tech_pvt->from_str) {
		const char* sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
		const char* format = strchr(sipip, ':') ? "\"%s\" <sip:%s%s[%s]>" : "\"%s\" <sip:%s%s%s>";
		const char *alt = NULL;

		if ((alt = switch_channel_get_variable(channel, "sip_invite_domain"))) {
			sipip = alt;
		}
			
		tech_pvt->from_str = 
			switch_core_session_sprintf(tech_pvt->session,
										format,
										cid_name,
										cid_num,
										!switch_strlen_zero(cid_num) ? "@" : "",
										sipip);
	}

	if ((alertbuf = switch_channel_get_variable(channel, "alert_info"))) {
		alert_info = switch_core_session_sprintf(tech_pvt->session, "Alert-Info: %s", alertbuf);
	}

	max_forwards = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);

	if ((status = sofia_glue_tech_choose_port(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);

	switch_set_flag_locked(tech_pvt, TFLAG_READY);

	if (!tech_pvt->nh) {
		char *d_url = NULL, *url = NULL;
		sofia_private_t *sofia_private;
		char *invite_contact = NULL, *to_str, *use_from_str, *from_str, *url_str;
		const char *t_var;
		char *rpid_domain = "cluecon.com", *p;
		const char *priv = "off";
		const char *screen = "no";
		const char *invite_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_params");

		if (switch_strlen_zero(tech_pvt->dest)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "URL Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		if ((d_url = sofia_glue_get_url_from_contact(tech_pvt->dest, 1))) {
			url = d_url;
		} else {
			url = tech_pvt->dest;
		}

		url_str = url;

		if (!switch_strlen_zero(tech_pvt->gateway_from_str)) {
			use_from_str = tech_pvt->gateway_from_str;
		} else {
			use_from_str = tech_pvt->from_str;
		}

		rpid_domain = switch_core_session_strdup(session, use_from_str);
		sofia_glue_get_url_from_contact(rpid_domain, 0);
		if ((rpid_domain = strrchr(rpid_domain, '@'))) {
			rpid_domain++;
			if ((p = strchr(rpid_domain, ';'))) {
				*p = '\0';
			}
		}

		if (!rpid_domain) {
			rpid_domain = "cluecon.com";
		}

		/*
		 * Ignore transport chanvar and uri parameter for gateway connections
		 * since all of them have been already taken care of in mod_sofia.c:sofia_outgoing_channel()
		 */
		if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN && switch_strlen_zero(tech_pvt->gateway_name)) {
			if ((p = (char *) switch_stristr("port=", url))) {
				p += 5;
				tech_pvt->transport = sofia_glue_str2transport(p);
			} else {
				if ((t_var = switch_channel_get_variable(channel, "sip_transport"))) {
					tech_pvt->transport = sofia_glue_str2transport(t_var);
				}
			}

			if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN) {
				tech_pvt->transport = SOFIA_TRANSPORT_UDP;
			}
		}

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_TLS) && sofia_glue_transport_has_tls(tech_pvt->transport)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "TLS not supported by profile\n");
			return SWITCH_STATUS_FALSE;
		}

		if (switch_strlen_zero(tech_pvt->invite_contact)) {
			const char * contact;
			if ((contact = switch_channel_get_variable(channel, "sip_contact_user"))) {
				char *ip_addr = (tech_pvt->profile->extsipip) ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
				char *ipv6 = strchr(ip_addr, ':');
				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact, 
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", 
																		   tech_pvt->profile->tls_sip_port);
				} else {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s", contact, 
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "");
				}
			} else {
				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = tech_pvt->profile->tls_url;
				} else {
					tech_pvt->invite_contact = tech_pvt->profile->url;
				}
			}
		}

		url_str = sofia_overcome_sip_uri_weakness(session, url, tech_pvt->transport, SWITCH_TRUE, invite_params);
		invite_contact = sofia_overcome_sip_uri_weakness(session, tech_pvt->invite_contact, tech_pvt->transport, SWITCH_FALSE, NULL);
		from_str = sofia_overcome_sip_uri_weakness(session, use_from_str, 0, SWITCH_TRUE, NULL);
		to_str = sofia_overcome_sip_uri_weakness(session, tech_pvt->dest_to, 0, SWITCH_FALSE, invite_params);


		/*
		   Does the "genius" who wanted SIP to be "text-based" so it was "easier to read" even use it now,
		   or did he just suggest it to make our lives miserable?
		 */
		use_from_str = from_str;
		from_str = switch_core_session_sprintf(session, "\"%s\" <%s>", tech_pvt->caller_profile->caller_id_name, use_from_str);
		

		tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL,
								  NUTAG_URL(url_str),
								  SIPTAG_TO_STR(to_str), 
								  SIPTAG_FROM_STR(from_str),
								  SIPTAG_CONTACT_STR(invite_contact),
								  TAG_END());
		

		if (tech_pvt->dest && (strstr(tech_pvt->dest, ";fs_nat") || strstr(tech_pvt->dest, ";received") 
							   || ((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val)))) {
			switch_set_flag(tech_pvt, TFLAG_NAT);
			tech_pvt->record_route = switch_core_session_strdup(tech_pvt->session, url_str);
			route_uri = tech_pvt->record_route;
			session_timeout = SOFIA_NAT_SESSION_TIMEOUT;
			switch_channel_set_variable(channel, "sip_nat_detected", "true");
		}

		/* TODO: We should use the new tags for making an rpid and add profile options to turn this on/off */
		if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NAME)) {
			priv = "name";
			if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
				priv = "full";
			}
		} else if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
			priv = "full";
		}

		if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
			screen = "yes";
		}

		tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "Remote-Party-ID: \"%s\"<sip:%s@%s>;screen=%s;privacy=%s",
													 tech_pvt->caller_profile->caller_id_name,
													 tech_pvt->caller_profile->caller_id_number, rpid_domain, screen, priv);

		switch_safe_free(d_url);

		if (!(sofia_private = malloc(sizeof(*sofia_private)))) {
			abort();
		}

		memset(sofia_private, 0, sizeof(*sofia_private));
		sofia_private->is_call++;

		tech_pvt->sofia_private = sofia_private;
		switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
		nua_handle_bind(tech_pvt->nh, tech_pvt->sofia_private);
	}

	if (tech_pvt->e_dest) {
		char *user = NULL, *host = NULL;
		char hash_key[256] = "";

		e_dest = strdup(tech_pvt->e_dest);
		switch_assert(e_dest != NULL);
		user = e_dest;

		if ((host = strchr(user, '@'))) {
			*host++ = '\0';
		}
		switch_snprintf(hash_key, sizeof(hash_key), "%s%s%s", user, host, cid_num);

		tech_pvt->chat_from = tech_pvt->from_str;
		tech_pvt->chat_to = tech_pvt->dest;
		if (tech_pvt->profile->pres_type) {
			tech_pvt->hash_key = switch_core_session_strdup(tech_pvt->session, hash_key);
			switch_mutex_lock(tech_pvt->profile->flag_mutex);
			switch_core_hash_insert(tech_pvt->profile->chat_hash, tech_pvt->hash_key, tech_pvt);
			switch_mutex_unlock(tech_pvt->profile->flag_mutex);
		}
		free(e_dest);
	}

	holdstr = switch_test_flag(tech_pvt, TFLAG_SIP_HOLD) ? "*" : "";

	if (!switch_channel_get_variable(channel, "sofia_profile_name")) {
		switch_channel_set_variable(channel, "sofia_profile_name", tech_pvt->profile->name);
	}

	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;

			if (!strncasecmp(name, SOFIA_SIP_HEADER_PREFIX, strlen(SOFIA_SIP_HEADER_PREFIX))) {
				const char *hname = name + strlen(SOFIA_SIP_HEADER_PREFIX);
				stream.write_function(&stream, "%s: %s\r\n", hname, value);
			}
		}
		switch_channel_variable_last(channel);
	}

	if (stream.data) {
		extra_headers = stream.data;
	}

	session_timeout = tech_pvt->profile->session_timeout;
	if ((val = switch_channel_get_variable(channel, SOFIA_SESSION_TIMEOUT))) {
		int v_session_timeout = atoi(val);
		if (v_session_timeout >= 0) {
			session_timeout = v_session_timeout;
		}
	}

	if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			sofia_glue_tech_proxy_remote_addr(tech_pvt);
		}
		sofia_glue_tech_patch_sdp(tech_pvt);
	}

	call_id = switch_channel_get_variable(channel, "sip_outgoing_call_id");

	if (tech_pvt->dest && (route = strstr(tech_pvt->dest, ";fs_path="))) {
		char *p;

		route = switch_core_session_strdup(tech_pvt->session, route + 9);
		switch_assert(route);

		for (p = route; p && *p ; p++) {
			if (*p == '>' || *p == ';') {
				*p = '\0';
				break;
			}
		}
		switch_url_decode(route);
		route_uri = switch_core_session_strdup(tech_pvt->session, route);
		if ((p = strchr(route_uri, ','))) {
			while (*(p-1) == ' ') {
				p--;
			}
			if (*p) {
				*p = '\0';
			}
		}
		
		route_uri = sofia_overcome_sip_uri_weakness(tech_pvt->session, route_uri, 0, SWITCH_TRUE, NULL);
	}


	if (route_uri) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Setting proxy route to %s\n", route_uri, switch_channel_get_name(channel));
	}

	nua_invite(tech_pvt->nh,
			   NUTAG_AUTOANSWER(0),
			   NUTAG_SESSION_TIMER(session_timeout),
			   TAG_IF(!switch_strlen_zero(tech_pvt->rpid), SIPTAG_HEADER_STR(tech_pvt->rpid)),
			   TAG_IF(!switch_strlen_zero(alert_info), SIPTAG_HEADER_STR(alert_info)),
			   TAG_IF(!switch_strlen_zero(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
			   TAG_IF(!switch_strlen_zero(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
			   TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
			   TAG_IF(route_uri, NUTAG_PROXY(route_uri)),
			   TAG_IF(route, SIPTAG_ROUTE_STR(route)),
			   SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip),
			   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
			   SOATAG_REUSE_REJECTED(1),
			   SOATAG_ORDERED_USER(1),
			   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE),
			   SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), SOATAG_HOLD(holdstr), TAG_END());

	switch_safe_free(stream.data);

	return SWITCH_STATUS_SUCCESS;
}

void sofia_glue_do_xfer_invite(switch_core_session_t *session)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *sipip, *format; 

	switch_assert(tech_pvt != NULL);
	switch_mutex_lock(tech_pvt->sofia_mutex);
	caller_profile = switch_channel_get_caller_profile(channel);

	sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
	format = strchr(sipip, ':') ? "\"%s\" <sip:%s@[%s]>" : "\"%s\" <sip:%s@%s>";
	if ((tech_pvt->from_str = switch_core_session_sprintf(session, format,
															caller_profile->caller_id_name,
															caller_profile->caller_id_number, sipip))) {

		const char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		tech_pvt->nh2 = nua_handle(tech_pvt->profile->nua, NULL,
								   SIPTAG_TO_STR(tech_pvt->dest),
								   SIPTAG_FROM_STR(tech_pvt->from_str), SIPTAG_CONTACT_STR(tech_pvt->profile->url), TAG_END());

		nua_handle_bind(tech_pvt->nh2, tech_pvt->sofia_private);

		nua_invite(tech_pvt->nh2,
				   SIPTAG_CONTACT_STR(tech_pvt->profile->url),
				   SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip),
				   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				   SOATAG_REUSE_REJECTED(1),
				   SOATAG_ORDERED_USER(1),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), TAG_END());
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
	}
	switch_mutex_unlock(tech_pvt->sofia_mutex);
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
		tech_pvt->local_sdp_str = switch_core_session_strdup(tech_pvt->session, sdp_str);
	}
}

void sofia_glue_deactivate_rtp(private_object_t *tech_pvt)
{
	int loops = 0;
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		while (loops < 10 && (switch_test_flag(tech_pvt, TFLAG_READING) || switch_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}
	}

	if (tech_pvt->rtp_session) {
		switch_rtp_destroy(&tech_pvt->rtp_session);
	} else if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_audio_port);
	}

	if (tech_pvt->video_rtp_session) {
		switch_rtp_destroy(&tech_pvt->video_rtp_session);
	} else if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->profile->rtpip, tech_pvt->local_sdp_video_port);
	}

}

switch_status_t sofia_glue_tech_set_video_codec(private_object_t *tech_pvt, int force)
{

	if (tech_pvt->video_read_codec.implementation) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding) ||
			tech_pvt->video_read_codec.implementation->samples_per_second != tech_pvt->video_rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->video_read_codec.implementation->iananame, tech_pvt->video_rm_encoding);
			switch_core_codec_destroy(&tech_pvt->video_read_codec);
			switch_core_codec_destroy(&tech_pvt->video_write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->video_read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (!tech_pvt->video_rm_encoding) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec with no name?\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init(&tech_pvt->video_read_codec,
							   tech_pvt->video_rm_encoding,
							   tech_pvt->video_rm_fmtp,
							   tech_pvt->video_rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			int ms;
			tech_pvt->video_read_frame.rate = tech_pvt->video_rm_rate;
			ms = tech_pvt->video_write_codec.implementation->microseconds_per_packet / 1000;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(tech_pvt->channel), tech_pvt->video_rm_encoding, tech_pvt->video_rm_rate, tech_pvt->video_codec_ms);
			tech_pvt->video_read_frame.codec = &tech_pvt->video_read_codec;

			tech_pvt->video_fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->video_write_codec.fmtp_out);

			tech_pvt->video_write_codec.agreed_pt = tech_pvt->video_agreed_pt;
			tech_pvt->video_read_codec.agreed_pt = tech_pvt->video_agreed_pt;
			switch_core_session_set_video_read_codec(tech_pvt->session, &tech_pvt->video_read_codec);
			switch_core_session_set_video_write_codec(tech_pvt->session, &tech_pvt->video_write_codec);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_tech_set_codec(private_object_t *tech_pvt, int force)
{
	int ms;

	if (!tech_pvt->iananame) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No audio codec available\n");
		return SWITCH_STATUS_FALSE;
	}

	if (tech_pvt->read_codec.implementation) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(tech_pvt->read_codec.implementation->iananame, tech_pvt->iananame) ||
			tech_pvt->read_codec.implementation->samples_per_second != tech_pvt->rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  tech_pvt->read_codec.implementation->iananame, tech_pvt->rm_encoding);
			switch_core_codec_destroy(&tech_pvt->read_codec);
			switch_core_codec_destroy(&tech_pvt->write_codec);
			switch_core_session_reset(tech_pvt->session, SWITCH_TRUE);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Already using %s\n", tech_pvt->read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}
	
	if (switch_core_codec_init(&tech_pvt->read_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_codec_init(&tech_pvt->write_codec,
							   tech_pvt->iananame,
							   tech_pvt->rm_fmtp,
							   tech_pvt->rm_rate,
							   tech_pvt->codec_ms,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | tech_pvt->profile->codec_flags,
							   NULL, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_set_default_samples_per_interval(tech_pvt->rtp_session, tech_pvt->read_codec.implementation->samples_per_packet);
	}

	tech_pvt->read_frame.rate = tech_pvt->rm_rate;
	ms = tech_pvt->write_codec.implementation->microseconds_per_packet / 1000;

	if (!tech_pvt->read_codec.implementation) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->iananame, tech_pvt->rm_rate, tech_pvt->codec_ms,
					  tech_pvt->read_codec.implementation->samples_per_packet);
	tech_pvt->read_frame.codec = &tech_pvt->read_codec;

	tech_pvt->write_codec.agreed_pt = tech_pvt->agreed_pt;
	tech_pvt->read_codec.agreed_pt = tech_pvt->agreed_pt;

	if (force != 2) {
		switch_core_session_set_read_codec(tech_pvt->session, &tech_pvt->read_codec);
		switch_core_session_set_write_codec(tech_pvt->session, &tech_pvt->write_codec);
	}

	tech_pvt->fmtp_out = switch_core_session_strdup(tech_pvt->session, tech_pvt->write_codec.fmtp_out);

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		switch_rtp_set_default_payload(tech_pvt->rtp_session, tech_pvt->pt);
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t sofia_glue_build_crypto(private_object_t *tech_pvt, int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction)
{
	unsigned char b64_key[512] = "";
	const char *type_str;
	unsigned char *key;
	const char *val;

	char *p;

	if (type == AES_CM_128_HMAC_SHA1_80) {
		type_str = SWITCH_RTP_CRYPTO_KEY_80;
	} else {
		type_str = SWITCH_RTP_CRYPTO_KEY_32;
	}

	if (direction == SWITCH_RTP_CRYPTO_SEND) {
		key = tech_pvt->local_raw_key;
	} else {
		key = tech_pvt->remote_raw_key;

	}

	switch_rtp_get_random(key, SWITCH_RTP_KEY_LEN);
	switch_b64_encode(key, SWITCH_RTP_KEY_LEN, b64_key, sizeof(b64_key));
	p = strrchr((char *) b64_key, '=');

	while (p && *p && *p == '=') {
		*p-- = '\0';
	}

	tech_pvt->local_crypto_key = switch_core_session_sprintf(tech_pvt->session, "%d %s inline:%s", index, type_str, b64_key);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Local Key [%s]\n", tech_pvt->local_crypto_key);

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_SRTP_AUTH) &&
		!((val = switch_channel_get_variable(tech_pvt->channel, "NDLB_support_asterisk_missing_srtp_auth")) && switch_true(val))) {
		tech_pvt->crypto_type = type;
	} else {
		tech_pvt->crypto_type = AES_CM_128_NULL_AUTH;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t sofia_glue_add_crypto(private_object_t *tech_pvt, const char *key_str, switch_rtp_crypto_direction_t direction)
{
	unsigned char key[SWITCH_RTP_MAX_CRYPTO_LEN];
	int index;
	switch_rtp_crypto_key_type_t type;
	char *p;


	if (!switch_rtp_ready(tech_pvt->rtp_session)) {
		goto bad;
	}

	index = atoi(key_str);

	p = strchr(key_str, ' ');

	if (p && *p && *(p + 1)) {
		p++;
		if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_32, strlen(SWITCH_RTP_CRYPTO_KEY_32))) {
			type = AES_CM_128_HMAC_SHA1_32;
		} else if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_80, strlen(SWITCH_RTP_CRYPTO_KEY_80))) {
			type = AES_CM_128_HMAC_SHA1_80;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
			goto bad;
		}

		p = strchr(p, ' ');
		if (p && *p && *(p + 1)) {
			p++;
			if (strncasecmp(p, "inline:", 7)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
				goto bad;
			}

			p += 7;
			switch_b64_decode(p, (char *) key, sizeof(key));

			if (direction == SWITCH_RTP_CRYPTO_SEND) {
				tech_pvt->crypto_send_type = type;
				memcpy(tech_pvt->local_raw_key, key, SWITCH_RTP_KEY_LEN);
			} else {
				tech_pvt->crypto_recv_type = type;
				memcpy(tech_pvt->remote_raw_key, key, SWITCH_RTP_KEY_LEN);
			}
			return SWITCH_STATUS_SUCCESS;
		}

	}

  bad:

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	return SWITCH_STATUS_FALSE;

}


switch_status_t sofia_glue_activate_rtp(private_object_t *tech_pvt, switch_rtp_flag_t myflags)
{
	int bw, ms;
	const char *err = NULL;
	const char *val = NULL;
	switch_rtp_flag_t flags;
	switch_status_t status;
	char tmp[50];
	uint32_t rtp_timeout_sec = tech_pvt->profile->rtp_timeout_sec;
	uint32_t rtp_hold_timeout_sec = tech_pvt->profile->rtp_hold_timeout_sec;
	char *timer_name = NULL;
	const char *var;

	switch_assert(tech_pvt != NULL);
	switch_mutex_lock(tech_pvt->sofia_mutex);

	if ((var = switch_channel_get_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_VARIABLE)) && switch_true(var)) {
		switch_set_flag_locked(tech_pvt, TFLAG_SECURE);
	}


	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if (switch_rtp_ready(tech_pvt->rtp_session) && !switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if ((status = sofia_glue_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	bw = tech_pvt->read_codec.implementation->bits_per_second;
	ms = tech_pvt->read_codec.implementation->microseconds_per_packet;

	if (myflags) {
		flags = myflags;
	} else if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
    } else {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_DATAWAIT);
	}

	if (switch_test_flag(tech_pvt, TFLAG_BUGGY_2833)) {
		flags |= SWITCH_RTP_FLAG_BUGGY_2833;
	}

	if ((val = switch_channel_get_variable(tech_pvt->channel, "dtmf_type"))) {
		if (!strcasecmp(val, "rfc2833")) {
			tech_pvt->dtmf_type = DTMF_2833;
		} else if (!strcasecmp(val, "info")) {
			tech_pvt->dtmf_type = DTMF_INFO;
		} else {
			tech_pvt->dtmf_type = tech_pvt->profile->dtmf_type;
		}
	}

	if ((tech_pvt->profile->pflags & PFLAG_PASS_RFC2833)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "pass_rfc2833")) && switch_true(val))) {
		flags |= SWITCH_RTP_FLAG_PASS_RFC2833;
	}

	if (!((tech_pvt->profile->pflags & PFLAG_REWRITE_TIMESTAMPS) ||
		  ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_rewrite_timestamps")) && !switch_true(val)))) {
		flags |= SWITCH_RTP_FLAG_RAW_WRITE;
	}

	if (tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG) {
		tech_pvt->cng_pt = 0;
	} else if (tech_pvt->cng_pt) {
		flags |= SWITCH_RTP_FLAG_AUTO_CNG;
	}

	if (tech_pvt->rtp_session && switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		//const char *ip = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
		//const char *port = switch_channel_get_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
		
		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n", switch_channel_get_name(tech_pvt->channel));
			goto video;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n", 
							  switch_channel_get_name(tech_pvt->channel),
							  remote_host, remote_port, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		}
	}

	if (!switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AUDIO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_codec.implementation->microseconds_per_packet / 1000);
	}

	switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);

	if (tech_pvt->rtp_session && switch_test_flag(tech_pvt, TFLAG_REINVITE)) {
		switch_clear_flag_locked(tech_pvt, TFLAG_REINVITE);
		
		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port, SWITCH_TRUE, &err) !=
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
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
		if ((status = sofia_glue_tech_proxy_remote_addr(tech_pvt)) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}
		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
			!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
		} else {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_DATAWAIT);
		}
		timer_name = NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PROXY AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
						  switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port, tech_pvt->agreed_pt, tech_pvt->read_codec.implementation->microseconds_per_packet / 1000);

	} else {
		timer_name = tech_pvt->profile->timer_name;
	}

	if ((var = switch_channel_get_variable(tech_pvt->channel, "rtp_timer_name"))) {
		timer_name = (char *) var;
	}

	tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
										   tech_pvt->local_sdp_audio_port,
										   tech_pvt->remote_sdp_audio_ip,
										   tech_pvt->remote_sdp_audio_port,
										   tech_pvt->agreed_pt,
										   tech_pvt->read_codec.implementation->samples_per_packet,
										   tech_pvt->codec_ms * 1000,
										   (switch_rtp_flag_t) flags, timer_name, &err, switch_core_session_get_pool(tech_pvt->session));

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = switch_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = switch_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = switch_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;
		uint32_t stun_ping = 0;
		
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

			stun_ping = (ival * tech_pvt->read_codec.implementation->samples_per_second) / tech_pvt->read_codec.implementation->samples_per_packet;
		}

		tech_pvt->ssrc = switch_rtp_get_ssrc(tech_pvt->rtp_session);
		switch_set_flag(tech_pvt, TFLAG_RTP);
		switch_set_flag(tech_pvt, TFLAG_IO);

		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(tech_pvt->rtp_session, tech_pvt->session, &tech_pvt->read_codec, SWITCH_VAD_FLAG_TALKING);
			switch_set_flag(tech_pvt, TFLAG_VAD);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AUDIO RTP Engage VAD for %s ( %s %s )\n",
							  switch_channel_get_name(switch_core_session_get_channel(tech_pvt->session)), vad_in ? "in" : "", vad_out ? "out" : "");
		}

		if (stun_ping) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting stun ping to %s:%d\n", tech_pvt->stun_ip, stun_ping);
			switch_rtp_activate_stun_ping(tech_pvt->rtp_session, tech_pvt->stun_ip, tech_pvt->stun_port, 
										  stun_ping, (tech_pvt->stun_flags & STUN_FLAG_FUNNY) ? 1 : 0);
		}

		if ((val = switch_channel_get_variable(tech_pvt->channel, "jitterbuffer_msec"))) {
			int len = atoi(val);

			if (len < 100 || len > 1000) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Jitterbuffer spec [%d] must be between 100 and 1000\n", len);
			} else {
				int qlen;

				qlen = len / (tech_pvt->read_codec.implementation->microseconds_per_packet / 1000);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setting Jitterbuffer to %dms (%d frames)\n", len, qlen);
				switch_rtp_activate_jitter_buffer(tech_pvt->rtp_session, qlen);
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
			tech_pvt->max_missed_packets = (tech_pvt->read_codec.implementation->samples_per_second * rtp_timeout_sec) /
				tech_pvt->read_codec.implementation->samples_per_packet;

			switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			if (!rtp_hold_timeout_sec) {
				rtp_hold_timeout_sec = rtp_timeout_sec * 10;
			}
		}

		if (rtp_hold_timeout_sec) {
			tech_pvt->max_missed_hold_packets = (tech_pvt->read_codec.implementation->samples_per_second * rtp_hold_timeout_sec) /
				tech_pvt->read_codec.implementation->samples_per_packet;
		}

		if (tech_pvt->te) {
			switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
		}

		if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "supress_cng")) && switch_true(val)) ||
			((val = switch_channel_get_variable(tech_pvt->channel, "suppress_cng")) && switch_true(val))) {
			tech_pvt->cng_pt = 0;
		}

		if (tech_pvt->cng_pt && !(tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", tech_pvt->cng_pt);
			switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
		}

		if (tech_pvt->remote_crypto_key && switch_test_flag(tech_pvt, TFLAG_SECURE)) {
			sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1, tech_pvt->crypto_type, tech_pvt->local_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag, tech_pvt->crypto_type, tech_pvt->remote_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_CONFIRMED_VARIABLE, "true");
		}

	  video:

		sofia_glue_check_video_codecs(tech_pvt);

		if (switch_test_flag(tech_pvt, TFLAG_VIDEO) && tech_pvt->video_rm_encoding && tech_pvt->remote_sdp_video_port) {
			if (!tech_pvt->local_sdp_video_port) {
				sofia_glue_tech_choose_video_port(tech_pvt, 1);
			}

			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_AUTOADJ |
		 									 SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_NOBLOCK | SWITCH_RTP_FLAG_RAW_WRITE);
			} else {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_NOBLOCK | SWITCH_RTP_FLAG_RAW_WRITE);
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				flags |= SWITCH_RTP_FLAG_PROXY_MEDIA;
			}
			sofia_glue_tech_set_video_codec(tech_pvt, 0);

			/* set video timer to 10ms so it can co-exist with audio */
			tech_pvt->video_rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
														 tech_pvt->local_sdp_video_port,
														 tech_pvt->remote_sdp_video_ip,
														 tech_pvt->remote_sdp_video_port,
														 tech_pvt->video_agreed_pt,
														 1, 10000, (switch_rtp_flag_t) flags, NULL, &err, switch_core_session_get_pool(tech_pvt->session));

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%sVIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(tech_pvt->channel),
							  tech_pvt->local_sdp_audio_ip,
							  tech_pvt->local_sdp_video_port,
							  tech_pvt->remote_sdp_video_ip,
							  tech_pvt->remote_sdp_video_port, tech_pvt->video_agreed_pt,
							  0, switch_rtp_ready(tech_pvt->video_rtp_session) ? "SUCCESS" : err);

			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_clear_flag_locked(tech_pvt, TFLAG_IO);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	switch_set_flag(tech_pvt, TFLAG_IO);
	status = SWITCH_STATUS_SUCCESS;

  end:

	switch_mutex_unlock(tech_pvt->sofia_mutex);

	return status;

}

switch_status_t sofia_glue_tech_media(private_object_t *tech_pvt, const char *r_sdp)
{
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	uint8_t match = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0);

	if (switch_strlen_zero(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if (tech_pvt->num_codecs) {
		if ((sdp = sdp_session(parser))) {
			match = sofia_glue_negotiate_sdp(tech_pvt->session, sdp);
		}
	}

	if (parser) {
		sdp_parser_free(parser);
	}

	if (match) {
		if (sofia_glue_tech_choose_port(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		if (sofia_glue_activate_rtp(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "EARLY MEDIA");
		switch_set_flag_locked(tech_pvt, TFLAG_EARLY_MEDIA);
		switch_channel_mark_pre_answered(tech_pvt->channel);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

void sofia_glue_toggle_hold(private_object_t *tech_pvt, int sendonly)
{
	if (sendonly && switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
		if (!switch_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *stream;

			switch_set_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_presence(tech_pvt->channel, "unknown", "hold", NULL);

			if (tech_pvt->max_missed_hold_packets) {
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_hold_packets);
			}

			if (!(stream = switch_channel_get_variable(tech_pvt->channel, SWITCH_HOLD_MUSIC_VARIABLE))) {
				stream = tech_pvt->profile->hold_music;
			}

			if (stream && switch_is_moh(stream)) {
				if (!strcasecmp(stream, "indicate_hold")) {
					switch_channel_set_flag(tech_pvt->channel, CF_SUSPEND);
					switch_channel_set_flag(tech_pvt->channel, CF_HOLD);
					switch_ivr_hold_uuid(switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE), NULL, 0);
				} else {
					switch_ivr_broadcast(switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE), stream, SMF_ECHO_ALEG | SMF_LOOP);
					switch_yield(250000);
				}
			}
		}
	} else {
		if (switch_test_flag(tech_pvt, TFLAG_HOLD_LOCK)) {
			switch_set_flag(tech_pvt, TFLAG_SIP_HOLD);
		}

		switch_clear_flag_locked(tech_pvt, TFLAG_HOLD_LOCK);
		
		if (switch_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
			const char *uuid;
			switch_core_session_t *b_session;

			switch_yield(250000);

			if (tech_pvt->max_missed_packets) {
				switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
			}

			if ((uuid = switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE)) && (b_session = switch_core_session_locate(uuid))) {
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

			switch_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
			switch_channel_presence(tech_pvt->channel, "unknown", "unhold", NULL);
		}
	}
}

uint8_t sofia_glue_negotiate_sdp(switch_core_session_t *session, sdp_session_t *sdp)
{
	uint8_t match = 0;
	switch_payload_t te = 0, cng_pt = 0;
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int first = 0, last = 0;
	int ptime = 0, dptime = 0;
	int sendonly = 0;
	int greedy = 0, x = 0, skip = 0, mine = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *val;
	const char *crypto = NULL;
	int got_crypto = 0, got_audio = 0, got_avp = 0, got_savp = 0;

	switch_assert(tech_pvt != NULL);

	greedy = !!(tech_pvt->profile->pflags & PFLAG_GREEDY);

	if (!greedy) {
		if ((val = switch_channel_get_variable(channel, "sip_codec_negotiation")) && !strcasecmp(val, "greedy")) {
			greedy = 1;
		}
	}

	if ((tech_pvt->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {
		if (strstr(tech_pvt->origin, "CiscoSystemsSIP-GW-UserAgent")) {
			switch_set_flag_locked(tech_pvt, TFLAG_BUGGY_2833);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
		}
	}

	if ((m = sdp->sdp_media) && (m->m_mode == sdp_sendonly || m->m_mode == sdp_inactive)) {
		sendonly = 2;			/* global sendonly always wins */
	}

	for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
		if (switch_strlen_zero(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "sendonly") || !strcasecmp(attr->a_name, "inactive")) {
			sendonly = 1;
		} else if (sendonly < 2 && !strcasecmp(attr->a_name, "sendrecv")) {
			sendonly = 0;
		} else if (!strcasecmp(attr->a_name, "ptime")) {
			dptime = atoi(attr->a_value);
		}
	}

	if (!tech_pvt->hold_laps) {
		tech_pvt->hold_laps++;
		sofia_glue_toggle_hold(tech_pvt, sendonly);
	}

	for (m = sdp->sdp_media; m; m = m->m_next) {
		sdp_connection_t *connection;

		ptime = dptime;

		if (m->m_proto == sdp_proto_srtp) {
			got_savp++;
		} else if (m->m_proto == sdp_proto_rtp) {
			got_avp++;
		}

		if (m->m_type == sdp_media_audio && m->m_port && !got_audio) {
			sdp_rtpmap_t *map;

			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !switch_strlen_zero(attr->a_value)) {
					int crypto_tag;

					if (m->m_proto != sdp_proto_srtp) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
						match = 0;
						goto done;
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);

					if (tech_pvt->remote_crypto_key && switch_rtp_ready(tech_pvt->rtp_session)) {
						if (crypto_tag && crypto_tag == tech_pvt->crypto_tag) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Existing key is still valid.\n");
						} else {
							const char *a = switch_stristr("AES", tech_pvt->remote_crypto_key);
							const char *b = switch_stristr("AES", crypto);

							if (a && b && !strncasecmp(a, b, 23)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Change Remote key to [%s]\n", crypto);
								tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
								tech_pvt->crypto_tag = crypto_tag;

								if (switch_rtp_ready(tech_pvt->rtp_session) && switch_test_flag(tech_pvt, TFLAG_SECURE)) {
									sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
									switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag,
															  tech_pvt->crypto_type, tech_pvt->remote_raw_key, SWITCH_RTP_KEY_LEN);
								}
								got_crypto++;
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ignoring unacceptable key\n");
							}
						}
					} else if (!switch_rtp_ready(tech_pvt->rtp_session)) {
						tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set Remote Key [%s]\n", tech_pvt->remote_crypto_key);
						tech_pvt->crypto_tag = crypto_tag;
						got_crypto++;

						if (switch_strlen_zero(tech_pvt->local_crypto_key)) {
							if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_32);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_80);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
							}
						}
					}
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

		  greed:
			x = 0;

			if (tech_pvt->rm_encoding) {
				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if (map->rm_pt < 96) {
						match = (map->rm_pt == tech_pvt->pt) ? 1 : 0;
					} else {
						match = strcasecmp(switch_str_nil(map->rm_encoding), tech_pvt->iananame) ? 0 : 1;
					}

					if (match && connection->c_address && tech_pvt->remote_sdp_audio_ip && 
						!strcmp(connection->c_address, tech_pvt->remote_sdp_audio_ip) && m->m_port == tech_pvt->remote_sdp_audio_port) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Our existing sdp is still good [%s %s:%d], let's keep it.\n",
										  tech_pvt->rm_encoding, tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
						got_audio = 1;
						break;
					}
				}
			}



			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				uint32_t near_rate = 0;
				const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
				const char *rm_encoding;

				if (x++ < skip) {
					continue;
				}

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				if (!te && !strcasecmp(rm_encoding, "telephone-event")) {
					te = tech_pvt->te = (switch_payload_t) map->rm_pt;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set 2833 dtmf payload to %u\n", te);
					if (tech_pvt->rtp_session) {
						switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
					}
				}

				if (!(tech_pvt->profile->pflags & PFLAG_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (tech_pvt->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(tech_pvt->rtp_session, tech_pvt->cng_pt);
					}
				}

				if (match) {
					if (te && cng_pt) {
						break;
					}
					continue;
				}

				if (greedy) {
					first = mine;
					last = first + 1;
				} else {
					first = 0;
					last = tech_pvt->num_codecs;
				}

				for (i = first; i < last && i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];
					uint32_t codec_rate = imp->samples_per_second;
					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Audio Codec Compare [%s:%d:%u]/[%s:%d:%u]\n",
									  rm_encoding, map->rm_pt, (int) map->rm_rate, imp->iananame, imp->ianacode, codec_rate);
					if (map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}

					if (match) {
						if ((ptime && ptime * 1000 != imp->microseconds_per_packet) || map->rm_rate != codec_rate) {
							near_rate = map->rm_rate;
							near_match = imp;
							match = 0;
							continue;
						}
						mimp = imp;
						break;
					} else {
						match = 0;
					}
				}

				if (!match && near_match) {
					const switch_codec_implementation_t *search[1];
					char *prefs[1];
					char tmp[80];
					int num;

					switch_snprintf(tmp, sizeof(tmp), "%s@%uk@%ui", near_match->iananame, near_rate ? near_rate : near_match->samples_per_second, ptime);

					prefs[0] = tmp;
					num = switch_loadable_module_get_codecs_sorted(search, 1, prefs, 1);

					if (num) {
						mimp = search[0];
					} else {
						mimp = near_match;
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Substituting codec %s@%ui@%uh\n",
									  mimp->iananame, mimp->microseconds_per_packet / 1000, mimp->samples_per_second);
					match = 1;
				}

				if (!match && greedy) {
					skip++;
					continue;
				}

				if (mimp) {
					char tmp[50];
					tech_pvt->rm_encoding = switch_core_session_strdup(session, (char *) map->rm_encoding);
					tech_pvt->iananame = switch_core_session_strdup(session, (char *) mimp->iananame);
					tech_pvt->pt = (switch_payload_t) map->rm_pt;
					tech_pvt->rm_rate = map->rm_rate;
					tech_pvt->codec_ms = mimp->microseconds_per_packet / 1000;
					tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					tech_pvt->rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
					tech_pvt->remote_sdp_audio_port = (switch_port_t) m->m_port;
					tech_pvt->agreed_pt = (switch_payload_t) map->rm_pt;
					switch_snprintf(tmp, sizeof(tmp), "%d", tech_pvt->remote_sdp_audio_port);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, tech_pvt->remote_sdp_audio_ip);
					switch_channel_set_variable(tech_pvt->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);

				}

				if (match) {
					if (sofia_glue_tech_set_codec(tech_pvt, 1) != SWITCH_STATUS_SUCCESS) {
						match = 0;
					}
				}
			}

			if (!match && greedy && mine < tech_pvt->num_codecs) {
				mine++;
				skip = 0;
				goto greed;
			}

		} else if (m->m_type == sdp_media_video && m->m_port) {
			sdp_rtpmap_t *map;
			const char *rm_encoding;
			int framerate = 0;
			const switch_codec_implementation_t *mimp = NULL;
			int vmatch = 0, i;
			switch_channel_set_variable(tech_pvt->channel, "video_possible", "true");

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {

				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
						framerate = atoi(attr->a_value);
					}
				}
				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				for (i = 0; i < tech_pvt->num_codecs; i++) {
					const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

					if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d]/[%s:%d]\n",
									  rm_encoding, map->rm_pt, imp->iananame, imp->ianacode);
					if (map->rm_pt < 96) {
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
						break;
					} else {
						vmatch = 0;
					}
				}
			}
		}
	}
	
 done:
	tech_pvt->cng_pt = cng_pt;
	switch_set_flag_locked(tech_pvt, TFLAG_SDP);

	return match;
}

/* map sip responses to QSIG cause codes ala RFC4497 section 8.4.4 */
switch_call_cause_t sofia_glue_sip_cause_to_freeswitch(int status)
{
	switch (status) {
	case 200:
		return SWITCH_CAUSE_NORMAL_CLEARING;
	case 401:
	case 402:
	case 403:
	case 407:
	case 603:
		return SWITCH_CAUSE_CALL_REJECTED;
	case 404:
	case 485:
	case 604:
		return SWITCH_CAUSE_NO_ROUTE_DESTINATION;
	case 408:
	case 504:
		return SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
	case 410:
		return SWITCH_CAUSE_NUMBER_CHANGED;
	case 413:
	case 414:
	case 416:
	case 420:
	case 421:
	case 423:
	case 505:
	case 513:
		return SWITCH_CAUSE_INTERWORKING;
	case 480:
		return SWITCH_CAUSE_NO_USER_RESPONSE;
	case 400:
	case 481:
	case 500:
	case 503:
		return SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE;
	case 486:
	case 600:
		return SWITCH_CAUSE_USER_BUSY;
	case 484:
		return SWITCH_CAUSE_INVALID_NUMBER_FORMAT;
	case 488:
	case 606:
		return SWITCH_CAUSE_INCOMPATIBLE_DESTINATION;
	case 502:
		return SWITCH_CAUSE_NETWORK_OUT_OF_ORDER;
	case 405:
		return SWITCH_CAUSE_SERVICE_UNAVAILABLE;
	case 406:
	case 415:
	case 501:
		return SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED;
	case 482:
	case 483:
		return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
	case 487:
		return SWITCH_CAUSE_ORIGINATOR_CANCEL;
	default:
		return SWITCH_CAUSE_NORMAL_UNSPECIFIED;
	}
}

void sofia_glue_pass_sdp(private_object_t *tech_pvt, char *sdp)
{
	const char *val;
	switch_core_session_t *other_session;
	switch_channel_t *other_channel;

	if ((val = switch_channel_get_variable(tech_pvt->channel, SWITCH_SIGNAL_BOND_VARIABLE))
		&& (other_session = switch_core_session_locate(val))) {
		other_channel = switch_core_session_get_channel(other_session);
		switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);

		if (!switch_test_flag(tech_pvt, TFLAG_CHANGE_MEDIA) && (switch_channel_test_flag(other_channel, CF_OUTBOUND) &&
																switch_channel_test_flag(tech_pvt->channel, CF_OUTBOUND) &&
																switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE))) {
			switch_ivr_nomedia(val, SMF_FORCE);
			switch_set_flag_locked(tech_pvt, TFLAG_CHANGE_MEDIA);
		}
		switch_core_session_rwunlock(other_session);
	}
}

char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup)
{
	char *url = NULL, *e;

	if ((url = strchr(buf, '<')) && (e = strchr(url, '>'))) {
		url++;
		if (to_dup) {
			url = strdup(url);
			e = strchr(url, '>');
		}

		*e = '\0';
	} else {
		if (to_dup) {
			url = strdup(buf);
		} else {
			url = buf;
		}
	}
	return url;
}

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key)
{
	sofia_profile_t *profile;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((profile = (sofia_profile_t *) switch_core_hash_find(mod_sofia_globals.profile_hash, key))) {
		if (!(profile->pflags & PFLAG_RUNNING)) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not running\n", profile->name);
#endif
			profile = NULL;
			goto done;
		}
		if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is locked\n", profile->name);
#endif
			profile = NULL;
		}
	} else {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not in the hash\n", key);
#endif
	}
#ifdef SOFIA_DEBUG_RWLOCKS
	if (profile) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX LOCK %s\n", profile->name);
	}
#endif

  done:
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return profile;
}

void sofia_glue_release_profile__(const char *file, const char *func, int line, sofia_profile_t *profile)
{
	if (profile) {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX UNLOCK %s\n", profile->name);
#endif
		switch_thread_rwlock_unlock(profile->rwlock);
	}
}

switch_status_t sofia_glue_add_profile(char *key, sofia_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (!switch_core_hash_find(mod_sofia_globals.profile_hash, key)) {
		status = switch_core_hash_insert(mod_sofia_globals.profile_hash, key, profile);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return status;
}

void sofia_glue_del_gateway(sofia_gateway_t *gp)
{
	if (!gp->deleted) {
		if (gp->state != REG_STATE_NOREG) {
			gp->retry = 0;
			gp->state = REG_STATE_UNREGISTER;
		}

		gp->deleted = 1;
	}
}

void sofia_glue_restart_all_profiles(void) 
{
	switch_hash_index_t *hi;
	const void *var;
    void *val;
    sofia_profile_t *pptr;
	switch_xml_t xml_root;
	const char *err;

	if ((xml_root = switch_xml_open_root(1, &err))) {
		switch_xml_free(xml_root);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Reload XML [%s]\n", err);
	}

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
        for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
            if ((pptr = (sofia_profile_t *) val)) {
				int rsec = 10;
				int diff = (int) (switch_timestamp(NULL) - pptr->started);
				int remain = rsec - diff;
				if (sofia_test_pflag(pptr, PFLAG_RESPAWN) || !sofia_test_pflag(pptr, PFLAG_RUNNING)) {
					continue;
				}

				if (diff < rsec) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
									  "Profile %s must be up for at least %d seconds to stop/restart.\nPlease wait %d second%s\n",
									  pptr->name, rsec, remain, remain == 1 ? "" : "s");
					continue;
				}
				sofia_set_pflag_locked(pptr, PFLAG_RESPAWN);
				sofia_clear_pflag_locked(pptr, PFLAG_RUNNING);
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_del_profile(sofia_profile_t *profile)
{
	sofia_gateway_t *gp;
	char *aliases[512];
	int i = 0, j = 0;
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val) && pptr == profile) {
				aliases[i++] = strdup((char *) var);
				if (i == 512) {
					abort();
				}
			}
		}

		for (j = 0; j < i && j < 512; j++) {
			switch_core_hash_delete(mod_sofia_globals.profile_hash, aliases[j]);
			free(aliases[j]);
		}

		for (gp = profile->gateways; gp; gp = gp->next) {
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->name);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->register_from);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->register_contact);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "deleted gateway %s\n", gp->name);
			profile->gateways = NULL;
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

int sofia_glue_init_sql(sofia_profile_t *profile)
{
	char *test_sql = NULL;

	char reg_sql[] =
		"CREATE TABLE sip_registrations (\n"
		"   call_id         VARCHAR(255),\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   presence_hosts  VARCHAR(255),\n"
		"   contact         VARCHAR(1024),\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n" 
		"   expires         INTEGER,\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   server_user     VARCHAR(255),\n"
		"   server_host     VARCHAR(255),\n" 
		"   profile_name    VARCHAR(255),\n" 
		"   hostname        VARCHAR(255)\n" 
		");\n";

	char dialog_sql[] =
		"CREATE TABLE sip_dialogs (\n"
		"   call_id         VARCHAR(255),\n"
		"   uuid            VARCHAR(255),\n"
		"   sip_to_user     VARCHAR(255),\n"
		"   sip_to_host     VARCHAR(255),\n"
		"   sip_from_user   VARCHAR(255),\n"
		"   sip_from_host   VARCHAR(255),\n"
		"   contact_user    VARCHAR(255),\n"
		"   contact_host    VARCHAR(255),\n"
		"   state           VARCHAR(255),\n" 
		"   direction       VARCHAR(255),\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   profile_name    VARCHAR(255),\n"
        "   hostname        VARCHAR(255)\n"
		");\n";

	char sub_sql[] =
		"CREATE TABLE sip_subscriptions (\n"
		"   proto           VARCHAR(255),\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   sub_to_user     VARCHAR(255),\n"
		"   sub_to_host     VARCHAR(255),\n"
		"   presence_hosts  VARCHAR(255),\n"
		"   event           VARCHAR(255),\n"
		"   contact         VARCHAR(1024),\n"
		"   call_id         VARCHAR(255),\n"
		"   full_from       VARCHAR(255),\n"
		"   full_via        VARCHAR(255),\n"
		"   expires         INTEGER,\n" 
		"   user_agent      VARCHAR(255),\n" 
		"   accept          VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255)\n"
		");\n";

	char auth_sql[] = 
		"CREATE TABLE sip_authentication (\n" 
		"   nonce           VARCHAR(255),\n" 
		"   expires         INTEGER," 
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255)\n"
		");\n";

	if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
		if (!(profile->master_odbc = switch_odbc_handle_new(profile->odbc_dsn, profile->odbc_user, profile->odbc_pass))) {
			return 0;
		}
		if (switch_odbc_handle_connect(profile->master_odbc) != SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Connecting ODBC DSN: %s\n", profile->odbc_dsn);
			return 0;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Connected ODBC DSN: %s\n", profile->odbc_dsn);
		
		test_sql = switch_mprintf("delete from sip_registrations where (contact like '%%TCP%%' "
								  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q'", 
								  mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_registrations", NULL);
			switch_odbc_handle_exec(profile->master_odbc, reg_sql, NULL);
		}
		free(test_sql);


		test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_subscriptions", NULL);
			switch_odbc_handle_exec(profile->master_odbc, sub_sql, NULL);
		}

		free(test_sql);
		test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_dialogs", NULL);
			switch_odbc_handle_exec(profile->master_odbc, dialog_sql, NULL);
		}

		free(test_sql);
		test_sql = switch_mprintf("delete from sip_authentication where hostname='%q'", mod_sofia_globals.hostname);

		if (switch_odbc_handle_exec(profile->master_odbc, test_sql, NULL) != SWITCH_ODBC_SUCCESS) {
			switch_odbc_handle_exec(profile->master_odbc, "DROP TABLE sip_authentication", NULL);
			switch_odbc_handle_exec(profile->master_odbc, auth_sql, NULL);
		}
		free(test_sql);

#else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
	} else {		
		if (!(profile->master_db = switch_core_db_open_file(profile->dbname))) {
			return 0;
		}

		test_sql = switch_mprintf("delete from sip_registrations where (contact like '%%TCP%%' "
								  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q'", 
								  mod_sofia_globals.hostname);
		
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_registrations", reg_sql);
		free(test_sql);

		test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_subscriptions", sub_sql);
		free(test_sql);
		
		test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_dialogs", dialog_sql);
		free(test_sql);
		test_sql = switch_mprintf("delete from sip_authentication where hostname='%q'", mod_sofia_globals.hostname);
		switch_core_db_test_reactive(profile->master_db, test_sql, "DROP TABLE sip_authentication", auth_sql);
		free(test_sql);
	}

#ifdef SWITCH_HAVE_ODBC
	if (profile->odbc_dsn) {
		return profile->master_odbc ? 1 : 0;
	}
#endif

	return profile->master_db ? 1 : 0;
}

void sofia_glue_sql_close(sofia_profile_t *profile)
{
#ifdef SWITCH_HAVE_ODBC
	if (profile->odbc_dsn) {
		switch_odbc_handle_destroy(&profile->master_odbc);
		return;
	}
#endif

	switch_core_db_close(profile->master_db);
	profile->master_db = NULL;
}


void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *d_sql = NULL, *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;

	if (profile->sql_queue) {
		if (sql_already_dynamic) {
			d_sql = sql;
		} else {
			d_sql = strdup(sql);
		}

		switch_assert(d_sql);
		if ((status = switch_queue_trypush(profile->sql_queue, d_sql)) == SWITCH_STATUS_SUCCESS) {
			d_sql = NULL;
		}
	} else if (sql_already_dynamic) {
		d_sql = sql;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		sofia_glue_actually_execute_sql(profile, SWITCH_FALSE, sql, profile->ireg_mutex);
	}

	switch_safe_free(d_sql);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}

void sofia_glue_actually_execute_sql(sofia_profile_t *profile, switch_bool_t master, char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
		SQLHSTMT stmt;
		if (switch_odbc_handle_exec(profile->master_odbc, sql, &stmt) != SWITCH_ODBC_SUCCESS) {
			char *err_str;
			err_str = switch_odbc_handle_get_error(profile->master_odbc, stmt);
			if (!switch_strlen_zero(err_str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, err_str);
			}
			switch_safe_free(err_str);
		}
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
#else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
	} else {
		if (master) {
			db = profile->master_db;
		} else {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				goto end;
			}
		}
		switch_core_db_persistant_execute(db, sql, 1);

		if (!master) {
			switch_core_db_close(db);
		}
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}

switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile,
											  switch_bool_t master, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_core_db_t *db;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}


	if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
		switch_odbc_handle_callback_exec(profile->master_odbc, sql, callback, pdata);
#else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
	} else {

		if (master) {
			db = profile->master_db;
		} else {
			if (!(db = switch_core_db_open_file(profile->dbname))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
				goto end;
			}
		}

		switch_core_db_exec(db, sql, callback, pdata, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
			free(errmsg);
		}

		if (!master && db) {
			switch_core_db_close(db);
		}
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
	return ret;
}

#ifdef SWITCH_HAVE_ODBC
static char *sofia_glue_execute_sql2str_odbc(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;
	SQLHSTMT stmt;
	SQLCHAR name[1024];
	SQLLEN m = 0;

	if (switch_odbc_handle_exec(profile->master_odbc, sql, &stmt) == SWITCH_ODBC_SUCCESS) {
		SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
		SQLULEN ColumnSize;
		SQLRowCount(stmt, &m);

		if (m <= 0) {
			return NULL;
		}

		if (SQLFetch(stmt) != SQL_SUCCESS) {
			return NULL;
		}

		SQLDescribeCol(stmt, 1, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
		SQLGetData(stmt, 1, SQL_C_CHAR, (SQLCHAR *) resbuf, (SQLLEN) len, NULL);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		ret = resbuf;
	}

	return ret;
}
#endif

char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	switch_core_db_t *db;
	switch_core_db_stmt_t *stmt;
	char *ret = NULL;

#ifdef SWITCH_HAVE_ODBC
	if (profile->odbc_dsn) {
		return sofia_glue_execute_sql2str_odbc(profile, mutex, sql, resbuf, len);
	}
#endif

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(db = switch_core_db_open_file(profile->dbname))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
		goto end;
	}

	if (switch_core_db_prepare(db, sql, -1, &stmt, 0)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Statement Error [%s]!\n", sql);
		goto fail;
	} else {
		int running = 1;
		int colcount;

		while (running < 5000) {
			int result = switch_core_db_step(stmt);

			if (result == SWITCH_CORE_DB_ROW) {
				if ((colcount = switch_core_db_column_count(stmt))) {
					switch_copy_string(resbuf, (char *) switch_core_db_column_text(stmt, 0), len);
					ret = resbuf;
				}
				break;
			} else if (result == SWITCH_CORE_DB_BUSY) {
				running++;
				switch_yield(1000);
				continue;
			}
			break;
		}

		switch_core_db_finalize(stmt);
	}

  fail:
	switch_core_db_close(db);

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

int sofia_glue_get_user_host(char *in, char **user, char **host)
{
	char *p, *h, *u = in;

	*user = NULL;
	*host = NULL;

	if (!strncasecmp(u, "sip:", 4)) {
		u += 4;
	}

	if ((h = strchr(u, '@'))) {
		*h++ = '\0';
	} else {
		return 0;
	}

	p = h + strlen(h) - 1;

	if (p && (*p == ':' || *p == ';' || *p == ' ')) {
		*p = '\0';
	}

	*user = u;
	*host = h;

	return 1;
}

const char *sofia_glue_strip_proto(const char *uri)
{
	char *p;

	if ((p = strchr(uri, ':'))) {
		return p+1;
	}

	return uri;
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
