/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_core_media.c -- Core Media
 *
 */

#include <switch.h>
#include <switch_ssl.h>
#include <switch_stun.h>
#include <switch_nat.h>
#include "private/switch_core_pvt.h"
#include <switch_curl.h>
#include <errno.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/su.h>

static switch_t38_options_t * switch_core_media_process_udptl(switch_core_session_t *session, sdp_session_t *sdp, sdp_media_t *m);
static void switch_core_media_find_zrtp_hash(switch_core_session_t *session, sdp_session_t *sdp);
static void switch_core_media_set_r_sdp_codec_string(switch_core_session_t *session, const char *codec_string, sdp_session_t *sdp, switch_sdp_type_t sdp_type);

//#define GOOGLE_ICE
#define RTCP_MUX
#define MAX_CODEC_CHECK_FRAMES 50//x:mod_sofia.h
#define MAX_MISMATCH_FRAMES 5//x:mod_sofia.h
#define type2str(type) type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : "audio"

typedef enum {
	SMF_INIT = (1 << 0),
	SMF_READY = (1 << 1),
	SMF_JB_PAUSED = (1 << 2)
} smh_flag_t;


typedef struct secure_settings_s {
	int crypto_tag;
	unsigned char local_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	unsigned char remote_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t crypto_type;
	char *local_crypto_key;
	char *remote_crypto_key;
} switch_secure_settings_t;



struct media_helper {
	switch_core_session_t *session;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	int up;
};

typedef enum {
	CRYPTO_MODE_OPTIONAL,
	CRYPTO_MODE_MANDATORY,
	CRYPTO_MODE_FORBIDDEN
} switch_rtp_crypto_mode_t;

typedef struct switch_rtp_engine_s {
	switch_secure_settings_t ssec[CRYPTO_INVALID+1];
	switch_rtp_crypto_key_type_t crypto_type;

	switch_media_type_t type;

	switch_rtp_t *rtp_session;
	switch_frame_t read_frame;
	switch_codec_t read_codec;
	switch_codec_t write_codec;

	switch_codec_implementation_t read_impl;
	switch_codec_implementation_t write_impl;

	switch_size_t last_ts;
	switch_size_t last_seq;
	uint32_t check_frames;
	uint32_t mismatch_count;
	uint32_t last_codec_ms;
	uint8_t codec_reinvites;
	uint32_t max_missed_packets;
	uint32_t max_missed_hold_packets;
	uint32_t ssrc;
	uint32_t remote_ssrc;
	switch_port_t remote_rtcp_port;
	switch_rtp_bug_flag_t rtp_bugs;


	char *local_sdp_ip;
	switch_port_t local_sdp_port;
	char *adv_sdp_ip;
	switch_port_t adv_sdp_port;
	char *proxy_sdp_ip;
	switch_port_t proxy_sdp_port;


	/** ZRTP **/
	char *local_sdp_zrtp_hash;
	char *remote_sdp_zrtp_hash;

	payload_map_t *cur_payload_map;
	payload_map_t *payload_map;
	payload_map_t *pmap_tail;

	uint32_t timestamp_send;

	char *cand_acl[SWITCH_MAX_CAND_ACL];
	int cand_acl_count;

	ice_t ice_in;
	ice_t ice_out;

	int8_t rtcp_mux;

	dtls_fingerprint_t local_dtls_fingerprint;
	dtls_fingerprint_t remote_dtls_fingerprint;

	char *remote_rtp_ice_addr;
	switch_port_t remote_rtp_ice_port;

	char *remote_rtcp_ice_addr;
	switch_port_t remote_rtcp_ice_port;

	struct media_helper mh;
	switch_thread_t *media_thread;
	switch_mutex_t *read_mutex[2];

	uint8_t reset_codec;
	uint8_t codec_negotiated;

	uint8_t fir;
	uint8_t pli;
	uint8_t no_crypto;
} switch_rtp_engine_t;


struct switch_media_handle_s {
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_core_media_flag_t media_flags[SCMF_MAX];
	smh_flag_t flags;
	switch_rtp_engine_t engines[SWITCH_MEDIA_TYPE_TOTAL];

	char *codec_order[SWITCH_MAX_CODECS];
    int codec_order_last;
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];

	int payload_space;
	char *origin;

	switch_mutex_t *mutex;
	switch_mutex_t *sdp_mutex;

	const switch_codec_implementation_t *negotiated_codecs[SWITCH_MAX_CODECS];
	int num_negotiated_codecs;
	switch_payload_t ianacodes[SWITCH_MAX_CODECS];
	char *fmtps[SWITCH_MAX_CODECS];
	int video_count;

	uint32_t owner_id;
	uint32_t session_id;

	switch_core_media_params_t *mparams;

	char *msid;
	char *cname;

	switch_rtp_crypto_mode_t crypto_mode;
	switch_rtp_crypto_key_type_t crypto_suite_order[CRYPTO_INVALID+1];
};


static switch_srtp_crypto_suite_t SUITES[CRYPTO_INVALID] = {
	{ "AEAD_AES_256_GCM_8", AEAD_AES_256_GCM_8, 44},
	{ "AEAD_AES_128_GCM_8", AEAD_AES_128_GCM_8, 28},
	{ "AES_CM_256_HMAC_SHA1_80", AES_CM_256_HMAC_SHA1_80, 46},
	{ "AES_CM_192_HMAC_SHA1_80", AES_CM_192_HMAC_SHA1_80, 38},
	{ "AES_CM_128_HMAC_SHA1_80", AES_CM_128_HMAC_SHA1_80, 30},
	{ "AES_CM_256_HMAC_SHA1_32", AES_CM_256_HMAC_SHA1_32, 46},
	{ "AES_CM_192_HMAC_SHA1_32", AES_CM_192_HMAC_SHA1_32, 38},
	{ "AES_CM_128_HMAC_SHA1_32", AES_CM_128_HMAC_SHA1_32, 30},
	{ "AES_CM_128_NULL_AUTH", AES_CM_128_NULL_AUTH, 30}
};

SWITCH_DECLARE(switch_rtp_crypto_key_type_t) switch_core_media_crypto_str2type(const char *str)
{
	int i;

	for (i = 0; i < CRYPTO_INVALID; i++) {
		if (!strncasecmp(str, SUITES[i].name, strlen(SUITES[i].name))) {
			return SUITES[i].type;
		}
	}

	return CRYPTO_INVALID;
}


SWITCH_DECLARE(const char *) switch_core_media_crypto_type2str(switch_rtp_crypto_key_type_t type)
{
	switch_assert(type < CRYPTO_INVALID);
	return SUITES[type].name;
}


SWITCH_DECLARE(int) switch_core_media_crypto_keylen(switch_rtp_crypto_key_type_t type)
{
	switch_assert(type < CRYPTO_INVALID);
	return SUITES[type].keylen;
}


static int get_channels(const char *name, int dft)
{

	if (!zstr(name) && !switch_true(switch_core_get_variable("NDLB_broken_opus_sdp")) && !strcasecmp(name, "opus")) {
		return 2; /* IKR???*/
	}

	return dft ? dft : 1;
}

static void _switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session, switch_media_type_t type)
{
	switch_rtp_engine_t *aleg_engine;
	switch_rtp_engine_t *bleg_engine;

	if (!aleg_session->media_handle || !bleg_session->media_handle) return;
	aleg_engine = &aleg_session->media_handle->engines[type];
	bleg_engine = &bleg_session->media_handle->engines[type];



	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_session->channel), SWITCH_LOG_DEBUG1, 
					  "Deciding whether to pass zrtp-hash between a-leg and b-leg\n");

	if (!(switch_channel_test_flag(aleg_session->channel, CF_ZRTP_PASSTHRU_REQ))) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_session->channel), SWITCH_LOG_DEBUG1, 
						  "CF_ZRTP_PASSTHRU_REQ not set on a-leg, so not propagating zrtp-hash\n");
		return;
	}

	if (aleg_engine->remote_sdp_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_session->channel), SWITCH_LOG_DEBUG, "Passing a-leg remote zrtp-hash (audio) to b-leg\n");
		bleg_engine->local_sdp_zrtp_hash = switch_core_session_strdup(bleg_session, aleg_engine->remote_sdp_zrtp_hash);
		switch_channel_set_variable(bleg_session->channel, "l_sdp_audio_zrtp_hash", bleg_engine->local_sdp_zrtp_hash);
	}
	
	if (bleg_engine->remote_sdp_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(aleg_session->channel), SWITCH_LOG_DEBUG, "Passing b-leg remote zrtp-hash (audio) to a-leg\n");
		aleg_engine->local_sdp_zrtp_hash = switch_core_session_strdup(aleg_session, bleg_engine->remote_sdp_zrtp_hash);
		switch_channel_set_variable(aleg_session->channel, "l_sdp_audio_zrtp_hash", aleg_engine->local_sdp_zrtp_hash);
	}
}

SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session)
{
	_switch_core_media_pass_zrtp_hash2(aleg_session, bleg_session, SWITCH_MEDIA_TYPE_AUDIO);
	_switch_core_media_pass_zrtp_hash2(aleg_session, bleg_session, SWITCH_MEDIA_TYPE_VIDEO);
}


SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_core_session_t *other_session;
	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Deciding whether to pass zrtp-hash between legs\n");
	if (!(switch_channel_test_flag(channel, CF_ZRTP_PASSTHRU_REQ))) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "CF_ZRTP_PASSTHRU_REQ not set, so not propagating zrtp-hash\n");
		return;
	} else if (!(switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "No partner channel found, so not propagating zrtp-hash\n");
		return;
	} else {
		switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Found peer channel; propagating zrtp-hash if set\n");
		switch_core_media_pass_zrtp_hash2(session, other_session);
		switch_core_session_rwunlock(other_session);
	}
}

SWITCH_DECLARE(const char *) switch_core_media_get_zrtp_hash(switch_core_session_t *session, switch_media_type_t type, switch_bool_t local)
{
	switch_rtp_engine_t *engine;
	if (!session->media_handle) return NULL;

	engine = &session->media_handle->engines[type];

	if (local) {
		return engine->local_sdp_zrtp_hash;
	}

	
	return engine->remote_sdp_zrtp_hash;

}

static void switch_core_media_find_zrtp_hash(switch_core_session_t *session, sdp_session_t *sdp)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_rtp_engine_t *audio_engine;
	switch_rtp_engine_t *video_engine;
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int got_audio = 0, got_video = 0;

	if (!session->media_handle) return;

	audio_engine = &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO];
	video_engine = &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO];


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
					audio_engine->remote_sdp_zrtp_hash = switch_core_session_strdup(session, attr->a_value);
					got_audio++;
				} else if (m->m_type == sdp_media_video) {
					switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG,
									  "Found video zrtp-hash; setting r_sdp_video_zrtp_hash=%s\n", attr->a_value);
					switch_channel_set_variable(channel, "r_sdp_video_zrtp_hash", attr->a_value);
					video_engine->remote_sdp_zrtp_hash = switch_core_session_strdup(session, attr->a_value);
					got_video++;
				}
				switch_channel_set_flag(channel, CF_ZRTP_HASH);
				break;
			}
		}
	}
}


static switch_t38_options_t * switch_core_media_process_udptl(switch_core_session_t *session, sdp_session_t *sdp, sdp_media_t *m)
{
	switch_t38_options_t *t38_options = switch_channel_get_private(session->channel, "t38_options");
	sdp_attribute_t *attr;

	if (!t38_options) {
		t38_options = switch_core_session_alloc(session, sizeof(switch_t38_options_t));

		// set some default value
		t38_options->T38FaxVersion = 0;
		t38_options->T38MaxBitRate = 14400;
		t38_options->T38FaxRateManagement = switch_core_session_strdup(session, "transferredTCF");
		t38_options->T38FaxUdpEC = switch_core_session_strdup(session, "t38UDPRedundancy");
		t38_options->T38FaxMaxBuffer = 500;
		t38_options->T38FaxMaxDatagram = 500;
	}

	t38_options->remote_port = (switch_port_t)m->m_port;

	if (sdp->sdp_origin) {
		t38_options->sdp_o_line = switch_core_session_strdup(session, sdp->sdp_origin->o_username);
	} else {
		t38_options->sdp_o_line = "unknown";
	}
	
	if (m->m_connections && m->m_connections->c_address) {
		t38_options->remote_ip = switch_core_session_strdup(session, m->m_connections->c_address);
	} else if (sdp && sdp->sdp_connection && sdp->sdp_connection->c_address) {
		t38_options->remote_ip = switch_core_session_strdup(session, sdp->sdp_connection->c_address);
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
			t38_options->T38FaxRateManagement = switch_core_session_strdup(session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxMaxBuffer") && attr->a_value) {
			t38_options->T38FaxMaxBuffer = (uint32_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxMaxDatagram") && attr->a_value) {
			t38_options->T38FaxMaxDatagram = (uint32_t) atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38FaxUdpEC") && attr->a_value) {
			t38_options->T38FaxUdpEC = switch_core_session_strdup(session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "T38VendorInfo") && attr->a_value) {
			t38_options->T38VendorInfo = switch_core_session_strdup(session, attr->a_value);
		}
	}

	switch_channel_set_variable(session->channel, "has_t38", "true");
	switch_channel_set_private(session->channel, "t38_options", t38_options);
	switch_channel_set_app_flag_key("T38", session->channel, CF_APP_T38);

	switch_channel_execute_on(session->channel, "sip_execute_on_image");
	switch_channel_api_on(session->channel, "sip_api_on_image");

	return t38_options;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_check_autoadj(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine;
	switch_rtp_engine_t *v_engine;
	switch_media_handle_t *smh;
	const char *val;
	int x = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];	
	
	if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) &&
		!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) && 
		!switch_channel_test_flag(session->channel, CF_WEBRTC)) {
		/* Reactivate the NAT buster flag. */
		
		if (a_engine->rtp_session) {
			switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			x++;
		}
		
		if (v_engine->rtp_session) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			x++;
		}
	}

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}



SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_extract_t38_options(switch_core_session_t *session, const char *r_sdp)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	switch_t38_options_t *t38_options = NULL;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return NULL;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return NULL;
	}

	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (m->m_proto == sdp_proto_udptl && m->m_type == sdp_media_image && m->m_port) {
			t38_options = switch_core_media_process_udptl(session, sdp, m);
			break;
		}
	}

	sdp_parser_free(parser);

	return t38_options;

}



//?
SWITCH_DECLARE(switch_status_t) switch_core_media_process_t38_passthru(switch_core_session_t *session, 
																	   switch_core_session_t *other_session, switch_t38_options_t *t38_options)
{
	char *remote_host;
	switch_port_t remote_port;
	char tmp[32] = "";
	switch_rtp_engine_t *a_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
	remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);
	
	a_engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(session, t38_options->remote_ip);
	a_engine->cur_payload_map->remote_sdp_port = t38_options->remote_port;
							
	if (remote_host && remote_port && !strcmp(remote_host, a_engine->cur_payload_map->remote_sdp_ip) && 
		remote_port == a_engine->cur_payload_map->remote_sdp_port) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
						  "Audio params are unchanged for %s.\n",
						  switch_channel_get_name(session->channel));
	} else {
		const char *err = NULL;
			
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
						  "Audio params changed for %s from %s:%d to %s:%d\n",
						  switch_channel_get_name(session->channel),
						  remote_host, remote_port, a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);
			
		switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->remote_sdp_port);
		switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->cur_payload_map->remote_sdp_ip);
		switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
		if (switch_rtp_set_remote_address(a_engine->rtp_session, a_engine->cur_payload_map->remote_sdp_ip,
										  a_engine->cur_payload_map->remote_sdp_port, 0, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		}
	}
		
	switch_core_media_copy_t38_options(t38_options, other_session);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_payload_code(switch_core_session_t *session,
																	 switch_media_type_t type,
																	 const char *iananame,
																	 uint32_t rate,
																	 switch_payload_t *ptP,
																	 switch_payload_t *recv_ptP,
																	 char **fmtpP)
{
	payload_map_t *pmap;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	switch_payload_t pt = 0, recv_pt = 0;
	int found = 0;
	char *fmtp = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	switch_mutex_lock(smh->sdp_mutex);
	for (pmap = engine->payload_map; pmap ; pmap = pmap->next) {

		if (!pmap->allocated) continue;

		if (!strcasecmp(pmap->iananame, iananame) && (!rate || (rate == pmap->rate))) {
			pt = pmap->pt;
			recv_pt = pmap->recv_pt;
			fmtp = pmap->rm_fmtp;
			found++;
			break;
		}
	}
	switch_mutex_unlock(smh->sdp_mutex);

	if (found) {
		if (ptP) {
			*ptP = pt;
		}
		if (recv_ptP) {
			*recv_ptP = recv_pt; 
		}
		
		if (!zstr(fmtp) && fmtpP) {
			*fmtpP = fmtp;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
	
}


SWITCH_DECLARE(payload_map_t *) switch_core_media_add_payload_map(switch_core_session_t *session, 
																  switch_media_type_t type,
																  const char *name, 
																  const char *fmtp,
																  switch_sdp_type_t sdp_type,
																  uint32_t pt, 
																  uint32_t rate, 
																  uint32_t ptime,
																  uint32_t channels,
																  uint8_t negotiated)
{
	payload_map_t *pmap;
	int exists = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	int local_pt = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	engine = &smh->engines[type];

	switch_mutex_lock(smh->sdp_mutex);


	for (pmap = engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {
		exists = (!strcasecmp(name, pmap->iananame) && (!pmap->rate || rate == pmap->rate) && (!pmap->ptime || pmap->ptime == ptime));

		if (exists) {
			
			if (!zstr(fmtp) && !zstr(pmap->rm_fmtp)) {
				if (strcmp(pmap->rm_fmtp, fmtp)) {
					exists = 0;
					local_pt = pmap->pt;
					continue;
				}
			}
			
			break;
		}
	}


	if (!exists) {
		switch_ssize_t hlen = -1;

		if (engine->payload_map && !engine->payload_map->allocated) {
			pmap = engine->payload_map;
		} else {
			pmap = switch_core_alloc(session->pool, sizeof(*pmap));
		}

		pmap->type = type;
		pmap->iananame = switch_core_strdup(session->pool, name);
		pmap->rm_encoding = pmap->iananame;
		pmap->hash = switch_ci_hashfunc_default(pmap->iananame, &hlen);
		pmap->channels = 1;
	}

	pmap->sdp_type = sdp_type;

	if (ptime) {
		pmap->ptime = ptime;
	}

	if (rate) {
		pmap->rate = rate;
	}

	if (channels) {
		pmap->channels = channels;
	}

	if (!zstr(fmtp) && (zstr(pmap->rm_fmtp) || strcmp(pmap->rm_fmtp, fmtp))) {
		pmap->rm_fmtp = switch_core_strdup(session->pool, fmtp);
	}

	pmap->allocated = 1;


	pmap->recv_pt = (switch_payload_t) pt;


	if (sdp_type == SDP_TYPE_REQUEST || !exists) {
		pmap->pt = (switch_payload_t)  (local_pt ? local_pt : pt);
	}

	if (negotiated) {
		pmap->negotiated = negotiated;
	}

	if (!exists) {
		if (pmap == engine->payload_map) {
			engine->pmap_tail = pmap;
		} else if (!engine->payload_map) {
			engine->payload_map = engine->pmap_tail = pmap;
		} else {
			engine->pmap_tail->next = pmap;
			engine->pmap_tail = engine->pmap_tail->next;
		}
	}
	
	switch_mutex_unlock(smh->sdp_mutex);

	return pmap;
}




SWITCH_DECLARE(const char *)switch_core_media_get_codec_string(switch_core_session_t *session)
{
	const char *preferred = NULL, *fallback = NULL;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		preferred = "PCMU";
		fallback = "PCMU";
	} else {
	
		if (!(preferred = switch_channel_get_variable(session->channel, "absolute_codec_string"))) {
			preferred = switch_channel_get_variable(session->channel, "codec_string");
		}
	
		if (!preferred) {
			if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				preferred = smh->mparams->outbound_codec_string;
				fallback = smh->mparams->inbound_codec_string;

			} else {
				preferred = smh->mparams->inbound_codec_string;
				fallback = smh->mparams->outbound_codec_string;
			}
		}
	}

	return !zstr(preferred) ? preferred : fallback;
}

SWITCH_DECLARE(void) switch_core_session_clear_crypto(switch_core_session_t *session)
{
	int i;
	switch_media_handle_t *smh;

	const char *vars[] = { "rtp_last_audio_local_crypto_key",
						   "srtp_remote_audio_crypto_key",
						   "srtp_remote_audio_crypto_tag",
						   "srtp_remote_audio_crypto_type",
						   "srtp_remote_video_crypto_key",
						   "srtp_remote_video_crypto_tag",
						   "srtp_remote_video_crypto_type",
						   "rtp_secure_media",
						   "rtp_secure_media_inbound",
						   "rtp_secure_media_outbound",
						   NULL};

	for(i = 0; vars[i] ;i++) {
		switch_channel_set_variable(session->channel, vars[i], NULL);
	}

	if (!(smh = session->media_handle)) {
		return;
	}
	for (i = 0; i < CRYPTO_INVALID; i++) {
		memset(&smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec[i], 0, sizeof(smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec[i]));
		memset(&smh->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec[i], 0, sizeof(smh->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec[i]));
	}

}

SWITCH_DECLARE(const char *) switch_core_session_local_crypto_key(switch_core_session_t *session, switch_media_type_t type)
{
	if (!session->media_handle) {
		return NULL;
	}

	return session->media_handle->engines[type].ssec[session->media_handle->engines[type].crypto_type].local_crypto_key;
}



SWITCH_DECLARE(void) switch_core_media_parse_rtp_bugs(switch_rtp_bug_flag_t *flag_pole, const char *str)
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

	if (switch_stristr("ACCEPT_ANY_PAYLOAD", str)) {
		*flag_pole |= RTP_BUG_ACCEPT_ANY_PAYLOAD;
	}

	if (switch_stristr("~ACCEPT_ANY_PAYLOAD", str)) {
		*flag_pole &= ~RTP_BUG_ACCEPT_ANY_PAYLOAD;
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

	if (switch_stristr("ALWAYS_AUTO_ADJUST", str)) {
		*flag_pole |= (RTP_BUG_ALWAYS_AUTO_ADJUST | RTP_BUG_ACCEPT_ANY_PACKETS);
	}

	if (switch_stristr("~ALWAYS_AUTO_ADJUST", str)) {
		*flag_pole &= ~(RTP_BUG_ALWAYS_AUTO_ADJUST | RTP_BUG_ACCEPT_ANY_PACKETS);
	}
}


static switch_status_t switch_core_media_build_crypto(switch_media_handle_t *smh,
													  switch_media_type_t type,
													  int index, switch_rtp_crypto_key_type_t ctype, switch_rtp_crypto_direction_t direction, int force)
{
	unsigned char b64_key[512] = "";
	unsigned char *key;
	const char *val;
	switch_channel_t *channel;
	char *p;
	switch_rtp_engine_t *engine;

	switch_assert(smh);
	channel = switch_core_session_get_channel(smh->session);

	engine = &smh->engines[type];

	if (!force && engine->ssec[ctype].local_raw_key[0]) {
		return SWITCH_STATUS_SUCCESS;
	}

	

//#define SAME_KEY
#ifdef SAME_KEY
	if (switch_channel_test_flag(channel, CF_WEBRTC) && type == SWITCH_MEDIA_TYPE_VIDEO) {
		if (direction == SWITCH_RTP_CRYPTO_SEND) {
			memcpy(engine->ssec[ctype].local_raw_key, smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec.local_raw_key, SUITES[ctype].keylen);
			key = engine->ssec[ctype].local_raw_key;
		} else {
			memcpy(engine->ssec[ctype].remote_raw_key, smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec.remote_raw_key, SUITES[ctype].keylen);
			key = engine->ssec[ctype].remote_raw_key;
		}
	} else {
#endif
		if (direction == SWITCH_RTP_CRYPTO_SEND) {
			key = engine->ssec[ctype].local_raw_key;
		} else {
			key = engine->ssec[ctype].remote_raw_key;
		}
		
		switch_rtp_get_random(key, SUITES[ctype].keylen);
#ifdef SAME_KEY
	}
#endif

	switch_b64_encode(key, SUITES[ctype].keylen, b64_key, sizeof(b64_key));
	p = strrchr((char *) b64_key, '=');

	while (p && *p && *p == '=') {
		*p-- = '\0';
	}

	if (!index) index = ctype + 1;

	engine->ssec[ctype].local_crypto_key = switch_core_session_sprintf(smh->session, "%d %s inline:%s", index, SUITES[ctype].name, b64_key);
	switch_channel_set_variable_name_printf(smh->session->channel, engine->ssec[ctype].local_crypto_key, "rtp_last_%s_local_crypto_key", type2str(type));
	switch_channel_set_flag(smh->session->channel, CF_SECURE);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "Set Local %s crypto Key [%s]\n", 
					  type2str(type),
					  engine->ssec[ctype].local_crypto_key);

	if (!(smh->mparams->ndlb & SM_NDLB_DISABLE_SRTP_AUTH) &&
		!((val = switch_channel_get_variable(channel, "NDLB_support_asterisk_missing_srtp_auth")) && switch_true(val))) {
		engine->ssec[ctype].crypto_type = ctype;
	} else {
		engine->ssec[ctype].crypto_type = AES_CM_128_NULL_AUTH;
	}

	return SWITCH_STATUS_SUCCESS;
}





switch_status_t switch_core_media_add_crypto(switch_secure_settings_t *ssec, const char *key_str, switch_rtp_crypto_direction_t direction)
{
	unsigned char key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t type;
	char *p;


	p = strchr(key_str, ' ');

	if (p && *p && *(p + 1)) {
		p++;

		type = switch_core_media_crypto_str2type(p);
		
		if (type == CRYPTO_INVALID) {
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
				memcpy(ssec->local_raw_key, key, SUITES[type].keylen);
			} else {
				memcpy(ssec->remote_raw_key, key, SUITES[type].keylen);
			}
			return SWITCH_STATUS_SUCCESS;
		}

	}

 bad:

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");
	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(void) switch_core_media_set_rtp_session(switch_core_session_t *session, switch_media_type_t type, switch_rtp_t *rtp_session)
{
	switch_rtp_engine_t *engine;
	if (!session->media_handle) return;
	engine = &session->media_handle->engines[type];
	engine->rtp_session = rtp_session;
	engine->type = type;
}


static void switch_core_session_get_recovery_crypto_key(switch_core_session_t *session, switch_media_type_t type)
{
	const char *tmp;
	switch_rtp_engine_t *engine;
	char *keyvar, *tagvar, *ctypevar;

	if (!session->media_handle) return;
	engine = &session->media_handle->engines[type];

	if (type == SWITCH_MEDIA_TYPE_AUDIO) {
		keyvar = "srtp_remote_audio_crypto_key";
		tagvar = "srtp_remote_audio_crypto_tag";
		ctypevar = "srtp_remote_audio_crypto_type";
	} else {
		keyvar = "srtp_remote_video_crypto_key";
		tagvar = "srtp_remote_video_crypto_tag";
		ctypevar = "srtp_remote_video_crypto_type";
	}

	if ((tmp = switch_channel_get_variable(session->channel, keyvar))) {
		if ((tmp = switch_channel_get_variable(session->channel, ctypevar))) {
			engine->crypto_type = switch_core_media_crypto_str2type(tmp);
		}

		engine->ssec[engine->crypto_type].remote_crypto_key = switch_core_session_strdup(session, tmp);

		if ((tmp = switch_channel_get_variable(session->channel, tagvar))) {
			int tv = atoi(tmp);
			engine->ssec[engine->crypto_type].crypto_tag = tv;
		} else {
			engine->ssec[engine->crypto_type].crypto_tag = 1;
		}

		switch_channel_set_flag(session->channel, CF_SECURE);
	}
}


static void switch_core_session_apply_crypto(switch_core_session_t *session, switch_media_type_t type)
{
	switch_rtp_engine_t *engine;
	const char *varname;

	if (type == SWITCH_MEDIA_TYPE_AUDIO) {
		varname = "rtp_secure_audio_confirmed";
	} else {
		varname = "rtp_secure_video_confirmed";
	}

	if (!session->media_handle) return;

	engine = &session->media_handle->engines[type];

	if (switch_channel_test_flag(session->channel, CF_RECOVERING)) {
		return;
	}

	if (engine->ssec[engine->crypto_type].remote_crypto_key && switch_channel_test_flag(session->channel, CF_SECURE)) {
		switch_core_media_add_crypto(&engine->ssec[engine->crypto_type], engine->ssec[engine->crypto_type].remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);

		
		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1,
								  engine->ssec[engine->crypto_type].crypto_type, 
								  engine->ssec[engine->crypto_type].local_raw_key, 
								  SUITES[engine->ssec[engine->crypto_type].crypto_type].keylen);

		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, 
								  engine->ssec[engine->crypto_type].crypto_tag,
								  engine->ssec[engine->crypto_type].crypto_type, 
								  engine->ssec[engine->crypto_type].remote_raw_key, 
								  SUITES[engine->ssec[engine->crypto_type].crypto_type].keylen);

		switch_channel_set_variable(session->channel, varname, "true");


		switch_channel_set_variable(session->channel, "rtp_secure_media_negotiated", SUITES[engine->crypto_type].name);

	}

}

static void switch_core_session_parse_crypto_prefs(switch_core_session_t *session)
{
	const char *var = NULL;
	const char *val = NULL;
	char *suites = NULL;
	switch_media_handle_t *smh;
	char *fields[CRYPTO_INVALID+1];
	int argc = 0, i = 0, j = 0, k = 0;
	
	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
		return;
	}

	if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		var = "rtp_secure_media_inbound";
	} else {
		var = "rtp_secure_media_outbound";
	}

	if (!(val = switch_channel_get_variable(session->channel, var))) {
		var = "rtp_secure_media";
		val = switch_channel_get_variable(session->channel, var);
	}
	
	if (!zstr(val) && (suites = strchr(val, ':'))) {
		*suites++ = '\0';
	}

	if (zstr(suites)) {
		suites = (char *) switch_channel_get_variable(session->channel, "rtp_secure_media_suites");
	}

	if (zstr(val)) {
		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND && !switch_channel_test_flag(session->channel, CF_RECOVERING)) {
			val = "optional";
		} else {
			val = "forbidden";
		}
	}

	if (!strcasecmp(val, "optional")) {
		smh->crypto_mode = CRYPTO_MODE_OPTIONAL;
	} else if (switch_true(val) || !strcasecmp(val, "mandatory")) {
		smh->crypto_mode = CRYPTO_MODE_MANDATORY;
	} else {
		smh->crypto_mode = CRYPTO_MODE_FORBIDDEN;
		if (!switch_false(val) && strcasecmp(val, "forbidden")) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "INVALID VALUE FOR %s defaulting to 'forbidden'\n", var);
		}
	}

	if (smh->crypto_mode != CRYPTO_MODE_FORBIDDEN && !zstr(suites)) {
		argc = switch_split((char *)suites, ':', fields);
		
		for (i = 0; i < argc; i++) {
			int ok = 0;
			
			for (j = 0; j < CRYPTO_INVALID; j++) {
				if (!strcasecmp(fields[i], SUITES[j].name)) {
					smh->crypto_suite_order[k++] = SUITES[j].type;
					ok++;
					break;
				}
			}
			
			if (!ok) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "INVALID SUITE SUPPLIED\n");
			}

		}
	} else {
		for (i = 0; i < CRYPTO_INVALID; i++) {
			smh->crypto_suite_order[k++] = SUITES[i].type;
		}
	}
}



SWITCH_DECLARE(int) switch_core_session_check_incoming_crypto(switch_core_session_t *session, 
															   const char *varname,
															  switch_media_type_t type, const char *crypto, int crypto_tag, switch_sdp_type_t sdp_type)
{
	int got_crypto = 0;
	int i = 0;
	int ctype = 0;
	const char *vval = NULL;
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	if (smh->crypto_mode == CRYPTO_MODE_FORBIDDEN) {
		return -1;
	}
	
	engine = &session->media_handle->engines[type];
	
	for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
		switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;
		
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,"looking for crypto suite [%s] in [%s]\n", SUITES[j].name, crypto);
		
		if (switch_stristr(SUITES[j].name, crypto)) {
			ctype = SUITES[j].type;
			vval = SUITES[j].name;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Found suite %s\n", vval);
			switch_channel_set_variable(session->channel, "rtp_secure_media_negotiated", vval);
			break;
		}
	}

	if (engine->ssec[engine->crypto_type].remote_crypto_key && switch_rtp_ready(engine->rtp_session)) {
		/* Compare all the key. The tag may remain the same even if key changed */
		if (crypto && engine->crypto_type != CRYPTO_INVALID && !strcmp(crypto, engine->ssec[engine->crypto_type].remote_crypto_key)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Existing key is still valid.\n");
			got_crypto = 1;
		} else {
			const char *a = switch_stristr("AE", engine->ssec[engine->crypto_type].remote_crypto_key);
			const char *b = switch_stristr("AE", crypto);

			if (sdp_type == SDP_TYPE_REQUEST) {
				if (!vval) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Unsupported Crypto [%s]\n", crypto);
					goto end;
				}
				switch_channel_set_variable(session->channel, varname, vval);

				switch_core_media_build_crypto(session->media_handle, type, crypto_tag, ctype, SWITCH_RTP_CRYPTO_SEND, 1);
				switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), engine->ssec[engine->crypto_type].crypto_type,
										  engine->ssec[engine->crypto_type].local_raw_key, SUITES[ctype].keylen);
			}

			if (a && b && !strncasecmp(a, b, 23)) {
				engine->crypto_type = ctype;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Change Remote key to [%s]\n", crypto);
				engine->ssec[engine->crypto_type].remote_crypto_key = switch_core_session_strdup(session, crypto);
				
				if (engine->type == SWITCH_MEDIA_TYPE_AUDIO) {
					switch_channel_set_variable(session->channel, "srtp_remote_audio_crypto_key", crypto);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_audio_crypto_tag", "%d", crypto_tag);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_audio_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
				} else if (engine->type == SWITCH_MEDIA_TYPE_VIDEO) {
					switch_channel_set_variable(session->channel, "srtp_remote_video_crypto_key", crypto);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_video_crypto_tag", "%d", crypto_tag);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_video_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
				}

				engine->ssec[engine->crypto_type].crypto_tag = crypto_tag;
								
				
				if (switch_rtp_ready(engine->rtp_session) && switch_channel_test_flag(session->channel, CF_SECURE)) {
					switch_core_media_add_crypto(&engine->ssec[engine->crypto_type], engine->ssec[engine->crypto_type].remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
					switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, engine->ssec[engine->crypto_type].crypto_tag,
											  engine->ssec[engine->crypto_type].crypto_type, engine->ssec[engine->crypto_type].remote_raw_key, SUITES[engine->ssec[engine->crypto_type].crypto_type].keylen);
				}
				got_crypto++;
				
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ignoring unacceptable key\n");
			}
		}
	} else if (!switch_rtp_ready(engine->rtp_session)) {

		if (!vval) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Unsupported Crypto [%s]\n", crypto);
			goto end;
		}

		engine->crypto_type = ctype;
		engine->ssec[engine->crypto_type].remote_crypto_key = switch_core_session_strdup(session, crypto);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Remote Key [%s]\n", engine->ssec[engine->crypto_type].remote_crypto_key);
		if (engine->type == SWITCH_MEDIA_TYPE_AUDIO) {
			switch_channel_set_variable(session->channel, "srtp_remote_audio_crypto_key", crypto);
			switch_channel_set_variable_printf(session->channel, "srtp_remote_audio_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
		} else if (engine->type == SWITCH_MEDIA_TYPE_VIDEO) {
			switch_channel_set_variable(session->channel, "srtp_remote_video_crypto_key", crypto);
			switch_channel_set_variable_printf(session->channel, "srtp_remote_video_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
		}

		engine->ssec[engine->crypto_type].crypto_tag = crypto_tag;
		got_crypto++;
		
		switch_channel_set_variable(session->channel, varname, vval);
		switch_channel_set_flag(smh->session->channel, CF_SECURE);

		if (zstr(engine->ssec[engine->crypto_type].local_crypto_key)) {
			switch_core_media_build_crypto(session->media_handle, type, crypto_tag, ctype, SWITCH_RTP_CRYPTO_SEND, 1);
		}
	}

 end:

	return got_crypto;
}


SWITCH_DECLARE(void) switch_core_session_check_outgoing_crypto(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_handle_t *smh;
	int i;

	if (!switch_core_session_media_handle_ready(session) == SWITCH_STATUS_SUCCESS) {
		return;
	}

	if (!(smh = session->media_handle)) {
		return;
	}

	if (!(smh->crypto_mode == CRYPTO_MODE_OPTIONAL || smh->crypto_mode == CRYPTO_MODE_MANDATORY)) {
		return;
	}

	switch_channel_set_flag(channel, CF_SECURE);

	for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
		switch_core_media_build_crypto(session->media_handle,
									   SWITCH_MEDIA_TYPE_AUDIO, 0, smh->crypto_suite_order[i], SWITCH_RTP_CRYPTO_SEND, 0);

		switch_core_media_build_crypto(session->media_handle,
									   SWITCH_MEDIA_TYPE_VIDEO, 0, smh->crypto_suite_order[i], SWITCH_RTP_CRYPTO_SEND, 0);
	}

}

#define add_stat(_i, _s)												\
	switch_snprintf(var_name, sizeof(var_name), "rtp_%s_%s", switch_str_nil(prefix), _s) ; \
	switch_snprintf(var_val, sizeof(var_val), "%" SWITCH_SIZE_T_FMT, _i); \
	switch_channel_set_variable(channel, var_name, var_val)

#define add_stat_double(_i, _s)												\
	switch_snprintf(var_name, sizeof(var_name), "rtp_%s_%s", switch_str_nil(prefix), _s) ; \
	switch_snprintf(var_val, sizeof(var_val), "%0.2f", _i); \
	switch_channel_set_variable(channel, var_name, var_val)

static void set_stats(switch_core_session_t *session, switch_media_type_t type, const char *prefix)
{
	switch_rtp_stats_t *stats = switch_core_media_get_stats(session, type, NULL);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	char var_name[256] = "", var_val[35] = "";

	if (stats) {
		stats->inbound.std_deviation = sqrt(stats->inbound.variance);

		add_stat(stats->inbound.raw_bytes, "in_raw_bytes");
		add_stat(stats->inbound.media_bytes, "in_media_bytes");
		add_stat(stats->inbound.packet_count, "in_packet_count");
		add_stat(stats->inbound.media_packet_count, "in_media_packet_count");
		add_stat(stats->inbound.skip_packet_count, "in_skip_packet_count");
		add_stat(stats->inbound.jb_packet_count, "in_jitter_packet_count");
		add_stat(stats->inbound.dtmf_packet_count, "in_dtmf_packet_count");
		add_stat(stats->inbound.cng_packet_count, "in_cng_packet_count");
		add_stat(stats->inbound.flush_packet_count, "in_flush_packet_count");
		add_stat(stats->inbound.largest_jb_size, "in_largest_jb_size");
		add_stat_double(stats->inbound.min_variance, "in_jitter_min_variance");
		add_stat_double(stats->inbound.max_variance, "in_jitter_max_variance");
		add_stat_double(stats->inbound.lossrate, "in_jitter_loss_rate");
		add_stat_double(stats->inbound.burstrate, "in_jitter_burst_rate");
		add_stat_double(stats->inbound.mean_interval, "in_mean_interval");
		add_stat(stats->inbound.flaws, "in_flaw_total");
		add_stat_double(stats->inbound.R, "in_quality_percentage");
		add_stat_double(stats->inbound.mos, "in_mos");


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

SWITCH_DECLARE(void) switch_core_media_set_stats(switch_core_session_t *session)
{
	
	if (!session->media_handle) {
		return;
	}

	set_stats(session, SWITCH_MEDIA_TYPE_AUDIO, "audio");
	set_stats(session, SWITCH_MEDIA_TYPE_VIDEO, "video");
}



SWITCH_DECLARE(void) switch_media_handle_destroy(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];	

	
	if (switch_core_codec_ready(&a_engine->read_codec)) {
		switch_core_codec_destroy(&a_engine->read_codec);
	}

	if (switch_core_codec_ready(&a_engine->write_codec)) {
		switch_core_codec_destroy(&a_engine->write_codec);
	}

	if (switch_core_codec_ready(&v_engine->read_codec)) {
		switch_core_codec_destroy(&v_engine->read_codec);
	}

	if (switch_core_codec_ready(&v_engine->write_codec)) {
		switch_core_codec_destroy(&v_engine->write_codec);
	}

	switch_core_session_unset_read_codec(session);
	switch_core_session_unset_write_codec(session);
	switch_core_media_deactivate_rtp(session);



}


SWITCH_DECLARE(switch_status_t) switch_media_handle_create(switch_media_handle_t **smhp, switch_core_session_t *session, switch_core_media_params_t *params)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_media_handle_t *smh = NULL;
	int i;

	*smhp = NULL;

	if (zstr(params->sdp_username)) {
		params->sdp_username = "FreeSWITCH";
	}


	if ((session->media_handle = switch_core_session_alloc(session, (sizeof(*smh))))) {
		session->media_handle->session = session;


		*smhp = session->media_handle;
		switch_set_flag(session->media_handle, SMF_INIT);
		session->media_handle->media_flags[SCMF_RUNNING] = 1;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].type = SWITCH_MEDIA_TYPE_AUDIO;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].crypto_type = CRYPTO_INVALID;

		for (i = 0; i < CRYPTO_INVALID; i++) {
			session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec[i].crypto_type = i;
		}

		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].type = SWITCH_MEDIA_TYPE_VIDEO;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].crypto_type = CRYPTO_INVALID;

		for (i = 0; i < CRYPTO_INVALID; i++) {
			session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec[i].crypto_type = i;
		}

		session->media_handle->mparams = params;

		for (i = 0; i <= CRYPTO_INVALID; i++) {
			session->media_handle->crypto_suite_order[i] = CRYPTO_INVALID;
		}

		switch_mutex_init(&session->media_handle->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_mutex_init(&session->media_handle->sdp_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].ssrc = 
			(uint32_t) ((intptr_t) &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO] + (uint32_t) time(NULL));

		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssrc = 
			(uint32_t) ((intptr_t) &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO] + (uint32_t) time(NULL) / 2);

		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].payload_map = switch_core_alloc(session->pool, sizeof(payload_map_t));
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].cur_payload_map = session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].payload_map;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].cur_payload_map->current = 1;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].payload_map = switch_core_alloc(session->pool, sizeof(payload_map_t));
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].cur_payload_map = session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].payload_map;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].cur_payload_map->current = 1;

		switch_channel_set_flag(session->channel, CF_DTLS_OK);

		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}

SWITCH_DECLARE(void) switch_media_handle_set_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag)
{
	switch_assert(smh);

	smh->media_flags[flag] = 1;
	
}

SWITCH_DECLARE(void) switch_media_handle_set_media_flags(switch_media_handle_t *smh, switch_core_media_flag_t flags[SCMF_MAX])
{
	int i;
	switch_assert(smh);

	for(i = 0; i < SCMF_MAX; i++) {
		if (flags[i]) {
			smh->media_flags[i] = flags[i];
		}
	}
	
}

SWITCH_DECLARE(void) switch_media_handle_clear_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag)
{
	switch_assert(smh);

	smh->media_flags[flag] = 0;
}

SWITCH_DECLARE(int32_t) switch_media_handle_test_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag)
{
	switch_assert(smh);
	return smh->media_flags[flag];
}

SWITCH_DECLARE(switch_status_t) switch_core_session_media_handle_ready(switch_core_session_t *session)
{
	if (session->media_handle && switch_test_flag(session->media_handle, SMF_INIT)) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}



SWITCH_DECLARE(switch_media_handle_t *) switch_core_session_get_media_handle(switch_core_session_t *session)
{
	if (switch_core_session_media_handle_ready(session) == SWITCH_STATUS_SUCCESS) {
		return session->media_handle;
	}

	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_clear_media_handle(switch_core_session_t *session)
{
	if (!session->media_handle) {
		return SWITCH_STATUS_FALSE;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_core_media_params_t *) switch_core_media_get_mparams(switch_media_handle_t *smh)
{
	switch_assert(smh);
	return smh->mparams;
}

SWITCH_DECLARE(void) switch_core_media_prepare_codecs(switch_core_session_t *session, switch_bool_t force)
{
	const char *abs, *codec_string = NULL;
	const char *ocodec = NULL;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (!force && (switch_channel_test_flag(session->channel, CF_PROXY_MODE) || switch_channel_test_flag(session->channel, CF_PROXY_MEDIA))) {
		return;
	}

	if (force) {
		smh->mparams->num_codecs = 0;
	}

	if (smh->mparams->num_codecs) {
		return;
	}

	smh->payload_space = 0;

	switch_assert(smh->session != NULL);

	if ((abs = switch_channel_get_variable(session->channel, "absolute_codec_string"))) {
		codec_string = abs;
		goto ready;
	}

	if (!(codec_string = switch_channel_get_variable(session->channel, "codec_string"))) {
		codec_string = switch_core_media_get_codec_string(smh->session);
	}

	if (codec_string && *codec_string == '=') {
		codec_string++;
		goto ready;
	}

	if ((ocodec = switch_channel_get_variable(session->channel, SWITCH_ORIGINATOR_CODEC_VARIABLE))) {
		if (!codec_string || (smh->media_flags[SCMF_DISABLE_TRANSCODING])) {
			codec_string = ocodec;
		} else {
			if (!(codec_string = switch_core_session_sprintf(smh->session, "%s,%s", ocodec, codec_string))) {
				codec_string = ocodec;
			}
		}
	}

 ready:
	if (codec_string) {
		char *tmp_codec_string = switch_core_session_strdup(smh->session, codec_string);


		switch_channel_set_variable(session->channel, "rtp_use_codec_string", codec_string);
		smh->codec_order_last = switch_separate_string(tmp_codec_string, ',', smh->codec_order, SWITCH_MAX_CODECS);
		smh->mparams->num_codecs = switch_loadable_module_get_codecs_sorted(smh->codecs, SWITCH_MAX_CODECS, smh->codec_order, smh->codec_order_last);
	} else {
		smh->mparams->num_codecs = switch_loadable_module_get_codecs(smh->codecs, sizeof(smh->codecs) / sizeof(smh->codecs[0]));
	}
}


static void check_jb(switch_core_session_t *session, const char *input)
{
	const char *val;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if (!a_engine->rtp_session) return;


	if (!zstr(input)) {
		const char *s;

		if (!strcasecmp(input, "pause")) {
			switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_TRUE);
			return;
		} else if (!strcasecmp(input, "resume")) {
			switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_FALSE);
			return;
		} else if (!strcasecmp(input, "stop")) {
			switch_rtp_deactivate_jitter_buffer(a_engine->rtp_session);
			return;
		} else if (!strncasecmp(input, "debug:", 6)) {
			s = input + 6;
			if (s && !strcmp(s, "off")) {
				s = NULL;
			}
			switch_rtp_debug_jitter_buffer(a_engine->rtp_session, s);
			return;
		}

		switch_channel_set_variable(session->channel, "jitterbuffer_msec", input);
	}
	

	if ((val = switch_channel_get_variable(session->channel, "jitterbuffer_msec")) || (val = smh->mparams->jb_msec)) {
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

		if (jb_msec < 0 && jb_msec > -20) {
			jb_msec = (a_engine->read_codec.implementation->microseconds_per_packet / 1000) * abs(jb_msec);
		}

		if (maxlen < 0 && maxlen > -20) {
			maxlen = (a_engine->read_codec.implementation->microseconds_per_packet / 1000) * abs(maxlen);
		}

		if (jb_msec < 10 || jb_msec > 10000) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
							  "Invalid Jitterbuffer spec [%d] must be between 10 and 10000\n", jb_msec);
		} else {
			int qlen, maxqlen = 10;
				
			qlen = jb_msec / (a_engine->read_impl.microseconds_per_packet / 1000);

			if (maxlen) {
				maxqlen = maxlen / (a_engine->read_impl.microseconds_per_packet / 1000);
			}

			if (maxqlen < qlen) {
				maxqlen = qlen * 5;
			}
			if (switch_rtp_activate_jitter_buffer(a_engine->rtp_session, qlen, maxqlen,
												  a_engine->read_impl.samples_per_packet, 
												  a_engine->read_impl.samples_per_second, max_drift) == SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), 
										  SWITCH_LOG_DEBUG, "Setting Jitterbuffer to %dms (%d frames) (%d max frames) (%d max drift)\n", 
										  jb_msec, qlen, maxqlen, max_drift);
						switch_channel_set_flag(session->channel, CF_JITTERBUFFER);
				if (!switch_false(switch_channel_get_variable(session->channel, "rtp_jitter_buffer_plc"))) {
					switch_channel_set_flag(session->channel, CF_JITTERBUFFER_PLC);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), 
								  SWITCH_LOG_WARNING, "Error Setting Jitterbuffer to %dms (%d frames)\n", jb_msec, qlen);
			}
				
		}
	}

}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_read_frame(switch_core_session_t *session, switch_frame_t **frame,
															 switch_io_flag_t flags, int stream_id, switch_media_type_t type)
{
	switch_rtcp_frame_t rtcp_frame;
	switch_rtp_engine_t *engine;
	switch_status_t status;
	switch_media_handle_t *smh;
	int do_cng = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!smh->media_flags[SCMF_RUNNING]) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	engine->read_frame.datalen = 0;

	if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(engine->rtp_session != NULL);
	engine->read_frame.datalen = 0;

	if (!switch_channel_up_nosig(session->channel) || !switch_rtp_ready(engine->rtp_session) || switch_channel_test_flag(session->channel, CF_NOT_READY)) {
		return SWITCH_STATUS_FALSE;
	}

	if (engine->read_mutex[type] && switch_mutex_trylock(engine->read_mutex[type]) != SWITCH_STATUS_SUCCESS) {
		/* return CNG, another thread is already reading  */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s is already being read for %s\n", 
						  switch_channel_get_name(session->channel), type2str(type));
		return SWITCH_STATUS_INUSE;
	}

	
	while (smh->media_flags[SCMF_RUNNING] && engine->read_frame.datalen == 0) {
		engine->read_frame.flags = SFF_NONE;

		status = switch_rtp_zerocopy_read_frame(engine->rtp_session, &engine->read_frame, flags);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			if (status == SWITCH_STATUS_TIMEOUT) {

				if (switch_channel_get_variable(session->channel, "execute_on_media_timeout")) {
					*frame = &engine->read_frame;
					switch_set_flag((*frame), SFF_CNG);
					(*frame)->datalen = engine->read_impl.encoded_bytes_per_packet;
					memset((*frame)->data, 0, (*frame)->datalen);
					switch_channel_execute_on(session->channel, "execute_on_media_timeout");
					switch_goto_status(SWITCH_STATUS_SUCCESS, end);
				}


				switch_channel_hangup(session->channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
			}
			goto end;
		}

		/* re-set codec if necessary */
		if (engine->reset_codec > 0) {
			const char *val;
			int rtp_timeout_sec = 0;
			int rtp_hold_timeout_sec = 0;

			engine->reset_codec = 0;
					
			if (switch_rtp_ready(engine->rtp_session)) {
				if (type == SWITCH_MEDIA_TYPE_VIDEO) {
					switch_core_media_set_video_codec(session, 1);
				} else {
					if (switch_core_media_set_codec(session, 1, 0) != SWITCH_STATUS_SUCCESS) {
						*frame = NULL;
						switch_goto_status(SWITCH_STATUS_GENERR, end);
					}
				}

				if ((val = switch_channel_get_variable(session->channel, "rtp_timeout_sec"))) {
					int v = atoi(val);
					if (v >= 0) {
						rtp_timeout_sec = v;
					}
				}
				
				if ((val = switch_channel_get_variable(session->channel, "rtp_hold_timeout_sec"))) {
					int v = atoi(val);
					if (v >= 0) {
						rtp_hold_timeout_sec = v;
					}
				}

				if (rtp_timeout_sec) {
					engine->max_missed_packets = (engine->read_impl.samples_per_second * rtp_timeout_sec) /
						engine->read_impl.samples_per_packet;

					switch_rtp_set_max_missed_packets(engine->rtp_session, engine->max_missed_packets);
					if (!rtp_hold_timeout_sec) {
						rtp_hold_timeout_sec = rtp_timeout_sec * 10;
					}
				}

				if (rtp_hold_timeout_sec) {
					engine->max_missed_hold_packets = (engine->read_impl.samples_per_second * rtp_hold_timeout_sec) /
						engine->read_impl.samples_per_packet;
				}
			}

			check_jb(session, NULL);

			engine->check_frames = 0;
			engine->last_ts = 0;
			engine->last_seq = 0;

			do_cng = 1;
		}

		if (do_cng) {
			/* return CNG for now */
			*frame = &engine->read_frame;
			switch_set_flag((*frame), SFF_CNG);
			(*frame)->datalen = engine->read_impl.encoded_bytes_per_packet;
			memset((*frame)->data, 0, (*frame)->datalen);
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
				

		/* Try to read an RTCP frame, if successful raise an event */
		if (switch_rtcp_zerocopy_read_frame(engine->rtp_session, &rtcp_frame) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event;

			if (switch_event_create(&event, SWITCH_EVENT_RECV_RTCP_MESSAGE) == SWITCH_STATUS_SUCCESS) {
				char value[30];
				char header[50];
				int i;

				char *uuid = switch_core_session_get_uuid(session);
				if (uuid) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(session));
				}

				snprintf(value, sizeof(value), "%.8x", rtcp_frame.ssrc);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "SSRC", value);

				snprintf(value, sizeof(value), "%u", rtcp_frame.ntp_msw);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "NTP-Most-Significant-Word", value);

				snprintf(value, sizeof(value), "%u", rtcp_frame.ntp_lsw);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "NTP-Least-Significant-Word", value);

				snprintf(value, sizeof(value), "%u", rtcp_frame.timestamp);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTP-Timestamp", value);

				snprintf(value, sizeof(value), "%u", rtcp_frame.packet_count);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Sender-Packet-Count", value);

				snprintf(value, sizeof(value), "%u", rtcp_frame.octect_count);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Octect-Packet-Count", value);

				snprintf(value, sizeof(value), "%" SWITCH_SIZE_T_FMT, engine->read_frame.timestamp);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Last-RTP-Timestamp", value);

				snprintf(value, sizeof(value), "%u", engine->read_frame.rate);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "RTP-Rate", value);

				snprintf(value, sizeof(value), "%" SWITCH_TIME_T_FMT, switch_time_now());
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Capture-Time", value);

				// Add sources info
				for (i = 0; i < rtcp_frame.report_count; i++) {
					snprintf(header, sizeof(header), "Source%u-SSRC", i);
					snprintf(value, sizeof(value), "%.8x", rtcp_frame.reports[i].ssrc);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-Fraction", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].fraction);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-Lost", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].lost);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-Highest-Sequence-Number-Received", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].highest_sequence_number_received);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-Jitter", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].jitter);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-LSR", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].lsr);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
					snprintf(header, sizeof(header), "Source%u-DLSR", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].dlsr);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header, value);
				}

				switch_event_fire(&event);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Dispatched RTCP event\n");
			}
		}

		/* Fast PASS! */
		if (switch_test_flag((&engine->read_frame), SFF_PROXY_PACKET)) {
			*frame = &engine->read_frame;
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}

		if (switch_rtp_has_dtmf(engine->rtp_session)) {
			switch_dtmf_t dtmf = { 0 };
			switch_rtp_dequeue_dtmf(engine->rtp_session, &dtmf);
			switch_channel_queue_dtmf(session->channel, &dtmf);
		}
		
		if (engine->read_frame.datalen > 0) {
			uint32_t bytes = 0;
			int frames = 1;

			/* autofix timing */
			if (!switch_test_flag((&engine->read_frame), SFF_CNG)) {
				if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
					*frame = NULL;
					switch_goto_status(SWITCH_STATUS_GENERR, end);
				}
				
				/* check for timing issues */
				if (smh->media_flags[SCMF_AUTOFIX_TIMING] && engine->check_frames < MAX_CODEC_CHECK_FRAMES) {


					engine->check_frames++;

					if (!engine->read_impl.encoded_bytes_per_packet) {
						engine->check_frames = MAX_CODEC_CHECK_FRAMES;
						goto skip;
					}
					
					if (smh->media_flags[SCMF_AUTOFIX_TIMING] && (engine->read_frame.datalen % 10) == 0) {
						
						if (engine->last_ts && engine->read_frame.datalen != engine->read_impl.encoded_bytes_per_packet) {

							uint32_t codec_ms = (int) (engine->read_frame.timestamp -
													   engine->last_ts) / (engine->read_impl.samples_per_second / 1000);

							if (engine->last_seq && (int) (engine->read_frame.seq - engine->last_seq) > 1) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Correcting calculated ptime value from %d to %d to compensate for %d lost packet(s)\n", codec_ms, codec_ms / (int) (engine->read_frame.seq - engine->last_seq), (int) (engine->read_frame.seq - engine->last_seq - 1));
								codec_ms = codec_ms / (int) (engine->read_frame.seq - engine->last_seq);
							}

							if ((codec_ms % 10) != 0 || codec_ms > engine->read_impl.samples_per_packet * 10) {
								engine->last_ts = 0;
								engine->last_seq = 0;
								goto skip;
							}


							if (engine->last_codec_ms && engine->last_codec_ms == codec_ms) {
								engine->mismatch_count++;
							}

							engine->last_codec_ms = codec_ms;

							if (engine->mismatch_count > MAX_MISMATCH_FRAMES) {
								if (codec_ms != engine->cur_payload_map->codec_ms) {

									if (codec_ms > 120) {	/* yeah right */
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "Your phone is trying to send timestamps that suggest an increment of %dms per packet\n"
														  "That seems hard to believe so I am going to go on ahead and um ignore that, mmkay?\n",
														  (int) codec_ms);
										engine->check_frames = MAX_CODEC_CHECK_FRAMES;
										goto skip;
									}

									engine->read_frame.datalen = 0;

									if (codec_ms != engine->cur_payload_map->codec_ms) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "Asynchronous PTIME not supported, changing our end from %d to %d\n",
														  (int) engine->cur_payload_map->codec_ms,
														  (int) codec_ms
														  );

										switch_channel_set_variable_printf(session->channel, "rtp_h_X-Broken-PTIME", "Adv=%d;Sent=%d",
																		   (int) engine->cur_payload_map->codec_ms, (int) codec_ms);

										engine->cur_payload_map->codec_ms = codec_ms;

										/* mark to re-set codec */
										engine->reset_codec = 2;
									}
								}
							}

						} else {
							engine->mismatch_count = 0;
						}

						engine->last_ts = engine->read_frame.timestamp;
						engine->last_seq = engine->read_frame.seq;


					} else {
						engine->mismatch_count = 0;
						engine->last_ts = 0;
						engine->last_seq = 0;
					}
				}

				/* autofix payload type */

				if (!engine->reset_codec &&
					engine->codec_negotiated &&
					(!smh->mparams->cng_pt || engine->read_frame.payload != smh->mparams->cng_pt) &&
					(!smh->mparams->recv_te || engine->read_frame.payload != smh->mparams->recv_te) &&
					(!smh->mparams->te || engine->read_frame.payload != smh->mparams->te) &&
					engine->read_frame.payload != engine->cur_payload_map->recv_pt &&
					engine->read_frame.payload != engine->cur_payload_map->agreed_pt &&
					engine->read_frame.payload != engine->cur_payload_map->pt) {
					
					payload_map_t *pmap;
					

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "alternate payload received (received %d, expecting %d)\n",
									  (int) engine->read_frame.payload, (int) engine->cur_payload_map->agreed_pt);


					/* search for payload type */
					switch_mutex_lock(smh->sdp_mutex);
					for (pmap = engine->payload_map; pmap; pmap = pmap->next) {
						if (engine->read_frame.payload == pmap->recv_pt && pmap->negotiated) {
							engine->cur_payload_map = pmap;
							engine->cur_payload_map->current = 1;
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
											  "Changing current codec to %s (payload type %d).\n",
											  pmap->iananame, pmap->pt);
								
							/* mark to re-set codec */
							engine->reset_codec = 1;
							break;
						}
					}
					switch_mutex_unlock(smh->sdp_mutex);
				
					if (!engine->reset_codec) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
										  "Could not change to payload type %d, ignoring...\n",
										  (int) engine->read_frame.payload);
					}
				}

			skip:

				if ((bytes = engine->read_impl.encoded_bytes_per_packet)) {
					frames = (engine->read_frame.datalen / bytes);
				}
				engine->read_frame.samples = (int) (frames * engine->read_impl.samples_per_packet);

				if (engine->read_frame.datalen == 0) {
					continue;
				}
			}
			break;
		}
	}
	
	if (engine->read_frame.datalen == 0) {
		*frame = NULL;
	}

	*frame = &engine->read_frame;

	status = SWITCH_STATUS_SUCCESS;

 end:

	if (engine->read_mutex[type]) {
		switch_mutex_unlock(engine->read_mutex[type]);
	}

	return status;
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_write_frame(switch_core_session_t *session, 
												  switch_frame_t *frame, switch_io_flag_t flags, int stream_id, switch_media_type_t type)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int bytes = 0, samples = 0, frames = 0;
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!smh->media_flags[SCMF_RUNNING]) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	while (!(engine->read_codec.implementation && switch_rtp_ready(engine->rtp_session))) {
		if (switch_channel_ready(session->channel)) {
			switch_yield(10000);
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
		return SWITCH_STATUS_GENERR;
	}


	if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(frame, SFF_CNG) && !switch_test_flag(frame, SFF_PROXY_PACKET)) {
		if (engine->read_impl.encoded_bytes_per_packet) {
			bytes = engine->read_impl.encoded_bytes_per_packet;
			frames = ((int) frame->datalen / bytes);
		} else
			frames = 1;

		samples = frames * engine->read_impl.samples_per_packet;
	}

	engine->timestamp_send += samples;

	if (switch_rtp_write_frame(engine->rtp_session, frame) < 0) {
		status = SWITCH_STATUS_FALSE;
	}


	return status;
}


//?
SWITCH_DECLARE(void) switch_core_media_copy_t38_options(switch_t38_options_t *t38_options, switch_core_session_t *session)
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

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_get_offered_pt(switch_core_session_t *session, const switch_codec_implementation_t *mimp, switch_payload_t *pt)
{
	int i = 0;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle) || !mimp) {
		return SWITCH_STATUS_FALSE;
	}


	for (i = 0; i < smh->mparams->num_codecs; i++) {
		const switch_codec_implementation_t *imp = smh->codecs[i];

		if (!strcasecmp(imp->iananame, mimp->iananame) && imp->actual_samples_per_second == mimp->actual_samples_per_second) {
			*pt = smh->ianacodes[i];

			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}



//?
SWITCH_DECLARE(switch_status_t) switch_core_media_set_video_codec(switch_core_session_t *session, int force)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];


	if (!v_engine->codec_negotiated) {
		return SWITCH_STATUS_FALSE;
	}

	if (v_engine->read_codec.implementation && switch_core_codec_ready(&v_engine->read_codec)) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(v_engine->read_codec.implementation->iananame, v_engine->cur_payload_map->rm_encoding) ||
			v_engine->read_codec.implementation->samples_per_second != v_engine->cur_payload_map->rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  v_engine->read_codec.implementation->iananame, v_engine->cur_payload_map->rm_encoding);
			switch_core_codec_destroy(&v_engine->read_codec);
			switch_core_codec_destroy(&v_engine->write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already using %s\n",
							  v_engine->read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}



	if (switch_core_codec_init(&v_engine->read_codec,
							   v_engine->cur_payload_map->rm_encoding,
							   v_engine->cur_payload_map->rm_fmtp,
							   v_engine->cur_payload_map->rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&v_engine->write_codec,
								   v_engine->cur_payload_map->rm_encoding,
								   v_engine->cur_payload_map->rm_fmtp,
								   v_engine->cur_payload_map->rm_rate,
								   0,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			v_engine->read_frame.rate = v_engine->cur_payload_map->rm_rate;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(session->channel), v_engine->cur_payload_map->rm_encoding, 
							  v_engine->cur_payload_map->rm_rate, v_engine->cur_payload_map->codec_ms);
			v_engine->read_frame.codec = &v_engine->read_codec;

			v_engine->write_codec.fmtp_out = switch_core_session_strdup(session, v_engine->write_codec.fmtp_out);

			v_engine->write_codec.agreed_pt = v_engine->cur_payload_map->agreed_pt;
			v_engine->read_codec.agreed_pt = v_engine->cur_payload_map->agreed_pt;
			switch_core_session_set_video_read_codec(session, &v_engine->read_codec);
			switch_core_session_set_video_write_codec(session, &v_engine->write_codec);


			switch_channel_set_variable_printf(session->channel, "rtp_last_video_codec_string", "%s@%dh", 
											   v_engine->cur_payload_map->rm_encoding, v_engine->cur_payload_map->rm_rate);


			if (switch_rtp_ready(v_engine->rtp_session)) {
				switch_core_session_message_t msg = { 0 };

				msg.from = __FILE__;
				msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;

				switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->agreed_pt);
				
				//XX

				switch_core_session_receive_message(session, &msg);


			}
			
			switch_channel_set_variable(session->channel, "rtp_use_video_codec_name", v_engine->cur_payload_map->rm_encoding);
			switch_channel_set_variable(session->channel, "rtp_use_video_codec_fmtp", v_engine->cur_payload_map->rm_fmtp);
			switch_channel_set_variable_printf(session->channel, "rtp_use_video_codec_rate", "%d", v_engine->cur_payload_map->rm_rate);
			switch_channel_set_variable_printf(session->channel, "rtp_use_video_codec_ptime", "%d", 0);

		}
	}
	return SWITCH_STATUS_SUCCESS;
}


//?
SWITCH_DECLARE(switch_status_t) switch_core_media_set_codec(switch_core_session_t *session, int force, uint32_t codec_flags)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int resetting = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}
	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if (!a_engine->cur_payload_map->iananame) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No audio codec available\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_ready(&a_engine->read_codec)) {
		if (!force) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
		if (strcasecmp(a_engine->read_impl.iananame, a_engine->cur_payload_map->iananame) ||
			(uint32_t) a_engine->read_impl.microseconds_per_packet / 1000 != a_engine->cur_payload_map->codec_ms ||
			a_engine->read_impl.samples_per_second != a_engine->cur_payload_map->rm_rate ) {
			
			if (session->read_resampler) {
				switch_mutex_lock(session->resample_mutex);
				switch_resample_destroy(&session->read_resampler);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Deactivating read resampler\n");
				switch_mutex_unlock(session->resample_mutex);
			}

			if (session->write_resampler) {
				switch_mutex_lock(session->resample_mutex);
				switch_resample_destroy(&session->write_resampler);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Deactivating write resampler\n");
				switch_mutex_unlock(session->resample_mutex);
			}

			switch_core_session_reset(session, 0, 0);
			switch_channel_audio_sync(session->channel);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
							  "Changing Codec from %s@%dms@%dhz to %s@%dms@%luhz\n",
							  a_engine->read_impl.iananame, 
							  a_engine->read_impl.microseconds_per_packet / 1000,
							  a_engine->read_impl.samples_per_second,

							  a_engine->cur_payload_map->iananame, 
							  a_engine->cur_payload_map->codec_ms,
							  a_engine->cur_payload_map->rm_rate);
			
			switch_yield(a_engine->read_impl.microseconds_per_packet);
			switch_core_session_lock_codec_write(session);
			switch_core_session_lock_codec_read(session);
			resetting = 1;
			switch_yield(a_engine->read_impl.microseconds_per_packet);
			switch_core_codec_destroy(&a_engine->read_codec);
			switch_core_codec_destroy(&a_engine->write_codec);
			switch_channel_audio_sync(session->channel);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already using %s\n", a_engine->read_impl.iananame);
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
	}

	if (switch_core_codec_init_with_bitrate(&a_engine->read_codec,
											a_engine->cur_payload_map->iananame,
											a_engine->cur_payload_map->rm_fmtp,
											a_engine->cur_payload_map->rm_rate,
											a_engine->cur_payload_map->codec_ms,
											a_engine->cur_payload_map->channels,
											a_engine->cur_payload_map->bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	
	a_engine->read_codec.session = session;


	if (switch_core_codec_init_with_bitrate(&a_engine->write_codec,
											a_engine->cur_payload_map->iananame,
											a_engine->cur_payload_map->rm_fmtp,
											a_engine->cur_payload_map->rm_rate,
											a_engine->cur_payload_map->codec_ms,
											a_engine->cur_payload_map->channels,
											a_engine->cur_payload_map->bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	a_engine->write_codec.session = session;

	switch_channel_set_variable(session->channel, "rtp_use_codec_name", a_engine->cur_payload_map->iananame);
	switch_channel_set_variable(session->channel, "rtp_use_codec_fmtp", a_engine->cur_payload_map->rm_fmtp);
	switch_channel_set_variable_printf(session->channel, "rtp_use_codec_rate", "%d", a_engine->cur_payload_map->rm_rate);
	switch_channel_set_variable_printf(session->channel, "rtp_use_codec_ptime", "%d", a_engine->cur_payload_map->codec_ms);
	switch_channel_set_variable_printf(session->channel, "rtp_use_codec_channels", "%d", a_engine->cur_payload_map->channels);
	switch_channel_set_variable_printf(session->channel, "rtp_last_audio_codec_string", "%s@%dh@%di@%dc", 
									   a_engine->cur_payload_map->iananame, a_engine->cur_payload_map->rm_rate, a_engine->cur_payload_map->codec_ms, a_engine->cur_payload_map->channels);

	switch_assert(a_engine->read_codec.implementation);
	switch_assert(a_engine->write_codec.implementation);

	a_engine->read_impl = *a_engine->read_codec.implementation;
	a_engine->write_impl = *a_engine->write_codec.implementation;

	switch_core_session_set_read_impl(session, a_engine->read_codec.implementation);
	switch_core_session_set_write_impl(session, a_engine->write_codec.implementation);

	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_assert(a_engine->read_codec.implementation);
		
		if (switch_rtp_change_interval(a_engine->rtp_session,
									   a_engine->read_impl.microseconds_per_packet, 
									   a_engine->read_impl.samples_per_packet) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	a_engine->read_frame.rate = a_engine->cur_payload_map->rm_rate;

	if (!switch_core_codec_ready(&a_engine->read_codec)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples %d bits %d channels\n",
					  switch_channel_get_name(session->channel), a_engine->cur_payload_map->iananame, a_engine->cur_payload_map->rm_rate, 
					  a_engine->cur_payload_map->codec_ms,
					  a_engine->read_impl.samples_per_packet, a_engine->read_impl.bits_per_second, a_engine->read_impl.number_of_channels);
	a_engine->read_frame.codec = &a_engine->read_codec;
	a_engine->read_frame.channels = a_engine->read_impl.number_of_channels;
	a_engine->write_codec.agreed_pt = a_engine->cur_payload_map->agreed_pt;
	a_engine->read_codec.agreed_pt = a_engine->cur_payload_map->agreed_pt;

	if (force != 2) {
		switch_core_session_set_real_read_codec(session, &a_engine->read_codec);
		switch_core_session_set_write_codec(session, &a_engine->write_codec);
	}

	a_engine->cur_payload_map->fmtp_out = switch_core_session_strdup(session, a_engine->write_codec.fmtp_out);

	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_rtp_set_default_payload(a_engine->rtp_session, a_engine->cur_payload_map->pt);
	}

 end:

	if (resetting) {
		switch_core_session_unlock_codec_write(session);
		switch_core_session_unlock_codec_read(session);
	}

	return status;
}
static void clear_ice(switch_core_session_t *session, switch_media_type_t type) 
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

	engine->ice_in.chosen[0] = 0;
	engine->ice_in.chosen[1] = 0;
	engine->ice_in.cand_idx = 0;
	memset(&engine->ice_in, 0, sizeof(engine->ice_in));
	engine->remote_rtcp_port = 0;

	if (engine->rtp_session) {
		switch_rtp_reset(engine->rtp_session);
	}

}

//?
SWITCH_DECLARE(void) switch_core_media_clear_ice(switch_core_session_t *session)
{
	clear_ice(session, SWITCH_MEDIA_TYPE_AUDIO);
	clear_ice(session, SWITCH_MEDIA_TYPE_VIDEO);

}

SWITCH_DECLARE(void) switch_core_media_pause(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (a_engine->rtp_session) {
		switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}
	
	if (v_engine->rtp_session) {
		switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}
}

SWITCH_DECLARE(void) switch_core_media_resume(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (a_engine->rtp_session) {
		switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}
	
	if (v_engine->rtp_session) {
		switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}
}


//?
SWITCH_DECLARE(switch_status_t) switch_core_media_add_ice_acl(switch_core_session_t *session, switch_media_type_t type, const char *acl_name)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	if (engine->cand_acl_count < SWITCH_MAX_CAND_ACL) {
		engine->cand_acl[engine->cand_acl_count++] = switch_core_session_strdup(session, acl_name);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

//?
SWITCH_DECLARE(void) switch_core_media_check_video_codecs(switch_core_session_t *session)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (smh->mparams->num_codecs && !switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE)) {
		int i;
		smh->video_count = 0;
		for (i = 0; i < smh->mparams->num_codecs; i++) {
			
			if (smh->codecs[i]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
					switch_channel_test_flag(session->channel, CF_NOVIDEO)) {
					continue;
				}
				smh->video_count++;
			}
		}
		if (smh->video_count) {
			switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
		}
	}
}

//?
static void generate_local_fingerprint(switch_media_handle_t *smh, switch_media_type_t type)
{
	switch_rtp_engine_t *engine = &smh->engines[type];

	if (!engine->local_dtls_fingerprint.len) {
		if (engine->remote_dtls_fingerprint.type) {
			engine->local_dtls_fingerprint.type = engine->remote_dtls_fingerprint.type;
		} else {
			engine->local_dtls_fingerprint.type = "sha-256";
		}
		switch_core_cert_gen_fingerprint(DTLS_SRTP_FNAME, &engine->local_dtls_fingerprint);
	}
}

//?
static int dtls_ok(switch_core_session_t *session)
{
	return switch_channel_test_flag(session->channel, CF_DTLS_OK);
}

#ifdef _MSC_VER
/* remove this if the break is removed from the following for loop which causes unreachable code loop */
/* for (i = 0; i < engine->cand_acl_count; i++) { */
#pragma warning(push)
#pragma warning(disable:4702)
#endif

//?
SWITCH_DECLARE(switch_call_direction_t) switch_ice_direction(switch_core_session_t *session)
{
	switch_call_direction_t r = switch_channel_direction(session->channel);

	if (switch_channel_test_flag(session->channel, CF_3PCC)) {
		r = (r == SWITCH_CALL_DIRECTION_INBOUND) ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND;
	}

	if ((switch_channel_test_flag(session->channel, CF_REINVITE) || switch_channel_test_flag(session->channel, CF_RECOVERING)) 
		&& switch_channel_test_flag(session->channel, CF_WEBRTC)) {
		r = SWITCH_CALL_DIRECTION_OUTBOUND;
	}

	return r;
}

//?
static void check_ice(switch_media_handle_t *smh, switch_media_type_t type, sdp_session_t *sdp, sdp_media_t *m)
{
	switch_rtp_engine_t *engine = &smh->engines[type];
	sdp_attribute_t *attr;
	int i = 0, got_rtcp_mux = 0;
	const char *val;

	if (engine->ice_in.chosen[0] && engine->ice_in.chosen[1] && !switch_channel_test_flag(smh->session->channel, CF_REINVITE)) {
		return;
	}

	engine->ice_in.chosen[0] = 0;
	engine->ice_in.chosen[1] = 0;
	engine->ice_in.cand_idx = 0;

	if (m) {
		attr = m->m_attributes;
	} else {
		attr = sdp->sdp_attributes;
	}

	for (; attr; attr = attr->a_next) {
		char *data;
		char *fields[15];
		int argc = 0, j = 0;
		int cid = 0;

		if (zstr(attr->a_name)) {
			continue;
		}

		if (!strcasecmp(attr->a_name, "ice-ufrag")) {
			engine->ice_in.ufrag = switch_core_session_strdup(smh->session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "ice-pwd")) {
			engine->ice_in.pwd = switch_core_session_strdup(smh->session, attr->a_value);
		} else if (!strcasecmp(attr->a_name, "ice-options")) {
			engine->ice_in.options = switch_core_session_strdup(smh->session, attr->a_value);
			
		} else if (switch_rtp_has_dtls() && dtls_ok(smh->session) && !strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
			char *p;

			engine->remote_dtls_fingerprint.type = switch_core_session_strdup(smh->session, attr->a_value);
			
			if ((p = strchr(engine->remote_dtls_fingerprint.type, ' '))) {
				*p++ = '\0';
				switch_set_string(engine->local_dtls_fingerprint.str, p);
			}
			
			//if (strcasecmp(engine->remote_dtls_fingerprint.type, "sha-256")) {
			//	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_WARNING, "Unsupported fingerprint type.\n");
				//engine->local_dtls_fingerprint.type = NULL;
				//engine->remote_dtls_fingerprint.type = NULL;
			//}


			generate_local_fingerprint(smh, type);
			switch_channel_set_flag(smh->session->channel, CF_DTLS);
			
		} else if (!engine->remote_ssrc && !strcasecmp(attr->a_name, "ssrc") && attr->a_value) {
			engine->remote_ssrc = (uint32_t) atol(attr->a_value);

			if (engine->rtp_session && engine->remote_ssrc) {
				switch_rtp_set_remote_ssrc(engine->rtp_session, engine->remote_ssrc);
			}
	

#ifdef RTCP_MUX
		} else if (!strcasecmp(attr->a_name, "rtcp-mux")) {
			engine->rtcp_mux = SWITCH_TRUE;
			engine->remote_rtcp_port = engine->cur_payload_map->remote_sdp_port;
			got_rtcp_mux++;
#endif
		} else if (!strcasecmp(attr->a_name, "candidate")) {
			switch_channel_set_flag(smh->session->channel, CF_ICE);

			if (!engine->cand_acl_count) {
				engine->cand_acl[engine->cand_acl_count++] = "wan.auto";
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_WARNING, "NO candidate ACL defined, Defaulting to wan.auto\n");
			}


			if (!switch_stristr(" udp ", attr->a_value)) {
				continue;
			}

			data = switch_core_session_strdup(smh->session, attr->a_value);

			argc = switch_split(data, ' ', fields);
			
			if (argc < 5 || engine->ice_in.cand_idx >= MAX_CAND - 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_WARNING, "Invalid data\n");
				continue;
			}

			cid = atoi(fields[1]) - 1;


			for (i = 0; i < argc; i++) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG1, "CAND %d [%s]\n", i, fields[i]);
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, 
							  "Checking Candidate cid: %d proto: %s type: %s addr: %s:%s\n", cid+1, fields[2], fields[7], fields[4], fields[5]);


			engine->ice_in.cand_idx++;

			for (i = 0; i < engine->cand_acl_count; i++) {
				if (!engine->ice_in.chosen[cid] && switch_check_network_list_ip(fields[4], engine->cand_acl[i])) {
					engine->ice_in.chosen[cid] = engine->ice_in.cand_idx;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
									  "Choose %s Candidate cid: %d proto: %s type: %s addr: %s:%s\n", 
									  type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : "audio",
									  cid+1, fields[2], fields[7], fields[4], fields[5]);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
									  "Save %s Candidate cid: %d proto: %s type: %s addr: %s:%s\n", 
									  type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : "audio",
									  cid+1, fields[2], fields[7], fields[4], fields[5]);
				}

				engine->ice_in.cands[engine->ice_in.cand_idx][cid].foundation = switch_core_session_strdup(smh->session, fields[0]);
				engine->ice_in.cands[engine->ice_in.cand_idx][cid].component_id = atoi(fields[1]);
				engine->ice_in.cands[engine->ice_in.cand_idx][cid].transport = switch_core_session_strdup(smh->session, fields[2]);
				engine->ice_in.cands[engine->ice_in.cand_idx][cid].priority = atol(fields[3]);
				engine->ice_in.cands[engine->ice_in.cand_idx][cid].con_addr = switch_core_session_strdup(smh->session, fields[4]);
				engine->ice_in.cands[engine->ice_in.cand_idx][cid].con_port = (switch_port_t)atoi(fields[5]);


				j = 6;

				while(j < argc && fields[j+1]) {
					if (!strcasecmp(fields[j], "typ")) {
						engine->ice_in.cands[engine->ice_in.cand_idx][cid].cand_type = switch_core_session_strdup(smh->session, fields[j+1]);							
					} else if (!strcasecmp(fields[j], "raddr")) {
						engine->ice_in.cands[engine->ice_in.cand_idx][cid].raddr = switch_core_session_strdup(smh->session, fields[j+1]);
					} else if (!strcasecmp(fields[j], "rport")) {
						engine->ice_in.cands[engine->ice_in.cand_idx][cid].rport = (switch_port_t)atoi(fields[j+1]);
					} else if (!strcasecmp(fields[j], "generation")) {
						engine->ice_in.cands[engine->ice_in.cand_idx][cid].generation = switch_core_session_strdup(smh->session, fields[j+1]);
					}
					
					j += 2;
				} 
				

				if (engine->ice_in.chosen[cid]) {
					engine->ice_in.cands[engine->ice_in.chosen[cid]][cid].ready++;
				}
				
				break;
			}
		}
		
	}
	
	/* still no candidates, so start searching for some based on sane deduction */

	/* look for candidates on the same network */
	if (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]) {
		for (i = 0; i <= engine->ice_in.cand_idx && (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]); i++) {
			if (!engine->ice_in.chosen[0] && engine->ice_in.cands[i][0].component_id == 1 && 
				!engine->ice_in.cands[i][0].rport && switch_check_network_list_ip(engine->ice_in.cands[i][0].con_addr, "localnet.auto")) {
				engine->ice_in.chosen[0] = i;
				engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
								  "No %s RTP candidate found; defaulting to the first local one.\n", type2str(type));
			}
			if (!engine->ice_in.chosen[1] && engine->ice_in.cands[i][1].component_id == 2 && 
				!engine->ice_in.cands[i][1].rport && switch_check_network_list_ip(engine->ice_in.cands[i][1].con_addr, "localnet.auto")) {
				engine->ice_in.chosen[1] = i;
				engine->ice_in.cands[engine->ice_in.chosen[1]][1].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session),SWITCH_LOG_NOTICE, 
								  "No %s RTCP candidate found; defaulting to the first local one.\n", type2str(type));
			}
		}
	}

	/* look for candidates with srflx */
	if (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]) {
		for (i = 0; i <= engine->ice_in.cand_idx && (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]); i++) {
			if (!engine->ice_in.chosen[0] && engine->ice_in.cands[i][0].component_id == 1 && engine->ice_in.cands[i][0].rport) {
				engine->ice_in.chosen[0] = i;
				engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
								  "No %s RTP candidate found; defaulting to the first srflx one.\n", type2str(type));
			}
			if (!engine->ice_in.chosen[1] && engine->ice_in.cands[i][1].component_id == 2 && engine->ice_in.cands[i][1].rport) {
				engine->ice_in.chosen[1] = i;
				engine->ice_in.cands[engine->ice_in.chosen[1]][1].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session),SWITCH_LOG_NOTICE, 
								  "No %s RTCP candidate found; defaulting to the first srflx one.\n", type2str(type));
			}
		}
	}

	/* Got RTP but not RTCP, probably mux */
	if (engine->ice_in.chosen[0] && !engine->ice_in.chosen[1] && got_rtcp_mux) {
		engine->ice_in.chosen[1] = engine->ice_in.chosen[0];

		memcpy(&engine->ice_in.cands[engine->ice_in.chosen[1]][1], &engine->ice_in.cands[engine->ice_in.chosen[0]][0], 
			   sizeof(engine->ice_in.cands[engine->ice_in.chosen[0]][0]));
		engine->ice_in.cands[engine->ice_in.chosen[1]][1].ready++;
		
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE,
						  "No %s RTCP candidate found; defaulting to the same as RTP [%s:%d]\n", type2str(type),
						  engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port);
	}

	/* look for any candidates and hope for auto-adjust */
	if (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]) {
		for (i = 0; i <= engine->ice_in.cand_idx && (!engine->ice_in.chosen[0] || !engine->ice_in.chosen[1]); i++) {
			if (!engine->ice_in.chosen[0] && engine->ice_in.cands[i][0].component_id == 1) {
				engine->ice_in.chosen[0] = i;
				engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
								  "No %s RTP candidate found; defaulting to the first one.\n", type2str(type));
			}
			if (!engine->ice_in.chosen[1] && engine->ice_in.cands[i][1].component_id == 2) {
				engine->ice_in.chosen[1] = i;
				engine->ice_in.cands[engine->ice_in.chosen[1]][1].ready++;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
								  "No %s RTCP candidate found; defaulting to the first one.\n", type2str(type));
			}
		}
	}

	for (i = 0; i < 2; i++) {
		if (engine->ice_in.cands[engine->ice_in.chosen[i]][i].ready) {
			if (zstr(engine->ice_in.ufrag) || zstr(engine->ice_in.pwd)) {
				engine->ice_in.cands[engine->ice_in.chosen[i]][i].ready = 0;
			}
		}
	}


	if (engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr && engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port) {
		char tmp[80] = "";
		engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(smh->session, (char *) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE, 
						  "setting remote %s ice addr to %s:%d based on candidate\n", type2str(type),
						  engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port);
		engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready++;

		engine->remote_rtp_ice_port = (switch_port_t) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port;
		engine->remote_rtp_ice_addr = switch_core_session_strdup(smh->session, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);

		engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(smh->session, (char *) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);
		engine->cur_payload_map->remote_sdp_port = (switch_port_t) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port;
		
		if (!smh->mparams->remote_ip) {
			smh->mparams->remote_ip = engine->cur_payload_map->remote_sdp_ip;
		}

		if (engine->remote_rtcp_port) {
			engine->remote_rtcp_port = engine->cur_payload_map->remote_sdp_port;
		}
																 
		switch_snprintf(tmp, sizeof(tmp), "%d", engine->cur_payload_map->remote_sdp_port);
		switch_channel_set_variable(smh->session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, engine->cur_payload_map->remote_sdp_ip);
		switch_channel_set_variable(smh->session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);					
	}

	if (engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port) {
		if (engine->rtcp_mux) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE,
							  "Asked by candidate to set remote rtcp %s addr to %s:%d but this is rtcp-mux so no thanks\n", type2str(type),
							  engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_NOTICE,
							  "Setting remote rtcp %s addr to %s:%d based on candidate\n", type2str(type),
							  engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port);
			engine->remote_rtcp_ice_port = engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port;
			engine->remote_rtcp_ice_addr = switch_core_session_strdup(smh->session, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr);
			
			engine->remote_rtcp_port = engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port;
		}
	}


	if (m && !got_rtcp_mux) {
		engine->rtcp_mux = -1;
	}

	if (switch_channel_test_flag(smh->session->channel, CF_REINVITE)) {

		if (switch_rtp_ready(engine->rtp_session) && engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_INFO, "RE-Activating %s ICE\n", type2str(type));

			switch_rtp_activate_ice(engine->rtp_session, 
									engine->ice_in.ufrag,
									engine->ice_out.ufrag,
									engine->ice_out.pwd,
									engine->ice_in.pwd,
									IPR_RTP,
#ifdef GOOGLE_ICE
									ICE_GOOGLE_JINGLE,
									NULL
#else
									switch_ice_direction(smh->session) == 
									SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
									&engine->ice_in
#endif
									);

		
			
		}
		

		if (engine->rtp_session && ((val = switch_channel_get_variable(smh->session->channel,
																	   type == SWITCH_MEDIA_TYPE_VIDEO ? 
																	   "rtcp_video_interval_msec" : "rtcp_audio_interval_msec")) 
									|| (val = type == SWITCH_MEDIA_TYPE_VIDEO ? 
										smh->mparams->rtcp_video_interval_msec : smh->mparams->rtcp_audio_interval_msec))) {
			
			switch_port_t remote_rtcp_port = engine->remote_rtcp_port;

			if (remote_rtcp_port) {
				if (!strcasecmp(val, "passthru")) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_INFO, "Activating %s RTCP PASSTHRU PORT %d\n", 
									  type2str(type), remote_rtcp_port);
					switch_rtp_activate_rtcp(engine->rtp_session, -1, remote_rtcp_port, engine->rtcp_mux > 0);
				} else {
					int interval = atoi(val);
					if (interval < 100 || interval > 500000) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_ERROR,
										  "Invalid rtcp interval spec [%d] must be between 100 and 500000\n", interval);
						interval = 10000;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_INFO, "Activating %s RTCP PORT %d\n", type2str(type), remote_rtcp_port);
					switch_rtp_activate_rtcp(engine->rtp_session, interval, remote_rtcp_port, engine->rtcp_mux > 0);
				}
			}
		}
			
		if (engine->rtp_session && engine->ice_in.cands[engine->ice_in.chosen[1]][1].ready) {
			if (engine->rtcp_mux > 0 && !strcmp(engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr)
				&& engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port == engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_INFO, "Skipping %s RTCP ICE (Same as RTP)\n", type2str(type));
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_INFO, "Activating %s RTCP ICE\n", type2str(type));
				
				switch_rtp_activate_ice(engine->rtp_session, 
										engine->ice_in.ufrag,
										engine->ice_out.ufrag,
										engine->ice_out.pwd,
										engine->ice_in.pwd,
										IPR_RTCP,
#ifdef GOOGLE_ICE
										ICE_GOOGLE_JINGLE,
										NULL
#else
										switch_ice_direction(smh->session) == 
										SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
										&engine->ice_in
#endif
										);
			}
			
		}
		
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

SWITCH_DECLARE(void) switch_core_session_set_ice(switch_core_session_t *session)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	switch_channel_set_flag(session->channel, CF_VERBOSE_SDP);
	switch_channel_set_flag(session->channel, CF_WEBRTC);
	switch_channel_set_flag(session->channel, CF_ICE);
	smh->mparams->rtcp_audio_interval_msec = "10000";
	smh->mparams->rtcp_video_interval_msec = "10000";

}

#define MAX_MATCHES 30
struct matches {
	const switch_codec_implementation_t *imp;
	sdp_rtpmap_t *map;
	int rate;
	int codec_idx;
};

static void greedy_sort(switch_media_handle_t *smh, struct matches *matches, int m_idx, const switch_codec_implementation_t **codec_array, int total_codecs)
{
	int j = 0, f = 0, g;
	struct matches mtmp[MAX_MATCHES] = { { 0 } };
	for(j = 0; j < m_idx; j++) {
		*&mtmp[j] = *&matches[j];
					}
	for (g = 0; g < smh->mparams->num_codecs && g < total_codecs; g++) {
		const switch_codec_implementation_t *imp = codec_array[g];
		
		for(j = 0; j < m_idx; j++) {
			if (mtmp[j].imp == imp) {
				*&matches[f++] = *&mtmp[j];
			}
		}
	}
}

static void clear_pmaps(switch_rtp_engine_t *engine)
{
	payload_map_t *pmap;

	for (pmap = engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {
		pmap->negotiated = 0;
		pmap->current = 0;
	}
}

//?
SWITCH_DECLARE(uint8_t) switch_core_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, uint8_t *proceed, switch_sdp_type_t sdp_type)
{
	uint8_t match = 0;
	uint8_t vmatch = 0;
	switch_payload_t best_te = 0, te = 0, cng_pt = 0;
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
	int sendonly = 0, recvonly = 0;
	int greedy = 0, x = 0, skip = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *val;
	const char *crypto = NULL;
	int got_crypto = 0, got_video_crypto = 0, got_audio = 0, got_avp = 0, got_video_avp = 0, got_video_savp = 0, got_savp = 0, got_udptl = 0, got_webrtc = 0;
	int scrooge = 0;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	int reneg = 1;
	const switch_codec_implementation_t **codec_array;
	int total_codecs;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;
	uint32_t near_rate = 0;
	const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
	sdp_rtpmap_t *mmap = NULL, *near_map = NULL;
	struct matches matches[MAX_MATCHES] = { { 0 } };
	struct matches near_matches[MAX_MATCHES] = { { 0 } };
	int codec_ms = 0;
	uint32_t remote_codec_rate = 0, fmtp_remote_codec_rate = 0;
	const char *tmp;
	int m_idx = 0;
	int nm_idx = 0;
	
	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return 0;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	codec_array = smh->codecs;
	total_codecs = smh->mparams->num_codecs;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	if (dtls_ok(session) && (tmp = switch_channel_get_variable(smh->session->channel, "webrtc_enable_dtls")) && switch_false(tmp)) {
		switch_channel_clear_flag(smh->session->channel, CF_DTLS_OK);
		switch_channel_clear_flag(smh->session->channel, CF_DTLS);
	}

	switch_core_session_parse_crypto_prefs(session);

	clear_pmaps(a_engine);
	clear_pmaps(v_engine);

	if (proceed) *proceed = 1;

	greedy = !!switch_media_handle_test_media_flag(smh, SCMF_CODEC_GREEDY);
	scrooge = !!switch_media_handle_test_media_flag(smh, SCMF_CODEC_SCROOGE);

	if ((val = switch_channel_get_variable(channel, "rtp_codec_negotiation"))) {
		if (!strcasecmp(val, "generous")) {
			greedy = 0;
			scrooge = 0;
		} else if (!strcasecmp(val, "greedy")) {
			greedy = 1;
			scrooge = 0;
		} else if (!strcasecmp(val, "scrooge")) {
			scrooge = 1;
			greedy = 1;
		} else {
		    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rtp_codec_negotiation ignored invalid value : '%s' \n", val );	
		}		
	}

	if ((smh->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {

		if ((smh->mparams->auto_rtp_bugs & RTP_BUG_CISCO_SKIP_MARK_BIT_2833)) {

			if (strstr(smh->origin, "CiscoSystemsSIP-GW-UserAgent")) {
				a_engine->rtp_bugs |= RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
			}
		}

		if ((smh->mparams->auto_rtp_bugs & RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833)) {
			if (strstr(smh->origin, "Sonus_UAC")) {
				a_engine->rtp_bugs |= RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "Hello,\nI see you have a Sonus!\n"
								  "FYI, Sonus cannot follow the RFC on the proper way to send DTMF.\n"
								  "Sadly, my creator had to spend several hours figuring this out so I thought you'd like to know that!\n"
								  "Don't worry, DTMF will work but you may want to ask them to fix it......\n");
			}
		}
	}

	if ((val = switch_channel_get_variable(session->channel, "rtp_liberal_dtmf")) && switch_true(val)) {
		switch_channel_set_flag(session->channel, CF_LIBERAL_DTMF);
	}

	if (switch_stristr("T38FaxFillBitRemoval:", r_sdp) || switch_stristr("T38FaxTranscodingMMR:", r_sdp) || 
		switch_stristr("T38FaxTranscodingJBIG:", r_sdp)) {
		switch_channel_set_variable(session->channel, "t38_broken_boolean", "true");
	}

	switch_core_media_find_zrtp_hash(session, sdp);
	switch_core_media_pass_zrtp_hash(session);

	check_ice(smh, SWITCH_MEDIA_TYPE_AUDIO, sdp, NULL);
	check_ice(smh, SWITCH_MEDIA_TYPE_VIDEO, sdp, NULL);

	if ((sdp->sdp_connection && sdp->sdp_connection->c_address && !strcmp(sdp->sdp_connection->c_address, "0.0.0.0"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "RFC2543 from March 1999 called; They want their 0.0.0.0 hold method back.....\n");
		sendonly = 2;			/* global sendonly always wins */
	}

	for (m = sdp->sdp_media; m; m = m->m_next) {
		sdp_connection_t *connection;
		switch_core_session_t *other_session;

		if (!m->m_port) {
			continue;
		}

		ptime = dptime;
		maxptime = dmaxptime;

		if (m->m_proto == sdp_proto_extended_srtp) {
			got_webrtc++;
			switch_core_session_set_ice(session);
		}
		
		if (m->m_proto_name && !strcasecmp(m->m_proto_name, "UDP/TLS/RTP/SAVPF")) {
			switch_channel_set_flag(session->channel, CF_WEBRTC_MOZ);
		}

		if (m->m_proto == sdp_proto_srtp || m->m_proto == sdp_proto_extended_srtp) {
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
			switch_t38_options_t *t38_options = switch_core_media_process_udptl(session, sdp, m);

			if (switch_channel_test_app_flag_key("T38", session->channel, CF_APP_T38_NEGOTIATED)) {
				match = 1;
				goto done;
			}

			if (switch_true(switch_channel_get_variable(channel, "refuse_t38"))) {
				switch_channel_clear_app_flag_key("T38", session->channel, CF_APP_T38);
				match = 0;
				goto done;
			} else {
				const char *var = switch_channel_get_variable(channel, "t38_passthru");
				int pass = switch_channel_test_flag(smh->session->channel, CF_T38_PASSTHRU);


				if (switch_channel_test_app_flag_key("T38", session->channel, CF_APP_T38)) {
					if (proceed) *proceed = 0;
				}

				if (var) {
					if (!(pass = switch_true(var))) {
						if (!strcasecmp(var, "once")) {
							pass = 2;
						}
					}
				}

				if ((pass == 2 && switch_channel_test_flag(smh->session->channel, CF_T38_PASSTHRU)) 
					|| !switch_channel_test_flag(session->channel, CF_REINVITE) ||
					
					switch_channel_test_flag(session->channel, CF_PROXY_MODE) || 
					switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) || 
					!switch_rtp_ready(a_engine->rtp_session)) {
					pass = 0;
				}
				
				if (pass && switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
					switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
					switch_core_session_message_t *msg;
					char *remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
					switch_port_t remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);
					char tmp[32] = "";


                    if (!switch_channel_test_flag(other_channel, CF_ANSWERED)) {
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
                                          SWITCH_LOG_WARNING, "%s Error Passing T.38 to unanswered channel %s\n",
                                          switch_channel_get_name(session->channel), switch_channel_get_name(other_channel));
                        switch_core_session_rwunlock(other_session);

                        pass = 0;
                        match = 0;
                        goto done;
                    }


					if (switch_true(switch_channel_get_variable(session->channel, "t38_broken_boolean")) && 
						switch_true(switch_channel_get_variable(session->channel, "t38_pass_broken_boolean"))) {
						switch_channel_set_variable(other_channel, "t38_broken_boolean", "true");
					}
					
					a_engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(session, t38_options->remote_ip);
					a_engine->cur_payload_map->remote_sdp_port = t38_options->remote_port;

					if (remote_host && remote_port && !strcmp(remote_host, a_engine->cur_payload_map->remote_sdp_ip) && remote_port == a_engine->cur_payload_map->remote_sdp_port) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n",
										  switch_channel_get_name(session->channel));
					} else {
						const char *err = NULL;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n",
										  switch_channel_get_name(session->channel),
										  remote_host, remote_port, a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);
						
						switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->remote_sdp_port);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->cur_payload_map->remote_sdp_ip);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);

						if (switch_rtp_set_remote_address(a_engine->rtp_session, a_engine->cur_payload_map->remote_sdp_ip,
														  a_engine->cur_payload_map->remote_sdp_port, 0, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
							switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
						}
						
					}

					

					switch_core_media_copy_t38_options(t38_options, other_session);

					switch_channel_set_flag(smh->session->channel, CF_T38_PASSTHRU);
					switch_channel_set_flag(other_session->channel, CF_T38_PASSTHRU);
					
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
			int ice = 0;

			nm_idx = 0;
			m_idx = 0;
			memset(matches, 0, sizeof(matches[0]) * MAX_MATCHES);
			memset(near_matches, 0, sizeof(near_matches[0]) * MAX_MATCHES);

			if (!sendonly && (m->m_mode == sdp_sendonly || m->m_mode == sdp_inactive)) {
				sendonly = 1;
			}

			if (!sendonly && m->m_connections && m->m_connections->c_address && !strcmp(m->m_connections->c_address, "0.0.0.0")) {
				sendonly = 1;
			}

			for (attr = sdp->sdp_attributes; attr; attr = attr->a_next) {
				if (zstr(attr->a_name)) {
					continue;
				}

				
				if (!strncasecmp(attr->a_name, "ice", 3)) {
					ice++;
				} else if (sendonly < 2 && !strcasecmp(attr->a_name, "sendonly")) {
					sendonly = 1;
					switch_channel_set_variable(session->channel, "media_audio_mode", "recvonly");
				} else if (sendonly < 2 && !strcasecmp(attr->a_name, "inactive")) {
					sendonly = 1;
					switch_channel_set_variable(session->channel, "media_audio_mode", "inactive");
				} else if (!strcasecmp(attr->a_name, "recvonly")) {
					switch_channel_set_variable(session->channel, "media_audio_mode", "sendonly");
					recvonly = 1;
					
					if (switch_rtp_ready(a_engine->rtp_session)) {
						switch_rtp_set_max_missed_packets(a_engine->rtp_session, 0);
						a_engine->max_missed_hold_packets = 0;
						a_engine->max_missed_packets = 0;
					} else {
						switch_channel_set_variable(session->channel, "rtp_timeout_sec", "0");
						switch_channel_set_variable(session->channel, "rtp_hold_timeout_sec", "0");
					}
				} else if (sendonly < 2 && !strcasecmp(attr->a_name, "sendrecv")) {
					sendonly = 0;
				} else if (!strcasecmp(attr->a_name, "ptime")) {
					ptime = dptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime")) {
					maxptime = dmaxptime = atoi(attr->a_value);
				}
			}

			if (sendonly == 2 && ice) {
				sendonly = 0;
			}


			if (sendonly != 1 && recvonly != 1) {
				switch_channel_set_variable(session->channel, "media_audio_mode", NULL);
			}

			if (!(switch_media_handle_test_media_flag(smh, SCMF_DISABLE_HOLD)
				  || ((val = switch_channel_get_variable(session->channel, "rtp_disable_hold"))
					  && switch_true(val)))
				&& !smh->mparams->hold_laps) {
				smh->mparams->hold_laps++;
				if (switch_core_media_toggle_hold(session, sendonly)) {
					reneg = switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_HOLD);
					if ((val = switch_channel_get_variable(session->channel, "rtp_renegotiate_codec_on_hold"))) {
						reneg = switch_true(val);
					}
				}
			}

			if (reneg) {
				reneg = switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_REINVITE);
				
				if ((val = switch_channel_get_variable(session->channel, "rtp_renegotiate_codec_on_reinvite"))) {
					reneg = switch_true(val);
				}
			}

			if (session->bugs) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
								  "Session is connected to a media bug. "
								  "Re-Negotiation implicitly disabled.\n");
				reneg = 0;
			}
			
			if (switch_channel_test_flag(session->channel, CF_RECOVERING)) {
				reneg = 0;
			}
			
			if (sdp_type == SDP_TYPE_RESPONSE && smh->num_negotiated_codecs) {
				/* response to re-invite or update, only negotiated codecs are valid */
				reneg = 0;
			}


			if (!reneg && smh->num_negotiated_codecs) {
				codec_array = smh->negotiated_codecs;
				total_codecs = smh->num_negotiated_codecs;
			} else if (reneg) {
				smh->mparams->num_codecs = 0;
				switch_core_media_prepare_codecs(session, SWITCH_FALSE);
				codec_array = smh->codecs;
				total_codecs = smh->mparams->num_codecs;
			}
			

			if (switch_rtp_has_dtls() && dtls_ok(session)) {
				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					
					if (!strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
						got_crypto = 1;
					}
				}
			}

			for (attr = m->m_attributes; attr; attr = attr->a_next) {

				if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value) {
					switch_channel_set_variable(session->channel, "rtp_remote_audio_rtcp_port", attr->a_value);
					a_engine->remote_rtcp_port = (switch_port_t)atoi(attr->a_value);
					if (!smh->mparams->rtcp_audio_interval_msec) {
						smh->mparams->rtcp_audio_interval_msec = "5000";
					}
				} else if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
					maxptime = atoi(attr->a_value);
				} else if (got_crypto < 1 && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
					int crypto_tag;

					if (!(smh->mparams->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
						!switch_true(switch_channel_get_variable(session->channel, "rtp_allow_crypto_in_avp"))) {
						if (m->m_proto != sdp_proto_srtp && !got_webrtc) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
							match = 0;
							goto done;
						}
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);
					got_crypto = switch_core_session_check_incoming_crypto(session, 
																		   "rtp_has_crypto", SWITCH_MEDIA_TYPE_AUDIO, crypto, crypto_tag, sdp_type);

				}
			}

			if (got_crypto == -1 && got_savp && !got_avp && !got_webrtc) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Declining invite with only SAVP because secure media is administratively disabled\n");
				match = 0;
				break;
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

			x = 0;

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				int32_t i;
				const char *rm_encoding;
				uint32_t map_bit_rate = 0;
				switch_codec_fmtp_t codec_fmtp = { 0 };
				int map_channels = map->rm_params ? atoi(map->rm_params) : 1;

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}
				
				
				if (!strcasecmp(rm_encoding, "telephone-event")) {
					if (!best_te || map->rm_rate == a_engine->cur_payload_map->rm_rate) {
						best_te = (switch_payload_t) map->rm_pt;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set telephone-event payload to %u\n", best_te);
					}
					continue;
				}
				
				if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (a_engine->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(a_engine->rtp_session, smh->mparams->cng_pt);
					}
					continue;
				}
				
				
				if (x++ < skip) {
					continue;
				}

				if (match) {
					continue;
				}

				codec_ms = ptime;

				if (switch_channel_get_variable(session->channel, "rtp_h_X-Broken-PTIME") && a_engine->read_impl.microseconds_per_packet) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Overwriting ptime from a known broken endpoint with the currently used value of %d ms\n", a_engine->read_impl.microseconds_per_packet / 1000);
					codec_ms = a_engine->read_impl.microseconds_per_packet / 1000;
				}

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
				
				remote_codec_rate = map->rm_rate;
				fmtp_remote_codec_rate = 0;
				memset(&codec_fmtp, 0, sizeof(codec_fmtp));

				if (zstr(map->rm_fmtp)) {
					if (!strcasecmp(map->rm_encoding, "ilbc")) {
						codec_ms = 30;
						map_bit_rate = 13330;
					} else if (!strcasecmp(map->rm_encoding, "isac")) {
						codec_ms = 30;
						map_bit_rate = 32000;
					}
				} else {
					if ((switch_core_codec_parse_fmtp(map->rm_encoding, map->rm_fmtp, map->rm_rate, &codec_fmtp)) == SWITCH_STATUS_SUCCESS) {
						if (codec_fmtp.bits_per_second) {
							map_bit_rate = codec_fmtp.bits_per_second;
						}
						if (codec_fmtp.microseconds_per_packet) {
							codec_ms = (codec_fmtp.microseconds_per_packet / 1000);
						}
						if (codec_fmtp.actual_samples_per_second) {
							fmtp_remote_codec_rate = codec_fmtp.actual_samples_per_second;
						}
						if (codec_fmtp.stereo) {
							map_channels = 2;
						} else if (!strcasecmp(map->rm_encoding, "opus")) {
							map_channels = 1;
						}
					}
				}

				for (i = 0; i < smh->mparams->num_codecs && i < total_codecs; i++) {
					const switch_codec_implementation_t *imp = codec_array[i];
					uint32_t bit_rate = imp->bits_per_second;
					uint32_t codec_rate = imp->samples_per_second;

					if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
						continue;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio Codec Compare [%s:%d:%u:%d:%u:%d]/[%s:%d:%u:%d:%u:%d]\n",
									  rm_encoding, map->rm_pt, (int) remote_codec_rate, codec_ms, map_bit_rate, map_channels,
									  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000, bit_rate, imp->number_of_channels);
					if ((zstr(map->rm_encoding) || (smh->mparams->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						match = (!strcasecmp(rm_encoding, imp->iananame) && 
								 ((map->rm_pt < 96 && imp->ianacode < 96) || (map->rm_pt > 95 && imp->ianacode > 95)) &&
								 (remote_codec_rate == codec_rate || fmtp_remote_codec_rate == imp->actual_samples_per_second)) ? 1 : 0;
						if (fmtp_remote_codec_rate) {
							remote_codec_rate = fmtp_remote_codec_rate;
						}
					}

					if (match && bit_rate && map_bit_rate && map_bit_rate != bit_rate && strcasecmp(map->rm_encoding, "ilbc") && 
						strcasecmp(map->rm_encoding, "isac")) {
						/* if a bit rate is specified and doesn't match, this is not a codec match, except for ILBC */
						match = 0;
					}

					if (match && remote_codec_rate && codec_rate && remote_codec_rate != codec_rate && (!strcasecmp(map->rm_encoding, "pcma") || 
																							  !strcasecmp(map->rm_encoding, "pcmu"))) {
						/* if the sampling rate is specified and doesn't match, this is not a codec match for G.711 */
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sampling rates have to match for G.711\n");
						match = 0;
					}
					
					if (match) {
						if (scrooge) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
											  "Bah HUMBUG! Sticking with %s@%uh@%ui\n",
											  imp->iananame, imp->samples_per_second, imp->microseconds_per_packet / 1000);
						} else if ((ptime && codec_ms && codec_ms * 1000 != imp->microseconds_per_packet) || remote_codec_rate != codec_rate) {
							/* ptime does not match */
							match = 0;
							
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
											  "Audio Codec Compare [%s:%d:%u:%d:%u:%d] is saved as a near-match\n", 
											  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000, bit_rate, imp->number_of_channels);

							near_matches[nm_idx].codec_idx = i;
							near_matches[nm_idx].rate = remote_codec_rate;
							near_matches[nm_idx].imp = imp;
							near_matches[nm_idx].map = map;
							nm_idx++;

							continue;
						}

						matches[m_idx].codec_idx = i;
						matches[m_idx].rate = codec_rate;
						matches[m_idx].imp = imp;
						matches[m_idx].map = map;
						m_idx++;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
										  "Audio Codec Compare [%s:%d:%u:%d:%u:%d] ++++ is saved as a match\n", 
										  imp->iananame, imp->ianacode, codec_rate, imp->microseconds_per_packet / 1000, bit_rate, imp->number_of_channels);
						
						if (m_idx >= MAX_MATCHES) {
							break;
						}

						match = 0;
					}
				}

				if (m_idx >= MAX_MATCHES) {
					break;
				}
			}

			if (smh->crypto_mode == CRYPTO_MODE_MANDATORY && got_crypto < 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Crypto not negotiated but required.\n");
				match = 0;
				m_idx = nm_idx = 0;
			}


			if (!m_idx && nm_idx) {
				int j;

				for(j = 0; j < nm_idx; j++) {
					const switch_codec_implementation_t *search[1];
					char *prefs[1];
					char tmp[80];
					int num;
					const switch_codec_implementation_t *timp = NULL;

					near_rate = near_matches[j].rate;
					near_match = near_matches[j].imp;
					near_map = near_matches[j].map;					
					
					switch_snprintf(tmp, sizeof(tmp), "%s@%uh@%ui%dc", near_match->iananame, near_rate ? near_rate : near_match->samples_per_second,
									codec_ms, near_match->number_of_channels);
					
					prefs[0] = tmp;
					num = switch_loadable_module_get_codecs_sorted(search, 1, prefs, 1);
				
					if (num) {
						timp = search[0];
					} else {
						timp = near_match;
					}
				
					if (!maxptime || timp->microseconds_per_packet / 1000 <= maxptime) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Substituting codec %s@%ui@%uh@%dc\n",
										  timp->iananame, timp->microseconds_per_packet / 1000, timp->actual_samples_per_second, timp->number_of_channels);
						match = 1;

						matches[m_idx].codec_idx = near_matches[j].codec_idx;
						matches[m_idx].rate = near_rate;
						matches[m_idx].imp = timp;
						matches[m_idx].map = near_map;
						m_idx++;

						break;
					}
				}
			}
			
			if (m_idx) {
				int j;

				if (greedy) { /* sort in favor of mine */
					greedy_sort(smh, matches, m_idx, codec_array, total_codecs);
				}

				match = 1;
				a_engine->codec_negotiated = 1;
				smh->num_negotiated_codecs = 0;

				for(j = 0; j < m_idx; j++) {
					payload_map_t *pmap = switch_core_media_add_payload_map(session, 
																			SWITCH_MEDIA_TYPE_AUDIO,
																			matches[j].map->rm_encoding,
																			matches[j].map->rm_fmtp,
																			sdp_type, 
																			matches[j].map->rm_pt,
																			matches[j].imp->samples_per_second,
																			matches[j].imp->microseconds_per_packet / 1000,
																			matches[j].imp->number_of_channels,
																			SWITCH_TRUE);

					mimp = matches[j].imp;
					mmap = matches[j].map;
			
					if (j == 0) {
						a_engine->cur_payload_map = pmap;
						a_engine->cur_payload_map->current = 1;
						if (a_engine->rtp_session) {
							switch_rtp_set_default_payload(a_engine->rtp_session, pmap->pt);
						}
					}
						
					pmap->rm_encoding = switch_core_session_strdup(session, (char *) mmap->rm_encoding);
					pmap->iananame = switch_core_session_strdup(session, (char *) mimp->iananame);
					pmap->recv_pt = (switch_payload_t) mmap->rm_pt;
					pmap->rm_rate = mimp->samples_per_second;
					pmap->adv_rm_rate = mimp->samples_per_second;
					if (strcasecmp(mimp->iananame, "g722")) {
						pmap->rm_rate = mimp->actual_samples_per_second;
					}
					pmap->codec_ms = mimp->microseconds_per_packet / 1000;
					pmap->bitrate = mimp->bits_per_second;
					pmap->channels = mmap->rm_params ? atoi(mmap->rm_params) : 1;
						
					if (!strcasecmp((char *) mmap->rm_encoding, "opus")) {
						if (pmap->channels == 1) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Invalid SDP for opus.  Don't ask.. but it needs a /2\n");
							pmap->adv_channels = 1;
						} else {
							pmap->adv_channels = 2; /* IKR ???*/
						}
						if (!zstr((char *) mmap->rm_fmtp) && switch_stristr("stereo=1", (char *) mmap->rm_fmtp)) {
							pmap->channels = 2;
						} else {
							pmap->channels = 1;
						}
					} else {
						pmap->adv_channels = pmap->channels;
					}
						
					pmap->remote_sdp_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					pmap->remote_sdp_port = (switch_port_t) m->m_port;
					pmap->rm_fmtp = switch_core_session_strdup(session, (char *) mmap->rm_fmtp);
						
					pmap->agreed_pt = (switch_payload_t) mmap->rm_pt;
					smh->negotiated_codecs[smh->num_negotiated_codecs++] = mimp;
					pmap->recv_pt = (switch_payload_t)mmap->rm_pt;
						
				}
			}
				
			if (match) {
				char tmp[50];
				//const char *mirror = switch_channel_get_variable(session->channel, "rtp_mirror_remote_audio_codec_payload");
						

				switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->remote_sdp_port);
				switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->cur_payload_map->remote_sdp_ip);
				switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);


				if (a_engine->cur_payload_map->pt == smh->mparams->te) {
					switch_payload_t pl = 0;
					payload_map_t *pmap;

					switch_mutex_lock(smh->sdp_mutex);
					for (pmap = a_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
						if (pmap->pt > pl) {
							pl = pmap->pt;
						}
					}
					switch_mutex_unlock(smh->sdp_mutex);
					
					smh->mparams->te  = (switch_payload_t) ++pl;
				}



#if 0
				if (!switch_true(mirror) && 
					switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND && 
					(!switch_channel_test_flag(session->channel, CF_REINVITE) || switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_REINVITE))) {
					switch_core_media_get_offered_pt(session, matches[0].imp, &a_engine->cur_payload_map->recv_pt);
				}
#endif
				
				switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->recv_pt);
				switch_channel_set_variable(session->channel, "rtp_audio_recv_pt", tmp);
				
				if (switch_core_codec_ready(&a_engine->read_codec) && strcasecmp(matches[0].imp->iananame, a_engine->read_codec.implementation->iananame)) {
					a_engine->reset_codec = 1;
				}

				if (switch_core_media_set_codec(session, 0, smh->mparams->codec_flags) == SWITCH_STATUS_SUCCESS) {
					got_audio = 1;
					check_ice(smh, SWITCH_MEDIA_TYPE_AUDIO, sdp, m);
				} else {
					match = 0;
				}
			}
				
			if (!best_te && (switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || 
							 switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
								  "No 2833 in SDP. Liberal DTMF mode adding %d as telephone-event.\n", smh->mparams->te);
				best_te = smh->mparams->te;
			}

			if (best_te) {
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
					te = smh->mparams->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send payload to %u\n", best_te);
					switch_channel_set_variable(session->channel, "dtmf_type", "rfc2833");
					smh->mparams->dtmf_type = DTMF_2833;
					if (a_engine->rtp_session) {
						switch_rtp_set_telephony_event(a_engine->rtp_session, (switch_payload_t) best_te);
						switch_channel_set_variable_printf(session->channel, "rtp_2833_send_payload", "%d", best_te);
					}
				} else {
					te = smh->mparams->recv_te = smh->mparams->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send/recv payload to %u\n", te);
					switch_channel_set_variable(session->channel, "dtmf_type", "rfc2833");
					smh->mparams->dtmf_type = DTMF_2833;
					if (a_engine->rtp_session) {
						switch_rtp_set_telephony_event(a_engine->rtp_session, te);
						switch_channel_set_variable_printf(session->channel, "rtp_2833_send_payload", "%d", te);
						switch_rtp_set_telephony_recv_event(a_engine->rtp_session, te);
						switch_channel_set_variable_printf(session->channel, "rtp_2833_recv_payload", "%d", te);
					}
				}
			} else {
				/* by default, use SIP INFO if 2833 is not in the SDP */
				if (!switch_false(switch_channel_get_variable(channel, "rtp_info_when_no_2833"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No 2833 in SDP.  Disable 2833 dtmf and switch to INFO\n");
					switch_channel_set_variable(session->channel, "dtmf_type", "info");
					smh->mparams->dtmf_type = DTMF_INFO;
					te = smh->mparams->recv_te = smh->mparams->te = 0;
				} else {
					switch_channel_set_variable(session->channel, "dtmf_type", "none");
					smh->mparams->dtmf_type = DTMF_NONE;
					te = smh->mparams->recv_te = smh->mparams->te = 0;
				}
			}

		} else if (m->m_type == sdp_media_video && m->m_port) {
			sdp_rtpmap_t *map;
			const char *rm_encoding;
			const switch_codec_implementation_t *mimp = NULL;
			int i;

			vmatch = 0;
			nm_idx = 0;
			m_idx = 0;
			memset(matches, 0, sizeof(matches[0]) * MAX_MATCHES);
			memset(near_matches, 0, sizeof(near_matches[0]) * MAX_MATCHES);

			switch_channel_set_variable(session->channel, "video_possible", "true");

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

				if (switch_rtp_has_dtls() && dtls_ok(session)) {
					for (attr = m->m_attributes; attr; attr = attr->a_next) {
						if (!strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
							got_video_crypto = 1;
						}
					}
				}
				
				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
						//framerate = atoi(attr->a_value);
					} else if (!strcasecmp(attr->a_name, "rtcp-fb")) {
						if (!zstr(attr->a_value)) {
							if (switch_stristr("fir", attr->a_value)) {
								v_engine->fir++;
							}
							
							//if (switch_stristr("pli", attr->a_value)) {
							//	v_engine->pli++;
							//}
							
							smh->mparams->rtcp_video_interval_msec = "10000";
						}
					} else if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value && !strcmp(attr->a_value, "1")) {
						switch_channel_set_variable(session->channel, "rtp_remote_video_rtcp_port", attr->a_value);
						v_engine->remote_rtcp_port = (switch_port_t)atoi(attr->a_value);
						if (!smh->mparams->rtcp_video_interval_msec) {
							smh->mparams->rtcp_video_interval_msec = "5000";
						}
					} else if (!got_video_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
						int crypto_tag;
						
						if (!(smh->mparams->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
							!switch_true(switch_channel_get_variable(session->channel, "rtp_allow_crypto_in_avp"))) {
							if (m->m_proto != sdp_proto_srtp && !got_webrtc) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
								match = 0;
								goto done;
							}
						}
						
						crypto = attr->a_value;
						crypto_tag = atoi(crypto);
						
						got_video_crypto = switch_core_session_check_incoming_crypto(session, 
																					 "rtp_has_video_crypto", 
																					 SWITCH_MEDIA_TYPE_VIDEO, crypto, crypto_tag, sdp_type);
					
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

					if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
						switch_channel_test_flag(session->channel, CF_NOVIDEO)) {
						continue;
					}
					
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d]/[%s:%d]\n",
									  rm_encoding, map->rm_pt, imp->iananame, imp->ianacode);
					if ((zstr(map->rm_encoding) || (smh->mparams->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						vmatch = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						vmatch = strcasecmp(rm_encoding, imp->iananame) ? 0 : 1;
					}


					if (vmatch && (map->rm_rate == imp->samples_per_second)) {
						matches[m_idx].imp = imp;
						matches[m_idx].map = map;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d] +++ is saved as a match\n",
										  imp->iananame, imp->ianacode);
						m_idx++;
					}

					vmatch = 0;
				}
			}

			if (smh->crypto_mode == CRYPTO_MODE_MANDATORY && got_video_crypto < 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Crypto not negotiated but required.\n");
				vmatch = 0;
				m_idx = 0;
			}

			if (m_idx) {
				char tmp[50];
				//const char *mirror = switch_channel_get_variable(session->channel, "rtp_mirror_remote_video_codec_payload");
				int j = 0;

				if (greedy) { /* sort in favor of mine */
					greedy_sort(smh, matches, m_idx, codec_array, total_codecs);
				}

				vmatch = 1;
				v_engine->codec_negotiated = 1;

				for(j = 0; j < m_idx; j++) {
					payload_map_t *pmap = switch_core_media_add_payload_map(session, 
																			SWITCH_MEDIA_TYPE_VIDEO,
																			matches[j].map->rm_encoding, 
																			matches[j].map->rm_fmtp,
																			sdp_type, 
																			matches[j].map->rm_pt,
																			matches[j].imp->samples_per_second,
																			matches[j].imp->microseconds_per_packet / 1000,
																			matches[j].imp->number_of_channels,
																			SWITCH_TRUE);
					if (j == 0) {
						v_engine->cur_payload_map = pmap;
						v_engine->cur_payload_map->current = 1;
						if (v_engine->rtp_session) {
							switch_rtp_set_default_payload(v_engine->rtp_session, pmap->pt);
						}
					}
					
					mimp = matches[j].imp;
					map = matches[j].map;
					
					pmap->rm_encoding = switch_core_session_strdup(session, (char *) map->rm_encoding);
					pmap->recv_pt = (switch_payload_t) map->rm_pt;
					pmap->rm_rate = map->rm_rate;
					pmap->codec_ms = mimp->microseconds_per_packet / 1000;


					pmap->remote_sdp_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					pmap->remote_sdp_port = (switch_port_t) m->m_port;
						
					pmap->rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);

					pmap->agreed_pt = (switch_payload_t) map->rm_pt;

					smh->negotiated_codecs[smh->num_negotiated_codecs++] = mimp;					
						
#if 0
					if (j == 0 && (!switch_true(mirror) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND)) {
						switch_core_media_get_offered_pt(session, mimp, &pmap->recv_pt);
					}
#endif
				}

				
				
				switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->cur_payload_map->remote_sdp_port);
				switch_channel_set_variable(session->channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE, v_engine->cur_payload_map->remote_sdp_ip);
				switch_channel_set_variable(session->channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE, tmp);
				switch_channel_set_variable(session->channel, "rtp_video_fmtp", v_engine->cur_payload_map->rm_fmtp);
				switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->cur_payload_map->agreed_pt);
				switch_channel_set_variable(session->channel, "rtp_video_pt", tmp);
				switch_core_media_check_video_codecs(session);
				switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->cur_payload_map->recv_pt);
				switch_channel_set_variable(session->channel, "rtp_video_recv_pt", tmp);

				if (switch_core_codec_ready(&v_engine->read_codec) && strcasecmp(matches[0].imp->iananame, v_engine->read_codec.implementation->iananame)) {
					v_engine->reset_codec = 1;
				}

				if (switch_core_media_set_video_codec(session, 0) == SWITCH_STATUS_SUCCESS) {
					check_ice(smh, SWITCH_MEDIA_TYPE_VIDEO, sdp, m);
				}
			}
		}
	}

	if (!match && vmatch) match = 1;

 done:

	if (parser) {
		sdp_parser_free(parser);
	}

	smh->mparams->cng_pt = cng_pt;

	return match;
}

//?

SWITCH_DECLARE(int) switch_core_media_toggle_hold(switch_core_session_t *session, int sendonly)
{
	int changed = 0;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;
	switch_core_session_t *b_session = NULL;
	switch_channel_t *b_channel = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return 0;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	
	if (switch_core_session_get_partner(session, &b_session) == SWITCH_STATUS_SUCCESS) {
		b_channel = switch_core_session_get_channel(b_session);
	}

	if (sendonly && switch_channel_test_flag(session->channel, CF_ANSWERED)) {
		if (!switch_channel_test_flag(session->channel, CF_PROTO_HOLD)) {
			const char *stream;
			const char *msg = "hold";
			const char *info;

			if ((switch_channel_test_flag(session->channel, CF_SLA_BARGE) || switch_channel_test_flag(session->channel, CF_SLA_BARGING)) && 
				(!b_channel || switch_channel_test_flag(b_channel, CF_EVENT_LOCK_PRI))) {
				switch_channel_mark_hold(session->channel, sendonly);
				switch_channel_set_flag(session->channel, CF_PROTO_HOLD);
				changed = 0;
				goto end;
			}

			info = switch_channel_get_variable(session->channel, "presence_call_info");

			if (info) {
				if (switch_stristr("private", info)) {
					msg = "hold-private";
				}
			}

			if (a_engine->rtp_session) {
				switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			if (v_engine->rtp_session) {
				switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			switch_channel_set_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_TRUE);
			switch_channel_presence(session->channel, "unknown", msg, NULL);
			changed = 1;

			if (a_engine->max_missed_hold_packets && a_engine->rtp_session) {
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_hold_packets);
			}

			if (!(stream = switch_channel_get_hold_music(session->channel))) {
				stream = "local_stream://moh";
			}


			if (stream && strcasecmp(stream, "silence") && (!b_channel || !switch_channel_test_flag(b_channel, CF_EVENT_LOCK_PRI))) {
				if (!strcasecmp(stream, "indicate_hold")) {
					switch_channel_set_flag(session->channel, CF_SUSPEND);
					switch_channel_set_flag(session->channel, CF_HOLD);
					switch_ivr_hold_uuid(switch_core_session_get_uuid(b_session), NULL, 0);
				} else {
					switch_ivr_broadcast(switch_core_session_get_uuid(b_session), stream,
										 SMF_ECHO_ALEG | SMF_LOOP | SMF_PRIORITY);
					switch_yield(250000);
				}
			}

		}
	} else {
		if (switch_channel_test_flag(session->channel, CF_HOLD_LOCK)) {
			switch_channel_set_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_TRUE);

			if (a_engine->rtp_session) {
				switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			if (v_engine->rtp_session) {
				switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			changed = 1;
		}

		switch_channel_clear_flag(session->channel, CF_HOLD_LOCK);

		if (switch_channel_test_flag(session->channel, CF_PROTO_HOLD)) {
			int media_on_hold_a = switch_true(switch_channel_get_variable_dup(session->channel, "bypass_media_resume_on_hold", SWITCH_FALSE, -1));
			int media_on_hold_b = 0;
			int bypass_after_hold_a = 0;
			int bypass_after_hold_b = 0;

			if (media_on_hold_a) {
				bypass_after_hold_a = switch_true(switch_channel_get_variable_dup(session->channel, "bypass_media_after_hold", SWITCH_FALSE, -1));
			}

			if (b_channel) {
				if ((media_on_hold_b = switch_true(switch_channel_get_variable_dup(b_channel, "bypass_media_resume_on_hold", SWITCH_FALSE, -1)))) {
					bypass_after_hold_b = switch_true(switch_channel_get_variable_dup(b_channel, "bypass_media_after_hold", SWITCH_FALSE, -1));
				}
			}
			
			switch_yield(250000);

			if (b_channel && (switch_channel_test_flag(session->channel, CF_BYPASS_MEDIA_AFTER_HOLD) ||
							  switch_channel_test_flag(b_channel, CF_BYPASS_MEDIA_AFTER_HOLD) || bypass_after_hold_a || bypass_after_hold_b)) {
				/* try to stay out from media stream */
				switch_ivr_nomedia(switch_core_session_get_uuid(session), SMF_REBRIDGE);
			}

			if (a_engine->max_missed_packets && a_engine->rtp_session) {
				switch_rtp_reset_media_timer(a_engine->rtp_session);
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_packets);
			}

			if (b_channel) {
				if (switch_channel_test_flag(session->channel, CF_HOLD)) {
					switch_ivr_unhold(b_session);
					switch_channel_clear_flag(session->channel, CF_SUSPEND);
					switch_channel_clear_flag(session->channel, CF_HOLD);
				} else {
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
				}
			}

			switch_core_media_check_autoadj(session);

			switch_channel_clear_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_FALSE);
			switch_channel_presence(session->channel, "unknown", "unhold", NULL);

			if (a_engine->rtp_session) {
				switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			if (v_engine->rtp_session) {
				switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
			}

			changed = 1;
		}
	}


 end:

	if (b_session) {
		switch_core_session_rwunlock(b_session);
	}


	return changed;
}

static void *SWITCH_THREAD_FUNC video_helper_thread(switch_thread_t *thread, void *obj)
{
	struct media_helper *mh = obj;
	switch_core_session_t *session = mh->session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	switch_frame_t *read_frame;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	switch_core_session_read_lock(session);

	mh->up = 1;
	switch_mutex_lock(mh->cond_mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread started. Echo is %s\n", 
					  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
	switch_core_session_refresh_video(session);

	while (switch_channel_up_nosig(channel)) {

		if (switch_channel_test_flag(channel, CF_VIDEO_PASSIVE)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread paused. Echo is %s\n", 
							  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
			switch_thread_cond_wait(mh->cond, mh->cond_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread resumed  Echo is %s\n", 
							  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
			switch_core_session_refresh_video(session);
		}

		if (switch_channel_test_flag(channel, CF_VIDEO_PASSIVE)) {
			continue;
		}

		if (!switch_channel_media_up(session->channel)) {
			switch_yield(10000);
			continue;
		}

		
		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			switch_cond_next();
			continue;
		}
		

		if (switch_channel_test_flag(channel, CF_VIDEO_REFRESH_REQ)) {
			switch_core_session_refresh_video(session);
			switch_channel_clear_flag(channel, CF_VIDEO_REFRESH_REQ);
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (switch_channel_test_flag(channel, CF_VIDEO_ECHO)) {
			switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
		}

	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread ended\n", switch_channel_get_name(session->channel));

	switch_mutex_unlock(mh->cond_mutex);
	switch_core_session_rwunlock(session);

	mh->up = 0;
	return NULL;
}


static switch_status_t start_video_thread(switch_core_session_t *session)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_rtp_engine_t *v_engine = NULL;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (v_engine->media_thread) {
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s Starting Video thread\n", switch_core_session_get_name(session));

	if (v_engine->rtp_session) {
		switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->agreed_pt);
	}

	v_engine->mh.session = session;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	
	switch_thread_cond_create(&v_engine->mh.cond, pool);
	switch_mutex_init(&v_engine->mh.cond_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&v_engine->read_mutex[SWITCH_MEDIA_TYPE_VIDEO], SWITCH_MUTEX_NESTED, pool);
	switch_thread_create(&v_engine->media_thread, thd_attr, video_helper_thread, &v_engine->mh, switch_core_session_get_pool(session));

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_start_video_thread(switch_core_session_t *session)
{
	return start_video_thread(session);
}

//?
#define RA_PTR_LEN 512
SWITCH_DECLARE(switch_status_t) switch_core_media_proxy_remote_addr(switch_core_session_t *session, const char *sdp_str)
{
	const char *err;
	char rip[RA_PTR_LEN] = "";
	char rp[RA_PTR_LEN] = "";
	char rvp[RA_PTR_LEN] = "";
	char *p, *ip_ptr = NULL, *port_ptr = NULL, *vid_port_ptr = NULL, *pe;
	int x;
	const char *val;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	
	if (zstr(sdp_str)) {
		sdp_str = smh->mparams->remote_sdp_str;
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "invalid SDP\n");
		goto end;
	}

	a_engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(session, rip);
	a_engine->cur_payload_map->remote_sdp_port = (switch_port_t) atoi(rp);

	if (*rvp) {
		v_engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(session, rip);
		v_engine->cur_payload_map->remote_sdp_port = (switch_port_t) atoi(rvp);
		switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
		switch_channel_set_flag(session->channel, CF_VIDEO);
	}

	if (v_engine->cur_payload_map->remote_sdp_ip && v_engine->cur_payload_map->remote_sdp_port) {
		if (!strcmp(v_engine->cur_payload_map->remote_sdp_ip, rip) && atoi(rvp) == v_engine->cur_payload_map->remote_sdp_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote video address:port [%s:%d] has not changed.\n",
							  v_engine->cur_payload_map->remote_sdp_ip, v_engine->cur_payload_map->remote_sdp_port);
		} else {
			switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
			switch_channel_set_flag(session->channel, CF_VIDEO);
			if (switch_rtp_ready(v_engine->rtp_session)) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = v_engine->remote_rtcp_port;

				if (!remote_rtcp_port) {
					if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_video_rtcp_port"))) {
						remote_rtcp_port = (switch_port_t)atoi(rport);
					}
				}

				
				if (switch_rtp_set_remote_address(v_engine->rtp_session, v_engine->cur_payload_map->remote_sdp_ip,
												  v_engine->cur_payload_map->remote_sdp_port, remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  v_engine->cur_payload_map->remote_sdp_ip, v_engine->cur_payload_map->remote_sdp_port);
					if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_PROXY_MODE) &&
						!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) && 
						!switch_channel_test_flag(session->channel, CF_WEBRTC)) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
						start_video_thread(session);
				
					}
					if (switch_media_handle_test_media_flag(smh, SCMF_AUTOFIX_TIMING)) {
						v_engine->check_frames = 0;
					}
				}
			}
		}
	}

	if (switch_rtp_ready(a_engine->rtp_session)) {
		char *remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);
		const char *rport = NULL;
		switch_port_t remote_rtcp_port = 0;

		if (remote_host && remote_port && !strcmp(remote_host, a_engine->cur_payload_map->remote_sdp_ip) && remote_port == a_engine->cur_payload_map->remote_sdp_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote address:port [%s:%d] has not changed.\n",
							  a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);
			switch_goto_status(SWITCH_STATUS_BREAK, end);
		} else if (remote_host && ( (strcmp(remote_host, "0.0.0.0") == 0) ||
									(strcmp(a_engine->cur_payload_map->remote_sdp_ip, "0.0.0.0") == 0))) {
			
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							  "Remote address changed from [%s] to [%s]. Ignoring...\n",
							  a_engine->cur_payload_map->remote_sdp_ip, remote_host);
			switch_goto_status(SWITCH_STATUS_BREAK, end);
		}

		if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_audio_rtcp_port"))) {
			remote_rtcp_port = (switch_port_t)atoi(rport);
		}


		if (switch_rtp_set_remote_address(a_engine->rtp_session, a_engine->cur_payload_map->remote_sdp_ip,
										  a_engine->cur_payload_map->remote_sdp_port, remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
			status = SWITCH_STATUS_GENERR;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);
			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) && 
				!switch_channel_test_flag(session->channel, CF_WEBRTC)) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
			if (switch_media_handle_test_media_flag(smh, SCMF_AUTOFIX_TIMING)) {
				a_engine->check_frames = 0;
			}
			status = SWITCH_STATUS_SUCCESS;
		}
	}

 end:

	return status;
}

//?
SWITCH_DECLARE(int) switch_core_media_check_nat(switch_media_handle_t *smh, const char *network_ip)
{
	switch_assert(network_ip);

	return (smh->mparams->extsipip && 
			!switch_check_network_list_ip(network_ip, "loopback.auto") && 
			!switch_check_network_list_ip(network_ip, smh->mparams->local_network));
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_ext_address_lookup(switch_core_session_t *session, char **ip, switch_port_t *port, const char *sourceip)
															  
{
	char *error = "";
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x;
	switch_port_t myport = *port;
	switch_port_t stun_port = SWITCH_STUN_DEFAULT_PORT;
	char *stun_ip = NULL;
	switch_media_handle_t *smh;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!sourceip) {
		return status;
	}

	if (!strncasecmp(sourceip, "host:", 5)) {
		status = (*ip = switch_stun_host_lookup(sourceip + 5, pool)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	} else if (!strncasecmp(sourceip, "stun:", 5)) {
		char *p;

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

		if (myport == *port && !strcmp(*ip, smh->mparams->rtpip)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "STUN Not Required ip and port match. [%s]:[%d]\n", *ip, *port);
		} else {
			smh->mparams->stun_ip = switch_core_session_strdup(session, stun_ip);
			smh->mparams->stun_port = stun_port;
			smh->mparams->stun_flags |= STUN_FLAG_SET;
		}
	} else {
		*ip = (char *) sourceip;
		status = SWITCH_STATUS_SUCCESS;
	}

 out:

	switch_safe_free(stun_ip);

	return status;
}

//?
SWITCH_DECLARE(void) switch_core_media_reset_autofix(switch_core_session_t *session, switch_media_type_t type)
{
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

	engine->check_frames = 0;
	engine->last_ts = 0;
	engine->last_seq = 0;
}



//?
SWITCH_DECLARE(switch_status_t) switch_core_media_choose_port(switch_core_session_t *session, switch_media_type_t type, int force)
{
	char *lookup_rtpip;	/* Pointer to externally looked up address */
	switch_port_t sdp_port;		/* The external port to be sent in the SDP */
	const char *use_ip = NULL;	/* The external IP to be sent in the SDP */
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;
	const char *tstr = switch_media_type2str(type);
	char vname[128] = "";

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	lookup_rtpip = smh->mparams->rtpip;

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) || engine->adv_sdp_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Release the local sdp port */
	if (engine->local_sdp_port) {
		switch_rtp_release_port(smh->mparams->rtpip, engine->local_sdp_port);
	}

	/* Request a local port from the core's allocator */
	if (!(engine->local_sdp_port = switch_rtp_request_port(smh->mparams->rtpip))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "No %s RTP ports available!\n", tstr);
		return SWITCH_STATUS_FALSE;
	}

	engine->local_sdp_ip = smh->mparams->rtpip;
	

	sdp_port = engine->local_sdp_port;

	/* Check if NAT is detected  */
	if (!zstr(smh->mparams->remote_ip) && switch_core_media_check_nat(smh, smh->mparams->remote_ip)) {
		/* Yes, map the port through switch_nat */
		switch_nat_add_mapping(engine->local_sdp_port, SWITCH_NAT_UDP, &sdp_port, SWITCH_FALSE);

		switch_snprintf(vname, sizeof(vname), "rtp_adv_%s_ip", tstr);
		
		/* Find an IP address to use */
		if (!(use_ip = switch_channel_get_variable(session->channel, vname))
			&& !zstr(smh->mparams->extrtpip)) {
			use_ip = smh->mparams->extrtpip;
		}

		if (use_ip) {
			if (switch_core_media_ext_address_lookup(session, &lookup_rtpip, &sdp_port, use_ip) != SWITCH_STATUS_SUCCESS) {
				/* Address lookup was required and fail (external ip was "host:..." or "stun:...") */
				return SWITCH_STATUS_FALSE;
			} else {
				/* Address properly resolved, use it as external ip */
				use_ip = lookup_rtpip;
			}
		} else {
			/* No external ip found, use the profile's rtp ip */
			use_ip = smh->mparams->rtpip;
		}
	} else {
		/* No NAT traversal required, use the profile's rtp ip */
		use_ip = smh->mparams->rtpip;
	}

	engine->adv_sdp_port = sdp_port;
	engine->adv_sdp_ip = smh->mparams->adv_sdp_audio_ip = smh->mparams->extrtpip = switch_core_session_strdup(session, use_ip);


	if (type == SWITCH_MEDIA_TYPE_AUDIO) {
		switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, engine->local_sdp_ip);
		switch_channel_set_variable_printf(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", sdp_port);
		switch_channel_set_variable(session->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, engine->adv_sdp_ip);
	} else {
		switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, engine->adv_sdp_ip);
		switch_channel_set_variable_printf(session->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, "%d", sdp_port);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_choose_ports(switch_core_session_t *session, switch_bool_t audio, switch_bool_t video)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (audio && (status = switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_AUDIO, 0)) == SWITCH_STATUS_SUCCESS) {
		if (video) {
			switch_core_media_check_video_codecs(session);
			if (switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE)) {
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 0);
			}
		}
	}

	return status;
}



//?
SWITCH_DECLARE(void) switch_core_media_deactivate_rtp(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (v_engine->media_thread) {
		switch_status_t st;
		switch_channel_clear_flag(session->channel, CF_VIDEO_PASSIVE);
		
		v_engine->mh.up = 0;
		switch_thread_join(&st, v_engine->media_thread);
		v_engine->media_thread = NULL;
	}

	if (v_engine->rtp_session) {
		switch_rtp_destroy(&v_engine->rtp_session);
	} else if (v_engine->local_sdp_port) {
		switch_rtp_release_port(smh->mparams->rtpip, v_engine->local_sdp_port);
	}


	if (v_engine->local_sdp_port > 0 && !zstr(smh->mparams->remote_ip) && 
		switch_core_media_check_nat(smh, smh->mparams->remote_ip)) {
		switch_nat_del_mapping((switch_port_t) v_engine->local_sdp_port, SWITCH_NAT_UDP);
		switch_nat_del_mapping((switch_port_t) v_engine->local_sdp_port + 1, SWITCH_NAT_UDP);
	}


	if (a_engine->rtp_session) {
		switch_rtp_destroy(&a_engine->rtp_session);
	} else if (a_engine->local_sdp_port) {
		switch_rtp_release_port(smh->mparams->rtpip, a_engine->local_sdp_port);
	}

	if (a_engine->local_sdp_port > 0 && !zstr(smh->mparams->remote_ip) && 
		switch_core_media_check_nat(smh, smh->mparams->remote_ip)) {
		switch_nat_del_mapping((switch_port_t) a_engine->local_sdp_port, SWITCH_NAT_UDP);
		switch_nat_del_mapping((switch_port_t) a_engine->local_sdp_port + 1, SWITCH_NAT_UDP);
	}

}


//?
static void gen_ice(switch_core_session_t *session, switch_media_type_t type, const char *ip, switch_port_t port)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	char tmp[33] = "";

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

#ifdef RTCP_MUX
	if (!engine->rtcp_mux) {//  && type == SWITCH_MEDIA_TYPE_AUDIO) {
		engine->rtcp_mux = SWITCH_TRUE;
	}
#endif

	if (!smh->msid) {
		switch_stun_random_string(tmp, 32, NULL);
		tmp[32] = '\0';
		smh->msid = switch_core_session_strdup(session, tmp);
	}

	if (!smh->cname) {
		switch_stun_random_string(tmp, 16, NULL);
		tmp[16] = '\0';
		smh->cname = switch_core_session_strdup(session, tmp);
	}
	
	if (!engine->ice_out.ufrag) {
		switch_stun_random_string(tmp, 16, NULL);
		tmp[16] = '\0';
		engine->ice_out.ufrag = switch_core_session_strdup(session, tmp);
	}

	if (!engine->ice_out.pwd) {
		switch_stun_random_string(tmp, 24, NULL);
		tmp[24] = '\0';
		engine->ice_out.pwd = switch_core_session_strdup(session, tmp);	
	}

	if (!engine->ice_out.cands[0][0].foundation) {
		switch_stun_random_string(tmp, 10, "0123456789");
		tmp[10] = '\0';
		engine->ice_out.cands[0][0].foundation = switch_core_session_strdup(session, tmp);
	}

	engine->ice_out.cands[0][0].transport = "udp";

	if (!engine->ice_out.cands[0][0].component_id) {
		engine->ice_out.cands[0][0].component_id = 1;
		engine->ice_out.cands[0][0].priority = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - engine->ice_out.cands[0][0].component_id);
	}

	if (!zstr(ip)) {
		engine->ice_out.cands[0][0].con_addr = switch_core_session_strdup(session, ip);
	}

	if (port) {
		engine->ice_out.cands[0][0].con_port = port;
	}

	engine->ice_out.cands[0][0].generation = "0";
	//add rport stuff later
	
	engine->ice_out.cands[0][0].ready = 1;


}

SWITCH_DECLARE(void) switch_core_session_wake_video_thread(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine; 

	if (!(smh = session->media_handle)) {
		return;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if ((!smh->mparams->external_video_source) && (!v_engine->rtp_session)) {
		return;
	}

	if (!v_engine->mh.cond_mutex) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Channel %s has no cond?\n",
						  switch_channel_get_name(session->channel));
		return;
	}

	if (switch_mutex_trylock(v_engine->mh.cond_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_broadcast(v_engine->mh.cond);
		switch_mutex_unlock(v_engine->mh.cond_mutex);
	}
}

static void check_dtls_reinvite(switch_core_session_t *session, switch_rtp_engine_t *engine)
{
	if (switch_channel_test_flag(session->channel, CF_REINVITE)) {

		if (!zstr(engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(session)) {
			dtls_type_t xtype, dtype = switch_ice_direction(session) == SWITCH_CALL_DIRECTION_INBOUND ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "RE-SETTING %s DTLS\n", type2str(engine->type));
		
			xtype = DTLS_TYPE_RTP;
			if (engine->rtcp_mux > 0) xtype |= DTLS_TYPE_RTCP;
			
			switch_rtp_add_dtls(engine->rtp_session, &engine->local_dtls_fingerprint, &engine->remote_dtls_fingerprint, dtype | xtype);
		
			if (engine->rtcp_mux < 1) {
				xtype = DTLS_TYPE_RTCP;
				switch_rtp_add_dtls(engine->rtp_session, &engine->local_dtls_fingerprint, &engine->remote_dtls_fingerprint, dtype | xtype);
			}
		
		}
	}
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_activate_rtp(switch_core_session_t *session)

{
	const char *err = NULL;
	const char *val = NULL;
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char tmp[50];
	char *timer_name = NULL;
	const char *var;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);
	
	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];


	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}


	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_rtp_reset_media_timer(a_engine->rtp_session);
	}

	if (a_engine->crypto_type != CRYPTO_INVALID) {
		switch_channel_set_flag(session->channel, CF_SECURE);
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if (!switch_channel_test_flag(session->channel, CF_REINVITE)) {
		if (switch_rtp_ready(a_engine->rtp_session)) {
			if (switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE) && !switch_rtp_ready(v_engine->rtp_session)) {
				goto video;
			}

			status = SWITCH_STATUS_SUCCESS;
			goto end;
		} 
	}

	if ((status = switch_core_media_set_codec(session, 0, smh->mparams->codec_flags)) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	switch_core_media_set_video_codec(session, 0);


	memset(flags, 0, sizeof(flags));
	flags[SWITCH_RTP_FLAG_DATAWAIT]++;

	if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_WEBRTC) &&
		!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
		flags[SWITCH_RTP_FLAG_AUTOADJ]++;
	}

	if (switch_media_handle_test_media_flag(smh, SCMF_PASS_RFC2833)
		|| ((val = switch_channel_get_variable(session->channel, "pass_rfc2833")) && switch_true(val))) {
		switch_channel_set_flag(session->channel, CF_PASS_RFC2833);
	}


	if (switch_media_handle_test_media_flag(smh, SCMF_AUTOFLUSH)
		|| ((val = switch_channel_get_variable(session->channel, "rtp_autoflush")) && switch_true(val))) {
		flags[SWITCH_RTP_FLAG_AUTOFLUSH]++;
	}

	if (!(switch_media_handle_test_media_flag(smh, SCMF_REWRITE_TIMESTAMPS) ||
		  ((val = switch_channel_get_variable(session->channel, "rtp_rewrite_timestamps")) && switch_true(val)))) {
		flags[SWITCH_RTP_FLAG_RAW_WRITE]++;
	}

	if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) {
		smh->mparams->cng_pt = 0;
	} else if (smh->mparams->cng_pt) {
		flags[SWITCH_RTP_FLAG_AUTO_CNG]++;
	}

#if __BYTE_ORDER == __LITTLE_ENDIAN
	if (!strcasecmp(a_engine->read_impl.iananame, "L16")) {
		flags[SWITCH_RTP_FLAG_BYTESWAP]++;
	}
#endif
	
	if ((flags[SWITCH_RTP_FLAG_BYTESWAP]) && (val = switch_channel_get_variable(session->channel, "rtp_disable_byteswap")) && switch_true(val)) {
		flags[SWITCH_RTP_FLAG_BYTESWAP] = 0;
	}

	if (a_engine->rtp_session && switch_channel_test_flag(session->channel, CF_REINVITE)) {
		//const char *ip = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
		//const char *port = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
		char *remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);

		if (remote_host && remote_port && !strcmp(remote_host, a_engine->cur_payload_map->remote_sdp_ip) && 
			remote_port == a_engine->cur_payload_map->remote_sdp_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n",
							  switch_channel_get_name(session->channel));
			a_engine->cur_payload_map->negotiated = 1;
			//XX
			goto video;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n",
							  switch_channel_get_name(session->channel),
							  remote_host, remote_port, a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);

			switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->remote_sdp_port);
			switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->cur_payload_map->remote_sdp_ip);
			switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
		}
	}

	if (!switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "AUDIO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n",
						  switch_channel_get_name(session->channel),
						  a_engine->local_sdp_ip,
						  a_engine->local_sdp_port,
						  a_engine->cur_payload_map->remote_sdp_ip,
						  a_engine->cur_payload_map->remote_sdp_port, a_engine->cur_payload_map->agreed_pt, a_engine->read_impl.microseconds_per_packet / 1000);

		//XX
	}

	switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->local_sdp_port);
	switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, a_engine->local_sdp_ip);
	switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);
	switch_channel_set_variable(session->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, a_engine->adv_sdp_ip);

	if (a_engine->rtp_session && switch_channel_test_flag(session->channel, CF_REINVITE)) {
		const char *rport = NULL;
		switch_port_t remote_rtcp_port = a_engine->remote_rtcp_port;
				
		if (!remote_rtcp_port) {
			if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_audio_rtcp_port"))) {
				remote_rtcp_port = (switch_port_t)atoi(rport);
			}
		}

		if (switch_rtp_set_remote_address(a_engine->rtp_session, a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port,
										  remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "AUDIO RTP CHANGING DEST TO: [%s:%d]\n",
							  a_engine->cur_payload_map->remote_sdp_ip, a_engine->cur_payload_map->remote_sdp_port);

			if (switch_channel_test_flag(session->channel, CF_PROTO_HOLD) && strcmp(a_engine->cur_payload_map->remote_sdp_ip, "0.0.0.0")) {
				switch_core_media_toggle_hold(session, 0);
			}


			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
				!switch_channel_test_flag(session->channel, CF_WEBRTC)) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}

		if (session && a_engine) {
			check_dtls_reinvite(session, a_engine);
		}

		goto video;
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
		switch_core_media_proxy_remote_addr(session, NULL);

		memset(flags, 0, sizeof(flags));
		flags[SWITCH_RTP_FLAG_DATAWAIT]++;
		flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;

		if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_WEBRTC) &&
			!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
			flags[SWITCH_RTP_FLAG_AUTOADJ]++;
		}
		timer_name = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						  "PROXY AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
						  switch_channel_get_name(session->channel),
						  a_engine->cur_payload_map->remote_sdp_ip,
						  a_engine->cur_payload_map->remote_sdp_port,
						  a_engine->cur_payload_map->remote_sdp_ip,
						  a_engine->cur_payload_map->remote_sdp_port, a_engine->cur_payload_map->agreed_pt, a_engine->read_impl.microseconds_per_packet / 1000);

		if (switch_rtp_ready(a_engine->rtp_session)) {
			switch_rtp_set_default_payload(a_engine->rtp_session, a_engine->cur_payload_map->agreed_pt);
		}

	} else {
		timer_name = smh->mparams->timer_name;

		if ((var = switch_channel_get_variable(session->channel, "rtp_timer_name"))) {
			timer_name = (char *) var;
		}
	}

	if (switch_channel_up(session->channel)) {
		switch_channel_set_variable(session->channel, "rtp_use_timer_name", timer_name);
		
		a_engine->rtp_session = switch_rtp_new(a_engine->local_sdp_ip,
											   a_engine->local_sdp_port,
											   a_engine->cur_payload_map->remote_sdp_ip,
											   a_engine->cur_payload_map->remote_sdp_port,
											   a_engine->cur_payload_map->agreed_pt,
											   a_engine->read_impl.samples_per_packet,
											   a_engine->cur_payload_map->codec_ms * 1000,
											   flags, timer_name, &err, switch_core_session_get_pool(session));

		if (switch_rtp_ready(a_engine->rtp_session)) {
			switch_rtp_set_payload_map(a_engine->rtp_session, &a_engine->payload_map);
		}
	}

	if (switch_rtp_ready(a_engine->rtp_session)) {
		uint8_t vad_in = (smh->mparams->vflags & VAD_IN);
		uint8_t vad_out = (smh->mparams->vflags & VAD_OUT);
		uint8_t inb = switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND;
		const char *ssrc;

		switch_mutex_init(&a_engine->read_mutex[SWITCH_MEDIA_TYPE_AUDIO], SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

		//switch_core_media_set_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO, a_engine->rtp_session);

		if ((ssrc = switch_channel_get_variable(session->channel, "rtp_use_ssrc"))) {
			uint32_t ssrc_ul = (uint32_t) strtoul(ssrc, NULL, 10);
			switch_rtp_set_ssrc(a_engine->rtp_session, ssrc_ul);
			a_engine->ssrc = ssrc_ul;
		} else {
			switch_rtp_set_ssrc(a_engine->rtp_session, a_engine->ssrc);
		}

		if (a_engine->remote_ssrc) {
			switch_rtp_set_remote_ssrc(a_engine->rtp_session, a_engine->remote_ssrc);
		}

		switch_channel_set_flag(session->channel, CF_FS_RTP);

		switch_channel_set_variable_printf(session->channel, "rtp_use_pt", "%d", a_engine->cur_payload_map->agreed_pt);

		if ((val = switch_channel_get_variable(session->channel, "rtp_enable_vad_in")) && switch_true(val)) {
			vad_in = 1;
		}
		if ((val = switch_channel_get_variable(session->channel, "rtp_enable_vad_out")) && switch_true(val)) {
			vad_out = 1;
		}

		if ((val = switch_channel_get_variable(session->channel, "rtp_disable_vad_in")) && switch_true(val)) {
			vad_in = 0;
		}
		if ((val = switch_channel_get_variable(session->channel, "rtp_disable_vad_out")) && switch_true(val)) {
			vad_out = 0;
		}


		a_engine->ssrc = switch_rtp_get_ssrc(a_engine->rtp_session);
		switch_channel_set_variable_printf(session->channel, "rtp_use_ssrc", "%u", a_engine->ssrc);



		if (smh->mparams->auto_rtp_bugs & RTP_BUG_IGNORE_MARK_BIT) {
			a_engine->rtp_bugs |= RTP_BUG_IGNORE_MARK_BIT;
		}

		if ((val = switch_channel_get_variable(session->channel, "rtp_manual_rtp_bugs"))) {
			switch_core_media_parse_rtp_bugs(&a_engine->rtp_bugs, val);
		}
		
		if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
			smh->mparams->manual_rtp_bugs = RTP_BUG_SEND_LINEAR_TIMESTAMPS;
		}

		switch_rtp_intentional_bugs(a_engine->rtp_session, a_engine->rtp_bugs | smh->mparams->manual_rtp_bugs);

		if ((vad_in && inb) || (vad_out && !inb)) {
			switch_rtp_enable_vad(a_engine->rtp_session, session, &a_engine->read_codec, SWITCH_VAD_FLAG_TALKING | SWITCH_VAD_FLAG_EVENTS_TALK | SWITCH_VAD_FLAG_EVENTS_NOTALK);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "AUDIO RTP Engage VAD for %s ( %s %s )\n",
							  switch_channel_get_name(switch_core_session_get_channel(session)), vad_in ? "in" : "", vad_out ? "out" : "");
		}

		
		if (a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].ready) {
			
			gen_ice(session, SWITCH_MEDIA_TYPE_AUDIO, NULL, 0);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating Audio ICE\n");

			switch_rtp_activate_ice(a_engine->rtp_session, 
									a_engine->ice_in.ufrag,
									a_engine->ice_out.ufrag,
									a_engine->ice_out.pwd,
									a_engine->ice_in.pwd,
									IPR_RTP,
#ifdef GOOGLE_ICE
									ICE_GOOGLE_JINGLE,
									NULL
#else
									switch_ice_direction(session) == 
									SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
									&a_engine->ice_in
#endif
									);

		
			
		}



		if ((val = switch_channel_get_variable(session->channel, "rtcp_audio_interval_msec")) || (val = smh->mparams->rtcp_audio_interval_msec)) {
			const char *rport = switch_channel_get_variable(session->channel, "rtp_remote_audio_rtcp_port");
			switch_port_t remote_rtcp_port = a_engine->remote_rtcp_port;

			if (!remote_rtcp_port && rport) {
				remote_rtcp_port = (switch_port_t)atoi(rport);
			}
			
			if (!strcasecmp(val, "passthru")) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating RTCP PASSTHRU PORT %d\n", remote_rtcp_port);
				switch_rtp_activate_rtcp(a_engine->rtp_session, -1, remote_rtcp_port, a_engine->rtcp_mux > 0);
			} else {
				int interval = atoi(val);
				if (interval < 100 || interval > 500000) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
									  "Invalid rtcp interval spec [%d] must be between 100 and 500000\n", interval);
					interval = 10000;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating RTCP PORT %d\n", remote_rtcp_port);
				switch_rtp_activate_rtcp(a_engine->rtp_session, interval, remote_rtcp_port, a_engine->rtcp_mux > 0);
				
			}

			if (a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].ready) {
				if (a_engine->rtcp_mux > 0 && !strcmp(a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].con_addr, a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].con_addr)
					&& a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].con_port == a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].con_port) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Skipping RTCP ICE (Same as RTP)\n");
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating RTCP ICE\n");
				
					switch_rtp_activate_ice(a_engine->rtp_session, 
											a_engine->ice_in.ufrag,
											a_engine->ice_out.ufrag,
											a_engine->ice_out.pwd,
											a_engine->ice_in.pwd,
											IPR_RTCP,
#ifdef GOOGLE_ICE
											ICE_GOOGLE_JINGLE,
											NULL
#else
											switch_ice_direction(session) == 
											SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
											&a_engine->ice_in
#endif
										);
				}
				
			}
		}

		if (!zstr(a_engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(smh->session)) {
			dtls_type_t xtype, dtype = switch_channel_direction(smh->session->channel) == SWITCH_CALL_DIRECTION_INBOUND ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;

			if (switch_channel_test_flag(smh->session->channel, CF_3PCC)) {
				dtype = (dtype == DTLS_TYPE_CLIENT) ? DTLS_TYPE_SERVER : DTLS_TYPE_CLIENT;
			}

			xtype = DTLS_TYPE_RTP;
			if (a_engine->rtcp_mux > 0 && smh->mparams->rtcp_audio_interval_msec) xtype |= DTLS_TYPE_RTCP;
		
			switch_rtp_add_dtls(a_engine->rtp_session, &a_engine->local_dtls_fingerprint, &a_engine->remote_dtls_fingerprint, dtype | xtype);

			if (a_engine->rtcp_mux < 1 && smh->mparams->rtcp_audio_interval_msec) {
				xtype = DTLS_TYPE_RTCP;
				switch_rtp_add_dtls(a_engine->rtp_session, &a_engine->local_dtls_fingerprint, &a_engine->remote_dtls_fingerprint, dtype | xtype);
			}

		}

		check_jb(session, NULL);

		if ((val = switch_channel_get_variable(session->channel, "rtp_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				smh->mparams->rtp_timeout_sec = v;
			}
		}

		if ((val = switch_channel_get_variable(session->channel, "rtp_hold_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				smh->mparams->rtp_hold_timeout_sec = v;
			}
		}

		if (smh->mparams->rtp_timeout_sec) {
			a_engine->max_missed_packets = (a_engine->read_impl.samples_per_second * smh->mparams->rtp_timeout_sec) / a_engine->read_impl.samples_per_packet;

			switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_packets);
			if (!smh->mparams->rtp_hold_timeout_sec) {
				smh->mparams->rtp_hold_timeout_sec = smh->mparams->rtp_timeout_sec * 10;
			}
		}

		if (smh->mparams->rtp_hold_timeout_sec) {
			a_engine->max_missed_hold_packets = (a_engine->read_impl.samples_per_second * smh->mparams->rtp_hold_timeout_sec) / a_engine->read_impl.samples_per_packet;
		}

		if (smh->mparams->te) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send payload to %u\n", smh->mparams->te);
			switch_rtp_set_telephony_event(a_engine->rtp_session, smh->mparams->te);
			switch_channel_set_variable_printf(session->channel, "rtp_2833_send_payload", "%d", smh->mparams->te);
		}

		if (smh->mparams->recv_te) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf receive payload to %u\n", smh->mparams->recv_te);
			switch_rtp_set_telephony_recv_event(a_engine->rtp_session, smh->mparams->recv_te);
			switch_channel_set_variable_printf(session->channel, "rtp_2833_recv_payload", "%d", smh->mparams->recv_te);
		}

		//XX

		if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) ||
			((val = switch_channel_get_variable(session->channel, "supress_cng")) && switch_true(val)) ||
			((val = switch_channel_get_variable(session->channel, "suppress_cng")) && switch_true(val))) {
			smh->mparams->cng_pt = 0;
		}

		if (((val = switch_channel_get_variable(session->channel, "rtp_digit_delay")))) {
			int delayi = atoi(val);
			if (delayi < 0) delayi = 0;
			smh->mparams->dtmf_delay = (uint32_t) delayi;
		}


		if (smh->mparams->dtmf_delay) {
			switch_rtp_set_interdigit_delay(a_engine->rtp_session, smh->mparams->dtmf_delay);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
							  "%s Set rtp dtmf delay to %u\n", switch_channel_get_name(session->channel), smh->mparams->dtmf_delay);
			
		}

		if (smh->mparams->cng_pt && !switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", smh->mparams->cng_pt);
			switch_rtp_set_cng_pt(a_engine->rtp_session, smh->mparams->cng_pt);
		}

		switch_core_session_apply_crypto(session, SWITCH_MEDIA_TYPE_AUDIO);

		switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->remote_sdp_port);
		switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->cur_payload_map->remote_sdp_ip);
		switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);


		if (switch_channel_test_flag(session->channel, CF_ZRTP_PASSTHRU)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating ZRTP PROXY MODE\n");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Disable NOTIMER_DURING_BRIDGE\n");
			switch_channel_clear_flag(session->channel, CF_NOTIMER_DURING_BRIDGE);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activating audio UDPTL mode\n");
			switch_rtp_udptl_mode(a_engine->rtp_session);
		}


		
	

	
	video:
	
		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_core_media_check_video_codecs(session);
		}

		if (switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE) && v_engine->cur_payload_map->rm_encoding && v_engine->cur_payload_map->remote_sdp_port) {
			/******************************************************************************************/
			if (v_engine->rtp_session && switch_channel_test_flag(session->channel, CF_REINVITE)) {
				//const char *ip = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
				//const char *port = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
				char *remote_host = switch_rtp_get_remote_host(v_engine->rtp_session);
				switch_port_t remote_port = switch_rtp_get_remote_port(v_engine->rtp_session);
				


				if (remote_host && remote_port && !strcmp(remote_host, v_engine->cur_payload_map->remote_sdp_ip) && remote_port == v_engine->cur_payload_map->remote_sdp_port) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video params are unchanged for %s.\n",
									  switch_channel_get_name(session->channel));
					v_engine->cur_payload_map->negotiated = 1;
					goto video_up;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video params changed for %s from %s:%d to %s:%d\n",
									  switch_channel_get_name(session->channel),
									  remote_host, remote_port, v_engine->cur_payload_map->remote_sdp_ip, v_engine->cur_payload_map->remote_sdp_port);
				}
			}

			if (!switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				if (switch_rtp_ready(v_engine->rtp_session)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "VIDEO RTP [%s] %s port %d -> %s port %d codec: %u\n", switch_channel_get_name(session->channel),
									  v_engine->local_sdp_ip, v_engine->local_sdp_port, v_engine->cur_payload_map->remote_sdp_ip,
									  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->agreed_pt);


					start_video_thread(session);
					switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->agreed_pt);
				}
			}
			
			switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->local_sdp_port);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, a_engine->adv_sdp_ip);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, tmp);


			if (v_engine->rtp_session && switch_channel_test_flag(session->channel, CF_REINVITE)) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = v_engine->remote_rtcp_port;

				//switch_channel_clear_flag(session->channel, CF_REINVITE);

				if (!remote_rtcp_port) {
					if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_video_rtcp_port"))) {
						remote_rtcp_port = (switch_port_t)atoi(rport);
					}
				}
				
				if (switch_rtp_set_remote_address
					(v_engine->rtp_session, v_engine->cur_payload_map->remote_sdp_ip, v_engine->cur_payload_map->remote_sdp_port, remote_rtcp_port, SWITCH_TRUE,
					 &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "VIDEO RTP CHANGING DEST TO: [%s:%d]\n",
									  v_engine->cur_payload_map->remote_sdp_ip, v_engine->cur_payload_map->remote_sdp_port);
					if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_WEBRTC) &&
						!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
						start_video_thread(session);
					}

				}
				goto video_up;
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				switch_core_media_proxy_remote_addr(session, NULL);

				memset(flags, 0, sizeof(flags));
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
				flags[SWITCH_RTP_FLAG_DATAWAIT]++;

				if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_WEBRTC) &&
					!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
					flags[SWITCH_RTP_FLAG_AUTOADJ]++;
				}
				timer_name = NULL;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "PROXY VIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
								  switch_channel_get_name(session->channel),
								  a_engine->cur_payload_map->remote_sdp_ip,
								  v_engine->local_sdp_port,
								  v_engine->cur_payload_map->remote_sdp_ip,
								  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->agreed_pt, v_engine->read_impl.microseconds_per_packet / 1000);

				if (switch_rtp_ready(v_engine->rtp_session)) {
					switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->agreed_pt);
				}
			} else {
				timer_name = smh->mparams->timer_name;

				if ((var = switch_channel_get_variable(session->channel, "rtp_timer_name"))) {
					timer_name = (char *) var;
				}
			}

			/******************************************************************************************/

			if (v_engine->rtp_session) {
				goto video_up;
			}


			if (!v_engine->local_sdp_port) {
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 1);
			}

			memset(flags, 0, sizeof(flags));
			flags[SWITCH_RTP_FLAG_DATAWAIT]++;
			flags[SWITCH_RTP_FLAG_RAW_WRITE]++;

			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) && 
				!switch_channel_test_flag(session->channel, CF_WEBRTC)) {
				flags[SWITCH_RTP_FLAG_AUTOADJ]++;				
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
			}
			switch_core_media_set_video_codec(session, 0);

			flags[SWITCH_RTP_FLAG_USE_TIMER] = 0;
			flags[SWITCH_RTP_FLAG_NOBLOCK] = 0;
			flags[SWITCH_RTP_FLAG_VIDEO]++;

			v_engine->rtp_session = switch_rtp_new(a_engine->local_sdp_ip,
														 v_engine->local_sdp_port,
														 v_engine->cur_payload_map->remote_sdp_ip,
														 v_engine->cur_payload_map->remote_sdp_port,
														 v_engine->cur_payload_map->agreed_pt,
														 1, 90000, flags, NULL, &err, switch_core_session_get_pool(session));


			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%sVIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(session->channel),
							  a_engine->cur_payload_map->remote_sdp_ip,
							  v_engine->local_sdp_port,
							  v_engine->cur_payload_map->remote_sdp_ip,
							  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->agreed_pt,
							  0, switch_rtp_ready(v_engine->rtp_session) ? "SUCCESS" : err);


			if (switch_rtp_ready(v_engine->rtp_session)) {
				const char *ssrc;

				if (v_engine->fir) {
					switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_FIR);
				}

				if (v_engine->pli) {
					switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PLI);
				}
				
				switch_rtp_set_payload_map(v_engine->rtp_session, &v_engine->payload_map);

				start_video_thread(session);
				switch_channel_set_flag(session->channel, CF_VIDEO);

				if ((ssrc = switch_channel_get_variable(session->channel, "rtp_use_video_ssrc"))) {
					uint32_t ssrc_ul = (uint32_t) strtoul(ssrc, NULL, 10);
					switch_rtp_set_ssrc(v_engine->rtp_session, ssrc_ul);
					v_engine->ssrc = ssrc_ul;
				} else {
					switch_rtp_set_ssrc(v_engine->rtp_session, v_engine->ssrc);
				}
				
				if (v_engine->remote_ssrc) {
					switch_rtp_set_remote_ssrc(v_engine->rtp_session, v_engine->remote_ssrc);
				}

				if (v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].ready) {
					
					gen_ice(session, SWITCH_MEDIA_TYPE_VIDEO, NULL, 0);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating Video ICE\n");
						
					switch_rtp_activate_ice(v_engine->rtp_session, 
											v_engine->ice_in.ufrag,
											v_engine->ice_out.ufrag,
											v_engine->ice_out.pwd,
											v_engine->ice_in.pwd,
											IPR_RTP,
#ifdef GOOGLE_ICE
											ICE_GOOGLE_JINGLE,
											NULL
#else
											switch_ice_direction(session) == 
											SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
											&v_engine->ice_in
#endif
											);
						
					
				}

				if ((val = switch_channel_get_variable(session->channel, "rtcp_video_interval_msec")) || (val = smh->mparams->rtcp_video_interval_msec)) {
					const char *rport = switch_channel_get_variable(session->channel, "rtp_remote_video_rtcp_port");
					switch_port_t remote_port = v_engine->remote_rtcp_port;

					if (rport) {
						remote_port = (switch_port_t)atoi(rport);
					}
					if (!strcasecmp(val, "passthru")) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating VIDEO RTCP PASSTHRU PORT %d\n", remote_port);
						switch_rtp_activate_rtcp(v_engine->rtp_session, -1, remote_port, v_engine->rtcp_mux > 0);
					} else {
						int interval = atoi(val);
						if (interval < 100 || interval > 500000) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
											  "Invalid rtcp interval spec [%d] must be between 100 and 500000\n", interval);
						}
						interval = 10000;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating VIDEO RTCP PORT %d mux %d\n", remote_port, v_engine->rtcp_mux);
						switch_rtp_activate_rtcp(v_engine->rtp_session, interval, remote_port, v_engine->rtcp_mux > 0);
							
					}
					

					if (v_engine->ice_in.cands[v_engine->ice_in.chosen[1]][1].ready) {

						if (v_engine->rtcp_mux > 0 && !strcmp(v_engine->ice_in.cands[v_engine->ice_in.chosen[1]][1].con_addr, v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].con_addr)
							&& v_engine->ice_in.cands[v_engine->ice_in.chosen[1]][1].con_port == v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].con_port) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Skipping VIDEO RTCP ICE (Same as VIDEO RTP)\n");
						} else {

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating VIDEO RTCP ICE\n");
							switch_rtp_activate_ice(v_engine->rtp_session, 
													v_engine->ice_in.ufrag,
													v_engine->ice_out.ufrag,
													v_engine->ice_out.pwd,
													v_engine->ice_in.pwd,
													IPR_RTCP,
#ifdef GOOGLE_ICE
													ICE_GOOGLE_JINGLE,
													NULL
#else
													switch_ice_direction(session) == 
													SWITCH_CALL_DIRECTION_OUTBOUND ? ICE_VANILLA : (ICE_VANILLA | ICE_CONTROLLED),
													
													&v_engine->ice_in
#endif
													);
						
						
						
						}
				
					}
				}
				
				if (!zstr(v_engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(smh->session)) {
					dtls_type_t xtype, 
						dtype = switch_channel_direction(smh->session->channel) == SWITCH_CALL_DIRECTION_INBOUND ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;
					xtype = DTLS_TYPE_RTP;
					if (v_engine->rtcp_mux > 0 && smh->mparams->rtcp_video_interval_msec) xtype |= DTLS_TYPE_RTCP;
					
					switch_rtp_add_dtls(v_engine->rtp_session, &v_engine->local_dtls_fingerprint, &v_engine->remote_dtls_fingerprint, dtype | xtype);
					
					if (v_engine->rtcp_mux < 1 && smh->mparams->rtcp_video_interval_msec) {
						xtype = DTLS_TYPE_RTCP;
						switch_rtp_add_dtls(v_engine->rtp_session, &v_engine->local_dtls_fingerprint, &v_engine->remote_dtls_fingerprint, dtype | xtype);
					}
				}
					
					
				if ((val = switch_channel_get_variable(session->channel, "rtp_manual_video_rtp_bugs"))) {
					switch_core_media_parse_rtp_bugs(&v_engine->rtp_bugs, val);
				}
				
				switch_rtp_intentional_bugs(v_engine->rtp_session, v_engine->rtp_bugs | smh->mparams->manual_video_rtp_bugs);

				
				//XX


				switch_channel_set_variable_printf(session->channel, "rtp_use_video_pt", "%d", v_engine->cur_payload_map->agreed_pt);
				v_engine->ssrc = switch_rtp_get_ssrc(v_engine->rtp_session);
				switch_channel_set_variable_printf(session->channel, "rtp_use_video_ssrc", "%u", v_engine->ssrc);

				switch_core_session_apply_crypto(session, SWITCH_MEDIA_TYPE_VIDEO);

				
				if (switch_channel_test_flag(session->channel, CF_ZRTP_PASSTHRU)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activating video UDPTL mode\n");
					switch_rtp_udptl_mode(v_engine->rtp_session);
				}

			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end;
			}
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

 video_up:

	if (session && v_engine) {
		check_dtls_reinvite(session, v_engine);
	}

	status = SWITCH_STATUS_SUCCESS;

 end:

	switch_channel_clear_flag(session->channel, CF_REINVITE);

	switch_core_recovery_track(session);



	return status;

}

static const char *get_media_profile_name(switch_core_session_t *session, int secure)
{
	switch_assert(session);

	if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
		if (switch_channel_test_flag(session->channel, CF_WEBRTC_MOZ)) {
			return "UDP/TLS/RTP/SAVPF";
		} else {
			return "RTP/SAVPF";
		}
	}

	if (secure) {
		return "RTP/SAVP";
	}

	return "RTP/AVP";
	
}

//?
static void generate_m(switch_core_session_t *session, char *buf, size_t buflen, 
					   switch_port_t port, const char *family, const char *ip,
					   int cur_ptime, const char *append_audio, const char *sr, int use_cng, int cng_type, switch_event_t *map, int secure, 
					   switch_sdp_type_t sdp_type)
{
	int i = 0;
	int rate;
	int already_did[128] = { 0 };
	int ptime = 0, noptime = 0;
	const char *local_sdp_audio_zrtp_hash;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	//switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "m=audio %d RTP/%sAVP%s", 
	//port, secure ? "S" : "", switch_channel_test_flag(session->channel, CF_WEBRTC) ? "F" : "");

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "m=audio %d %s", port, 
					get_media_profile_name(session, secure || a_engine->crypto_type != CRYPTO_INVALID));

	for (i = 0; i < smh->mparams->num_codecs; i++) {
		const switch_codec_implementation_t *imp = smh->codecs[i];
		int this_ptime = (imp->microseconds_per_packet / 1000);

		if (!strcasecmp(imp->iananame, "ilbc") || !strcasecmp(imp->iananame, "isac") ) {
			this_ptime = 20;
		}

		if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
			continue;
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

		if (smh->ianacodes[i] < 128) {
			if (already_did[smh->ianacodes[i]]) {
				continue;
			}

			already_did[smh->ianacodes[i]] = 1;
		}

		
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", smh->ianacodes[i]);
	}

	if (smh->mparams->dtmf_type == DTMF_2833 && smh->mparams->te > 95) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", smh->mparams->te);
	}
		
	if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && cng_type && use_cng) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", cng_type);
	}
		
	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "\n");


	memset(already_did, 0, sizeof(already_did));

		
	for (i = 0; i < smh->mparams->num_codecs; i++) {
		const switch_codec_implementation_t *imp = smh->codecs[i];
		char *fmtp = imp->fmtp;
		int this_ptime = imp->microseconds_per_packet / 1000;

		if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
			continue;
		}

		if (!strcasecmp(imp->iananame, "ilbc") || !strcasecmp(imp->iananame, "isac")) {
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
		
		if (smh->ianacodes[i] < 128) {
			if (already_did[smh->ianacodes[i]]) {
				continue;
			}
			
			already_did[smh->ianacodes[i]] = 1;
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

		if (smh->fmtps[i]) {
			fmtp = smh->fmtps[i];
		}

		if (smh->ianacodes[i] > 95 || switch_channel_test_flag(session->channel, CF_VERBOSE_SDP)) {
			int channels = get_channels(imp->iananame, imp->number_of_channels);

			if (channels > 1) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d %s/%d/%d\n", smh->ianacodes[i], imp->iananame, rate, channels);
								
			} else {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d %s/%d\n", smh->ianacodes[i], imp->iananame, rate);
			}
		}

		if (fmtp) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=fmtp:%d %s\n", smh->ianacodes[i], fmtp);
		}
	}


	if ((smh->mparams->dtmf_type == DTMF_2833 || switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || 
		 switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF)) && smh->mparams->te > 95) {

		if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d telephone-event/8000\n", smh->mparams->te);
		} else {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", smh->mparams->te, smh->mparams->te);
		}

	}

	if (!zstr(a_engine->local_dtls_fingerprint.type) && secure) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=fingerprint:%s %s\n", a_engine->local_dtls_fingerprint.type, 
						a_engine->local_dtls_fingerprint.str);
	}

	if (smh->mparams->rtcp_audio_interval_msec) {
		if (a_engine->rtcp_mux > 0) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-mux\n");
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp:%d IN %s %s\n", port, family, ip);
		} else {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp:%d IN %s %s\n", port + 1, family, ip);
		}
	}

	//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\n", a_engine->ssrc);

	if (a_engine->ice_out.cands[0][0].ready) {
		char tmp1[11] = "";
		char tmp2[11] = "";
		uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
		uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
		//uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
		//uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);
		ice_t *ice_out;

		tmp1[10] = '\0';
		tmp2[10] = '\0';
		switch_stun_random_string(tmp1, 10, "0123456789");
		switch_stun_random_string(tmp2, 10, "0123456789");

		gen_ice(session, SWITCH_MEDIA_TYPE_AUDIO, NULL, 0);

		ice_out = &a_engine->ice_out;

		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u cname:%s\n", a_engine->ssrc, smh->cname);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u msid:%s a0\n", a_engine->ssrc, smh->msid);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u mslabel:%s\n", a_engine->ssrc, smh->msid);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u label:%sa0\n", a_engine->ssrc, smh->msid);


		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-ufrag:%s\n", ice_out->ufrag);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-pwd:%s\n", ice_out->pwd);


		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\n", 
						tmp1, ice_out->cands[0][0].transport, c1,
						ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
						);

		if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
			strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
			&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\n", 
							tmp2, ice_out->cands[0][0].transport, c2,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
							a_engine->local_sdp_ip, a_engine->local_sdp_port
							);
		}

		if (a_engine->rtcp_mux < 1 || switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			

			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\n", 
							tmp1, ice_out->cands[0][0].transport, c1,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
							);
			
			if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][1].con_addr) && 
				strcmp(a_engine->local_sdp_ip, ice_out->cands[0][1].con_addr)
				&& a_engine->local_sdp_port != ice_out->cands[0][1].con_port) {
				
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx raddr %s rport %d generation 0\n", 
								tmp2, ice_out->cands[0][0].transport, c2,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1),
								a_engine->local_sdp_ip, a_engine->local_sdp_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
								);
			}
		}

			
				
#ifdef GOOGLE_ICE
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-options:google-ice\n");
#endif
	}


	if (secure && !switch_channel_test_flag(session->channel, CF_DTLS)) {
		int i;

		for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
			switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;

			if ((a_engine->crypto_type == j || a_engine->crypto_type == CRYPTO_INVALID) && !zstr(a_engine->ssec[j].local_crypto_key)) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=crypto:%s\n", a_engine->ssec[j].local_crypto_key);
			}
		}
		//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
	}

	if (!cng_type) {
		//switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d CN/8000\n", cng_type);
		//} else {
		if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) { 
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=silenceSupp:off - - - -\n");
		}
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

	local_sdp_audio_zrtp_hash = switch_core_media_get_zrtp_hash(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_TRUE);

	if (local_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n", local_sdp_audio_zrtp_hash);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=zrtp-hash:%s\n", local_sdp_audio_zrtp_hash);
	}

	if (!zstr(sr)) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=%s\n", sr);
	}
}

//?
SWITCH_DECLARE(void) switch_core_media_check_dtmf_type(switch_core_session_t *session) 
{
	const char *val;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if ((val = switch_channel_get_variable(session->channel, "dtmf_type"))) {
		if (!strcasecmp(val, "rfc2833")) {
			smh->mparams->dtmf_type = DTMF_2833;
		} else if (!strcasecmp(val, "info")) {
			smh->mparams->dtmf_type = DTMF_INFO;
		} else if (!strcasecmp(val, "none")) {
			smh->mparams->dtmf_type = DTMF_NONE;
		}
	}
}

//?
switch_status_t switch_core_media_sdp_map(const char *r_sdp, switch_event_t **fmtp, switch_event_t **pt)
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

//?
SWITCH_DECLARE(void)switch_core_media_set_local_sdp(switch_core_session_t *session, const char *sdp_str, switch_bool_t dup)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (smh->sdp_mutex) switch_mutex_lock(smh->sdp_mutex);
	smh->mparams->local_sdp_str = dup ? switch_core_session_strdup(session, sdp_str) : (char *) sdp_str;
	switch_channel_set_variable(session->channel, "rtp_local_sdp_str", smh->mparams->local_sdp_str);
	if (smh->sdp_mutex) switch_mutex_unlock(smh->sdp_mutex);
}


//?
#define SDPBUFLEN 65536
SWITCH_DECLARE(void) switch_core_media_gen_local_sdp(switch_core_session_t *session, switch_sdp_type_t sdp_type, const char *ip, switch_port_t port, const char *sr, int force)
{
	char *buf;
	int ptime = 0;
	uint32_t rate = 0;
	uint32_t v_port;
	int use_cng = 1;
	const char *val;
	const char *family;
	const char *pass_fmtp = switch_channel_get_variable(session->channel, "rtp_video_fmtp");
	const char *ov_fmtp = switch_channel_get_variable(session->channel, "rtp_force_video_fmtp");
	const char *append_audio = switch_channel_get_variable(session->channel, "rtp_append_audio_sdp");
	const char *append_video = switch_channel_get_variable(session->channel, "rtp_append_video_sdp");
	char srbuf[128] = "";
	const char *var_val;
	const char *username;
	const char *fmtp_out;
	const char *fmtp_out_var = switch_channel_get_variable(session->channel, "rtp_force_audio_fmtp");
	switch_event_t *map = NULL, *ptmap = NULL;
	//const char *b_sdp = NULL;
	//const char *local_audio_crypto_key = switch_core_session_local_crypto_key(session, SWITCH_MEDIA_TYPE_AUDIO);
	const char *local_sdp_audio_zrtp_hash = switch_core_media_get_zrtp_hash(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_TRUE);
	const char *local_sdp_video_zrtp_hash = switch_core_media_get_zrtp_hash(session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_TRUE);
	const char *tmp;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;
	ice_t *ice_out;
	int vp8 = 0;
	int red = 0;
	payload_map_t *pmap;
	int is_outbound = switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (switch_true(switch_channel_get_variable(session->channel, "rtcp_mux"))) {
		a_engine->rtcp_mux = 1;
		v_engine->rtcp_mux = 1;
	}

	if (!smh->mparams->rtcp_audio_interval_msec) {
		smh->mparams->rtcp_audio_interval_msec = (char *)switch_channel_get_variable(session->channel, "rtcp_audio_interval_msec");
	}

	if (!smh->mparams->rtcp_video_interval_msec) {
		smh->mparams->rtcp_video_interval_msec = (char *)switch_channel_get_variable(session->channel, "rtcp_video_interval_msec");
	}

	if (dtls_ok(session) && (tmp = switch_channel_get_variable(smh->session->channel, "webrtc_enable_dtls")) && switch_false(tmp)) {
		switch_channel_clear_flag(smh->session->channel, CF_DTLS_OK);
		switch_channel_clear_flag(smh->session->channel, CF_DTLS);
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_OFF) && (tmp = switch_channel_get_variable(smh->session->channel, "uuid_media_secure_media"))) {
		switch_channel_set_variable(smh->session->channel, "rtp_secure_media", tmp);
		switch_core_session_parse_crypto_prefs(session);
		switch_core_session_check_outgoing_crypto(session);
	}

	if (is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING) ||
		switch_channel_test_flag(session->channel, CF_3PCC)) {
		if (!switch_channel_test_flag(session->channel, CF_WEBRTC) && 
			switch_true(switch_channel_get_variable(session->channel, "media_webrtc"))) {
			switch_channel_set_flag(session->channel, CF_WEBRTC);
			switch_channel_set_flag(session->channel, CF_ICE);
			smh->mparams->rtcp_audio_interval_msec = "5000";
			smh->mparams->rtcp_video_interval_msec = "5000";
		}

		if ( switch_rtp_has_dtls() && dtls_ok(session)) {
			if (switch_channel_test_flag(session->channel, CF_WEBRTC) ||
				switch_true(switch_channel_get_variable(smh->session->channel, "rtp_use_dtls"))) {
				switch_channel_set_flag(smh->session->channel, CF_DTLS);
				switch_channel_set_flag(smh->session->channel, CF_SECURE);
				generate_local_fingerprint(smh, SWITCH_MEDIA_TYPE_AUDIO);
			}
		}
		switch_core_session_parse_crypto_prefs(session);
		switch_core_session_check_outgoing_crypto(session);
	}

	fmtp_out = a_engine->cur_payload_map->fmtp_out;
	username = smh->mparams->sdp_username;


	switch_zmalloc(buf, SDPBUFLEN);
	
	switch_core_media_check_dtmf_type(session);

	if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) ||
		((val = switch_channel_get_variable(session->channel, "supress_cng")) && switch_true(val)) ||
		((val = switch_channel_get_variable(session->channel, "suppress_cng")) && switch_true(val))) {
		use_cng = 0;
		smh->mparams->cng_pt = 0;
	}

	


	if (!smh->payload_space) {
		int i;

		/* it could be 98 but chrome reserves 98 and 99 for some internal stuff even though they should not.  
		   Everyone expects dtmf to be at 101 and Its not worth the trouble so we'll start at 102 */
		smh->payload_space = 102;

		for (i = 0; i < smh->mparams->num_codecs; i++) {
			smh->ianacodes[i] = smh->codecs[i]->ianacode;
		}
		
		if (sdp_type == SDP_TYPE_REQUEST) {
			switch_core_session_t *orig_session = NULL;

			switch_core_session_get_partner(session, &orig_session);			

			for (i = 0; i < smh->mparams->num_codecs; i++) {
				const switch_codec_implementation_t *imp = smh->codecs[i];
				switch_payload_t orig_pt = 0;
				char *orig_fmtp = NULL;

				//smh->ianacodes[i] = imp->ianacode;
				
				if (smh->ianacodes[i] > 64) {
					if (smh->mparams->dtmf_type == DTMF_2833 && smh->mparams->te > 95 && smh->mparams->te == smh->payload_space) {
						smh->payload_space++;
					}
					if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) &&
						smh->mparams->cng_pt && use_cng  && smh->mparams->cng_pt == smh->payload_space) {
						smh->payload_space++;
					}

					if (orig_session && 
						switch_core_session_get_payload_code(orig_session, 
															 imp->codec_type == SWITCH_CODEC_TYPE_AUDIO ? SWITCH_MEDIA_TYPE_AUDIO : SWITCH_MEDIA_TYPE_VIDEO,
															 imp->iananame, imp->samples_per_second, &orig_pt, NULL, &orig_fmtp) == SWITCH_STATUS_SUCCESS) {
						if (orig_pt == smh->mparams->te) {
							smh->mparams->te  = (switch_payload_t)smh->payload_space++;
						}

						smh->ianacodes[i] = orig_pt;
												
						if (orig_fmtp) {
							smh->fmtps[i] = switch_core_session_strdup(session, orig_fmtp);
						}
					} else {
						smh->ianacodes[i] = (switch_payload_t)smh->payload_space++;
					}
				}

				switch_core_media_add_payload_map(session,
												  imp->codec_type == SWITCH_CODEC_TYPE_AUDIO ? SWITCH_MEDIA_TYPE_AUDIO : SWITCH_MEDIA_TYPE_VIDEO,
												  imp->iananame,
												  NULL,
												  sdp_type,
												  smh->ianacodes[i],
												  imp->samples_per_second,
												  imp->microseconds_per_packet / 1000,
												  imp->number_of_channels,
												  SWITCH_FALSE);
			}
				

			if (orig_session) {
				switch_core_session_rwunlock(orig_session);
			}
		}
	}

	if (fmtp_out_var) {
		fmtp_out = fmtp_out_var;
	}

	val = switch_channel_get_variable(session->channel, "verbose_sdp");

	if (!val || switch_true(val)) {
		switch_channel_set_flag(session->channel, CF_VERBOSE_SDP);
	}

	if (!force && !ip && zstr(sr)
		&& (switch_channel_test_flag(session->channel, CF_PROXY_MODE) || switch_channel_test_flag(session->channel, CF_PROXY_MEDIA))) {
		switch_safe_free(buf);
		return;
	}

	if (!ip) {
		if (!(ip = a_engine->adv_sdp_ip)) {
			ip = a_engine->proxy_sdp_ip;
		}
	}

	if (!ip) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO IP!\n", switch_channel_get_name(session->channel));
		switch_safe_free(buf);
		return;
	}

	if (!port) {
		if (!(port = a_engine->adv_sdp_port)) {
			port = a_engine->proxy_sdp_port;
		}
	}

	if (!port) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO PORT!\n", switch_channel_get_name(session->channel));
		switch_safe_free(buf);
		return;
	}

	//if (!a_engine->cur_payload_map->rm_encoding && (b_sdp = switch_channel_get_variable(session->channel, SWITCH_B_SDP_VARIABLE))) {
	//switch_core_media_sdp_map(b_sdp, &map, &ptmap);
	//}

	if (zstr(sr)) {
		if ((var_val = switch_channel_get_variable(session->channel, "media_audio_mode"))) {
			sr = var_val;
		} else {
			sr = "sendrecv";
		}
	}

	if (!smh->owner_id) {
		smh->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!smh->session_id) {
		smh->session_id = smh->owner_id;
	}

	if (switch_true(switch_channel_get_variable_dup(session->channel, "drop_dtmf", SWITCH_FALSE, -1))) {
		switch_channel_set_flag(session->channel, CF_DROP_DTMF);
	}

	smh->session_id++;
	
	if ((smh->mparams->ndlb & SM_NDLB_SENDRECV_IN_SESSION) ||
		((var_val = switch_channel_get_variable(session->channel, "ndlb_sendrecv_in_session")) && switch_true(var_val))) {
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
					"c=IN %s %s\n" 
					"t=0 0\n"
					"%s",
					username, smh->owner_id, smh->session_id, family, ip, username, family, ip, srbuf);


	if (switch_channel_test_flag(smh->session->channel, CF_ICE)) {
		gen_ice(session, SWITCH_MEDIA_TYPE_AUDIO, ip, port);
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=msid-semantic: WMS %s\n", smh->msid);
	}


	if (a_engine->codec_negotiated) {
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=audio %d %s", port, 
						get_media_profile_name(session, !a_engine->no_crypto && 
											   (switch_channel_test_flag(session->channel, CF_DTLS) || a_engine->crypto_type != CRYPTO_INVALID)));
															
		
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", a_engine->cur_payload_map->pt);

		
		if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_AUDIO)) {
			switch_mutex_lock(smh->sdp_mutex);
			for (pmap = a_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
				if (pmap->pt != a_engine->cur_payload_map->pt) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", pmap->pt);
				}
			}
			switch_mutex_unlock(smh->sdp_mutex);
		}

		if ((smh->mparams->dtmf_type == DTMF_2833 || switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || 
			 switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF)) && smh->mparams->te > 95) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", smh->mparams->te);
		}
		
		if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && smh->mparams->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", smh->mparams->cng_pt);
		}
		
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\n");


		rate = a_engine->cur_payload_map->adv_rm_rate;

		if (!a_engine->cur_payload_map->adv_channels) {
			a_engine->cur_payload_map->adv_channels = get_channels(a_engine->cur_payload_map->rm_encoding, 1);
		}
		
		if (a_engine->cur_payload_map->adv_channels > 1) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d/%d\n", 
							a_engine->cur_payload_map->agreed_pt, a_engine->cur_payload_map->rm_encoding, rate, a_engine->cur_payload_map->adv_channels);
		} else {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\n", 
							a_engine->cur_payload_map->agreed_pt, a_engine->cur_payload_map->rm_encoding, rate);
		}

		if (fmtp_out) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", a_engine->cur_payload_map->agreed_pt, fmtp_out);
		}

		if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_AUDIO)) {
			switch_mutex_lock(smh->sdp_mutex);
			for (pmap = a_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
				if (pmap->pt != a_engine->cur_payload_map->pt) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\n",
									pmap->pt, pmap->iananame,
									pmap->rate);
				}
			}
			switch_mutex_unlock(smh->sdp_mutex);
		}


		if (a_engine->read_codec.implementation && !ptime) {
			ptime = a_engine->read_codec.implementation->microseconds_per_packet / 1000;
		}


		if ((smh->mparams->dtmf_type == DTMF_2833 || switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || 
			 switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))
			&& smh->mparams->te > 95) {
			if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d telephone-event/8000\n", smh->mparams->te);
			} else {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d telephone-event/8000\na=fmtp:%d 0-16\n", smh->mparams->te, smh->mparams->te);
			}
		}

		if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=silenceSupp:off - - - -\n");
		} else if (smh->mparams->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d CN/8000\n", smh->mparams->cng_pt);

			if (!a_engine->codec_negotiated) {
				smh->mparams->cng_pt = 0;
			}
		}

		if (append_audio) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s%s", append_audio, end_of(append_audio) == '\n' ? "" : "\n");
		}

		if (ptime) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ptime:%d\n", ptime);
		}


		if (local_sdp_audio_zrtp_hash) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n",
							  local_sdp_audio_zrtp_hash);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n",
							local_sdp_audio_zrtp_hash);
		}

		if (!zstr(sr)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=%s\n", sr);
		}
	

		if (!zstr(a_engine->local_dtls_fingerprint.type)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fingerprint:%s %s\n", a_engine->local_dtls_fingerprint.type, 
							a_engine->local_dtls_fingerprint.str);
		}
		
		if (smh->mparams->rtcp_audio_interval_msec) {
			if (a_engine->rtcp_mux > 0) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp-mux\n");
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\n", port, family, ip);
			} else {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\n", port + 1, family, ip);
			}
		}

		//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\n", a_engine->ssrc);

		if (a_engine->ice_out.cands[0][0].ready) {
			char tmp1[11] = "";
			char tmp2[11] = "";
			uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
			uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
			uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
			uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);

			tmp1[10] = '\0';
			tmp2[10] = '\0';
			switch_stun_random_string(tmp1, 10, "0123456789");
			switch_stun_random_string(tmp2, 10, "0123456789");

			ice_out = &a_engine->ice_out;
			
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u cname:%s\n", a_engine->ssrc, smh->cname);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u msid:%s a0\n", a_engine->ssrc, smh->msid);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u mslabel:%s\n", a_engine->ssrc, smh->msid);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u label:%sa0\n", a_engine->ssrc, smh->msid);
			


			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-ufrag:%s\n", ice_out->ufrag);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-pwd:%s\n", ice_out->pwd);


			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\n", 
							tmp1, ice_out->cands[0][0].transport, c1,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
							);

			if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
				strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
				&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\n", 
								tmp2, ice_out->cands[0][0].transport, c3,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
								a_engine->local_sdp_ip, a_engine->local_sdp_port
								);
			}


			if (a_engine->rtcp_mux < 1 || is_outbound || 
				switch_channel_test_flag(session->channel, CF_RECOVERING)) {

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\n", 
								tmp1, ice_out->cands[0][0].transport, c2,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
								);
				

				
				if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
					strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
					&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {			
					
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx raddr %s rport %d generation 0\n", 
									tmp2, ice_out->cands[0][0].transport, c4,
									ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1),
									a_engine->local_sdp_ip, a_engine->local_sdp_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
									);
				}
			}
			
				
#ifdef GOOGLE_ICE
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-options:google-ice\n");
#endif
		}

		if (a_engine->crypto_type != CRYPTO_INVALID && !switch_channel_test_flag(session->channel, CF_DTLS) &&
			!zstr(a_engine->ssec[a_engine->crypto_type].local_crypto_key) && switch_channel_test_flag(session->channel, CF_SECURE)) {

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\n", a_engine->ssec[a_engine->crypto_type].local_crypto_key);
		//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=encryption:optional\n");
		}

	} else if (smh->mparams->num_codecs) {
		int i;
		int cur_ptime = 0, this_ptime = 0, cng_type = 0;
		const char *mult;

		if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && smh->mparams->cng_pt && use_cng) {
			cng_type = smh->mparams->cng_pt;

			if (!a_engine->codec_negotiated) {
				smh->mparams->cng_pt = 0;
			}
		}
		
		mult = switch_channel_get_variable(session->channel, "sdp_m_per_ptime");

		if (switch_channel_test_flag(session->channel, CF_WEBRTC) || (mult && switch_false(mult))) {
			char *bp = buf;
			int both = (switch_channel_test_flag(session->channel, CF_WEBRTC) || switch_channel_test_flag(session->channel, CF_DTLS)) ? 0 : 1;

			if ((!a_engine->no_crypto && switch_channel_test_flag(session->channel, CF_SECURE)) || 
				switch_channel_test_flag(session->channel, CF_DTLS)) {
				generate_m(session, buf, SDPBUFLEN, port, family, ip, 0, append_audio, sr, use_cng, cng_type, map, 1, sdp_type);
				bp = (buf + strlen(buf));

				if (smh->crypto_mode == CRYPTO_MODE_MANDATORY) {
					both = 0;
				}

			}

			if (both) {
				generate_m(session, bp, SDPBUFLEN - strlen(buf), port, family, ip, 0, append_audio, sr, use_cng, cng_type, map, 0, sdp_type);
			}

		} else {

			for (i = 0; i < smh->mparams->num_codecs; i++) {
				const switch_codec_implementation_t *imp = smh->codecs[i];
				
				if (imp->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
					continue;
				}
				
				this_ptime = imp->microseconds_per_packet / 1000;
				
				if (!strcasecmp(imp->iananame, "ilbc") || !strcasecmp(imp->iananame, "isac")) {
					this_ptime = 20;
				}
				
				if (cur_ptime != this_ptime) {
					char *bp = buf;
					int both = 1;

					cur_ptime = this_ptime;			
					
					if ((!a_engine->no_crypto && switch_channel_test_flag(session->channel, CF_SECURE)) || 
						switch_channel_test_flag(session->channel, CF_DTLS)) {
						generate_m(session, bp, SDPBUFLEN - strlen(buf), port, family, ip, cur_ptime, append_audio, sr, use_cng, cng_type, map, 1, sdp_type);
						bp = (buf + strlen(buf));

						if (smh->crypto_mode == CRYPTO_MODE_MANDATORY) {
							both = 0;
						}
					}

					if (switch_channel_test_flag(session->channel, CF_WEBRTC) || switch_channel_test_flag(session->channel, CF_DTLS)) {
						both = 0;
					}

					if (both) {
						generate_m(session, bp, SDPBUFLEN - strlen(buf), port, family, ip, cur_ptime, append_audio, sr, use_cng, cng_type, map, 0, sdp_type);
					}
				}
				
			}
		}

	}
	
	if (switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE)) {
		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if (switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
				v_engine->no_crypto = 1;
			}
		}

		
		if (!v_engine->local_sdp_port) {
			switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 0);
		}

		if (switch_channel_test_flag(session->channel, CF_WEBRTC)) {
			switch_media_handle_set_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO);
		}

		if ((v_port = v_engine->adv_sdp_port)) {
			int loops;

			for (loops = 0; loops < 2; loops++) {

				if (switch_channel_test_flag(smh->session->channel, CF_ICE)) {
					gen_ice(session, SWITCH_MEDIA_TYPE_VIDEO, ip, (switch_port_t)v_port);
				}


				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=video %d %s", 
								v_port, 
								get_media_profile_name(session, 
													   (loops == 0 && switch_channel_test_flag(session->channel, CF_SECURE) 
														&& switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) || 
													   a_engine->crypto_type != CRYPTO_INVALID || switch_channel_test_flag(session->channel, CF_DTLS)));
			
							
			

				/*****************************/
				if (v_engine->codec_negotiated) {
					payload_map_t *pmap;
					switch_core_media_set_video_codec(session, 0);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", v_engine->cur_payload_map->agreed_pt);
				
					if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO)) {
						switch_mutex_lock(smh->sdp_mutex);
						for (pmap = v_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
							if (pmap->pt != v_engine->cur_payload_map->pt) {
								switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", pmap->pt);
							}
						}
						switch_mutex_unlock(smh->sdp_mutex);
					}

				} else if (smh->mparams->num_codecs) {
					int i;
					int already_did[128] = { 0 };
					for (i = 0; i < smh->mparams->num_codecs; i++) {
						const switch_codec_implementation_t *imp = smh->codecs[i];
					

						if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
							continue;
						}

						if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
							switch_channel_test_flag(session->channel, CF_NOVIDEO)) {
							continue;
						}

						if (smh->ianacodes[i] < 128) {
							if (already_did[smh->ianacodes[i]]) {
								continue;
							}
							already_did[smh->ianacodes[i]] = 1;
						}

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", smh->ianacodes[i]);

						if (!ptime) {
							ptime = imp->microseconds_per_packet / 1000;
						}
					}
				}

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\n");

			
				if (v_engine->codec_negotiated) {
					const char *of;
					payload_map_t *pmap;

					if (!strcasecmp(v_engine->cur_payload_map->rm_encoding, "VP8")) {
						vp8 = v_engine->cur_payload_map->pt;
					}

					if (!strcasecmp(v_engine->cur_payload_map->rm_encoding, "red")) {
						red = v_engine->cur_payload_map->pt;
					}

					rate = v_engine->cur_payload_map->rm_rate;
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\n",
									v_engine->cur_payload_map->pt, v_engine->cur_payload_map->rm_encoding,
									v_engine->cur_payload_map->rm_rate);


					if (switch_channel_test_flag(session->channel, CF_RECOVERING)) {
						pass_fmtp = v_engine->cur_payload_map->rm_fmtp;
					} else {

						pass_fmtp = NULL;

						if (switch_channel_get_partner_uuid(session->channel)) {
							if ((of = switch_channel_get_variable_partner(session->channel, "rtp_video_fmtp"))) {
								pass_fmtp = of;
							}
						}

						if (ov_fmtp) {
							pass_fmtp = ov_fmtp;
						} else { //if (switch_true(switch_channel_get_variable_dup(session->channel, "rtp_mirror_fmtp", SWITCH_FALSE, -1))) { 
							// seems to break eyebeam at least...
							pass_fmtp = switch_channel_get_variable(session->channel, "rtp_video_fmtp");
						}
					}
				
					if (pass_fmtp) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", v_engine->cur_payload_map->pt, pass_fmtp);
					}


					if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO)) {
						switch_mutex_lock(smh->sdp_mutex);
						for (pmap = v_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
							if (pmap->pt != v_engine->cur_payload_map->pt && pmap->negotiated) {
								switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\n",
												pmap->pt, pmap->iananame, pmap->rate);
							
							}
						}
						switch_mutex_unlock(smh->sdp_mutex);
					}


					if (append_video) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s%s", append_video, end_of(append_video) == '\n' ? "" : "\n");
					}

				} else if (smh->mparams->num_codecs) {
					int i;
					int already_did[128] = { 0 };

					for (i = 0; i < smh->mparams->num_codecs; i++) {
						const switch_codec_implementation_t *imp = smh->codecs[i];
						char *fmtp = NULL;
						uint32_t ianacode = smh->ianacodes[i];
						int channels;

						if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
							continue;
						}

						if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
							switch_channel_test_flag(session->channel, CF_NOVIDEO)) {
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
					
						channels = get_channels(imp->iananame, imp->number_of_channels);

						if (!strcasecmp(imp->iananame, "VP8")) {
							vp8 = ianacode;
						}

						if (!strcasecmp(imp->iananame, "red")) {
							red = ianacode;
						}

						if (channels > 1) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d/%d\n", ianacode, imp->iananame,
											imp->samples_per_second, channels);
						} else {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\n", ianacode, imp->iananame,
											imp->samples_per_second);
						}
					
						if (!zstr(ov_fmtp)) {
							fmtp = (char *) ov_fmtp;
						} else {
					
							if (map) {
								fmtp = switch_event_get_header(map, imp->iananame);
							}
						
							if (smh->fmtps[i]) {
								fmtp = smh->fmtps[i];
							}
						
							if (zstr(fmtp)) fmtp = imp->fmtp;

							if (zstr(fmtp)) fmtp = (char *) pass_fmtp;
						}
					
						if (!zstr(fmtp) && strcasecmp(fmtp, "_blank_")) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\n", ianacode, fmtp);
						}
					}
				
				}

				if ((is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING))
					&& switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
					generate_local_fingerprint(smh, SWITCH_MEDIA_TYPE_VIDEO);
				}


				if (!zstr(v_engine->local_dtls_fingerprint.type)) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fingerprint:%s %s\n", v_engine->local_dtls_fingerprint.type, 
									v_engine->local_dtls_fingerprint.str);
				}


				if (smh->mparams->rtcp_video_interval_msec) {
					if (v_engine->rtcp_mux > 0) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp-mux\n");
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\n", v_port, family, ip);
					} else {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\n", v_port + 1, family, ip);
					}
				}


				if (v_engine->fir || v_engine->pli) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), 
									"a=rtcp-fb:* %s%s\n", v_engine->fir ? "fir " : "", v_engine->pli ? "pli" : "");
				}

				//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\n", v_engine->ssrc);

				if (v_engine->ice_out.cands[0][0].ready) {
					char tmp1[11] = "";
					char tmp2[11] = "";
					uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
					uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
					uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
					uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);
					const char *vbw;
					int bw = 256;
				
					tmp1[10] = '\0';
					tmp2[10] = '\0';
					switch_stun_random_string(tmp1, 10, "0123456789");
					switch_stun_random_string(tmp2, 10, "0123456789");

					ice_out = &v_engine->ice_out;


					if ((vbw = switch_channel_get_variable(smh->session->channel, "rtp_video_max_bandwidth"))) {
						int v = atoi(vbw);
						bw = v;
					}
				
					if (bw > 0) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "b=AS:%d\n", bw);
					}

					if (vp8 && switch_channel_test_flag(session->channel, CF_WEBRTC)) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), 
										"a=rtcp-fb:%d ccm fir\n", vp8);
					}
				
					if (red) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), 
										"a=rtcp-fb:%d nack\n", vp8);
					}
				
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u cname:%s\n", v_engine->ssrc, smh->cname);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u msid:%s v0\n", v_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u mslabel:%s\n", v_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u label:%sv0\n", v_engine->ssrc, smh->msid);
				

				
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-ufrag:%s\n", ice_out->ufrag);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-pwd:%s\n", ice_out->pwd);


					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\n", 
									tmp1, ice_out->cands[0][0].transport, c1,
									ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
									);

					if (!zstr(v_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
						strcmp(v_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
						&& v_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\n", 
										tmp2, ice_out->cands[0][0].transport, c3,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
										v_engine->local_sdp_ip, v_engine->local_sdp_port
										);
					}


					if (v_engine->rtcp_mux < 1 || is_outbound || 
						switch_channel_test_flag(session->channel, CF_RECOVERING)) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\n", 
										tmp1, ice_out->cands[0][0].transport, c2,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (v_engine->rtcp_mux > 0 ? 0 : 1)
										);
					
					
						if (!zstr(v_engine->local_sdp_ip) && !zstr(ice_out->cands[0][1].con_addr) && 
							strcmp(v_engine->local_sdp_ip, ice_out->cands[0][1].con_addr)
							&& v_engine->local_sdp_port != ice_out->cands[0][1].con_port) {
						
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx generation 0\n", 
											tmp2, ice_out->cands[0][0].transport, c4,
											ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (v_engine->rtcp_mux > 0 ? 0 : 1),
											v_engine->local_sdp_ip, v_engine->local_sdp_port + (v_engine->rtcp_mux > 0 ? 0 : 1)
											);
						}
					}

			
				
#ifdef GOOGLE_ICE
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-options:google-ice\n");
#endif
				}

				

				if (loops == 0 && switch_channel_test_flag(session->channel, CF_SECURE) && !switch_channel_test_flag(session->channel, CF_DTLS)) {
					int i;
				
					for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
						switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;
					
						if ((a_engine->crypto_type == j || a_engine->crypto_type == CRYPTO_INVALID) && !zstr(a_engine->ssec[j].local_crypto_key)) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\n", v_engine->ssec[j].local_crypto_key);
						}
					}
					//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\n");
				}


				if (local_sdp_video_zrtp_hash) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding video a=zrtp-hash:%s\n", local_sdp_video_zrtp_hash);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\n", local_sdp_video_zrtp_hash);
				}


				if (switch_channel_test_flag(session->channel, CF_DTLS) || 
					!switch_channel_test_flag(session->channel, CF_SECURE) || 
					smh->crypto_mode == CRYPTO_MODE_MANDATORY || smh->crypto_mode == CRYPTO_MODE_FORBIDDEN) {
					break;
				}
			}
		}

	}


	if (map) {
		switch_event_destroy(&map);
	}
	
	if (ptmap) {
		switch_event_destroy(&ptmap);
	}

	switch_core_media_set_local_sdp(session, buf, SWITCH_TRUE);

	switch_safe_free(buf);
}



//?
SWITCH_DECLARE(void) switch_core_media_absorb_sdp(switch_core_session_t *session)
{
	const char *sdp_str;
	switch_rtp_engine_t *a_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if ((sdp_str = switch_channel_get_variable(session->channel, SWITCH_B_SDP_VARIABLE))) {
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
						a_engine->proxy_sdp_ip = switch_core_session_strdup(session, connection->c_address);
					}
					a_engine->proxy_sdp_port = (switch_port_t) m->m_port;
					if (a_engine->proxy_sdp_ip && a_engine->proxy_sdp_port) {
						break;
					}
				}
			}
			sdp_parser_free(parser);
		}
		switch_core_media_set_local_sdp(session, sdp_str, SWITCH_TRUE);
	}
}


//?
SWITCH_DECLARE(void) switch_core_media_set_udptl_image_sdp(switch_core_session_t *session, switch_t38_options_t *t38_options, int insist)
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
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];


	switch_assert(t38_options);

	ip = t38_options->local_ip;
	port = t38_options->local_port;
	username = smh->mparams->sdp_username;

	var = switch_channel_get_variable(session->channel, "t38_broken_boolean");
	
	broken_boolean = switch_true(var);


	if (!ip) {
		if (!(ip = a_engine->adv_sdp_ip)) {
			ip = a_engine->proxy_sdp_ip;
		}
	}

	if (!ip) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO IP!\n", switch_channel_get_name(session->channel));
		return;
	}

	if (!port) {
		if (!(port = a_engine->adv_sdp_port)) {
			port = a_engine->proxy_sdp_port;
		}
	}

	if (!port) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s NO PORT!\n", switch_channel_get_name(session->channel));
		return;
	}

	if (!smh->owner_id) {
		smh->owner_id = (uint32_t) switch_epoch_time_now(NULL) - port;
	}

	if (!smh->session_id) {
		smh->session_id = smh->owner_id;
	}

	smh->session_id++;

	family = strchr(ip, ':') ? "IP6" : "IP4";


	switch_snprintf(buf, sizeof(buf),
					"v=0\n"
					"o=%s %010u %010u IN %s %s\n"
					"s=%s\n" "c=IN %s %s\n" "t=0 0\n", username, smh->owner_id, smh->session_id, family, ip, username, family, ip);

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

	switch_core_media_set_local_sdp(session, buf, SWITCH_TRUE);


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s image media sdp:\n%s\n",
					  switch_channel_get_name(session->channel), smh->mparams->local_sdp_str);


}



//?
SWITCH_DECLARE(void) switch_core_media_patch_sdp(switch_core_session_t *session)
{
	switch_size_t len;
	char *p, *q, *pe, *qe;
	int has_video = 0, has_audio = 0, has_ip = 0;
	char port_buf[25] = "";
	char vport_buf[25] = "";
	char *new_sdp;
	int bad = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine;
	payload_map_t *pmap;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (zstr(smh->mparams->local_sdp_str)) {
		return;
	}

	len = strlen(smh->mparams->local_sdp_str) * 2;

	if (!(smh->mparams->ndlb & SM_NDLB_NEVER_PATCH_REINVITE)) {
		if (switch_channel_test_flag(session->channel, CF_ANSWERED) &&
			(switch_stristr("sendonly", smh->mparams->local_sdp_str) || switch_stristr("0.0.0.0", smh->mparams->local_sdp_str))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Skip patch on hold SDP\n");
			return;
		}
	}

	if (zstr(a_engine->local_sdp_ip) || !a_engine->local_sdp_port) {// || switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
		if (switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_AUDIO, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s I/O Error\n",
							  switch_channel_get_name(session->channel));
			return;
		}

		clear_pmaps(a_engine);
		
		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "PROXY",
												 NULL,
												 SDP_TYPE_RESPONSE,
												 0,
												 8000,
												 8000,
												 1,
												 SWITCH_TRUE);

		a_engine->cur_payload_map = pmap;

	}

	new_sdp = switch_core_session_alloc(session, len);
	switch_snprintf(port_buf, sizeof(port_buf), "%u", a_engine->local_sdp_port);


	p = smh->mparams->local_sdp_str;
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

		if (a_engine->local_sdp_ip && !strncmp("c=IN IP", p, 7)) {
			strncpy(q, p, 7);
			p += 7;
			q += 7;
			strncpy(q, strchr(a_engine->adv_sdp_ip, ':') ? "6 " : "4 ", 2);
			p +=2;
			q +=2;			
			strncpy(q, a_engine->adv_sdp_ip, strlen(a_engine->adv_sdp_ip));
			q += strlen(a_engine->adv_sdp_ip);

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


				family = strchr(smh->mparams->sipip, ':') ? "IP6" : "IP4";

				if (!smh->owner_id) {
					smh->owner_id = (uint32_t) switch_epoch_time_now(NULL) * 31821U + 13849U;
				}

				if (!smh->session_id) {
					smh->session_id = smh->owner_id;
				}

				smh->session_id++;


				snprintf(o_line, sizeof(o_line), "o=%s %010u %010u IN %s %s\n",
						 smh->mparams->sdp_username, smh->owner_id, smh->session_id, family, smh->mparams->sipip);

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

				snprintf(s_line, sizeof(s_line), "s=%s\n", smh->mparams->sdp_username);

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
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 1);
				clear_pmaps(v_engine);
				pmap = switch_core_media_add_payload_map(session,
														 SWITCH_MEDIA_TYPE_AUDIO,
														 "PROXY-VID",
														 NULL,
														 SDP_TYPE_RESPONSE,
														 0,
														 90000,
														 90000,
														 1,
														 SWITCH_TRUE);
				v_engine->cur_payload_map = pmap;

				switch_snprintf(vport_buf, sizeof(vport_buf), "%u", v_engine->adv_sdp_port);
				if (switch_channel_media_ready(session->channel) && !switch_rtp_ready(v_engine->rtp_session)) {
					switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
					switch_channel_set_flag(session->channel, CF_REINVITE);
					switch_core_media_activate_rtp(session);
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


	if (switch_channel_down(session->channel)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s too late.\n", switch_channel_get_name(session->channel));
		return;
	}


	if (!has_ip && !has_audio) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SDP has no audio in it.\n%s\n",
						  switch_channel_get_name(session->channel), smh->mparams->local_sdp_str);
		return;
	}


	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Patched SDP\n---\n%s\n+++\n%s\n",
					  switch_channel_get_name(session->channel), smh->mparams->local_sdp_str, new_sdp);

	switch_core_media_set_local_sdp(session, new_sdp, SWITCH_FALSE);

}

//?
SWITCH_DECLARE(void) switch_core_media_start_udptl(switch_core_session_t *session, switch_t38_options_t *t38_options)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_channel_down(session->channel)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];


	if (switch_rtp_ready(a_engine->rtp_session)) {
		char *remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
		switch_port_t remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);
		const char *err, *val;

		switch_channel_clear_flag(session->channel, CF_NOTIMER_DURING_BRIDGE);
		switch_rtp_udptl_mode(a_engine->rtp_session);

		if (!t38_options || !t38_options->remote_ip) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No remote address\n");
			return;
		}

		if (remote_host && remote_port && remote_port == t38_options->remote_port && !strcmp(remote_host, t38_options->remote_ip)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote address:port [%s:%d] has not changed.\n",
							  t38_options->remote_ip, t38_options->remote_port);
			return;
		}

		if (switch_rtp_set_remote_address(a_engine->rtp_session, t38_options->remote_ip,
										  t38_options->remote_port, 0, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "IMAGE UDPTL REPORTS ERROR: [%s]\n", err);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "IMAGE UDPTL CHANGING DEST TO: [%s:%d]\n",
							  t38_options->remote_ip, t38_options->remote_port);
			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_WEBRTC) &&
				!((val = switch_channel_get_variable(session->channel, "disable_udptl_auto_adjust")) && switch_true(val))) {
				/* Reactivate the NAT buster flag. */
				switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}
	}
}

//?
SWITCH_DECLARE(void) switch_core_media_hard_mute(switch_core_session_t *session, switch_bool_t on)
{
	switch_core_session_message_t msg = { 0 };
	
	msg.from = __FILE__;

	msg.message_id = SWITCH_MESSAGE_INDICATE_HARD_MUTE;
	msg.numeric_arg = on;
	switch_core_session_receive_message(session, &msg);
}


//?
SWITCH_DECLARE(switch_status_t) switch_core_media_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	switch (msg->message_id) {

	case SWITCH_MESSAGE_RESAMPLE_EVENT:
		{
			if (switch_channel_test_flag(session->channel, CF_CONFERENCE)) {
				switch_channel_set_flag(session->channel, CF_CONFERENCE_RESET_MEDIA);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ:
		{
			if (v_engine->rtp_session) {
				switch_rtp_video_refresh(v_engine->rtp_session);
			}
		}

		break;

	case SWITCH_MESSAGE_INDICATE_PROXY_MEDIA:
		{
			if (switch_rtp_ready(a_engine->rtp_session)) {
				if (msg->numeric_arg) {
					switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA);
				} else {
					switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA);
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_JITTER_BUFFER:
		{
			if (switch_rtp_ready(a_engine->rtp_session)) {
				check_jb(session, msg->string_arg);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_HARD_MUTE:
		{
			if (session->bugs) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
								  "%s has a media bug, hard mute not allowed.\n", switch_channel_get_name(session->channel));
			} else if (a_engine->rtp_session) {
				if (msg->numeric_arg) {
					switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_MUTE);
				} else {
					switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_MUTE);
				}

				rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_ONCE);				
			}
		}

		break;

	case SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA:
		{
			switch_rtp_t *rtp = a_engine->rtp_session;
			const char *direction = msg->string_array_arg[0];

			if (direction && *direction == 'v') {
				direction++;
				rtp = v_engine->rtp_session;
			}

			if (switch_rtp_ready(rtp) && !zstr(direction) && !zstr(msg->string_array_arg[1])) {
				switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
				int both = !strcasecmp(direction, "both");
				int set = 0;

				if (both || !strcasecmp(direction, "read")) {
					flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ]++;
					set++;
				}

				if (both || !strcasecmp(direction, "write")) {
					flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]++;
					set++;
				}

				if (set) {
					if (switch_true(msg->string_array_arg[1])) {
						switch_rtp_set_flags(rtp, flags);
					} else {
						switch_rtp_clear_flags(rtp, flags);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Options\n");
				}
			}
		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY:
		if (a_engine->rtp_session && switch_rtp_test_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Pass 2833 mode may not work on a transcoded call.\n");
		}
		goto end;

	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		{

			if (switch_rtp_ready(a_engine->rtp_session)) {
				const char *val;
				int ok = 0;
				
				if (!(val = switch_channel_get_variable(session->channel, "rtp_jitter_buffer_during_bridge")) || switch_false(val)) {
					if (switch_channel_test_flag(session->channel, CF_JITTERBUFFER) && switch_channel_test_cap_partner(session->channel, CC_FS_RTP)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "%s PAUSE Jitterbuffer\n", switch_channel_get_name(session->channel));					
						switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_TRUE);
						switch_set_flag(smh, SMF_JB_PAUSED);
					}
				}
				
				if (switch_channel_test_flag(session->channel, CF_PASS_RFC2833) && switch_channel_test_flag_partner(session->channel, CF_FS_RTP)) {
					switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "%s activate passthru 2833 mode.\n", switch_channel_get_name(session->channel));
				}


				if ((val = switch_channel_get_variable(session->channel, "rtp_notimer_during_bridge"))) {
					ok = switch_true(val);
				} else {
					ok = switch_channel_test_flag(session->channel, CF_RTP_NOTIMER_DURING_BRIDGE);
				}

				if (ok && !switch_rtp_test_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
					ok = 0;
				}

				if (ok) {
					switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
					//switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
					switch_channel_set_flag(session->channel, CF_NOTIMER_DURING_BRIDGE);
				}

				if (ok && switch_channel_test_flag(session->channel, CF_NOTIMER_DURING_BRIDGE)) {
					/* these are not compat */
					ok = 0;
				} else {
					if ((val = switch_channel_get_variable(session->channel, "rtp_autoflush_during_bridge"))) {
						ok = switch_true(val);
					} else {
						ok = smh->media_flags[SCMF_RTP_AUTOFLUSH_DURING_BRIDGE];
					}
				}
				
				if (ok) {
					rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_STICK);
					switch_channel_set_flag(session->channel, CF_AUTOFLUSH_DURING_BRIDGE);
				} else {
					rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_ONCE);
				}
				
			}
		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		if (switch_rtp_ready(a_engine->rtp_session)) {
			
			if (switch_test_flag(smh, SMF_JB_PAUSED)) {
				switch_clear_flag(smh, SMF_JB_PAUSED);
				if (switch_channel_test_flag(session->channel, CF_JITTERBUFFER)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "%s RESUME Jitterbuffer\n", switch_channel_get_name(session->channel));					
					switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_FALSE);
				}
			}
			

			if (switch_rtp_test_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s deactivate passthru 2833 mode.\n",
								  switch_channel_get_name(session->channel));
				switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833);
			}
			
			if (switch_channel_test_flag(session->channel, CF_NOTIMER_DURING_BRIDGE)) {
				if (!switch_rtp_test_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_UDPTL) && 
					!switch_rtp_test_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
					switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
					//switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
				}
				switch_channel_clear_flag(session->channel, CF_NOTIMER_DURING_BRIDGE);
			}

			if (switch_channel_test_flag(session->channel, CF_AUTOFLUSH_DURING_BRIDGE)) {
				rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_UNSTICK);
				switch_channel_clear_flag(session->channel, CF_AUTOFLUSH_DURING_BRIDGE);
			} else {
				rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_ONCE);
			}

		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
		if (switch_rtp_ready(a_engine->rtp_session)) {
			rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_ONCE);
		}
		goto end;

	case SWITCH_MESSAGE_INDICATE_MEDIA:
		{

			a_engine->codec_negotiated = 0;
			v_engine->codec_negotiated = 0;
			
			if (session->track_duration) {
				switch_core_session_enable_heartbeat(session, session->track_duration);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_NOMEDIA:
		{
			const char *uuid;
			switch_core_session_t *other_session;
			switch_channel_t *other_channel;
			const char *ip = NULL, *port = NULL;

			switch_channel_set_flag(session->channel, CF_PROXY_MODE);

			switch_core_media_set_local_sdp(session, NULL, SWITCH_FALSE);

			if (switch_true(switch_channel_get_variable(session->channel, "bypass_keep_codec"))) {
				switch_channel_set_variable(session->channel, "absolute_codec_string", switch_channel_get_variable(session->channel, "ep_codec_string"));
			}


			if ((uuid = switch_channel_get_partner_uuid(session->channel))
				&& (other_session = switch_core_session_locate(uuid))) {
				other_channel = switch_core_session_get_channel(other_session);
				ip = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
				port = switch_channel_get_variable(other_channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
				switch_core_session_rwunlock(other_session);

				if (ip && port) {
					switch_core_media_prepare_codecs(session, 1);
					clear_pmaps(a_engine);
					clear_pmaps(v_engine);
					switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, ip, (switch_port_t)atoi(port), NULL, 1);
				}
			}


			if (!smh->mparams->local_sdp_str) {
				switch_core_media_absorb_sdp(session);
			}

			if (session->track_duration) {
				switch_core_session_enable_heartbeat(session, session->track_duration);
			}

		}
		break;


	default:
		break;
	}


	if (smh->mutex) switch_mutex_lock(smh->mutex);


	if (switch_channel_down(session->channel)) {
		status = SWITCH_STATUS_FALSE;
		goto end_lock;
	}

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_MEDIA_RENEG:
		{
			switch_core_session_t *nsession;

			if (msg->string_arg) {
				switch_channel_set_variable(session->channel, "absolute_codec_string", NULL);
				if (*msg->string_arg == '=') {
					switch_channel_set_variable(session->channel, "codec_string", msg->string_arg);
				} else {
					switch_channel_set_variable_printf(session->channel, "codec_string", "=%s%s%s,%s", 
													   v_engine->cur_payload_map->rm_encoding ? v_engine->cur_payload_map->rm_encoding : "",
													   v_engine->cur_payload_map->rm_encoding ? "," : "",
													   a_engine->cur_payload_map->rm_encoding, msg->string_arg);					
				}


				a_engine->codec_negotiated = 0;
				v_engine->codec_negotiated = 0;
				smh->num_negotiated_codecs = 0;
				switch_channel_clear_flag(session->channel, CF_VIDEO_POSSIBLE);
				switch_core_media_prepare_codecs(session, SWITCH_TRUE);
				switch_core_media_check_video_codecs(session);
				switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 1);
			}

			switch_media_handle_set_media_flag(smh, SCMF_RENEG_ON_REINVITE);
			
			if (msg->numeric_arg && switch_core_session_get_partner(session, &nsession) == SWITCH_STATUS_SUCCESS) {
				msg->numeric_arg = 0;
				switch_core_session_receive_message(nsession, msg);
				switch_core_session_rwunlock(nsession);
			}

		}
		break;

	case SWITCH_MESSAGE_INDICATE_AUDIO_DATA:
		{
			if (switch_rtp_ready(a_engine->rtp_session)) {
				if (msg->numeric_arg) {
					if (switch_channel_test_flag(session->channel, CF_JITTERBUFFER)) {
						switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_TRUE);
						switch_set_flag(smh, SMF_JB_PAUSED);
					}

					rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_UNSTICK);
					
				} else {
					if (switch_test_flag(smh, SMF_JB_PAUSED)) {
						switch_clear_flag(smh, SMF_JB_PAUSED);
						if (switch_channel_test_flag(session->channel, CF_JITTERBUFFER)) {
							switch_rtp_pause_jitter_buffer(a_engine->rtp_session, SWITCH_FALSE);
						}
					}
				}
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_UDPTL_MODE:
		{
			switch_t38_options_t *t38_options = switch_channel_get_private(session->channel, "t38_options");

			if (t38_options) {
				switch_core_media_start_udptl(session, t38_options);
			}

		}

		
	default:
		break;
	}


 end_lock:

	if (smh->mutex) switch_mutex_unlock(smh->mutex);

 end:

	if (switch_channel_down(session->channel)) {
		status = SWITCH_STATUS_FALSE;
	}

	return status;

}

//?
SWITCH_DECLARE(void) switch_core_media_break(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_break(smh->engines[type].rtp_session);
	}
}

//?
SWITCH_DECLARE(void) switch_core_media_kill_socket(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_kill_socket(smh->engines[type].rtp_session);
	}
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_queue_rfc2833(switch_core_session_t *session, switch_media_type_t type, const switch_dtmf_t *dtmf)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		return switch_rtp_queue_rfc2833(smh->engines[type].rtp_session, dtmf);
	}

	return SWITCH_STATUS_FALSE;
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_queue_rfc2833_in(switch_core_session_t *session, switch_media_type_t type, const switch_dtmf_t *dtmf)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		return switch_rtp_queue_rfc2833_in(smh->engines[type].rtp_session, dtmf);
	}

	return SWITCH_STATUS_FALSE;
}

//?
SWITCH_DECLARE(uint8_t) switch_core_media_ready(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return 0;
	}	

	return switch_rtp_ready(smh->engines[type].rtp_session);
}

//?
SWITCH_DECLARE(void) switch_core_media_set_rtp_flag(switch_core_session_t *session, switch_media_type_t type, switch_rtp_flag_t flag)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_set_flag(smh->engines[type].rtp_session, flag);
	}	
}

//?
SWITCH_DECLARE(void) switch_core_media_clear_rtp_flag(switch_core_session_t *session, switch_media_type_t type, switch_rtp_flag_t flag)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_clear_flag(smh->engines[type].rtp_session, flag);
	}	
}

//?
SWITCH_DECLARE(void) switch_core_media_set_telephony_event(switch_core_session_t *session, switch_media_type_t type, switch_payload_t te)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}	

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_set_telephony_event(smh->engines[type].rtp_session, te);
	}
}

//?
SWITCH_DECLARE(void) switch_core_media_set_telephony_recv_event(switch_core_session_t *session, switch_media_type_t type, switch_payload_t te)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		switch_rtp_set_telephony_recv_event(smh->engines[type].rtp_session, te);
	}
}

//?
SWITCH_DECLARE(switch_rtp_stats_t *) switch_core_media_get_stats(switch_core_session_t *session, switch_media_type_t type, switch_memory_pool_t *pool)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	if (smh->engines[type].rtp_session) {
		return switch_rtp_get_stats(smh->engines[type].rtp_session, pool);
	}

	return NULL;
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_udptl_mode(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		return switch_rtp_udptl_mode(smh->engines[type].rtp_session);
	}

	return SWITCH_STATUS_FALSE;
}

//?
SWITCH_DECLARE(stfu_instance_t *) switch_core_media_get_jb(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		return switch_rtp_get_jitter_buffer(smh->engines[type].rtp_session);
	}

	return NULL;
}


//?
SWITCH_DECLARE(void) switch_core_media_set_sdp_codec_string(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t sdp_type)
{
	sdp_parser_t *parser;
	sdp_session_t *sdp;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}


	if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {

		if ((sdp = sdp_session(parser))) {
			switch_core_media_set_r_sdp_codec_string(session, switch_core_media_get_codec_string(session), sdp, sdp_type);
		}

		sdp_parser_free(parser);
	}

}


static void add_audio_codec(sdp_rtpmap_t *map, int ptime, char *buf, switch_size_t buflen)
{
	int codec_ms = ptime;
	uint32_t map_bit_rate = 0, map_channels = 1;
	char ptstr[20] = "";
	char ratestr[20] = "";
	char bitstr[20] = "";
	switch_codec_fmtp_t codec_fmtp = { 0 };
						
	if (!codec_ms) {
		codec_ms = switch_default_ptime(map->rm_encoding, map->rm_pt);
	}

	map_channels = map->rm_params ? atoi(map->rm_params) : 1;
	map_bit_rate = switch_known_bitrate((switch_payload_t)map->rm_pt);
				
	if (!ptime && !strcasecmp(map->rm_encoding, "g723")) {
		ptime = codec_ms = 30;
	}
				
	if (zstr(map->rm_fmtp)) {
		if (!strcasecmp(map->rm_encoding, "ilbc")) {
			ptime = codec_ms = 30;
			map_bit_rate = 13330;
		} else if (!strcasecmp(map->rm_encoding, "isac")) {
			ptime = codec_ms = 30;
			map_bit_rate = 32000;
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

	if (map_channels > 1) {
		switch_snprintf(bitstr, sizeof(bitstr), "@%dc", map_channels);
	}

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), ",%s%s%s%s", map->rm_encoding, ratestr, ptstr, bitstr);

}


static void switch_core_media_set_r_sdp_codec_string(switch_core_session_t *session, const char *codec_string, sdp_session_t *sdp, switch_sdp_type_t sdp_type)
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
	int prefer_sdp = 0;
	const char *var;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}


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
		
		if ((m->m_type == sdp_media_audio || m->m_type == sdp_media_video) && m->m_port) {
			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (zstr(attr->a_name)) {
						continue;
					}
					if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
						ptime = atoi(attr->a_value);
						break;
					}
				}
				switch_core_media_add_payload_map(session, 
												  m->m_type == sdp_media_audio ? SWITCH_MEDIA_TYPE_AUDIO : SWITCH_MEDIA_TYPE_VIDEO,
												  map->rm_encoding,
												  map->rm_fmtp,
												  sdp_type,
												  map->rm_pt,
												  map->rm_rate,
												  ptime,
												  map->rm_params ? atoi(map->rm_params) : 1,
												  SWITCH_FALSE);
			}
		}
	}

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

						if ((zstr(map->rm_encoding) || (smh->mparams->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
							match = (map->rm_pt == imp->ianacode) ? 1 : 0;
						} else {
							if (map->rm_encoding) {
								match = !strcasecmp(map->rm_encoding, imp->iananame) &&
									((map->rm_pt < 96 && imp->ianacode < 96) || (map->rm_pt > 95 && imp->ianacode > 95));
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

						if ((zstr(map->rm_encoding) || (smh->mparams->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
							match = (map->rm_pt == imp->ianacode) ? 1 : 0;
						} else {
							if (map->rm_encoding) {
								match = !strcasecmp(map->rm_encoding, imp->iananame) &&
									((map->rm_pt < 96 && imp->ianacode < 96) || (map->rm_pt > 95 && imp->ianacode > 95));
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
				int channels;

				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO || imp->ianacode > 127 || already_did[imp->ianacode]) {
					continue;
				}

				if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND &&
					switch_channel_test_flag(session->channel, CF_NOVIDEO)) {
					continue;
				}

				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if (map->rm_pt > 127 || already_did[map->rm_pt]) {
						continue;
					}

					if ((zstr(map->rm_encoding) || (smh->mparams->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == imp->ianacode) ? 1 : 0;
					} else {
						if (map->rm_encoding) {
							match = !strcasecmp(map->rm_encoding, imp->iananame) &&
								((map->rm_pt < 96 && imp->ianacode < 96) || (map->rm_pt > 95 && imp->ianacode > 95));
						} else {
							match = 0;
						}
					}

					if (match) {
						channels = map->rm_params ? atoi(map->rm_params) : 1;
						if (ptime > 0) {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh@%di@%dc", imp->iananame, (unsigned int) map->rm_rate,
											ptime, channels);
						} else {
							switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s@%uh@%dc", imp->iananame, (unsigned int) map->rm_rate, channels);
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

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_codec_chosen(switch_core_session_t *session, switch_media_type_t type)
{
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}
	
	engine = &smh->engines[type];

	if (engine->cur_payload_map->iananame) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


//?
SWITCH_DECLARE(void) switch_core_media_check_outgoing_proxy(switch_core_session_t *session, switch_core_session_t *o_session)
{
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;
	const char *r_sdp = NULL;
	payload_map_t *pmap;

	switch_assert(session);

	if (!switch_channel_test_flag(o_session->channel, CF_PROXY_MEDIA)) {
		return;
	}

	if (!(smh = session->media_handle)) {
		return;
	}

	r_sdp = switch_channel_get_variable(o_session->channel, SWITCH_R_SDP_VARIABLE);
	
	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	switch_channel_set_flag(session->channel, CF_PROXY_MEDIA);

	clear_pmaps(a_engine);
	clear_pmaps(v_engine);
	
	pmap = switch_core_media_add_payload_map(session,
											 SWITCH_MEDIA_TYPE_AUDIO,
											 "PROXY",
											 NULL,
											 SDP_TYPE_RESPONSE,
											 0,
											 8000,
											 8000,
											 1,
											 SWITCH_TRUE);

	a_engine->cur_payload_map = pmap;

	if (switch_stristr("m=video", r_sdp)) {
		switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 1);
		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "PROXY-VID",
												 NULL,
												 SDP_TYPE_RESPONSE,
												 0,
												 90000,
												 90000,
												 1,
												 SWITCH_TRUE);
		
		v_engine->cur_payload_map = pmap;

		switch_channel_set_flag(session->channel, CF_VIDEO);
		switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
	}
}

#ifdef _MSC_VER
/* remove this if the break is removed from the following for loop which causes unreachable code loop */
/* for (m = sdp->sdp_media; m; m = m->m_next) { */
#pragma warning(push)
#pragma warning(disable:4702)
#endif

//?
SWITCH_DECLARE(void) switch_core_media_proxy_codec(switch_core_session_t *session, const char *r_sdp)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0;
	switch_rtp_engine_t *a_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}
	
	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return;
	}


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
				a_engine->cur_payload_map->iananame = switch_core_session_strdup(session, map->rm_encoding);
				a_engine->cur_payload_map->rm_rate = map->rm_rate;
				a_engine->cur_payload_map->adv_rm_rate = map->rm_rate;
				a_engine->cur_payload_map->codec_ms = ptime;
				switch_core_media_set_codec(session, 0, smh->mparams->codec_flags);
				break;
			}

			break;
		}
	}

	sdp_parser_free(parser);

}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

SWITCH_DECLARE (void) switch_core_media_recover_session(switch_core_session_t *session)
{
	const char *ip;
	const char *port;
	const char *a_ip;
	const char *r_ip;
	const char *r_port;
	const char *tmp;	
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}
	
	ip = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
	port = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE)  || !(ip && port)) {
		return;
	} else {
		a_ip = switch_channel_get_variable(session->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE);
		r_ip = switch_channel_get_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
		r_port = switch_channel_get_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE);
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	a_engine->cur_payload_map->iananame = a_engine->cur_payload_map->rm_encoding = (char *) switch_channel_get_variable(session->channel, "rtp_use_codec_name");
	a_engine->cur_payload_map->rm_fmtp = (char *) switch_channel_get_variable(session->channel, "rtp_use_codec_fmtp");

	if ((tmp = switch_channel_get_variable(session->channel, SWITCH_R_SDP_VARIABLE))) {
		smh->mparams->remote_sdp_str = switch_core_session_strdup(session, tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_timer_name"))) {
		smh->mparams->timer_name = switch_core_session_strdup(session, tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_last_audio_codec_string"))) {
		const char *vtmp = switch_channel_get_variable(session->channel, "rtp_last_video_codec_string");
		switch_channel_set_variable_printf(session->channel, "rtp_use_codec_string", "%s%s%s", tmp, vtmp ? "," : "", vtmp ? vtmp : "");
	}
	
	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_codec_string"))) {
		char *tmp_codec_string = switch_core_session_strdup(smh->session, tmp);
		smh->codec_order_last = switch_separate_string(tmp_codec_string, ',', smh->codec_order, SWITCH_MAX_CODECS);
		smh->mparams->num_codecs = switch_loadable_module_get_codecs_sorted(smh->codecs, SWITCH_MAX_CODECS, smh->codec_order, smh->codec_order_last);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_2833_send_payload"))) {
		smh->mparams->te = (switch_payload_t)atoi(tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_2833_recv_payload"))) {
		smh->mparams->recv_te = (switch_payload_t)atoi(tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_codec_rate"))) {
		a_engine->cur_payload_map->rm_rate = atoi(tmp);
		a_engine->cur_payload_map->adv_rm_rate = a_engine->cur_payload_map->rm_rate;
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_codec_ptime"))) {
		a_engine->cur_payload_map->codec_ms = atoi(tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_codec_channels"))) {
		a_engine->cur_payload_map->channels = atoi(tmp);
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_pt"))) {
		a_engine->cur_payload_map->pt = a_engine->cur_payload_map->agreed_pt = (switch_payload_t)(smh->payload_space = atoi(tmp));
	}

	if ((tmp = switch_channel_get_variable(session->channel, "rtp_audio_recv_pt"))) {
		a_engine->cur_payload_map->recv_pt = (switch_payload_t)atoi(tmp);
	}

	switch_core_media_set_codec(session, 0, smh->mparams->codec_flags);
	
	a_engine->adv_sdp_ip = smh->mparams->extrtpip = (char *) ip;
	a_engine->adv_sdp_port = a_engine->local_sdp_port = (switch_port_t)atoi(port);
	a_engine->codec_negotiated = 1;

	if (!zstr(ip)) {
		a_engine->local_sdp_ip = switch_core_session_strdup(session, ip);
		smh->mparams->rtpip = a_engine->local_sdp_ip;
	}

	if (!zstr(a_ip)) {
		a_engine->adv_sdp_ip = switch_core_session_strdup(session, a_ip);
	}

	if (r_ip && r_port) {
		a_engine->cur_payload_map->remote_sdp_ip = (char *) r_ip;
		a_engine->cur_payload_map->remote_sdp_port = (switch_port_t)atoi(r_port);
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO)) {
		if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_video_pt"))) {
			v_engine->cur_payload_map->pt = v_engine->cur_payload_map->agreed_pt = (switch_payload_t)atoi(tmp);
		}
		
		if ((tmp = switch_channel_get_variable(session->channel, "rtp_video_recv_pt"))) {
			v_engine->cur_payload_map->recv_pt = (switch_payload_t)atoi(tmp);
		}

		v_engine->cur_payload_map->rm_encoding = (char *) switch_channel_get_variable(session->channel, "rtp_use_video_codec_name");
		v_engine->cur_payload_map->rm_fmtp = (char *) switch_channel_get_variable(session->channel, "rtp_use_video_codec_fmtp");

		ip = switch_channel_get_variable(session->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE);
		port = switch_channel_get_variable(session->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE);
		r_ip = switch_channel_get_variable(session->channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE);
		r_port = switch_channel_get_variable(session->channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE);

		switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);

		if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_video_codec_rate"))) {
			v_engine->cur_payload_map->rm_rate = atoi(tmp);
			v_engine->cur_payload_map->adv_rm_rate = v_engine->cur_payload_map->rm_rate;
		}

		if ((tmp = switch_channel_get_variable(session->channel, "rtp_use_video_codec_ptime"))) {
			v_engine->cur_payload_map->codec_ms = atoi(tmp);
		}

		v_engine->adv_sdp_port = v_engine->local_sdp_port = (switch_port_t)atoi(port);
		v_engine->local_sdp_ip = smh->mparams->rtpip;

		if (r_ip && r_port) {
			v_engine->cur_payload_map->remote_sdp_ip = (char *) r_ip;
			v_engine->cur_payload_map->remote_sdp_port = (switch_port_t)atoi(r_port);
		}
	}

	switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 1);
	switch_core_media_set_video_codec(session, 1);

	if (switch_core_media_activate_rtp(session) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	switch_core_session_get_recovery_crypto_key(session, SWITCH_MEDIA_TYPE_AUDIO);
	switch_core_session_get_recovery_crypto_key(session, SWITCH_MEDIA_TYPE_VIDEO);


	if ((tmp = switch_channel_get_variable(session->channel, "rtp_last_audio_local_crypto_key")) && a_engine->ssec[a_engine->crypto_type].remote_crypto_key) {
		int idx = atoi(tmp);

		a_engine->ssec[a_engine->crypto_type].local_crypto_key = switch_core_session_strdup(session, tmp);
		switch_core_media_add_crypto(&a_engine->ssec[a_engine->crypto_type], a_engine->ssec[a_engine->crypto_type].local_crypto_key, SWITCH_RTP_CRYPTO_SEND);
		switch_core_media_add_crypto(&a_engine->ssec[a_engine->crypto_type], a_engine->ssec[a_engine->crypto_type].remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
		switch_channel_set_flag(smh->session->channel, CF_SECURE);
		
		switch_rtp_add_crypto_key(a_engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, idx,
								  a_engine->crypto_type,
								  a_engine->ssec[a_engine->crypto_type].local_raw_key, 
								  SUITES[a_engine->crypto_type].keylen);
		
		switch_rtp_add_crypto_key(a_engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, 
								  a_engine->ssec[a_engine->crypto_type].crypto_tag,
								  a_engine->crypto_type, 
								  a_engine->ssec[a_engine->crypto_type].remote_raw_key, 
								  SUITES[a_engine->crypto_type].keylen);
	}


	if (switch_core_media_ready(session, SWITCH_MEDIA_TYPE_AUDIO)) {
		switch_rtp_set_telephony_event(a_engine->rtp_session, smh->mparams->te);
		switch_rtp_set_telephony_recv_event(a_engine->rtp_session, smh->mparams->recv_te);
	}

}


SWITCH_DECLARE(void) switch_core_media_init(void)
{
	switch_core_gen_certs(DTLS_SRTP_FNAME ".pem");	
}

SWITCH_DECLARE(void) switch_core_media_deinit(void)
{
	
}

static int payload_number(const char *name)
{
	if (!strcasecmp(name, "pcmu")) {
		return 0;
	}

	if (!strcasecmp(name, "pcma")) {
		return 8;
	}

	if (!strcasecmp(name, "gsm")) {
		return 3;
	}

	if (!strcasecmp(name, "g722")) {
		return 9;
	}

	if (!strcasecmp(name, "g729")) {
		return 18;
	}

	if (!strcasecmp(name, "dvi4")) {
		return 5;
	}

	if (!strcasecmp(name, "h261")) {
		return 31;
	}

	if (!strcasecmp(name, "h263")) {
		return 34;
	}

	return -1;
}

static int find_pt(const char *sdp, const char *name)
{
	const char *p;
	
	if ((p = switch_stristr(name, sdp))) {
		if (p < end_of_p(sdp) && *(p+strlen(name)) == '/' && *(p-1) == ' ') {
			p -= 2;

			while(*p > 47 && *p < 58) {
				p--;
			}
			p++;

			if (p) {
				return atoi(p);
			}
		}
	}

	return -1;
}


SWITCH_DECLARE(char *) switch_core_media_filter_sdp(const char *sdp_str, const char *cmd, const char *arg)
{
	char *new_sdp = NULL;
	int pt = -1, te = -1;
	switch_size_t len;
	const char *i;
	char *o;
	int in_m = 0, m_tally = 0, slash = 0;
	int number = 0, skip = 0;
	int remove = !strcasecmp(cmd, "remove");
	int only = !strcasecmp(cmd, "only");
	char *end = end_of_p((char *)sdp_str);
	int tst;
	end++;

	
	if (remove || only) {
		pt = payload_number(arg);
		
		if (pt < 0) {
			pt = find_pt(sdp_str, arg);
		}
	} else {
		return NULL;
	}

	if (only) {
		te = find_pt(sdp_str, "telephone-event");
	}


	len = strlen(sdp_str) + 2;
	new_sdp = malloc(len);
	o = new_sdp;
	i = sdp_str;


	while(i && *i && i < end) {

		if (*i == 'm' && *(i+1) == '=') {
			in_m = 1;
			m_tally++;
		}

		if (in_m) {
			if (*i == '\r' || *i == '\n') {
				in_m = 0;
				slash = 0;
			} else {
				if (*i == '/') {
					slash++;
					while(*i != ' ' && i < end) {
						*o++ = *i++;
					}
					
					*o++ = *i++;
				}
					
				if (slash && switch_is_leading_number(i)) {

					
					number = atoi(i);
						
					while(i < end && ((*i > 47 && *i < 58) || *i == ' ')) {

						if (remove)  {
							tst = (number != pt);
						} else {
							tst = (number == pt || number == te);
						}

						if (tst) {
							*o++ = *i;
						}
						i++;
							
						if (*i == ' ') {
							break;
						}
							
					}

					if (remove)  {
						tst = (number == pt);
					} else {
						tst = (number != pt && number != te);
					}

					if (tst) {
						skip++;
					}
				}
			}
		}

		while (i < end && !strncasecmp(i, "a=rtpmap:", 9)) {
			const char *t = i + 9;
				
			number = atoi(t);

			if (remove)  {
				tst = (number == pt);
			} else {
				tst = (number != pt && number != te);
			}

			while(i < end && (*i != '\r' && *i != '\n')) {
				if (!tst) *o++ = *i;
				i++;
			}

			while(i < end && (*i == '\r' || *i == '\n')) {
				if (!tst) *o++ = *i;
				i++;
			}
		}

		while (i < end && !strncasecmp(i, "a=fmtp:", 7)) {
			const char *t = i + 7;
				
			number = atoi(t);

			if (remove)  {
				tst = (number == pt);
			} else {
				tst = (number != pt && number != te);
			}

			while(i < end && (*i != '\r' && *i != '\n')) {
				if (!tst) *o++ = *i;
				i++;
			}

			while(i < end && (*i == '\r' || *i == '\n')) {
				if (!tst) *o++ = *i;
				i++;
			}
		}

		if (!skip) {
			*o++ = *i;
		}

		skip = 0;

		i++;
	}

	*o = '\0';
	
	return new_sdp;
}

SWITCH_DECLARE(char *) switch_core_media_process_sdp_filter(const char *sdp, const char *cmd_buf, switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *cmd = switch_core_session_strdup(session, cmd_buf);
	int argc = 0;
	char *argv[50];
	int x = 0;
	char *patched_sdp = NULL;

	argc = switch_split(cmd, '|', argv);

	for (x = 0; x < argc; x++) {
		char *command = argv[x];
		char *arg = strchr(command, '(');

		if (arg) {
			char *e = switch_find_end_paren(arg, '(', ')');
			*arg++ = '\0';
			if (e) *e = '\0';
		}

		if (zstr(command) || zstr(arg)) {
			switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_WARNING, "%s SDP FILTER PARSE ERROR\n", switch_channel_get_name(channel));
		} else {
			char *tmp_sdp = NULL;

			if (patched_sdp) {
				tmp_sdp = switch_core_media_filter_sdp(patched_sdp, command, arg);
			} else {
				tmp_sdp = switch_core_media_filter_sdp(sdp, command, arg);
			}


			switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG, 
							  "%s Filter command %s(%s)\nFROM:\n==========\n%s\nTO:\n==========\n%s\n\n", 
							  switch_channel_get_name(channel),
							  command, arg, patched_sdp ? patched_sdp : sdp, tmp_sdp);
			

			if (tmp_sdp) {
				switch_safe_free(patched_sdp);
				patched_sdp = tmp_sdp;
			}
		}
	}

	return patched_sdp;

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

