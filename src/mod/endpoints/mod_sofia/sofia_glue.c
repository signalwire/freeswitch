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
 * Ken Rice <krice@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Eliot Gable <egable AT.AT broadvox.com>
 *
 *
 * sofia_glue.c -- SOFIA SIP Endpoint (code to tie sofia to freeswitch)
 *
 */
#include "mod_sofia.h"
#include <switch_stun.h>

switch_cache_db_handle_t *_sofia_glue_get_db_handle(sofia_profile_t *profile, const char *file, const char *func, int line);
#define sofia_glue_get_db_handle(_p) _sofia_glue_get_db_handle(_p, __FILE__, __SWITCH_FUNC__, __LINE__)

void sofia_glue_set_udptl_image_sdp(private_object_t *tech_pvt, switch_t38_options_t *t38_options, int insist)
{
	char buf[2048] = "";
	char max_buf[128] = "";
	char max_data[128] = "";
	const char *ip;
	uint32_t port;
	const char *family = "IP4";
	const char *username;
	const char *bit_removal_on = "a=T38FaxFillBitRemoval\n";
	const char *bit_removal_off = "";
	
	const char *mmr_on = "a=T38FaxTranscodingMMR\n";
	const char *mmr_off = "";

	const char *jbig_on = "a=T38FaxTranscodingJBIG\n";
	const char *jbig_off = "";
	const char *var;
	int broken_boolean;

	switch_assert(tech_pvt);
	switch_assert(t38_options);

	ip = t38_options->local_ip;
	port = t38_options->local_port;
	username = tech_pvt->profile->username;

	//sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);

	var = switch_channel_get_variable(tech_pvt->channel, "t38_broken_boolean");
	
	broken_boolean = switch_true(var);

	if (!ip) {
		if (!(ip = tech_pvt->adv_sdp_audio_ip)) {
			ip = tech_pvt->proxy_sdp_audio_ip;
		}
	}

	if (!ip) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO IP!\n", switch_channel_get_name(tech_pvt->channel));
		return;
	}

	if (!port) {
		if (!(port = tech_pvt->adv_sdp_audio_port)) {
			port = tech_pvt->proxy_sdp_audio_port;
		}
	}
	
	if (!port) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO PORT!\n", switch_channel_get_name(tech_pvt->channel));
		return;
	}

	if (!tech_pvt->owner_id) {
		tech_pvt->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!tech_pvt->session_id) {
		tech_pvt->session_id = tech_pvt->owner_id;
	}

	tech_pvt->session_id++;

	family = strchr(ip, ':') ? "IP6" : "IP4";


	switch_snprintf(buf, sizeof(buf),
					"v=0\n"
					"o=%s %010u %010u IN %s %s\n"
					"s=%s\n"
					"c=IN %s %s\n"
					"t=0 0\n", username, tech_pvt->owner_id, tech_pvt->session_id, family, ip, username, family, ip);

	if (t38_options->T38FaxMaxBuffer) {
		switch_snprintf(max_buf, sizeof(max_buf), "a=T38FaxMaxBuffer:%d\n", t38_options->T38FaxMaxBuffer);
	};

	if (t38_options->T38FaxMaxDatagram) {
		switch_snprintf(max_data, sizeof(max_data), "a=T38FaxMaxDatagram:%d\n", t38_options->T38FaxMaxDatagram);
	};


	

	if (broken_boolean) {
		bit_removal_on = "a=T38FaxFillBitRemoval:1\n";
		bit_removal_off = "a=T38FaxFillBitRemoval:0\n";

		mmr_on = "a=T38FaxTranscodingMMR:1\n";
		mmr_off = "a=T38FaxTranscodingMMR:0\n";

		jbig_on = "a=T38FaxTranscodingJBIG:1\n";
		jbig_off = "a=T38FaxTranscodingJBIG:0\n";

	}
	

	switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
					"m=image %d udptl t38\n"
					"a=T38FaxVersion:%d\n"
					"a=T38MaxBitRate:%d\n"
					"%s"
					"%s"
					"%s"
					"a=T38FaxRateManagement:%s\n"
					"%s"
					"%s"
					"a=T38FaxUdpEC:%s\n",
					//"a=T38VendorInfo:%s\n",
					port,
					t38_options->T38FaxVersion,
					t38_options->T38MaxBitRate,
					t38_options->T38FaxFillBitRemoval ? bit_removal_on : bit_removal_off,
					t38_options->T38FaxTranscodingMMR ? mmr_on : mmr_off,
					t38_options->T38FaxTranscodingJBIG ? jbig_on : jbig_off,
					t38_options->T38FaxRateManagement,
					max_buf,
					max_data,
					t38_options->T38FaxUdpEC
					//t38_options->T38VendorInfo ? t38_options->T38VendorInfo : "0 0 0"
					);



	if (insist) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=audio 0 RTP/AVP 19\n");
	}

	sofia_glue_tech_set_local_sdp(tech_pvt, buf, SWITCH_TRUE);


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s image media sdp:\n%s\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->local_sdp_str);


}

static void generate_m(private_object_t *tech_pvt, char *buf, size_t buflen, 
					   switch_port_t port,
					   int cur_ptime, const char *append_audio, const char *sr, int use_cng, int cng_type, switch_event_t *map, int verbose_sdp, int secure)
{
	int i = 0;
	int rate;
	int already_did[128] = { 0 };
	int ptime = 0, noptime = 0;

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
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=crypto:%s\n", tech_pvt->local_crypto_key);
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

void sofia_glue_check_dtmf_type(private_object_t *tech_pvt) 
{
	const char *val;

	if ((val = switch_channel_get_variable(tech_pvt->channel, "dtmf_type"))) {
		if (!strcasecmp(val, "rfc2833")) {
			tech_pvt->dtmf_type = DTMF_2833;
		} else if (!strcasecmp(val, "info")) {
			tech_pvt->dtmf_type = DTMF_INFO;
		} else if (!strcasecmp(val, "none")) {
			tech_pvt->dtmf_type = DTMF_NONE;
		} else {
			tech_pvt->dtmf_type = tech_pvt->profile->dtmf_type;
		}
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
						port, (!zstr(tech_pvt->local_crypto_key) && sofia_test_flag(tech_pvt, TFLAG_SECURE)) ? "S" : "");

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
	
		if (!zstr(tech_pvt->local_crypto_key) && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\n", tech_pvt->local_crypto_key);
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

			if ((!zstr(tech_pvt->local_crypto_key) && sofia_test_flag(tech_pvt, TFLAG_SECURE))) {
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
					
					if ((!zstr(tech_pvt->local_crypto_key) && sofia_test_flag(tech_pvt, TFLAG_SECURE))) {
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

		if (!tech_pvt->local_sdp_video_port) {
			sofia_glue_tech_choose_video_port(tech_pvt, 0);
		}

		if ((v_port = tech_pvt->adv_sdp_video_port)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=video %d RTP/AVP", v_port);

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

					if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
						switch_channel_test_flag(tech_pvt->channel, CF_NOVIDEO)) {
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
		switch_channel_set_variable(tech_pvt->channel, "rtp_use_codec_string", codec_string);
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
	if (tech_pvt->num_codecs && !sofia_test_flag(tech_pvt, TFLAG_VIDEO) && !switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
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

private_object_t *sofia_glue_new_pvt(switch_core_session_t *session)
{
	private_object_t *tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t));
	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->sofia_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	return tech_pvt;
}

void sofia_glue_set_name(private_object_t *tech_pvt, const char *channame)
{
	char name[256];
	char *p;

	switch_snprintf(name, sizeof(name), "sofia/%s/%s", tech_pvt->profile->name, channame);
	if ((p = strchr(name, ';'))) {
		*p = '\0';
	}
	switch_channel_set_name(tech_pvt->channel, name);
}

void sofia_glue_attach_private(switch_core_session_t *session, sofia_profile_t *profile, private_object_t *tech_pvt, const char *channame)
{

	unsigned int x;

	switch_assert(session != NULL);
	switch_assert(profile != NULL);
	switch_assert(tech_pvt != NULL);

	switch_core_session_add_stream(session, NULL);

	switch_mutex_lock(tech_pvt->flag_mutex);
	switch_mutex_lock(profile->flag_mutex);

	/* copy flags from profile to the sofia private */
	for (x = 0; x < TFLAG_MAX; x++) {
		tech_pvt->flags[x] = profile->flags[x];
	}

	tech_pvt->x_freeswitch_support_local = FREESWITCH_SUPPORT;

	tech_pvt->profile = profile;

	tech_pvt->rtpip = switch_core_session_strdup(session, profile->rtpip[profile->rtpip_next++]);
	if (profile->rtpip_next >= profile->rtpip_index) {
		profile->rtpip_next = 0;
	}

	profile->inuse++;
	switch_mutex_unlock(profile->flag_mutex);
	switch_mutex_unlock(tech_pvt->flag_mutex);

	if (tech_pvt->bte) {
		tech_pvt->recv_te = tech_pvt->te = tech_pvt->bte;
	} else if (!tech_pvt->te) {
		tech_pvt->recv_te = tech_pvt->te = profile->te;
	}

	tech_pvt->dtmf_type = tech_pvt->profile->dtmf_type;

	if (!sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
		if (tech_pvt->bcng_pt) {
			tech_pvt->cng_pt = tech_pvt->bcng_pt;
		} else if (!tech_pvt->cng_pt) {
			tech_pvt->cng_pt = profile->cng_pt;
		}
	}

	tech_pvt->session = session;
	tech_pvt->channel = switch_core_session_get_channel(session);

	if (sofia_test_pflag(profile, PFLAG_TRACK_CALLS)) {
		switch_channel_set_flag(tech_pvt->channel, CF_TRACKABLE);
	}

	sofia_glue_check_dtmf_type(tech_pvt);
	switch_channel_set_cap(tech_pvt->channel, CC_MEDIA_ACK);
	switch_channel_set_cap(tech_pvt->channel, CC_BYPASS_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_PROXY_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_JITTERBUFFER);
	switch_channel_set_cap(tech_pvt->channel, CC_FS_RTP);
	switch_channel_set_cap(tech_pvt->channel, CC_QUEUEABLE_DTMF_DELAY);

	switch_core_session_set_private(session, tech_pvt);


	if (channame) {
		sofia_glue_set_name(tech_pvt, channame);
	}

}

switch_status_t sofia_glue_ext_address_lookup(sofia_profile_t *profile, private_object_t *tech_pvt, char **ip, switch_port_t *port,
											  const char *sourceip, switch_memory_pool_t *pool)
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

	if (!strncasecmp(sourceip, "host:", 5)) {
		status = (*ip = switch_stun_host_lookup(sourceip + 5, pool)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	} else if (!strncasecmp(sourceip, "stun:", 5)) {
		char *p;

		if (!sofia_test_pflag(profile, PFLAG_STUN_ENABLED)) {
			*ip = switch_core_strdup(pool, tech_pvt->rtpip);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Trying to use STUN but its disabled!\n");
			goto out;
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

		if (zstr(stun_ip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STUN Failed! NO STUN SERVER\n");
			goto out;
		}

		for (x = 0; x < 5; x++) {
			if (sofia_test_pflag(profile, PFLAG_FUNNY_STUN) ||
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
			if (myport == *port && !strcmp(*ip, tech_pvt->rtpip)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Not Required ip and port match. [%s]:[%d]\n", *ip, *port);
				if (sofia_test_pflag(profile, PFLAG_STUN_AUTO_DISABLE)) {
					sofia_clear_pflag(profile, PFLAG_STUN_ENABLED);
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
		*ip = (char *) sourceip;
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
			if (!zstr(un->un_value)) {
				return un->un_value;
			}
		}
	}
	return NULL;
}

switch_status_t sofia_glue_tech_choose_port(private_object_t *tech_pvt, int force)
{
	char *lookup_rtpip = tech_pvt->rtpip;	/* Pointer to externally looked up address */
	switch_port_t sdp_port, rtcp_port;	/* The external port to be sent in the SDP */
	const char *use_ip = NULL;	/* The external IP to be sent in the SDP */

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || tech_pvt->adv_sdp_audio_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Release the local sdp port */
	if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->rtpip, tech_pvt->local_sdp_audio_port);
	}

	/* Request a local port from the core's allocator */
	if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(tech_pvt->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->local_sdp_audio_ip = tech_pvt->rtpip;

	sdp_port = tech_pvt->local_sdp_audio_port;

	/* Check if NAT is detected  */
	if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
		/* Yes, map the port through switch_nat */
		switch_nat_add_mapping(tech_pvt->local_sdp_audio_port, SWITCH_NAT_UDP, &sdp_port, SWITCH_FALSE);
		switch_nat_add_mapping(tech_pvt->local_sdp_audio_port + 1, SWITCH_NAT_UDP, &rtcp_port, SWITCH_FALSE);

		/* Find an IP address to use */
		if (!(use_ip = switch_channel_get_variable(tech_pvt->channel, "rtp_adv_audio_ip"))
			&& !zstr(tech_pvt->profile->extrtpip)) {
			use_ip = tech_pvt->profile->extrtpip;
		}

		if (use_ip) {
			if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &lookup_rtpip, &sdp_port,
											  use_ip, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				/* Address lookup was required and fail (external ip was "host:..." or "stun:...") */
				return SWITCH_STATUS_FALSE;
			} else {
				/* Address properly resolved, use it as external ip */
				use_ip = lookup_rtpip;
			}
		} else {
			/* No external ip found, use the profile's rtp ip */
			use_ip = tech_pvt->rtpip;
		}
	} else {
		/* No NAT traversal required, use the profile's rtp ip */
		use_ip = tech_pvt->rtpip;
	}

	tech_pvt->adv_sdp_audio_port = sdp_port;
	tech_pvt->adv_sdp_audio_ip = tech_pvt->extrtpip = switch_core_session_strdup(tech_pvt->session, use_ip);

	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, tech_pvt->local_sdp_audio_ip);
	switch_channel_set_variable_printf(tech_pvt->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", sdp_port);
	switch_channel_set_variable(tech_pvt->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t sofia_glue_tech_choose_video_port(private_object_t *tech_pvt, int force)
{
	char *lookup_rtpip = tech_pvt->rtpip;	/* Pointer to externally looked up address */
	switch_port_t sdp_port;		/* The external port to be sent in the SDP */
	const char *use_ip = NULL;	/* The external IP to be sent in the SDP */

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA) || tech_pvt->adv_sdp_video_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Release the local sdp port */
	if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->rtpip, tech_pvt->local_sdp_video_port);
	}

	/* Request a local port from the core's allocator */
	if (!(tech_pvt->local_sdp_video_port = switch_rtp_request_port(tech_pvt->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
		return SWITCH_STATUS_FALSE;
	}

	sdp_port = tech_pvt->local_sdp_video_port;

	/* Check if NAT is detected  */
	if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
		/* Yes, map the port through switch_nat */
		switch_nat_add_mapping(tech_pvt->local_sdp_video_port, SWITCH_NAT_UDP, &sdp_port, SWITCH_FALSE);

		/* Find an IP address to use */
		if (!(use_ip = switch_channel_get_variable(tech_pvt->channel, "rtp_adv_video_ip"))
			&& !zstr(tech_pvt->profile->extrtpip)) {
			use_ip = tech_pvt->profile->extrtpip;
		}

		if (use_ip) {
			if (sofia_glue_ext_address_lookup(tech_pvt->profile, tech_pvt, &lookup_rtpip, &sdp_port,
											  use_ip, switch_core_session_get_pool(tech_pvt->session)) != SWITCH_STATUS_SUCCESS) {
				/* Address lookup was required and fail (external ip was "host:..." or "stun:...") */
				return SWITCH_STATUS_FALSE;
			} else {
				/* Address properly resolved, use it as external ip */
				use_ip = lookup_rtpip;
			}
		} else {
			/* No external ip found, use the profile's rtp ip */
			use_ip = tech_pvt->rtpip;
		}
	} else {
		/* No NAT traversal required, use the profile's rtp ip */
		use_ip = tech_pvt->rtpip;
	}

	tech_pvt->adv_sdp_video_port = sdp_port;
	switch_channel_set_variable(tech_pvt->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, tech_pvt->adv_sdp_audio_ip);
	switch_channel_set_variable_printf(tech_pvt->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, "%d", sdp_port);

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

enum tport_tls_verify_policy sofia_glue_str2tls_verify_policy(const char * str){
	char *ptr_next;
	int len;
	enum tport_tls_verify_policy ret;
	char *ptr_cur = (char *) str;
	ret = TPTLS_VERIFY_NONE;

	while (ptr_cur) {
		if ((ptr_next = strchr(ptr_cur, '|'))) {
			len = ptr_next++ - ptr_cur;
		} else {
			len = strlen(ptr_cur);
		}
		if (!strncasecmp(ptr_cur, "in",len)) {
			ret |= TPTLS_VERIFY_IN;
		} else if (!strncasecmp(ptr_cur, "out",len)) {
			ret |= TPTLS_VERIFY_OUT;
		} else if (!strncasecmp(ptr_cur, "all",len)) {
			ret |= TPTLS_VERIFY_ALL;
		} else if (!strncasecmp(ptr_cur, "subjects_in",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_IN;
		} else if (!strncasecmp(ptr_cur, "subjects_out",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_OUT;
		} else if (!strncasecmp(ptr_cur, "subjects_all",len)) {
			ret |= TPTLS_VERIFY_SUBJECTS_ALL;
		}
		ptr_cur = ptr_next;
	}
	return ret;
}

char *sofia_glue_find_parameter_value(switch_core_session_t *session, const char *str, const char *param)
{
	const char *param_ptr;
	char *param_value;
	char *tmp;
	switch_size_t param_len;

	if (zstr(str) || zstr(param) || !session) return NULL;

	if (end_of(param) != '=') {
		param = switch_core_session_sprintf(session, "%s=", param);
		if (zstr(param)) return NULL;
	}

	param_len = strlen(param);
	param_ptr = sofia_glue_find_parameter(str, param);

	if (zstr(param_ptr)) return NULL;

	param_value = switch_core_session_strdup(session, param_ptr + param_len);

	if (zstr(param_value)) return NULL;

	if ((tmp = strchr(param_value, ';'))) *tmp = '\0';

	return param_value;
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

sofia_transport_t sofia_glue_via2transport(const sip_via_t * via)
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

char *sofia_glue_create_external_via(switch_core_session_t *session, sofia_profile_t *profile, sofia_transport_t transport)
{
	return sofia_glue_create_via(session, profile->extsipip, (sofia_glue_transport_has_tls(transport))
								 ? profile->tls_sip_port : profile->extsipport, transport);
}

char *sofia_glue_create_via(switch_core_session_t *session, const char *ip, switch_port_t port, sofia_transport_t transport)
{
	if (port && port != 5060) {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);
		} else {
			return switch_mprintf("SIP/2.0/%s %s:%d;rport", sofia_glue_transport2str(transport), ip, port);
		}
	} else {
		if (session) {
			return switch_core_session_sprintf(session, "SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		} else {
			return switch_mprintf("SIP/2.0/%s %s;rport", sofia_glue_transport2str(transport), ip);
		}
	}
}

char *sofia_glue_strip_uri(const char *str)
{
	char *p;
	char *r;

	if ((p = strchr(str, '<'))) {
		p++;
		r = strdup(p);
		if ((p = strchr(r, '>'))) {
			*p = '\0';
		}
	} else {
		r = strdup(str);
	}

	return r;
}

int sofia_glue_check_nat(sofia_profile_t *profile, const char *network_ip)
{
	switch_assert(network_ip);

	return (profile->extsipip && 
			!switch_check_network_list_ip(network_ip, "loopback.auto") && 
			!switch_check_network_list_ip(network_ip, profile->local_network));
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

void sofia_glue_get_addr(msg_t *msg, char *buf, size_t buflen, int *port)
{
	su_addrinfo_t *addrinfo = msg_addrinfo(msg);

	if (buf) {
		get_addr(buf, buflen, addrinfo->ai_addr, addrinfo->ai_addrlen);
	}

	if (port) {
		*port = get_port(addrinfo->ai_addr);
	}
}

char *sofia_overcome_sip_uri_weakness(switch_core_session_t *session, const char *uri, const sofia_transport_t transport, switch_bool_t uri_only,
									  const char *params, const char *invite_tel_params)
{
	char *stripped = switch_core_session_strdup(session, uri);
	char *new_uri = NULL;
	char *p;


	stripped = sofia_glue_get_url_from_contact(stripped, 0);

	/* remove our params so we don't make any whiny moronic device piss it's pants and forget who it is for a half-hour */
	if ((p = (char *) switch_stristr(";fs_", stripped))) {
		*p = '\0';
	}

	if (transport && transport != SOFIA_TRANSPORT_UDP) {

		if (switch_stristr("port=", stripped)) {
			new_uri = switch_core_session_sprintf(session, "%s%s%s", uri_only ? "" : "<", stripped, uri_only ? "" : ">");
		} else {

			if (strchr(stripped, ';')) {
				if (params) {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s;%s%s",
														  uri_only ? "" : "<", stripped, sofia_glue_transport2str(transport), params, uri_only ? "" : ">");
				} else {
					new_uri = switch_core_session_sprintf(session, "%s%s;transport=%s%s",
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



	if (!zstr(invite_tel_params)) {
		char *lhs, *rhs = strchr(new_uri, '@');

		if (!zstr(rhs)) {
			*rhs++ = '\0';
			lhs = new_uri;
			new_uri = switch_core_session_sprintf(session, "%s;%s@%s", lhs, invite_tel_params, rhs);
		}
	}
	
	return new_uri;
}

#define RA_PTR_LEN 512
switch_status_t sofia_glue_tech_proxy_remote_addr(private_object_t *tech_pvt, const char *sdp_str)
{
	const char *err;
	char rip[RA_PTR_LEN] = "";
	char rp[RA_PTR_LEN] = "";
	char rvp[RA_PTR_LEN] = "";
	char *p, *ip_ptr = NULL, *port_ptr = NULL, *vid_port_ptr = NULL, *pe;
	int x;
	const char *val;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	if (zstr(sdp_str)) {
		sdp_str = tech_pvt->remote_sdp_str;
	}

	if (zstr(sdp_str)) {
		goto end;
	}

	if ((p = (char *) switch_stristr("c=IN IP4 ", sdp_str)) || (p = (char *) switch_stristr("c=IN IP6 ", sdp_str))) {
		ip_ptr = p + 9;
	}

	if ((p = (char *) switch_stristr("m=audio ", sdp_str))) {
		port_ptr = p + 8;
	}

	if ((p = (char *) switch_stristr("m=image ", sdp_str))) {
		char *tmp = p + 8;

		if (tmp && atoi(tmp)) {
			port_ptr = tmp;
		}
	}

	if ((p = (char *) switch_stristr("m=video ", sdp_str))) {
		vid_port_ptr = p + 8;
	}

	if (!(ip_ptr && port_ptr)) {
		goto end;
	}

	p = ip_ptr;
	pe = p + strlen(p);
	x = 0;
	while (x < sizeof(rip) - 1 && p && *p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ':' || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
		rip[x++] = *p;
		p++;
		if (p >= pe) {
			goto end;
		}
	}

	p = port_ptr;
	x = 0;
	while (x < sizeof(rp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
		rp[x++] = *p;
		p++;
		if (p >= pe) {
			goto end;
		}
	}

	p = vid_port_ptr;
	x = 0;
	while (x < sizeof(rvp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
		rvp[x++] = *p;
		p++;
		if (p >= pe) {
			goto end;
		}
	}

	if (!(*rip && *rp)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "invalid SDP\n");
		goto end;
	}

	tech_pvt->remote_sdp_audio_ip = switch_core_session_strdup(tech_pvt->session, rip);
	tech_pvt->remote_sdp_audio_port = (switch_port_t) atoi(rp);

	if (*rvp) {
		tech_pvt->remote_sdp_video_ip = switch_core_session_strdup(tech_pvt->session, rip);
		tech_pvt->remote_sdp_video_port = (switch_port_t) atoi(rvp);
	}

	if (tech_pvt->remote_sdp_video_ip && tech_pvt->remote_sdp_video_port) {
		if (!strcmp(tech_pvt->remote_sdp_video_ip, rip) && atoi(rvp) == tech_pvt->remote_sdp_video_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Remote video address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
		} else {
			sofia_set_flag_locked(tech_pvt, TFLAG_VIDEO);
			switch_channel_set_flag(tech_pvt->channel, CF_VIDEO);
			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = 0;

				if ((rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_video_rtcp_port"))) {
					remote_rtcp_port = (switch_port_t)atoi(rport);
				}


				if (switch_rtp_set_remote_address(tech_pvt->video_rtp_session, tech_pvt->remote_sdp_video_ip,
												  tech_pvt->remote_sdp_video_port, remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  tech_pvt->remote_sdp_video_ip, tech_pvt->remote_sdp_video_port);
					if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
						!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(tech_pvt->video_rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}
					if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFIX_TIMING)) {
						tech_pvt->check_frames = 0;
					}
				}
			}
		}
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		char *remote_host = switch_rtp_get_remote_host(tech_pvt->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(tech_pvt->rtp_session);
		const char *rport = NULL;
		switch_port_t remote_rtcp_port = 0;

		if (remote_host && remote_port && !strcmp(remote_host, tech_pvt->remote_sdp_audio_ip) && remote_port == tech_pvt->remote_sdp_audio_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Remote address:port [%s:%d] has not changed.\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			switch_goto_status(SWITCH_STATUS_BREAK, end);
		}

		if ((rport = switch_channel_get_variable(tech_pvt->channel, "sip_remote_audio_rtcp_port"))) {
			remote_rtcp_port = (switch_port_t)atoi(rport);
		}


		if (switch_rtp_set_remote_address(tech_pvt->rtp_session, tech_pvt->remote_sdp_audio_ip,
										  tech_pvt->remote_sdp_audio_port, remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
			status = SWITCH_STATUS_GENERR;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  tech_pvt->remote_sdp_audio_ip, tech_pvt->remote_sdp_audio_port);
			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(tech_pvt->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
			if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFIX_TIMING)) {
				tech_pvt->check_frames = 0;
			}
			status = SWITCH_STATUS_SUCCESS;
		}
	}

 end:

	return status;
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
					sofia_glue_activate_rtp(tech_pvt, 0);
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
	switch_mutex_lock(tech_pvt->flag_mutex);
	tech_pvt->local_sdp_str = dup ? switch_core_session_strdup(tech_pvt->session, sdp_str) : (char *) sdp_str;
	switch_channel_set_variable(tech_pvt->channel, "sip_local_sdp_str", tech_pvt->local_sdp_str);
	switch_mutex_unlock(tech_pvt->flag_mutex);
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


char *sofia_glue_get_extra_headers(switch_channel_t *channel, const char *prefix)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hi = NULL;
	const char *exclude_regex = NULL;
	switch_regex_t *re = NULL;
	int ovector[30] = {0};
	int proceed;

	exclude_regex = switch_channel_get_variable(channel, "exclude_outgoing_extra_header");
	SWITCH_STANDARD_STREAM(stream);
	if ((hi = switch_channel_variable_first(channel))) {
		for (; hi; hi = hi->next) {
			const char *name = (char *) hi->name;
			char *value = (char *) hi->value;
			
			if (!strcasecmp(name, "sip_geolocation")) {
				stream.write_function(&stream, "Geolocation: %s\r\n", value);
			}

			if (!strncasecmp(name, prefix, strlen(prefix))) {
				if ( !exclude_regex || !(proceed = switch_regex_perform(name, exclude_regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
					const char *hname = name + strlen(prefix);
					stream.write_function(&stream, "%s: %s\r\n", hname, value);
					switch_regex_safe_free(re);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ignoring Extra Header [%s] , matches exclude_outgoing_extra_header [%s]\n", name, exclude_regex);
				}
			}
		}
		switch_channel_variable_last(channel);
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
}

void sofia_glue_set_extra_headers(switch_core_session_t *session, sip_t const *sip, const char *prefix)
{
	sip_unknown_t *un;
	char name[512] = "";
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *pstr;

	
	if (!sip || !channel) {
		return;
	}

	for (un = sip->sip_unknown; un; un = un->un_next) {
		if ((!strncasecmp(un->un_name, "X-", 2) && strncasecmp(un->un_name, "X-FS-", 5)) || !strncasecmp(un->un_name, "P-", 2)) {
			if (!zstr(un->un_value)) {
				switch_snprintf(name, sizeof(name), "%s%s", prefix, un->un_name);
				switch_channel_set_variable(channel, name, un->un_value);
			}
		}
	}

	pstr = switch_core_session_sprintf(session, "execute_on_%sprefix", prefix);
	switch_channel_execute_on(channel, pstr);
	switch_channel_api_on(channel, pstr);

	switch_channel_execute_on(channel, "execute_on_sip_extra_headers");
	switch_channel_api_on(channel, "api_on_sip_extra_headers");
}

char *sofia_glue_get_extra_headers_from_event(switch_event_t *event, const char *prefix)
{
	char *extra_headers = NULL;
	switch_stream_handle_t stream = { 0 };
	switch_event_header_t *hp;

	SWITCH_STANDARD_STREAM(stream);
	for (hp = event->headers; hp; hp = hp->next) {
		if (!zstr(hp->name) && !zstr(hp->value) && !strncasecmp(hp->name, prefix, strlen(prefix))) {
			char *name = strdup(hp->name);
			const char *hname = name + strlen(prefix);
			stream.write_function(&stream, "%s: %s\r\n", hname, (char *)hp->value);
			free(name);
		}
	}

	if (!zstr((char *) stream.data)) {
		extra_headers = stream.data;
	} else {
		switch_safe_free(stream.data);
	}

	return extra_headers;
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
	char *extra_headers = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t session_timeout = 0;
	const char *val;
	const char *rep;
	const char *call_id = NULL;
	char *route = NULL;
	char *route_uri = NULL;
	sofia_destination_t *dst = NULL;
	sofia_cid_type_t cid_type = tech_pvt->profile->cid_type;
	sip_cseq_t *cseq = NULL;
	const char *invite_record_route = switch_channel_get_variable(tech_pvt->channel, "sip_invite_record_route");
	const char *invite_route_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_route_uri");
	const char *invite_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_from");
	const char *invite_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_invite_full_to");
	const char *handle_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_handle_full_from");
	const char *handle_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_handle_full_to");
	const char *force_full_from = switch_channel_get_variable(tech_pvt->channel, "sip_force_full_from");
	const char *force_full_to = switch_channel_get_variable(tech_pvt->channel, "sip_force_full_to");
	char *mp = NULL, *mp_type = NULL;
	char *record_route = NULL;
	const char *recover_via = NULL;
	int require_timer = 1;

	if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
		const char *recover_contact = switch_channel_get_variable(tech_pvt->channel, "sip_recover_contact");
		recover_via = switch_channel_get_variable(tech_pvt->channel, "sip_recover_via");

		if (!zstr(invite_record_route)) {
			record_route = switch_core_session_sprintf(session, "Record-Route: %s", invite_record_route);
		}
		
		if (recover_contact) {
			char *tmp = switch_core_session_strdup(session, recover_contact);
			tech_pvt->redirected = sofia_glue_get_url_from_contact(tmp, 0);
		}
	}


	if ((rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER))) {
		switch_channel_set_variable(channel, SOFIA_REPLACES_HEADER, NULL);
	}

	switch_assert(tech_pvt != NULL);

	sofia_clear_flag_locked(tech_pvt, TFLAG_SDP);

	caller_profile = switch_channel_get_caller_profile(channel);

	if (!caller_profile) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return SWITCH_STATUS_FALSE;
	}


	if ((val = switch_channel_get_variable_dup(channel, "sip_require_timer", SWITCH_FALSE, -1)) && switch_false(val)) {
		require_timer = 0;
	}


	cid_name = caller_profile->caller_id_name;
	cid_num = caller_profile->caller_id_number;
	sofia_glue_tech_prepare_codecs(tech_pvt);
	sofia_glue_check_video_codecs(tech_pvt);
	check_decode(cid_name, session);
	check_decode(cid_num, session);


	if ((alertbuf = switch_channel_get_variable(channel, "alert_info"))) {
		alert_info = switch_core_session_sprintf(tech_pvt->session, "Alert-Info: %s", alertbuf);
	}

	max_forwards = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);

	if ((status = sofia_glue_tech_choose_port(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Port Error!\n");
		return status;
	}

	if (!switch_channel_get_private(tech_pvt->channel, "t38_options") || zstr(tech_pvt->local_sdp_str)) {
		sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 0);
	}

	sofia_set_flag_locked(tech_pvt, TFLAG_READY);

	if (!tech_pvt->nh) {
		char *d_url = NULL, *url = NULL, *url_str = NULL;
		sofia_private_t *sofia_private;
		char *invite_contact = NULL, *to_str, *use_from_str, *from_str;
		const char *t_var;
		char *rpid_domain = NULL, *p;
		const char *priv = "off";
		const char *screen = "no";
		const char *invite_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_params");
		const char *invite_to_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_params");
		const char *invite_tel_params = switch_channel_get_variable(switch_core_session_get_channel(session), "sip_invite_tel_params");
		const char *invite_to_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_to_uri");
		const char *invite_from_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_from_uri");
		const char *invite_contact_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_contact_params");
		const char *invite_from_params = switch_channel_get_variable(tech_pvt->channel, "sip_invite_from_params");
		const char *from_var = switch_channel_get_variable(tech_pvt->channel, "sip_from_uri");
		const char *from_display = switch_channel_get_variable(tech_pvt->channel, "sip_from_display");
		const char *invite_req_uri = switch_channel_get_variable(tech_pvt->channel, "sip_invite_req_uri");
		const char *invite_domain = switch_channel_get_variable(tech_pvt->channel, "sip_invite_domain");
		
		const char *use_name, *use_number;

		if (zstr(tech_pvt->dest)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "URL Error!\n");
			return SWITCH_STATUS_FALSE;
		}

		if ((d_url = sofia_glue_get_url_from_contact(tech_pvt->dest, 1))) {
			url = d_url;
		} else {
			url = tech_pvt->dest;
		}

		url_str = url;

		if (!tech_pvt->from_str) {
			const char *sipip;
			const char *format;

			sipip = tech_pvt->profile->sipip;

			if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
				sipip = tech_pvt->profile->extsipip;
			}

			format = strchr(sipip, ':') ? "\"%s\" <sip:%s%s[%s]>" : "\"%s\" <sip:%s%s%s>";

			if (!zstr(invite_domain)) {
				sipip = invite_domain;
			}

			tech_pvt->from_str = switch_core_session_sprintf(tech_pvt->session, format, cid_name, cid_num, !zstr(cid_num) ? "@" : "", sipip);
		}

		if (from_var) {
			if (strncasecmp(from_var, "sip:", 4) || strncasecmp(from_var, "sips:", 5)) {
				use_from_str = switch_core_session_strdup(tech_pvt->session, from_var);
			} else {
				use_from_str = switch_core_session_sprintf(tech_pvt->session, "sip:%s", from_var);
			}
		} else if (!zstr(tech_pvt->gateway_from_str)) {
			use_from_str = tech_pvt->gateway_from_str;
		} else {
			use_from_str = tech_pvt->from_str;
		}

		if (!zstr(tech_pvt->gateway_from_str)) {
			rpid_domain = switch_core_session_strdup(session, tech_pvt->gateway_from_str);
		} else if (!zstr(tech_pvt->from_str)) {
			rpid_domain = switch_core_session_strdup(session, use_from_str);
		}

		sofia_glue_get_url_from_contact(rpid_domain, 0);
		if ((rpid_domain = strrchr(rpid_domain, '@'))) {
			rpid_domain++;
			if ((p = strchr(rpid_domain, ';'))) {
				*p = '\0';
			}
		}

		if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTO_NAT)) {
			if (!zstr(tech_pvt->remote_ip) && !zstr(tech_pvt->profile->extsipip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
				rpid_domain = tech_pvt->profile->extsipip;
			} else {
				rpid_domain = tech_pvt->profile->sipip;
			}
		}

		if (!zstr(invite_domain)) {
			rpid_domain = (char *)invite_domain;
		}

		if (zstr(rpid_domain)) {
			rpid_domain = "cluecon.com";
		}

		/*
		 * Ignore transport chanvar and uri parameter for gateway connections
		 * since all of them have been already taken care of in mod_sofia.c:sofia_outgoing_channel()
		 */
		if (tech_pvt->transport == SOFIA_TRANSPORT_UNKNOWN && zstr(tech_pvt->gateway_name)) {
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

		if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
			tech_pvt->user_via = sofia_glue_create_external_via(session, tech_pvt->profile, tech_pvt->transport);
		}

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_TLS) && sofia_glue_transport_has_tls(tech_pvt->transport)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "TLS not supported by profile\n");
			return SWITCH_STATUS_FALSE;
		}

		if (zstr(tech_pvt->invite_contact)) {
			const char *contact;
			if ((contact = switch_channel_get_variable(channel, "sip_contact_user"))) {
				char *ip_addr = tech_pvt->profile->sipip;
				char *ipv6;

				if ( !zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip ) ) {
					ip_addr = tech_pvt->profile->extsipip;
				}

				ipv6 = strchr(ip_addr, ':');

				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact,
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", tech_pvt->profile->tls_sip_port);
				} else {
					tech_pvt->invite_contact = switch_core_session_sprintf(session, "sip:%s@%s%s%s:%d", contact,
																		   ipv6 ? "[" : "", ip_addr, ipv6 ? "]" : "", tech_pvt->profile->extsipport);
				}
			} else {
				if (sofia_glue_transport_has_tls(tech_pvt->transport)) {
					tech_pvt->invite_contact = tech_pvt->profile->tls_url;
				} else {
					if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
						tech_pvt->invite_contact = tech_pvt->profile->public_url;
					} else {
						tech_pvt->invite_contact = tech_pvt->profile->url;
					}
				}
			}
		}

		url_str = sofia_overcome_sip_uri_weakness(session, url, tech_pvt->transport, SWITCH_TRUE, invite_params, invite_tel_params);
		invite_contact = sofia_overcome_sip_uri_weakness(session, tech_pvt->invite_contact, tech_pvt->transport, SWITCH_FALSE, invite_contact_params, NULL);
		from_str = sofia_overcome_sip_uri_weakness(session, invite_from_uri ? invite_from_uri : use_from_str, 0, SWITCH_TRUE, invite_from_params, NULL);
		to_str = sofia_overcome_sip_uri_weakness(session, invite_to_uri ? invite_to_uri : tech_pvt->dest_to, 0, SWITCH_FALSE, invite_to_params, NULL);

		switch_channel_set_variable(channel, "sip_outgoing_contact_uri", invite_contact);


		/*
		  Does the "genius" who wanted SIP to be "text-based" so it was "easier to read" even use it now,
		  or did he just suggest it to make our lives miserable?
		*/
		use_from_str = from_str;

		if (!switch_stristr("sip:", use_from_str)) {
			use_from_str = switch_core_session_sprintf(session, "sip:%s", use_from_str);
		}

		if (!from_display && !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
			from_str = switch_core_session_sprintf(session, "<%s>", use_from_str);
		} else {
			char *name = switch_core_session_strdup(session, from_display ? from_display : tech_pvt->caller_profile->caller_id_name);
			check_decode(name, session);
			from_str = switch_core_session_sprintf(session, "\"%s\" <%s>", name, use_from_str);
		}

		if (!(call_id = switch_channel_get_variable(channel, "sip_invite_call_id"))) {
			if (sofia_test_pflag(tech_pvt->profile, PFLAG_UUID_AS_CALLID)) {
				call_id = switch_core_session_get_uuid(session);
			}
		}

		if (handle_full_from) {
			from_str = (char *) handle_full_from;
		}

		if (handle_full_to) {
			to_str = (char *) handle_full_to;
		}


		if (force_full_from) {
			from_str = (char *) force_full_from;
		}

		if (force_full_to) {
			to_str = (char *) force_full_to;
		}


		if (invite_req_uri) {
			url_str = (char *) invite_req_uri;
		}

		if (url_str) {
			char *s = NULL;
			if (!strncasecmp(url_str, "sip:", 4)) {
				s = url_str + 4;
			}
			if (!strncasecmp(url_str, "sips:", 5)) {
				s = url_str + 5;
			}

			/* tel: patch from jaybinks, added by MC
               It compiles but I don't have a way to test it
			*/
			if (!strncasecmp(url_str, "tel:", 4)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session),
								  SWITCH_LOG_ERROR, "URL Error! tel: uri's not supported at this time\n");
				return SWITCH_STATUS_FALSE;
			}
			if (!s) {
				s = url_str;
			}
			switch_channel_set_variable(channel, "sip_req_uri", s);
		}
		
		switch_channel_set_variable(channel, "sip_to_host", sofia_glue_get_host(to_str, switch_core_session_get_pool(session)));
		switch_channel_set_variable(channel, "sip_from_host", sofia_glue_get_host(from_str, switch_core_session_get_pool(session)));


		if (!(tech_pvt->nh = nua_handle(tech_pvt->profile->nua, NULL,
										NUTAG_URL(url_str),
										TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
										TAG_IF(!zstr(record_route), SIPTAG_HEADER_STR(record_route)),
										SIPTAG_TO_STR(to_str), SIPTAG_FROM_STR(from_str), SIPTAG_CONTACT_STR(invite_contact), TAG_END()))) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, 
							  "Error creating HANDLE!\nurl_str=[%s]\ncall_id=[%s]\nto_str=[%s]\nfrom_str=[%s]\ninvite_contact=[%s]\n",
							  url_str,
							  call_id ? call_id : "N/A",
							  to_str,
							  from_str,
							  invite_contact);
			
			switch_safe_free(d_url);
			return SWITCH_STATUS_FALSE;
		}

		if (tech_pvt->dest && (strstr(tech_pvt->dest, ";fs_nat") || strstr(tech_pvt->dest, ";received")
							   || ((val = switch_channel_get_variable(channel, "sip_sticky_contact")) && switch_true(val)))) {
			sofia_set_flag(tech_pvt, TFLAG_NAT);
			tech_pvt->record_route = switch_core_session_strdup(tech_pvt->session, url_str);
			route_uri = tech_pvt->record_route;
			session_timeout = SOFIA_NAT_SESSION_TIMEOUT;
			switch_channel_set_variable(channel, "sip_nat_detected", "true");
		}

		if ((val = switch_channel_get_variable(channel, "sip_cid_type"))) {
			cid_type = sofia_cid_name2type(val);
		}

		if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING) && switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if (zstr((use_name = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_name"))) &&
				zstr((use_name = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_name")))) {
				if (!(use_name = switch_channel_get_variable(tech_pvt->channel, "sip_to_display"))) {
					use_name = switch_channel_get_variable(tech_pvt->channel, "sip_to_user");
				}
			}

			if (zstr((use_number = switch_channel_get_variable(tech_pvt->channel, "effective_callee_id_number"))) &&
				zstr((use_number = switch_channel_get_variable(tech_pvt->channel, "sip_callee_id_number")))) {
				use_number = switch_channel_get_variable(tech_pvt->channel, "sip_to_user");
			}

			if (zstr(use_name) && zstr(use_name = tech_pvt->caller_profile->callee_id_name)) {
				use_name = tech_pvt->caller_profile->caller_id_name;
			}

			if (zstr(use_number) && zstr(use_number = tech_pvt->caller_profile->callee_id_number)) {
				use_number = tech_pvt->caller_profile->caller_id_number;
			}
		} else {
			use_name = tech_pvt->caller_profile->caller_id_name;
			use_number = tech_pvt->caller_profile->caller_id_number;
		}

		check_decode(use_name, session);

		switch (cid_type) {
		case CID_TYPE_PID:
			if (switch_test_flag(caller_profile, SWITCH_CPF_SCREEN)) {
				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->asserted_id = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>",
																		use_number, rpid_domain);
				} else {
					tech_pvt->asserted_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																		use_name, use_number, rpid_domain);
				}
			} else {
				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->preferred_id = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>",
																		 tech_pvt->caller_profile->caller_id_number, rpid_domain);
				} else {
					tech_pvt->preferred_id = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>",
																		 tech_pvt->caller_profile->caller_id_name,
																		 tech_pvt->caller_profile->caller_id_number, rpid_domain);
				}
			}

			if (switch_test_flag(caller_profile, SWITCH_CPF_HIDE_NUMBER)) {
				tech_pvt->privacy = "id";
			} else {
				tech_pvt->privacy = "none";
			}

			break;
		case CID_TYPE_RPID:
			{
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

				if (zstr(tech_pvt->caller_profile->caller_id_name) || !strcasecmp(tech_pvt->caller_profile->caller_id_name, "_undef_")) {
					tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "<sip:%s@%s>;party=calling;screen=%s;privacy=%s",
																 use_number, rpid_domain, screen, priv);
				} else {
					tech_pvt->rpid = switch_core_session_sprintf(tech_pvt->session, "\"%s\"<sip:%s@%s>;party=calling;screen=%s;privacy=%s",
																 use_name, use_number, rpid_domain, screen, priv);
				}
			}
			break;
		default:
			break;
		}


		switch_safe_free(d_url);

		if (!(sofia_private = su_alloc(tech_pvt->nh->nh_home, sizeof(*sofia_private)))) {
			abort();
		}

		memset(sofia_private, 0, sizeof(*sofia_private));
		sofia_private->is_call = 2;
		sofia_private->is_static++;

		if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
			sofia_private->is_call++;
		}

		tech_pvt->sofia_private = sofia_private;
		switch_copy_string(tech_pvt->sofia_private->uuid, switch_core_session_get_uuid(session), sizeof(tech_pvt->sofia_private->uuid));
		nua_handle_bind(tech_pvt->nh, tech_pvt->sofia_private);
	}

	if (tech_pvt->e_dest && sofia_test_pflag(tech_pvt->profile, PFLAG_IN_DIALOG_CHAT)) {
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

	holdstr = sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD) ? "*" : "";

	if (!switch_channel_get_variable(channel, "sofia_profile_name")) {
		switch_channel_set_variable(channel, "sofia_profile_name", tech_pvt->profile->name);
		switch_channel_set_variable(channel, "recovery_profile_name", tech_pvt->profile->name);
	}

	extra_headers = sofia_glue_get_extra_headers(channel, SOFIA_SIP_HEADER_PREFIX);

	session_timeout = tech_pvt->profile->session_timeout;

	if ((val = switch_channel_get_variable(channel, SOFIA_SESSION_TIMEOUT))) {
		int v_session_timeout = atoi(val);
		if (v_session_timeout >= 0) {
			session_timeout = v_session_timeout;
		}
	}

	if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		if (switch_rtp_ready(tech_pvt->rtp_session)) {
			sofia_glue_tech_proxy_remote_addr(tech_pvt, NULL);
		}
		sofia_glue_tech_patch_sdp(tech_pvt);
	}

	if (!zstr(tech_pvt->dest)) {
		dst = sofia_glue_get_destination(tech_pvt->dest);

		if (dst->route_uri) {
			route_uri = sofia_overcome_sip_uri_weakness(tech_pvt->session, dst->route_uri, tech_pvt->transport, SWITCH_TRUE, NULL, NULL);
		}

		if (dst->route) {
			route = dst->route;
		}
	}

	if ((val = switch_channel_get_variable(channel, "sip_route_uri"))) {
		route_uri = switch_core_session_strdup(session, val);
		route = NULL;
	}

	if (route_uri) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "%s Setting proxy route to %s\n", route_uri,
						  switch_channel_get_name(channel));
		tech_pvt->route_uri = switch_core_session_strdup(tech_pvt->session, route_uri);
	}


	if ((val = switch_channel_get_variable(tech_pvt->channel, "sip_invite_cseq"))) {
		uint32_t callsequence = (uint32_t) strtoul(val, NULL, 10);
		cseq = sip_cseq_create(tech_pvt->nh->nh_home, callsequence, SIP_METHOD_INVITE);
	}


	switch_channel_clear_flag(channel, CF_MEDIA_ACK);

	if (handle_full_from) {
		tech_pvt->nh->nh_has_invite = 1;
	}

	if ((mp = sofia_glue_get_multipart(session, SOFIA_MULTIPART_PREFIX, tech_pvt->local_sdp_str, &mp_type))) {
		sofia_clear_flag(tech_pvt, TFLAG_ENABLE_SOA);
	}

	if ((tech_pvt->session_timeout = session_timeout)) {
		tech_pvt->session_refresher = switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? nua_local_refresher : nua_remote_refresher;
	} else {
		tech_pvt->session_refresher = nua_no_refresher;
	}

	if (tech_pvt->local_sdp_str) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
						  "Local SDP:\n%s\n", tech_pvt->local_sdp_str);
	}

	if (sofia_use_soa(tech_pvt)) {
		nua_invite(tech_pvt->nh,
				   NUTAG_AUTOANSWER(0),
				   //TAG_IF(zstr(tech_pvt->local_sdp_str), NUTAG_AUTOACK(0)),
				   //TAG_IF(!zstr(tech_pvt->local_sdp_str), NUTAG_AUTOACK(1)),
				   // The code above is breaking things...... grrr WE need this because we handle our own acks and there are 3pcc cases in there too
				   NUTAG_AUTOACK(0),
				   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
				   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
				   TAG_IF(sofia_test_flag(tech_pvt, TFLAG_RECOVERED), NUTAG_INVITE_TIMER(UINT_MAX)),
				   TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				   TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				   TAG_IF(tech_pvt->redirected, NUTAG_URL(tech_pvt->redirected)),
				   TAG_IF(!zstr(recover_via), SIPTAG_VIA_STR(recover_via)),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   TAG_IF(!zstr(tech_pvt->rpid), SIPTAG_REMOTE_PARTY_ID_STR(tech_pvt->rpid)),
				   TAG_IF(!zstr(tech_pvt->preferred_id), SIPTAG_P_PREFERRED_IDENTITY_STR(tech_pvt->preferred_id)),
				   TAG_IF(!zstr(tech_pvt->asserted_id), SIPTAG_P_ASSERTED_IDENTITY_STR(tech_pvt->asserted_id)),
				   TAG_IF(!zstr(tech_pvt->privacy), SIPTAG_PRIVACY_STR(tech_pvt->privacy)),
				   TAG_IF(!zstr(alert_info), SIPTAG_HEADER_STR(alert_info)),
				   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
				   TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_CALLEE_ID), SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)),
				   TAG_IF(!zstr(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
				   TAG_IF(!zstr(route_uri), NUTAG_PROXY(route_uri)),
				   TAG_IF(!zstr(invite_route_uri), NUTAG_INITIAL_ROUTE_STR(invite_route_uri)),
				   TAG_IF(!zstr(route), SIPTAG_ROUTE_STR(route)),
				   TAG_IF(tech_pvt->profile->minimum_session_expires, NUTAG_MIN_SE(tech_pvt->profile->minimum_session_expires)),
				   TAG_IF(cseq, SIPTAG_CSEQ(cseq)),
				   TAG_IF(zstr(tech_pvt->local_sdp_str), SIPTAG_PAYLOAD_STR("")),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_REUSE_REJECTED(1)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_ORDERED_USER(1)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL)),
				   TAG_IF(rep, SIPTAG_REPLACES_STR(rep)),
				   TAG_IF(!require_timer, NUTAG_TIMER_AUTOREQUIRE(0)),
				   TAG_IF(!zstr(tech_pvt->local_sdp_str), SOATAG_HOLD(holdstr)), TAG_END());
	} else {
		nua_invite(tech_pvt->nh,
				   NUTAG_AUTOANSWER(0),
				   NUTAG_AUTOACK(0),
				   NUTAG_SESSION_TIMER(tech_pvt->session_timeout),
				   NUTAG_SESSION_REFRESHER(tech_pvt->session_refresher),
				   TAG_IF(sofia_test_flag(tech_pvt, TFLAG_RECOVERED), NUTAG_INVITE_TIMER(UINT_MAX)),
				   TAG_IF(invite_full_from, SIPTAG_FROM_STR(invite_full_from)),
				   TAG_IF(invite_full_to, SIPTAG_TO_STR(invite_full_to)),
				   TAG_IF(tech_pvt->redirected, NUTAG_URL(tech_pvt->redirected)),
				   TAG_IF(!zstr(recover_via), SIPTAG_VIA_STR(recover_via)),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   TAG_IF(!zstr(tech_pvt->rpid), SIPTAG_REMOTE_PARTY_ID_STR(tech_pvt->rpid)),
				   TAG_IF(!zstr(tech_pvt->preferred_id), SIPTAG_P_PREFERRED_IDENTITY_STR(tech_pvt->preferred_id)),
				   TAG_IF(!zstr(tech_pvt->asserted_id), SIPTAG_P_ASSERTED_IDENTITY_STR(tech_pvt->asserted_id)),
				   TAG_IF(!zstr(tech_pvt->privacy), SIPTAG_PRIVACY_STR(tech_pvt->privacy)),
				   TAG_IF(!zstr(alert_info), SIPTAG_HEADER_STR(alert_info)),
				   TAG_IF(!zstr(extra_headers), SIPTAG_HEADER_STR(extra_headers)),
				   TAG_IF(sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_CALLEE_ID), SIPTAG_HEADER_STR("X-FS-Support: " FREESWITCH_SUPPORT)),
				   TAG_IF(!zstr(max_forwards), SIPTAG_MAX_FORWARDS_STR(max_forwards)),
				   TAG_IF(!zstr(route_uri), NUTAG_PROXY(route_uri)),
				   TAG_IF(!zstr(route), SIPTAG_ROUTE_STR(route)),
				   TAG_IF(!zstr(invite_route_uri), NUTAG_INITIAL_ROUTE_STR(invite_route_uri)),
				   TAG_IF(tech_pvt->profile->minimum_session_expires, NUTAG_MIN_SE(tech_pvt->profile->minimum_session_expires)),
				   TAG_IF(!require_timer, NUTAG_TIMER_AUTOREQUIRE(0)),
				   TAG_IF(cseq, SIPTAG_CSEQ(cseq)),
				   NUTAG_MEDIA_ENABLE(0),
				   SIPTAG_CONTENT_TYPE_STR(mp_type ? mp_type : "application/sdp"),
				   SIPTAG_PAYLOAD_STR(mp ? mp : tech_pvt->local_sdp_str), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), SOATAG_HOLD(holdstr), TAG_END());
	}

	sofia_glue_free_destination(dst);
	switch_safe_free(extra_headers);
	switch_safe_free(mp);
	tech_pvt->redirected = NULL;

	return SWITCH_STATUS_SUCCESS;
}

void sofia_glue_do_xfer_invite(switch_core_session_t *session)
{
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	const char *sipip, *format, *contact_url;

	switch_assert(tech_pvt != NULL);
	switch_mutex_lock(tech_pvt->sofia_mutex);
	caller_profile = switch_channel_get_caller_profile(channel);

	if (!zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
		sipip = tech_pvt->profile->extsipip;
		contact_url = tech_pvt->profile->public_url;
	} else {
		sipip = tech_pvt->profile->extsipip ? tech_pvt->profile->extsipip : tech_pvt->profile->sipip;
		contact_url = tech_pvt->profile->url;
	}

	format = strchr(sipip, ':') ? "\"%s\" <sip:%s@[%s]>" : "\"%s\" <sip:%s@%s>";

	if ((tech_pvt->from_str = switch_core_session_sprintf(session, format, caller_profile->caller_id_name, caller_profile->caller_id_number, sipip))) {

		const char *rep = switch_channel_get_variable(channel, SOFIA_REPLACES_HEADER);

		tech_pvt->nh2 = nua_handle(tech_pvt->profile->nua, NULL,
								   SIPTAG_TO_STR(tech_pvt->dest), SIPTAG_FROM_STR(tech_pvt->from_str), SIPTAG_CONTACT_STR(contact_url), TAG_END());

		nua_handle_bind(tech_pvt->nh2, tech_pvt->sofia_private);

		nua_invite(tech_pvt->nh2,
				   SIPTAG_CONTACT_STR(contact_url),
				   TAG_IF(!zstr(tech_pvt->user_via), SIPTAG_VIA_STR(tech_pvt->user_via)),
				   SOATAG_ADDRESS(tech_pvt->adv_sdp_audio_ip),
				   SOATAG_USER_SDP_STR(tech_pvt->local_sdp_str),
				   SOATAG_REUSE_REJECTED(1),
				   SOATAG_ORDERED_USER(1),
				   SOATAG_RTP_SORT(SOA_RTP_SORT_REMOTE), SOATAG_RTP_SELECT(SOA_RTP_SELECT_ALL), TAG_IF(rep, SIPTAG_REPLACES_STR(rep)), TAG_END());
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Memory Error!\n");
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
		sofia_glue_tech_set_local_sdp(tech_pvt, sdp_str, SWITCH_TRUE);
	}
}


#define add_stat(_i, _s)												\
	switch_snprintf(var_name, sizeof(var_name), "rtp_%s_%s", switch_str_nil(prefix), _s) ; \
	switch_snprintf(var_val, sizeof(var_val), "%" SWITCH_SIZE_T_FMT, _i); \
	switch_channel_set_variable(tech_pvt->channel, var_name, var_val)

static void set_stats(switch_rtp_t *rtp_session, private_object_t *tech_pvt, const char *prefix)
{
	switch_rtp_stats_t *stats = switch_rtp_get_stats(rtp_session, NULL);
	char var_name[256] = "", var_val[35] = "";

	if (stats) {

		add_stat(stats->inbound.raw_bytes, "in_raw_bytes");
		add_stat(stats->inbound.media_bytes, "in_media_bytes");
		add_stat(stats->inbound.packet_count, "in_packet_count");
		add_stat(stats->inbound.media_packet_count, "in_media_packet_count");
		add_stat(stats->inbound.skip_packet_count, "in_skip_packet_count");
		add_stat(stats->inbound.jb_packet_count, "in_jb_packet_count");
		add_stat(stats->inbound.dtmf_packet_count, "in_dtmf_packet_count");
		add_stat(stats->inbound.cng_packet_count, "in_cng_packet_count");
		add_stat(stats->inbound.flush_packet_count, "in_flush_packet_count");
		add_stat(stats->inbound.largest_jb_size, "in_largest_jb_size");

		add_stat(stats->outbound.raw_bytes, "out_raw_bytes");
		add_stat(stats->outbound.media_bytes, "out_media_bytes");
		add_stat(stats->outbound.packet_count, "out_packet_count");
		add_stat(stats->outbound.media_packet_count, "out_media_packet_count");
		add_stat(stats->outbound.skip_packet_count, "out_skip_packet_count");
		add_stat(stats->outbound.dtmf_packet_count, "out_dtmf_packet_count");
		add_stat(stats->outbound.cng_packet_count, "out_cng_packet_count");

		add_stat(stats->rtcp.packet_count, "rtcp_packet_count");
		add_stat(stats->rtcp.octet_count, "rtcp_octet_count");

	}
}

void sofia_glue_set_rtp_stats(private_object_t *tech_pvt)
{
	if (tech_pvt->rtp_session) {
		set_stats(tech_pvt->rtp_session, tech_pvt, "audio");
	}

	if (tech_pvt->video_rtp_session) {
		set_stats(tech_pvt->video_rtp_session, tech_pvt, "video");
	}
}

void sofia_glue_deactivate_rtp(private_object_t *tech_pvt)
{
	int loops = 0;
	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		while (loops < 10 && (sofia_test_flag(tech_pvt, TFLAG_READING) || sofia_test_flag(tech_pvt, TFLAG_WRITING))) {
			switch_yield(10000);
			loops++;
		}
	}

	if (tech_pvt->video_rtp_session) {
		switch_rtp_destroy(&tech_pvt->video_rtp_session);
	} else if (tech_pvt->local_sdp_video_port) {
		switch_rtp_release_port(tech_pvt->rtpip, tech_pvt->local_sdp_video_port);
	}


	if (tech_pvt->local_sdp_video_port > 0 && !zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
		switch_nat_del_mapping((switch_port_t) tech_pvt->local_sdp_video_port, SWITCH_NAT_UDP);
		switch_nat_del_mapping((switch_port_t) tech_pvt->local_sdp_video_port + 1, SWITCH_NAT_UDP);
	}


	if (tech_pvt->rtp_session) {
		switch_rtp_destroy(&tech_pvt->rtp_session);
	} else if (tech_pvt->local_sdp_audio_port) {
		switch_rtp_release_port(tech_pvt->rtpip, tech_pvt->local_sdp_audio_port);
	}

	if (tech_pvt->local_sdp_audio_port > 0 && !zstr(tech_pvt->remote_ip) && sofia_glue_check_nat(tech_pvt->profile, tech_pvt->remote_ip)) {
		switch_nat_del_mapping((switch_port_t) tech_pvt->local_sdp_audio_port, SWITCH_NAT_UDP);
		switch_nat_del_mapping((switch_port_t) tech_pvt->local_sdp_audio_port + 1, SWITCH_NAT_UDP);
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
		switch_rtp_set_recv_pt(tech_pvt->rtp_session, tech_pvt->agreed_pt);
	}

 end:
	if (resetting) {
		switch_core_session_unlock_codec_write(tech_pvt->session);
		switch_core_session_unlock_codec_read(tech_pvt->session);
	}

	sofia_glue_tech_set_video_codec(tech_pvt, force);

	return status;
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
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Set Local Key [%s]\n", tech_pvt->local_crypto_key);

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
	switch_rtp_crypto_key_type_t type;
	char *p;


	if (!switch_rtp_ready(tech_pvt->rtp_session)) {
		goto bad;
	}

	p = strchr(key_str, ' ');

	if (p && *p && *(p + 1)) {
		p++;
		if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_32, strlen(SWITCH_RTP_CRYPTO_KEY_32))) {
			type = AES_CM_128_HMAC_SHA1_32;
		} else if (!strncasecmp(p, SWITCH_RTP_CRYPTO_KEY_80, strlen(SWITCH_RTP_CRYPTO_KEY_80))) {
			type = AES_CM_128_HMAC_SHA1_80;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
			goto bad;
		}

		p = strchr(p, ' ');
		if (p && *p && *(p + 1)) {
			p++;
			if (strncasecmp(p, "inline:", 7)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Parse Error near [%s]\n", p);
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

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "Error!\n");
	return SWITCH_STATUS_FALSE;

}


switch_status_t sofia_glue_activate_rtp(private_object_t *tech_pvt, switch_rtp_flag_t myflags)
{
	const char *err = NULL;
	const char *val = NULL;
	switch_rtp_flag_t flags;
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
		sofia_set_flag_locked(tech_pvt, TFLAG_SECURE);
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


	if (myflags) {
		flags = myflags;
	} else if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
			   !((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
	} else {
		flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_DATAWAIT);
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_PASS_RFC2833)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "pass_rfc2833")) && switch_true(val))) {
		sofia_set_flag(tech_pvt, TFLAG_PASS_RFC2833);
	}


	if (sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFLUSH)
		|| ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_autoflush")) && switch_true(val))) {
		flags |= SWITCH_RTP_FLAG_AUTOFLUSH;
	}

	if (!(sofia_test_pflag(tech_pvt->profile, PFLAG_REWRITE_TIMESTAMPS) ||
		  ((val = switch_channel_get_variable(tech_pvt->channel, "rtp_rewrite_timestamps")) && switch_true(val)))) {
		flags |= SWITCH_RTP_FLAG_RAW_WRITE;
	}

	if (sofia_test_pflag(tech_pvt->profile, PFLAG_SUPPRESS_CNG)) {
		tech_pvt->cng_pt = 0;
	} else if (tech_pvt->cng_pt) {
		flags |= SWITCH_RTP_FLAG_AUTO_CNG;
	}

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if (!strcasecmp(tech_pvt->read_impl.iananame, "L16")) {
		flags |= SWITCH_RTP_FLAG_BYTESWAP;
	}
#endif
	
	if ((flags & SWITCH_RTP_FLAG_BYTESWAP) && (val = switch_channel_get_variable(tech_pvt->channel, "rtp_disable_byteswap")) && switch_true(val)) {
		flags &= ~SWITCH_RTP_FLAG_BYTESWAP;
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

		if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
			!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
		} else {
			flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_DATAWAIT);
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
											   (switch_rtp_flag_t) flags, timer_name, &err, switch_core_session_get_pool(tech_pvt->session));
	}

	if (switch_rtp_ready(tech_pvt->rtp_session)) {
		uint8_t vad_in = sofia_test_flag(tech_pvt, TFLAG_VAD_IN) ? 1 : 0;
		uint8_t vad_out = sofia_test_flag(tech_pvt, TFLAG_VAD_OUT) ? 1 : 0;
		uint8_t inb = sofia_test_flag(tech_pvt, TFLAG_OUTBOUND) ? 0 : 1;
		uint32_t stun_ping = 0;
		const char *ssrc;

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

		if (tech_pvt->remote_crypto_key && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
			sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1, tech_pvt->crypto_type, tech_pvt->local_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag, tech_pvt->crypto_type, tech_pvt->remote_raw_key,
									  SWITCH_RTP_KEY_LEN);
			switch_channel_set_variable(tech_pvt->channel, SOFIA_SECURE_MEDIA_CONFIRMED_VARIABLE, "true");
		}


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

				if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) &&
					!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
					flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT);
				} else {
					flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_PROXY_MEDIA | SWITCH_RTP_FLAG_DATAWAIT);
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

			if (!sofia_test_pflag(tech_pvt->profile, PFLAG_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(tech_pvt->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_AUTOADJ | SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_RAW_WRITE);
			} else {
				flags = (switch_rtp_flag_t) (SWITCH_RTP_FLAG_DATAWAIT | SWITCH_RTP_FLAG_RAW_WRITE);
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MEDIA)) {
				flags |= SWITCH_RTP_FLAG_PROXY_MEDIA;
			}
			sofia_glue_tech_set_video_codec(tech_pvt, 0);

			flags &= ~(SWITCH_RTP_FLAG_USE_TIMER | SWITCH_RTP_FLAG_NOBLOCK);
			flags |= SWITCH_RTP_FLAG_VIDEO;

			tech_pvt->video_rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
														 tech_pvt->local_sdp_video_port,
														 tech_pvt->remote_sdp_video_ip,
														 tech_pvt->remote_sdp_video_port,
														 tech_pvt->video_agreed_pt,
														 1, 90000, (switch_rtp_flag_t) flags, NULL, &err, switch_core_session_get_pool(tech_pvt->session));

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

void sofia_glue_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session)
{
	switch_channel_t *aleg_channel;
	private_object_t *aleg_tech_pvt;
	switch_channel_t *bleg_channel;
	private_object_t *bleg_tech_pvt;

	if (!switch_core_session_compare(aleg_session, bleg_session)) {
		/* since this digs into channel internals its only compatible with sofia sessions*/
		return;
	}

	aleg_channel = switch_core_session_get_channel(aleg_session);
	aleg_tech_pvt = switch_core_session_get_private(aleg_session);
	bleg_channel = switch_core_session_get_channel(bleg_session);
	bleg_tech_pvt = switch_core_session_get_private(bleg_session);

	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG1, "Deciding whether to pass zrtp-hash between a-leg and b-leg\n");
	if (!(switch_channel_test_flag(aleg_tech_pvt->channel, CF_ZRTP_PASSTHRU_REQ))) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG1, "CF_ZRTP_PASSTHRU_REQ not set on a-leg, so not propagating zrtp-hash\n");
		return;
	}
	if (aleg_tech_pvt->remote_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG, "Passing a-leg remote zrtp-hash (audio) to b-leg\n");
		bleg_tech_pvt->local_sdp_audio_zrtp_hash = switch_core_session_strdup(bleg_tech_pvt->session, aleg_tech_pvt->remote_sdp_audio_zrtp_hash);
		switch_channel_set_variable(bleg_channel, "l_sdp_audio_zrtp_hash", bleg_tech_pvt->local_sdp_audio_zrtp_hash);
	}
	if (aleg_tech_pvt->remote_sdp_video_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG, "Passing a-leg remote zrtp-hash (video) to b-leg\n");
		bleg_tech_pvt->local_sdp_video_zrtp_hash = switch_core_session_strdup(bleg_tech_pvt->session, aleg_tech_pvt->remote_sdp_video_zrtp_hash);
		switch_channel_set_variable(bleg_channel, "l_sdp_video_zrtp_hash", bleg_tech_pvt->local_sdp_video_zrtp_hash);
	}
	if (bleg_tech_pvt->remote_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG, "Passing b-leg remote zrtp-hash (audio) to a-leg\n");
		aleg_tech_pvt->local_sdp_audio_zrtp_hash = switch_core_session_strdup(aleg_tech_pvt->session, bleg_tech_pvt->remote_sdp_audio_zrtp_hash);
		switch_channel_set_variable(aleg_channel, "l_sdp_audio_zrtp_hash", aleg_tech_pvt->local_sdp_audio_zrtp_hash);
	}
	if (bleg_tech_pvt->remote_sdp_video_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_channel), SWITCH_LOG_DEBUG, "Passing b-leg remote zrtp-hash (video) to a-leg\n");
		aleg_tech_pvt->local_sdp_video_zrtp_hash = switch_core_session_strdup(aleg_tech_pvt->session, bleg_tech_pvt->remote_sdp_video_zrtp_hash);
		switch_channel_set_variable(aleg_channel, "l_sdp_video_zrtp_hash", aleg_tech_pvt->local_sdp_video_zrtp_hash);
	}
}

void sofia_glue_pass_zrtp_hash(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_core_session_t *other_session;
	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Deciding whether to pass zrtp-hash between legs\n");
	if (!(switch_channel_test_flag(tech_pvt->channel, CF_ZRTP_PASSTHRU_REQ))) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "CF_ZRTP_PASSTHRU_REQ not set, so not propagating zrtp-hash\n");
		return;
	} else if (!(switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "No partner channel found, so not propagating zrtp-hash\n");
		return;
	} else {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Found peer channel; propagating zrtp-hash if set\n");
		sofia_glue_pass_zrtp_hash2(session, other_session);
		switch_core_session_rwunlock(other_session);
	}
}

static void find_zrtp_hash(switch_core_session_t *session, sdp_session_t *sdp)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = switch_core_session_get_private(session);
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int got_audio = 0, got_video = 0;

	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Looking for zrtp-hash\n");
	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (got_audio && got_video) break;
		if (m->m_port && ((m->m_type == sdp_media_audio && !got_audio)
						  || (m->m_type == sdp_media_video && !got_video))) {
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (zstr(attr->a_name)) continue;
				if (strcasecmp(attr->a_name, "zrtp-hash") || !(attr->a_value)) continue;
				if (m->m_type == sdp_media_audio) {
					switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG,
									  "Found audio zrtp-hash; setting r_sdp_audio_zrtp_hash=%s\n", attr->a_value);
					switch_channel_set_variable(channel, "r_sdp_audio_zrtp_hash", attr->a_value);
					tech_pvt->remote_sdp_audio_zrtp_hash = switch_core_session_strdup(tech_pvt->session, attr->a_value);
					got_audio++;
				} else if (m->m_type == sdp_media_video) {
					switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG,
									  "Found video zrtp-hash; setting r_sdp_video_zrtp_hash=%s\n", attr->a_value);
					switch_channel_set_variable(channel, "r_sdp_video_zrtp_hash", attr->a_value);
					tech_pvt->remote_sdp_video_zrtp_hash = switch_core_session_strdup(tech_pvt->session, attr->a_value);
					got_video++;
				}
				switch_channel_set_flag(channel, CF_ZRTP_HASH);
				break;
			}
		}
	}
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

						if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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

						if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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

					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
		if (sofia_glue_activate_rtp(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
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

static switch_t38_options_t *tech_process_udptl(private_object_t *tech_pvt, sdp_session_t *sdp, sdp_media_t *m)
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

switch_status_t sofia_glue_get_offered_pt(private_object_t *tech_pvt, const switch_codec_implementation_t *mimp, switch_payload_t *pt)
{
	int i = 0;

	for (i = 0; i < tech_pvt->num_codecs; i++) {
		const switch_codec_implementation_t *imp = tech_pvt->codecs[i];

		if (!strcasecmp(imp->iananame, mimp->iananame) && imp->actual_samples_per_second == mimp->actual_samples_per_second) {
			*pt = tech_pvt->ianacodes[i];

			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
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
	int got_crypto = 0, got_audio = 0, got_avp = 0, got_savp = 0, got_udptl = 0;
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
			got_savp++;
		} else if (m->m_proto == sdp_proto_rtp) {
			got_avp++;
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

					if (!switch_channel_test_flag(other_channel, CF_ANSWERED)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), 
										  SWITCH_LOG_WARNING, "%s Error Passing T.38 to unanswered channel %s\n", 
										  switch_channel_get_name(tech_pvt->channel), switch_channel_get_name(other_channel));
						switch_core_session_rwunlock(other_session);						
						sofia_set_flag(tech_pvt, TFLAG_NOREPLY);
						pass = 0;
						match = 0;
						goto done;
					}


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

					if (!(tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_CRYPTO_IN_AVP) && 
						!switch_true(switch_channel_get_variable(tech_pvt->channel, "sip_allow_crypto_in_avp"))) {
						if (m->m_proto != sdp_proto_srtp) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
							match = 0;
							goto done;
						}
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);

					if (tech_pvt->remote_crypto_key && switch_rtp_ready(tech_pvt->rtp_session)) {
						/* Compare all the key. The tag may remain the same even if key changed */
						if (crypto && !strcmp(crypto, tech_pvt->remote_crypto_key)) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Existing key is still valid.\n");
						} else {
							const char *a = switch_stristr("AES", tech_pvt->remote_crypto_key);
							const char *b = switch_stristr("AES", crypto);

							/* Change our key every time we can */
							
							if (sofia_test_flag(tech_pvt, TFLAG_CRYPTO_RECOVER)) {
								sofia_clear_flag(tech_pvt, TFLAG_CRYPTO_RECOVER);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_32);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
								switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), tech_pvt->crypto_type,
														  tech_pvt->local_raw_key, SWITCH_RTP_KEY_LEN);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_80);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
								switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), tech_pvt->crypto_type,
														  tech_pvt->local_raw_key, SWITCH_RTP_KEY_LEN);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
							}

							if (a && b && !strncasecmp(a, b, 23)) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Change Remote key to [%s]\n", crypto);
								tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
								switch_channel_set_variable(tech_pvt->channel, "srtp_remote_audio_crypto_key", crypto);
								tech_pvt->crypto_tag = crypto_tag;
								
								if (switch_rtp_ready(tech_pvt->rtp_session) && sofia_test_flag(tech_pvt, TFLAG_SECURE)) {
									sofia_glue_add_crypto(tech_pvt, tech_pvt->remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
									switch_rtp_add_crypto_key(tech_pvt->rtp_session, SWITCH_RTP_CRYPTO_RECV, tech_pvt->crypto_tag,
															  tech_pvt->crypto_type, tech_pvt->remote_raw_key, SWITCH_RTP_KEY_LEN);
								}
								got_crypto++;
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ignoring unacceptable key\n");
							}
						}
					} else if (!switch_rtp_ready(tech_pvt->rtp_session)) {
						tech_pvt->remote_crypto_key = switch_core_session_strdup(tech_pvt->session, crypto);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Remote Key [%s]\n", tech_pvt->remote_crypto_key);
						switch_channel_set_variable(tech_pvt->channel, "srtp_remote_audio_crypto_key", crypto);
						tech_pvt->crypto_tag = crypto_tag;
						got_crypto++;

						if (zstr(tech_pvt->local_crypto_key)) {
							if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_32);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
							} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
								switch_channel_set_variable(tech_pvt->channel, SOFIA_HAS_CRYPTO_VARIABLE, SWITCH_RTP_CRYPTO_KEY_80);
								sofia_glue_build_crypto(tech_pvt, atoi(crypto), AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
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
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
						switch_channel_set_variable(tech_pvt->channel, "dtmf_type", "rfc2833");
						tech_pvt->dtmf_type = DTMF_2833;
						switch_rtp_set_telephony_event(tech_pvt->rtp_session, (switch_payload_t) best_te);
						switch_channel_set_variable_printf(tech_pvt->channel, "sip_2833_send_payload", "%d", best_te);
					}
				} else {
					te = tech_pvt->recv_te = tech_pvt->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send/recv payload to %u\n", te);
					if (tech_pvt->rtp_session) {
						switch_channel_set_variable(tech_pvt->channel, "dtmf_type", "rfc2833");
						tech_pvt->dtmf_type = DTMF_2833;
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
					}
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
					if ((zstr(map->rm_encoding) || (tech_pvt->profile->ndlb & PFLAG_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
		return SWITCH_CAUSE_UNALLOCATED_NUMBER;
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

	if ((val = switch_channel_get_partner_uuid(tech_pvt->channel))
		&& (other_session = switch_core_session_locate(val))) {
		other_channel = switch_core_session_get_channel(other_session);
		switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);
		
#if 0
		if (!sofia_test_flag(tech_pvt, TFLAG_CHANGE_MEDIA) && !switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING) &&
			(switch_channel_direction(other_channel) == SWITCH_CALL_DIRECTION_OUTBOUND &&
			 switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_OUTBOUND && switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE))) {
			switch_ivr_nomedia(val, SMF_FORCE);
			sofia_set_flag_locked(tech_pvt, TFLAG_CHANGE_MEDIA);
		}
#endif

		switch_core_session_rwunlock(other_session);
	}
}

char *sofia_glue_get_path_from_contact(char *buf)
{
	char *p, *e, *path = NULL, *contact = NULL;

	if (!buf) return NULL;

	contact = sofia_glue_get_url_from_contact(buf, SWITCH_TRUE);

	if (!contact) return NULL;
	
	if ((p = strstr(contact, "fs_path="))) {
		p += 8;

		if (!zstr(p)) {
			path = strdup(p);
		}
	}

	if (!path) return NULL;

	if ((e = strrchr(path, ';'))) {
		*e = '\0';
	}

	switch_url_decode(path);

	free(contact);

	return path;
}

char *sofia_glue_get_url_from_contact(char *buf, uint8_t to_dup)
{
	char *url = NULL, *e;

	switch_assert(buf);
	
	while(*buf == ' ') {
		buf++;
	}

	if (*buf == '"') {
		buf++;
		if((e = strchr(buf, '"'))) {
			buf = e+1;
		}
	}

	while(*buf == ' ') {
		buf++;
	}

	if ((url = strchr(buf, '<')) && (e = switch_find_end_paren(url, '<', '>'))) {
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

switch_status_t sofia_glue_profile_rdlock__(const char *file, const char *func, int line, sofia_profile_t *profile)
{
	switch_status_t status = switch_thread_rwlock_tryrdlock(profile->rwlock);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is locked\n", profile->name);
		return status;
	}
#ifdef SOFIA_DEBUG_RWLOCKS
	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "XXXXXXXXXXXXXX LOCK %s\n", profile->name);
#endif
	return status;
}

switch_bool_t sofia_glue_profile_exists(const char *key)
{
	switch_bool_t tf = SWITCH_FALSE;
	
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (switch_core_hash_find(mod_sofia_globals.profile_hash, key)) {
		tf = SWITCH_TRUE;
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

	return tf;
}

sofia_profile_t *sofia_glue_find_profile__(const char *file, const char *func, int line, const char *key)
{
	sofia_profile_t *profile;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if ((profile = (sofia_profile_t *) switch_core_hash_find(mod_sofia_globals.profile_hash, key))) {
		if (!sofia_test_pflag(profile, PFLAG_RUNNING)) {
#ifdef SOFIA_DEBUG_RWLOCKS
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not running\n", profile->name);
#endif
			profile = NULL;
			goto done;
		}
		if (sofia_glue_profile_rdlock__(file, func, line, profile) != SWITCH_STATUS_SUCCESS) {
			profile = NULL;
		}
	} else {
#ifdef SOFIA_DEBUG_RWLOCKS
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_ERROR, "Profile %s is not in the hash\n", key);
#endif
	}

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


void sofia_glue_del_every_gateway(sofia_profile_t *profile)
{
	sofia_gateway_t *gp = NULL;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gp = profile->gateways; gp; gp = gp->next) {
		sofia_glue_del_gateway(gp);
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}


void sofia_glue_gateway_list(sofia_profile_t *profile, switch_stream_handle_t *stream, int up)
{
	sofia_gateway_t *gp = NULL;
	char *r = (char *) stream->data;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (gp = profile->gateways; gp; gp = gp->next) {
		int reged = (gp->status == SOFIA_GATEWAY_UP);
		
		if (up ? reged : !reged) {
			stream->write_function(stream, "%s ", gp->name);
		}
	}

	if (r) {
		end_of(r) = '\0';
	}

	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
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
				int diff = (int) (switch_epoch_time_now(NULL) - pptr->started);
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


void sofia_glue_global_siptrace(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				nua_set_params(pptr->nua, TPTAG_LOG(on), TAG_END());				
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_global_standby(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				if (on) {
					sofia_set_pflag_locked(pptr, PFLAG_STANDBY);
				} else {
					sofia_clear_pflag_locked(pptr, PFLAG_STANDBY);
				}
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}

void sofia_glue_global_capture(switch_bool_t on)
{
       switch_hash_index_t *hi;
       const void *var;
       void *val;
       sofia_profile_t *pptr;

       switch_mutex_lock(mod_sofia_globals.hash_mutex);
       if (mod_sofia_globals.profile_hash) {
               for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
                       switch_hash_this(hi, &var, NULL, &val);
                       if ((pptr = (sofia_profile_t *) val)) {
                               nua_set_params(pptr->nua, TPTAG_CAPT(on ? mod_sofia_globals.capture_server : NULL), TAG_END());
                       }
               }
       }
       switch_mutex_unlock(mod_sofia_globals.hash_mutex);

}


void sofia_glue_global_watchdog(switch_bool_t on)
{
	switch_hash_index_t *hi;
	const void *var;
	void *val;
	sofia_profile_t *pptr;

	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	if (mod_sofia_globals.profile_hash) {
		for (hi = switch_hash_first(NULL, mod_sofia_globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			if ((pptr = (sofia_profile_t *) val)) {
				pptr->watchdog_enabled = (on ? 1 : 0);
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
			char *pkey = switch_mprintf("%s::%s", profile->name, gp->name);

			switch_core_hash_delete(mod_sofia_globals.gateway_hash, gp->name);
			switch_core_hash_delete(mod_sofia_globals.gateway_hash, pkey);
			switch_safe_free(pkey);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "deleted gateway %s from profile %s\n", gp->name, profile->name);
		}
		profile->gateways = NULL;
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
}

int sofia_recover_callback(switch_core_session_t *session) 
{

	switch_channel_t *channel = switch_core_session_get_channel(session);
	private_object_t *tech_pvt = NULL;
	sofia_profile_t *profile = NULL;
	const char *tmp;
	const char *rr;
	int r = 0;
	const char *profile_name = switch_channel_get_variable_dup(channel, "recovery_profile_name", SWITCH_FALSE, -1);
	

	if (zstr(profile_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Missing profile\n");
		return 0;
	}

	if (!(profile = sofia_glue_find_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Invalid profile %s\n", profile_name);
		return 0;
	}


	tech_pvt = (private_object_t *) switch_core_session_alloc(session, sizeof(private_object_t));
	tech_pvt->channel = channel;

	switch_mutex_init(&tech_pvt->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
	switch_mutex_init(&tech_pvt->sofia_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	tech_pvt->remote_ip = (char *) switch_channel_get_variable(channel, "sip_network_ip");
	tech_pvt->remote_port = atoi(switch_str_nil(switch_channel_get_variable(channel, "sip_network_port")));
	tech_pvt->caller_profile = switch_channel_get_caller_profile(channel);

	if ((tmp = switch_channel_get_variable(tech_pvt->channel, "sip_2833_send_payload"))) {
		int te = atoi(tmp);
		if (te > 64) {
			tech_pvt->te = te;
		} 
	}

	if ((tmp = switch_channel_get_variable(tech_pvt->channel, "sip_2833_recv_payload"))) {
		int te = atoi(tmp);
		if (te > 64) {
			tech_pvt->recv_te = te;
		} 
	}

	if ((tmp = switch_channel_get_variable(channel, "rtp_use_codec_string"))) {
		char *tmp_codec_string = switch_core_session_strdup(session, tmp);
		tech_pvt->codec_order_last = switch_separate_string(tmp_codec_string, ',', tech_pvt->codec_order, SWITCH_MAX_CODECS);
		tech_pvt->num_codecs = switch_loadable_module_get_codecs_sorted(tech_pvt->codecs, SWITCH_MAX_CODECS, tech_pvt->codec_order, tech_pvt->codec_order_last);
	}
	

	rr = switch_channel_get_variable(channel, "sip_invite_record_route");

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		int break_rfc = switch_true(switch_channel_get_variable(channel, "sip_recovery_break_rfc"));
		tech_pvt->dest = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_req_uri"));
		switch_channel_set_variable(channel, "sip_handle_full_from", switch_channel_get_variable(channel, break_rfc ? "sip_full_to" : "sip_full_from"));
		switch_channel_set_variable(channel, "sip_handle_full_to", switch_channel_get_variable(channel, break_rfc ? "sip_full_from" : "sip_full_to"));
	} else {
		tech_pvt->redirected = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_contact_uri"));

		if (zstr(rr)) {
			switch_channel_set_variable_printf(channel, "sip_invite_route_uri", "<sip:%s@%s:%s;lr>",
											   switch_channel_get_variable(channel, "sip_from_user"),
											   switch_channel_get_variable(channel, "sip_network_ip"), switch_channel_get_variable(channel, "sip_network_port")
											   );
		}

		tech_pvt->dest = switch_core_session_sprintf(session, "sip:%s", switch_channel_get_variable(channel, "sip_from_uri"));

		if (!switch_channel_get_variable_dup(channel, "sip_handle_full_from", SWITCH_FALSE, -1)) {
			switch_channel_set_variable(channel, "sip_handle_full_from", switch_channel_get_variable(channel, "sip_full_to"));
		}

		if (!switch_channel_get_variable_dup(channel, "sip_handle_full_to", SWITCH_FALSE, -1)) {
			switch_channel_set_variable(channel, "sip_handle_full_to", switch_channel_get_variable(channel, "sip_full_from"));
		}
	}

	if (rr) {
		switch_channel_set_variable(channel, "sip_invite_route_uri", rr);
	}

	tech_pvt->dest_to = tech_pvt->dest;

	sofia_glue_attach_private(session, profile, tech_pvt, NULL);
	switch_channel_set_name(tech_pvt->channel, switch_channel_get_variable(channel, "channel_name"));

	if ((tmp = switch_channel_get_variable(channel, "srtp_remote_audio_crypto_key"))) {
		tech_pvt->remote_crypto_key = switch_core_session_strdup(session, tmp);
		sofia_set_flag(tech_pvt, TFLAG_CRYPTO_RECOVER);
	}

	if ((tmp = switch_channel_get_variable(channel, "sip_local_sdp_str"))) {
		tech_pvt->local_sdp_str = switch_core_session_strdup(session, tmp);
	}

	if ((tmp = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE))) {
		tech_pvt->remote_sdp_str = switch_core_session_strdup(session, tmp);
	}

	switch_channel_set_variable(channel, "sip_invite_call_id", switch_channel_get_variable(channel, "sip_call_id"));

	if (switch_true(switch_channel_get_variable(channel, "sip_nat_detected"))) {
		switch_channel_set_variable_printf(channel, "sip_route_uri", "sip:%s@%s:%s",
										   switch_channel_get_variable(channel, "sip_req_user"),
										   switch_channel_get_variable(channel, "sip_network_ip"), switch_channel_get_variable(channel, "sip_network_port")
										   );
	}

	if (session) {
		const char *ip = switch_channel_get_variable(channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
		const char *a_ip = switch_channel_get_variable(channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE);
		const char *port = switch_channel_get_variable(channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
		const char *r_ip = switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
		const char *r_port = switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
		const char *use_uuid;

		switch_channel_set_flag(channel, CF_RECOVERING);

		if ((use_uuid = switch_channel_get_variable(channel, "origination_uuid"))) {
			if (switch_core_session_set_uuid(session, use_uuid) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s set UUID=%s\n", switch_channel_get_name(channel),
								  use_uuid);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s set UUID=%s FAILED\n",
								  switch_channel_get_name(channel), use_uuid);
			}
		}

		if (!switch_channel_test_flag(channel, CF_PROXY_MODE) && ip && port) {
			const char *tmp;
			tech_pvt->iananame = tech_pvt->rm_encoding = (char *) switch_channel_get_variable(channel, "sip_use_codec_name");
			tech_pvt->rm_fmtp = (char *) switch_channel_get_variable(channel, "sip_use_codec_fmtp");

			if ((tmp = switch_channel_get_variable(channel, "sip_use_codec_rate"))) {
				tech_pvt->rm_rate = atoi(tmp);
			}

			if ((tmp = switch_channel_get_variable(channel, "sip_use_codec_ptime"))) {
				tech_pvt->codec_ms = atoi(tmp);
			}

			if ((tmp = switch_channel_get_variable(channel, "sip_use_pt"))) {
				tech_pvt->pt = tech_pvt->agreed_pt = (switch_payload_t)atoi(tmp);
			}
			
			sofia_glue_tech_set_codec(tech_pvt, 1);

			tech_pvt->adv_sdp_audio_ip = tech_pvt->extrtpip = (char *) ip;
			tech_pvt->adv_sdp_audio_port = tech_pvt->local_sdp_audio_port = (switch_port_t)atoi(port);

			if (!zstr(ip)) {
				tech_pvt->local_sdp_audio_ip = switch_core_session_strdup(session, ip);
				tech_pvt->rtpip = tech_pvt->local_sdp_audio_ip;
			}

			if (!zstr(a_ip)) {
				tech_pvt->adv_sdp_audio_ip = switch_core_session_strdup(session, a_ip);
			}

			if (r_ip && r_port) {
				tech_pvt->remote_sdp_audio_ip = (char *) r_ip;
				tech_pvt->remote_sdp_audio_port = (switch_port_t)atoi(r_port);
			}

			if (switch_channel_test_flag(channel, CF_VIDEO)) {
				if ((tmp = switch_channel_get_variable(channel, "sip_use_video_pt"))) {
					tech_pvt->video_pt = tech_pvt->video_agreed_pt = (switch_payload_t)atoi(tmp);
				}


				tech_pvt->video_rm_encoding = (char *) switch_channel_get_variable(channel, "sip_use_video_codec_name");
				tech_pvt->video_rm_fmtp = (char *) switch_channel_get_variable(channel, "sip_use_video_codec_fmtp");

				ip = switch_channel_get_variable(channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE);
				port = switch_channel_get_variable(channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE);
				r_ip = switch_channel_get_variable(channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE);
				r_port = switch_channel_get_variable(channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE);

				sofia_set_flag(tech_pvt, TFLAG_VIDEO);

				if ((tmp = switch_channel_get_variable(channel, "sip_use_video_codec_rate"))) {
					tech_pvt->video_rm_rate = atoi(tmp);
				}

				if ((tmp = switch_channel_get_variable(channel, "sip_use_video_codec_ptime"))) {
					tech_pvt->video_codec_ms = atoi(tmp);
				}

				tech_pvt->adv_sdp_video_port = tech_pvt->local_sdp_video_port = (switch_port_t)atoi(port);

				if (r_ip && r_port) {
					tech_pvt->remote_sdp_video_ip = (char *) r_ip;
					tech_pvt->remote_sdp_video_port = (switch_port_t)atoi(r_port);
				}
				//sofia_glue_tech_set_video_codec(tech_pvt, 1);
			}

			sofia_glue_set_local_sdp(tech_pvt, NULL, 0, NULL, 1);

			if (sofia_glue_activate_rtp(tech_pvt, 0) != SWITCH_STATUS_SUCCESS) {
				goto end;
			}
			
			if (switch_rtp_ready(tech_pvt->rtp_session)) {
				if ((tmp = switch_channel_get_variable(channel, "sip_audio_recv_pt"))) {
					switch_rtp_set_recv_pt(tech_pvt->rtp_session, (switch_payload_t)atoi(tmp));
				}
			}

			if (switch_rtp_ready(tech_pvt->video_rtp_session)) {
				if ((tmp = switch_channel_get_variable(channel, "sip_video_recv_pt"))) {
					switch_rtp_set_recv_pt(tech_pvt->video_rtp_session, (switch_payload_t)atoi(tmp));
				}
			}

			if (tech_pvt->te) {
				switch_rtp_set_telephony_event(tech_pvt->rtp_session, tech_pvt->te);
			}

			if (tech_pvt->recv_te) {
				switch_rtp_set_telephony_recv_event(tech_pvt->rtp_session, tech_pvt->recv_te);
			}

		}
	}

	r++;

 end:

	return r;

}

int sofia_glue_recover(switch_bool_t flush)
{
	sofia_profile_t *profile;
	int r = 0;
	switch_console_callback_match_t *matches;


	if (list_profiles_full(NULL, NULL, &matches, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
		switch_console_callback_match_node_t *m;
		for (m = matches->head; m; m = m->next) {
			if ((profile = sofia_glue_find_profile(m->val))) {
				r += sofia_glue_profile_recover(profile, flush);
				sofia_glue_release_profile(profile);
			}
		}
		switch_console_free_matches(&matches);
	}
	return r;
}

int sofia_glue_profile_recover(sofia_profile_t *profile, switch_bool_t flush)
{
	int r = 0;

	if (profile) {
		sofia_clear_pflag_locked(profile, PFLAG_STANDBY);

		if (flush) {
			switch_core_recovery_flush(SOFIA_RECOVER, profile->name);
		} else {
			r = switch_core_recovery_recover(SOFIA_RECOVER, profile->name);
		}
	}

	return r;
}


int sofia_glue_init_sql(sofia_profile_t *profile)
{
	char *test_sql = NULL;

	char reg_sql[] =
		"CREATE TABLE sip_registrations (\n"
		"   call_id          VARCHAR(255),\n"
		"   sip_user         VARCHAR(255),\n"
		"   sip_host         VARCHAR(255),\n"
		"   presence_hosts   VARCHAR(255),\n"
		"   contact          VARCHAR(1024),\n"
		"   status           VARCHAR(255),\n"
		"   rpid             VARCHAR(255),\n"
		"   expires          INTEGER,\n"
		"   user_agent       VARCHAR(255),\n"
		"   server_user      VARCHAR(255),\n"
		"   server_host      VARCHAR(255),\n"
		"   profile_name     VARCHAR(255),\n"
		"   hostname         VARCHAR(255),\n"
		"   network_ip       VARCHAR(255),\n"
		"   network_port     VARCHAR(6),\n"
		"   sip_username     VARCHAR(255),\n"
		"   sip_realm        VARCHAR(255),\n"
		"   mwi_user         VARCHAR(255),\n"
		"   mwi_host         VARCHAR(255),\n"
		"   orig_server_host VARCHAR(255),\n"
		"   orig_hostname    VARCHAR(255),\n"
		"   sub_host         VARCHAR(255)\n"
		");\n";

	char pres_sql[] =
		"CREATE TABLE sip_presence (\n"
		"   sip_user        VARCHAR(255),\n"
		"   sip_host        VARCHAR(255),\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n"
		"   expires         INTEGER,\n"
		"   user_agent      VARCHAR(255),\n"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   network_ip      VARCHAR(255),\n"
		"   network_port    VARCHAR(6),\n"
		"   open_closed     VARCHAR(255)\n"
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
		"   hostname        VARCHAR(255),\n"
		"   contact         VARCHAR(255),\n"
		"   presence_id     VARCHAR(255),\n"
		"   presence_data   VARCHAR(255),\n"
		"   call_info       VARCHAR(255),\n"
		"   call_info_state VARCHAR(255) default '',\n"
		"   expires         INTEGER default 0,\n"
		"   status          VARCHAR(255),\n"
		"   rpid            VARCHAR(255),\n"
		"   sip_to_tag      VARCHAR(255),\n"
		"   sip_from_tag    VARCHAR(255),\n"
		"   rcd             INTEGER not null default 0\n"
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
		"   hostname        VARCHAR(255),\n"
		"   network_port    VARCHAR(6),\n"
		"   network_ip      VARCHAR(255),\n"
		"   version         INTEGER DEFAULT 0 NOT NULL,\n"
		"   orig_proto      VARCHAR(255),\n"
		"   full_to         VARCHAR(255)\n"
		");\n";

	char auth_sql[] =
		"CREATE TABLE sip_authentication (\n"
		"   nonce           VARCHAR(255),\n"
		"   expires         INTEGER,"
		"   profile_name    VARCHAR(255),\n"
		"   hostname        VARCHAR(255),\n"
		"   last_nc         INTEGER\n"
		");\n";

	/* should we move this glue to sofia_sla or keep it here where all db init happens? XXX MTK */
	char shared_appearance_sql[] =
		"CREATE TABLE sip_shared_appearance_subscriptions (\n"
		"   subscriber        VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   aor               VARCHAR(255),\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   network_ip        VARCHAR(255)\n"
		");\n";

	char shared_appearance_dialogs_sql[] =
		"CREATE TABLE sip_shared_appearance_dialogs (\n"
		"   profile_name      VARCHAR(255),\n"
		"   hostname          VARCHAR(255),\n"
		"   contact_str       VARCHAR(255),\n"
		"   call_id           VARCHAR(255),\n"
		"   network_ip        VARCHAR(255),\n"
		"   expires           INTEGER\n"
		");\n";

	
	int x;
	char *indexes[] = {
		"create index sr_call_id on sip_registrations (call_id)",
		"create index sr_sip_user on sip_registrations (sip_user)",
		"create index sr_sip_host on sip_registrations (sip_host)",
		"create index sr_sub_host on sip_registrations (sub_host)",
		"create index sr_mwi_user on sip_registrations (mwi_user)",
		"create index sr_mwi_host on sip_registrations (mwi_host)",
		"create index sr_profile_name on sip_registrations (profile_name)",
		"create index sr_presence_hosts on sip_registrations (presence_hosts)",
		"create index sr_contact on sip_registrations (contact)",
		"create index sr_expires on sip_registrations (expires)",
		"create index sr_hostname on sip_registrations (hostname)",
		"create index sr_status on sip_registrations (status)",
		"create index sr_network_ip on sip_registrations (network_ip)",
		"create index sr_network_port on sip_registrations (network_port)",
		"create index sr_sip_username on sip_registrations (sip_username)",
		"create index sr_sip_realm on sip_registrations (sip_realm)",
		"create index sr_orig_server_host on sip_registrations (orig_server_host)",
		"create index sr_orig_hostname on sip_registrations (orig_hostname)",
		"create index ss_call_id on sip_subscriptions (call_id)",
		"create index ss_hostname on sip_subscriptions (hostname)",
		"create index ss_network_ip on sip_subscriptions (network_ip)",
		"create index ss_sip_user on sip_subscriptions (sip_user)",
		"create index ss_sip_host on sip_subscriptions (sip_host)",
		"create index ss_presence_hosts on sip_subscriptions (presence_hosts)",
		"create index ss_event on sip_subscriptions (event)",
		"create index ss_proto on sip_subscriptions (proto)",
		"create index ss_sub_to_user on sip_subscriptions (sub_to_user)",
		"create index ss_sub_to_host on sip_subscriptions (sub_to_host)",
		"create index ss_expires on sip_subscriptions (expires)",
		"create index ss_orig_proto on sip_subscriptions (orig_proto)",
		"create index ss_network_port on sip_subscriptions (network_port)",
		"create index ss_profile_name on sip_subscriptions (profile_name)",
		"create index ss_version on sip_subscriptions (version)",
		"create index ss_full_from on sip_subscriptions (full_from)",
		"create index ss_contact on sip_subscriptions (contact)",
		"create index sd_uuid on sip_dialogs (uuid)",
		"create index sd_hostname on sip_dialogs (hostname)",
		"create index sd_presence_data on sip_dialogs (presence_data)",
		"create index sd_call_info on sip_dialogs (call_info)",
		"create index sd_call_info_state on sip_dialogs (call_info_state)",
		"create index sd_expires on sip_dialogs (expires)",
		"create index sd_rcd on sip_dialogs (rcd)",
		"create index sd_sip_to_tag on sip_dialogs (sip_to_tag)",
		"create index sd_sip_from_user on sip_dialogs (sip_from_user)",
		"create index sd_sip_from_host on sip_dialogs (sip_from_host)",
		"create index sd_sip_to_host on sip_dialogs (sip_to_host)",
		"create index sd_presence_id on sip_dialogs (presence_id)",
		"create index sd_call_id on sip_dialogs (call_id)",
		"create index sd_sip_from_tag on sip_dialogs (sip_from_tag)",
		"create index sp_hostname on sip_presence (hostname)",
		"create index sp_open_closed on sip_presence (open_closed)",
		"create index sp_sip_user on sip_presence (sip_user)",
		"create index sp_sip_host on sip_presence (sip_host)",
		"create index sp_profile_name on sip_presence (profile_name)",
		"create index sp_expires on sip_presence (expires)",
		"create index sa_nonce on sip_authentication (nonce)",
		"create index sa_hostname on sip_authentication (hostname)",
		"create index sa_expires on sip_authentication (expires)",
		"create index sa_last_nc on sip_authentication (last_nc)",
		"create index ssa_hostname on sip_shared_appearance_subscriptions (hostname)",
		"create index ssa_network_ip on sip_shared_appearance_subscriptions (network_ip)",
		"create index ssa_subscriber on sip_shared_appearance_subscriptions (subscriber)",
		"create index ssa_profile_name on sip_shared_appearance_subscriptions (profile_name)",
		"create index ssa_aor on sip_shared_appearance_subscriptions (aor)",
		"create index ssd_profile_name on sip_shared_appearance_dialogs (profile_name)",
		"create index ssd_hostname on sip_shared_appearance_dialogs (hostname)",
		"create index ssd_contact_str on sip_shared_appearance_dialogs (contact_str)",
		"create index ssd_call_id on sip_shared_appearance_dialogs (call_id)",
		"create index ssd_expires on sip_shared_appearance_dialogs (expires)",
		NULL
	};
		
	switch_cache_db_handle_t *dbh = sofia_glue_get_db_handle(profile);
	char *test2;
	char *err;

	if (!dbh) {
		return 0;
	}
		

	test_sql = switch_mprintf("delete from sip_registrations where (sub_host is null or contact like '%%TCP%%' "
							  "or status like '%%TCP%%' or status like '%%TLS%%') and hostname='%q' "
							  "and network_ip like '%%' and network_port like '%%' and sip_username "
							  "like '%%' and mwi_user  like '%%' and mwi_host like '%%' "
							  "and orig_server_host like '%%' and orig_hostname like '%%'", mod_sofia_globals.hostname);


	switch_cache_db_test_reactive(dbh, test_sql, "drop table sip_registrations", reg_sql);
	
	test2 = switch_mprintf("%s;%s", test_sql, test_sql);
			
	if (switch_cache_db_execute_sql(dbh, test2, &err) != SWITCH_STATUS_SUCCESS) {

		if (switch_stristr("read-only", err)) {
			free(err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "GREAT SCOTT!!! Cannot execute batched statements! [%s]\n"
							  "If you are using mysql, make sure you are using MYODBC 3.51.18 or higher and enable FLAG_MULTI_STATEMENTS\n", err);
			
			switch_cache_db_release_db_handle(&dbh);
			free(test2);
			free(test_sql);
			free(err);
			return 0;
		}
	}

	free(test2);


	free(test_sql);

	test_sql = switch_mprintf("delete from sip_subscriptions where hostname='%q' and full_to='XXX'", mod_sofia_globals.hostname);
							  
	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_subscriptions", sub_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_dialogs where hostname='%q' and (expires <> -9999 or rpid='' or sip_from_tag='' or rcd > 0)",
							  mod_sofia_globals.hostname);


	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_dialogs", dialog_sql);
		
	free(test_sql);
	test_sql = switch_mprintf("delete from sip_presence where hostname='%q' or open_closed=''", mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_presence", pres_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_authentication where hostname='%q' or last_nc >= 0", mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_authentication", auth_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_shared_appearance_subscriptions where contact_str='' or hostname='%q' and network_ip like '%%'",
							  mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_shared_appearance_subscriptions", shared_appearance_sql);

	free(test_sql);
	test_sql = switch_mprintf("delete from sip_shared_appearance_dialogs where contact_str='' or hostname='%q' and network_ip like '%%'",
							  mod_sofia_globals.hostname);

	switch_cache_db_test_reactive(dbh, test_sql, "DROP TABLE sip_shared_appearance_dialogs", shared_appearance_dialogs_sql);
		
	free(test_sql);

	for (x = 0; indexes[x]; x++) {
		switch_cache_db_execute_sql(dbh, indexes[x], NULL);
	}

	switch_cache_db_release_db_handle(&dbh);

	return 1;

}

void sofia_glue_execute_sql(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_sql_queue_manager_push(profile->qm, sql, 1, !sql_already_dynamic);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}


void sofia_glue_execute_sql_now(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_mutex_lock(profile->dbh_mutex);
	switch_sql_queue_manager_push_confirm(profile->qm, sql, 0, !sql_already_dynamic);
	switch_mutex_unlock(profile->dbh_mutex);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}

void sofia_glue_execute_sql_soon(sofia_profile_t *profile, char **sqlp, switch_bool_t sql_already_dynamic)
{
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	

	switch_sql_queue_manager_push(profile->qm, sql, 0, !sql_already_dynamic);

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}
}


switch_cache_db_handle_t *_sofia_glue_get_db_handle(sofia_profile_t *profile, const char *file, const char *func, int line)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(profile->odbc_dsn)) {
		dsn = profile->odbc_dsn;
	} else {
		dsn = profile->dbname;
	}

	if (_switch_cache_db_get_db_handle_dsn(&dbh, dsn, file, func, line) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;
}

void sofia_glue_actually_execute_sql_trans(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}


	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		goto end;
	}

	switch_cache_db_persistant_execute_trans_full(dbh, sql, 1,
												  profile->pre_trans_execute,
												  profile->post_trans_execute,
												  profile->inner_pre_trans_execute,
												  profile->inner_post_trans_execute
												  );

	switch_cache_db_release_db_handle(&dbh);

 end:

	if (mutex) {
		switch_mutex_unlock(mutex);
	}
}

void sofia_glue_actually_execute_sql(sofia_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *err = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}

		return;
	}

	switch_cache_db_execute_sql(dbh, sql, &err);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s]\n%s\n", err, sql);
		free(err);
	}

	switch_cache_db_release_db_handle(&dbh);
}

switch_bool_t sofia_glue_execute_sql_callback(sofia_profile_t *profile,
											  switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}

		return ret;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	switch_cache_db_release_db_handle(&dbh);


	sofia_glue_fire_events(profile);

	return ret;
}

char *sofia_glue_execute_sql2str(sofia_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;
	char *err = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = sofia_glue_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");

		if (mutex) {
			switch_mutex_unlock(mutex);
		}
		
		return NULL;
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, &err);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s]\n%s\n", err, sql);
		free(err);
	}

	switch_cache_db_release_db_handle(&dbh);


	sofia_glue_fire_events(profile);

	return ret;
}

char *sofia_glue_get_register_host(const char *uri)
{
	char *register_host = NULL;
	const char *s;
	char *p = NULL;

	if (zstr(uri)) {
		return NULL;
	}

	if ((s = switch_stristr("sip:", uri))) {
		s += 4;
	} else if ((s = switch_stristr("sips:", uri))) {
		s += 5;
	}

	if (!s) {
		return NULL;
	}

	register_host = strdup(s);
	
	/* remove port for register_host for testing nat acl take into account 
	   ipv6 addresses which are required to have brackets around the addr 
	*/
	
	if ((p = strchr(register_host, ']'))) {
		if (*(p + 1) == ':') {
			*(p + 1) = '\0';
		}
	} else {
		if ((p = strrchr(register_host, ':'))) {
			*p = '\0';
		}
	}
	
	/* register_proxy should always start with "sip:" or "sips:" */
	assert(register_host);

	return register_host;
}

const char *sofia_glue_strip_proto(const char *uri)
{
	char *p;

	if ((p = strchr(uri, ':'))) {
		return p + 1;
	}

	return uri;
}

sofia_cid_type_t sofia_cid_name2type(const char *name)
{
	if (!strcasecmp(name, "rpid")) {
		return CID_TYPE_RPID;
	}

	if (!strcasecmp(name, "pid")) {
		return CID_TYPE_PID;
	}

	return CID_TYPE_NONE;

}

/* all the values of the structure are initialized to NULL  */
/* in case of failure the function returns NULL */
/* sofia_destination->route can be NULL */
sofia_destination_t *sofia_glue_get_destination(char *data)
{
	sofia_destination_t *dst = NULL;
	char *to = NULL;
	char *contact = NULL;
	char *route = NULL;
	char *route_uri = NULL;
	char *eoc = NULL;
	char *p = NULL;

	if (zstr(data)) {
		return NULL;
	}

	if (!(dst = (sofia_destination_t *) malloc(sizeof(sofia_destination_t)))) {
		return NULL;
	}

	/* return a copy of what is in the buffer between the first < and > */
	if (!(contact = sofia_glue_get_url_from_contact(data, 1))) {
		goto mem_fail;
	}

	if ((eoc = strstr(contact, ";fs_path="))) {
		*eoc = '\0';

		if (!(route = strdup(eoc + 9))) {
			goto mem_fail;
		}

		for (p = route; p && *p; p++) {
			if (*p == '>' || *p == ';') {
				*p = '\0';
				break;
			}
		}

		switch_url_decode(route);

		if (!(route_uri = strdup(route))) {
			goto mem_fail;
		}
		if ((p = strchr(route_uri, ','))) {
			do {
				*p = '\0';
			} while ((--p > route_uri) && *p == ' ');
		}
	}

	if (!(to = strdup(data))) {
		goto mem_fail;
	}

	if ((eoc = strstr(to, ";fs_path="))) {
		*eoc++ = '>';
		*eoc = '\0';
	}

	if ((p = strstr(contact, ";fs_"))) {
		*p = '\0';
	}

	dst->contact = contact;
	dst->to = to;
	dst->route = route;
	dst->route_uri = route_uri;
	return dst;

 mem_fail:
	switch_safe_free(contact);
	switch_safe_free(to);
	switch_safe_free(route);
	switch_safe_free(route_uri);
	switch_safe_free(dst);
	return NULL;
}

void sofia_glue_free_destination(sofia_destination_t *dst)
{
	if (dst) {
		switch_safe_free(dst->contact);
		switch_safe_free(dst->route);
		switch_safe_free(dst->route_uri);
		switch_safe_free(dst->to);
		switch_safe_free(dst);
	}
}

switch_status_t sofia_glue_send_notify(sofia_profile_t *profile, const char *user, const char *host, const char *event, const char *contenttype,
									   const char *body, const char *o_contact, const char *network_ip, const char *call_id)
{
	char *id = NULL;
	nua_handle_t *nh;
	sofia_destination_t *dst = NULL;
	char *contact_str, *contact, *user_via = NULL;
	char *route_uri = NULL, *p;

	contact = sofia_glue_get_url_from_contact((char *) o_contact, 1);

	if ((p = strstr(contact, ";fs_"))) {
		*p = '\0';
	}

	if (!zstr(network_ip) && sofia_glue_check_nat(profile, network_ip)) {
		char *ptr = NULL;
		//const char *transport_str = NULL;


		id = switch_mprintf("sip:%s@%s", user, profile->extsipip);
		switch_assert(id);

		if ((ptr = sofia_glue_find_parameter(o_contact, "transport="))) {
			sofia_transport_t transport = sofia_glue_str2transport(ptr);
			//transport_str = sofia_glue_transport2str(transport);
			switch (transport) {
			case SOFIA_TRANSPORT_TCP:
				contact_str = profile->tcp_public_contact;
				break;
			case SOFIA_TRANSPORT_TCP_TLS:
				contact_str = profile->tls_public_contact;
				break;
			default:
				contact_str = profile->public_url;
				break;
			}
			user_via = sofia_glue_create_external_via(NULL, profile, transport);
		} else {
			user_via = sofia_glue_create_external_via(NULL, profile, SOFIA_TRANSPORT_UDP);
			contact_str = profile->public_url;
		}

	} else {
		contact_str = profile->url;
		id = switch_mprintf("sip:%s@%s", user, host);
	}

	dst = sofia_glue_get_destination((char *) o_contact);
	switch_assert(dst);

	if (dst->route_uri) {
		route_uri = sofia_glue_strip_uri(dst->route_uri);
	}

	nh = nua_handle(profile->nua, NULL, NUTAG_URL(contact), SIPTAG_FROM_STR(id), SIPTAG_TO_STR(id), SIPTAG_CONTACT_STR(contact_str), TAG_END());
	nua_handle_bind(nh, &mod_sofia_globals.destroy_private);

	nua_notify(nh,
			   NUTAG_NEWSUB(1),
			   TAG_IF(dst->route_uri, NUTAG_PROXY(route_uri)), TAG_IF(dst->route, SIPTAG_ROUTE_STR(dst->route)),
			   TAG_IF(user_via, SIPTAG_VIA_STR(user_via)),
			   SIPTAG_SUBSCRIPTION_STATE_STR("terminated;reason=noresource"),
			   TAG_IF(event, SIPTAG_EVENT_STR(event)),
			   TAG_IF(call_id, SIPTAG_CALL_ID_STR(call_id)),
			   TAG_IF(contenttype, SIPTAG_CONTENT_TYPE_STR(contenttype)), TAG_IF(body, SIPTAG_PAYLOAD_STR(body)), TAG_END());

	switch_safe_free(contact);
	switch_safe_free(route_uri);
	switch_safe_free(id);
	sofia_glue_free_destination(dst);
	switch_safe_free(user_via);

	return SWITCH_STATUS_SUCCESS;
}


int sofia_glue_tech_simplify(private_object_t *tech_pvt)
{
	const char *uuid, *network_addr_a = NULL, *network_addr_b = NULL, *simplify, *simplify_other_channel;
	switch_channel_t *other_channel = NULL, *inbound_channel = NULL;
	switch_core_session_t *other_session = NULL, *inbound_session = NULL;
	uint8_t did_simplify = 0;
	int r = 0;

	if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED) || switch_channel_test_flag(tech_pvt->channel, CF_SIMPLIFY)) {
		goto end;
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_BRIDGED) && 
		(uuid = switch_channel_get_partner_uuid(tech_pvt->channel)) && (other_session = switch_core_session_locate(uuid))) {

		other_channel = switch_core_session_get_channel(other_session);

		if (switch_channel_test_flag(other_channel, CF_ANSWERED)) {	/* Check if the other channel is answered */
			simplify = switch_channel_get_variable(tech_pvt->channel, "sip_auto_simplify");
			simplify_other_channel = switch_channel_get_variable(other_channel, "sip_auto_simplify");

			r = 1;

			if (switch_true(simplify) && !switch_channel_test_flag(tech_pvt->channel, CF_BRIDGE_ORIGINATOR)) {
				network_addr_a = switch_channel_get_variable(tech_pvt->channel, "network_addr");
				network_addr_b = switch_channel_get_variable(other_channel, "network_addr");
				inbound_session = other_session;
				inbound_channel = other_channel;
			} else if (switch_true(simplify_other_channel) && !switch_channel_test_flag(other_channel, CF_BRIDGE_ORIGINATOR)) {
				network_addr_a = switch_channel_get_variable(other_channel, "network_addr");
				network_addr_b = switch_channel_get_variable(tech_pvt->channel, "network_addr");
				inbound_session = tech_pvt->session;
				inbound_channel = tech_pvt->channel;
			}

			if (inbound_channel && inbound_session && !zstr(network_addr_a) && !zstr(network_addr_b) && !strcmp(network_addr_a, network_addr_b)) {
				if (strcmp(network_addr_a, switch_str_nil(tech_pvt->profile->sipip))
					&& strcmp(network_addr_a, switch_str_nil(tech_pvt->profile->extsipip))) {

					switch_core_session_message_t *msg;
					
					switch_log_printf(SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, switch_channel_get_uuid(inbound_channel),
									  SWITCH_LOG_NOTICE, "Will simplify channel [%s]\n", switch_channel_get_name(inbound_channel));
					
					msg = switch_core_session_alloc(inbound_session, sizeof(*msg));
					MESSAGE_STAMP_FFL(msg);
					msg->message_id = SWITCH_MESSAGE_INDICATE_SIMPLIFY;
					msg->from = __FILE__;
					switch_core_session_receive_message(inbound_session, msg);

					did_simplify = 1;

					switch_core_recovery_track(inbound_session);

					switch_channel_set_flag(inbound_channel, CF_SIMPLIFY);
					
				}
			}

			if (!did_simplify && inbound_channel) {
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, __FILE__, __SWITCH_FUNC__, __LINE__, switch_channel_get_uuid(inbound_channel), SWITCH_LOG_NOTICE,
								  "Could not simplify channel [%s]\n", switch_channel_get_name(inbound_channel));
			}
		}

		switch_core_session_rwunlock(other_session);
	}


 end:

	return r;
}

void sofia_glue_pause_jitterbuffer(switch_core_session_t *session, switch_bool_t on)
{
	switch_core_session_message_t *msg;
	msg = switch_core_session_alloc(session, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_JITTER_BUFFER;
	msg->string_arg = switch_core_session_strdup(session, on ? "pause" : "resume");
	msg->from = __FILE__;

	switch_core_session_queue_message(session, msg);
}


void sofia_glue_build_vid_refresh_message(switch_core_session_t *session, const char *pl)
{
	switch_core_session_message_t *msg;
	msg = switch_core_session_alloc(session, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
	if (pl) {
		msg->string_arg = switch_core_session_strdup(session, pl);
	}
	msg->from = __FILE__;

	switch_core_session_queue_message(session, msg);
}


void sofia_glue_parse_rtp_bugs(switch_rtp_bug_flag_t *flag_pole, const char *str)
{

	if (switch_stristr("clear", str)) {
		*flag_pole = 0;
	}

	if (switch_stristr("CISCO_SKIP_MARK_BIT_2833", str)) {
		*flag_pole |= RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
	}

	if (switch_stristr("~CISCO_SKIP_MARK_BIT_2833", str)) {
		*flag_pole &= ~RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
	}

	if (switch_stristr("SONUS_SEND_INVALID_TIMESTAMP_2833", str)) {
		*flag_pole |= RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
	}

	if (switch_stristr("~SONUS_SEND_INVALID_TIMESTAMP_2833", str)) {
		*flag_pole &= ~RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
	}

	if (switch_stristr("IGNORE_MARK_BIT", str)) {
		*flag_pole |= RTP_BUG_IGNORE_MARK_BIT;
	}	

	if (switch_stristr("~IGNORE_MARK_BIT", str)) {
		*flag_pole &= ~RTP_BUG_IGNORE_MARK_BIT;
	}	

	if (switch_stristr("SEND_LINEAR_TIMESTAMPS", str)) {
		*flag_pole |= RTP_BUG_SEND_LINEAR_TIMESTAMPS;
	}

	if (switch_stristr("~SEND_LINEAR_TIMESTAMPS", str)) {
		*flag_pole &= ~RTP_BUG_SEND_LINEAR_TIMESTAMPS;
	}

	if (switch_stristr("START_SEQ_AT_ZERO", str)) {
		*flag_pole |= RTP_BUG_START_SEQ_AT_ZERO;
	}

	if (switch_stristr("~START_SEQ_AT_ZERO", str)) {
		*flag_pole &= ~RTP_BUG_START_SEQ_AT_ZERO;
	}

	if (switch_stristr("NEVER_SEND_MARKER", str)) {
		*flag_pole |= RTP_BUG_NEVER_SEND_MARKER;
	}

	if (switch_stristr("~NEVER_SEND_MARKER", str)) {
		*flag_pole &= ~RTP_BUG_NEVER_SEND_MARKER;
	}

	if (switch_stristr("IGNORE_DTMF_DURATION", str)) {
		*flag_pole |= RTP_BUG_IGNORE_DTMF_DURATION;
	}

	if (switch_stristr("~IGNORE_DTMF_DURATION", str)) {
		*flag_pole &= ~RTP_BUG_IGNORE_DTMF_DURATION;
	}

	if (switch_stristr("ACCEPT_ANY_PACKETS", str)) {
		*flag_pole |= RTP_BUG_ACCEPT_ANY_PACKETS;
	}

	if (switch_stristr("~ACCEPT_ANY_PACKETS", str)) {
		*flag_pole &= ~RTP_BUG_ACCEPT_ANY_PACKETS;
	}

	if (switch_stristr("GEN_ONE_GEN_ALL", str)) {
		*flag_pole |= RTP_BUG_GEN_ONE_GEN_ALL;
	}

	if (switch_stristr("~GEN_ONE_GEN_ALL", str)) {
		*flag_pole &= ~RTP_BUG_GEN_ONE_GEN_ALL;
	}

	if (switch_stristr("CHANGE_SSRC_ON_MARKER", str)) {
		*flag_pole |= RTP_BUG_CHANGE_SSRC_ON_MARKER;
	}

	if (switch_stristr("~CHANGE_SSRC_ON_MARKER", str)) {
		*flag_pole &= ~RTP_BUG_CHANGE_SSRC_ON_MARKER;
	}

	if (switch_stristr("FLUSH_JB_ON_DTMF", str)) {
		*flag_pole |= RTP_BUG_FLUSH_JB_ON_DTMF;
	}

	if (switch_stristr("~FLUSH_JB_ON_DTMF", str)) {
		*flag_pole &= ~RTP_BUG_FLUSH_JB_ON_DTMF;
	}
}

char *sofia_glue_gen_contact_str(sofia_profile_t *profile, sip_t const *sip, nua_handle_t *nh, sofia_dispatch_event_t *de, sofia_nat_parse_t *np)
{
	char *contact_str = NULL;
	const char *contact_host;//, *contact_user;
	sip_contact_t const *contact;
	char *port;
	const char *display = "\"user\"";
	char new_port[25] = "";
	sofia_nat_parse_t lnp = { { 0 } };
	const char *ipv6;
	sip_from_t const *from;

	if (!sip || !sip->sip_contact || !sip->sip_contact->m_url) {
		return NULL;
	}

	from = sip->sip_from;
	contact = sip->sip_contact;

	if (!np) {
		np = &lnp;
	}

	sofia_glue_get_addr(de->data->e_msg, np->network_ip, sizeof(np->network_ip), &np->network_port);
	
	if (sofia_glue_check_nat(profile, np->network_ip)) {
		np->is_auto_nat = 1;
	}

	port = (char *) contact->m_url->url_port;
	contact_host = sip->sip_contact->m_url->url_host;
	//contact_user = sip->sip_contact->m_url->url_user;

	display = contact->m_display;


	if (zstr(display)) {
		if (from) {
			display = from->a_display;
			if (zstr(display)) {
				display = "\"user\"";
			}
		}
	} else {
		display = "\"user\"";
	}

	if (sofia_test_pflag(profile, PFLAG_AGGRESSIVE_NAT_DETECTION)) {
		if (sip->sip_via) {
			const char *v_port = sip->sip_via->v_port;
			const char *v_host = sip->sip_via->v_host;

			if (v_host && sip->sip_via->v_received) {
				np->is_nat = "via received";
			} else if (v_host && strcmp(np->network_ip, v_host)) {
				np->is_nat = "via host";
			} else if (v_port && atoi(v_port) != np->network_port) {
				np->is_nat = "via port";
			}
		}
	}

	if (!np->is_nat && sip && sip->sip_via && sip->sip_via->v_port &&
		atoi(sip->sip_via->v_port) == 5060 && np->network_port != 5060 ) {
		np->is_nat = "via port";
	}

	if (!np->is_nat && profile->nat_acl_count) {
		uint32_t x = 0;
		int ok = 1;
		char *last_acl = NULL;

		if (!zstr(contact_host)) {
			for (x = 0; x < profile->nat_acl_count; x++) {
				last_acl = profile->nat_acl[x];
				if (!(ok = switch_check_network_list_ip(contact_host, last_acl))) {
					break;
				}
			}

			if (ok) {
				np->is_nat = last_acl;
			}
		}
	}

	if (np->is_nat && profile->local_network && switch_check_network_list_ip(np->network_ip, profile->local_network)) {
		if (profile->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IP %s is on local network, not seting NAT mode.\n", np->network_ip);
		}
		np->is_nat = NULL;
	}

	if (sip->sip_record_route && sip->sip_record_route->r_url) {
		char *full_contact = sip_header_as_string(nh->nh_home, (void *) contact);
		char *route = sofia_glue_strip_uri(sip_header_as_string(nh->nh_home, (void *) sip->sip_record_route));
		char *full_contact_dup;
		char *route_encoded;
		int route_encoded_len;
		full_contact_dup = sofia_glue_get_url_from_contact(full_contact, 1);
		route_encoded_len = (int)(strlen(route) * 3) + 1;
		switch_zmalloc(route_encoded, route_encoded_len);
		switch_url_encode(route, route_encoded, route_encoded_len);
		contact_str = switch_mprintf("%s <%s;fs_path=%s>", display, full_contact_dup, route_encoded);
		free(full_contact_dup);
		free(route_encoded);
	}
	else if (np->is_nat && np->fs_path) {
		char *full_contact = sip_header_as_string(nh->nh_home, (void *) contact);
		char *full_contact_dup;
		char *path_encoded;
		int path_encoded_len;
		char *path_val;
		const char *tp;

		full_contact_dup = sofia_glue_get_url_from_contact(full_contact, 1);

		if ((tp = switch_stristr("transport=", full_contact_dup))) {
			tp += 10;
		}
		
		if (zstr(tp)) {
			tp = "udp";
		}

		path_val = switch_mprintf("sip:%s:%d;transport=%s", np->network_ip, np->network_port, tp);
		path_encoded_len = (int)(strlen(path_val) * 3) + 1;

		switch_zmalloc(path_encoded, path_encoded_len);
		switch_copy_string(path_encoded, ";fs_path=", 10);
		switch_url_encode(path_val, path_encoded + 9, path_encoded_len - 9);
		
		contact_str = switch_mprintf("%s <%s;fs_nat=yes%s>", display, full_contact_dup, path_encoded);

		free(full_contact_dup);
		free(path_encoded);
		free(path_val);

	} else {

		if (zstr(contact_host)) {
			np->is_nat = "No contact host";
		}
		
		if (np->is_nat) {
			contact_host = np->network_ip;
			switch_snprintf(new_port, sizeof(new_port), ":%d", np->network_port);
			port = NULL;
		}
		
		
		if (port) {
			switch_snprintf(new_port, sizeof(new_port), ":%s", port);
		}
		
		ipv6 = strchr(contact_host, ':');
		

		if (contact->m_url->url_params) {
			contact_str = switch_mprintf("%s <sip:%s%s%s%s%s%s;%s>%s",
										 display, contact->m_url->url_user,
										 contact->m_url->url_user ? "@" : "",
										 ipv6 ? "[" : "",
										 contact_host, ipv6 ? "]" : "", new_port, contact->m_url->url_params, np->is_nat ? ";fs_nat=yes" : "");
		} else {
			contact_str = switch_mprintf("%s <sip:%s%s%s%s%s%s>%s",
										 display,
										 contact->m_url->url_user,
										 contact->m_url->url_user ? "@" : "",
										 ipv6 ? "[" : "", contact_host, ipv6 ? "]" : "", new_port, np->is_nat ? ";fs_nat=yes" : "");
		}
	}
		
	return contact_str;
}

char *sofia_glue_get_host(const char *str, switch_memory_pool_t *pool)
{
	char *s, *p;

	if ((p = strchr(str, '@'))) {
		p++;
	} else {
		return NULL;
	}

	if (pool) {
		s = switch_core_strdup(pool, p);
	} else {
		s = strdup(p);
	}

	for (p = s; p && *p; p++) {
		if ((*p == ';') || (*p == '>')) {
			*p = '\0';
			break;
		}
	}

	return s;
}

void sofia_glue_fire_events(sofia_profile_t *profile)
{
	void *pop = NULL;

	while (profile->event_queue && switch_queue_trypop(profile->event_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_fire(&event);
	}

}

void sofia_event_fire(sofia_profile_t *profile, switch_event_t **event)
{
	switch_queue_push(profile->event_queue, *event);
	*event = NULL;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
