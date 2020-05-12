/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
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
static void gen_ice(switch_core_session_t *session, switch_media_type_t type, const char *ip, switch_port_t port);
//#define GOOGLE_ICE
#define RTCP_MUX
#define MAX_CODEC_CHECK_FRAMES 50//x:mod_sofia.h
#define MAX_MISMATCH_FRAMES 5//x:mod_sofia.h
#define type2str(type) type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : (type == SWITCH_MEDIA_TYPE_AUDIO ? "audio" : "text")
#define VIDEO_REFRESH_FREQ 1000000

#define TEXT_TIMER_MS 100
#define TEXT_TIMER_SAMPLES 10
#define TEXT_PERIOD_TIMEOUT 3000
#define MAX_RED_FRAMES 25
#define RED_PACKET_SIZE 100

typedef enum {
	SMF_INIT = (1 << 0),
	SMF_READY = (1 << 1),
	SMF_JB_PAUSED = (1 << 2),
	SMF_VB_PAUSED = (1 << 3)
} smh_flag_t;

typedef struct core_video_globals_s {
	int cpu_count;
	int cur_cpu;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	uint32_t fps;
	uint32_t synced;
} core_video_globals_t;

static core_video_globals_t video_globals = { 0 };

struct media_helper {
	switch_core_session_t *session;
	switch_thread_cond_t *cond;
	switch_mutex_t *cond_mutex;
	switch_mutex_t *file_read_mutex;
	switch_mutex_t *file_write_mutex;
	int up;
	int ready;
};

typedef enum {
	CRYPTO_MODE_OPTIONAL,
	CRYPTO_MODE_MANDATORY,
	CRYPTO_MODE_FORBIDDEN
} switch_rtp_crypto_mode_t;

struct switch_rtp_text_factory_s {
	switch_memory_pool_t *pool;
	switch_frame_t text_frame;
	int red_level;
	switch_byte_t *text_write_frame_data;
	switch_frame_t text_write_frame;
	switch_buffer_t *write_buffer;
	int write_empty;
	switch_byte_t *red_buf[MAX_RED_FRAMES];
	int red_bufsize;
	int red_buflen[MAX_RED_FRAMES];
	uint32_t red_ts[MAX_RED_FRAMES];
	int red_pos;
	int red_max;
	switch_timer_t timer;
};


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
	uint32_t media_timeout;
	uint32_t media_hold_timeout;
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


	uint8_t reset_codec;
	uint8_t codec_negotiated;

	uint8_t fir;
	uint8_t pli;
	uint8_t nack;
	uint8_t tmmbr;
	uint8_t no_crypto;
	uint8_t dtls_controller;
	uint8_t pass_codecs;
	switch_codec_settings_t codec_settings;
	switch_media_flow_t rmode;
	switch_media_flow_t smode;
	switch_thread_id_t thread_id;
	switch_thread_id_t thread_write_lock;
	uint8_t new_ice;
	uint8_t new_dtls;
	uint32_t sdp_bw;
	uint32_t orig_bitrate;
	float bw_mult;
	uint8_t reject_avp;
	int t140_pt;
	int red_pt;
	switch_rtp_text_factory_t *tf;

	switch_engine_function_t engine_function;
	void *engine_user_data;
	int8_t engine_function_running;
	switch_frame_buffer_t *write_fb;
} switch_rtp_engine_t;

#define MAX_REJ_STREAMS 10

struct switch_media_handle_s {
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_core_media_flag_t media_flags[SCMF_MAX];
	smh_flag_t flags;
	switch_rtp_engine_t engines[SWITCH_MEDIA_TYPE_TOTAL];
	switch_msrp_session_t *msrp_session;
	switch_mutex_t *read_mutex[SWITCH_MEDIA_TYPE_TOTAL];
	switch_mutex_t *write_mutex[SWITCH_MEDIA_TYPE_TOTAL];
	char *codec_order[SWITCH_MAX_CODECS];
	int codec_order_last;
	const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
	char fmtp[SWITCH_MAX_CODECS][MAX_FMTP_LEN];
	int payload_space;
	char *origin;

	sdp_media_e rejected_streams[MAX_REJ_STREAMS];
	int rej_idx;

	switch_mutex_t *mutex;
	switch_mutex_t *sdp_mutex;
	switch_mutex_t *control_mutex;

	const switch_codec_implementation_t *negotiated_codecs[SWITCH_MAX_CODECS];
	int num_negotiated_codecs;
	switch_payload_t ianacodes[SWITCH_MAX_CODECS];
	switch_payload_t dtmf_ianacodes[SWITCH_MAX_CODECS];
	switch_payload_t cng_ianacodes[SWITCH_MAX_CODECS];
	char *fmtps[SWITCH_MAX_CODECS];
	int video_count;

	int rates[SWITCH_MAX_CODECS];
	uint32_t num_rates;

	uint32_t owner_id;
	uint32_t session_id;

	switch_core_media_params_t *mparams;

	char *msid;
	char *cname;

	switch_rtp_crypto_mode_t crypto_mode;
	switch_rtp_crypto_key_type_t crypto_suite_order[CRYPTO_INVALID+1];
	switch_time_t video_last_key_time;
	switch_time_t video_init;
	switch_time_t last_codec_refresh;
	switch_time_t last_video_refresh_req;
	switch_timer_t video_timer;


	switch_vid_params_t vid_params;
	switch_file_handle_t *video_read_fh;
	switch_file_handle_t *video_write_fh;

	uint64_t vid_frames;
	time_t vid_started;
	int ready_loops;

	switch_thread_t *video_write_thread;
	int video_write_thread_running;

	switch_time_t last_text_frame;

};

switch_srtp_crypto_suite_t SUITES[CRYPTO_INVALID] = {
	{ "AEAD_AES_256_GCM_8", "", AEAD_AES_256_GCM_8, 44, 12},
	{ "AEAD_AES_128_GCM_8", "", AEAD_AES_128_GCM_8, 28, 12},
	{ "AES_256_CM_HMAC_SHA1_80", "AES_CM_256_HMAC_SHA1_80", AES_CM_256_HMAC_SHA1_80, 46, 14},
	{ "AES_192_CM_HMAC_SHA1_80", "AES_CM_192_HMAC_SHA1_80", AES_CM_192_HMAC_SHA1_80, 38, 14},
	{ "AES_CM_128_HMAC_SHA1_80", "", AES_CM_128_HMAC_SHA1_80, 30, 14},
	{ "AES_256_CM_HMAC_SHA1_32", "AES_CM_256_HMAC_SHA1_32", AES_CM_256_HMAC_SHA1_32, 46, 14},
	{ "AES_192_CM_HMAC_SHA1_32", "AES_CM_192_HMAC_SHA1_32", AES_CM_192_HMAC_SHA1_32, 38, 14},
	{ "AES_CM_128_HMAC_SHA1_32", "", AES_CM_128_HMAC_SHA1_32, 30, 14},
	{ "AES_CM_128_NULL_AUTH", "", AES_CM_128_NULL_AUTH, 30, 14}
};

SWITCH_DECLARE(switch_rtp_crypto_key_type_t) switch_core_media_crypto_str2type(const char *str)
{
	int i;

	for (i = 0; i < CRYPTO_INVALID; i++) {
		if (!strncasecmp(str, SUITES[i].name, strlen(SUITES[i].name)) || (SUITES[i].alias && strlen(SUITES[i].alias) && !strncasecmp(str, SUITES[i].alias, strlen(SUITES[i].alias)))) {
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


SWITCH_DECLARE(int) switch_core_media_crypto_keysalt_len(switch_rtp_crypto_key_type_t type)
{
	switch_assert(type < CRYPTO_INVALID);
	return SUITES[type].keysalt_len;
}

SWITCH_DECLARE(int) switch_core_media_crypto_salt_len(switch_rtp_crypto_key_type_t type)
{
	switch_assert(type < CRYPTO_INVALID);
	return SUITES[type].salt_len;
}

static const char* CRYPTO_KEY_PARAM_METHOD[CRYPTO_KEY_PARAM_METHOD_INVALID] = {
	[CRYPTO_KEY_PARAM_METHOD_INLINE] = "inline",
};

static inline switch_media_flow_t sdp_media_flow(unsigned in)
{
	switch(in) {
	case sdp_sendonly:
		return SWITCH_MEDIA_FLOW_SENDONLY;
	case sdp_recvonly:
		return SWITCH_MEDIA_FLOW_RECVONLY;
	case sdp_sendrecv:
		return SWITCH_MEDIA_FLOW_SENDRECV;
	case sdp_inactive:
		return SWITCH_MEDIA_FLOW_INACTIVE;
	}

	return SWITCH_MEDIA_FLOW_SENDRECV;
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

SWITCH_DECLARE(uint32_t) switch_core_media_get_video_fps(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	time_t now;
	uint32_t fps, elapsed = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return 0;
	}

	now = switch_epoch_time_now(NULL);

	elapsed = now - smh->vid_started;

	if (!(smh->vid_started && smh->vid_frames && elapsed > 0)) {
		return 0;
	}

	fps = switch_round_to_step(smh->vid_frames / (elapsed), 5);

	if (smh->vid_frames > 1000) {
		smh->vid_started = switch_epoch_time_now(NULL);
		smh->vid_frames = 1;
	}

	if (fps > 0) {
		video_globals.fps = fps;

		if (smh->vid_params.fps != fps) {
			switch_channel_set_variable_printf(session->channel, "video_fps", "%d", fps);
			smh->vid_params.fps = fps;
		}
	}

	return fps;
}

SWITCH_DECLARE(void) switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session)
{
	_switch_core_media_pass_zrtp_hash2(aleg_session, bleg_session, SWITCH_MEDIA_TYPE_AUDIO);
	_switch_core_media_pass_zrtp_hash2(aleg_session, bleg_session, SWITCH_MEDIA_TYPE_VIDEO);
	_switch_core_media_pass_zrtp_hash2(aleg_session, bleg_session, SWITCH_MEDIA_TYPE_TEXT);
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
	switch_rtp_engine_t *text_engine;
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int got_audio = 0, got_video = 0, got_text = 0;

	if (!session->media_handle) return;

	audio_engine = &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO];
	video_engine = &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO];
	text_engine = &session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT];


	switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG1, "Looking for zrtp-hash\n");
	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (got_audio && got_video && got_text) break;
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
				} else if (m->m_type == sdp_media_text) {
					switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG,
									  "Found text zrtp-hash; setting r_sdp_video_zrtp_hash=%s\n", attr->a_value);
					switch_channel_set_variable(channel, "r_sdp_text_zrtp_hash", attr->a_value);
					text_engine->remote_sdp_zrtp_hash = switch_core_session_strdup(session, attr->a_value);
					got_text++;
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

	switch_assert(sdp);

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
	} else if (sdp->sdp_connection && sdp->sdp_connection->c_address) {
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
	switch_rtp_engine_t *t_engine;
	switch_media_handle_t *smh;
	const char *val;
	int x = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) &&
		!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
		!switch_channel_test_flag(session->channel, CF_AVPF)) {
		/* Reactivate the NAT buster flag. */

		if (a_engine->rtp_session) {
			switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			x++;
		}

		if (v_engine->rtp_session) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			x++;
		}

		if (t_engine->rtp_session) {
			switch_rtp_set_flag(t_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			x++;
		}
	}

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_get_vid_params(switch_core_session_t *session, switch_vid_params_t *vid_params)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(smh->control_mutex);
	*vid_params = smh->vid_params;
	switch_mutex_unlock(smh->control_mutex);

	return SWITCH_STATUS_SUCCESS;
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
		switch_channel_execute_on(session->channel, "execute_on_audio_change");
	}

	switch_core_media_copy_t38_options(t38_options, other_session);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_payload_code(switch_core_session_t *session,
																	 switch_media_type_t type,
																	 const char *iananame,
																	 uint32_t rate,
																	 const char *fmtp_in,
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
		char *fmtp_a = pmap->rm_fmtp;

		if (!pmap->allocated) continue;

		if (!fmtp_a) fmtp_a = "";
		if (!fmtp_in) fmtp_in = "";


		if (!strcasecmp(pmap->iananame, iananame) && !strcasecmp(fmtp_a, fmtp_in) && (!rate || (rate == pmap->rate))) {
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
																  const char *modname,
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

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	engine = &smh->engines[type];

	switch_mutex_lock(smh->sdp_mutex);


	for (pmap = engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {

		if (sdp_type == SDP_TYPE_RESPONSE) {
			switch(type) {
			case SWITCH_MEDIA_TYPE_TEXT:
				exists = (type == pmap->type && !strcasecmp(name, pmap->iananame));
				break;
			case SWITCH_MEDIA_TYPE_AUDIO:
				exists = (type == pmap->type && !strcasecmp(name, pmap->iananame) && pmap->pt == pt && (!pmap->rate || rate == pmap->rate) && (!pmap->ptime || pmap->ptime == ptime));
				break;
			case SWITCH_MEDIA_TYPE_VIDEO:
				if (sdp_type == SDP_TYPE_RESPONSE) {
					exists = (pmap->sdp_type == SDP_TYPE_REQUEST && type == pmap->type && !strcasecmp(name, pmap->iananame));
				} else {
					exists = (type == pmap->type && !strcasecmp(name, pmap->iananame));
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "CHECK PMAP %s:%s %d %s:%s %d ... %d\n", 
								  name, sdp_type == SDP_TYPE_REQUEST ? "REQ" : "RES", pt, 
								  pmap->iananame, pmap->sdp_type == SDP_TYPE_REQUEST ? "REQ" : "RES", pmap->pt, exists);
								  

				break;
			}

			if (exists) {

				if (!zstr(fmtp) && !zstr(pmap->rm_fmtp)) {
					if (strcmp(pmap->rm_fmtp, fmtp)) {
						exists = 0;
						continue;
					}
				}

				break;
			}

		} else {
			if (type == SWITCH_MEDIA_TYPE_TEXT) {
				exists = (type == pmap->type && !strcasecmp(name, pmap->iananame) && pmap->pt == pt);
			} else {
				exists = (type == pmap->type && !strcasecmp(name, pmap->iananame) && pmap->pt == pt && (!pmap->rate || rate == pmap->rate) && (!pmap->ptime || pmap->ptime == ptime));
			}

			if (exists) {
				if (type != SWITCH_MEDIA_TYPE_TEXT && !zstr(fmtp) && !zstr(pmap->rm_fmtp)) {
					if (strcmp(pmap->rm_fmtp, fmtp)) {
						exists = 0;
						continue;
					}
				}

				break;
			}
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

	if (ptime) {
		pmap->ptime = ptime;
	}

	if (rate) {
		pmap->rate = rate;
	}

	if (channels) {
		pmap->channels = channels;
	}

	if (modname) {
		pmap->modname = switch_core_strdup(session->pool, modname);
	}

	if (!zstr(fmtp)) {
		if (sdp_type == SDP_TYPE_REQUEST || !exists) {
			pmap->rm_fmtp = switch_core_strdup(session->pool, fmtp);
		}
	}

	pmap->allocated = 1;

	pmap->recv_pt = (switch_payload_t) pt;


	if (sdp_type == SDP_TYPE_REQUEST || !exists) {
		pmap->pt = (switch_payload_t) pt;
	}

	if (negotiated) {
		pmap->negotiated = negotiated;
	}

	if (!exists) {
		pmap->sdp_type = sdp_type;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "ADD PMAP %s %s %d\n", sdp_type == SDP_TYPE_REQUEST ? "REQ" : "RES", name, pt);

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
						   "srtp_remote_text_crypto_key",
						   "srtp_remote_text_crypto_tag",
						   "srtp_remote_text_crypto_type",
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
		memset(&smh->engines[SWITCH_MEDIA_TYPE_TEXT].ssec[i], 0, sizeof(smh->engines[SWITCH_MEDIA_TYPE_TEXT].ssec[i]));
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

/**
 * If @use_alias != 0 then send crypto with alias name instead of name.
 */ 
static switch_status_t switch_core_media_build_crypto(switch_media_handle_t *smh,
													  switch_media_type_t type,
													  int index, switch_rtp_crypto_key_type_t ctype, switch_rtp_crypto_direction_t direction, int force, int use_alias)
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
	if (switch_channel_test_flag(channel, CF_AVPF) && type == SWITCH_MEDIA_TYPE_VIDEO) {
		if (direction == SWITCH_RTP_CRYPTO_SEND) {
			memcpy(engine->ssec[ctype].local_raw_key, smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec.local_raw_key, SUITES[ctype].keysalt_len);
			key = engine->ssec[ctype].local_raw_key;
		} else {
			memcpy(engine->ssec[ctype].remote_raw_key, smh->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec.remote_raw_key, SUITES[ctype].keysalt_len);
			key = engine->ssec[ctype].remote_raw_key;
		}
	} else {
#endif
		if (direction == SWITCH_RTP_CRYPTO_SEND) {
			key = engine->ssec[ctype].local_raw_key;
		} else {
			key = engine->ssec[ctype].remote_raw_key;
		}

		switch_rtp_get_random(key, SUITES[ctype].keysalt_len);
#ifdef SAME_KEY
	}
#endif

	switch_b64_encode(key, SUITES[ctype].keysalt_len, b64_key, sizeof(b64_key));
	if (switch_channel_var_false(channel, "rtp_pad_srtp_keys")) {
		p = strrchr((char *) b64_key, '=');

		while (p && *p && *p == '=') {
			*p-- = '\0';
		}
	}

	if (index == SWITCH_NO_CRYPTO_TAG) index = ctype + 1;

	if (switch_channel_var_true(channel, "rtp_secure_media_mki")) {	
		engine->ssec[ctype].local_crypto_key = switch_core_session_sprintf(smh->session, "%d %s inline:%s|2^31|1:1", index, (use_alias ? SUITES[ctype].alias : SUITES[ctype].name), b64_key);
	} else {
		engine->ssec[ctype].local_crypto_key = switch_core_session_sprintf(smh->session, "%d %s inline:%s", index, (use_alias ? SUITES[ctype].alias : SUITES[ctype].name), b64_key);
	}

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

#define CRYPTO_KEY_MATERIAL_LIFETIME_MKI_ERR	0x0u
#define CRYPTO_KEY_MATERIAL_MKI					0x1u
#define CRYPTO_KEY_MATERIAL_LIFETIME			0x2u

#define RAW_BITS_PER_64_ENCODED_CHAR 6

/* Return uint32_t which contains all the info about the field found:
 * XXXXXXXa | YYYYYYYY | YYYYYYYY | ZZZZZZZZ
 * where:
 * a - CRYPTO_KEY_MATERIAL_LIFETIME if LIFETIME, CRYPTO_KEY_MATERIAL_MKI if MKI
 * YYYYYYYY and ZZZZZZZZ - depend on 'a':
 *				if a is LIFETIME then YYYYYYYY is decimal Base, ZZZZZZZZ is decimal Exponent
 *				if a is MKI, then YYYYYYYY is decimal MKI_ID, ZZZZZZZZ is decimal MKI_SIZE
 */
static uint32_t parse_lifetime_mki(const char **p, const char *end)
{
	const char *field_begin;
	const char *field_end;
	const char *sep, *space;
	uint32_t res = 0;

	uint32_t val = 0;
	int i;

	switch_assert(*p != NULL);
	switch_assert(end != NULL);

	field_begin = strchr(*p, '|');

	if (field_begin && (field_begin + 1 < end)) {
		space = strchr(field_begin, ' ');
		field_end = strchr(field_begin + 2, '|');

		if (!field_end) {
				field_end = end;
		}

		if (space) {
			if ((field_end == NULL) || (space < field_end)) {
				field_end = space;
			}
		}

		if (field_end) {
			/* Closing '|' found. */
			sep = strchr(field_begin, ':');		/* The lifetime field never includes a colon, whereas the third field always does. (RFC 4568) */
			if (sep && (sep + 1 < field_end) && isdigit(*(sep + 1))) {
				res |= (CRYPTO_KEY_MATERIAL_MKI << 24);

				for (i = 1, *p = sep - 1; *p != field_begin; --(*p), i *= 10) {
					val += ((**p) - '0') * i;
				}

				res |= ((val << 8) & 0x00ffff00);			/* MKI_ID */

				val = 0;
				for (i = 1, *p = field_end - 1; *p != sep; --(*p), i *= 10) {
					val += ((**p) - '0') * i;
				}
				res |= (val & 0x000000ff);					/* MKI_SIZE */
			} else if (isdigit(*(field_begin + 1)) && (field_begin + 2) && (*(field_begin + 2) == '^') && (field_begin + 3) && isdigit(*(field_begin + 3))) {
				res |= (CRYPTO_KEY_MATERIAL_LIFETIME << 24);
				val = ((uint32_t) (*(field_begin + 1) - '0')) << 8;
				res |= val;									/* LIFETIME base. */

				val = 0;
				for (i = 1, *p = field_end - 1; *p != (field_begin + 2); --(*p), i *= 10) {
					val += ((**p) - '0') * i;
				}

				res |= (val & 0x000000ff);					/* LIFETIME exponent. */
			}
		}

		*p = field_end;
	}

	return res;
}

static switch_crypto_key_material_t* switch_core_media_crypto_append_key_material(
		switch_core_session_t *session,
		switch_crypto_key_material_t *tail,
		switch_rtp_crypto_key_param_method_type_t method,
		unsigned char raw_key[SWITCH_RTP_MAX_CRYPTO_LEN],
		int raw_key_len,
		const char *key_material,
		int key_material_len,
		uint64_t lifetime,
		unsigned int mki_id,
		unsigned int mki_size)
{
	struct switch_crypto_key_material_s *new_key_material;

	new_key_material = switch_core_session_alloc(session, (sizeof(*new_key_material)));
	if (new_key_material == NULL) {
		return NULL;
	}

	new_key_material->method = method;
	memcpy(new_key_material->raw_key, raw_key, raw_key_len);
	new_key_material->crypto_key = switch_core_session_strdup(session, key_material);
	new_key_material->lifetime = lifetime;
	new_key_material->mki_id = mki_id;
	new_key_material->mki_size = mki_size;

	new_key_material->next = tail;

	return new_key_material;
}

/* 
 * Skip all space and return pointer to the '\0' terminator of the char string candidate to be a key-material
 * or pointer to first ' ' in the candidate string.
 */
static const char* switch_core_media_crypto_find_key_material_candidate_end(const char *p)
{
	const char *end = NULL;

	switch_assert(p != NULL);

	if (p) {
		end = strchr(p, ' ');	/* find the end of the continuous string of characters in the candidate string */
		if (end == NULL)
			end = p + strlen(p);
	}

	return end;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_add_crypto(switch_core_session_t *session, switch_secure_settings_t *ssec, switch_rtp_crypto_direction_t direction)
{
	unsigned char key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t type;

	const char *p, *delimit;
	const char *key_material_begin;
	const char *key_material_end = NULL; /* begin and end of the current key material candidate */
	int method_len;
	int keysalt_len;

	const char		*opts;
	uint32_t	opt_field;		/* LIFETIME or MKI */
	switch_rtp_crypto_key_param_method_type_t	method = CRYPTO_KEY_PARAM_METHOD_INLINE;
	uint64_t		lifetime = 0;
	uint16_t		lifetime_base = 0;
	uint16_t		lifetime_exp = 0;
	uint16_t		mki_id = 0;
	uint16_t		mki_size = 0;
	switch_crypto_key_material_t *key_material = NULL;
	unsigned long	*key_material_n = NULL;

	bool multiple_keys = false;

	const char *key_param;


	if (direction == SWITCH_RTP_CRYPTO_SEND || direction == SWITCH_RTP_CRYPTO_SEND_RTCP) {
		key_param = ssec->local_crypto_key;
		key_material = ssec->local_key_material_next;
		key_material_n = &ssec->local_key_material_n;
	} else {
		key_param = ssec->remote_crypto_key;
		key_material = ssec->remote_key_material_next;
		key_material_n = &ssec->remote_key_material_n;
	}

	if (zstr(key_param)) {
		goto no_crypto_found;
	}

	*key_material_n = 0;

	p = strchr(key_param, ' ');

	if (!(p && *p && *(p + 1))) {
		goto no_crypto_found;
	}

	p++;

	type = switch_core_media_crypto_str2type(p);

	if (type == CRYPTO_INVALID) {
		goto bad_crypto;
	}

	p = strchr(p, ' '); /* skip the crypto suite description */
	if (p == NULL) {
		goto bad_crypto;
	}

	do {
		if (*key_material_n == SWITCH_CRYPTO_MKI_MAX) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Skipping excess of MKIs due to max number of suppoerted MKIs %d exceeded\n", SWITCH_CRYPTO_MKI_MAX);
			break;
		}

		p = switch_strip_spaces((char*) p, 0);
		if (!p) {
			break;
		}

		key_material_begin = p;
		key_material_end = switch_core_media_crypto_find_key_material_candidate_end(p);

		/* Parsing the key material candidate within [begin, end). */

		if ((delimit = strchr(p, ':')) == NULL) {
			goto bad_error_parsing_near;
		}

		method_len = delimit - p;

		if (strncasecmp(p, CRYPTO_KEY_PARAM_METHOD[CRYPTO_KEY_PARAM_METHOD_INLINE], method_len)) {
			goto bad_key_param_method;
		}

		method = CRYPTO_KEY_PARAM_METHOD_INLINE;

		/* Valid key-material found. Save as default key in secure_settings_s. */

		p = delimit + 1;				/* skip ':' */
		if (!(p && *p && *(p + 1))) {
			goto bad_keysalt;
		}

		/* Check if '|' is present in currently considered key-material. */
		if ((opts = strchr(p, '|')) && (opts < key_material_end)) {
			keysalt_len = opts - p;
		} else {
			keysalt_len = key_material_end - p;
		}

		if (keysalt_len > sizeof(key)) {
			goto bad_keysalt_len;
		}

		switch_b64_decode(p, (char *) key, keysalt_len);

		if (!multiple_keys) { /* First key becomes default (used in case no MKI is found). */
			if (direction == SWITCH_RTP_CRYPTO_SEND) {
				memcpy(ssec->local_raw_key, key, SUITES[type].keysalt_len);
			} else {
				memcpy(ssec->remote_raw_key, key, SUITES[type].keysalt_len);
			}
			multiple_keys = true;
		}

		p += keysalt_len;

		if (!(p < key_material_end)) {
			continue;
		}

		if (opts) {	/* if opts != NULL then opts points to first '|' in current key-material cadidate, parse it as LIFETIME or MKI */

			lifetime = 0;
			mki_id = 0;
			mki_size = 0;

			for (int i = 0; i < 2 && (*opts == '|'); ++i) {

				opt_field = parse_lifetime_mki(&opts, key_material_end);

				switch ((opt_field  >> 24) & 0x3) {

					case CRYPTO_KEY_MATERIAL_LIFETIME:

						lifetime_base = ((opt_field & 0x00ffff00) >> 8) & 0xffff;
						lifetime_exp = (opt_field & 0x000000ff) & 0xffff;
						lifetime = pow(lifetime_base, lifetime_exp);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "LIFETIME found in %s, base %u exp %u\n", p, lifetime_base, lifetime_exp);
						break;

					case CRYPTO_KEY_MATERIAL_MKI:

						mki_id = ((opt_field & 0x00ffff00) >> 8) & 0xffff;
						mki_size = (opt_field & 0x000000ff) & 0xffff;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "MKI found in %s, id %u size %u\n", p, mki_id, mki_size);
						break;

					default:
						goto bad_key_lifetime_or_mki;
				}
			}

			if (mki_id == 0 && lifetime == 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Bad MKI found in %s, (parsed as: id %u size %u lifetime base %u exp %u\n", p, mki_id, mki_size, lifetime_base, lifetime_exp);
				return SWITCH_STATUS_FALSE;
			} else if (mki_id == 0 || lifetime == 0) {
				if (mki_id == 0) {
					if (key_material)
						goto bad_key_no_mki_index;

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Skipping MKI due to empty index\n");
				} else {
					if (mki_size == 0)
						goto bad_key_no_mki_size;

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Skipping MKI due to empty lifetime\n");
				}
				continue;
			}
		}

		if (key_material) {
			if (mki_id == 0) {
				goto bad_key_no_mki_index;
			}

			if (mki_size != key_material->mki_size) {
				goto bad_key_mki_size;
			}
		}

		key_material = switch_core_media_crypto_append_key_material(session, key_material, method, (unsigned char*) key,
				SUITES[type].keysalt_len, (char*) key_material_begin, key_material_end - key_material_begin, lifetime, mki_id, mki_size);
		*key_material_n = *key_material_n + 1;
	} while ((p = switch_strip_spaces((char*) key_material_end, 0)) && (*p != '\0'));

	if (direction == SWITCH_RTP_CRYPTO_SEND || direction == SWITCH_RTP_CRYPTO_SEND_RTCP) {
		ssec->local_key_material_next = key_material;
	} else {
		ssec->remote_key_material_next = key_material;
	}

	return SWITCH_STATUS_SUCCESS;


no_crypto_found:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! No crypto to parse\n");
	return SWITCH_STATUS_FALSE;

bad_error_parsing_near:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! Parsing near %s\n", p);
	return SWITCH_STATUS_FALSE;

bad_crypto:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! SRTP: Invalid format of crypto attribute %s\n", key_param);
	return SWITCH_STATUS_FALSE;

bad_keysalt:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! SRTP: Invalid keysalt in the crypto attribute %s\n", key_param);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! Parsing near %s\n", p);
	return SWITCH_STATUS_FALSE;

bad_keysalt_len:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! SRTP: Invalid keysalt length in the crypto attribute %s\n", key_param);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! Parsing near %s\n", p);
	return SWITCH_STATUS_FALSE;

bad_key_lifetime_or_mki:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! SRTP: Invalid key param MKI or LIFETIME in the crypto attribute %s\n", key_param);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! Parsing near %s\n", p);
	return SWITCH_STATUS_FALSE;

bad_key_no_mki_index:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto invalid: multiple keys in a single crypto MUST all have MKI indices, %s\n", key_param);
	return SWITCH_STATUS_FALSE;

bad_key_no_mki_size:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto invalid: MKI index with no MKI size in %s\n", key_param);
	return SWITCH_STATUS_FALSE;

bad_key_mki_size:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto invalid: MKI sizes differ in %s\n", key_param);
	return SWITCH_STATUS_FALSE;

bad_key_param_method:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! SRTP: Invalid key param method type in %s\n", key_param);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! Parsing near %s\n", p);
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
	} else if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		keyvar = "srtp_remote_video_crypto_key";
		tagvar = "srtp_remote_video_crypto_tag";
		ctypevar = "srtp_remote_video_crypto_type";
	} else if (type == SWITCH_MEDIA_TYPE_TEXT) {
		keyvar = "srtp_remote_text_crypto_key";
		tagvar = "srtp_remote_text_crypto_tag";
		ctypevar = "srtp_remote_text_crypto_type";
	} else return;

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
	} else if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		varname = "rtp_secure_video_confirmed";
	} else if (type == SWITCH_MEDIA_TYPE_TEXT) {
		varname = "rtp_secure_text_confirmed";
	} else {
		return;
	}

	if (!session->media_handle) return;

	engine = &session->media_handle->engines[type];

	if (switch_channel_test_flag(session->channel, CF_RECOVERING)) {
		return;
	}

	if (engine->ssec[engine->crypto_type].remote_crypto_key && switch_channel_test_flag(session->channel, CF_SECURE)) {
		
		if (switch_channel_var_true(session->channel, "rtp_secure_media_mki"))
			switch_core_media_add_crypto(session, &engine->ssec[engine->crypto_type], SWITCH_RTP_CRYPTO_SEND);

		switch_core_media_add_crypto(session, &engine->ssec[engine->crypto_type], SWITCH_RTP_CRYPTO_RECV);


		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1, &engine->ssec[engine->crypto_type]);

		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, engine->ssec[engine->crypto_type].crypto_tag, &engine->ssec[engine->crypto_type]);

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

	if (switch_channel_test_flag(session->channel, CF_AVPF)) {
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
	int use_alias = 0;
	switch_rtp_engine_t *engine;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	if (smh->crypto_mode == CRYPTO_MODE_FORBIDDEN) {
		return -1;
	}

	if (switch_channel_test_flag(session->channel, CF_AVPF)) {
		return 0;
	}

	if (!crypto) {
		return 0;
	}
	engine = &session->media_handle->engines[type];

	for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
		switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "looking for crypto suite [%s]alias=[%s] in [%s]\n", SUITES[j].name, SUITES[j].alias, crypto);

		if (switch_stristr(SUITES[j].alias, crypto)) {
			use_alias = 1;
		}
		
		if (use_alias || switch_stristr(SUITES[j].name, crypto)) {
			ctype = SUITES[j].type;
			vval = use_alias ? SUITES[j].alias : SUITES[j].name;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Found suite %s\n", vval);
			switch_channel_set_variable(session->channel, "rtp_secure_media_negotiated", vval);
			break;
		}

		use_alias = 0;
	}

	if (engine->ssec[engine->crypto_type].remote_crypto_key && switch_rtp_ready(engine->rtp_session)) {
		/* Compare all the key. The tag may remain the same even if key changed */
		if (engine->crypto_type != CRYPTO_INVALID && !strcmp(crypto, engine->ssec[engine->crypto_type].remote_crypto_key)) {
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

				switch_core_media_build_crypto(session->media_handle, type, crypto_tag, ctype, SWITCH_RTP_CRYPTO_SEND, 1, use_alias);
				switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), &engine->ssec[engine->crypto_type]);
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
				} else if (engine->type == SWITCH_MEDIA_TYPE_TEXT) {
					switch_channel_set_variable(session->channel, "srtp_remote_text_crypto_key", crypto);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_text_crypto_tag", "%d", crypto_tag);
					switch_channel_set_variable_printf(session->channel, "srtp_remote_text_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
				}

				engine->ssec[engine->crypto_type].crypto_tag = crypto_tag;


				if (switch_rtp_ready(engine->rtp_session) && switch_channel_test_flag(session->channel, CF_SECURE)) {
					switch_core_media_add_crypto(session, &engine->ssec[engine->crypto_type], SWITCH_RTP_CRYPTO_RECV);
					switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, engine->ssec[engine->crypto_type].crypto_tag, &engine->ssec[engine->crypto_type]);
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
		} else if (engine->type == SWITCH_MEDIA_TYPE_TEXT) {
			switch_channel_set_variable(session->channel, "srtp_remote_text_crypto_key", crypto);
			switch_channel_set_variable_printf(session->channel, "srtp_remote_text_crypto_type", "%s", switch_core_media_crypto_type2str(ctype));
		}

		engine->ssec[engine->crypto_type].crypto_tag = crypto_tag;
		got_crypto++;

		switch_channel_set_variable(session->channel, varname, vval);
		switch_channel_set_flag(smh->session->channel, CF_SECURE);

		if (zstr(engine->ssec[engine->crypto_type].local_crypto_key)) {
			switch_core_media_build_crypto(session->media_handle, type, crypto_tag, ctype, SWITCH_RTP_CRYPTO_SEND, 1, use_alias);
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

	if (switch_core_session_media_handle_ready(session) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	if (!(smh = session->media_handle)) {
		return;
	}

	if (!(smh->crypto_mode == CRYPTO_MODE_OPTIONAL || smh->crypto_mode == CRYPTO_MODE_MANDATORY)) {
		return;
	}

	if (switch_channel_test_flag(session->channel, CF_AVPF)) {
		return;
	}

	switch_channel_set_flag(channel, CF_SECURE);

	for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
		switch_core_media_build_crypto(session->media_handle,
									   SWITCH_MEDIA_TYPE_AUDIO, SWITCH_NO_CRYPTO_TAG, smh->crypto_suite_order[i], SWITCH_RTP_CRYPTO_SEND, 0, 0);

		switch_core_media_build_crypto(session->media_handle,
									   SWITCH_MEDIA_TYPE_VIDEO, SWITCH_NO_CRYPTO_TAG, smh->crypto_suite_order[i], SWITCH_RTP_CRYPTO_SEND, 0, 0);

		switch_core_media_build_crypto(session->media_handle,
									   SWITCH_MEDIA_TYPE_TEXT, SWITCH_NO_CRYPTO_TAG, smh->crypto_suite_order[i], SWITCH_RTP_CRYPTO_SEND, 0, 0);
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

SWITCH_DECLARE(void) switch_core_media_sync_stats(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (a_engine->rtp_session) {
		switch_rtp_sync_stats(a_engine->rtp_session);
	}

	if (v_engine->rtp_session) {
		switch_rtp_sync_stats(v_engine->rtp_session);
	}

	if (t_engine->rtp_session) {
		switch_rtp_sync_stats(t_engine->rtp_session);
	}

}

SWITCH_DECLARE(void) switch_core_media_set_stats(switch_core_session_t *session)
{

	if (!session->media_handle) {
		return;
	}

	switch_core_media_sync_stats(session);

	set_stats(session, SWITCH_MEDIA_TYPE_AUDIO, "audio");
	set_stats(session, SWITCH_MEDIA_TYPE_VIDEO, "video");
	set_stats(session, SWITCH_MEDIA_TYPE_TEXT, "text");
}



SWITCH_DECLARE(void) switch_media_handle_destroy(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine;//, *t_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	//t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];


	if (smh->video_timer.timer_interface) {
		switch_core_timer_destroy(&smh->video_timer);
	}

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

	if (a_engine->write_fb) switch_frame_buffer_destroy(&a_engine->write_fb);

	if (smh->msrp_session) switch_msrp_session_destroy(&smh->msrp_session);
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



		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].type = SWITCH_MEDIA_TYPE_TEXT;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].crypto_type = CRYPTO_INVALID;

		for (i = 0; i < CRYPTO_INVALID; i++) {
			session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].ssec[i].crypto_type = i;
		}



		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].type = SWITCH_MEDIA_TYPE_VIDEO;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].crypto_type = CRYPTO_INVALID;


		switch_channel_set_variable(session->channel, "video_media_flow", "disabled");
		switch_channel_set_variable(session->channel, "audio_media_flow", "disabled");
		switch_channel_set_variable(session->channel, "text_media_flow", "disabled");

		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].smode = SWITCH_MEDIA_FLOW_DISABLED;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].smode = SWITCH_MEDIA_FLOW_DISABLED;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].smode = SWITCH_MEDIA_FLOW_DISABLED;

		for (i = 0; i < CRYPTO_INVALID; i++) {
			session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec[i].crypto_type = i;
		}

		session->media_handle->mparams = params;

		//if (!session->media_handle->mparams->video_key_freq) {
		//	session->media_handle->mparams->video_key_freq = 10000000;
		//}

		if (!session->media_handle->mparams->video_key_first) {
			session->media_handle->mparams->video_key_first = 1000000;
		}


		for (i = 0; i <= CRYPTO_INVALID; i++) {
			session->media_handle->crypto_suite_order[i] = CRYPTO_INVALID;
		}

		switch_mutex_init(&session->media_handle->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_mutex_init(&session->media_handle->sdp_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_mutex_init(&session->media_handle->control_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].ssrc =
			(uint32_t) ((intptr_t) &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO] + (uint32_t) time(NULL));

		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssrc =
			(uint32_t) ((intptr_t) &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO] + (uint32_t) time(NULL) / 2);

		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].ssrc =
			(uint32_t) ((intptr_t) &session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT] + (uint32_t) time(NULL) / 2);



		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].payload_map = switch_core_alloc(session->pool, sizeof(payload_map_t));
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].cur_payload_map = session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].payload_map;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].cur_payload_map->current = 1;

		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].payload_map = switch_core_alloc(session->pool, sizeof(payload_map_t));
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].cur_payload_map = session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].payload_map;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].cur_payload_map->current = 1;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].codec_settings.video.try_hardware_encoder = 1;


		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].payload_map = switch_core_alloc(session->pool, sizeof(payload_map_t));
		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].cur_payload_map = session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].payload_map;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_TEXT].cur_payload_map->current = 1;

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

SWITCH_DECLARE(switch_media_flow_t) switch_core_session_media_flow(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_flow_t flow = SWITCH_MEDIA_FLOW_SENDRECV;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		goto end;
	}

	if (!smh->media_flags[SCMF_RUNNING]) {
		goto end;
	}

	engine = &smh->engines[type];
	flow = engine->smode;

 end:

	return flow;
}


SWITCH_DECLARE(switch_media_flow_t) switch_core_session_remote_media_flow(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_flow_t flow = SWITCH_MEDIA_FLOW_SENDRECV;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		goto end;
	}

	if (!smh->media_flags[SCMF_RUNNING]) {
		goto end;
	}
	
	engine = &smh->engines[type];
	flow = engine->rmode;

 end:

	return flow;
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
	const char *ocodec = NULL, *val;
	switch_media_handle_t *smh;
	char *tmp_codec_string;

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

	ocodec = switch_channel_get_variable(session->channel, SWITCH_ORIGINATOR_CODEC_VARIABLE);

	smh->payload_space = 0;

	switch_assert(smh->session != NULL);

	if ((abs = switch_channel_get_variable(session->channel, "absolute_codec_string"))) {
		codec_string = abs;
		goto ready;
	}

	val = switch_channel_get_variable_dup(session->channel, "media_mix_inbound_outbound_codecs", SWITCH_FALSE, -1);

	if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && (!val || !switch_true(val) || smh->media_flags[SCMF_DISABLE_TRANSCODING]) && ocodec) {
		codec_string = ocodec;
		goto ready;
	}

	if (!(codec_string = switch_channel_get_variable(session->channel, "codec_string"))) {
		codec_string = switch_core_media_get_codec_string(smh->session);
	}

	if (codec_string && *codec_string == '=') {
		codec_string++;
		goto ready;
	}

	if (ocodec) {
		codec_string = switch_core_session_sprintf(smh->session, "%s,%s", ocodec, codec_string);
	}

 ready:

	if (!codec_string) {
		codec_string = "PCMU@20i,PCMA@20i,speex@20i";
	}

	tmp_codec_string = switch_core_session_strdup(smh->session, codec_string);
	switch_channel_set_variable(session->channel, "rtp_use_codec_string", codec_string);
	smh->codec_order_last = switch_separate_string(tmp_codec_string, ',', smh->codec_order, SWITCH_MAX_CODECS);
	smh->mparams->num_codecs = switch_loadable_module_get_codecs_sorted(smh->codecs, smh->fmtp, SWITCH_MAX_CODECS, smh->codec_order, smh->codec_order_last);
}

static void check_jb(switch_core_session_t *session, const char *input, int32_t jb_msec, int32_t maxlen, switch_bool_t silent)
{
	const char *val;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine = NULL, *v_engine = NULL, *t_engine = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];


	if (!zstr(input)) {
		const char *s;
		if (a_engine->rtp_session) {
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

		if (v_engine->rtp_session) {
			if (!strncasecmp(input, "vbsize:", 7)) {
				int frames = 0, max_frames = 0;
				s = input + 7;

				frames = atoi(s);

				if ((s = strchr(s, ':')) && *(s+1) != '\0') {
					max_frames = atoi(s+1);
				}

				if (frames > 0) {
					switch_rtp_set_video_buffer_size(v_engine->rtp_session, frames, max_frames);
				} else {
					switch_rtp_deactivate_jitter_buffer(v_engine->rtp_session);
				}
				return;
			} else if (!strncasecmp(input, "vdebug:", 7)) {
				s = input + 7;

				if (s && !strcmp(s, "off")) {
					s = NULL;
				}
				switch_rtp_debug_jitter_buffer(v_engine->rtp_session, s);
				return;
			}
		}

		if (t_engine->rtp_session) {
			if (!strncasecmp(input, "tbsize:", 7)) {
				int frames = 0, max_frames = 0;
				s = input + 7;

				frames = atoi(s);

				if ((s = strchr(s, ':')) && *(s+1) != '\0') {
					max_frames = atoi(s+1);
				}

				if (frames > 0) {
					switch_rtp_set_video_buffer_size(t_engine->rtp_session, frames, max_frames);
				}
				return;
			} else if (!strncasecmp(input, "tdebug:", 7)) {
				s = input + 7;

				if (s && !strcmp(s, "off")) {
					s = NULL;
				}
				switch_rtp_debug_jitter_buffer(t_engine->rtp_session, s);
				return;
			}
		}
	}


	if (jb_msec || (val = switch_channel_get_variable(session->channel, "jitterbuffer_msec")) || (val = smh->mparams->jb_msec)) {
		char *p;

		if (!jb_msec) {
			jb_msec = atoi(val);

			if (strchr(val, 'p') && jb_msec > 0) {
				jb_msec *= -1;
				if (!maxlen) maxlen = jb_msec * 50;
			}

			if ((p = strchr(val, ':'))) {
				p++;
				maxlen = atoi(p);

				if (strchr(p, 'p') && maxlen > 0) {
					maxlen *= -1;
				}
			}
		}

		if (!maxlen) maxlen = jb_msec * 50;
		
		if (jb_msec < 0 && jb_msec > -1000) {
			jb_msec = (a_engine->read_codec.implementation->microseconds_per_packet / 1000) * abs(jb_msec);
		}

		if (maxlen < 0 && maxlen > -1000) {
			maxlen = (a_engine->read_codec.implementation->microseconds_per_packet / 1000) * abs(maxlen);
		}
		
		
		if (jb_msec < 10 || jb_msec > 10000) {
			//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			//"Invalid Jitterbuffer spec [%d] must be between 10 and 10000\n", jb_msec);
			jb_msec = (a_engine->read_codec.implementation->microseconds_per_packet / 1000) * 1;
			maxlen = jb_msec * 100;
		}

		if (jb_msec && maxlen) {
			int qlen, maxqlen = 50;

			qlen = jb_msec / (a_engine->read_impl.microseconds_per_packet / 1000);

			if (maxlen) {
				maxqlen = maxlen / (a_engine->read_impl.microseconds_per_packet / 1000);
			}

			if (maxqlen < qlen) {
				maxqlen = qlen * 5;
			}
			if (switch_rtp_activate_jitter_buffer(a_engine->rtp_session, qlen, maxqlen,
												  a_engine->read_impl.samples_per_packet,
												  a_engine->read_impl.samples_per_second) == SWITCH_STATUS_SUCCESS) {
				if (!silent) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
									  SWITCH_LOG_DEBUG, "Setting Jitterbuffer to %dms (%d frames) (%d max frames)\n",
									  jb_msec, qlen, maxqlen);
				}
				switch_channel_set_flag(session->channel, CF_JITTERBUFFER);
				if (!switch_false(switch_channel_get_variable(session->channel, "rtp_jitter_buffer_plc"))) {
					switch_channel_set_flag(session->channel, CF_JITTERBUFFER_PLC);
				}
			} else if (!silent) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
								  SWITCH_LOG_WARNING, "Error Setting Jitterbuffer to %dms (%d frames)\n", jb_msec, qlen);
			}

		}
	}

}

static void check_jb_sync(switch_core_session_t *session)
{
	int32_t jb_sync_msec = 0;
	uint32_t fps = 0, frames = 0;
	uint32_t min_frames = 0;
	uint32_t max_frames = 0;
	uint32_t cur_frames = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine = NULL;
	int sync_audio = 0;

	const char *var;

	switch_assert(session);

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return;
	}

	if (!(smh = session->media_handle)) {
		return;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if ((var = switch_channel_get_variable_dup(session->channel, "jb_av_sync_msec", SWITCH_FALSE, -1))) {
		int tmp;
		char *p;

		if (!strcasecmp(var, "disabled")) {
			return;
		}

		tmp = atol(var);

		if (tmp && tmp > -50 && tmp < 10000) {
			jb_sync_msec = tmp;
		}

		if ((p = strchr(var, ':'))) {
			p++;
			frames = atoi(p);
		}
	}

	switch_rtp_get_video_buffer_size(v_engine->rtp_session, &min_frames, &max_frames, &cur_frames, NULL);

	fps = video_globals.fps;

	if (fps < 15) return;

	sync_audio = 1;

	if (!frames) {
		if (cur_frames && min_frames && cur_frames >= min_frames) {
			frames = cur_frames;
		} else if (min_frames) {
			frames = min_frames;
		} else {
			frames = 0;
			sync_audio = 0;
		}
	}

	if (!jb_sync_msec && frames) {
		jb_sync_msec = (double)(1000 / fps) * frames;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
					  SWITCH_LOG_DEBUG1, "%s %s \"%s\" Sync A/V JB to %dms %u VFrames, FPS %u a:%s sync_ms:%d\n",
					  switch_core_session_get_uuid(session),
					  switch_channel_get_name(session->channel),
					  switch_channel_get_variable_dup(session->channel, "caller_id_name", SWITCH_FALSE, -1),
					  jb_sync_msec, frames, video_globals.fps, sync_audio ? "yes" : "no", jb_sync_msec);

	if (sync_audio) {
		check_jb(session, NULL, jb_sync_msec, jb_sync_msec * 2, SWITCH_TRUE);
	}

	video_globals.synced++;
}


//?
SWITCH_DECLARE(switch_status_t) switch_core_media_read_lock_unlock(switch_core_session_t *session, switch_media_type_t type, switch_bool_t lock)
{
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

	if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(engine->rtp_session != NULL);


	if (!switch_channel_up_nosig(session->channel) || !switch_rtp_ready(engine->rtp_session) || switch_channel_test_flag(session->channel, CF_NOT_READY)) {
		return SWITCH_STATUS_FALSE;
	}

	if (lock) {
		if (smh->read_mutex[type] && switch_mutex_trylock(smh->read_mutex[type]) != SWITCH_STATUS_SUCCESS) {
			/* return CNG, another thread is already reading  */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s is already being read for %s\n",
							  switch_channel_get_name(session->channel), type2str(type));
			return SWITCH_STATUS_INUSE;
		}
	} else {
		switch_mutex_unlock(smh->read_mutex[type]);
	}

	return SWITCH_STATUS_SUCCESS;
}




//?
SWITCH_DECLARE(switch_status_t) switch_rtp_text_factory_create(switch_rtp_text_factory_t **tfP, switch_memory_pool_t *pool)
{
	int x;

	*tfP = switch_core_alloc(pool, sizeof(**tfP));

	switch_buffer_create_dynamic(&(*tfP)->write_buffer,  512, 1024, 0);
	(*tfP)->pool = pool;
	(*tfP)->text_write_frame_data = switch_core_alloc(pool, SWITCH_RTP_MAX_BUF_LEN);
	(*tfP)->text_write_frame.packet = (*tfP)->text_write_frame_data;
	(*tfP)->text_write_frame.data = (switch_byte_t *)(*tfP)->text_write_frame.packet + 12;
	(*tfP)->text_write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;

	(*tfP)->red_max = 5;
	(*tfP)->red_bufsize = SWITCH_RTP_MAX_BUF_LEN;

	switch_core_timer_init(&(*tfP)->timer, "soft", TEXT_TIMER_MS, TEXT_TIMER_SAMPLES, pool);

	for(x = 0; x < (*tfP)->red_max; x++) {
		(*tfP)->red_buf[x] = switch_core_alloc(pool, SWITCH_RTP_MAX_BUF_LEN);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_text_factory_destroy(switch_rtp_text_factory_t **tfP)
{
	switch_core_timer_destroy(&(*tfP)->timer);
	switch_buffer_destroy(&(*tfP)->write_buffer);

	return SWITCH_STATUS_SUCCESS;;
}

#include <wchar.h>

static int get_rtt_red_seq(int want_seq, void *data, switch_size_t datalen, int seq, switch_payload_t *new_payload, void *new_data, uint32_t *new_datalen)
{
	unsigned char *buf = data;
	int count = 0;
	unsigned char *e = (buf + datalen);

	int len[MAX_RED_FRAMES] = { 0 };
	int pt[MAX_RED_FRAMES] = { 0 };
	int idx = 0, x = 0;

	*new_datalen = datalen;

	*(buf + datalen) = '\0';

	while (*buf & 0x80) {
		if (buf + 3 > e) {
			*new_datalen = 0;
			return 0;
		}

		pt[count] = *buf & 0x7F;
		len[count] = (ntohs(*(uint16_t *)(buf + 2)) & 0x03ff);
		buf += 4;
		count++;
	}

	buf++;

	idx = count - (seq - want_seq);

	if (idx < 0) {
		*new_datalen = 0;
		return 0;
	}

	if (!len[idx]) {
		*new_datalen = len[idx];
		return 0;
	}

	for(x = 0; x < idx; x++) {
		buf += len[x];
	}

	*new_datalen = len[idx];
	*new_payload = pt[idx];

	memcpy(new_data, buf, len[idx]);

	*(((char *)new_data) + len[idx]) = '\0';

	return 1;

}

static void *get_rtt_payload(void *data, switch_size_t datalen, switch_payload_t *new_payload, uint32_t *new_datalen, int *red_level)
{
	unsigned char *buf = data;
	int bytes = 0, count = 0, pt = 0, len = 0;//, ts = 0;
	unsigned char *e = (buf + datalen);

	*new_datalen = datalen;
	*red_level = 1;

	while (*buf & 0x80) {
		if (buf + 3 > e) {
			*new_datalen = 0;
			return NULL;
		}
		count++;
		pt = *buf & 0x7F;
		//ts = ntohs(*(uint16_t *)(buf + 1)) >> 2;
		len  = (ntohs(*(uint16_t *)(buf + 2)) & 0x03ff);
		buf += 4;
		bytes += len;
	}

	*new_datalen = datalen - bytes - 1 - (count *4);
	*new_payload = pt;
	buf += bytes + 1;

	if (buf > e) {
		*new_datalen = 0;
		return NULL;
	}

	return buf;
}

//?

static void check_media_timeout_params(switch_core_session_t *session, switch_rtp_engine_t *engine)
{
	switch_media_type_t type = engine->type;
	const char *val;

	if ((val = switch_channel_get_variable(session->channel, "media_hold_timeout"))) {
		engine->media_hold_timeout = atoi(val);
	}

	if ((val = switch_channel_get_variable(session->channel, "media_timeout"))) {
		engine->media_timeout = atoi(val);
	}

	if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		if ((val = switch_channel_get_variable(session->channel, "media_hold_timeout_video"))) {
			engine->media_hold_timeout = atoi(val);
		}

		if ((val = switch_channel_get_variable(session->channel, "media_timeout_video"))) {
			engine->media_timeout = atoi(val);
		}
	} else {

		if ((val = switch_channel_get_variable(session->channel, "media_hold_timeout_audio"))) {
			engine->media_hold_timeout = atoi(val);
		}

		if ((val = switch_channel_get_variable(session->channel, "media_timeout_audio"))) {
			engine->media_timeout = atoi(val);
		}
	}

	if (switch_rtp_ready(engine->rtp_session) && engine->media_timeout) {
		switch_rtp_set_media_timeout(engine->rtp_session, engine->media_timeout);
	}

}

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

	if (type == SWITCH_MEDIA_TYPE_AUDIO && ! switch_channel_test_flag(session->channel, CF_AUDIO)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s Reading audio from a non-audio session.\n", switch_channel_get_name(session->channel));
		switch_yield(50000);
		return SWITCH_STATUS_INUSE;
	}

	if (type != SWITCH_MEDIA_TYPE_TEXT && (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec))) {
		switch_yield(50000);
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_up_nosig(session->channel) || !switch_rtp_ready(engine->rtp_session) || switch_channel_test_flag(session->channel, CF_NOT_READY)) {
		switch_yield(50000);
		return SWITCH_STATUS_FALSE;
	}

	if (smh->read_mutex[type] && switch_mutex_trylock(smh->read_mutex[type]) != SWITCH_STATUS_SUCCESS) {
		/* return CNG, another thread is already reading  */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s is already being read for %s\n",
						  switch_channel_get_name(session->channel), type2str(type));
		return SWITCH_STATUS_INUSE;
	}


	engine->read_frame.datalen = 0;
	engine->read_frame.flags = SFF_NONE;
	engine->read_frame.m = SWITCH_FALSE;
	engine->read_frame.img = NULL;
	engine->read_frame.payload = 0;

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

		if (switch_channel_test_flag(session->channel, CF_LEG_HOLDING)) {
			status = SWITCH_STATUS_INUSE;
			goto end;
		}

		if (status == SWITCH_STATUS_BREAK) {
			goto end;
		}

		if (type == SWITCH_MEDIA_TYPE_VIDEO) {
			if (engine->read_frame.m) {
				if (!smh->vid_started) {
					smh->vid_started = switch_epoch_time_now(NULL);
				}
				smh->vid_frames++;

				if ((smh->vid_frames % 5) == 0) {
					switch_core_media_get_video_fps(session);
				}

				if (video_globals.fps && (!video_globals.synced || ((smh->vid_frames % 300) == 0))) {
					check_jb_sync(session);
				}
			}
		}

		/* re-set codec if necessary */
		if (type != SWITCH_MEDIA_TYPE_TEXT && engine->reset_codec > 0) {
			const char *val;
			int rtp_timeout_sec = 0;
			int rtp_hold_timeout_sec = 0;

			engine->reset_codec = 0;

			if (switch_rtp_ready(engine->rtp_session)) {

				check_media_timeout_params(session, engine);

				if (type == SWITCH_MEDIA_TYPE_VIDEO) {
					switch_core_media_set_video_codec(session, 1);
				} else {

					if (switch_core_media_set_codec(session, 1, smh->mparams->codec_flags) != SWITCH_STATUS_SUCCESS) {
						*frame = NULL;
						switch_goto_status(SWITCH_STATUS_GENERR, end);
					}
				}

				if (type == SWITCH_MEDIA_TYPE_AUDIO && engine->read_impl.samples_per_second) {
					if ((val = switch_channel_get_variable(session->channel, "rtp_timeout_sec"))) {
						int v = atoi(val);
						if (v >= 0) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
											  "rtp_timeout_sec deprecated use media_timeout variable.\n"); 
							rtp_timeout_sec = v;
						}
					}

					if ((val = switch_channel_get_variable(session->channel, "rtp_hold_timeout_sec"))) {
						int v = atoi(val);
						if (v >= 0) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
											  "rtp_hold_timeout_sec deprecated use media_timeout variable.\n"); 
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
			}

			check_jb(session, NULL, 0, 0, SWITCH_FALSE);

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

				snprintf(value, sizeof(value), "%u", engine->read_frame.timestamp);
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
					snprintf(header, sizeof(header), "Source%u-Loss-Avg", i);
					snprintf(value, sizeof(value), "%u", rtcp_frame.reports[i].loss_avg);
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
					snprintf(header, sizeof(header), "Rtt%u-Avg", i);
					snprintf(value, sizeof(value), "%f", rtcp_frame.reports[i].rtt_avg);
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

		if (type != SWITCH_MEDIA_TYPE_TEXT && engine->read_frame.datalen > 0) {
			uint32_t bytes = 0;
			int frames = 1;

			/* autofix timing */
			if (!switch_test_flag((&engine->read_frame), SFF_CNG)) {
				if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
					*frame = NULL;
					switch_goto_status(SWITCH_STATUS_GENERR, end);
				}

				/* check for timing issues */
				if (smh->media_flags[SCMF_AUTOFIX_TIMING] && type == SWITCH_MEDIA_TYPE_AUDIO && engine->read_impl.samples_per_second) {
					char is_vbr;
					is_vbr = engine->read_impl.encoded_bytes_per_packet?0:1;

					engine->check_frames++;
					/* CBR */
					if ((smh->media_flags[SCMF_AUTOFIX_TIMING] && (engine->read_frame.datalen % 10) == 0)
							&& (engine->check_frames < MAX_CODEC_CHECK_FRAMES) && !is_vbr) {
						engine->check_frames++;

						if (engine->last_ts && engine->read_frame.datalen != engine->read_impl.encoded_bytes_per_packet) {

							uint32_t codec_ms = (int) (engine->read_frame.timestamp -
													   engine->last_ts) / (engine->read_impl.samples_per_second / 1000);
							if (engine->last_seq && (int) (engine->read_frame.seq - engine->last_seq) > 1) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[%s]: Correcting calculated ptime value from [%d] to [%d] to compensate for [%d] lost packet(s). \n", is_vbr?"VBR":"CBR", codec_ms, codec_ms / (int) (engine->read_frame.seq - engine->last_seq), (int) (engine->read_frame.seq - engine->last_seq - 1));
								codec_ms = codec_ms / (int) (engine->read_frame.seq - engine->last_seq);
							}

							if ((codec_ms % 10) != 0 || codec_ms > engine->read_impl.samples_per_packet * 10) {
								engine->last_ts = 0;
								engine->last_seq = 0;
								goto skip;
							}

							if (engine->last_codec_ms && engine->last_codec_ms == codec_ms) {
								engine->mismatch_count++;
							} else {
								engine->mismatch_count = 0;
							}

							engine->last_codec_ms = codec_ms;

							if (engine->mismatch_count > MAX_MISMATCH_FRAMES) {
								if (codec_ms != engine->cur_payload_map->codec_ms) {

									if (codec_ms > 120) {	/* yeah right */
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "[%s]: Your phone is trying to send timestamps that suggest an increment of %dms per packet\n"
														  "That seems hard to believe so I am going to go on ahead and um ignore that, mmkay?\n",
														  is_vbr?"VBR":"CBR",
														  (int) codec_ms);
										engine->check_frames = MAX_CODEC_CHECK_FRAMES;
										goto skip;
									}

									engine->read_frame.datalen = 0;

									if (codec_ms != engine->cur_payload_map->codec_ms) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "[%s]: Asynchronous PTIME not supported, changing our end from %d to %d\n",
														  is_vbr?"VBR":"CBR",
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

					} else if (smh->media_flags[SCMF_AUTOFIX_TIMING] && is_vbr && switch_rtp_get_jitter_buffer(engine->rtp_session)
							   && type == SWITCH_MEDIA_TYPE_AUDIO
							   && engine->read_frame.timestamp && engine->read_frame.seq && engine->read_impl.samples_per_second) {
						uint32_t codec_ms = (int) (engine->read_frame.timestamp -
								   engine->last_ts) / (engine->read_impl.samples_per_second / 1000);

						if (engine->last_seq && (int) (engine->read_frame.seq - engine->last_seq) > 1) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "[%s]: Correcting calculated ptime value from [%d] to [%d] to compensate for [%d] lost packet(s)\n", is_vbr?"VBR":"CBR", codec_ms, codec_ms / (int) (engine->read_frame.seq - engine->last_seq), (int) (engine->read_frame.seq - engine->last_seq - 1));
								codec_ms = codec_ms / (int) (engine->read_frame.seq - engine->last_seq);
						}

						if (codec_ms && codec_ms != engine->cur_payload_map->codec_ms) {
							if (engine->last_codec_ms && engine->last_codec_ms == codec_ms) {
								engine->mismatch_count++;
							}
						} else {
							engine->mismatch_count = 0;
						}

						engine->last_codec_ms = codec_ms;

						if (engine->mismatch_count > MAX_MISMATCH_FRAMES) {

							if (codec_ms > 120) {
								/*will show too many times with packet loss*/
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG3,
												  "[%s]: Remote party is trying to send timestamps that suggest an increment of [%d] ms per packet, which is too high. Ignoring.\n",
												   is_vbr?"VBR":"CBR",
												  (int) codec_ms);
								engine->last_ts = engine->read_frame.timestamp;
								engine->last_seq = engine->read_frame.seq;
								goto skip;
							}

							if (codec_ms != engine->cur_payload_map->codec_ms) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
												  "[%s]: Packet size change detected. Remote PTIME changed from [%d] to [%d]\n",
												  is_vbr?"VBR":"CBR",
												  (int) engine->cur_payload_map->codec_ms,
												  (int) codec_ms
												  );
			 					engine->cur_payload_map->codec_ms = codec_ms;
								engine->reset_codec = 2;

								if (switch_channel_test_flag(session->channel, CF_CONFERENCE)) {
									switch_channel_set_flag(session->channel, CF_CONFERENCE_RESET_MEDIA);
								}
							}
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
					!switch_test_flag((&engine->read_frame), SFF_CNG) &&
					!switch_test_flag((&engine->read_frame), SFF_PLC) &&
					engine->read_frame.payload != engine->cur_payload_map->recv_pt &&
					engine->read_frame.payload != engine->cur_payload_map->pt) {

					payload_map_t *pmap;


					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "alternate payload received (received %d, expecting %d)\n",
									  (int) engine->read_frame.payload, (int) engine->cur_payload_map->pt);


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


	if (type == SWITCH_MEDIA_TYPE_TEXT && !switch_test_flag((&engine->read_frame), SFF_CNG)) {
		if (engine->red_pt) {
			unsigned char *p = engine->read_frame.data;

			*(p + engine->read_frame.datalen) = '\0';
			engine->tf->text_frame = engine->read_frame;

			if (switch_test_flag((&engine->read_frame), SFF_PLC)) {
				switch_jb_t *jb = switch_core_session_get_jb(session, SWITCH_MEDIA_TYPE_TEXT);
				int i = 0;

				engine->tf->text_frame.datalen = 0;

				for (i = 1; i < 3; i++) {
					switch_frame_t frame = { 0 };
					uint8_t buf[SWITCH_RTP_MAX_BUF_LEN];
					frame.data = buf;
					frame.buflen = sizeof(buf);

					if (switch_jb_peek_frame(jb, 0, engine->read_frame.seq, i, &frame) == SWITCH_STATUS_SUCCESS) {
						if (get_rtt_red_seq(engine->read_frame.seq,
											frame.data,
											frame.datalen,
											frame.seq,
											&engine->tf->text_frame.payload,
											engine->tf->text_frame.data,
											&engine->tf->text_frame.datalen)) {
							break;

						}
					}

				}

				if (engine->tf->text_frame.datalen == 0) {
					engine->tf->text_frame.data = "� ";
					engine->tf->text_frame.datalen = strlen(engine->tf->text_frame.data);
				}

			} else {
				if (!(engine->tf->text_frame.data = get_rtt_payload(engine->read_frame.data,
																	engine->tf->text_frame.datalen,
																	&engine->tf->text_frame.payload,
																	&engine->tf->text_frame.datalen,
																	&engine->tf->red_level))) {
					engine->tf->text_frame.datalen = 0;
				}
			}

			*frame = &engine->tf->text_frame;

			if ((*frame)->datalen == 0) {
				(*frame)->flags |= SFF_CNG;
				(*frame)->data = "";
			}
		}

	} else {
		*frame = &engine->read_frame;
	}

	status = SWITCH_STATUS_SUCCESS;

 end:

	if (smh->read_mutex[type]) {
		switch_mutex_unlock(smh->read_mutex[type]);
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

	if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		if (engine->thread_write_lock && engine->thread_write_lock != switch_thread_self()) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO_ONLY) && type == SWITCH_MEDIA_TYPE_AUDIO) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (type != SWITCH_MEDIA_TYPE_TEXT) {

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

		if (!switch_test_flag(frame, SFF_CNG) && !switch_test_flag(frame, SFF_PROXY_PACKET)) {
			if (engine->read_impl.encoded_bytes_per_packet) {
				bytes = engine->read_impl.encoded_bytes_per_packet;
				frames = ((int) frame->datalen / bytes);
			} else
				frames = 1;

			samples = frames * engine->read_impl.samples_per_packet;
		}
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

//#define get_int_value(_var, _set) { const char *__v = switch_channel_get_variable(session->channel, _var); if (__v) { _set = atol(__v);} }
//?
static void switch_core_session_parse_codec_settings(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (!(engine = &smh->engines[type])) return;

	switch(type) {
	case SWITCH_MEDIA_TYPE_AUDIO:
		break;
	case SWITCH_MEDIA_TYPE_VIDEO: {
		uint32_t system_bw = 0;
		const char *var = NULL, *bwv;

		if ((var = switch_channel_get_variable(session->channel, "video_try_hardware_encoder"))) {
			engine->codec_settings.video.try_hardware_encoder = switch_true(var);
		}

		if (!(bwv = switch_channel_get_variable(session->channel, "rtp_video_max_bandwidth"))) {
			bwv = switch_channel_get_variable(session->channel, "rtp_video_max_bandwidth_out");
		}

		if (!bwv) {
			bwv = "1mb";
		}

		system_bw = switch_parse_bandwidth_string(bwv);

		if (engine->sdp_bw && engine->sdp_bw <= system_bw) {
			engine->codec_settings.video.bandwidth = engine->sdp_bw;
		} else {
			engine->codec_settings.video.bandwidth = system_bw;
		}
	}
		break;
	default:
		break;
	}
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

	switch_core_session_parse_codec_settings(session, SWITCH_MEDIA_TYPE_VIDEO);

	if (switch_core_codec_init(&v_engine->read_codec,
							   v_engine->cur_payload_map->rm_encoding,
							   v_engine->cur_payload_map->modname,
							   v_engine->cur_payload_map->rm_fmtp,
							   v_engine->cur_payload_map->rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &v_engine->codec_settings, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&v_engine->write_codec,
								   v_engine->cur_payload_map->rm_encoding,
								   v_engine->cur_payload_map->modname,
								   v_engine->cur_payload_map->rm_fmtp,
								   v_engine->cur_payload_map->rm_rate,
								   0,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   &v_engine->codec_settings, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			v_engine->read_frame.rate = v_engine->cur_payload_map->rm_rate;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(session->channel), v_engine->cur_payload_map->rm_encoding,
							  v_engine->cur_payload_map->rm_rate, v_engine->cur_payload_map->codec_ms);
			v_engine->read_frame.codec = &v_engine->read_codec;

			v_engine->write_codec.fmtp_out = switch_core_session_strdup(session, v_engine->write_codec.fmtp_out);

			v_engine->write_codec.agreed_pt = v_engine->cur_payload_map->pt;
			v_engine->read_codec.agreed_pt = v_engine->cur_payload_map->pt;
			switch_core_session_set_video_read_codec(session, &v_engine->read_codec);
			switch_core_session_set_video_write_codec(session, &v_engine->write_codec);


			switch_channel_set_variable_printf(session->channel, "rtp_last_video_codec_string", "%s@%dh",
											   v_engine->cur_payload_map->rm_encoding, v_engine->cur_payload_map->rm_rate);


			if (switch_rtp_ready(v_engine->rtp_session)) {
				switch_core_session_message_t msg = { 0 };

				msg.from = __FILE__;
				msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;

				switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->pt);

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
							  a_engine->read_impl.actual_samples_per_second,

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


	switch_core_session_parse_codec_settings(session, SWITCH_MEDIA_TYPE_AUDIO);

	if (switch_core_codec_init_with_bitrate(&a_engine->read_codec,
											a_engine->cur_payload_map->iananame,
											a_engine->cur_payload_map->modname,
											a_engine->cur_payload_map->rm_fmtp,
											a_engine->cur_payload_map->rm_rate,
											a_engine->cur_payload_map->codec_ms,
											a_engine->cur_payload_map->channels,
											a_engine->cur_payload_map->bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											&a_engine->codec_settings, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	a_engine->read_codec.session = session;


	if (switch_core_codec_init_with_bitrate(&a_engine->write_codec,
											a_engine->cur_payload_map->iananame,
											a_engine->cur_payload_map->modname,
											a_engine->cur_payload_map->rm_fmtp,
											a_engine->cur_payload_map->rm_rate,
											a_engine->cur_payload_map->codec_ms,
											a_engine->cur_payload_map->channels,
											a_engine->cur_payload_map->bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											&a_engine->codec_settings, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	a_engine->write_codec.session = session;

	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_channel_audio_sync(session->channel);
		switch_rtp_reset_jb(a_engine->rtp_session);
	}

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
	a_engine->write_codec.agreed_pt = a_engine->cur_payload_map->pt;
	a_engine->read_codec.agreed_pt = a_engine->cur_payload_map->pt;

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
	engine->ice_in.is_chosen[0] = 0;
	engine->ice_in.is_chosen[1] = 0;
	engine->ice_in.cand_idx[0] = 0;
	engine->ice_in.cand_idx[1] = 0;
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
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (a_engine->rtp_session) {
		switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}

	if (v_engine->rtp_session) {
		switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}

	if (t_engine->rtp_session) {
		switch_rtp_set_flag(t_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}
}

SWITCH_DECLARE(void) switch_core_media_resume(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (a_engine->rtp_session) {
		switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}

	if (v_engine->rtp_session) {
		switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
	}

	if (t_engine->rtp_session) {
		switch_rtp_clear_flag(t_engine->rtp_session, SWITCH_RTP_FLAG_PAUSE);
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
static switch_call_direction_t switch_ice_direction(switch_rtp_engine_t *engine, switch_core_session_t *session)
{
	switch_call_direction_t r = switch_channel_direction(session->channel);
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_CALL_DIRECTION_OUTBOUND;
	}
	
	if (switch_channel_test_flag(session->channel, CF_3PCC)) {
		r = (r == SWITCH_CALL_DIRECTION_INBOUND) ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND;
	}

	if (switch_rtp_has_dtls() && dtls_ok(smh->session)) {
		r = engine->dtls_controller ? SWITCH_CALL_DIRECTION_INBOUND : SWITCH_CALL_DIRECTION_OUTBOUND;
	} else {
		if ((switch_channel_test_flag(session->channel, CF_REINVITE) || switch_channel_test_flag(session->channel, CF_RECOVERING))
			&& switch_channel_test_flag(session->channel, CF_AVPF)) {
			r = SWITCH_CALL_DIRECTION_OUTBOUND;
		}
	}

	return r;
}

static switch_core_media_ice_type_t switch_determine_ice_type(switch_rtp_engine_t *engine, switch_core_session_t *session) {
	switch_core_media_ice_type_t ice_type = ICE_VANILLA;

	if (switch_channel_var_true(session->channel, "ice_lite")) {
		ice_type |= ICE_CONTROLLED;
		ice_type |= ICE_LITE;
	} else {
		switch_call_direction_t direction = switch_ice_direction(engine, session);
		if (direction == SWITCH_CALL_DIRECTION_INBOUND) {
			ice_type |= ICE_CONTROLLED;
		}
	}

	return ice_type;
}

//?
static switch_status_t ip_choose_family(switch_media_handle_t *smh, const char *ip)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (zstr(ip)) {
		return status;
	}

	if (strchr(ip, ':')) {
		if (!zstr(smh->mparams->rtpip6)) {
			smh->mparams->rtpip = smh->mparams->rtpip6;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "%s choosing family v6\n",
							  switch_channel_get_name(smh->session->channel));
			status = SWITCH_STATUS_SUCCESS;
		}
	} else {
		if (!zstr(smh->mparams->rtpip4)) {
			smh->mparams->rtpip = smh->mparams->rtpip4;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "%s choosing family v4\n",
							  switch_channel_get_name(smh->session->channel));
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

//?
static switch_bool_t ip_possible(switch_media_handle_t *smh, const char *ip)
{
	switch_bool_t r = SWITCH_FALSE;

	if (zstr(ip)) {
		return r;
	}

	if (strchr(ip, ':')) {
		r = (switch_bool_t) !zstr(smh->mparams->rtpip6);
	} else {
		r = (switch_bool_t) !zstr(smh->mparams->rtpip4);
	}

	return r;
}

//?
static switch_status_t check_ice(switch_media_handle_t *smh, switch_media_type_t type, sdp_session_t *sdp, sdp_media_t *m)
{
	switch_rtp_engine_t *engine = &smh->engines[type];
	sdp_attribute_t *attr = NULL, *attrs[2] = { 0 };
	int i = 0, got_rtcp_mux = 0;
	const char *val;
	int ice_seen = 0, cid = 0, ai = 0, attr_idx = 0, cand_seen = 0, relay_ok = 0;

	if (switch_true(switch_channel_get_variable_dup(smh->session->channel, "ignore_sdp_ice", SWITCH_FALSE, -1))) {
		return SWITCH_STATUS_BREAK;
	}

	//if (engine->ice_in.is_chosen[0] && engine->ice_in.is_chosen[1]) {
		//return SWITCH_STATUS_SUCCESS;
	//}

	engine->ice_in.chosen[0] = 0;
	engine->ice_in.chosen[1] = 0;
	engine->ice_in.is_chosen[0] = 0;
	engine->ice_in.is_chosen[1] = 0;
	engine->ice_in.cand_idx[0] = 0;
	engine->ice_in.cand_idx[1] = 0;
	engine->remote_ssrc = 0;

	if (m) {
		attrs[0] = m->m_attributes;
		attrs[1] = sdp->sdp_attributes;
	} else {
		attrs[0] = sdp->sdp_attributes;
	}

	for (attr_idx = 0; attr_idx < 2 && !(ice_seen && cand_seen); attr_idx++) {
		for (attr = attrs[attr_idx]; attr; attr = attr->a_next) {
			char *data;
			char *fields[32] = {0};
			int argc = 0, j = 0;

			if (zstr(attr->a_name)) {
				continue;
			}

			if (!strcasecmp(attr->a_name, "ice-ufrag")) {
				if (engine->ice_in.ufrag && !strcmp(engine->ice_in.ufrag, attr->a_value)) {
					engine->new_ice = 0;
				} else {
					engine->ice_in.ufrag = switch_core_session_strdup(smh->session, attr->a_value);
					engine->new_ice = 1;
				}
				ice_seen++;
			} else if (!strcasecmp(attr->a_name, "ice-pwd")) {
				if (!engine->ice_in.pwd || strcmp(engine->ice_in.pwd, attr->a_value)) {
					engine->ice_in.pwd = switch_core_session_strdup(smh->session, attr->a_value);
				}
			} else if (!strcasecmp(attr->a_name, "ice-options")) {
				engine->ice_in.options = switch_core_session_strdup(smh->session, attr->a_value);
			} else if (!strcasecmp(attr->a_name, "setup")) {
				if (!strcasecmp(attr->a_value, "passive") ||
					(!strcasecmp(attr->a_value, "actpass") && !switch_channel_test_flag(smh->session->channel, CF_REINVITE))) {
					if (!engine->dtls_controller) {
						engine->new_dtls = 1;
						engine->new_ice = 1;
					}
					engine->dtls_controller = 1;
				} else if (!strcasecmp(attr->a_value, "active")) {
					if (engine->dtls_controller) {
						engine->new_dtls = 1;
						engine->new_ice = 1;
					}
					engine->dtls_controller = 0;
				}
			} else if (switch_rtp_has_dtls() && dtls_ok(smh->session) && !strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
				char *p;

				engine->remote_dtls_fingerprint.type = switch_core_session_strdup(smh->session, attr->a_value);

				if ((p = strchr(engine->remote_dtls_fingerprint.type, ' '))) {
					*p++ = '\0';

					if (switch_channel_test_flag(smh->session->channel, CF_REINVITE) && !switch_channel_test_flag(smh->session->channel, CF_RECOVERING) &&
						!zstr(engine->remote_dtls_fingerprint.str) && !strcmp(engine->remote_dtls_fingerprint.str, p)) {
						engine->new_dtls = 0;
					} else {
						switch_set_string(engine->remote_dtls_fingerprint.str, p);
						engine->new_dtls = 1;
						engine->new_ice = 1;
					}
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

				if (!smh->mparams->rtcp_audio_interval_msec) {
					smh->mparams->rtcp_audio_interval_msec = SWITCH_RTCP_AUDIO_INTERVAL_MSEC;
				}
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

				cid = fields[1] ? atoi(fields[1]) - 1 : 0;

				if (argc < 6 || engine->ice_in.cand_idx[cid] >= MAX_CAND - 1) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_WARNING, "Invalid data\n");
					continue;
				}

				for (i = 0; i < argc; i++) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG1, "CAND %d [%s]\n", i, fields[i]);
				}

				if (!ip_possible(smh, fields[4])) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
									  "Drop %s Candidate cid: %d proto: %s type: %s addr: %s:%s (no network path)\n",
									  type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : "audio",
									  cid+1, fields[2], fields[7] ? fields[7] : "N/A", fields[4], fields[5]);
					continue;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
									  "Save %s Candidate cid: %d proto: %s type: %s addr: %s:%s\n",
									  type == SWITCH_MEDIA_TYPE_VIDEO ? "video" : "audio",
									  cid+1, fields[2], fields[7] ? fields[7] : "N/A", fields[4], fields[5]);
				}


				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].foundation = switch_core_session_strdup(smh->session, fields[0]);
				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].component_id = atoi(fields[1]);
				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].transport = switch_core_session_strdup(smh->session, fields[2]);
				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].priority = atol(fields[3]);
				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].con_addr = switch_core_session_strdup(smh->session, fields[4]);
				engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].con_port = (switch_port_t)atoi(fields[5]);

				j = 6;

				while(j < argc && j <= sizeof(fields)/sizeof(char*) && fields[j+1] && engine->ice_in.cand_idx[cid] < MAX_CAND - 1) {
					if (!strcasecmp(fields[j], "typ")) {
						engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].cand_type = switch_core_session_strdup(smh->session, fields[j+1]);
					} else if (!strcasecmp(fields[j], "raddr")) {
						engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].raddr = switch_core_session_strdup(smh->session, fields[j+1]);
					} else if (!strcasecmp(fields[j], "rport")) {
						engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].rport = (switch_port_t)atoi(fields[j+1]);
					} else if (!strcasecmp(fields[j], "generation")) {
						engine->ice_in.cands[engine->ice_in.cand_idx[cid]][cid].generation = switch_core_session_strdup(smh->session, fields[j+1]);
					}

					j += 2;
				}

				cand_seen++;
				engine->ice_in.cand_idx[cid]++;
			}
		}
	}

	if (!ice_seen) {
		return SWITCH_STATUS_SUCCESS;
	}

	relay_ok = 0;
	
 relay:
	
	for (cid = 0; cid < 2; cid++) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "Searching for %s candidate.\n", cid ? "rtcp" : "rtp");

		for (ai = 0; ai < engine->cand_acl_count; ai++) {
			for (i = 0; i < engine->ice_in.cand_idx[cid]; i++) {
				int is_relay = engine->ice_in.cands[i][cid].cand_type && !strcmp(engine->ice_in.cands[i][cid].cand_type, "relay");

				if (relay_ok != is_relay) continue;

				if (switch_check_network_list_ip(engine->ice_in.cands[i][cid].con_addr, engine->cand_acl[ai])) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
									  "Choose %s candidate, index %d, %s:%d\n", cid ? "rtcp" : "rtp", i,
									  engine->ice_in.cands[i][cid].con_addr, engine->ice_in.cands[i][cid].con_port);

					engine->ice_in.chosen[cid] = i;
					engine->ice_in.is_chosen[cid] = 1;
					engine->ice_in.cands[i][cid].ready++;
					ip_choose_family(smh, engine->ice_in.cands[i][cid].con_addr);

					if (cid == 0 && got_rtcp_mux && engine->ice_in.cand_idx[1] < MAX_CAND) {

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
										  "Choose same candidate, index %d, for rtcp based on rtcp-mux attribute %s:%d\n", engine->ice_in.cand_idx[1],
										  engine->ice_in.cands[i][cid].con_addr, engine->ice_in.cands[i][cid].con_port);


						engine->ice_in.cands[engine->ice_in.cand_idx[1]][1] = engine->ice_in.cands[i][0];
						engine->ice_in.chosen[1] = engine->ice_in.cand_idx[1];
						engine->ice_in.is_chosen[1] = 1;
						engine->ice_in.cand_idx[1]++;

						goto done_choosing;
					}

					goto next_cid;
				}
			}
		}

	next_cid:

		continue;
	}

 done_choosing:

	if (!engine->ice_in.is_chosen[0]) {
		if (!relay_ok) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "Look for Relay Candidates as last resort\n");
			relay_ok = 1;
			goto relay;
		}

		/* PUNT */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "%s no suitable candidates found.\n",
						  switch_channel_get_name(smh->session->channel));
		return SWITCH_STATUS_FALSE;
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
		const char *media_varname = NULL, *port_varname = NULL;

		engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(smh->session, (char *) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
						  "setting remote %s ice addr to index %d %s:%d based on candidate\n", type2str(type), engine->ice_in.chosen[0],
						  engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port);
		engine->ice_in.cands[engine->ice_in.chosen[0]][0].ready++;

		engine->remote_rtp_ice_port = (switch_port_t) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port;
		engine->remote_rtp_ice_addr = switch_core_session_strdup(smh->session, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);

		engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(smh->session, (char *) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);
		engine->cur_payload_map->remote_sdp_port = (switch_port_t) engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port;

		if (!smh->mparams->remote_ip) {
			smh->mparams->remote_ip = engine->cur_payload_map->remote_sdp_ip;
		}

		if (engine->type == SWITCH_MEDIA_TYPE_VIDEO) {
			media_varname = "remote_video_ip";
			port_varname = "remote_video_port";
		} else if (engine->type == SWITCH_MEDIA_TYPE_AUDIO) {
			media_varname = "remote_audio_ip";
			port_varname = "remote_audio_port";
		} else if (engine->type == SWITCH_MEDIA_TYPE_TEXT) {
			media_varname = "remote_text_ip";
			port_varname = "remote_text_port";
		}

		switch_snprintf(tmp, sizeof(tmp), "%d", engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_port);
		switch_channel_set_variable(smh->session->channel, media_varname, engine->ice_in.cands[engine->ice_in.chosen[0]][0].con_addr);
		switch_channel_set_variable(smh->session->channel, port_varname, tmp);
	}

	if (engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port) {
		const char *media_varname = NULL, *port_varname = NULL;
		char tmp[35] = "";

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG,
						  "Setting remote rtcp %s addr to %s:%d based on candidate\n", type2str(type),
						  engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port);
		engine->remote_rtcp_ice_port = engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port;
		engine->remote_rtcp_ice_addr = switch_core_session_strdup(smh->session, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr);

		engine->remote_rtcp_port = engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port;


		if (engine->type == SWITCH_MEDIA_TYPE_VIDEO) {
			media_varname = "remote_video_rtcp_ip";
			port_varname = "remote_video_rtcp_port";
		} else if (engine->type == SWITCH_MEDIA_TYPE_AUDIO) {
			media_varname = "remote_audio_rtcp_ip";
			port_varname = "remote_audio_rtcp_port";
		} else if (engine->type == SWITCH_MEDIA_TYPE_TEXT) {
			media_varname = "remote_text_rtcp_ip";
			port_varname = "remote_text_rtcp_port";
		}

		switch_snprintf(tmp, sizeof(tmp), "%d", engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_port);
		switch_channel_set_variable(smh->session->channel, media_varname, engine->ice_in.cands[engine->ice_in.chosen[1]][1].con_addr);
		switch_channel_set_variable(smh->session->channel, port_varname, tmp);

	}


	if (m && !got_rtcp_mux) {
		engine->rtcp_mux = -1;
	}

	if (engine->new_ice) {
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
									switch_determine_ice_type(engine, smh->session),
									&engine->ice_in
#endif
									);


			engine->new_ice = 0;
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
						interval = 5000;
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
										switch_determine_ice_type(engine, smh->session),
										&engine->ice_in
#endif
										);
			}

		}

	}

	return ice_seen ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_BREAK;
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
	switch_channel_set_flag(session->channel, CF_AVPF);
	switch_channel_set_flag(session->channel, CF_ICE);
	smh->mparams->rtcp_audio_interval_msec = SWITCH_RTCP_AUDIO_INTERVAL_MSEC;
	smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;

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

static void restore_pmaps(switch_rtp_engine_t *engine)
{
	payload_map_t *pmap;
	int top = 0;

	for (pmap = engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {
		pmap->negotiated = 1;
		if (!top++) pmap->current = 1;
	}
}

static const char *media_flow_varname(switch_media_type_t type)
{
	const char *varname = "invalid";

	switch(type) {
	case SWITCH_MEDIA_TYPE_AUDIO:
		varname = "audio_media_flow";
		break;
	case SWITCH_MEDIA_TYPE_VIDEO:
		varname = "video_media_flow";
		break;
	case SWITCH_MEDIA_TYPE_TEXT:
		varname = "text_media_flow";
		break;
	}

	return varname;
}

static const char *remote_media_flow_varname(switch_media_type_t type)
{
	const char *varname = "invalid";

	switch(type) {
	case SWITCH_MEDIA_TYPE_AUDIO:
		varname = "remote_audio_media_flow";
		break;
	case SWITCH_MEDIA_TYPE_VIDEO:
		varname = "remote_video_media_flow";
		break;
	case SWITCH_MEDIA_TYPE_TEXT:
		varname = "remote_text_media_flow";
		break;
	}

	return varname;
}

static void media_flow_get_mode(switch_media_flow_t smode, const char **mode_str, switch_media_flow_t *opp_mode)
{
	const char *smode_str = "";
	switch_media_flow_t opp_smode = smode;

	switch(smode) {
	case SWITCH_MEDIA_FLOW_SENDONLY:
		opp_smode = SWITCH_MEDIA_FLOW_RECVONLY;
		smode_str = "sendonly";
		break;
	case SWITCH_MEDIA_FLOW_RECVONLY:
		opp_smode = SWITCH_MEDIA_FLOW_SENDONLY;
		smode_str = "recvonly";
		break;
	case SWITCH_MEDIA_FLOW_INACTIVE:
		smode_str = "inactive";
		break;
	case SWITCH_MEDIA_FLOW_DISABLED:
		smode_str = "disabled";
		break;
	case SWITCH_MEDIA_FLOW_SENDRECV:
		smode_str = "sendrecv";
		break;
	}

	*mode_str = smode_str;
	*opp_mode = opp_smode;
	
}

static void check_stream_changes(switch_core_session_t *session, const char *r_sdp, switch_sdp_type_t sdp_type)
{
	switch_core_session_t *other_session = NULL;
	switch_core_session_message_t *msg;

	switch_core_session_get_partner(session, &other_session);


	if (switch_channel_test_flag(session->channel, CF_STREAM_CHANGED)) {
		switch_channel_clear_flag(session->channel, CF_STREAM_CHANGED);
		
		if (other_session) {
			switch_channel_set_flag(other_session->channel, CF_PROCESSING_STREAM_CHANGE);
			switch_channel_set_flag(session->channel, CF_AWAITING_STREAM_CHANGE);

			if (sdp_type == SDP_TYPE_REQUEST && r_sdp) {
				const char *filter_codec_string = switch_channel_get_variable(session->channel, "filter_codec_string");
				
				switch_channel_set_variable(session->channel, "codec_string", NULL);
				switch_core_media_merge_sdp_codec_string(session, r_sdp, sdp_type, filter_codec_string);
			}

			if (switch_channel_test_flag(session->channel, CF_SECURE)) {
				other_session->media_handle->crypto_mode = session->media_handle->crypto_mode;
				switch_core_session_check_outgoing_crypto(other_session);
			}

			msg = switch_core_session_alloc(other_session, sizeof(*msg));
			msg->message_id = SWITCH_MESSAGE_INDICATE_MEDIA_RENEG;
			msg->string_arg = switch_core_session_sprintf(other_session, "=%s", switch_channel_get_variable(session->channel, "ep_codec_string"));
			msg->from = __FILE__;
			switch_core_session_queue_message(other_session, msg);
		}
	}

	if (other_session) {
		if (sdp_type == SDP_TYPE_RESPONSE && switch_channel_test_flag(session->channel, CF_PROCESSING_STREAM_CHANGE)) {
			switch_channel_clear_flag(session->channel, CF_PROCESSING_STREAM_CHANGE);
			
			if (switch_channel_test_flag(other_session->channel, CF_AWAITING_STREAM_CHANGE)) {
				uint8_t proceed = 1;
				const char *sdp_in, *other_ep;

				if ((other_ep = switch_channel_get_variable(session->channel, "ep_codec_string"))) {
					switch_channel_set_variable(other_session->channel, "codec_string", other_ep);
				}

				sdp_in = switch_channel_get_variable(other_session->channel, SWITCH_R_SDP_VARIABLE);
				switch_core_media_negotiate_sdp(other_session, sdp_in, &proceed, SDP_TYPE_REQUEST);
				switch_core_media_activate_rtp(other_session);
				msg = switch_core_session_alloc(other_session, sizeof(*msg));
				msg->message_id = SWITCH_MESSAGE_INDICATE_RESPOND;
				msg->from = __FILE__;

				switch_channel_set_flag(other_session->channel, CF_AWAITING_STREAM_CHANGE);
				switch_core_session_queue_message(other_session, msg);
			}
		}

		switch_core_session_rwunlock(other_session);
	}
}

SWITCH_DECLARE(void) switch_core_media_set_smode(switch_core_session_t *session, switch_media_type_t type, switch_media_flow_t smode, switch_sdp_type_t sdp_type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	const char *varname = NULL, *smode_str = NULL;
	switch_media_flow_t old_smode, opp_smode = smode;
	switch_core_session_t *other_session;
	int pass_codecs = 0;
	
	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

	varname = media_flow_varname(type);
	
	media_flow_get_mode(smode, &smode_str, &opp_smode);

	old_smode = engine->smode;

	engine->smode = smode;

	switch_channel_set_variable(session->channel, varname, smode_str);

	if (switch_channel_var_true(session->channel, "rtp_pass_codecs_on_reinvite") || engine->pass_codecs) {
		pass_codecs = 1;
	}

	engine->pass_codecs = 0;
	
	if (switch_channel_var_true(session->channel, "rtp_pass_codecs_on_stream_change")) {
		if (sdp_type == SDP_TYPE_REQUEST && switch_channel_test_flag(session->channel, CF_REINVITE) && 
			switch_channel_media_up(session->channel) && (pass_codecs || old_smode != smode)) {

			if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {
				switch_core_media_set_smode(other_session, type, opp_smode, SDP_TYPE_REQUEST);
				switch_channel_set_flag(session->channel, CF_STREAM_CHANGED);
				switch_core_session_rwunlock(other_session);
			}
		}
	}
}

static void switch_core_media_set_rmode(switch_core_session_t *session, switch_media_type_t type, switch_media_flow_t rmode, switch_sdp_type_t sdp_type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	const char *varname = NULL, *rmode_str = NULL;
	switch_media_flow_t opp_rmode = rmode;
	switch_core_session_t *other_session = NULL;

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];
	
	varname = remote_media_flow_varname(type);
	media_flow_get_mode(rmode, &rmode_str, &opp_rmode);

	if (engine->rmode != rmode) {
		engine->pass_codecs = 1;
	}
	
	engine->rmode = rmode;

	if (switch_core_session_get_partner(session, &other_session) == SWITCH_STATUS_SUCCESS) {

		if (!switch_channel_media_up(session->channel) && sdp_type == SDP_TYPE_REQUEST) {
			engine->rmode = switch_core_session_remote_media_flow(other_session, type);
			
			media_flow_get_mode(engine->rmode, &rmode_str, &opp_rmode);
		} else if (sdp_type == SDP_TYPE_RESPONSE && (switch_channel_test_flag(other_session->channel, CF_REINVITE) || switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND)) {
			switch_core_media_set_smode(other_session, type, rmode, sdp_type);
		}

		switch_core_session_rwunlock(other_session);
	}

	switch_channel_set_variable(session->channel, varname, rmode_str);
}

//?
SWITCH_DECLARE(uint8_t) switch_core_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, uint8_t *proceed, switch_sdp_type_t sdp_type)
{
	uint8_t match = 0, vmatch = 0, almost_vmatch = 0, tmatch = 0, fmatch = 0;
	switch_payload_t best_te = 0, cng_pt = 0;
	unsigned long best_te_rate = 8000, cng_rate = 8000;
	sdp_media_t *m;
	sdp_attribute_t *attr;
	int ptime = 0, dptime = 0, maxptime = 0, dmaxptime = 0;
	int sendonly = 0, recvonly = 0, inactive = 0;
	int greedy = 0, x = 0, skip = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *val;
	const char *crypto = NULL;
	int got_crypto = 0, got_video_crypto = 0, got_audio = 0, saw_audio = 0, saw_video = 0, got_avp = 0, got_video_avp = 0, got_video_savp = 0, got_savp = 0, got_udptl = 0, got_webrtc = 0, got_text = 0, got_text_crypto = 0, got_msrp = 0;
	int scrooge = 0;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	const switch_codec_implementation_t **codec_array;
	int total_codecs;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;
	uint32_t near_rate = 0;
	const switch_codec_implementation_t *mimp = NULL, *near_match = NULL;
	sdp_rtpmap_t *mmap = NULL, *near_map = NULL;
	struct matches matches[MAX_MATCHES] = { { 0 } };
	struct matches near_matches[MAX_MATCHES] = { { 0 } };
	int codec_ms = 0;
	uint32_t remote_codec_rate = 0, fmtp_remote_codec_rate = 0;
	const char *tmp;
	int m_idx = 0, skip_rtcp = 0, skip_video_rtcp = 0, got_rtcp_mux = 0, got_video_rtcp_mux = 0;
	int nm_idx = 0;
	int vmatch_pt = 1, consider_video_fmtp = 1;
	int rtcp_auto_audio = 0, rtcp_auto_video = 0;
	int got_audio_rtcp = 0, got_video_rtcp = 0;
	switch_port_t audio_port = 0, video_port = 0;

	switch_assert(session);

	if (!r_sdp) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Tried to negotiate a blank SDP?\n");
		return 0;
	}

	if (!(smh = session->media_handle)) {
		return 0;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	smh->mparams->num_codecs = 0;
	smh->num_negotiated_codecs = 0;
	switch_core_media_prepare_codecs(session, SWITCH_TRUE);
	codec_array = smh->codecs;
	total_codecs = smh->mparams->num_codecs;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	switch_channel_clear_flag(channel, CF_AUDIO_PAUSE_READ);
	switch_channel_clear_flag(channel, CF_AUDIO_PAUSE_WRITE);

	if (dtls_ok(session) && (tmp = switch_channel_get_variable(smh->session->channel, "webrtc_enable_dtls")) && switch_false(tmp)) {
		switch_channel_clear_flag(smh->session->channel, CF_DTLS_OK);
		switch_channel_clear_flag(smh->session->channel, CF_DTLS);
	}

	if (switch_true(switch_channel_get_variable_dup(session->channel, "rtp_assume_rtcp", SWITCH_FALSE, -1))) {
		rtcp_auto_video = 1;
		rtcp_auto_audio = 1;
	}

	v_engine->new_dtls = 1;
	v_engine->new_ice = 1;
	a_engine->new_dtls = 1;
	a_engine->new_ice = 1;
	a_engine->reject_avp = 0;

	switch_media_handle_set_media_flag(smh, SCMF_RECV_SDP);

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

	/* check dtmf_type variable */
	switch_core_media_check_dtmf_type(session);

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
	check_ice(smh, SWITCH_MEDIA_TYPE_TEXT, sdp, NULL);

	if ((sdp->sdp_connection && sdp->sdp_connection->c_address && !strcmp(sdp->sdp_connection->c_address, "0.0.0.0"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "RFC2543 from March 1999 called; They want their 0.0.0.0 hold method back.....\n");
		sendonly = 2;			/* global sendonly always wins */
	}

	memset(smh->rejected_streams, 0, sizeof(smh->rejected_streams));
	smh->rej_idx = 0;

	switch_core_media_set_rmode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);
	switch_core_media_set_rmode(smh->session, SWITCH_MEDIA_TYPE_TEXT, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);
	
	for (m = sdp->sdp_media; m; m = m->m_next) {
		sdp_connection_t *connection;
		switch_core_session_t *other_session;

		if (!m->m_port && smh->rej_idx < MAX_REJ_STREAMS - 1) {

			switch(m->m_type) {
			case sdp_media_audio:
				smh->rejected_streams[smh->rej_idx++] = sdp_media_audio;
				continue;
			case sdp_media_video:
				smh->rejected_streams[smh->rej_idx++] = sdp_media_video;
				continue;
			case sdp_media_image:
				smh->rejected_streams[smh->rej_idx++] = sdp_media_image;
				continue;
			default:
				break;
			}
		}

		if (m->m_type == sdp_media_audio) {
			saw_audio = 1;
		}

		if (m->m_type == sdp_media_video) {
			saw_video = 1;
		}

		ptime = dptime;
		maxptime = dmaxptime;

		if (m->m_proto == sdp_proto_extended_srtp || m->m_proto == sdp_proto_extended_rtp) {
			got_webrtc++;
			switch_core_session_set_ice(session);
		}

		if (m->m_proto_name && !strcasecmp(m->m_proto_name, "UDP/TLS/RTP/SAVPF")) {
			switch_channel_set_flag(session->channel, CF_AVPF_MOZ);
		}

		if (m->m_proto_name && !strcasecmp(m->m_proto_name, "UDP/RTP/AVPF")) {
			switch_channel_set_flag(session->channel, CF_AVPF_MOZ);
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
		} else if (m->m_proto == sdp_proto_msrp || m->m_proto == sdp_proto_msrps){
			got_msrp++;
		}

		if (got_msrp && m->m_type == sdp_media_message) {
			if (!smh->msrp_session) {
				smh->msrp_session = switch_msrp_session_new(switch_core_session_get_pool(session), switch_core_session_get_uuid(session), m->m_proto == sdp_proto_msrps);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP session created %s\n", smh->msrp_session->call_id);
			}

			switch_assert(smh->msrp_session);

			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%s]=[%s]\n", attr->a_name, attr->a_value);
				if (!strcasecmp(attr->a_name, "path") && attr->a_value) {
					smh->msrp_session->remote_path = switch_core_session_strdup(session, attr->a_value);
					switch_channel_set_variable(session->channel, "sip_msrp_remote_path", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "accept-types") && attr->a_value) {
					smh->msrp_session->remote_accept_types = switch_core_session_strdup(session, attr->a_value);
					switch_channel_set_variable(session->channel, "sip_msrp_remote_accept_types", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "accept-wrapped-types") && attr->a_value) {
					smh->msrp_session->remote_accept_wrapped_types = switch_core_session_strdup(session, attr->a_value);
					switch_channel_set_variable(session->channel, "sip_msrp_remote_accept_wrapped_types", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "setup") && attr->a_value) {
					smh->msrp_session->remote_setup = switch_core_session_strdup(session, attr->a_value);
					switch_channel_set_variable(session->channel, "sip_msrp_remote_setup", attr->a_value);
					if (!strcmp(attr->a_value, "passive")) {
						smh->msrp_session->active = 1;
					}
				} else if (!strcasecmp(attr->a_name, "file-selector") && attr->a_value) {
					char *tmp = switch_mprintf("%s", attr->a_value);
					char *argv[4] = { 0 };
					int argc;
					int i;

					smh->msrp_session->remote_file_selector = switch_core_session_strdup(session, attr->a_value);
					switch_channel_set_variable(session->channel, "sip_msrp_remote_file_selector", attr->a_value);

					argc = switch_separate_string(tmp, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

					for(i = 0; i<argc; i++) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "::::%s\n", switch_str_nil(argv[i]));
						if (zstr(argv[i])) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERRRRRRR\n");
							continue;
						}
						if (!strncasecmp(argv[i], "name:", 5)) {
							char *p = argv[i] + 5;
							int len = strlen(p);

							if (*p == '"') {
								*(p + len - 1) = '\0';
								p++;
							}
							switch_channel_set_variable(session->channel, "sip_msrp_file_name", p);
						} else if (!strncasecmp(argv[i], "type:", 5)) {
							switch_channel_set_variable(session->channel, "sip_msrp_file_type", argv[i] + 5);
						}
						if (!strncasecmp(argv[i], "size:", 5)) {
							switch_channel_set_variable(session->channel, "sip_msrp_file_size", argv[i] + 5);
						}
						if (!strncasecmp(argv[i], "hash:", 5)) {
							switch_channel_set_variable(session->channel, "sip_msrp_file_hash", argv[i] + 5);
						}
					}
					switch_safe_free(tmp);
				} else if (!strcasecmp(attr->a_name, "file-transfer-id") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_msrp_file_transfer_id", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "file-disposition") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_msrp_file_disposition", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "file-date") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_msrp_file_date", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "file-icon") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_msrp_file_icon", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "file-range") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_msrp_file_range", attr->a_value);
				}
			}

			smh->msrp_session->local_accept_types = smh->msrp_session->remote_accept_types;
			smh->msrp_session->local_accept_wrapped_types = smh->msrp_session->remote_accept_types;
			smh->msrp_session->local_setup = smh->msrp_session->remote_setup;

			switch_channel_set_flag(session->channel, CF_HAS_TEXT);
			switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
			switch_channel_set_flag(session->channel, CF_TEXT_LINE_BASED);
			switch_channel_set_flag(session->channel, CF_MSRP);

			if (m->m_proto == sdp_proto_msrps) {
				switch_channel_set_flag(session->channel, CF_MSRPS);
			}

			if (smh->msrp_session->active) {
				const char *ip = switch_msrp_listen_ip();

				smh->msrp_session->local_path = switch_core_session_sprintf(session,
					"msrp%s://%s:%d/%s;tcp",
					smh->msrp_session->secure ? "s" : "",
					ip, smh->msrp_session->local_port, smh->msrp_session->call_id);

				switch_msrp_start_client(smh->msrp_session);
			}

			switch_core_session_start_text_thread(session);
			tmatch = 1;
		}

		if (got_udptl && m->m_type == sdp_media_image) {
			switch_channel_set_flag(session->channel, CF_IMAGE_SDP);

			if (m->m_port) {
				if (switch_channel_test_app_flag_key("T38", session->channel, CF_APP_T38_NEGOTIATED)) {
					fmatch = 1;
					goto done;
				}

				if (switch_channel_var_true(channel, "refuse_t38") || !switch_channel_var_true(channel, "fax_enable_t38")) {
					switch_channel_clear_app_flag_key("T38", session->channel, CF_APP_T38);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s T38 REFUSE on %s\n",
									  switch_channel_get_name(channel),
									  sdp_type == SDP_TYPE_RESPONSE ? "response" : "request");

					restore_pmaps(a_engine);
					fmatch = 0;

					goto t38_done;
				} else {
					switch_t38_options_t *t38_options = switch_core_media_process_udptl(session, sdp, m);
					const char *var = switch_channel_get_variable(channel, "t38_passthru");
					int pass = switch_channel_test_flag(smh->session->channel, CF_T38_PASSTHRU);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s T38 ACCEPT on %s\n",
									  switch_channel_get_name(channel),
									  sdp_type == SDP_TYPE_RESPONSE ? "response" : "request");

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

							match = 0;
							fmatch = 0;
							goto done;
						}

						switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_POSSIBLE);
						switch_channel_set_app_flag_key("T38", other_channel, CF_APP_T38_POSSIBLE);

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

							switch_core_media_check_autoadj(session);
							switch_channel_execute_on(session->channel, "execute_on_audio_change");
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
				if (switch_channel_wait_for_app_flag(channel, CF_APP_T38_POSSIBLE, "T38", SWITCH_TRUE, 2000)) {
					fmatch = 1;
				} else {

					fmatch = 0;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s T38 %s POSSIBLE on %s\n",
								  switch_channel_get_name(channel),
								  fmatch ? "IS" : "IS NOT",
								  sdp_type == SDP_TYPE_RESPONSE ? "response" : "request");


				goto done;
			}
		} else if (m->m_type == sdp_media_audio && m->m_port && got_audio && got_savp) {
			a_engine->reject_avp = 1;
		} else if (m->m_type == sdp_media_audio && m->m_port && !got_audio) {
			sdp_rtpmap_t *map;
			int ice = 0;

			nm_idx = 0;
			m_idx = 0;
			memset(matches, 0, sizeof(matches[0]) * MAX_MATCHES);
			memset(near_matches, 0, sizeof(near_matches[0]) * MAX_MATCHES);

			audio_port = m->m_port;

			if (!sendonly && (m->m_mode == sdp_sendonly || m->m_mode == sdp_inactive)) {
				sendonly = 1;
				if (m->m_mode == sdp_inactive) {
					inactive = 1;
				}
			}

			if (!sendonly && m->m_connections && m->m_connections->c_address && !strcmp(m->m_connections->c_address, "0.0.0.0")) {
				sendonly = 1;
			}


			switch_core_media_set_rmode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, sdp_media_flow(m->m_mode), sdp_type);

			if (sdp_type == SDP_TYPE_REQUEST) {
				switch(a_engine->rmode) {
				case SWITCH_MEDIA_FLOW_RECVONLY:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_MEDIA_FLOW_SENDONLY, sdp_type);
					break;
				case SWITCH_MEDIA_FLOW_SENDONLY:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_MEDIA_FLOW_RECVONLY, sdp_type);
					break;
				case SWITCH_MEDIA_FLOW_INACTIVE:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);
					break;
				default:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_MEDIA_FLOW_SENDRECV, sdp_type);
					break;
				}
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
					switch_channel_set_variable(session->channel, "media_audio_mode", "inactive");
				} else if (!strcasecmp(attr->a_name, "recvonly")) {
					switch_channel_set_variable(session->channel, "media_audio_mode", "sendonly");
					recvonly = 1;

					a_engine->media_timeout = 0;
					a_engine->media_hold_timeout = 0;

					if (switch_rtp_ready(a_engine->rtp_session)) {
						switch_rtp_set_max_missed_packets(a_engine->rtp_session, 0);
						a_engine->max_missed_hold_packets = 0;
						a_engine->max_missed_packets = 0;
						switch_rtp_set_media_timeout(a_engine->rtp_session, a_engine->media_timeout);
					} else {
						switch_channel_set_variable(session->channel, "media_timeout_audio", "0");
						switch_channel_set_variable(session->channel, "media_hold_timeout_audio", "0");
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


			if (sendonly != 1 && recvonly != 1 && inactive != 1) {
				switch_channel_set_variable(session->channel, "media_audio_mode", NULL);
			}

			if (sdp_type == SDP_TYPE_RESPONSE) {
				if (inactive) {
					// When freeswitch had previously sent inactive in sip request. it should remain inactive otherwise smode should be sendrecv
					if (a_engine->smode==SWITCH_MEDIA_FLOW_INACTIVE) {
						switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, sdp_media_flow(sdp_inactive), sdp_type);
					} else {
						switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, sdp_media_flow(sdp_sendrecv), sdp_type);
					}
				} else if (sendonly) {
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, sdp_media_flow(sdp_sendonly), sdp_type);
				} else if (recvonly) {
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, sdp_media_flow(sdp_recvonly), sdp_type);
				}
			}


			if (!(switch_media_handle_test_media_flag(smh, SCMF_DISABLE_HOLD)
				  || ((val = switch_channel_get_variable(session->channel, "rtp_disable_hold"))
					  && switch_true(val)))
				&& !smh->mparams->hold_laps) {
				smh->mparams->hold_laps++;
				switch_core_media_toggle_hold(session, sendonly);
			}


			if (switch_rtp_has_dtls() && dtls_ok(session)) {
				for (attr = m->m_attributes; attr; attr = attr->a_next) {

					if (!strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
						got_crypto = 1;
					}
				}
			}

			skip_rtcp = 0;
			got_rtcp_mux = 0;
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "rtcp-mux")) {
					got_rtcp_mux = 1;
					skip_rtcp = 1;
					if (!smh->mparams->rtcp_video_interval_msec) {
						smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;
					}
				} else if (!strcasecmp(attr->a_name, "ice-ufrag")) {
					skip_rtcp = 1;
				}
			}

			if (!got_rtcp_mux) {
				a_engine->rtcp_mux = -1;
			}

			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value && !skip_rtcp) {
					a_engine->remote_rtcp_port = (switch_port_t)atoi(attr->a_value);
					switch_channel_set_variable_printf(session->channel, "rtp_remote_audio_rtcp_port", "%d", a_engine->remote_rtcp_port);

					if (!smh->mparams->rtcp_audio_interval_msec) {
						smh->mparams->rtcp_audio_interval_msec = SWITCH_RTCP_AUDIO_INTERVAL_MSEC;
					}
					got_audio_rtcp = 1;
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
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Received invite with SAVP but secure media is administratively disabled\n");
				match = 0;
				continue;
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
						best_te_rate = map->rm_rate;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set telephone-event payload to %u@%ld\n", best_te, best_te_rate);
					}
					continue;
				}

				if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (a_engine->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(a_engine->rtp_session, cng_pt);
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
					char fmtp[SWITCH_MAX_CODECS][MAX_FMTP_LEN];
					int num;
					const switch_codec_implementation_t *timp = NULL;

					near_rate = near_matches[j].rate;
					near_match = near_matches[j].imp;
					near_map = near_matches[j].map;

					switch_snprintf(tmp, sizeof(tmp), "%s@%uh@%ui%dc", near_match->iananame, near_rate ? near_rate : near_match->samples_per_second,
									codec_ms, near_match->number_of_channels);

					prefs[0] = tmp;
					num = switch_loadable_module_get_codecs_sorted(search, fmtp, 1, prefs, 1);

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

				for(j = 0; j < m_idx && smh->num_negotiated_codecs < SWITCH_MAX_CODECS; j++) {
					payload_map_t *pmap = switch_core_media_add_payload_map(session,
																			SWITCH_MEDIA_TYPE_AUDIO,
																			matches[j].map->rm_encoding,
																			matches[j].imp->modname,
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



				switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->cur_payload_map->recv_pt);
				switch_channel_set_variable(session->channel, "rtp_audio_recv_pt", tmp);

				if (a_engine->read_impl.iananame) {
					if (!switch_core_codec_ready(&a_engine->read_codec) ||
						((strcasecmp(matches[0].imp->iananame, a_engine->read_impl.iananame) ||
						  matches[0].imp->microseconds_per_packet != a_engine->read_impl.microseconds_per_packet ||
						  matches[0].imp->samples_per_second != a_engine->read_impl.samples_per_second
						  ))) {

						a_engine->reset_codec = 1;
					}
				} else if (switch_core_media_set_codec(session, 0, smh->mparams->codec_flags) != SWITCH_STATUS_SUCCESS) {
					match = 0;
				}

				if (match) {
					if (check_ice(smh, SWITCH_MEDIA_TYPE_AUDIO, sdp, m) == SWITCH_STATUS_FALSE) {
						match = 0;
						got_audio = 0;
					} else {
						got_audio = 1;
					}
				}

			}

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				const char *rm_encoding;

				if (!(rm_encoding = map->rm_encoding)) {
					rm_encoding = "";
				}

				if (!strcasecmp(rm_encoding, "telephone-event")) {
					if (!best_te || map->rm_rate == a_engine->cur_payload_map->adv_rm_rate) {
						best_te = (switch_payload_t) map->rm_pt;
						best_te_rate = map->rm_rate;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set telephone-event payload to %u@%lu\n", best_te, best_te_rate);
					}
					continue;
				}

				if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && !strcasecmp(rm_encoding, "CN")) {

					if (!cng_pt || map->rm_rate == a_engine->cur_payload_map->adv_rm_rate) {
						cng_pt = (switch_payload_t) map->rm_pt;
						cng_rate = map->rm_rate;

						if (a_engine->rtp_session) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u@%lu\n", cng_pt, cng_rate);
							switch_rtp_set_cng_pt(a_engine->rtp_session, cng_pt);
						}
					}
					continue;
				}
			}

			if (cng_rate != a_engine->cur_payload_map->adv_rm_rate) {
				cng_rate = 8000;
			}

			if (best_te_rate != a_engine->cur_payload_map->adv_rm_rate) {
				best_te_rate = 8000;
			}

			if (!best_te && (switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "No 2833 in SDP. Liberal DTMF mode adding %d as telephone-event.\n", smh->mparams->te);
				best_te = smh->mparams->te;
			}

			if (best_te) {
				smh->mparams->te_rate = best_te_rate;

				if (smh->mparams->dtmf_type == DTMF_AUTO || smh->mparams->dtmf_type == DTMF_2833 ||
					switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF)) {
					if (sdp_type == SDP_TYPE_REQUEST) {
						smh->mparams->te = smh->mparams->recv_te = (switch_payload_t) best_te;
						switch_channel_set_variable(session->channel, "dtmf_type", "rfc2833");
						smh->mparams->dtmf_type = DTMF_2833;
					} else {
						smh->mparams->te = (switch_payload_t) best_te;
						switch_channel_set_variable(session->channel, "dtmf_type", "rfc2833");
						smh->mparams->dtmf_type = DTMF_2833;
					}
				}

				if (a_engine->rtp_session) {
					switch_rtp_set_telephony_event(a_engine->rtp_session, smh->mparams->te);
					switch_channel_set_variable_printf(session->channel, "rtp_2833_send_payload", "%d", smh->mparams->te);
					switch_rtp_set_telephony_recv_event(a_engine->rtp_session, smh->mparams->recv_te);
					switch_channel_set_variable_printf(session->channel, "rtp_2833_recv_payload", "%d", smh->mparams->recv_te);
				}


				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Set 2833 dtmf send payload to %u recv payload to %u\n",
								  switch_channel_get_name(session->channel), smh->mparams->te, smh->mparams->recv_te);


			} else {
				/* by default, use SIP INFO if 2833 is not in the SDP */
				if (!switch_false(switch_channel_get_variable(channel, "rtp_info_when_no_2833"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No 2833 in SDP.  Disable 2833 dtmf and switch to INFO\n");
					switch_channel_set_variable(session->channel, "dtmf_type", "info");
					smh->mparams->dtmf_type = DTMF_INFO;
					smh->mparams->recv_te = smh->mparams->te = 0;
				} else {
					switch_channel_set_variable(session->channel, "dtmf_type", "none");
					smh->mparams->dtmf_type = DTMF_NONE;
					smh->mparams->recv_te = smh->mparams->te = 0;
				}
			}

		} else if (!got_text && m->m_type == sdp_media_text && m->m_port) {
			sdp_rtpmap_t *map;
			payload_map_t *red_pmap = NULL;

			switch_channel_set_flag(session->channel, CF_RTT);

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

			switch_channel_set_variable(session->channel, "text_possible", "true");
			switch_channel_set_flag(session->channel, CF_TEXT_SDP_RECVD);
			switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);

			got_text++;

			for (map = m->m_rtpmaps; map; map = map->rm_next) {
				payload_map_t *pmap;

				pmap = switch_core_media_add_payload_map(session,
														 SWITCH_MEDIA_TYPE_TEXT,
														 map->rm_encoding,
														 NULL,
														 NULL,
														 SDP_TYPE_REQUEST,
														 map->rm_pt,
														 1000,
														 0,
														 1,
														 SWITCH_TRUE);


				pmap->remote_sdp_ip = switch_core_session_strdup(session, (char *) connection->c_address);
				pmap->remote_sdp_port = (switch_port_t) m->m_port;
				if (map->rm_fmtp) {
					pmap->rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
				}


				t_engine->cur_payload_map = pmap;

				if (!strcasecmp(map->rm_encoding, "red")) {
					red_pmap = pmap;
				}
			}

			if (red_pmap) {
				t_engine->cur_payload_map = red_pmap;
			}

			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value) {
					switch_channel_set_variable(session->channel, "sip_remote_text_rtcp_port", attr->a_value);

				} else if (!got_text_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
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

					got_text_crypto = switch_core_session_check_incoming_crypto(session,
																				"rtp_has_text_crypto",
																				SWITCH_MEDIA_TYPE_TEXT, crypto, crypto_tag, sdp_type);

				}
			}


			//map->rm_encoding
			//map->rm_fmtp
			//map->rm_pt
			//t_engine->cur_payload_map = pmap;

			t_engine->codec_negotiated = 1;
			tmatch = 1;


			if (!t_engine->local_sdp_port) {
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_TEXT, 1);
			}

			check_ice(smh, SWITCH_MEDIA_TYPE_TEXT, sdp, m);
			//parse rtt

		} else if (m->m_type == sdp_media_video) {
			sdp_rtpmap_t *map;
			const char *rm_encoding;
			const switch_codec_implementation_t *mimp = NULL;
			int i;
			const char *inherit_video_fmtp = NULL;
			
			switch_core_media_set_rmode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, sdp_media_flow(m->m_mode), sdp_type);
			
			if (sdp_type == SDP_TYPE_REQUEST) {
				sdp_bandwidth_t *bw;
				int tias = 0;

				for (bw = m->m_bandwidths; bw; bw = bw->b_next) {
					if (bw->b_modifier == sdp_bw_as && !tias) {
						v_engine->sdp_bw = bw->b_value;
					} else if (bw->b_modifier == sdp_bw_tias) {
						tias = 1;
						v_engine->sdp_bw = bw->b_value / 1024;
					}
				}

				switch(v_engine->rmode) {
				case SWITCH_MEDIA_FLOW_RECVONLY:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_SENDONLY, sdp_type);
					switch_channel_set_flag(smh->session->channel, CF_VIDEO_READY);
					break;
				case SWITCH_MEDIA_FLOW_SENDONLY:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_RECVONLY, sdp_type);
					break;
				case SWITCH_MEDIA_FLOW_INACTIVE:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);
					break;
				default:
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_SENDRECV, sdp_type);
					break;
				}
			}

			if (!m->m_port) {
				goto endsdp;
			}

			vmatch = 0;
			almost_vmatch = 0;
			m_idx = 0;
			memset(matches, 0, sizeof(matches[0]) * MAX_MATCHES);
			memset(near_matches, 0, sizeof(near_matches[0]) * MAX_MATCHES);

			switch_channel_set_variable(session->channel, "video_possible", "true");
			switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
			switch_channel_set_flag(session->channel, CF_VIDEO_SDP_RECVD);

			connection = sdp->sdp_connection;
			if (m->m_connections) {
				connection = m->m_connections;
			}

			if (!connection) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot find a c= line in the sdp at media or session level!\n");
				match = 0;
				break;
			}

			

			skip_video_rtcp = 0;
			got_video_rtcp_mux = 0;
			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "rtcp-mux")) {
					got_video_rtcp_mux = 1;
					skip_video_rtcp = 1;
				} else if (!strcasecmp(attr->a_name, "ice-ufrag")) {
					skip_video_rtcp = 1;
				}
			}

			if (!got_video_rtcp_mux) {
				v_engine->rtcp_mux = -1;
			}

			for (attr = m->m_attributes; attr; attr = attr->a_next) {
				if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
					//framerate = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "rtcp-fb")) {
					if (!zstr(attr->a_value)) {
						if (switch_stristr("fir", attr->a_value)) {
							v_engine->fir++;
						}

						if (switch_stristr("pli", attr->a_value)) {
							v_engine->pli++;
						}

						if (switch_stristr("nack", attr->a_value)) {
							v_engine->nack++;
						}

						if (switch_stristr("tmmbr", attr->a_value)) {
							v_engine->tmmbr++;
						}

						rtcp_auto_video = 1;
						smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;
					}
				} else if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value && !skip_video_rtcp) {
					v_engine->remote_rtcp_port = (switch_port_t)atoi(attr->a_value);
					switch_channel_set_variable_printf(session->channel, "rtp_remote_video_rtcp_port", "%d", v_engine->remote_rtcp_port);
					if (!smh->mparams->rtcp_video_interval_msec) {
						smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;
					}
					got_video_rtcp = 1;
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


			if (switch_true(switch_channel_get_variable_dup(session->channel, "inherit_codec", SWITCH_FALSE, -1))) {
				vmatch_pt = 1;
			}

		compare:

			for (map = m->m_rtpmaps; map; map = map->rm_next) {

				if (switch_rtp_has_dtls() && dtls_ok(session)) {
					for (attr = m->m_attributes; attr; attr = attr->a_next) {
						if (!strcasecmp(attr->a_name, "fingerprint") && !zstr(attr->a_value)) {
							got_video_crypto = 1;
						}
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

					if (sdp_type == SDP_TYPE_RESPONSE && consider_video_fmtp && vmatch && !zstr(map->rm_fmtp) && !zstr(smh->fmtps[i])) {
						almost_vmatch = 1;
						vmatch = !strcasecmp(smh->fmtps[i], map->rm_fmtp);
					}

					if (vmatch && vmatch_pt) {
						const char *other_pt = switch_channel_get_variable_partner(channel, "rtp_video_pt");

						if (other_pt) {
							int opt = atoi(other_pt);
							if (map->rm_pt != opt) {
								vmatch = 0;
							} else {
								if (switch_channel_var_true(channel, "inherit_video_fmtp")) {
									inherit_video_fmtp = switch_channel_get_variable_partner(channel, "rtp_video_fmtp");
								}
							}
						}
					}

					if (vmatch) {
						matches[m_idx].imp = imp;
						matches[m_idx].map = map;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Video Codec Compare [%s:%d] +++ is saved as a match\n",
										  imp->iananame, map->rm_pt);

						m_idx++;
					}

					vmatch = 0;
				}
			}

			if (consider_video_fmtp && (!m_idx || almost_vmatch)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No matches with FTMP, fallback to ignoring FMTP\n");
				almost_vmatch = 0;
				m_idx = 0;
				consider_video_fmtp = 0;
				goto compare;
			}

			if (vmatch_pt && !m_idx) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No matches with inherit_codec, fallback to ignoring PT\n");
				vmatch_pt = 0;
				goto compare;
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
				v_engine->payload_map = NULL;

				for(j = 0; j < m_idx && smh->num_negotiated_codecs < SWITCH_MAX_CODECS; j++) {
					payload_map_t *pmap = switch_core_media_add_payload_map(session,
																			SWITCH_MEDIA_TYPE_VIDEO,
																			matches[j].map->rm_encoding,
																			matches[j].imp->modname,
																			consider_video_fmtp ? matches[j].map->rm_fmtp : NULL,
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
					pmap->rm_fmtp = switch_core_session_strdup(session, (char *) inherit_video_fmtp ? inherit_video_fmtp : map->rm_fmtp);
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
				switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->cur_payload_map->pt);
				switch_channel_set_variable(session->channel, "rtp_video_pt", tmp);
				switch_core_media_check_video_codecs(session);
				switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->cur_payload_map->recv_pt);
				switch_channel_set_variable(session->channel, "rtp_video_recv_pt", tmp);

				if (switch_core_codec_ready(&v_engine->read_codec) && strcasecmp(matches[0].imp->iananame, v_engine->read_codec.implementation->iananame)) {
					v_engine->reset_codec = 1;
				}

				if (switch_core_media_set_video_codec(session, 0) == SWITCH_STATUS_SUCCESS) {
					if (check_ice(smh, SWITCH_MEDIA_TYPE_VIDEO, sdp, m) == SWITCH_STATUS_FALSE) {
						vmatch = 0;
					}
				}
			}

			video_port = m->m_port;
		}
	}

 endsdp:

	if (rtcp_auto_audio || rtcp_auto_video) {
		if (rtcp_auto_audio && !skip_rtcp && !got_audio_rtcp && audio_port) {
			switch_channel_set_variable_printf(session->channel, "rtp_remote_audio_rtcp_port", "%d", audio_port + 1);
			a_engine->remote_rtcp_port = audio_port + 1;

			if (!smh->mparams->rtcp_audio_interval_msec) {
				smh->mparams->rtcp_audio_interval_msec = SWITCH_RTCP_AUDIO_INTERVAL_MSEC;
			}
		}
		if (rtcp_auto_video && !skip_video_rtcp && !got_video_rtcp && video_port) {
			switch_channel_set_variable_printf(session->channel, "rtp_remote_video_rtcp_port", "%d", video_port + 1);
			v_engine->remote_rtcp_port = video_port + 1;
			if (!smh->mparams->rtcp_video_interval_msec) {
				smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;
			}
		}
	}


	if (!saw_audio) {
		payload_map_t *pmap;

		a_engine->rmode = SWITCH_MEDIA_FLOW_DISABLED;
		switch_channel_set_variable(smh->session->channel, "audio_media_flow", "inactive");


		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "L16",
												 NULL,
												 NULL,
												 SDP_TYPE_REQUEST,
												 97,
												 8000,
												 20,
												 1,
												 SWITCH_TRUE);

		pmap->remote_sdp_ip = "127.0.0.1";
		pmap->remote_sdp_port = 9999;
		pmap->pt = 97;
		pmap->recv_pt = 97;
		pmap->codec_ms = 20;
		a_engine->cur_payload_map = pmap;
		switch_channel_set_flag(channel, CF_AUDIO_PAUSE_READ);
		switch_channel_set_flag(channel, CF_AUDIO_PAUSE_WRITE);
	}

 done:

	if (v_engine->rtp_session) {
		if (v_engine->fir) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_FIR);
		} else {
			switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_FIR);
		}
		
		if (v_engine->pli) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PLI);
		} else {
			switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PLI);
		}
		
		if (v_engine->nack) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_NACK);
		} else {
			switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_NACK);
		}
		
		if (v_engine->tmmbr) {
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_TMMBR);
		} else {
			switch_rtp_clear_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_TMMBR);
		}
	}

	if (match) {
		switch_channel_set_flag(channel, CF_AUDIO);
	} else {
		switch_channel_clear_flag(channel, CF_AUDIO);
	}

	if (vmatch) {
		switch_channel_set_flag(channel, CF_VIDEO);
	} else {
		if (switch_channel_test_flag(channel, CF_VIDEO) && !saw_video) {
			//switch_core_media_set_rmode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);

			if (sdp_type == SDP_TYPE_REQUEST) {
				switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_INACTIVE, sdp_type);
			}
		}
	}

	if (tmatch) {
		switch_channel_set_flag(channel, CF_HAS_TEXT);
	} else {
		switch_channel_clear_flag(channel, CF_HAS_TEXT);
	}

	if (fmatch) {
		switch_channel_set_flag(channel, CF_IMAGE_SDP);
		switch_channel_set_flag(channel, CF_AUDIO);
	} else {
		switch_channel_clear_flag(channel, CF_IMAGE_SDP);
	}

 t38_done:

	if (parser) {
		sdp_parser_free(parser);
	}

	smh->mparams->cng_pt = cng_pt;
	smh->mparams->cng_rate = cng_rate;

	check_stream_changes(session, r_sdp, sdp_type);

	return match || vmatch || tmatch || fmatch;
}

//?
SWITCH_DECLARE(void) switch_core_media_reset_t38(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine;
	switch_media_handle_t *smh;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	restore_pmaps(a_engine);

	switch_channel_set_private(channel, "t38_options", NULL);
	switch_channel_clear_flag(channel, CF_T38_PASSTHRU);
	switch_channel_clear_app_flag_key("T38", channel, CF_APP_T38);
	switch_channel_clear_app_flag_key("T38", channel, CF_APP_T38_REQ);
	switch_channel_set_app_flag_key("T38", channel, CF_APP_T38_FAIL);
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

			if (a_engine->media_hold_timeout) {
				switch_rtp_set_media_timeout(a_engine->rtp_session, a_engine->media_hold_timeout);
			}

			if (v_engine->media_hold_timeout) {
				switch_rtp_set_media_timeout(v_engine->rtp_session, v_engine->media_hold_timeout);
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
				switch_ivr_bg_media(switch_core_session_get_uuid(session), SMF_REBRIDGE, SWITCH_FALSE, SWITCH_TRUE, 200);
			}

			if (a_engine->rtp_session) {
				switch_rtp_reset_media_timer(a_engine->rtp_session);

				if (a_engine->max_missed_packets) {
					switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_packets);
				}

				if (a_engine->media_hold_timeout) {
					switch_rtp_set_media_timeout(a_engine->rtp_session, a_engine->media_timeout);
				}
			}

			if (v_engine->rtp_session) {
				switch_rtp_reset_media_timer(v_engine->rtp_session);

				if (v_engine->media_hold_timeout) {
					switch_rtp_set_media_timeout(v_engine->rtp_session, v_engine->media_timeout);
				}
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
	switch_core_session_request_video_refresh(session);


	if (b_session) {
		switch_core_session_request_video_refresh(b_session);
		switch_core_session_rwunlock(b_session);
	}


	return changed;
}


SWITCH_DECLARE(switch_file_handle_t *) switch_core_media_get_video_file(switch_core_session_t *session, switch_rw_t rw)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;
	switch_file_handle_t *fh;

	switch_assert(session);

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return NULL;
	}

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];



	if (rw == SWITCH_RW_READ) {
		switch_mutex_lock(v_engine->mh.file_read_mutex);
		fh = smh->video_read_fh;
		switch_mutex_unlock(v_engine->mh.file_read_mutex);
	} else {
		switch_mutex_lock(v_engine->mh.file_write_mutex);
		fh = smh->video_write_fh;
		switch_mutex_unlock(v_engine->mh.file_write_mutex);
	}



	return fh;
}

SWITCH_DECLARE(void) switch_core_session_write_blank_video(switch_core_session_t *session, uint32_t ms)
{
	switch_frame_t fr = { 0 };
	int i = 0;
	switch_rgb_color_t bgcolor = { 0 };
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	unsigned char buf[SWITCH_RTP_MAX_BUF_LEN];
	switch_media_handle_t *smh;
	switch_image_t *blank_img = NULL;
	uint32_t frames = 0, frame_ms = 0;
	uint32_t fps, width, height;
	switch_assert(session != NULL);

	if (!(smh = session->media_handle)) {
		return;
	}

	fps = smh->vid_params.fps;
	width = smh->vid_params.width;
	height = smh->vid_params.height;

	if (!width) width = 352;
	if (!height) height = 288;
	if (!fps) fps = 15;

	if (!(width && height && fps)) {
		return;
	}

	fr.packet = buf;
	fr.packetlen = buflen;
	fr.data = buf + 12;
	fr.buflen = buflen - 12;


	blank_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
	switch_color_set_rgb(&bgcolor, "#000000");
	switch_img_fill(blank_img, 0, 0, blank_img->d_w, blank_img->d_h, &bgcolor);

	if (fps < 15) fps = 15;
	frame_ms = (uint32_t) 1000 / fps;
	if (frame_ms <= 0) frame_ms = 66;
	frames = (uint32_t) ms / frame_ms;

	switch_core_media_gen_key_frame(session);
	for (i = 0; i < frames; i++) {
		fr.img = blank_img;
		switch_core_session_write_video_frame(session, &fr, SWITCH_IO_FLAG_NONE, 0);
		switch_yield(frame_ms * 1000);
	}
	switch_core_media_gen_key_frame(session);

	switch_img_free(&blank_img);

}

static void *SWITCH_THREAD_FUNC video_write_thread(switch_thread_t *thread, void *obj)
{
	switch_core_session_t *session = (switch_core_session_t *) obj;
	switch_media_handle_t *smh;
	unsigned char *buf = NULL;
	switch_frame_t fr = { 0 };
	switch_rtp_engine_t *v_engine;
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	switch_timer_t timer = { 0 };
	switch_video_read_flag_t read_flags = SVR_BLOCK;
	switch_core_session_t *b_session = NULL;
	switch_fps_t fps_data = { 0 };
	float fps;
	switch_image_t *last_frame = NULL;
	
	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	switch_core_session_get_partner(session, &b_session);

	switch_channel_set_flag(session->channel, CF_VIDEO_WRITING);

	if (b_session) {
		switch_channel_set_flag(b_session->channel, CF_VIDEO_BLANK);
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	switch_mutex_lock(smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO]);
	v_engine->thread_write_lock = switch_thread_self();

	
	buf = switch_core_session_alloc(session, buflen);
	fr.packet = buf;
	fr.packetlen = buflen;
	fr.data = buf + 12;
	fr.buflen = buflen - 12;
	switch_core_media_gen_key_frame(session);


	if (smh->video_write_fh && smh->video_write_fh->mm.source_fps) {
		fps = smh->video_write_fh->mm.source_fps;
	} else if (video_globals.fps) {
		fps = video_globals.fps;
	} else {
		fps = 15;
	}
	switch_calc_video_fps(&fps_data, fps);

	switch_core_timer_init(&timer, "soft", fps_data.ms, fps_data.samples, switch_core_session_get_pool(session));

	while (smh->video_write_thread_running > 0 &&
		   switch_channel_up_nosig(session->channel) && smh->video_write_fh && switch_test_flag(smh->video_write_fh, SWITCH_FILE_OPEN)) {
		switch_status_t wstatus = SWITCH_STATUS_FALSE;

		switch_core_timer_next(&timer);
		switch_mutex_lock(v_engine->mh.file_write_mutex);

		//if (smh->video_write_fh && smh->video_write_fh->mm.source_fps && smh->video_write_fh->mm.source_fps != fps) {
		//	switch_core_timer_destroy(&timer);
		//	switch_calc_video_fps(&fps_data, fps);
		//	switch_core_timer_init(&timer, "soft", fps_data.ms, fps_data.samples, switch_core_session_get_pool(session));
		//}

		if (smh->video_write_fh && !switch_test_flag(smh->video_write_fh, SWITCH_FILE_FLAG_VIDEO_EOF)) {
			wstatus = switch_core_file_read_video(smh->video_write_fh, &fr, read_flags);

			if (wstatus == SWITCH_STATUS_SUCCESS) {
				fr.timestamp = timer.samplecount;
				fr.flags = SFF_USE_VIDEO_TIMESTAMP|SFF_RAW_RTP|SFF_RAW_RTP_PARSE_FRAME;
				
				if (smh->vid_params.d_width && smh->vid_params.d_height) {
					switch_img_fit(&fr.img, smh->vid_params.d_width, smh->vid_params.d_height, SWITCH_FIT_SIZE);
				}
				
				switch_core_session_write_video_frame(session, &fr, SWITCH_IO_FLAG_FORCE, 0);

				switch_img_free(&last_frame);
				last_frame = fr.img;
				fr.img = NULL;
				
			} else if (wstatus != SWITCH_STATUS_BREAK && wstatus != SWITCH_STATUS_IGNORE) {
				switch_set_flag_locked(smh->video_write_fh, SWITCH_FILE_FLAG_VIDEO_EOF);
			}
		}
		switch_mutex_unlock(v_engine->mh.file_write_mutex);
	}

	if (last_frame) {
		int x = 0;
		switch_rgb_color_t bgcolor;
		switch_color_set_rgb(&bgcolor, "#000000");
		switch_img_fill(last_frame, 0, 0, last_frame->d_w, last_frame->d_h, &bgcolor);
		fr.img = last_frame;

		for (x = 0; x < (int)(fps_data.fps / 2); x++) {
			switch_core_timer_next(&timer);
			fr.timestamp = timer.samplecount;
			fr.flags = SFF_USE_VIDEO_TIMESTAMP|SFF_RAW_RTP|SFF_RAW_RTP_PARSE_FRAME;
			switch_core_session_write_video_frame(session, &fr, SWITCH_IO_FLAG_FORCE, 0);
		}
		switch_core_media_gen_key_frame(session);
		switch_core_session_request_video_refresh(session);
		switch_img_free(&last_frame);
	}


	switch_core_timer_destroy(&timer);

	switch_core_session_rwunlock(session);

	if (b_session) {
		switch_channel_clear_flag(b_session->channel, CF_VIDEO_BLANK);
		switch_core_session_rwunlock(b_session);
	}

	
	v_engine->thread_write_lock = 0;
	switch_mutex_unlock(smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO]);

	switch_channel_clear_flag(session->channel, CF_VIDEO_WRITING);
	smh->video_write_thread_running = 0;

	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_lock_video_file(switch_core_session_t *session, switch_rw_t rw)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;

	switch_assert(session);

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (rw == SWITCH_RW_READ) {
		switch_mutex_lock(v_engine->mh.file_read_mutex);
	} else {
		switch_mutex_lock(v_engine->mh.file_write_mutex);
	}

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_core_media_unlock_video_file(switch_core_session_t *session, switch_rw_t rw)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;

	switch_assert(session);

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (rw == SWITCH_RW_READ) {
		switch_mutex_unlock(v_engine->mh.file_read_mutex);
	} else {
		switch_mutex_unlock(v_engine->mh.file_write_mutex);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_set_video_file(switch_core_session_t *session, switch_file_handle_t *fh, switch_rw_t rw)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!smh->video_read_fh && !smh->video_write_fh && !switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return SWITCH_STATUS_FALSE;
	}

	if (fh && !switch_core_file_has_video(fh, SWITCH_TRUE)) {
		return SWITCH_STATUS_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	switch_core_session_start_video_thread(session);

	//if (!v_engine->media_thread) {
	//	return SWITCH_STATUS_FALSE;
	//}



	if (rw == SWITCH_RW_READ) {
		switch_mutex_lock(v_engine->mh.file_read_mutex);

		if (fh && smh->video_read_fh) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "File is already open\n");
			switch_mutex_unlock(v_engine->mh.file_read_mutex);
			return SWITCH_STATUS_FALSE;
		}


		if (fh) {
			switch_channel_set_flag_recursive(session->channel, CF_VIDEO_DECODED_READ);
			switch_channel_set_flag(session->channel, CF_VIDEO_READ_FILE_ATTACHED);
		} else if (smh->video_read_fh) {
			switch_channel_clear_flag_recursive(session->channel, CF_VIDEO_DECODED_READ);
			switch_core_session_video_reset(session);
		}

		if (!fh) {
			switch_channel_clear_flag(session->channel, CF_VIDEO_READ_FILE_ATTACHED);
		}

		smh->video_read_fh = fh;

		switch_mutex_unlock(v_engine->mh.file_read_mutex);

	} else {
		if (!fh && smh->video_write_thread) {
			if (smh->video_write_thread_running > 0) {
				smh->video_write_thread_running = -1;
			}
		}

		switch_mutex_lock(v_engine->mh.file_write_mutex);

		if (fh && smh->video_write_fh) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "File is already open\n");
			smh->video_write_fh = fh;
			switch_mutex_unlock(v_engine->mh.file_write_mutex);
			return SWITCH_STATUS_SUCCESS;
		}

		if (fh) {
			switch_channel_set_flag(session->channel, CF_VIDEO_WRITE_FILE_ATTACHED);
		} else {
			switch_channel_clear_flag(session->channel, CF_VIDEO_WRITE_FILE_ATTACHED);
		}

		switch_core_media_gen_key_frame(session);
		switch_core_session_request_video_refresh(session);

		if (fh) {
			switch_threadattr_t *thd_attr = NULL;
			//switch_core_session_write_blank_video(session, 500);
			switch_threadattr_create(&thd_attr, switch_core_session_get_pool(session));
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			smh->video_write_thread_running = 1;
			switch_thread_create(&smh->video_write_thread, thd_attr, video_write_thread, session, switch_core_session_get_pool(session));
		}

		if (!fh && smh->video_write_thread) {
			switch_status_t st;

			if (smh->video_write_thread_running > 0) {
				smh->video_write_thread_running = -1;
			}
			switch_mutex_unlock(v_engine->mh.file_write_mutex);
			switch_thread_join(&st, smh->video_write_thread);
			switch_mutex_lock(v_engine->mh.file_write_mutex);
			smh->video_write_thread = NULL;
			//switch_core_session_write_blank_video(session, 500);
		}

		smh->video_write_fh = fh;

		switch_mutex_unlock(v_engine->mh.file_write_mutex);
	}

	if (!fh) switch_channel_video_sync(session->channel);

	switch_core_session_wake_video_thread(session);


	return SWITCH_STATUS_SUCCESS;
}

int next_cpu(void)
{
	int x = 0;

	switch_mutex_lock(video_globals.mutex);
	x = video_globals.cur_cpu++;
	if (video_globals.cur_cpu == video_globals.cpu_count) {
		video_globals.cur_cpu = 0;
	}
	switch_mutex_unlock(video_globals.mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Binding to CPU %d\n", x);

	return x;
}

SWITCH_DECLARE(void) switch_core_autobind_cpu(void)
{
	if (video_globals.cpu_count > 1) {
		switch_core_thread_set_cpu_affinity(next_cpu());
	}
}

static switch_status_t perform_write(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_io_event_hook_write_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_media_handle_t *smh;

	switch_assert(session != NULL);

	if ((smh = session->media_handle)) {
		switch_rtp_engine_t *a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

		if (a_engine && a_engine->write_fb && !(flags & SWITCH_IO_FLAG_QUEUED)) {
			switch_frame_t *dupframe = NULL;

			if (switch_frame_buffer_dup(a_engine->write_fb, frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
				switch_frame_buffer_push(a_engine->write_fb, dupframe);
				dupframe = NULL;
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (session->bugs && !(frame->flags & SFF_NOT_AUDIO)) {
		switch_media_bug_t *bp;
		switch_bool_t ok = SWITCH_TRUE;
		int prune = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);

		for (bp = session->bugs; bp; bp = bp->next) {
			ok = SWITCH_TRUE;

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}
			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (bp->ready) {
				if (switch_test_flag(bp, SMBF_TAP_NATIVE_WRITE)) {
					if (bp->callback) {
						bp->native_write_frame = frame;
						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_TAP_NATIVE_WRITE);
						bp->native_write_frame = NULL;
					}
				}
			}

			if ((bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) || ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (prune) {
			switch_core_media_bug_prune(session);
		}
	}


	if (session->endpoint_interface->io_routines->write_frame) {
		if ((status = session->endpoint_interface->io_routines->write_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->write_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}


static void *SWITCH_THREAD_FUNC audio_write_thread(switch_thread_t *thread, void *obj)
{
	struct media_helper *mh = obj;
	switch_core_session_t *session = mh->session;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine = NULL;
	switch_codec_implementation_t write_impl;
	switch_timer_t timer = {0};

	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	if (!(smh = session->media_handle)) {
		switch_core_session_rwunlock(session);
		return NULL;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	a_engine->thread_id = switch_thread_self();


	write_impl = session->write_impl;

	switch_core_timer_init(&timer, "soft", write_impl.microseconds_per_packet / 1000,
						   write_impl.samples_per_packet, switch_core_session_get_pool(session));

	mh->up = 1;

	switch_frame_buffer_create(&a_engine->write_fb, 500);

	while(switch_channel_up_nosig(session->channel) && mh->up == 1) {
		void *pop;

		if (session->write_impl.microseconds_per_packet != write_impl.microseconds_per_packet ||
			session->write_impl.samples_per_packet != write_impl.samples_per_packet) {


			write_impl = session->write_impl;
			switch_core_timer_destroy(&timer);
			switch_core_timer_init(&timer, "soft", write_impl.microseconds_per_packet / 1000,
								   write_impl.samples_per_packet, switch_core_session_get_pool(session));

		}

		switch_core_timer_next(&timer);

		if (switch_frame_buffer_trypop(a_engine->write_fb, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			switch_frame_t *frame = (switch_frame_t *)pop;

			if ((switch_size_t)pop == 1) {
				break;
			}

			perform_write(session, frame, SWITCH_IO_FLAG_QUEUED, 0);
			switch_frame_buffer_free(a_engine->write_fb, &frame);
		}
	}

	switch_mutex_lock(smh->control_mutex);
	mh->up = 0;
	switch_mutex_unlock(smh->control_mutex);

	switch_core_timer_destroy(&timer);

	switch_core_session_rwunlock(session);
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_start_audio_write_thread(switch_core_session_t *session)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_rtp_engine_t *a_engine = NULL;
	switch_media_handle_t *smh;

	if (!switch_channel_test_flag(session->channel, CF_AUDIO)) {
		return SWITCH_STATUS_NOTIMPL;
	}

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if (a_engine->media_thread) {
		return SWITCH_STATUS_INUSE;
	}

	switch_mutex_lock(smh->control_mutex);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Starting Audio write thread\n", switch_core_session_get_name(session));

	a_engine->mh.session = session;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	switch_thread_cond_create(&a_engine->mh.cond, pool);
	switch_mutex_init(&a_engine->mh.cond_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_thread_create(&a_engine->media_thread, thd_attr, audio_write_thread, &a_engine->mh, switch_core_session_get_pool(session));

	switch_mutex_unlock(smh->control_mutex);
	return SWITCH_STATUS_SUCCESS;
}



static void *SWITCH_THREAD_FUNC text_helper_thread(switch_thread_t *thread, void *obj)
{
	struct media_helper *mh = obj;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_status_t status;
	switch_frame_t *read_frame = NULL;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *t_engine = NULL;
	unsigned char CR[] = TEXT_UNICODE_LINEFEED;
	switch_frame_t cr_frame = { 0 };


	session = mh->session;
	
	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		mh->ready = -1;
		return NULL;
	}

	mh->ready = 1;

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	channel = switch_core_session_get_channel(session);

	if (switch_channel_var_true(session->channel, "fire_text_events")) {
		switch_channel_set_flag(session->channel, CF_FIRE_TEXT_EVENTS);
	}

	cr_frame.data = CR;
	cr_frame.datalen = 3;

	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];
	t_engine->thread_id = switch_thread_self();

	mh->up = 1;

	switch_core_media_check_dtls(session, SWITCH_MEDIA_TYPE_TEXT);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Text thread started.\n", switch_channel_get_name(session->channel));

	if (!switch_channel_test_flag(channel, CF_MSRP)) {
		switch_core_session_write_text_frame(session, &cr_frame, 0, 0);
	}

	while (switch_channel_up_nosig(channel)) {

		if (t_engine->engine_function) {
			int run = 0;

			switch_mutex_lock(smh->control_mutex);
			if (t_engine->engine_function_running == 0) {
				t_engine->engine_function_running = 1;
				run = 1;
			}
			switch_mutex_unlock(smh->control_mutex);

			if (run) {
				t_engine->engine_function(session, t_engine->engine_user_data);
				switch_mutex_lock(smh->control_mutex);
				t_engine->engine_function = NULL;
				t_engine->engine_user_data = NULL;
				t_engine->engine_function_running = 0;
				switch_mutex_unlock(smh->control_mutex);
			}
		}

		if (!switch_channel_test_flag(session->channel, CF_TEXT_PASSIVE)) {

			status = switch_core_session_read_text_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

			if (!SWITCH_READ_ACCEPTABLE(status)) {
				switch_cond_next();
				continue;
			}

			if (!switch_test_flag(read_frame, SFF_CNG)) {
				if (switch_channel_test_flag(session->channel, CF_TEXT_ECHO)) {
					switch_core_session_write_text_frame(session, read_frame, 0, 0);
				}
			}
		}

		switch_core_session_write_text_frame(session, NULL, 0, 0);


	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Text thread ended\n", switch_channel_get_name(session->channel));

	switch_core_session_rwunlock(session);

	mh->up = 0;
	return NULL;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_start_text_thread(switch_core_session_t *session)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_rtp_engine_t *t_engine = NULL;
	switch_media_handle_t *smh;

	if (!switch_channel_test_flag(session->channel, CF_HAS_TEXT)) {
		return SWITCH_STATUS_NOTIMPL;
	}

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	switch_mutex_lock(smh->control_mutex);

	if (t_engine->media_thread) {
		switch_mutex_unlock(smh->control_mutex);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Starting Text thread\n", switch_core_session_get_name(session));

	if (t_engine->rtp_session) {
		switch_rtp_set_default_payload(t_engine->rtp_session, t_engine->cur_payload_map->pt);
	}

	t_engine->mh.session = session;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	switch_thread_cond_create(&t_engine->mh.cond, pool);
	switch_mutex_init(&t_engine->mh.cond_mutex, SWITCH_MUTEX_NESTED, pool);
	//switch_mutex_init(&t_engine->mh.file_read_mutex, SWITCH_MUTEX_NESTED, pool);
	//switch_mutex_init(&t_engine->mh.file_write_mutex, SWITCH_MUTEX_NESTED, pool);
	//switch_mutex_init(&smh->read_mutex[SWITCH_MEDIA_TYPE_TEXT], SWITCH_MUTEX_NESTED, pool);
	//switch_mutex_init(&smh->write_mutex[SWITCH_MEDIA_TYPE_TEXT], SWITCH_MUTEX_NESTED, pool);

	t_engine->mh.ready = 0;

	if (switch_thread_create(&t_engine->media_thread, thd_attr, text_helper_thread, &t_engine->mh, 
							 switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		while(!t_engine->mh.ready) {
			switch_cond_next();
		}
	}

	switch_mutex_unlock(smh->control_mutex);
	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC video_helper_thread(switch_thread_t *thread, void *obj)
{
	struct media_helper *mh = obj;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_status_t status;
	switch_frame_t *read_frame = NULL;
	switch_media_handle_t *smh;
	uint32_t loops = 0, xloops = 0, vloops = 0;
	switch_image_t *blank_img = NULL;
	switch_frame_t fr = { 0 };
	unsigned char *buf = NULL;
	switch_rgb_color_t bgcolor;
	switch_rtp_engine_t *v_engine = NULL;
	const char *var;
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int blank_enabled = 1;

	session = mh->session;
	
	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		mh->ready = -1;
		return NULL;
	}

	mh->ready = 1;

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	channel = switch_core_session_get_channel(session);

	switch_core_autobind_cpu();

	if ((var = switch_channel_get_variable(session->channel, "core_video_blank_image"))) {
		if (switch_false(var)) {
			blank_enabled = 0;
		} else {
			blank_img = switch_img_read_png(var, SWITCH_IMG_FMT_I420);
		}
	}

	if (!blank_img) {
		switch_color_set_rgb(&bgcolor, "#000000");
		if ((blank_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 352, 288, 1))) {
			switch_img_fill(blank_img, 0, 0, blank_img->d_w, blank_img->d_h, &bgcolor);
		}
	}



	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	v_engine->thread_id = switch_thread_self();

	mh->up = 1;
	switch_mutex_lock(mh->cond_mutex);

	switch_core_media_check_dtls(session, SWITCH_MEDIA_TYPE_VIDEO);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread started. Echo is %s\n",
					  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
	switch_core_session_request_video_refresh(session);

	buf = switch_core_session_alloc(session, buflen);
	fr.packet = buf;
	fr.packetlen = buflen;
	fr.data = buf + 12;
	fr.buflen = buflen - 12;

	switch_core_media_gen_key_frame(session);

	while (switch_channel_up_nosig(channel)) {
		int send_blank = 0;

		if (!switch_channel_test_flag(channel, CF_VIDEO)) {
			if ((++loops % 100) == 0) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Waiting for video......\n");
			switch_yield(20000);
			continue;
		}

		if (!switch_channel_test_flag(channel, CF_VIDEO_READY) &&
			switch_channel_test_flag(channel, CF_VIDEO_DECODED_READ) && (++xloops > 10 || switch_channel_test_flag(channel, CF_VIDEO_PASSIVE))) {
			switch_channel_set_flag(channel, CF_VIDEO_READY);
		}

		if (switch_channel_test_flag(channel, CF_VIDEO_PASSIVE)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread paused. Echo is %s\n",
							  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
			switch_thread_cond_wait(mh->cond, mh->cond_mutex);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread resumed  Echo is %s\n",
							  switch_channel_get_name(session->channel), switch_channel_test_flag(channel, CF_VIDEO_ECHO) ? "on" : "off");
			switch_core_session_request_video_refresh(session);
		}

		if (switch_channel_test_flag(channel, CF_VIDEO_PASSIVE)) {
			continue;
		}

		if (!switch_channel_media_up(session->channel)) {
			switch_yield(10000);
			continue;
		}

		if (v_engine->engine_function) {
			int run = 0;

			switch_mutex_lock(smh->control_mutex);
			if (v_engine->engine_function_running == 0) {
				v_engine->engine_function_running = 1;
				run = 1;
			}
			switch_mutex_unlock(smh->control_mutex);

			if (run) {
				v_engine->engine_function(session, v_engine->engine_user_data);
				switch_mutex_lock(smh->control_mutex);
				v_engine->engine_function = NULL;
				v_engine->engine_user_data = NULL;
				v_engine->engine_function_running = 0;
				switch_mutex_unlock(smh->control_mutex);
			}
		}

		status = switch_core_session_read_video_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			switch_cond_next();
			continue;
		}

		vloops++;

		send_blank = blank_enabled || switch_channel_test_flag(channel, CF_VIDEO_ECHO);

		if (switch_channel_test_flag(channel, CF_VIDEO_READY) && !switch_test_flag(read_frame, SFF_CNG)) {
			switch_mutex_lock(mh->file_read_mutex);
			if (smh->video_read_fh && switch_test_flag(smh->video_read_fh, SWITCH_FILE_OPEN) && read_frame->img) {
				smh->video_read_fh->mm.fps = smh->vid_params.fps;
				switch_core_file_write_video(smh->video_read_fh, read_frame);
			}
			switch_mutex_unlock(mh->file_read_mutex);
		}

		if ((switch_channel_test_flag(channel, CF_VIDEO_WRITING) || session->video_read_callback) && !switch_channel_test_flag(channel, CF_VIDEO_BLANK)) {
			send_blank = 0;
		}

		if (send_blank) {
			if (read_frame && (switch_channel_test_flag(channel, CF_VIDEO_ECHO))) {
				switch_core_session_write_video_frame(session, read_frame, SWITCH_IO_FLAG_NONE, 0);
			} else if (blank_img) {
				fr.img = blank_img;
				switch_yield(10000);
				switch_core_session_write_video_frame(session, &fr, SWITCH_IO_FLAG_FORCE, 0);
			}
		}
	}

	switch_img_free(&blank_img);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Video thread ended\n", switch_channel_get_name(session->channel));

	switch_mutex_unlock(mh->cond_mutex);
	switch_core_session_rwunlock(session);

	mh->up = 0;
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_start_video_thread(switch_core_session_t *session)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_rtp_engine_t *v_engine = NULL;
	switch_media_handle_t *smh;

	if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
		return SWITCH_STATUS_NOTIMPL;
	}

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	switch_mutex_lock(smh->control_mutex);

	if (v_engine->media_thread) {
		switch_mutex_unlock(smh->control_mutex);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Starting Video thread\n", switch_core_session_get_name(session));

	if (v_engine->rtp_session) {
		switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->pt);
	}

	v_engine->mh.session = session;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	switch_thread_cond_create(&v_engine->mh.cond, pool);
	switch_mutex_init(&v_engine->mh.cond_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&v_engine->mh.file_read_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&v_engine->mh.file_write_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&smh->read_mutex[SWITCH_MEDIA_TYPE_VIDEO], SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO], SWITCH_MUTEX_NESTED, pool);
	v_engine->mh.ready = 0;

	if (switch_thread_create(&v_engine->media_thread, thd_attr, video_helper_thread, &v_engine->mh, 
							 switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		while(!v_engine->mh.ready) {
			switch_cond_next();
		}
	}

	switch_mutex_unlock(smh->control_mutex);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_core_media_start_engine_function(switch_core_session_t *session, switch_media_type_t type, switch_engine_function_t engine_function, void *user_data)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

	if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		switch_core_session_start_video_thread(session);
	}

	if (type == SWITCH_MEDIA_TYPE_TEXT) {
		switch_core_session_start_text_thread(session);
	}

	switch_mutex_lock(smh->control_mutex);
	if (!engine->engine_function_running) {
		engine->engine_function = engine_function;
		engine->engine_user_data = user_data;
		switch_core_session_video_reset(session);
	}
	switch_mutex_unlock(smh->control_mutex);
}

SWITCH_DECLARE(int) switch_core_media_check_engine_function(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;
	int r;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	engine = &smh->engines[type];

	switch_mutex_lock(smh->control_mutex);
	r = (engine->engine_function_running > 0);
	switch_mutex_unlock(smh->control_mutex);

	return r;
}

SWITCH_DECLARE(void) switch_core_media_end_engine_function(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return;
	}

	engine = &smh->engines[type];

	switch_mutex_lock(smh->control_mutex);
	if (engine->engine_function_running > 0) {
		engine->engine_function_running = -1;
	}
	switch_mutex_unlock(smh->control_mutex);

	while(engine->engine_function_running != 0) {
		switch_yield(10000);
	}
}

SWITCH_DECLARE(switch_bool_t) switch_core_session_in_video_thread(switch_core_session_t *session)
{
	switch_rtp_engine_t *v_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_FALSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	return switch_thread_equal(switch_thread_self(), v_engine->thread_id) ? SWITCH_TRUE : SWITCH_FALSE;
}


SWITCH_DECLARE(void) switch_core_media_parse_media_flags(switch_core_session_t *session)
{
	const char *var;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return;
	}

	if ((var = switch_channel_get_variable(session->channel, "rtp_media_autofix_timing"))) {
		if (switch_true(var)) {
			switch_media_handle_set_media_flag(smh, SCMF_AUTOFIX_TIMING);
		} else {
			switch_media_handle_clear_media_flag(smh, SCMF_AUTOFIX_TIMING);
		}
	}
}

//?
#define RA_PTR_LEN 512
SWITCH_DECLARE(switch_status_t) switch_core_media_proxy_remote_addr(switch_core_session_t *session, const char *sdp_str)
{
	const char *err;
	char rip[RA_PTR_LEN] = "";
	char rp[RA_PTR_LEN] = "";
	char rvp[RA_PTR_LEN] = "";
	char rtp[RA_PTR_LEN] = "";
	char *p, *ip_ptr = NULL, *port_ptr = NULL, *vid_port_ptr = NULL, *text_port_ptr = NULL, *pe;
	int x;
	const char *val;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

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

	if ((p = (char *) switch_stristr("m=text ", sdp_str))) {
		text_port_ptr = p + 7;
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

	if (port_ptr) {
		p = port_ptr;
		x = 0;
		while (x < sizeof(rp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
			rp[x++] = *p;
			p++;
			if (p >= pe) {
				goto end;
			}
		}
	}

	if (vid_port_ptr) {
		p = vid_port_ptr;
		x = 0;
		while (x < sizeof(rvp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
			rvp[x++] = *p;
			p++;
			if (p >= pe) {
				goto end;
			}
		}
	}

	if (text_port_ptr) {
		p = text_port_ptr;
		x = 0;
		while (x < sizeof(rtp) - 1 && p && *p && (*p >= '0' && *p <= '9')) {
			rtp[x++] = *p;
			p++;
			if (p >= pe) {
				goto end;
			}
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

	if (*rtp) {
		t_engine->cur_payload_map->remote_sdp_ip = switch_core_session_strdup(session, rip);
		t_engine->cur_payload_map->remote_sdp_port = (switch_port_t) atoi(rtp);
		switch_channel_set_flag(session->channel, CF_HAS_TEXT);
		switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
	}

	if (v_engine->cur_payload_map && v_engine->cur_payload_map->remote_sdp_ip && v_engine->cur_payload_map->remote_sdp_port) {
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
					if (switch_media_handle_test_media_flag(smh, SCMF_AUTOFIX_TIMING)) {
						v_engine->check_frames = 0;
					}
				}
			}
		}
		if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_PROXY_MODE) &&
			!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
			v_engine->rtp_session &&
			!switch_channel_test_flag(session->channel, CF_AVPF)) {
			/* Reactivate the NAT buster flag. */
			switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
		}
	}

	if (t_engine->cur_payload_map && t_engine->cur_payload_map->remote_sdp_ip && t_engine->cur_payload_map->remote_sdp_port) {
		if (!strcmp(t_engine->cur_payload_map->remote_sdp_ip, rip) && atoi(rvp) == t_engine->cur_payload_map->remote_sdp_port) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote text address:port [%s:%d] has not changed.\n",
							  t_engine->cur_payload_map->remote_sdp_ip, t_engine->cur_payload_map->remote_sdp_port);
		} else {
			switch_channel_set_flag(session->channel, CF_HAS_TEXT);
			switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
			if (switch_rtp_ready(t_engine->rtp_session)) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = t_engine->remote_rtcp_port;

				if (!remote_rtcp_port) {
					if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_text_rtcp_port"))) {
						remote_rtcp_port = (switch_port_t)atoi(rport);
					}
				}


				if (switch_rtp_set_remote_address(t_engine->rtp_session, t_engine->cur_payload_map->remote_sdp_ip,
												  t_engine->cur_payload_map->remote_sdp_port, remote_rtcp_port, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "TEXT RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "TEXT RTP CHANGING DEST TO: [%s:%d]\n",
									  t_engine->cur_payload_map->remote_sdp_ip, t_engine->cur_payload_map->remote_sdp_port);
					if (switch_media_handle_test_media_flag(smh, SCMF_AUTOFIX_TIMING)) {
						t_engine->check_frames = 0;
					}
				}
			}
		}
		if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_PROXY_MODE) &&
			!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
			t_engine->rtp_session &&
			!switch_channel_test_flag(session->channel, CF_AVPF)) {
			/* Reactivate the NAT buster flag. */
			switch_rtp_set_flag(t_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
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
				!switch_channel_test_flag(session->channel, CF_AVPF)) {
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

	if (!lookup_rtpip) {
		return SWITCH_STATUS_FALSE;
	}

	/* Don't do anything if we're in proxy mode or if a (remote) port already has been found */
	if (!force) {
		if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) ||
			switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) || engine->adv_sdp_port) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Always too late when RTP has already started */
	if (engine->rtp_session) {
		return SWITCH_STATUS_SUCCESS;
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

	if (zstr(smh->mparams->remote_ip)) { /* no remote_ip, we're originating */
		if (!zstr(smh->mparams->extrtpip)) { /* and we've got an ext-rtp-ip, eg, from verto config */
			use_ip = smh->mparams->extrtpip; /* let's use it for composing local sdp to send to client */
			/*
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
						"%s will use %s instead of %s in SDP, because we're originating and we have an ext-rtp-ip setting\n",
						switch_channel_get_name(smh->session->channel), smh->mparams->extrtpip, smh->mparams->rtpip);
			*/
		}
	}
	engine->adv_sdp_port = sdp_port;
	engine->adv_sdp_ip = smh->mparams->adv_sdp_audio_ip = smh->mparams->extrtpip = switch_core_session_strdup(session, use_ip);

	if (type == SWITCH_MEDIA_TYPE_AUDIO) {
		switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, engine->local_sdp_ip);
		switch_channel_set_variable_printf(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", sdp_port);
		switch_channel_set_variable(session->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, engine->adv_sdp_ip);
	} else if (type == SWITCH_MEDIA_TYPE_VIDEO) {
		switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, engine->adv_sdp_ip);
		switch_channel_set_variable_printf(session->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, "%d", sdp_port);
	} else if (type == SWITCH_MEDIA_TYPE_TEXT) {
		switch_channel_set_variable(session->channel, SWITCH_LOCAL_TEXT_IP_VARIABLE, engine->adv_sdp_ip);
		switch_channel_set_variable_printf(session->channel, SWITCH_LOCAL_TEXT_PORT_VARIABLE, "%d", sdp_port);
	}


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_media_choose_ports(switch_core_session_t *session, switch_bool_t audio, switch_bool_t video)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(smh->mparams->rtpip)) {

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has no media ip\n",
						  switch_channel_get_name(smh->session->channel));
		switch_channel_hangup(smh->session->channel, SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL);

		return SWITCH_STATUS_FALSE;
	}

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
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (t_engine->tf) {
		switch_rtp_text_factory_destroy(&t_engine->tf);
	}

	if (a_engine->media_thread) {
		switch_status_t st;

		switch_mutex_lock(smh->control_mutex);
		if (a_engine->mh.up && a_engine->write_fb) {
			switch_frame_buffer_push(a_engine->write_fb, (void *) 1);
		}
		a_engine->mh.up = 0;
		switch_mutex_unlock(smh->control_mutex);

		switch_thread_join(&st, a_engine->media_thread);
		a_engine->media_thread = NULL;
	}

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


	if (t_engine->media_thread) {
		switch_status_t st;

		t_engine->mh.up = 0;
		switch_thread_join(&st, t_engine->media_thread);
		t_engine->media_thread = NULL;
	}


	if (t_engine->rtp_session) {
		switch_rtp_destroy(&t_engine->rtp_session);
	} else if (t_engine->local_sdp_port) {
		switch_rtp_release_port(smh->mparams->rtpip, t_engine->local_sdp_port);
	}


	if (t_engine->local_sdp_port > 0 && !zstr(smh->mparams->remote_ip) &&
		switch_core_media_check_nat(smh, smh->mparams->remote_ip)) {
		switch_nat_del_mapping((switch_port_t) t_engine->local_sdp_port, SWITCH_NAT_UDP);
		switch_nat_del_mapping((switch_port_t) t_engine->local_sdp_port + 1, SWITCH_NAT_UDP);
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

	//#ifdef RTCP_MUX
	//if (!engine->rtcp_mux) {//  && type == SWITCH_MEDIA_TYPE_AUDIO) {
	//	engine->rtcp_mux = SWITCH_TRUE;
	//}
	//#endif

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

	if (!v_engine->media_thread) {
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
	if (switch_channel_test_flag(session->channel, CF_REINVITE) && engine->new_dtls) {

		if (!zstr(engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(session)) {

#ifdef HAVE_OPENSSL_DTLSv1_2_method
			uint8_t want_DTLSv1_2 = 1;
#else
			uint8_t want_DTLSv1_2 = 0;
#endif // HAVE_OPENSSL_DTLSv1_2_method

			dtls_type_t xtype, dtype = engine->dtls_controller ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "RE-SETTING %s DTLS\n", type2str(engine->type));

			xtype = DTLS_TYPE_RTP;
			if (engine->rtcp_mux > 0) xtype |= DTLS_TYPE_RTCP;

			if (switch_channel_var_true(session->channel, "legacyDTLS")) {
				switch_channel_clear_flag(session->channel, CF_WANT_DTLSv1_2);
				want_DTLSv1_2 = 0;
			}

			switch_rtp_add_dtls(engine->rtp_session, &engine->local_dtls_fingerprint, &engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);

			if (engine->rtcp_mux < 1) {
				xtype = DTLS_TYPE_RTCP;
				switch_rtp_add_dtls(engine->rtp_session, &engine->local_dtls_fingerprint, &engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);
			}

		}
		engine->new_dtls = 0;
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
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;
	int is_reinvite = 0;

#ifdef HAVE_OPENSSL_DTLSv1_2_method
			uint8_t want_DTLSv1_2 = 1;
#else
			uint8_t want_DTLSv1_2 = 0;
#endif

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (a_engine->rtp_session || v_engine->rtp_session || t_engine->rtp_session || switch_channel_test_flag(session->channel, CF_REINVITE)) {
		is_reinvite = 1;
	}


	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_core_media_parse_media_flags(session);

	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_rtp_reset_media_timer(a_engine->rtp_session);
		check_media_timeout_params(session, a_engine);
		check_media_timeout_params(session, v_engine);
	}

	if (a_engine->crypto_type != CRYPTO_INVALID) {
		switch_channel_set_flag(session->channel, CF_SECURE);
	}
	
	if (want_DTLSv1_2) {
		switch_channel_set_flag(session->channel, CF_WANT_DTLSv1_2);
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE)) {
		status = SWITCH_STATUS_SUCCESS;
		goto end;
	}

	if (!is_reinvite) {
		if (switch_rtp_ready(a_engine->rtp_session)) {
			if (switch_channel_test_flag(session->channel, CF_TEXT_POSSIBLE) && !switch_rtp_ready(t_engine->rtp_session)) {
				goto text;
			}

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

	if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
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

	if (a_engine->rtp_session && is_reinvite) {
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
			switch_channel_execute_on(session->channel, "execute_on_audio_change");
		}
	}

	if (!switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "AUDIO RTP [%s] %s port %d -> %s port %d codec: %u ms: %d\n",
						  switch_channel_get_name(session->channel),
						  a_engine->local_sdp_ip,
						  a_engine->local_sdp_port,
						  a_engine->cur_payload_map->remote_sdp_ip,
						  a_engine->cur_payload_map->remote_sdp_port, a_engine->cur_payload_map->pt, a_engine->read_impl.microseconds_per_packet / 1000);

		//XX
	}

	switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->local_sdp_port);
	switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, a_engine->local_sdp_ip);
	switch_channel_set_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, tmp);
	switch_channel_set_variable(session->channel, SWITCH_ADVERTISED_MEDIA_IP_VARIABLE, a_engine->adv_sdp_ip);

	if (a_engine->rtp_session && is_reinvite) {
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

			//if (switch_channel_test_flag(session->channel, CF_PROTO_HOLD) && strcmp(a_engine->cur_payload_map->remote_sdp_ip, "0.0.0.0")) {
			//	switch_core_media_toggle_hold(session, 0);
			//}


			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) &&
				!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
				!switch_channel_test_flag(session->channel, CF_AVPF)) {
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

		if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
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
						  a_engine->cur_payload_map->remote_sdp_port, a_engine->cur_payload_map->pt, a_engine->read_impl.microseconds_per_packet / 1000);

		if (switch_rtp_ready(a_engine->rtp_session)) {
			switch_rtp_set_default_payload(a_engine->rtp_session, a_engine->cur_payload_map->pt);
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
											   a_engine->cur_payload_map->pt,
											   a_engine->read_impl.samples_per_packet,
											   a_engine->cur_payload_map->codec_ms * 1000,
											   flags, timer_name, &err, switch_core_session_get_pool(session),
											   0, 0);

		if (switch_rtp_ready(a_engine->rtp_session)) {
			switch_rtp_set_payload_map(a_engine->rtp_session, &a_engine->payload_map);
		}
	}

	if (switch_rtp_ready(a_engine->rtp_session)) {
		uint8_t vad_in = (smh->mparams->vflags & VAD_IN);
		uint8_t vad_out = (smh->mparams->vflags & VAD_OUT);
		uint8_t inb = switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND;
		const char *ssrc;

		switch_mutex_init(&smh->read_mutex[SWITCH_MEDIA_TYPE_AUDIO], SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
		switch_mutex_init(&smh->write_mutex[SWITCH_MEDIA_TYPE_AUDIO], SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

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

		check_media_timeout_params(session, a_engine);

		switch_channel_set_flag(session->channel, CF_FS_RTP);

		switch_channel_set_variable_printf(session->channel, "rtp_use_pt", "%d", a_engine->cur_payload_map->pt);

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

		//if (switch_channel_test_flag(session->channel, CF_AVPF)) {
		//	smh->mparams->manual_rtp_bugs = RTP_BUG_SEND_LINEAR_TIMESTAMPS;
		//}

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
									switch_determine_ice_type(a_engine, session),
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
					interval = 5000;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activating RTCP PORT %d\n", remote_rtcp_port);
				switch_rtp_activate_rtcp(a_engine->rtp_session, interval, remote_rtcp_port, a_engine->rtcp_mux > 0);

			}

			if (a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].ready && a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].ready &&
				!zstr(a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].con_addr) && 
				!zstr(a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].con_addr)) {
				if (a_engine->rtcp_mux > 0 && 
					!strcmp(a_engine->ice_in.cands[a_engine->ice_in.chosen[1]][1].con_addr, a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].con_addr)
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
											switch_determine_ice_type(a_engine, session),
											&a_engine->ice_in
#endif
										);
				}

			}
		}

		if (!zstr(a_engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(smh->session)) {
			dtls_type_t xtype, dtype = a_engine->dtls_controller ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;

			//if (switch_channel_test_flag(smh->session->channel, CF_3PCC)) {
			//	dtype = (dtype == DTLS_TYPE_CLIENT) ? DTLS_TYPE_SERVER : DTLS_TYPE_CLIENT;
			//}

			xtype = DTLS_TYPE_RTP;
			if (a_engine->rtcp_mux > 0 && smh->mparams->rtcp_audio_interval_msec) xtype |= DTLS_TYPE_RTCP;

			if (switch_channel_var_true(session->channel, "legacyDTLS")) {
				switch_channel_clear_flag(session->channel, CF_WANT_DTLSv1_2);
				want_DTLSv1_2 = 0;
			}

			switch_rtp_add_dtls(a_engine->rtp_session, &a_engine->local_dtls_fingerprint, &a_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);

			if (a_engine->rtcp_mux < 1 && smh->mparams->rtcp_audio_interval_msec) {
				xtype = DTLS_TYPE_RTCP;
				switch_rtp_add_dtls(a_engine->rtp_session, &a_engine->local_dtls_fingerprint, &a_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);
			}

		}

		check_jb(session, NULL, 0, 0, SWITCH_FALSE);

		if ((val = switch_channel_get_variable(session->channel, "rtp_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "rtp_timeout_sec deprecated use media_timeout variable.\n"); 
				smh->mparams->rtp_timeout_sec = v;
			}
		}

		if ((val = switch_channel_get_variable(session->channel, "rtp_hold_timeout_sec"))) {
			int v = atoi(val);
			if (v >= 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "rtp_hold_timeout_sec deprecated use media_hold_timeout variable.\n"); 
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
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Set 2833 dtmf send payload to %u\n",
							  switch_channel_get_name(session->channel), smh->mparams->te);
			switch_rtp_set_telephony_event(a_engine->rtp_session, smh->mparams->te);
			switch_channel_set_variable_printf(session->channel, "rtp_2833_send_payload", "%d", smh->mparams->te);
		}

		if (smh->mparams->recv_te) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Set 2833 dtmf receive payload to %u\n",
							  switch_channel_get_name(session->channel), smh->mparams->recv_te);
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



	text:

		//if (switch_channel_test_flag(session->channel, CF_MSRP)) { // skip RTP RTT
		//	goto video;
		//}

		if (switch_channel_test_flag(session->channel, CF_TEXT_POSSIBLE) && t_engine->cur_payload_map->rm_encoding && t_engine->cur_payload_map->remote_sdp_port) {
			/******************************************************************************************/
			if (t_engine->rtp_session && is_reinvite) {
				//const char *ip = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE);
				//const char *port = switch_channel_get_variable(session->channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE);
				char *remote_host = switch_rtp_get_remote_host(t_engine->rtp_session);
				switch_port_t remote_port = switch_rtp_get_remote_port(t_engine->rtp_session);



				if (remote_host && remote_port && !strcmp(remote_host, t_engine->cur_payload_map->remote_sdp_ip) && remote_port == t_engine->cur_payload_map->remote_sdp_port) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Text params are unchanged for %s.\n",
									  switch_channel_get_name(session->channel));
					t_engine->cur_payload_map->negotiated = 1;
					goto text_up;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Text params changed for %s from %s:%d to %s:%d\n",
									  switch_channel_get_name(session->channel),
									  remote_host, remote_port, t_engine->cur_payload_map->remote_sdp_ip, t_engine->cur_payload_map->remote_sdp_port);
				}
			}

			if (!switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				if (switch_rtp_ready(t_engine->rtp_session)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "TEXT RTP [%s] %s port %d -> %s port %d codec: %u\n", switch_channel_get_name(session->channel),
									  t_engine->local_sdp_ip, t_engine->local_sdp_port, t_engine->cur_payload_map->remote_sdp_ip,
									  t_engine->cur_payload_map->remote_sdp_port, t_engine->cur_payload_map->pt);

					switch_rtp_set_default_payload(t_engine->rtp_session, t_engine->cur_payload_map->pt);
				}
			}

			switch_snprintf(tmp, sizeof(tmp), "%d", t_engine->local_sdp_port);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_TEXT_IP_VARIABLE, a_engine->adv_sdp_ip);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_TEXT_PORT_VARIABLE, tmp);


			if (t_engine->rtp_session && is_reinvite) {
				const char *rport = NULL;
				switch_port_t remote_rtcp_port = t_engine->remote_rtcp_port;

				//switch_channel_clear_flag(session->channel, CF_REINVITE);

				if (!remote_rtcp_port) {
					if ((rport = switch_channel_get_variable(session->channel, "rtp_remote_text_rtcp_port"))) {
						remote_rtcp_port = (switch_port_t)atoi(rport);
					}
				}

				if (switch_rtp_set_remote_address
					(t_engine->rtp_session, t_engine->cur_payload_map->remote_sdp_ip, t_engine->cur_payload_map->remote_sdp_port, remote_rtcp_port, SWITCH_TRUE,
					 &err) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "TEXT RTP REPORTS ERROR: [%s]\n", err);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "TEXT RTP CHANGING DEST TO: [%s:%d]\n",
									  t_engine->cur_payload_map->remote_sdp_ip, t_engine->cur_payload_map->remote_sdp_port);
					if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
						!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(t_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}

				}
				goto text_up;
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				switch_core_media_proxy_remote_addr(session, NULL);

				memset(flags, 0, sizeof(flags));
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
				flags[SWITCH_RTP_FLAG_DATAWAIT]++;

				if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
					!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
					flags[SWITCH_RTP_FLAG_AUTOADJ]++;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "PROXY TEXT RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
								  switch_channel_get_name(session->channel),
								  a_engine->cur_payload_map->remote_sdp_ip,
								  t_engine->local_sdp_port,
								  t_engine->cur_payload_map->remote_sdp_ip,
								  t_engine->cur_payload_map->remote_sdp_port, t_engine->cur_payload_map->pt, t_engine->read_impl.microseconds_per_packet / 1000);

				if (switch_rtp_ready(t_engine->rtp_session)) {
					switch_rtp_set_default_payload(t_engine->rtp_session, t_engine->cur_payload_map->pt);
				}
			}

			/******************************************************************************************/

			if (t_engine->rtp_session) {
				goto text_up;
			}


			if (!t_engine->local_sdp_port) {
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_TEXT, 1);
			}

			memset(flags, 0, sizeof(flags));
			flags[SWITCH_RTP_FLAG_DATAWAIT]++;
			flags[SWITCH_RTP_FLAG_RAW_WRITE]++;

			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_PROXY_MODE) &&
				!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val)) &&
				!switch_channel_test_flag(session->channel, CF_AVPF)) {
				flags[SWITCH_RTP_FLAG_AUTOADJ]++;
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
			}
			//TEXT switch_core_media_set_text_codec(session, 0);

			flags[SWITCH_RTP_FLAG_USE_TIMER] = 1;
			flags[SWITCH_RTP_FLAG_NOBLOCK] = 0;
			flags[SWITCH_RTP_FLAG_TEXT]++;
			//flags[SWITCH_RTP_FLAG_VIDEO]++;

			t_engine->rtp_session = switch_rtp_new(a_engine->local_sdp_ip,
												   t_engine->local_sdp_port,
												   t_engine->cur_payload_map->remote_sdp_ip,
												   t_engine->cur_payload_map->remote_sdp_port,
												   t_engine->cur_payload_map->pt,
												   TEXT_TIMER_SAMPLES, TEXT_TIMER_MS * 1000, flags, NULL, &err, switch_core_session_get_pool(session),
												   0, 0);


			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%sTEXT RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(session->channel),
							  a_engine->local_sdp_ip,
							  t_engine->local_sdp_port,
							  t_engine->cur_payload_map->remote_sdp_ip,
							  t_engine->cur_payload_map->remote_sdp_port, t_engine->cur_payload_map->pt,
							  0, switch_rtp_ready(t_engine->rtp_session) ? "SUCCESS" : err);


			if (switch_rtp_ready(t_engine->rtp_session)) {
				const char *ssrc;


				if (!t_engine->tf) {
					switch_rtp_text_factory_create(&t_engine->tf, switch_core_session_get_pool(session));
				}

				switch_rtp_set_video_buffer_size(t_engine->rtp_session, 2, 2048);

				switch_rtp_set_payload_map(t_engine->rtp_session, &t_engine->payload_map);
				switch_channel_set_flag(session->channel, CF_HAS_TEXT);
				switch_core_session_start_text_thread(session);

				if ((ssrc = switch_channel_get_variable(session->channel, "rtp_use_text_ssrc"))) {
					uint32_t ssrc_ul = (uint32_t) strtoul(ssrc, NULL, 10);
					switch_rtp_set_ssrc(t_engine->rtp_session, ssrc_ul);
					t_engine->ssrc = ssrc_ul;
				} else {
					switch_rtp_set_ssrc(t_engine->rtp_session, t_engine->ssrc);
				}

				if (t_engine->remote_ssrc) {
					switch_rtp_set_remote_ssrc(t_engine->rtp_session, t_engine->remote_ssrc);
				}

				if (t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].ready) {

					gen_ice(session, SWITCH_MEDIA_TYPE_TEXT, NULL, 0);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating Text ICE\n");

					switch_rtp_activate_ice(t_engine->rtp_session,
											t_engine->ice_in.ufrag,
											t_engine->ice_out.ufrag,
											t_engine->ice_out.pwd,
											t_engine->ice_in.pwd,
											IPR_RTP,
#ifdef GOOGLE_ICE
											ICE_GOOGLE_JINGLE,
											NULL
#else
											switch_determine_ice_type(t_engine, session),
											&t_engine->ice_in
#endif
											);


				}

				if ((val = switch_channel_get_variable(session->channel, "rtcp_text_interval_msec")) || (val = smh->mparams->rtcp_text_interval_msec)) {
					const char *rport = switch_channel_get_variable(session->channel, "rtp_remote_text_rtcp_port");
					switch_port_t remote_port = t_engine->remote_rtcp_port;

					if (rport) {
						remote_port = (switch_port_t)atoi(rport);
					}
					if (!strcasecmp(val, "passthru")) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating TEXT RTCP PASSTHRU PORT %d\n", remote_port);
						switch_rtp_activate_rtcp(t_engine->rtp_session, -1, remote_port, t_engine->rtcp_mux > 0);
					} else {
						int interval = atoi(val);
						if (interval < 100 || interval > 500000) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
											  "Invalid rtcp interval spec [%d] must be between 100 and 500000\n", interval);
							interval = 5000;
						}
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
										  "Activating TEXT RTCP PORT %d interval %d mux %d\n", remote_port, interval, t_engine->rtcp_mux);
						switch_rtp_activate_rtcp(t_engine->rtp_session, interval, remote_port, t_engine->rtcp_mux > 0);

					}


					if (t_engine->ice_in.cands[t_engine->ice_in.chosen[1]][1].ready && t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].ready &&
						!zstr(t_engine->ice_in.cands[t_engine->ice_in.chosen[1]][1].con_addr) && 
						!zstr(t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].con_addr)) {
						if (t_engine->rtcp_mux > 0 && !strcmp(t_engine->ice_in.cands[t_engine->ice_in.chosen[1]][1].con_addr,
															  t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].con_addr) &&
							t_engine->ice_in.cands[t_engine->ice_in.chosen[1]][1].con_port == t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].con_port) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Skipping TEXT RTCP ICE (Same as TEXT RTP)\n");
						} else {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating TEXT RTCP ICE\n");
							switch_rtp_activate_ice(t_engine->rtp_session,
													t_engine->ice_in.ufrag,
													t_engine->ice_out.ufrag,
													t_engine->ice_out.pwd,
													t_engine->ice_in.pwd,
													IPR_RTCP,
#ifdef GOOGLE_ICE
													ICE_GOOGLE_JINGLE,
													NULL
#else
													switch_determine_ice_type(t_engine, session),
													&t_engine->ice_in
#endif
													);



						}

					}
				}

				if (!zstr(t_engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(smh->session)) {
					dtls_type_t xtype,
						dtype = t_engine->dtls_controller ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;
					xtype = DTLS_TYPE_RTP;
					if (t_engine->rtcp_mux > 0 && smh->mparams->rtcp_text_interval_msec) xtype |= DTLS_TYPE_RTCP;
			
					if (switch_channel_var_true(session->channel, "legacyDTLS")) {
						switch_channel_clear_flag(session->channel, CF_WANT_DTLSv1_2);
						want_DTLSv1_2 = 0;
					}

					switch_rtp_add_dtls(t_engine->rtp_session, &t_engine->local_dtls_fingerprint, &t_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);

					if (t_engine->rtcp_mux < 1 && smh->mparams->rtcp_text_interval_msec) {
						xtype = DTLS_TYPE_RTCP;
						switch_rtp_add_dtls(t_engine->rtp_session, &t_engine->local_dtls_fingerprint, &t_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);
					}
				}


				if ((val = switch_channel_get_variable(session->channel, "rtp_manual_text_rtp_bugs"))) {
					switch_core_media_parse_rtp_bugs(&t_engine->rtp_bugs, val);
				}


				//if (switch_channel_test_flag(session->channel, CF_AVPF)) {
					//smh->mparams->manual_video_rtp_bugs = RTP_BUG_SEND_LINEAR_TIMESTAMPS;
				//}

				switch_rtp_intentional_bugs(t_engine->rtp_session, t_engine->rtp_bugs | smh->mparams->manual_text_rtp_bugs);

				//XX


				switch_channel_set_variable_printf(session->channel, "rtp_use_text_pt", "%d", t_engine->cur_payload_map->pt);
				t_engine->ssrc = switch_rtp_get_ssrc(t_engine->rtp_session);
				switch_channel_set_variable_printf(session->channel, "rtp_use_text_ssrc", "%u", t_engine->ssrc);

				switch_core_session_apply_crypto(session, SWITCH_MEDIA_TYPE_TEXT);


				if (switch_channel_test_flag(session->channel, CF_ZRTP_PASSTHRU)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activating text UDPTL mode\n");
					switch_rtp_udptl_mode(t_engine->rtp_session);
				}

			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "TEXT RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				goto end;
			}
		}


	text_up:
	video:

		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_core_media_check_video_codecs(session);
		}

		if (switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE) && v_engine->cur_payload_map->rm_encoding && v_engine->cur_payload_map->remote_sdp_port) {
			/******************************************************************************************/
			if (v_engine->rtp_session && is_reinvite) {
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
									  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->pt);

					switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->pt);
				}
			}

			switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->local_sdp_port);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_IP_VARIABLE, a_engine->adv_sdp_ip);
			switch_channel_set_variable(session->channel, SWITCH_LOCAL_VIDEO_PORT_VARIABLE, tmp);


			if (v_engine->rtp_session && is_reinvite) {
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
					if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
						!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
						/* Reactivate the NAT buster flag. */
						switch_rtp_set_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}

				}
				goto video_up;
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				switch_core_media_proxy_remote_addr(session, NULL);

				memset(flags, 0, sizeof(flags));
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
				flags[SWITCH_RTP_FLAG_DATAWAIT]++;

				if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
					!((val = switch_channel_get_variable(session->channel, "disable_rtp_auto_adjust")) && switch_true(val))) {
					flags[SWITCH_RTP_FLAG_AUTOADJ]++;
				}

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "PROXY VIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d\n",
								  switch_channel_get_name(session->channel),
								  a_engine->cur_payload_map->remote_sdp_ip,
								  v_engine->local_sdp_port,
								  v_engine->cur_payload_map->remote_sdp_ip,
								  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->pt, v_engine->read_impl.microseconds_per_packet / 1000);

				if (switch_rtp_ready(v_engine->rtp_session)) {
					switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->cur_payload_map->pt);
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
				!switch_channel_test_flag(session->channel, CF_AVPF)) {
				flags[SWITCH_RTP_FLAG_AUTOADJ]++;
			}

			if (switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
				flags[SWITCH_RTP_FLAG_PROXY_MEDIA]++;
			}
			switch_core_media_set_video_codec(session, 0);

			flags[SWITCH_RTP_FLAG_USE_TIMER] = 0;
			flags[SWITCH_RTP_FLAG_NOBLOCK] = 0;
			flags[SWITCH_RTP_FLAG_VIDEO]++;

			if (v_engine->fir) {
				flags[SWITCH_RTP_FLAG_FIR]++;
			}

			if (v_engine->pli) {
				flags[SWITCH_RTP_FLAG_PLI]++;
			}

			if ((v_engine->nack) && !switch_channel_var_true(session->channel, "rtp_video_nack_disable")) { 
				flags[SWITCH_RTP_FLAG_NACK]++;
			}

			if (v_engine->tmmbr) {
				flags[SWITCH_RTP_FLAG_TMMBR]++;
			}
			
			v_engine->rtp_session = switch_rtp_new(a_engine->local_sdp_ip,
														 v_engine->local_sdp_port,
														 v_engine->cur_payload_map->remote_sdp_ip,
														 v_engine->cur_payload_map->remote_sdp_port,
														 v_engine->cur_payload_map->pt,
														 1, 90000, flags, NULL, &err, switch_core_session_get_pool(session),
														 0, 0);


			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%sVIDEO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
							  switch_channel_test_flag(session->channel, CF_PROXY_MEDIA) ? "PROXY " : "",
							  switch_channel_get_name(session->channel),
							  a_engine->local_sdp_ip,
							  v_engine->local_sdp_port,
							  v_engine->cur_payload_map->remote_sdp_ip,
							  v_engine->cur_payload_map->remote_sdp_port, v_engine->cur_payload_map->pt,
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
				switch_channel_set_flag(session->channel, CF_VIDEO);
				switch_core_session_start_video_thread(session);

				switch_rtp_set_video_buffer_size(v_engine->rtp_session, 1, 0);
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

				check_media_timeout_params(session, v_engine);

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
											switch_determine_ice_type(v_engine, session),
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
							interval = 5000;
						}
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
										  "Activating VIDEO RTCP PORT %d interval %d mux %d\n", remote_port, interval, v_engine->rtcp_mux);
						switch_rtp_activate_rtcp(v_engine->rtp_session, interval, remote_port, v_engine->rtcp_mux > 0);

					}


					if (v_engine->ice_in.cands[v_engine->ice_in.chosen[1]][1].ready && v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].ready &&
						!zstr(v_engine->ice_in.cands[v_engine->ice_in.chosen[1]][1].con_addr) && 
						!zstr(v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].con_addr)) {

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
													switch_determine_ice_type(v_engine, session),
													&v_engine->ice_in
#endif
													);



						}

					}
				}

				if (!zstr(v_engine->local_dtls_fingerprint.str) && switch_rtp_has_dtls() && dtls_ok(smh->session)) {
					dtls_type_t xtype,
						dtype = v_engine->dtls_controller ? DTLS_TYPE_CLIENT : DTLS_TYPE_SERVER;
					xtype = DTLS_TYPE_RTP;
					if (v_engine->rtcp_mux > 0 && smh->mparams->rtcp_video_interval_msec) xtype |= DTLS_TYPE_RTCP;
			

					if (switch_channel_var_true(session->channel, "legacyDTLS")) {
						switch_channel_clear_flag(session->channel, CF_WANT_DTLSv1_2);
						want_DTLSv1_2 = 0;
					}

					switch_rtp_add_dtls(v_engine->rtp_session, &v_engine->local_dtls_fingerprint, &v_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);

					if (v_engine->rtcp_mux < 1 && smh->mparams->rtcp_video_interval_msec) {
						xtype = DTLS_TYPE_RTCP;
						switch_rtp_add_dtls(v_engine->rtp_session, &v_engine->local_dtls_fingerprint, &v_engine->remote_dtls_fingerprint, dtype | xtype, want_DTLSv1_2);
					}
				}


				if ((val = switch_channel_get_variable(session->channel, "rtp_manual_video_rtp_bugs"))) {
					switch_core_media_parse_rtp_bugs(&v_engine->rtp_bugs, val);
				}

				if (switch_channel_test_flag(session->channel, CF_AVPF)) {
					smh->mparams->manual_video_rtp_bugs = RTP_BUG_SEND_LINEAR_TIMESTAMPS;
				}

				switch_rtp_intentional_bugs(v_engine->rtp_session, v_engine->rtp_bugs | smh->mparams->manual_video_rtp_bugs);

				//XX


				switch_channel_set_variable_printf(session->channel, "rtp_use_video_pt", "%d", v_engine->cur_payload_map->pt);
				v_engine->ssrc = switch_rtp_get_ssrc(v_engine->rtp_session);
				switch_channel_set_variable_printf(session->channel, "rtp_use_video_ssrc", "%u", v_engine->ssrc);

				switch_core_session_apply_crypto(session, SWITCH_MEDIA_TYPE_VIDEO);


				if (switch_channel_test_flag(session->channel, CF_ZRTP_PASSTHRU)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activating video UDPTL mode\n");
					switch_rtp_udptl_mode(v_engine->rtp_session);
				}

			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "VIDEO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
				switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				goto end;
			}
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", switch_str_nil(err));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
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

	if (switch_channel_test_flag(session->channel, CF_AVPF)) {
		if (switch_channel_test_flag(session->channel, CF_DTLS) || secure) {
			if (switch_channel_test_flag(session->channel, CF_AVPF_MOZ)) {
				return "UDP/TLS/RTP/SAVPF";
			} else {
				return "RTP/SAVPF";
			}
		} else {
			if (switch_channel_test_flag(session->channel, CF_AVPF_MOZ)) {
				return "UDP/AVPF";
			} else {
				return "RTP/AVPF";
			}
		}
	}

	if (secure) {
		return "RTP/SAVP";
	}

	return "RTP/AVP";

}

static char *get_setup(switch_rtp_engine_t *engine, switch_core_session_t *session, switch_sdp_type_t sdp_type)
{

	if (sdp_type == SDP_TYPE_REQUEST) {
		engine->dtls_controller = 0;
		engine->new_dtls = 1;
		engine->new_ice = 1;
		return "actpass";
	} else {
		return engine->dtls_controller ? "active" : "passive";
	}
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
	int include_external;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	//switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "m=audio %d RTP/%sAVP%s",
	//port, secure ? "S" : "", switch_channel_test_flag(session->channel, CF_AVPF) ? "F" : "");

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "m=audio %d %s", port,
					get_media_profile_name(session, secure || a_engine->crypto_type != CRYPTO_INVALID));

	include_external = switch_channel_var_true(session->channel, "include_external_ip");

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
		int i;
		for (i = 0; i < smh->num_rates; i++) {
			if (smh->dtmf_ianacodes[i]) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", smh->dtmf_ianacodes[i]);
			}
			if (smh->cng_ianacodes[i] && !switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && cng_type && use_cng) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", smh->cng_ianacodes[i]);
			}
		}
	}

	//if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && cng_type && use_cng) {
		//switch_snprintf(buf + strlen(buf), buflen - strlen(buf), " %d", cng_type);
	//}

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "\r\n");


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
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d %s/%d/%d\r\n", smh->ianacodes[i], imp->iananame, rate, channels);

			} else {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d %s/%d\r\n", smh->ianacodes[i], imp->iananame, rate);
			}
		}

		if (fmtp) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=fmtp:%d %s\r\n", smh->ianacodes[i], fmtp);
		}
	}


	if ((smh->mparams->dtmf_type == DTMF_2833 || switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF)) && smh->mparams->te > 95) {

		for (i = 0; i < smh->num_rates; i++) {
			if (switch_channel_test_flag(session->channel, CF_AVPF)) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d telephone-event/%d\r\n",
								smh->dtmf_ianacodes[i], smh->rates[i]);
			} else {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d telephone-event/%d\r\na=fmtp:%d 0-16\r\n",
								smh->dtmf_ianacodes[i], smh->rates[i], smh->dtmf_ianacodes[i]);
			}
		}
	}

	if (!zstr(a_engine->local_dtls_fingerprint.type) && secure) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=fingerprint:%s %s\r\na=setup:%s\r\n", a_engine->local_dtls_fingerprint.type,
						a_engine->local_dtls_fingerprint.str, get_setup(a_engine, session, sdp_type));
	}

	if (smh->mparams->rtcp_audio_interval_msec) {
		if (a_engine->rtcp_mux > 0) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-mux\r\n");
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp:%d IN %s %s\r\n", port, family, ip);
		} else {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp:%d IN %s %s\r\n", port + 1, family, ip);
		}
	}

	//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\r\n", a_engine->ssrc);

	if (a_engine->ice_out.cands[0][0].ready) {
		char tmp1[11] = "";
		char tmp2[11] = "";
		char tmp3[11] = "";
		uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
		uint32_t c2 = c1 - 1;

		//uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
		//uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
		//uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);
		ice_t *ice_out;

		tmp1[10] = '\0';
		tmp2[10] = '\0';
		tmp3[10] = '\0';
		switch_stun_random_string(tmp1, 10, "0123456789");
		switch_stun_random_string(tmp2, 10, "0123456789");
		switch_stun_random_string(tmp3, 10, "0123456789");

		gen_ice(session, SWITCH_MEDIA_TYPE_AUDIO, NULL, 0);

		ice_out = &a_engine->ice_out;

		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u cname:%s\r\n", a_engine->ssrc, smh->cname);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u msid:%s a0\r\n", a_engine->ssrc, smh->msid);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u mslabel:%s\r\n", a_engine->ssrc, smh->msid);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ssrc:%u label:%sa0\r\n", a_engine->ssrc, smh->msid);


		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-ufrag:%s\r\n", ice_out->ufrag);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-pwd:%s\r\n", ice_out->pwd);


		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
						tmp1, ice_out->cands[0][0].transport, c1,
						ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
						);

		if (include_external && !zstr(smh->mparams->extsipip)) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
				tmp3, ice_out->cands[0][0].transport, c1,
				smh->mparams->extsipip, ice_out->cands[0][0].con_port
				);
		}

		if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
			strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
			&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
							tmp2, ice_out->cands[0][0].transport, c2,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
							a_engine->local_sdp_ip, a_engine->local_sdp_port
							);
		}

		if (a_engine->rtcp_mux < 1 || switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND || switch_channel_test_flag(session->channel, CF_RECOVERING)) {


			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
							tmp1, ice_out->cands[0][0].transport, c1,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
							);
			
			if (include_external && !zstr(smh->mparams->extsipip)) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
					tmp3, ice_out->cands[0][0].transport, c1,
					smh->mparams->extsipip, ice_out->cands[0][0].con_port
					);
			}

			if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][1].con_addr) && 
				strcmp(a_engine->local_sdp_ip, ice_out->cands[0][1].con_addr)
				&& a_engine->local_sdp_port != ice_out->cands[0][1].con_port) {

				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
								tmp2, ice_out->cands[0][0].transport, c2,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1),
								a_engine->local_sdp_ip, a_engine->local_sdp_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
								);
			}
		}



#ifdef GOOGLE_ICE
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ice-options:google-ice\r\n");
#endif
	}


	if (secure && !switch_channel_test_flag(session->channel, CF_DTLS)) {
		int i;

		for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
			switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;

			if ((a_engine->crypto_type == j || a_engine->crypto_type == CRYPTO_INVALID) && !zstr(a_engine->ssec[j].local_crypto_key)) {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=crypto:%s\r\n", a_engine->ssec[j].local_crypto_key);
			}
		}
		//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\r\n");
	}

	if (cng_type) {
		for (i = 0; i < smh->num_rates; i++) {
			//if (smh->rates[i] == 8000) {
			//	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d CN/%d\r\n", cng_type, smh->rates[i]);
			//} else {
				switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtpmap:%d CN/%d\r\n", smh->cng_ianacodes[i], smh->rates[i]);
				//}
		}
	} else {
		if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) {
			switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=silenceSupp:off - - - -\r\n");
		}
	}

	if (append_audio) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "%s%s", append_audio, end_of(append_audio) == '\n' ? "" : "\r\n");
	}

	if (!cur_ptime) {
		cur_ptime = ptime;
	}

	if (!noptime && cur_ptime) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=ptime:%d\r\n", cur_ptime);
	}

	local_sdp_audio_zrtp_hash = switch_core_media_get_zrtp_hash(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_TRUE);

	if (local_sdp_audio_zrtp_hash) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\n", local_sdp_audio_zrtp_hash);
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=zrtp-hash:%s\r\n", local_sdp_audio_zrtp_hash);
	}

	if (!zstr(sr)) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=%s\r\n", sr);
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

static void add_fb(char *buf, uint32_t buflen, int pt, int fir, int nack, int pli, int tmmbr)
{
	if (fir) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-fb:%d ccm fir\r\n", pt);
	}

	if (tmmbr) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-fb:%d ccm tmmbr\r\n", pt);
	}

	if (nack) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-fb:%d nack\r\n", pt);
	}

	if (pli) {
		switch_snprintf(buf + strlen(buf), buflen - strlen(buf), "a=rtcp-fb:%d nack pli\r\n", pt);
	}

}

//?
#define SDPBUFLEN 65536
SWITCH_DECLARE(void) switch_core_media_gen_local_sdp(switch_core_session_t *session, switch_sdp_type_t sdp_type, const char *ip, switch_port_t port, const char *sr, int force)
{
	char *buf;
	int ptime = 0;
	uint32_t rate = 0;
	uint32_t v_port, t_port;
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
	const char *local_sdp_text_zrtp_hash = switch_core_media_get_zrtp_hash(session, SWITCH_MEDIA_TYPE_TEXT, SWITCH_TRUE);
	const char *tmp;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;
	ice_t *ice_out;
	//int vp8 = 0;
	//int red = 0;
	payload_map_t *pmap;
	int is_outbound = switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND;
	const char *vbw;
	int bw = 256, i = 0;
	uint8_t fir = 0, nack = 0, pli = 0, tmmbr = 0, has_vid = 0;
	const char *use_rtcp_mux = NULL;
	int include_external;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	include_external = switch_channel_var_true(session->channel, "include_external_ip");

	use_rtcp_mux = switch_channel_get_variable(session->channel, "rtcp_mux");

	if (use_rtcp_mux && switch_false(use_rtcp_mux)) {
		a_engine->rtcp_mux = -1;
		v_engine->rtcp_mux = -1;
	}

	if ((a_engine->rtcp_mux != -1 && v_engine->rtcp_mux != -1) && (sdp_type == SDP_TYPE_REQUEST)) {
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
		if (!switch_channel_test_flag(session->channel, CF_AVPF) &&
			switch_true(switch_channel_get_variable(session->channel, "media_webrtc"))) {
			switch_channel_set_flag(session->channel, CF_AVPF);
			switch_channel_set_flag(session->channel, CF_ICE);
			smh->mparams->rtcp_audio_interval_msec = SWITCH_RTCP_AUDIO_INTERVAL_MSEC;
			smh->mparams->rtcp_video_interval_msec = SWITCH_RTCP_VIDEO_INTERVAL_MSEC;
		}

		if (switch_true(switch_channel_get_variable(session->channel, "add_ice_candidates"))) {
			switch_channel_set_flag(session->channel, CF_ICE);
		}

		if ( switch_rtp_has_dtls() && dtls_ok(session)) {
			if (switch_channel_test_flag(session->channel, CF_AVPF) ||
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
		/* it could be 98 but chrome reserves 98 and 99 for some internal stuff even though they should not.  
		   Everyone expects dtmf to be at 101 and Its not worth the trouble so we'll start at 102 */
		smh->payload_space = 102;
		memset(smh->rates, 0, sizeof(smh->rates));
		smh->num_rates = 0;

		for (i = 0; i < smh->mparams->num_codecs; i++) {
			int j;
			smh->ianacodes[i] = smh->codecs[i]->ianacode;

			if (smh->codecs[i]->codec_type != SWITCH_CODEC_TYPE_AUDIO) {
				continue;
			}

			if (sdp_type == SDP_TYPE_REQUEST) {
				for (j = 0; j < SWITCH_MAX_CODECS; j++) {
					if (smh->rates[j] == 0) {
						break;
					}

					if (smh->rates[j] == smh->codecs[i]->samples_per_second) {
						goto do_next;
					}
				}

				smh->rates[smh->num_rates++] = smh->codecs[i]->samples_per_second;
			}

		do_next:
			continue;
		}

		if (sdp_type == SDP_TYPE_REQUEST) {
			switch_core_session_t *orig_session = NULL;

			switch_core_session_get_partner(session, &orig_session);

			if (orig_session && !switch_channel_test_flag(session->channel, CF_ANSWERED)) {
				switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO,
											switch_core_session_remote_media_flow(orig_session, SWITCH_MEDIA_TYPE_AUDIO), sdp_type);
				switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO,
											switch_core_session_remote_media_flow(orig_session, SWITCH_MEDIA_TYPE_VIDEO), sdp_type);
			}

			for (i = 0; i < smh->mparams->num_codecs; i++) {
				const switch_codec_implementation_t *imp = smh->codecs[i];
				switch_payload_t orig_pt = 0;
				char *orig_fmtp = NULL;

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
															 imp->iananame, imp->samples_per_second, smh->fmtp[i], &orig_pt, NULL, &orig_fmtp) == SWITCH_STATUS_SUCCESS) {

						if (orig_pt == smh->mparams->te) {
							smh->mparams->te  = (switch_payload_t)smh->payload_space++;
						}

						smh->ianacodes[i] = orig_pt;

						if (!zstr(orig_fmtp)) {
							smh->fmtps[i] = switch_core_session_strdup(session, orig_fmtp);
						}
					} else {
						smh->ianacodes[i] = (switch_payload_t)smh->payload_space++;
					}
				}

				switch_core_media_add_payload_map(session,
												  imp->codec_type == SWITCH_CODEC_TYPE_AUDIO ? SWITCH_MEDIA_TYPE_AUDIO : SWITCH_MEDIA_TYPE_VIDEO,
												  imp->iananame,
												  imp->modname,
												  smh->fmtps[i],
												  sdp_type,
												  smh->ianacodes[i],
												  imp->samples_per_second,
												  imp->microseconds_per_packet / 1000,
												  imp->number_of_channels,
												  SWITCH_FALSE);
			}

			for (i = 0; i < smh->num_rates; i++) {
				if (smh->rates[i] == 8000 || smh->num_rates == 1) {
					smh->dtmf_ianacodes[i] = smh->mparams->te;
					smh->cng_ianacodes[i] = smh->mparams->cng_pt;
				} else {
					int j = 0;

					for (j = 0; j < smh->mparams->num_codecs; j++) {
						if (smh->ianacodes[j] == smh->payload_space) {
							smh->payload_space++;
							break;
						}
					}

					smh->dtmf_ianacodes[i] = (switch_payload_t)smh->payload_space++;
					smh->cng_ianacodes[i] = (switch_payload_t)smh->payload_space++;
				}
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

	for (i = 0; i < smh->mparams->num_codecs; i++) {
		const switch_codec_implementation_t *imp = smh->codecs[i];
		
		if (imp->codec_type == SWITCH_CODEC_TYPE_AUDIO && a_engine->smode == SWITCH_MEDIA_FLOW_DISABLED) {
			switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_MEDIA_FLOW_SENDRECV, sdp_type);
			break;
		}
	}
	
	if (zstr(sr)) {
		if (a_engine->smode == SWITCH_MEDIA_FLOW_SENDONLY) {
			sr = "sendonly";
		} else if (a_engine->smode == SWITCH_MEDIA_FLOW_RECVONLY) {
			sr = "recvonly";
		} else if (a_engine->smode == SWITCH_MEDIA_FLOW_INACTIVE) {
			sr = "inactive";
		} else {
			sr = "sendrecv";
		}

		if ((var_val = switch_channel_get_variable(session->channel, "origination_audio_mode"))) {
			if (!strcasecmp(sr, "sendonly") || !strcasecmp(sr, "recvonly") || !strcasecmp(sr, "sendrecv") || !strcasecmp(sr, "inactive")) {
				sr = var_val;
			}
			switch_channel_set_variable(session->channel, "origination_audio_mode", NULL);
		}

		if (zstr(sr)) {
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
			switch_snprintf(srbuf, sizeof(srbuf), "a=%s\r\n", sr);
		}
		sr = NULL;
	}

	family = strchr(ip, ':') ? "IP6" : "IP4";
	switch_snprintf(buf, SDPBUFLEN,
					"v=0\r\n"
					"o=%s %010u %010u IN %s %s\r\n"
					"s=%s\r\n"
					"c=IN %s %s\r\n"
					"t=0 0\r\n"
					"%s",
					username, smh->owner_id, smh->session_id, family, ip, username, family, ip, srbuf);

	if (switch_channel_test_flag(smh->session->channel, CF_ICE) && switch_channel_var_true(session->channel, "ice_lite")) {
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-lite\r\n");
	}

	if (a_engine->rmode == SWITCH_MEDIA_FLOW_DISABLED) {
		goto video;
	}

	if (switch_channel_test_flag(smh->session->channel, CF_ICE)) {
		gen_ice(session, SWITCH_MEDIA_TYPE_AUDIO, ip, port);
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=msid-semantic: WMS %s\r\n", smh->msid);
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

		if ((smh->mparams->dtmf_type == DTMF_2833 || switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF)) && smh->mparams->te > 95) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", smh->mparams->te);
		}

		if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && smh->mparams->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", smh->mparams->cng_pt);
		}

		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\r\n");


		rate = a_engine->cur_payload_map->adv_rm_rate;

		if (!a_engine->cur_payload_map->adv_channels) {
			a_engine->cur_payload_map->adv_channels = get_channels(a_engine->cur_payload_map->rm_encoding, 1);
		}

		if (a_engine->cur_payload_map->adv_channels > 1) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d/%d\r\n",
							a_engine->cur_payload_map->pt, a_engine->cur_payload_map->rm_encoding, rate, a_engine->cur_payload_map->adv_channels);
		} else {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\r\n",
							a_engine->cur_payload_map->pt, a_engine->cur_payload_map->rm_encoding, rate);
		}

		if (fmtp_out) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\r\n", a_engine->cur_payload_map->pt, fmtp_out);
		}

		if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_AUDIO)) {
			switch_mutex_lock(smh->sdp_mutex);
			for (pmap = a_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
				if (pmap->pt != a_engine->cur_payload_map->pt) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\r\n",
									pmap->pt, pmap->iananame,
									pmap->rate);
				}
			}
			switch_mutex_unlock(smh->sdp_mutex);
		}


		if (a_engine->read_codec.implementation && !ptime) {
			ptime = a_engine->read_codec.implementation->microseconds_per_packet / 1000;
		}


		if ((smh->mparams->dtmf_type == DTMF_2833 || switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))
			&& smh->mparams->te > 95) {

			if (switch_channel_test_flag(session->channel, CF_AVPF)) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d telephone-event/%d\r\n",
								smh->mparams->te, smh->mparams->te_rate);
			} else {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d telephone-event/%d\r\na=fmtp:%d 0-16\r\n",
								smh->mparams->te, smh->mparams->te_rate, smh->mparams->te);
			}
		}

		if (switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=silenceSupp:off - - - -\r\n");
		} else if (smh->mparams->cng_pt && use_cng) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d CN/%lu\r\n", smh->mparams->cng_pt, smh->mparams->cng_rate);

			if (!a_engine->codec_negotiated) {
				smh->mparams->cng_pt = 0;
			}
		}

		if (append_audio) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s%s", append_audio, end_of(append_audio) == '\n' ? "" : "\r\n");
		}

		if (ptime) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ptime:%d\r\n", ptime);
		}


		if (local_sdp_audio_zrtp_hash) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding audio a=zrtp-hash:%s\r\n",
							  local_sdp_audio_zrtp_hash);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\r\n",
							local_sdp_audio_zrtp_hash);
		}

		if (!zstr(sr)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=%s\r\n", sr);
		}


		if (!zstr(a_engine->local_dtls_fingerprint.type)) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fingerprint:%s %s\r\na=setup:%s\r\n",
							a_engine->local_dtls_fingerprint.type,
							a_engine->local_dtls_fingerprint.str, get_setup(a_engine, session, sdp_type));
		}

		if (smh->mparams->rtcp_audio_interval_msec) {
			if (a_engine->rtcp_mux > 0) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp-mux\r\n");
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", port, family, ip);
			} else {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", port + 1, family, ip);
			}
		}

		//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\r\n", a_engine->ssrc);

		if (a_engine->ice_out.cands[0][0].ready) {
			char tmp1[11] = "";
			char tmp2[11] = "";
			char tmp3[11] = "";
			uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
			//uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
			//uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
			//uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);

			uint32_t c2 = c1 - 1;
			uint32_t c3 = c1 - 2;
			uint32_t c4 = c1 - 3;

			tmp1[10] = '\0';
			tmp2[10] = '\0';
			tmp3[10] = '\0';
			switch_stun_random_string(tmp1, 10, "0123456789");
			switch_stun_random_string(tmp2, 10, "0123456789");
			switch_stun_random_string(tmp3, 10, "0123456789");

			ice_out = &a_engine->ice_out;


			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-ufrag:%s\r\n", ice_out->ufrag);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-pwd:%s\r\n", ice_out->pwd);


			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
							tmp1, ice_out->cands[0][0].transport, c1,
							ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
							);

			if (include_external && !zstr(smh->mparams->extsipip)) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
					tmp3, ice_out->cands[0][0].transport, c1,
					smh->mparams->extsipip, ice_out->cands[0][0].con_port
					);
			}

			if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
				strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
				&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
								tmp2, ice_out->cands[0][0].transport, c3,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
								a_engine->local_sdp_ip, a_engine->local_sdp_port
								);
			}


			if (a_engine->rtcp_mux < 1 || is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING)) {

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
								tmp1, ice_out->cands[0][0].transport, c2,
								ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
								);

				if (include_external && !zstr(smh->mparams->extsipip)) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
						tmp3, ice_out->cands[0][0].transport, c2,
						smh->mparams->extsipip, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
						);
				}



				if (!zstr(a_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) &&
					strcmp(a_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
					&& a_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
									tmp2, ice_out->cands[0][0].transport, c4,
									ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (a_engine->rtcp_mux > 0 ? 0 : 1),
									a_engine->local_sdp_ip, a_engine->local_sdp_port + (a_engine->rtcp_mux > 0 ? 0 : 1)
									);
				}
			}

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=end-of-candidates\r\n");

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u cname:%s\r\n", a_engine->ssrc, smh->cname);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u msid:%s a0\r\n", a_engine->ssrc, smh->msid);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u mslabel:%s\r\n", a_engine->ssrc, smh->msid);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u label:%sa0\r\n", a_engine->ssrc, smh->msid);


#ifdef GOOGLE_ICE
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-options:google-ice\r\n");
#endif
		}

		if (a_engine->crypto_type != CRYPTO_INVALID && !switch_channel_test_flag(session->channel, CF_DTLS) &&
			!zstr(a_engine->ssec[a_engine->crypto_type].local_crypto_key) && switch_channel_test_flag(session->channel, CF_SECURE)) {

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\r\n", a_engine->ssec[a_engine->crypto_type].local_crypto_key);
		//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=encryption:optional\r\n");
		}

		if (a_engine->reject_avp) {
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=audio 0 RTP/AVP 19\r\n");
		}

	} else if (smh->mparams->num_codecs) {
		int cur_ptime = 0, this_ptime = 0, cng_type = 0;
		const char *mult;


		if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && smh->mparams->cng_pt && use_cng) {
			cng_type = smh->mparams->cng_pt;

			if (!a_engine->codec_negotiated) {
				smh->mparams->cng_pt = 0;
			}
		}

		mult = switch_channel_get_variable(session->channel, "sdp_m_per_ptime");

		if (switch_channel_test_flag(session->channel, CF_AVPF) || (mult && switch_false(mult))) {
			char *bp = buf;
			int both = (switch_channel_test_flag(session->channel, CF_AVPF) || switch_channel_test_flag(session->channel, CF_DTLS)) ? 0 : 1;

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

					if (switch_channel_test_flag(session->channel, CF_AVPF) || switch_channel_test_flag(session->channel, CF_DTLS)) {
						both = 0;
					}

					if (both) {
						generate_m(session, bp, SDPBUFLEN - strlen(buf), port, family, ip, cur_ptime, append_audio, sr, use_cng, cng_type, map, 0, sdp_type);
					}
				}

			}
		}

	}

	if (switch_channel_test_flag(session->channel, CF_IMAGE_SDP)) {
		switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=image 0 UDPTL T38\r\n", SWITCH_VA_NONE);

	}


 video:


	if (!switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE) && switch_media_handle_test_media_flag(smh, SCMF_RECV_SDP)) {
		has_vid = 0;
	} else {
		for (i = 0; i < smh->mparams->num_codecs; i++) {
			const switch_codec_implementation_t *imp = smh->codecs[i];


			if (imp->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				has_vid = 1;
				break;
			}
		}

	}


	if (!has_vid) {
		if (switch_channel_test_flag(session->channel, CF_VIDEO_SDP_RECVD)) {
			switch_channel_clear_flag(session->channel, CF_VIDEO_SDP_RECVD);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=video 0 %s 19\r\n",
							get_media_profile_name(session,
												   (switch_channel_test_flag(session->channel, CF_SECURE)
													&& switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) ||
												   a_engine->crypto_type != CRYPTO_INVALID || switch_channel_test_flag(session->channel, CF_DTLS)));
		}
	} else {
		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if (switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
				v_engine->no_crypto = 1;
			}
		}


		if (!v_engine->local_sdp_port) {
			switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 0);
		}

		//if (switch_channel_test_flag(session->channel, CF_AVPF)) {
		//	switch_media_handle_set_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO);
		//}

		if ((v_port = v_engine->adv_sdp_port)) {
			int loops;
			int got_vid = 0;

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
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", v_engine->cur_payload_map->pt);

					if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO)) {
						switch_mutex_lock(smh->sdp_mutex);
						for (pmap = v_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
							if (pmap->pt != v_engine->cur_payload_map->pt && pmap->negotiated) {
								switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", pmap->pt);
							}
						}
						switch_mutex_unlock(smh->sdp_mutex);
					}
					
				} else if (smh->mparams->num_codecs) {
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

						got_vid++;
					}					
				}
				if (got_vid && v_engine->smode == SWITCH_MEDIA_FLOW_DISABLED) {
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_MEDIA_FLOW_SENDRECV, sdp_type);
				}
				
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\r\n");


				if (!(vbw = switch_channel_get_variable(smh->session->channel, "rtp_video_max_bandwidth"))) {
					vbw = switch_channel_get_variable(smh->session->channel, "rtp_video_max_bandwidth_in");
				}

				if (!vbw) {
					vbw = "1mb";
				}

				bw = switch_parse_bandwidth_string(vbw);

				if (bw > 0) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "b=AS:%d\r\n", bw);
					//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "b=TIAS:%d\r\n", bw);
				}



				if (v_engine->codec_negotiated) {
					payload_map_t *pmap;

					//if (!strcasecmp(v_engine->cur_payload_map->rm_encoding, "VP8")) {
					//	vp8 = v_engine->cur_payload_map->pt;
					//}

					//if (!strcasecmp(v_engine->cur_payload_map->rm_encoding, "red")) {
					//	red = v_engine->cur_payload_map->pt;
					//}

					rate = v_engine->cur_payload_map->rm_rate;
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\r\n",
									v_engine->cur_payload_map->pt, v_engine->cur_payload_map->rm_encoding,
									v_engine->cur_payload_map->rm_rate);

					if (switch_channel_test_flag(session->channel, CF_RECOVERING)) {
						pass_fmtp = v_engine->cur_payload_map->rm_fmtp;
					} else {

						pass_fmtp = NULL;

						if (ov_fmtp) {
							pass_fmtp = ov_fmtp;
						} else {

							pass_fmtp = v_engine->cur_payload_map->fmtp_out;

							if (!pass_fmtp || switch_true(switch_channel_get_variable_dup(session->channel, "rtp_mirror_fmtp", SWITCH_FALSE, -1))) {
								pass_fmtp = switch_channel_get_variable(session->channel, "rtp_video_fmtp");
							}
						}
					}


					if (pass_fmtp) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\r\n", v_engine->cur_payload_map->pt, pass_fmtp);
					}


					if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO)) {
						switch_mutex_lock(smh->sdp_mutex);
						for (pmap = v_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
							if (pmap->pt != v_engine->cur_payload_map->pt && pmap->negotiated) {
								switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\r\n",
												pmap->pt, pmap->iananame, pmap->rate);
							}
						}
						switch_mutex_unlock(smh->sdp_mutex);
					}


					if (append_video) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s%s", append_video, end_of(append_video) == '\n' ? "" : "\r\n");
					}
					
				} else if (smh->mparams->num_codecs) {
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

						//if (!strcasecmp(imp->iananame, "VP8")) {
						//	vp8 = ianacode;
						//}

						//if (!strcasecmp(imp->iananame, "red")) {
						//		red = ianacode;
						//}

						if (channels > 1) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d/%d\r\n", ianacode, imp->iananame,
											imp->samples_per_second, channels);
						} else {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%d\r\n", ianacode, imp->iananame,
											imp->samples_per_second);
						}



						if (!zstr(ov_fmtp)) {
							fmtp = (char *) ov_fmtp;
						} else {

							if (map) {
								fmtp = switch_event_get_header(map, imp->iananame);
							}

							if (!zstr(smh->fmtp[i])) {
								fmtp = smh->fmtp[i];
							} else if (smh->fmtps[i]) {
								fmtp = smh->fmtps[i];
							}

							if (zstr(fmtp)) fmtp = imp->fmtp;

							if (zstr(fmtp)) fmtp = (char *) pass_fmtp;
						}

						if (!zstr(fmtp) && strcasecmp(fmtp, "_blank_")) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %s\r\n", ianacode, fmtp);
						}
					}

				}
				
				if (v_engine->smode == SWITCH_MEDIA_FLOW_SENDRECV) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=sendrecv\r\n");
				} else if (v_engine->smode == SWITCH_MEDIA_FLOW_SENDONLY) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=sendonly\r\n");
				} else if (v_engine->smode == SWITCH_MEDIA_FLOW_RECVONLY) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=recvonly\r\n");
				} else if (v_engine->smode == SWITCH_MEDIA_FLOW_INACTIVE) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=inactive\r\n");
				}


				if ((is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING))
					&& switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
					generate_local_fingerprint(smh, SWITCH_MEDIA_TYPE_VIDEO);
				}


				if (!zstr(v_engine->local_dtls_fingerprint.type)) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fingerprint:%s %s\r\na=setup:%s\r\n",
									v_engine->local_dtls_fingerprint.type, v_engine->local_dtls_fingerprint.str, get_setup(v_engine, session, sdp_type));
				}


				if (smh->mparams->rtcp_video_interval_msec) {
					if (v_engine->rtcp_mux > 0) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp-mux\r\n");
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", v_port, family, ip);
					} else {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", v_port + 1, family, ip);
					}
				}

				if (sdp_type == SDP_TYPE_REQUEST) {
					fir++;
					pli++;
					nack++;
					tmmbr++;
				}

				/* DFF nack pli etc */
				//nack = v_engine->nack = 0;
				//pli = v_engine->pli = 0;


				if (v_engine->codec_negotiated) {
					add_fb(buf, SDPBUFLEN, v_engine->cur_payload_map->pt, v_engine->fir || fir,
						   v_engine->nack || nack, v_engine->pli || pli, v_engine->tmmbr || tmmbr);

					if (switch_media_handle_test_media_flag(smh, SCMF_MULTI_ANSWER_VIDEO)) {
						switch_mutex_lock(smh->sdp_mutex);
						for (pmap = v_engine->cur_payload_map; pmap && pmap->allocated; pmap = pmap->next) {
							if (pmap->pt != v_engine->cur_payload_map->pt && pmap->negotiated) {
								add_fb(buf, SDPBUFLEN, pmap->pt, v_engine->fir || fir, v_engine->nack || nack, v_engine->pli || pli, v_engine->tmmbr || tmmbr);
							}
						}
						switch_mutex_unlock(smh->sdp_mutex);
					}

				} else if (smh->mparams->num_codecs) {
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

						add_fb(buf, SDPBUFLEN, smh->ianacodes[i], v_engine->fir || fir, v_engine->nack || nack, v_engine->pli || pli, v_engine->pli || pli);
					}

				}

				//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\r\n", v_engine->ssrc);

				if (v_engine->ice_out.cands[0][0].ready) {
					char tmp1[11] = "";
					char tmp2[11] = "";
					char tmp3[11] = "";
					uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
					//uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
					//uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
					//uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);

					uint32_t c2 = c1 - 1;
					uint32_t c3 = c1 - 2;
					uint32_t c4 = c1 - 3;

					tmp1[10] = '\0';
					tmp2[10] = '\0';
					tmp3[10] = '\0';
					switch_stun_random_string(tmp1, 10, "0123456789");
					switch_stun_random_string(tmp2, 10, "0123456789");
					switch_stun_random_string(tmp3, 10, "0123456789");

					ice_out = &v_engine->ice_out;


					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u cname:%s\r\n", v_engine->ssrc, smh->cname);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u msid:%s v0\r\n", v_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u mslabel:%s\r\n", v_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u label:%sv0\r\n", v_engine->ssrc, smh->msid);



					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-ufrag:%s\r\n", ice_out->ufrag);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-pwd:%s\r\n", ice_out->pwd);


					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
									tmp1, ice_out->cands[0][0].transport, c1,
									ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
									);

					if (include_external && !zstr(smh->mparams->extsipip)) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
							tmp3, ice_out->cands[0][0].transport, c1,
							smh->mparams->extsipip, ice_out->cands[0][0].con_port
							);
					}

					if (!zstr(v_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) && 
						strcmp(v_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
						&& v_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
										tmp2, ice_out->cands[0][0].transport, c3,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
										v_engine->local_sdp_ip, v_engine->local_sdp_port
										);
					}


					if (v_engine->rtcp_mux < 1 || is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING)) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
										tmp1, ice_out->cands[0][0].transport, c2,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (v_engine->rtcp_mux > 0 ? 0 : 1)
										);

					if (include_external && !zstr(smh->mparams->extsipip)) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
								tmp3, ice_out->cands[0][0].transport, c2,
								smh->mparams->extsipip, ice_out->cands[0][0].con_port + (v_engine->rtcp_mux > 0 ? 0 : 1)
								);
						}


						if (!zstr(v_engine->local_sdp_ip) && !zstr(ice_out->cands[0][1].con_addr) && 
							strcmp(v_engine->local_sdp_ip, ice_out->cands[0][1].con_addr)
							&& v_engine->local_sdp_port != ice_out->cands[0][1].con_port) {

							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx generation 0\r\n",
											tmp2, ice_out->cands[0][0].transport, c4,
											ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (v_engine->rtcp_mux > 0 ? 0 : 1),
											v_engine->local_sdp_ip, v_engine->local_sdp_port + (v_engine->rtcp_mux > 0 ? 0 : 1)
											);
						}
					}



#ifdef GOOGLE_ICE
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-options:google-ice\r\n");
#endif
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=end-of-candidates\r\n");

				}



				if (loops == 0 && switch_channel_test_flag(session->channel, CF_SECURE) && !switch_channel_test_flag(session->channel, CF_DTLS)) {

					for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
						switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;

						if ((a_engine->crypto_type == j || a_engine->crypto_type == CRYPTO_INVALID) && !zstr(a_engine->ssec[j].local_crypto_key)) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\r\n", v_engine->ssec[j].local_crypto_key);
						}
					}
					//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\r\n");
				}


				if (local_sdp_video_zrtp_hash) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding video a=zrtp-hash:%s\n", local_sdp_video_zrtp_hash);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\r\n", local_sdp_video_zrtp_hash);
				}


				if (switch_channel_test_flag(session->channel, CF_DTLS) ||
					!switch_channel_test_flag(session->channel, CF_SECURE) ||
					smh->crypto_mode == CRYPTO_MODE_MANDATORY || smh->crypto_mode == CRYPTO_MODE_FORBIDDEN) {
					break;
				}
			}
		}

	}

	if (switch_channel_test_cap(session->channel, CC_MSRP) && !smh->msrp_session) {
		int want_msrp = switch_channel_var_true(session->channel, "sip_enable_msrp");
		int want_msrps = switch_channel_var_true(session->channel, "sip_enable_msrps");

		if (!want_msrp) {
			want_msrp = switch_channel_test_flag(session->channel, CF_WANT_MSRP);
		}

		if (!want_msrps) {
			want_msrps = switch_channel_test_flag(session->channel, CF_WANT_MSRPS);
		}

		if (want_msrp || want_msrps) {
			smh->msrp_session = switch_msrp_session_new(switch_core_session_get_pool(session), switch_core_session_get_uuid(session), want_msrps);

			switch_assert(smh->msrp_session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP session created %s\n", smh->msrp_session->call_id);

			switch_channel_set_flag(session->channel, CF_HAS_TEXT);
			switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
			switch_channel_set_flag(session->channel, CF_TEXT_LINE_BASED);
			switch_channel_set_flag(session->channel, CF_MSRP);

			if (want_msrps) {
				switch_channel_set_flag(session->channel, CF_MSRPS);
			}

			switch_core_session_start_text_thread(session);
		}
	}

	if (smh->msrp_session) {
		switch_msrp_session_t *msrp_session = smh->msrp_session;

		if (!zstr(msrp_session->remote_path)) {
			if (zstr(msrp_session->local_path)) {
				msrp_session->local_path = switch_core_session_sprintf(session,
					"msrp%s://%s:%d/%s;tcp",
					msrp_session->secure ? "s" : "",
					ip, msrp_session->local_port, msrp_session->call_id);
			}

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf),
				"m=message %d TCP/%sMSRP *\r\n"
				"a=path:%s\r\n"
				"a=accept-types:%s\r\n"
				"a=accept-wrapped-types:%s\r\n"
				"a=setup:%s\r\n",
				msrp_session->local_port,
				msrp_session->secure ? "TLS/" : "",
				msrp_session->local_path,
				msrp_session->local_accept_types,
				msrp_session->local_accept_wrapped_types,
				msrp_session->active ? "active" : "passive");
		} else {
			char *uuid = switch_core_session_get_uuid(session);
			const char *file_selector = switch_channel_get_variable(session->channel, "sip_msrp_local_file_selector");
			const char *msrp_offer_active = switch_channel_get_variable(session->channel, "sip_msrp_offer_active");

			if (switch_true(msrp_offer_active)) {
				msrp_session->active = 1;
				// switch_msrp_start_client(msrp_session);
			}

			if (zstr(msrp_session->local_path)) {
				msrp_session->local_path = switch_core_session_sprintf(session,
					"msrp%s://%s:%d/%s;tcp",
					msrp_session->secure ? "s" : "",
					ip, msrp_session->local_port, uuid);
			}

			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf),
				"m=message %d TCP/%sMSRP *\r\n"
				"a=path:%s\r\n"
				"a=accept-types:message/cpim text/* application/im-iscomposing+xml\r\n"
				"a=accept-wrapped-types:*\r\n"
				"a=setup:%s\r\n",
				msrp_session->local_port,
				msrp_session->secure ? "TLS/" : "",
				msrp_session->local_path,
				msrp_session->active ? "active" : "passive");

			if (!zstr(file_selector)) {
				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf),
					"a=sendonly\r\na=file-selector:%s\r\n", file_selector);
			}
		}
	}

	// RTP TEXT

	if (sdp_type == SDP_TYPE_RESPONSE && !switch_channel_test_flag(session->channel, CF_RTT)) {
		if (switch_channel_test_flag(session->channel, CF_TEXT_SDP_RECVD)) {
			switch_channel_clear_flag(session->channel, CF_TEXT_SDP_RECVD);
			switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=text 0 %s 19\r\n",
							get_media_profile_name(session,
												   (switch_channel_test_flag(session->channel, CF_SECURE)
													&& switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) ||
												   a_engine->crypto_type != CRYPTO_INVALID || switch_channel_test_flag(session->channel, CF_DTLS)));
		}
	} else if ((switch_channel_test_flag(session->channel, CF_WANT_RTT) || switch_channel_test_flag(session->channel, CF_RTT) ||
				switch_channel_var_true(session->channel, "rtp_enable_text")) &&
			   switch_channel_test_cap(session->channel, CC_RTP_RTT)) {
		t_engine->t140_pt = 0;
		t_engine->red_pt = 0;

		if (sdp_type == SDP_TYPE_REQUEST) {
			t_engine->t140_pt = 96;
			t_engine->red_pt = 97;

			switch_core_media_add_payload_map(session,
											  SWITCH_MEDIA_TYPE_TEXT,
											  "red",
											  NULL,
											  NULL,
											  SDP_TYPE_REQUEST,
											  t_engine->red_pt,
											  1000,
											  0,
											  1,
											  SWITCH_TRUE);

			switch_core_media_add_payload_map(session,
											  SWITCH_MEDIA_TYPE_TEXT,
											  "t140",
											  NULL,
											  NULL,
											  SDP_TYPE_REQUEST,
											  t_engine->t140_pt,
											  1000,
											  0,
											  1,
											  SWITCH_TRUE);

			t_engine->codec_negotiated = 1;
		}

		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			if (switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
				t_engine->no_crypto = 1;
			}
		}


		if (!t_engine->local_sdp_port) {
			switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_TEXT, 0);
		}

		if ((t_port = t_engine->adv_sdp_port)) {
			int loops;

			for (loops = 0; loops < 2; loops++) {

				if (switch_channel_test_flag(smh->session->channel, CF_ICE)) {
					gen_ice(session, SWITCH_MEDIA_TYPE_TEXT, ip, (switch_port_t)t_port);
				}


				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "m=text %d %s",
								t_port,
								get_media_profile_name(session,
													   (loops == 0 && switch_channel_test_flag(session->channel, CF_SECURE)
														&& switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) ||
													   a_engine->crypto_type != CRYPTO_INVALID || switch_channel_test_flag(session->channel, CF_DTLS)));


				/*****************************/
				if (t_engine->codec_negotiated) {

					switch_mutex_lock(smh->sdp_mutex);
					for (pmap = t_engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {

						if (pmap->type != SWITCH_MEDIA_TYPE_TEXT || !pmap->negotiated) {
							continue;
						}

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), " %d", pmap->pt);

					}
					switch_mutex_unlock(smh->sdp_mutex);
				} else {
					switch_core_media_set_smode(smh->session, SWITCH_MEDIA_TYPE_TEXT, SWITCH_MEDIA_FLOW_SENDRECV, sdp_type);
				}

				switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "\r\n");

				if (t_engine->codec_negotiated) {
					switch_mutex_lock(smh->sdp_mutex);
					for (pmap = t_engine->payload_map; pmap && pmap->allocated; pmap = pmap->next) {

						if (pmap->type != SWITCH_MEDIA_TYPE_TEXT || !pmap->negotiated) {
							continue;
						}

						if (!strcasecmp(pmap->iananame, "t140")) {
							t_engine->t140_pt = pmap->pt;
						}

						if (!strcasecmp(pmap->iananame, "red")) {
							t_engine->red_pt = pmap->pt;
						}

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtpmap:%d %s/%ld\r\n",
										pmap->pt, pmap->iananame, pmap->rate);

					}
					switch_mutex_unlock(smh->sdp_mutex);


					if (t_engine->t140_pt && t_engine->red_pt) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fmtp:%d %d/%d/%d\r\n", t_engine->red_pt, t_engine->t140_pt, t_engine->t140_pt, t_engine->t140_pt);
					}


					if (t_engine->smode == SWITCH_MEDIA_FLOW_SENDONLY) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=sendonly\r\n");
					} else if (t_engine->smode == SWITCH_MEDIA_FLOW_RECVONLY) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "%s", "a=recvonly\r\n");
					}

				}

				if ((is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING))
					&& switch_channel_test_flag(smh->session->channel, CF_DTLS)) {
					generate_local_fingerprint(smh, SWITCH_MEDIA_TYPE_TEXT);
				}


				if (!zstr(t_engine->local_dtls_fingerprint.type)) {
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=fingerprint:%s %s\r\na=setup:%s\r\n", t_engine->local_dtls_fingerprint.type,
									t_engine->local_dtls_fingerprint.str, get_setup(t_engine, session, sdp_type));
				}


				if (smh->mparams->rtcp_text_interval_msec) {
					if (t_engine->rtcp_mux > 0) {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp-mux\r\n");
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", t_port, family, ip);
					} else {
						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=rtcp:%d IN %s %s\r\n", t_port + 1, family, ip);
					}
				}

				//switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u\r\n", t_engine->ssrc);

				if (t_engine->ice_out.cands[0][0].ready) {
					char tmp1[11] = "";
					char tmp2[11] = "";
					uint32_t c1 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 1);
					//uint32_t c2 = (2^24)*126 + (2^8)*65535 + (2^0)*(256 - 2);
					//uint32_t c3 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 1);
					//uint32_t c4 = (2^24)*126 + (2^8)*65534 + (2^0)*(256 - 2);

					uint32_t c2 = c1 - 1;
					uint32_t c3 = c1 - 2;
					uint32_t c4 = c1 - 3;

					tmp1[10] = '\0';
					tmp2[10] = '\0';
					switch_stun_random_string(tmp1, 10, "0123456789");
					switch_stun_random_string(tmp2, 10, "0123456789");

					ice_out = &t_engine->ice_out;


					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u cname:%s\r\n", t_engine->ssrc, smh->cname);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u msid:%s v0\r\n", t_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u mslabel:%s\r\n", t_engine->ssrc, smh->msid);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ssrc:%u label:%sv0\r\n", t_engine->ssrc, smh->msid);



					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-ufrag:%s\r\n", ice_out->ufrag);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-pwd:%s\r\n", ice_out->pwd);


					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ host generation 0\r\n",
									tmp1, ice_out->cands[0][0].transport, c1,
									ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port
									);

					if (!zstr(t_engine->local_sdp_ip) && !zstr(ice_out->cands[0][0].con_addr) &&
						strcmp(t_engine->local_sdp_ip, ice_out->cands[0][0].con_addr)
						&& t_engine->local_sdp_port != ice_out->cands[0][0].con_port) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 1 %s %u %s %d typ srflx raddr %s rport %d generation 0\r\n",
										tmp2, ice_out->cands[0][0].transport, c3,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port,
										t_engine->local_sdp_ip, t_engine->local_sdp_port
										);
					}


					if (t_engine->rtcp_mux < 1 || is_outbound || switch_channel_test_flag(session->channel, CF_RECOVERING)) {

						switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ host generation 0\r\n",
										tmp1, ice_out->cands[0][0].transport, c2,
										ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (t_engine->rtcp_mux > 0 ? 0 : 1)
										);


						if (!zstr(t_engine->local_sdp_ip) && !zstr(ice_out->cands[0][1].con_addr) &&
							strcmp(t_engine->local_sdp_ip, ice_out->cands[0][1].con_addr)
							&& t_engine->local_sdp_port != ice_out->cands[0][1].con_port) {

							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=candidate:%s 2 %s %u %s %d typ srflx generation 0\r\n",
											tmp2, ice_out->cands[0][0].transport, c4,
											ice_out->cands[0][0].con_addr, ice_out->cands[0][0].con_port + (t_engine->rtcp_mux > 0 ? 0 : 1),
											t_engine->local_sdp_ip, t_engine->local_sdp_port + (t_engine->rtcp_mux > 0 ? 0 : 1)
											);
						}
					}



#ifdef GOOGLE_ICE
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=ice-options:google-ice\r\n");
#endif
				}



				if (loops == 0 && switch_channel_test_flag(session->channel, CF_SECURE) && !switch_channel_test_flag(session->channel, CF_DTLS)) {

					for (i = 0; smh->crypto_suite_order[i] != CRYPTO_INVALID; i++) {
						switch_rtp_crypto_key_type_t j = SUITES[smh->crypto_suite_order[i]].type;

						if ((t_engine->crypto_type == j || t_engine->crypto_type == CRYPTO_INVALID) && !zstr(t_engine->ssec[j].local_crypto_key)) {
							switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=crypto:%s\r\n", t_engine->ssec[j].local_crypto_key);
						}
					}
					//switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "a=encryption:optional\r\n");
				}


				if (local_sdp_text_zrtp_hash) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Adding text a=zrtp-hash:%s\n", local_sdp_text_zrtp_hash);
					switch_snprintf(buf + strlen(buf), SDPBUFLEN - strlen(buf), "a=zrtp-hash:%s\r\n", local_sdp_text_zrtp_hash);
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

	check_stream_changes(session, NULL, sdp_type);

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

static switch_bool_t stream_rejected(switch_media_handle_t *smh, sdp_media_e st)
{
	int x;

	for (x = 0; x < smh->rej_idx; x++) {
		if (smh->rejected_streams[x] == st) {
			return SWITCH_TRUE;
		}
	}

	return SWITCH_FALSE;
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
	const char *bit_removal_on = "a=T38FaxFillBitRemoval\r\n";
	const char *bit_removal_off = "";

	const char *mmr_on = "a=T38FaxTranscodingMMR\r\n";
	const char *mmr_off = "";

	const char *jbig_on = "a=T38FaxTranscodingJBIG\r\n";
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

	switch_channel_clear_flag(session->channel, CF_IMAGE_SDP);

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
					"v=0\r\n"
					"o=%s %010u %010u IN %s %s\r\n"
					"s=%s\r\n" "c=IN %s %s\r\n" "t=0 0\r\n", username, smh->owner_id, smh->session_id, family, ip, username, family, ip);

	if (t38_options->T38FaxMaxBuffer) {
		switch_snprintf(max_buf, sizeof(max_buf), "a=T38FaxMaxBuffer:%d\r\n", t38_options->T38FaxMaxBuffer);
	};

	if (t38_options->T38FaxMaxDatagram) {
		switch_snprintf(max_data, sizeof(max_data), "a=T38FaxMaxDatagram:%d\r\n", t38_options->T38FaxMaxDatagram);
	};




	if (broken_boolean) {
		bit_removal_on = "a=T38FaxFillBitRemoval:1\r\n";
		bit_removal_off = "a=T38FaxFillBitRemoval:0\r\n";

		mmr_on = "a=T38FaxTranscodingMMR:1\r\n";
		mmr_off = "a=T38FaxTranscodingMMR:0\r\n";

		jbig_on = "a=T38FaxTranscodingJBIG:1\r\n";
		jbig_off = "a=T38FaxTranscodingJBIG:0\r\n";

	}

	if (stream_rejected(smh, sdp_media_audio)) {
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
						"m=audio 0 RTP/AVP 0\r\n");
	}

	switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
					"m=image %d udptl t38\r\n"
					"a=T38FaxVersion:%d\r\n"
					"a=T38MaxBitRate:%d\r\n"
					"%s"
					"%s"
					"%s"
					"a=T38FaxRateManagement:%s\r\n"
					"%s"
					"%s"
					"a=T38FaxUdpEC:%s\r\n",
					//"a=T38VendorInfo:%s\r\n",
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
		switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "m=audio 0 RTP/AVP 19\r\n");
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
	int has_video = 0, has_audio = 0, has_text = 0, has_ip = 0;
	char port_buf[25] = "";
	char vport_buf[25] = "";
	char tport_buf[25] = "";
	char *new_sdp;
	int bad = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	payload_map_t *pmap;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (zstr(smh->mparams->local_sdp_str)) {
		return;
	}

	len = strlen(smh->mparams->local_sdp_str) * 2;

	if (!(smh->mparams->ndlb & SM_NDLB_NEVER_PATCH_REINVITE)) {
		if (switch_channel_test_flag(session->channel, CF_ANSWERED) &&
			(switch_stristr("sendonly", smh->mparams->local_sdp_str) || switch_stristr("inactive", smh->mparams->local_sdp_str) || switch_stristr("0.0.0.0", smh->mparams->local_sdp_str))) {
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

		switch_channel_set_flag(session->channel, CF_AUDIO);

		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "PROXY",
												 NULL,
												 NULL,
												 SDP_TYPE_RESPONSE,
												 0,
												 8000,
												 20,
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
			memcpy(q, strchr(a_engine->adv_sdp_ip, ':') ? "6 " : "4 ", 2);
			p +=2;
			q +=2;
			snprintf(q, qe - q, "%s", a_engine->adv_sdp_ip);
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


				snprintf(o_line, sizeof(o_line), "o=%s %010u %010u IN %s %s\r\n",
						 smh->mparams->sdp_username, smh->owner_id, smh->session_id, family, smh->mparams->sipip);

				snprintf(q, qe-q, "%s", o_line);
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

				snprintf(s_line, sizeof(s_line), "s=%s\r\n", smh->mparams->sdp_username);
				snprintf(q, qe-q, "%s", s_line);

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


			snprintf(q, qe - q, "%s", port_buf);
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
														 SWITCH_MEDIA_TYPE_VIDEO,
														 "PROXY-VID",
														 NULL,
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

				v_engine->codec_negotiated = 1;
				switch_core_media_set_video_codec(session, SWITCH_FALSE);
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

			snprintf(q, qe-q, "%s", vport_buf);
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
		} else if (!strncmp("m=text ", p, 8) && *(p + 8) != '0') {
			if (!has_text) {
				switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_TEXT, 1);
				clear_pmaps(t_engine);
				pmap = switch_core_media_add_payload_map(session,
														 SWITCH_MEDIA_TYPE_TEXT,
														 "PROXY-TXT",
														 NULL,
														 NULL,
														 SDP_TYPE_RESPONSE,
														 0,
														 90000,
														 90000,
														 1,
														 SWITCH_TRUE);
				t_engine->cur_payload_map = pmap;

				switch_snprintf(tport_buf, sizeof(tport_buf), "%u", t_engine->adv_sdp_port);

				if (switch_channel_media_ready(session->channel) && !switch_rtp_ready(t_engine->rtp_session)) {
					switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
					switch_channel_set_flag(session->channel, CF_REINVITE);
					switch_core_media_activate_rtp(session);
				}

				t_engine->codec_negotiated = 1;
				//TEXT switch_core_media_set_text_codec(session, SWITCH_FALSE);
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

			snprintf(q, qe-q, "%s", tport_buf);
			q += strlen(tport_buf);

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

			has_text++;
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
			if (!switch_media_handle_test_media_flag(smh, SCMF_DISABLE_RTP_AUTOADJ) && !switch_channel_test_flag(session->channel, CF_AVPF) &&
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

static int check_engine(switch_rtp_engine_t *engine)
{
	dtls_state_t dtls_state = switch_rtp_dtls_state(engine->rtp_session, DTLS_TYPE_RTP);
	int flags = 0;
	switch_status_t status;

	if (dtls_state == DS_READY || dtls_state >= DS_FAIL) return 0;

	status = switch_rtp_zerocopy_read_frame(engine->rtp_session, &engine->read_frame, flags);

	if (!SWITCH_READ_ACCEPTABLE(status)) {
		return 0;
	}

	return 1;
}

SWITCH_DECLARE(switch_bool_t) switch_core_media_check_dtls(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	int checking = 0;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_FALSE;
	}

	if (!switch_channel_media_up(session->channel)) {
		return SWITCH_FALSE;
	}

	if (!switch_channel_test_flag(session->channel, CF_DTLS)) {
		return SWITCH_TRUE;
	}

	engine = &smh->engines[type];

	if (engine->rmode == SWITCH_MEDIA_FLOW_DISABLED) {
		return SWITCH_TRUE;
	}

	do {
		if (engine->rtp_session) checking = check_engine(engine);
	} while (switch_channel_ready(session->channel) && checking);

	if (!checking) {
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}

SWITCH_DECLARE(uint32_t) switch_core_media_get_orig_bitrate(switch_core_session_t *session, switch_media_type_t type) 
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	if (switch_channel_down(session->channel)) {
		return 0;
	}

	engine = &smh->engines[type];

	if (engine) {
		return engine->orig_bitrate;
	} else {
		return 0;
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_media_set_outgoing_bitrate(switch_core_session_t *session, switch_media_type_t type, uint32_t bitrate)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t new_bitrate;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	new_bitrate = bitrate - bitrate * engine->bw_mult;
	if (switch_core_codec_ready(&engine->write_codec)) {
		status = switch_core_codec_control(&engine->write_codec, SCC_VIDEO_BANDWIDTH,
										   SCCT_INT, &new_bitrate, SCCT_NONE, NULL, NULL, NULL);
	}
	engine->orig_bitrate = bitrate;

	return status;
}

SWITCH_DECLARE(float) switch_core_media_get_media_bw_mult(switch_core_session_t *session) 
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	if (switch_channel_down(session->channel)) {
		return 0;
	}

	engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (engine) {
		return engine->bw_mult;
	}
	return 0;
}

SWITCH_DECLARE(void) switch_core_media_set_media_bw_mult(switch_core_session_t *session, float mult)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_channel_down(session->channel)) {
		return;
	}

	engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (engine) {
		engine->bw_mult = mult;
	}
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_reset_jb(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *engine;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	if (switch_rtp_ready(engine->rtp_session)) {
		switch_rtp_reset_jb(engine->rtp_session);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

//?
SWITCH_DECLARE(switch_status_t) switch_core_media_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
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
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	switch (msg->message_id) {

	case SWITCH_MESSAGE_RESAMPLE_EVENT:
		{
			if (switch_rtp_ready(a_engine->rtp_session)) {
				switch_channel_audio_sync(session->channel);
				switch_rtp_reset_jb(a_engine->rtp_session);
			}

			if (switch_channel_test_flag(session->channel, CF_CONFERENCE)) {
				switch_channel_set_flag(session->channel, CF_CONFERENCE_RESET_MEDIA);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_HOLD:
		{
			if (a_engine && a_engine->rtp_session) {
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_hold_packets);
				switch_rtp_set_media_timeout(a_engine->rtp_session, a_engine->media_hold_timeout);
			}

			if (v_engine && v_engine->rtp_session) {
				switch_rtp_set_media_timeout(v_engine->rtp_session, v_engine->media_hold_timeout);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_UNHOLD:
		{
			if (a_engine && a_engine->rtp_session) {
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_packets);
				switch_rtp_set_media_timeout(a_engine->rtp_session, a_engine->media_timeout);
			}

			if (v_engine && v_engine->rtp_session) {
				switch_rtp_set_media_timeout(v_engine->rtp_session, v_engine->media_timeout);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ:
		{
			if (v_engine->rtp_session) {
				if (msg->numeric_arg || !switch_channel_test_flag(session->channel, CF_MANUAL_VID_REFRESH)) {
					if (switch_rtp_test_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_FIR)) {
						switch_rtp_video_refresh(v_engine->rtp_session);
					}// else {
					if (switch_rtp_test_flag(v_engine->rtp_session, SWITCH_RTP_FLAG_PLI)) {
						switch_rtp_video_loss(v_engine->rtp_session);
					}
					//}
				}
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
				check_jb(session, msg->string_arg, 0, 0, SWITCH_FALSE);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_HARD_MUTE:
		if (a_engine->rtp_session) {
			a_engine->last_seq = 0;
			
			if (session->bugs && msg->numeric_arg) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "%s has a media bug, hard mute not allowed.\n", switch_channel_get_name(session->channel));
			} else {
				if (msg->numeric_arg) {
					switch_rtp_set_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_MUTE);
				} else {
					switch_rtp_clear_flag(a_engine->rtp_session, SWITCH_RTP_FLAG_MUTE);
				}

				rtp_flush_read_buffer(a_engine->rtp_session, SWITCH_RTP_FLUSH_ONCE);
			}
		}

		break;

	case SWITCH_MESSAGE_INDICATE_BITRATE_REQ:
		{
			if (v_engine->rtp_session) {
				switch_rtp_req_bitrate(v_engine->rtp_session, msg->numeric_arg);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_BITRATE_ACK:
		{
			if (v_engine->rtp_session) {
				switch_rtp_ack_bitrate(v_engine->rtp_session, msg->numeric_arg);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_CODEC_DEBUG_REQ:
		{
			switch_rtp_engine_t *engine = &smh->engines[msg->numeric_reply];
			uint32_t level = (uint32_t) msg->numeric_arg;

			if (engine->rtp_session) {
				switch_core_codec_control(&engine->read_codec, SCC_DEBUG, SCCT_INT, (void *)&level, SCCT_NONE, NULL, NULL, NULL);
				switch_core_codec_control(&engine->write_codec, SCC_DEBUG, SCCT_INT, (void *)&level, SCCT_NONE, NULL, NULL, NULL);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_CODEC_SPECIFIC_REQ:
		{
			switch_rtp_engine_t *engine;
			switch_io_type_t iotype = SWITCH_IO_READ;
			switch_media_type_t type = SWITCH_MEDIA_TYPE_AUDIO;
			switch_codec_control_type_t reply_type = SCCT_NONE;
			void *reply = NULL;

			if (!strcasecmp(msg->string_array_arg[0], "video")) {
				type = SWITCH_MEDIA_TYPE_VIDEO;
			}

			if (!strcasecmp(msg->string_array_arg[1], "write")) {
				iotype = SWITCH_IO_WRITE;
			}

			engine = &smh->engines[type];

			if (engine->rtp_session) {
				if (iotype == SWITCH_IO_READ) {
					switch_core_codec_control(&engine->read_codec, SCC_CODEC_SPECIFIC,
											  SCCT_STRING, (void *)msg->string_array_arg[2],
											  SCCT_STRING, (void *)msg->string_array_arg[3], &reply_type, &reply);
				} else {
					switch_core_codec_control(&engine->write_codec, SCC_CODEC_SPECIFIC,
											  SCCT_STRING, (void *)msg->string_array_arg[2],
											  SCCT_STRING, (void *)msg->string_array_arg[3], &reply_type, &reply);
				}


				if (reply_type == SCCT_STRING) {
					msg->string_array_arg[4] = (char *)reply;
				}
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
			} else if (direction && *direction == 't' && t_engine) {
				direction++;
				rtp = t_engine->rtp_session;
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

#if 0
			if (switch_rtp_ready(v_engine->rtp_session)) {
				const char *val;

				if ((!(val = switch_channel_get_variable(session->channel, "rtp_jitter_buffer_during_bridge")) || switch_false(val))) {
					if (switch_rtp_get_jitter_buffer(v_engine->rtp_session) && switch_channel_test_cap_partner(session->channel, CC_FS_RTP)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "%s PAUSE Jitterbuffer\n", switch_channel_get_name(session->channel));
						switch_rtp_pause_jitter_buffer(v_engine->rtp_session, SWITCH_TRUE);
						switch_set_flag(smh, SMF_VB_PAUSED);
					}
				}
			}
#endif 

			if (switch_rtp_ready(a_engine->rtp_session)) {
				const char *val;
				int ok = 0;

				if (!switch_channel_test_flag(session->channel, CF_VIDEO_READY) && 
					(!(val = switch_channel_get_variable(session->channel, "rtp_jitter_buffer_during_bridge")) || switch_false(val))) {
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

#if 0
		if (switch_rtp_ready(v_engine->rtp_session)) {

			if (switch_test_flag(smh, SMF_VB_PAUSED)) {
				switch_clear_flag(smh, SMF_VB_PAUSED);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "%s RESUME Video Jitterbuffer\n", switch_channel_get_name(session->channel));
				switch_rtp_pause_jitter_buffer(v_engine->rtp_session, SWITCH_FALSE);
				
			}
		}
#endif

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
			//switch_rtp_reset_jb(a_engine->rtp_session);
		}
		goto end;

	case SWITCH_MESSAGE_INDICATE_VIDEO_SYNC:
		if (switch_rtp_ready(v_engine->rtp_session)) {
			switch_rtp_flush(v_engine->rtp_session);
			//switch_rtp_reset_jb(v_engine->rtp_session);
		}
		goto end;
	case SWITCH_MESSAGE_INDICATE_3P_MEDIA:
	case SWITCH_MESSAGE_INDICATE_MEDIA:
		{

			a_engine->codec_negotiated = 0;
			v_engine->codec_negotiated = 0;

			if (session->track_duration) {
				switch_core_session_enable_heartbeat(session, session->track_duration);
			}
		}
		break;

	case SWITCH_MESSAGE_INDICATE_3P_NOMEDIA:
		switch_channel_set_flag(session->channel, CF_PROXY_MODE);
		switch_core_media_set_local_sdp(session, NULL, SWITCH_FALSE);
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
					switch_channel_set_variable_printf(session->channel,
													   "codec_string", "=%s", switch_channel_get_variable(session->channel, "ep_codec_string"));
				}
				
				a_engine->codec_negotiated = 0;
				v_engine->codec_negotiated = 0;
				smh->num_negotiated_codecs = 0;
				switch_channel_clear_flag(session->channel, CF_VIDEO_POSSIBLE);
				switch_core_media_prepare_codecs(session, SWITCH_TRUE);
				switch_core_media_check_video_codecs(session);
				
				switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 1);
			}

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
SWITCH_DECLARE(switch_bool_t) switch_core_media_check_udptl_mode(switch_core_session_t *session, switch_media_type_t type)
{
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_FALSE;
	}

	if (switch_rtp_ready(smh->engines[type].rtp_session)) {
		return switch_rtp_test_flag(smh->engines[type].rtp_session, SWITCH_RTP_FLAG_UDPTL) ? SWITCH_TRUE : SWITCH_FALSE;
	}

	return SWITCH_FALSE;
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
SWITCH_DECLARE(switch_jb_t *) switch_core_media_get_jb(switch_core_session_t *session, switch_media_type_t type)
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
	switch_core_media_merge_sdp_codec_string(session, r_sdp, sdp_type, switch_core_media_get_codec_string(session));
}

SWITCH_DECLARE(void) switch_core_media_merge_sdp_codec_string(switch_core_session_t *session, const char *r_sdp,
															  switch_sdp_type_t sdp_type, const char *codec_string)
{

	

	sdp_parser_t *parser;
	sdp_session_t *sdp;
	switch_media_handle_t *smh;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (!r_sdp) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Setting NULL SDP is invalid\n");
		return;
	}

	if (zstr(codec_string)) {
		codec_string = switch_core_media_get_codec_string(session);
	}
	
	if ((parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {

		if ((sdp = sdp_session(parser))) {
			switch_core_media_set_r_sdp_codec_string(session, codec_string, sdp, sdp_type);
		}

		sdp_parser_free(parser);
	}

}


static void add_audio_codec(sdp_rtpmap_t *map, const switch_codec_implementation_t *imp, int ptime, char *buf, switch_size_t buflen)
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
		codec_ms = 30;
	}

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

	switch_snprintf(buf + strlen(buf), buflen - strlen(buf), ",%s.%s%s%s%s", imp->modname, map->rm_encoding, ratestr, ptstr, bitstr);

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
	char fmtp[SWITCH_MAX_CODECS][MAX_FMTP_LEN];

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

		if (*codec_string == '=') codec_string++;

		if ((tmp_codec_string = strdup(codec_string))) {
			num_codecs = switch_separate_string(tmp_codec_string, ',', codec_order, SWITCH_MAX_CODECS);
			num_codecs = switch_loadable_module_get_codecs_sorted(codecs, fmtp, SWITCH_MAX_CODECS, codec_order, num_codecs);
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
				int found = 0;
				for (attr = m->m_attributes; attr && found < 2; attr = attr->a_next) {
					if (zstr(attr->a_name)) {
						continue;
					}
					if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
						ptime = atoi(attr->a_value);
						found++;
					}
					if (!strcasecmp(attr->a_name, "rtcp-mux")) {
						if (switch_channel_var_true(channel, "rtcp_mux_auto_detect")) {
							switch_log_printf(SWITCH_CHANNEL_CHANNEL_LOG(channel), SWITCH_LOG_DEBUG, "setting rtcp-mux from sdp\n");
							switch_channel_set_variable(channel, "rtcp_mux", "true");
						}
						found++;
					}
				}
				switch_core_media_add_payload_map(session,
												  m->m_type == sdp_media_audio ? SWITCH_MEDIA_TYPE_AUDIO : SWITCH_MEDIA_TYPE_VIDEO,
												  map->rm_encoding,
												  NULL,
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
							add_audio_codec(map, imp, ptime, buf, sizeof(buf));
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
							add_audio_codec(map, imp, ptime, buf, sizeof(buf));
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
							if (map->rm_fmtp) {
								switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s.%s~%s", imp->modname, imp->iananame, map->rm_fmtp);
							} else {
								switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s.%s", imp->modname, imp->iananame);
							}
							already_did[imp->ianacode] = 1;
						}
					}
				}

			} else {
				for (i = 0; i < num_codecs; i++) {
					const switch_codec_implementation_t *imp = codecs[i];

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
							if (map->rm_fmtp) {
								switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s.%s~%s", imp->modname, imp->iananame, map->rm_fmtp);
							} else {
								switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), ",%s.%s", imp->modname, imp->iananame);
							}
							already_did[imp->ianacode] = 1;
						}
					}
				}
			}
		} else if (m->m_proto == sdp_proto_msrp) {
			switch_channel_set_flag(channel, CF_WANT_MSRP);
		} else if (m->m_proto == sdp_proto_msrps) {
			switch_channel_set_flag(channel, CF_WANT_MSRPS);
		} else if (m->m_type == sdp_media_text) {
			switch_channel_set_flag(channel, CF_WANT_RTT);
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

SWITCH_DECLARE(void) switch_core_session_stop_media(switch_core_session_t *session)
{
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
	switch_media_handle_t *smh;
	int type;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (switch_core_codec_ready(&v_engine->read_codec)) {
		type = 1;
		switch_core_codec_control(&v_engine->read_codec, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, SCCT_NONE, NULL, NULL, NULL);
	}

	if (switch_core_codec_ready(&v_engine->write_codec)) {
		type = 2;
		switch_core_codec_control(&v_engine->write_codec, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, SCCT_NONE, NULL, NULL, NULL);
	}

	if (a_engine->rtp_session) {
		switch_rtp_reset(a_engine->rtp_session);
	}

	if (v_engine->rtp_session) {
		switch_rtp_reset(v_engine->rtp_session);
	}

	if (t_engine->rtp_session) {
		switch_rtp_reset(t_engine->rtp_session);
	}


	smh->msid = NULL;
	smh->cname = NULL;
	v_engine->ice_out.ufrag = NULL;
	v_engine->ice_out.pwd = NULL;
	v_engine->ice_out.cands[0][0].foundation = NULL;
	v_engine->ice_out.cands[0][0].component_id = 0;

	t_engine->ice_out.ufrag = NULL;
	t_engine->ice_out.pwd = NULL;
	t_engine->ice_out.cands[0][0].foundation = NULL;
	t_engine->ice_out.cands[0][0].component_id = 0;


	a_engine->ice_out.ufrag = NULL;
	a_engine->ice_out.pwd = NULL;
	a_engine->ice_out.cands[0][0].foundation = NULL;
	a_engine->ice_out.cands[0][0].component_id = 0;

	if (v_engine->ice_in.cands[v_engine->ice_in.chosen[0]][0].ready) {
		gen_ice(smh->session, SWITCH_MEDIA_TYPE_VIDEO, NULL, 0);
	}

	if (t_engine->ice_in.cands[t_engine->ice_in.chosen[0]][0].ready) {
		gen_ice(smh->session, SWITCH_MEDIA_TYPE_TEXT, NULL, 0);
	}

	if (a_engine->ice_in.cands[a_engine->ice_in.chosen[0]][0].ready) {
		gen_ice(smh->session, SWITCH_MEDIA_TYPE_AUDIO, NULL, 0);
	}

	smh->owner_id = 0;
	smh->session_id = 0;

	a_engine->local_dtls_fingerprint.len = 0;
	v_engine->local_dtls_fingerprint.len = 0;
	t_engine->local_dtls_fingerprint.len = 0;

	a_engine->remote_ssrc = 0;
	v_engine->remote_ssrc = 0;
	t_engine->remote_ssrc = 0;

	switch_channel_clear_flag(smh->session->channel, CF_VIDEO_READY);
	switch_core_session_wake_video_thread(smh->session);
	switch_core_session_request_video_refresh(smh->session);
}



//?
SWITCH_DECLARE(void) switch_core_media_check_outgoing_proxy(switch_core_session_t *session, switch_core_session_t *o_session)
{
	switch_rtp_engine_t *a_engine, *v_engine, *t_engine;
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
	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	switch_channel_set_flag(session->channel, CF_PROXY_MEDIA);

	clear_pmaps(a_engine);
	clear_pmaps(v_engine);

	pmap = switch_core_media_add_payload_map(session,
											 SWITCH_MEDIA_TYPE_AUDIO,
											 "PROXY",
											 NULL,
											 NULL,
											 SDP_TYPE_RESPONSE,
											 0,
											 8000,
											 20,
											 1,
											 SWITCH_TRUE);

	a_engine->cur_payload_map = pmap;

	if (switch_stristr("m=video", r_sdp)) {
		switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 1);
		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "PROXY-VID",
												 NULL,
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


	if (switch_stristr("m=text", r_sdp)) {
		switch_core_media_choose_port(session, SWITCH_MEDIA_TYPE_VIDEO, 1);
		pmap = switch_core_media_add_payload_map(session,
												 SWITCH_MEDIA_TYPE_AUDIO,
												 "PROXY-TXT",
												 NULL,
												 NULL,
												 SDP_TYPE_RESPONSE,
												 0,
												 1000,
												 1000,
												 1,
												 SWITCH_TRUE);

		t_engine->cur_payload_map = pmap;

		switch_channel_set_flag(session->channel, CF_HAS_TEXT);
		switch_channel_set_flag(session->channel, CF_TEXT_POSSIBLE);
	}
}


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
		smh->mparams->num_codecs = switch_loadable_module_get_codecs_sorted(smh->codecs, smh->fmtp, SWITCH_MAX_CODECS, smh->codec_order, smh->codec_order_last);
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
		a_engine->cur_payload_map->pt = (switch_payload_t)(smh->payload_space = atoi(tmp));
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
			v_engine->cur_payload_map->pt = (switch_payload_t)atoi(tmp);
		}

		if ((tmp = switch_channel_get_variable(session->channel, "rtp_video_recv_pt"))) {
			v_engine->cur_payload_map->recv_pt = (switch_payload_t)atoi(tmp);
		}

		v_engine->cur_payload_map->rm_encoding = (char *) switch_channel_get_variable(session->channel, "rtp_use_video_codec_name");
		v_engine->cur_payload_map->rm_fmtp = (char *) switch_channel_get_variable(session->channel, "rtp_use_video_codec_fmtp");
		v_engine->codec_negotiated = 1;

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
		switch_core_media_add_crypto(session, &a_engine->ssec[a_engine->crypto_type],SWITCH_RTP_CRYPTO_SEND);
		switch_core_media_add_crypto(session, &a_engine->ssec[a_engine->crypto_type],SWITCH_RTP_CRYPTO_RECV);
		switch_channel_set_flag(smh->session->channel, CF_SECURE);

		switch_rtp_add_crypto_key(a_engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, idx, &a_engine->ssec[a_engine->crypto_type]);

		switch_rtp_add_crypto_key(a_engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, a_engine->ssec[a_engine->crypto_type].crypto_tag, &a_engine->ssec[a_engine->crypto_type]);
	}


	if (switch_core_media_ready(session, SWITCH_MEDIA_TYPE_AUDIO)) {
		switch_rtp_set_telephony_event(a_engine->rtp_session, smh->mparams->te);
		switch_rtp_set_telephony_recv_event(a_engine->rtp_session, smh->mparams->recv_te);
	}

}


SWITCH_DECLARE(void) switch_core_media_init(void)
{
	switch_core_gen_certs(DTLS_SRTP_FNAME ".pem");

	video_globals.cpu_count = switch_core_cpu_count();
	video_globals.cur_cpu = 0;

	switch_core_new_memory_pool(&video_globals.pool);
	switch_mutex_init(&video_globals.mutex, SWITCH_MUTEX_NESTED, video_globals.pool);

}

SWITCH_DECLARE(void) switch_core_media_deinit(void)
{
	if (video_globals.pool) {
		switch_core_destroy_memory_pool(&video_globals.pool);
	}
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


SWITCH_DECLARE(switch_timer_t *) switch_core_media_get_timer(switch_core_session_t *session, switch_media_type_t mtype)
{
	switch_rtp_engine_t *engine = NULL;
	switch_media_handle_t *smh = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return NULL;
	}

	if (!(engine = &smh->engines[mtype])) {
		return NULL;
	}

	return switch_rtp_get_media_timer(engine->rtp_session);

}

SWITCH_DECLARE(switch_status_t) _switch_core_session_request_video_refresh(switch_core_session_t *session, int force, const char *file, const char *func, int line)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_handle_t *smh = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_media_up(channel) && switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_core_session_message_t msg = { 0 };
		switch_time_t now = switch_micro_time_now();

		if (!force && (smh->last_video_refresh_req && (now - smh->last_video_refresh_req) < VIDEO_REFRESH_FREQ)) {
			return SWITCH_STATUS_BREAK;
		}

		smh->last_video_refresh_req = now;

		if (force) {
			msg.numeric_arg = 1;
		}

		msg._file = file;
		msg._func = func;
		msg._line = line;
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, switch_core_session_get_uuid(session), 
						  SWITCH_LOG_DEBUG1, "%s Video refresh requested.\n", switch_channel_get_name(session->channel));
		msg.from = __FILE__;
		msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
		switch_core_session_receive_message(session, &msg);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_send_and_request_video_refresh(switch_core_session_t *session)
{
	if (switch_channel_test_flag(session->channel, CF_VIDEO)) {
		switch_core_session_request_video_refresh(session);
		switch_core_media_gen_key_frame(session);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_media_codec_control(switch_core_session_t *session,
																switch_media_type_t mtype,
																switch_io_type_t iotype,
																switch_codec_control_command_t cmd,
																switch_codec_control_type_t ctype,
																void *cmd_data,
																switch_codec_control_type_t atype,
																void *cmd_arg,
																switch_codec_control_type_t *rtype,
																void **ret_data)
{
	switch_rtp_engine_t *engine = NULL;
	switch_media_handle_t *smh = NULL;
	switch_codec_t *codec = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(engine = &smh->engines[mtype])) {
		return SWITCH_STATUS_NOTIMPL;
	}

	if (iotype == SWITCH_IO_READ) {
		codec = &engine->read_codec;
	} else {
		codec = &engine->write_codec;
	}

	if (!switch_core_codec_ready(codec)) {
		return SWITCH_STATUS_FALSE;
	}

	if (mtype == SWITCH_MEDIA_TYPE_VIDEO) {
		if (!switch_channel_test_flag(session->channel, CF_VIDEO)) {
			return SWITCH_STATUS_FALSE;
		}
	}

	if (codec) {
		return switch_core_codec_control(codec, cmd, ctype, cmd_data, atype, cmd_arg, rtype, ret_data);
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_bool_t) switch_core_media_codec_get_cap(switch_core_session_t *session,
																switch_media_type_t mtype,
																switch_codec_flag_t flag) {
	switch_rtp_engine_t *engine = NULL;
	switch_media_handle_t *smh = NULL;
	switch_codec_t *codec = NULL;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_FALSE;
	}

	if (!(engine = &smh->engines[mtype])) {
		return SWITCH_FALSE;
	}

	codec = &engine->write_codec;

	if (!switch_core_codec_ready(codec)) {
		return SWITCH_FALSE;
	}

	if (switch_test_flag(codec, flag)){
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_write_encoded_video_frame(switch_core_session_t *session,
																		switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	switch_io_event_hook_video_write_frame_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY || switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG3, "Writing video to RECVONLY/INACTIVE session\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (session->endpoint_interface->io_routines->write_video_frame) {
		if ((status = session->endpoint_interface->io_routines->write_video_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.video_write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->video_write_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	return status;
}

SWITCH_DECLARE(void) switch_core_session_video_reinit(switch_core_session_t *session)
{
	switch_media_handle_t *smh;
	int type;

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_channel_down(session->channel)) {
		return;
	}

	smh->video_init = 0;
	smh->video_last_key_time = 0;
	switch_core_session_send_and_request_video_refresh(session);

	type = 1;
	switch_core_media_codec_control(session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_IO_READ, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, SCCT_NONE, NULL, NULL, NULL);
	switch_core_session_request_video_refresh(session);

}

SWITCH_DECLARE(switch_status_t) switch_core_session_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																	  int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_time_t now = switch_micro_time_now();
	switch_codec_t *codec = switch_core_session_get_video_write_codec(session);
	switch_timer_t *timer;
	switch_media_handle_t *smh;
	switch_image_t *dup_img = NULL, *img = frame->img;
	switch_status_t encode_status;
	switch_frame_t write_frame = {0};
	switch_rtp_engine_t *v_engine = NULL;
	switch_bool_t need_free = SWITCH_FALSE;
	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!codec) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s has no video codec\n", switch_core_session_get_name(session));
		return SWITCH_STATUS_FALSE;
	}


	if (switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY || switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG3, "Writing video to RECVONLY/INACTIVE session\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO_PAUSE_WRITE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(switch_channel_test_flag(session->channel, CF_VIDEO_READY) || (flags & SWITCH_IO_FLAG_FORCE))) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO] && switch_mutex_trylock(smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO]) != SWITCH_STATUS_SUCCESS) {
		/* return CNG, another thread is already writing  */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s is already being written to for %s\n",
						  switch_channel_get_name(session->channel), type2str(SWITCH_MEDIA_TYPE_VIDEO));
		return SWITCH_STATUS_INUSE;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];
	if (v_engine->thread_write_lock && v_engine->thread_write_lock != switch_thread_self()) {
		switch_goto_status(SWITCH_STATUS_SUCCESS, done);
	}

	if (!smh->video_init && smh->mparams->video_key_first && (now - smh->video_last_key_time) > smh->mparams->video_key_first) {
		switch_core_media_gen_key_frame(session);

		if (smh->video_last_key_time) {
			smh->video_init = 1;
		}

		smh->video_last_key_time = now;
	}

	if (smh->mparams->video_key_freq && (now - smh->video_last_key_time) > smh->mparams->video_key_freq) {
		switch_core_media_gen_key_frame(smh->session);
		smh->video_last_key_time = now;
	}

	if (!img) {
		switch_status_t vstatus;

		vstatus = switch_core_session_write_encoded_video_frame(session, frame, flags, stream_id);
		switch_goto_status(vstatus, done);
	}


    /* When desired, scale video to match the input signal (if output is bigger) */
	if (switch_channel_test_flag(session->channel, CF_VIDEO_READY) &&
		switch_channel_test_flag(session->channel, CF_VIDEO_MIRROR_INPUT)) {
		switch_vid_params_t vid_params = { 0 };

		switch_core_media_get_vid_params(session, &vid_params);

		if (vid_params.width && vid_params.height && ((vid_params.width != img->d_w) || (vid_params.height != img->d_h))) {
			switch_img_letterbox(img, &dup_img, vid_params.width, vid_params.height, "#000000f");
			if (!(img = dup_img)) {
				switch_goto_status(SWITCH_STATUS_INUSE, done);
			}
		}
	}

	if (!switch_channel_test_flag(session->channel, CF_VIDEO_WRITING)) {
		smh->vid_params.d_width = img->d_w;
		smh->vid_params.d_height = img->d_h;
	}

	if (session->bugs) {
		switch_media_bug_t *bp;
		int prune = 0;
		int patched = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			switch_bool_t ok = SWITCH_TRUE;

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}

			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (bp->ready && switch_test_flag(bp, SMBF_WRITE_VIDEO_STREAM)) {
				switch_image_t *dimg = NULL;

				switch_img_copy(img, &dimg);
				switch_queue_push(bp->write_video_queue, dimg);

				if (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM_BLEG)) {
					switch_core_media_bug_patch_spy_frame(bp, img, SWITCH_RW_WRITE);
					patched = 1;
				}

			}

			if (bp->ready && img &&
				(switch_test_flag(bp, SMBF_WRITE_VIDEO_PING) || (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM) && !patched))) {
				switch_frame_t bug_frame = { 0 };

				bug_frame.img = img;

				if (bp->callback && switch_test_flag(bp, SMBF_WRITE_VIDEO_PING)) {
					bp->video_ping_frame = &bug_frame;
					if (bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE_VIDEO_PING) == SWITCH_FALSE
						|| (bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL))) {
						ok = SWITCH_FALSE;
					}
					bp->video_ping_frame = NULL;
				}

				if (bug_frame.img && bug_frame.img != img) {
					need_free = SWITCH_TRUE;
					img = bug_frame.img;
				}

				if (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM_BLEG) && !patched) {
					switch_core_media_bug_patch_spy_frame(bp, img, SWITCH_RW_WRITE);
				}


			}

			if (ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}

		}

		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (prune) {
			switch_core_media_bug_prune(session);
		}

	}

	write_frame = *frame;
	frame = &write_frame;
	frame->img = img;

	if (!switch_test_flag(frame, SFF_USE_VIDEO_TIMESTAMP)) {

		if (!(timer = switch_core_media_get_timer(session, SWITCH_MEDIA_TYPE_VIDEO))) {

			if (!smh->video_timer.timer_interface) {
				switch_core_timer_init(&smh->video_timer, "soft", 1, 90, switch_core_session_get_pool(session));
			}
			switch_core_timer_sync(&smh->video_timer);
			timer = &smh->video_timer;
		}

		frame->timestamp = timer->samplecount;
	}

	switch_clear_flag(frame, SFF_SAME_IMAGE);
	frame->m = 0;

	do {
		frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;
		encode_status = switch_core_codec_encode_video(codec, frame);

		if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

			switch_assert((encode_status == SWITCH_STATUS_SUCCESS && frame->m) || !frame->m);

			if (frame->flags & SFF_PICTURE_RESET) {
				switch_core_session_video_reinit(session);
				frame->flags &= ~SFF_PICTURE_RESET;
			}

			if (frame->datalen == 0) break;

			switch_set_flag(frame, SFF_RAW_RTP_PARSE_FRAME);
			status = switch_core_session_write_encoded_video_frame(session, frame, flags, stream_id);
		}

	} while(status == SWITCH_STATUS_SUCCESS && encode_status == SWITCH_STATUS_MORE_DATA);

 done:

	if (smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO]) {
		switch_mutex_unlock(smh->write_mutex[SWITCH_MEDIA_TYPE_VIDEO]);
	}

	switch_img_free(&dup_img);

	if (need_free) {
		switch_img_free(&frame->img);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_wait_for_video_input_params(switch_core_session_t *session, uint32_t timeout_ms)
{
	switch_media_handle_t *smh;
	switch_codec_implementation_t read_impl = { 0 };
	switch_rtp_engine_t *v_engine = NULL;

	switch_assert(session != NULL);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_test_flag(session->channel, CF_VIDEO_DECODED_READ)) {
		return SWITCH_STATUS_GENERR;
	}

	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (v_engine->smode == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_NOTIMPL;
	}

	switch_core_session_get_read_impl(session, &read_impl);

	while(switch_channel_ready(session->channel) && timeout_ms > 0) {
		switch_frame_t *read_frame;
		switch_status_t status;

		if (video_globals.synced &&
			switch_channel_test_flag(session->channel, CF_VIDEO_READY) && smh->vid_params.width && smh->vid_params.height && smh->vid_params.fps) {
			return SWITCH_STATUS_SUCCESS;
		}

		switch_core_session_request_video_refresh(session);
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			return SWITCH_STATUS_FALSE;
		}

		timeout_ms -= (read_impl.microseconds_per_packet / 1000);
	}

	return SWITCH_STATUS_TIMEOUT;

}

SWITCH_DECLARE(switch_bool_t) switch_core_session_transcoding(switch_core_session_t *session_a, switch_core_session_t *session_b, switch_media_type_t type)
{
	switch_bool_t transcoding = SWITCH_FALSE;

	switch(type) {
	case SWITCH_MEDIA_TYPE_AUDIO:
		{
			switch_codec_implementation_t read_impl_a = { 0 }, read_impl_b = { 0 };

			switch_core_session_get_read_impl(session_a, &read_impl_a);
			switch_core_session_get_read_impl(session_b, &read_impl_b);
			
			if (read_impl_a.impl_id && read_impl_b.impl_id) {
				transcoding = (read_impl_a.impl_id != read_impl_b.impl_id || read_impl_a.decoded_bytes_per_packet != read_impl_b.decoded_bytes_per_packet);
			}
		}
		break;
	case SWITCH_MEDIA_TYPE_VIDEO:
		transcoding = (switch_channel_test_flag(session_a->channel, CF_VIDEO_DECODED_READ) || 
					   switch_channel_test_flag(session_b->channel, CF_VIDEO_DECODED_READ));
		break;
	default:
		break;
	}

	return transcoding;

}

SWITCH_DECLARE(void) switch_core_session_passthru(switch_core_session_t *session, switch_media_type_t type, switch_bool_t on)
{
	switch_rtp_engine_t *engine;

	if (!session->media_handle) return;

	engine = &session->media_handle->engines[type];


	if (switch_rtp_ready(engine->rtp_session)) {
		char var[50] = "";
		switch_snprintf(var, sizeof(var), "disable_%s_jb_during_passthru", type2str(type));

		if (switch_channel_var_true(session->channel, var)) {
			if (on) {
				switch_rtp_set_flag(engine->rtp_session, SWITCH_RTP_FLAG_PASSTHRU);
			} else {
				switch_rtp_clear_flag(engine->rtp_session, SWITCH_RTP_FLAG_PASSTHRU);
			}
		}

		if (type == SWITCH_MEDIA_TYPE_VIDEO) {
			switch_core_session_request_video_refresh(session);
			if (!on) {
				switch_core_media_gen_key_frame(session);
			}
		}

	}

}

SWITCH_DECLARE(switch_status_t) switch_core_session_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
																	 int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_io_event_hook_video_read_frame_t *ptr;
	uint32_t loops = 0;
	switch_media_handle_t *smh;
	int is_keyframe = 0;

	switch_assert(session != NULL);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

 top:

	loops++;

	if (switch_channel_down_nosig(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (session->endpoint_interface->io_routines->read_video_frame) {
		if ((status = session->endpoint_interface->io_routines->read_video_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.video_read_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->video_read_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO_PAUSE_READ)) {
		*frame = &runtime.dummy_cng_frame;
		switch_cond_next();
		return SWITCH_STATUS_SUCCESS;
	}

	if (status == SWITCH_STATUS_INUSE) {
		*frame = &runtime.dummy_cng_frame;
		switch_cond_next();
		return SWITCH_STATUS_SUCCESS;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if (!(*frame)) {
		goto done;
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO_DEBUG_READ)) {
		if (switch_test_flag((*frame), SFF_CNG)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "VIDEO: CNG\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
							  "VIDEO: seq: %d ts: %u len: %4d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x mark: %d\n",
							  (*frame)->seq, (*frame)->timestamp, (*frame)->datalen,
							  *((uint8_t *)(*frame)->data), *((uint8_t *)(*frame)->data + 1),
							  *((uint8_t *)(*frame)->data + 2), *((uint8_t *)(*frame)->data + 3),
							  *((uint8_t *)(*frame)->data + 4), *((uint8_t *)(*frame)->data + 5),
							  *((uint8_t *)(*frame)->data + 6), *((uint8_t *)(*frame)->data + 7),
							  *((uint8_t *)(*frame)->data + 8), *((uint8_t *)(*frame)->data + 9),
							  *((uint8_t *)(*frame)->data + 10), (*frame)->m);
		}
	}


	if (switch_test_flag(*frame, SFF_CNG)) {
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}

	if (switch_channel_test_flag(session->channel, CF_VIDEO_DECODED_READ) && (*frame)->img == NULL) {
		switch_status_t decode_status;

		(*frame)->img = NULL;

		decode_status = switch_core_codec_decode_video((*frame)->codec, *frame);
		if (switch_test_flag(*frame, SFF_IS_KEYFRAME)) {
			is_keyframe++;
		}
		if ((*frame)->img && switch_channel_test_flag(session->channel, CF_VIDEO_DEBUG_READ)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "IMAGE %dx%d %dx%d\n",
							  (*frame)->img->w, (*frame)->img->h, (*frame)->img->d_w, (*frame)->img->d_h);
		}

		if ((*frame)->img && (*frame)->img->d_w && (*frame)->img->d_h) {
			int new_w = 0, new_h = 0;
			
			if ((*frame)->img->d_w != smh->vid_params.width || (*frame)->img->d_h != smh->vid_params.height) {
				new_w = (*frame)->img->d_w;
				new_h = (*frame)->img->d_h;

				if (new_w && new_h) {
					switch_mutex_lock(smh->control_mutex);
					smh->vid_params.width = new_w;
					smh->vid_params.height = new_h;
					switch_channel_set_variable_printf(session->channel, "video_width", "%d", new_w);
					switch_channel_set_variable_printf(session->channel, "video_height", "%d", new_h);
					switch_mutex_unlock(smh->control_mutex);
				}
			}
		}

		if (switch_test_flag((*frame), SFF_WAIT_KEY_FRAME)) {
			switch_core_session_request_video_refresh(session);
			switch_clear_flag((*frame), SFF_WAIT_KEY_FRAME);

			if (!(*frame)->img) {
				*frame = &runtime.dummy_cng_frame;
				switch_cond_next();
				return SWITCH_STATUS_SUCCESS;
			}
		}

		if (decode_status == SWITCH_STATUS_MORE_DATA || !(*frame)->img) {
			goto top;
		}
	}

	if (!switch_channel_test_flag(session->channel, CF_VIDEO_READY) && *frame) {
		if (switch_channel_test_flag(session->channel, CF_VIDEO_DECODED_READ)) {
			if ((*frame)->img) {
				switch_channel_set_flag(session->channel, CF_VIDEO_READY);
			}
		} else if ((*frame)->m || ++smh->ready_loops > 5) {
			switch_channel_set_flag(session->channel, CF_VIDEO_READY);
		}
	}

  done:

	if (*frame && is_keyframe) {
		switch_set_flag(*frame, SFF_IS_KEYFRAME);
	}

	if (session->bugs) {
		switch_media_bug_t *bp;
		int prune = 0;
		int patched = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			switch_bool_t ok = SWITCH_TRUE;

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}

			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (bp->ready && switch_test_flag(bp, SMBF_READ_VIDEO_STREAM)) {
				if ((*frame) && (*frame)->img) {
					switch_image_t *img = NULL;
					switch_img_copy((*frame)->img, &img);
					switch_queue_push(bp->read_video_queue, img);
					if (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM)) {
						switch_core_media_bug_patch_spy_frame(bp, (*frame)->img, SWITCH_RW_READ);
						patched = 1;
					}
				}
			}

			if (bp->ready && (*frame) && (*frame)->img &&
				(switch_test_flag(bp, SMBF_READ_VIDEO_PING) || (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM) && !patched))) {


				if (bp->callback && switch_test_flag(bp, SMBF_READ_VIDEO_PING)) {
					bp->video_ping_frame = *frame;

					if (bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_VIDEO_PING) == SWITCH_FALSE
						|| (bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL))) {
						ok = SWITCH_FALSE;
					}
					bp->video_ping_frame = NULL;
				}

				if (switch_core_media_bug_test_flag(bp, SMBF_SPY_VIDEO_STREAM) && !patched) {
					switch_core_media_bug_patch_spy_frame(bp, (*frame)->img, SWITCH_RW_READ);
				}
			}

			if (ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}
		}

		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (prune) {
			switch_core_media_bug_prune(session);
		}
	}

	if ((*frame) && (*frame)->codec) {
		(*frame)->pmap = NULL;
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_core_session_video_read_callback(session, *frame);
	}

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_video_read_callback(switch_core_session_t *session,
																			switch_core_video_thread_callback_func_t func, void *user_data)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(smh->control_mutex);
	if (!func) {
		session->video_read_callback = NULL;
		session->video_read_user_data = NULL;
	} else if (session->video_read_callback) {
		status = SWITCH_STATUS_FALSE;
	} else {
		session->video_read_callback = func;
		session->video_read_user_data = user_data;
	}

	switch_core_session_start_video_thread(session);
	switch_mutex_unlock(smh->control_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_video_read_callback(switch_core_session_t *session, switch_frame_t *frame)
{
	switch_media_handle_t *smh;
	switch_status_t status = SWITCH_STATUS_CONTINUE;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(smh->control_mutex);

	if (session->video_read_callback) {
		status = session->video_read_callback(session, frame, session->video_read_user_data);
	}

	switch_mutex_unlock(smh->control_mutex);

	return status;
}



SWITCH_DECLARE(switch_status_t) switch_core_session_set_text_read_callback(switch_core_session_t *session,
																			switch_core_text_thread_callback_func_t func, void *user_data)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(smh->control_mutex);
	if (!func) {
		session->text_read_callback = NULL;
		session->text_read_user_data = NULL;
	} else if (session->text_read_callback) {
		status = SWITCH_STATUS_FALSE;
	} else {
		session->text_read_callback = func;
		session->text_read_user_data = user_data;
	}

	switch_core_session_start_text_thread(session);
	switch_mutex_unlock(smh->control_mutex);

	return status;
}



SWITCH_DECLARE(switch_status_t) switch_core_session_text_read_callback(switch_core_session_t *session, switch_frame_t *frame)
{
	switch_media_handle_t *smh;
	switch_status_t status = SWITCH_STATUS_CONTINUE;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(smh->control_mutex);

	if (session->text_read_callback) {
		status = session->text_read_callback(session, frame, session->text_read_user_data);
	}

	switch_mutex_unlock(smh->control_mutex);

	return status;
}



SWITCH_DECLARE(switch_status_t) switch_core_session_read_text_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags,
																	 int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_io_event_hook_text_read_frame_t *ptr;
	switch_media_handle_t *smh;
	switch_io_read_text_frame_t read_text_frame = NULL;
	switch_time_t now;

	switch_assert(session != NULL);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_down_nosig(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(read_text_frame = session->endpoint_interface->io_routines->read_text_frame)) {
		if (session->io_override) {
			read_text_frame = session->io_override->read_text_frame;
		}
	}

	if (read_text_frame) {
		if ((status = read_text_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.text_read_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->text_read_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}

	if (status == SWITCH_STATUS_INUSE) {
		*frame = &runtime.dummy_cng_frame;
		switch_cond_next();
		return SWITCH_STATUS_SUCCESS;
	}

	if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
		goto done;
	}

	if (!(*frame)) {
		goto done;
	}

	now = switch_micro_time_now();

	if (switch_test_flag((*frame), SFF_CNG)) {
		if (smh->last_text_frame && now - smh->last_text_frame > TEXT_PERIOD_TIMEOUT * 1000) {
			switch_channel_set_flag(session->channel, CF_TEXT_IDLE);
			switch_channel_clear_flag(session->channel, CF_TEXT_ACTIVE);
			smh->last_text_frame = 0;
		}
	} else {
		unsigned char *p = (*frame)->data;

		smh->last_text_frame = now;
		switch_channel_set_flag(session->channel, CF_TEXT_ACTIVE);
		switch_channel_clear_flag(session->channel, CF_TEXT_IDLE);

		while(p && *p) {
			if (*p == '\r' || *p == '\n') {
				switch_set_flag((*frame), SFF_TEXT_LINE_BREAK);
				break;
			}

			if (*p == 0xE2 && *(p+1) == 0x80 && *(p+2) == 0xA8) {
				switch_set_flag((*frame), SFF_TEXT_LINE_BREAK);
				break;
			}

			p++;
		}
	}

	if ((*frame)->data && (*frame)->datalen && !((*frame)->flags & SFF_CNG)) {
		if (!session->text_buffer) {
			switch_mutex_init(&session->text_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
			switch_buffer_create_dynamic(&session->text_buffer, 512, 1024, 0);
		}
		switch_buffer_write(session->text_buffer, (*frame)->data, (*frame)->datalen);
	}

	if (session->bugs) {
		switch_media_bug_t *bp;
		int prune = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			switch_bool_t ok = SWITCH_TRUE;

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}

			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (bp->ready && switch_test_flag(bp, SMBF_READ_TEXT_STREAM)) {
				int bytes = 0;

				if ((*frame)) {
					switch_size_t inuse = 0;

					if ((*frame)->data && (*frame)->datalen && !((*frame)->flags & SFF_CNG)) {
						switch_mutex_lock(session->text_mutex);
						switch_buffer_write(bp->text_buffer, (char *)(*frame)->data, (*frame)->datalen);
						switch_mutex_unlock(session->text_mutex);
					}

					inuse = switch_buffer_inuse(bp->text_buffer);

					if (zstr(bp->text_framedata) && inuse &&
						(switch_channel_test_flag(session->channel, CF_TEXT_IDLE) || switch_test_flag((*frame), SFF_TEXT_LINE_BREAK))) {

						if (inuse + 1 > bp->text_framesize) {
							void *tmp = malloc(inuse + 1024);
							memcpy(tmp, bp->text_framedata, bp->text_framesize);

							switch_assert(tmp);

							bp->text_framesize = inuse + 1024;

							free(bp->text_framedata);
							bp->text_framedata = tmp;

						}


						bytes = switch_buffer_read(bp->text_buffer, bp->text_framedata, inuse);
						*(bp->text_framedata + bytes) = '\0';

						ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_READ_TEXT);
						bp->text_framedata[0] = '\0';
					} else ok = SWITCH_TRUE;
				}
			}

			if (ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}
		}

		switch_thread_rwlock_unlock(session->bug_rwlock);

		if (prune) {
			switch_core_media_bug_prune(session);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_BREAK) {		
		if ((switch_channel_test_flag(session->channel, CF_QUEUE_TEXT_EVENTS) || switch_channel_test_flag(session->channel, CF_FIRE_TEXT_EVENTS)) && 
			(*frame)->datalen && !switch_test_flag((*frame), SFF_CNG)) {
			int ok = 1;
			switch_event_t *event;
			void *data = (*frame)->data;
			char eof[1] = {'\0'};

			//uint32_t datalen = (*frame)->datalen;

			if (!switch_channel_test_flag(session->channel, CF_TEXT_LINE_BASED)) {
				if (!session->text_line_buffer) {
					switch_buffer_create_dynamic(&session->text_line_buffer, 512, 1024, 0);
				}
				switch_buffer_write(session->text_line_buffer, (*frame)->data, (*frame)->datalen);


				if (switch_channel_test_flag(session->channel, CF_TEXT_IDLE) || switch_test_flag((*frame), SFF_TEXT_LINE_BREAK)) {
					switch_buffer_write(session->text_line_buffer, eof, 1);
					data = switch_buffer_get_head_pointer(session->text_line_buffer);
					//datalen = strlen((char *)smh->line_text_frame.data);
				} else {
					ok = 0;
				}
			}


			if (ok) {
				if (switch_event_create(&event, SWITCH_EVENT_TEXT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(session->channel, event);

					switch_event_add_body(event, "%s", (char *)data);

					if (switch_channel_test_flag(session->channel, CF_QUEUE_TEXT_EVENTS)) {
						switch_event_t *q_event = NULL;

						if (switch_channel_test_flag(session->channel, CF_FIRE_TEXT_EVENTS)) {
							switch_event_dup(&q_event, event);
						} else {
							q_event = event;
							event = NULL;
						}

						switch_core_session_queue_event(session, &q_event);
					}
					
					if (switch_channel_test_flag(session->channel, CF_FIRE_TEXT_EVENTS)) {
						switch_event_fire(&event);
					}
				}
				if (session->text_line_buffer) {
					switch_buffer_zero(session->text_line_buffer);
				}
			}
		}
		switch_core_session_text_read_callback(session, *frame);
	}

 done:

	return status;
}

static void build_red_packet(switch_rtp_engine_t *t_engine)
{
	int pos;
	switch_frame_t *frame = &t_engine->tf->text_write_frame;
	switch_byte_t *buf = (switch_byte_t *) frame->data;
	uint32_t plen = 0, loops = 0;
	uint16_t *u16;

	pos = t_engine->tf->red_pos + 1;

	if (pos == t_engine->tf->red_max) pos = 0;

	for (;;) {
		uint16_t ts = frame->timestamp - t_engine->tf->red_ts[pos];
		uint16_t len = t_engine->tf->red_buflen[pos];

		loops++;

		//1
		*buf = t_engine->t140_pt & 0x7f;

		if (pos != t_engine->tf->red_pos) {
			*buf |= 0x80;

			buf++; //2
			u16 = (uint16_t *) buf;
			*u16 = htons(ts << 2);
			buf++;//3
			*buf += (len & 0x300) >> 8;
			buf++;//4
			*buf = len & 0xff;
		}

		buf++;

		if (pos == t_engine->tf->red_pos) break;


		pos++;

		if (pos == t_engine->tf->red_max) pos = 0;
	}


	plen = ((loops - 1) * 4) + 1;
	pos = t_engine->tf->red_pos + 1;

	if (pos == t_engine->tf->red_max) pos = 0;

	for (;;) {
		if (t_engine->tf->red_buflen[pos]) {
			memcpy(buf, t_engine->tf->red_buf[pos], t_engine->tf->red_buflen[pos]);
			plen += t_engine->tf->red_buflen[pos];
			buf += t_engine->tf->red_buflen[pos];
		}

		if (pos == t_engine->tf->red_pos) break;

		pos++;

		if (pos == t_engine->tf->red_max) pos = 0;
	}


	buf = frame->data;
	*(buf+plen) = '\0';

	frame->datalen = plen;
	frame->payload = t_engine->red_pt;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_write_text_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																	  int stream_id)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_media_handle_t *smh;
	switch_io_event_hook_text_write_frame_t *ptr;
	switch_rtp_engine_t *t_engine;
	switch_io_write_text_frame_t write_text_frame = NULL;
	int is_msrp = switch_channel_test_flag(session->channel, CF_MSRP);

	switch_assert(session);

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_down(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_TEXT) == SWITCH_MEDIA_FLOW_RECVONLY || switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_TEXT) == SWITCH_MEDIA_FLOW_INACTIVE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG3, "Writing text to RECVONLY/INACTIVE session\n");
		return SWITCH_STATUS_SUCCESS;
	}

	//if (switch_channel_test_flag(session->channel, CF_TEXT_PAUSE_WRITE)) {
	//	return SWITCH_STATUS_SUCCESS;
	//}

	if (smh->write_mutex[SWITCH_MEDIA_TYPE_TEXT] && switch_mutex_trylock(smh->write_mutex[SWITCH_MEDIA_TYPE_TEXT]) != SWITCH_STATUS_SUCCESS) {
		/* return CNG, another thread is already writing  */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s is already being written to for %s\n",
						  switch_channel_get_name(session->channel), type2str(SWITCH_MEDIA_TYPE_TEXT));
		goto done;
	}

	t_engine = &smh->engines[SWITCH_MEDIA_TYPE_TEXT];

	if (!is_msrp && switch_channel_test_cap(session->channel, CC_RTP_RTT)) {

		if (frame) {
			char *str = (char *) frame->data;
			switch_buffer_write(t_engine->tf->write_buffer, str, frame->datalen);
		}

		if (!switch_buffer_inuse(t_engine->tf->write_buffer)) {
			t_engine->tf->write_empty++;
			switch_goto_status(SWITCH_STATUS_BREAK, done);
		}

		frame = &t_engine->tf->text_write_frame;
		switch_core_timer_sync(&t_engine->tf->timer);
		frame->timestamp = t_engine->tf->timer.samplecount;

		if (t_engine->red_pt) {
			t_engine->tf->red_ts[t_engine->tf->red_pos] = frame->timestamp;

			if (t_engine->tf->write_empty > TEXT_PERIOD_TIMEOUT / TEXT_TIMER_MS) {
				int pos;

				for(pos = 0; pos < t_engine->tf->red_max; pos++) {
					t_engine->tf->red_ts[pos] = 0;
					t_engine->tf->red_buf[pos][0] = '\0';
					t_engine->tf->red_buflen[pos] = 0;
				}

				frame->m = 1;
				t_engine->tf->write_empty = 0;

			} else {
				frame->m = 0;
			}

			t_engine->tf->red_buflen[t_engine->tf->red_pos] =
				switch_buffer_read(t_engine->tf->write_buffer, t_engine->tf->red_buf[t_engine->tf->red_pos], RED_PACKET_SIZE);

			*(t_engine->tf->red_buf[t_engine->tf->red_pos] + t_engine->tf->red_buflen[t_engine->tf->red_pos]) = '\0';

			build_red_packet(t_engine);
		} else {
			frame->datalen = switch_buffer_read(t_engine->tf->write_buffer, t_engine->tf->text_write_frame.data, RED_PACKET_SIZE);
			frame->payload = t_engine->t140_pt;
		}
	}

	if (!(write_text_frame = session->endpoint_interface->io_routines->write_text_frame)) {
		if (session->io_override) {
			write_text_frame = session->io_override->write_text_frame;
		}
	}

	if (write_text_frame) {
		if ((status = write_text_frame(session, frame, flags, stream_id)) == SWITCH_STATUS_SUCCESS) {
			for (ptr = session->event_hooks.text_write_frame; ptr; ptr = ptr->next) {
				if ((status = ptr->text_write_frame(session, frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}
	}


	if (!is_msrp && switch_channel_test_cap(session->channel, CC_RTP_RTT)) {
		if (t_engine->red_pt) {
			t_engine->tf->red_pos++;
			if (t_engine->tf->red_pos == t_engine->tf->red_max) {
				t_engine->tf->red_pos = 0;
			}
		}
	}

 done:

	if (smh->write_mutex[SWITCH_MEDIA_TYPE_TEXT]) {
		switch_mutex_unlock(smh->write_mutex[SWITCH_MEDIA_TYPE_TEXT]);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_printf(switch_core_session_t *session, const char *fmt, ...)
{
	char *data = NULL;
	int ret = 0;
	va_list ap;
	switch_frame_t frame = { 0 };
	unsigned char CR[] = TEXT_UNICODE_LINEFEED;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		abort();
	}

	frame.data = data;
	frame.datalen = strlen(data);

	switch_core_session_write_text_frame(session, &frame, 0, 0);

	frame.data = CR;
	frame.datalen = 3;

	switch_core_session_write_text_frame(session, &frame, 0, 0);

	switch_safe_free(data);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_print(switch_core_session_t *session, const char *data)
{
	switch_frame_t frame = { 0 };

	if (!switch_channel_test_flag(session->channel, CF_HAS_TEXT)) {
		return SWITCH_STATUS_NOTIMPL;
	}

	frame.data = (char *) data;
	frame.datalen = strlen(data);

	switch_core_session_write_text_frame(session, &frame, 0, 0);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_msrp_session_t *) switch_core_media_get_msrp_session(switch_core_session_t *session)
{
	if (!session->media_handle) return NULL;

	return session->media_handle->msrp_session;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_write_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags,
																int stream_id)
{

	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_frame_t *enc_frame = NULL, *write_frame = frame;
	unsigned int flag = 0, need_codec = 0, perfect = 0, do_bugs = 0, do_write = 0, do_resample = 0, ptime_mismatch = 0, pass_cng = 0, resample = 0;
	int did_write_resample = 0;

	switch_assert(session != NULL);
	switch_assert(frame != NULL);

	if (!switch_channel_up_nosig(session->channel)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_mutex_trylock(session->codec_write_mutex) == SWITCH_STATUS_SUCCESS) {
		switch_mutex_unlock(session->codec_write_mutex);
	} else {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(frame, SFF_CNG)) {
		if (switch_channel_test_flag(session->channel, CF_ACCEPT_CNG)) {
			pass_cng = 1;
		} else {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	if (switch_channel_test_flag(session->channel, CF_AUDIO_PAUSE_WRITE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(session->write_codec && switch_core_codec_ready(session->write_codec)) && !pass_cng) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has no write codec.\n", switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(session->channel, CF_HOLD)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(frame, SFF_PROXY_PACKET) || pass_cng) {
		/* Fast PASS! */
		switch_mutex_lock(session->codec_write_mutex);
		status = perform_write(session, frame, flag, stream_id);
		switch_mutex_unlock(session->codec_write_mutex);
		return status;
	}

	switch_mutex_lock(session->codec_write_mutex);

	if (!(frame->codec && frame->codec->implementation)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "%s has received a bad frame with no codec!\n",
						  switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_mutex_unlock(session->codec_write_mutex);
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(frame->codec != NULL);
	switch_assert(frame->codec->implementation != NULL);

	if (!(switch_core_codec_ready(session->write_codec) && frame->codec) ||
		!switch_channel_ready(session->channel) || !switch_channel_media_ready(session->channel)) {
		switch_mutex_unlock(session->codec_write_mutex);
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(session->write_codec->mutex);
	switch_mutex_lock(frame->codec->mutex);

	if (!(switch_core_codec_ready(session->write_codec) && switch_core_codec_ready(frame->codec))) goto error;

	if ((session->write_codec && frame->codec && session->write_codec->implementation != frame->codec->implementation)) {
		if (session->write_impl.codec_id == frame->codec->implementation->codec_id ||
			session->write_impl.microseconds_per_packet != frame->codec->implementation->microseconds_per_packet) {
			ptime_mismatch = TRUE;

			if ((switch_test_flag(frame->codec, SWITCH_CODEC_FLAG_PASSTHROUGH) || switch_test_flag(session->read_codec, SWITCH_CODEC_FLAG_PASSTHROUGH)) ||
				switch_channel_test_flag(session->channel, CF_PASSTHRU_PTIME_MISMATCH)) {
				status = perform_write(session, frame, flags, stream_id);
				goto error;
			}

			if (session->write_impl.microseconds_per_packet < frame->codec->implementation->microseconds_per_packet) {
				switch_core_session_start_audio_write_thread(session);
			}
		}
		need_codec = TRUE;
	}

	if (session->write_codec && !frame->codec) {
		need_codec = TRUE;
	}

	if (session->bugs && !need_codec && !switch_test_flag(session, SSF_MEDIA_BUG_TAP_ONLY)) {
		do_bugs = TRUE;
		need_codec = TRUE;
	}

	if (frame->codec->implementation->actual_samples_per_second != session->write_impl.actual_samples_per_second) {
		need_codec = TRUE;
		do_resample = TRUE;
	}


	if ((frame->flags & SFF_NOT_AUDIO)) {
		do_resample = 0;
		do_bugs = 0;
		need_codec = 0;
	}

	if (switch_test_flag(session, SSF_WRITE_TRANSCODE) && !need_codec && switch_core_codec_ready(session->write_codec)) {
		switch_core_session_t *other_session;
		const char *uuid = switch_channel_get_partner_uuid(switch_core_session_get_channel(session));

		if (uuid && (other_session = switch_core_session_locate(uuid))) {
			switch_set_flag(other_session, SSF_READ_CODEC_RESET);
			switch_set_flag(other_session, SSF_READ_CODEC_RESET);
			switch_set_flag(other_session, SSF_WRITE_CODEC_RESET);
			switch_core_session_rwunlock(other_session);
		}

		switch_clear_flag(session, SSF_WRITE_TRANSCODE);
	}


	if (switch_test_flag(session, SSF_WRITE_CODEC_RESET)) {
		switch_core_codec_reset(session->write_codec);
		switch_clear_flag(session, SSF_WRITE_CODEC_RESET);
	}

	if (!need_codec) {
		do_write = TRUE;
		write_frame = frame;
		goto done;
	}

	if (!switch_test_flag(session, SSF_WARN_TRANSCODE)) {
		switch_core_session_message_t msg = { 0 };

		msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY;
		switch_core_session_receive_message(session, &msg);
		switch_set_flag(session, SSF_WARN_TRANSCODE);
	}

	if (frame->codec) {
		session->raw_write_frame.datalen = session->raw_write_frame.buflen;
		frame->codec->cur_frame = frame;
		session->write_codec->cur_frame = frame;
		status = switch_core_codec_decode(frame->codec,
										  session->write_codec,
										  frame->data,
										  frame->datalen,
										  session->write_impl.actual_samples_per_second,
										  session->raw_write_frame.data, &session->raw_write_frame.datalen, &session->raw_write_frame.rate, &frame->flags);
		frame->codec->cur_frame = NULL;
		session->write_codec->cur_frame = NULL;
		if (do_resample && status == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_RESAMPLE;
		}

		/* mux or demux to match */
		if (session->write_impl.number_of_channels != frame->codec->implementation->number_of_channels) {
			uint32_t rlen = session->raw_write_frame.datalen / 2 / frame->codec->implementation->number_of_channels;
			switch_mux_channels((int16_t *) session->raw_write_frame.data, rlen,
								frame->codec->implementation->number_of_channels, session->write_impl.number_of_channels);
			session->raw_write_frame.datalen = rlen * 2 * session->write_impl.number_of_channels;
		}

		switch (status) {
		case SWITCH_STATUS_RESAMPLE:
			resample++;
			write_frame = &session->raw_write_frame;
			write_frame->rate = frame->codec->implementation->actual_samples_per_second;
			if (!session->write_resampler) {
				switch_mutex_lock(session->resample_mutex);
				status = switch_resample_create(&session->write_resampler,
												frame->codec->implementation->actual_samples_per_second,
												session->write_impl.actual_samples_per_second,
												session->write_impl.decoded_bytes_per_packet, SWITCH_RESAMPLE_QUALITY, session->write_impl.number_of_channels);


				switch_mutex_unlock(session->resample_mutex);
				if (status != SWITCH_STATUS_SUCCESS) {
					goto done;
				} else {
					switch_core_session_message_t msg = { 0 };
					msg.numeric_arg = 1;
					msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
					switch_core_session_receive_message(session, &msg);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Activating write resampler\n");
				}
			}
			break;
		case SWITCH_STATUS_SUCCESS:
			session->raw_write_frame.samples = session->raw_write_frame.datalen / sizeof(int16_t) / session->write_impl.number_of_channels;
			session->raw_write_frame.channels = session->write_impl.number_of_channels;
			session->raw_write_frame.timestamp = frame->timestamp;
			session->raw_write_frame.rate = frame->rate;
			session->raw_write_frame.m = frame->m;
			session->raw_write_frame.ssrc = frame->ssrc;
			session->raw_write_frame.seq = frame->seq;
			session->raw_write_frame.payload = frame->payload;
			session->raw_write_frame.flags = 0;
			if (switch_test_flag(frame, SFF_PLC)) {
				session->raw_write_frame.flags |= SFF_PLC;
			}

			write_frame = &session->raw_write_frame;
			break;
		case SWITCH_STATUS_BREAK:
			status = SWITCH_STATUS_SUCCESS;
			goto error;
		case SWITCH_STATUS_NOOP:
			if (session->write_resampler) {
				switch_mutex_lock(session->resample_mutex);
				switch_resample_destroy(&session->write_resampler);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Deactivating write resampler\n");
				switch_mutex_unlock(session->resample_mutex);

				{
					switch_core_session_message_t msg = { 0 };
					msg.numeric_arg = 0;
					msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
					switch_core_session_receive_message(session, &msg);
				}

			}
			write_frame = frame;
			status = SWITCH_STATUS_SUCCESS;
			break;
		default:

			if (status == SWITCH_STATUS_NOT_INITALIZED) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec init error!\n");
				goto error;
			}
			if (ptime_mismatch && status != SWITCH_STATUS_GENERR) {
				perform_write(session, frame, flags, stream_id);
				status = SWITCH_STATUS_SUCCESS;
				goto error;
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec %s decoder error!\n",
							  frame->codec->codec_interface->interface_name);
			goto error;
		}
	}



	if (session->write_resampler) {
		short *data = write_frame->data;

		switch_mutex_lock(session->resample_mutex);
		if (session->write_resampler) {

			if (switch_resample_calc_buffer_size(session->write_resampler->to_rate, session->write_resampler->from_rate,
												 write_frame->datalen / 2 / session->write_resampler->channels) > SWITCH_RECOMMENDED_BUFFER_SIZE) {

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "%s not enough buffer space for required resample operation!\n",
								  switch_channel_get_name(session->channel));
				switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				switch_mutex_unlock(session->resample_mutex);
				goto error;
			}


			switch_resample_process(session->write_resampler, data, write_frame->datalen / 2 / session->write_resampler->channels);

			memcpy(data, session->write_resampler->to, session->write_resampler->to_len * 2 * session->write_resampler->channels);

			write_frame->samples = session->write_resampler->to_len;
			write_frame->channels = session->write_resampler->channels;
			write_frame->datalen = write_frame->samples * 2 * session->write_resampler->channels;

			write_frame->rate = session->write_resampler->to_rate;

			did_write_resample = 1;
		}
		switch_mutex_unlock(session->resample_mutex);
	}



	if (session->bugs) {
		switch_media_bug_t *bp;
		int prune = 0;

		switch_thread_rwlock_rdlock(session->bug_rwlock);
		for (bp = session->bugs; bp; bp = bp->next) {
			switch_bool_t ok = SWITCH_TRUE;

			if (!bp->ready) {
				continue;
			}

			if (switch_channel_test_flag(session->channel, CF_PAUSE_BUGS) && !switch_core_media_bug_test_flag(bp, SMBF_NO_PAUSE)) {
				continue;
			}

			if (!switch_channel_test_flag(session->channel, CF_ANSWERED) && switch_core_media_bug_test_flag(bp, SMBF_ANSWER_REQ)) {
				continue;
			}

			if (switch_test_flag(bp, SMBF_PRUNE)) {
				prune++;
				continue;
			}

			if (switch_test_flag(bp, SMBF_WRITE_STREAM)) {
				switch_mutex_lock(bp->write_mutex);
				switch_buffer_write(bp->raw_write_buffer, write_frame->data, write_frame->datalen);
				switch_mutex_unlock(bp->write_mutex);

				if (bp->callback) {
					ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE);
				}
			}

			if (switch_test_flag(bp, SMBF_WRITE_REPLACE)) {
				do_bugs = 0;
				if (bp->callback) {
					bp->write_replace_frame_in = write_frame;
					bp->write_replace_frame_out = write_frame;
					if ((ok = bp->callback(bp, bp->user_data, SWITCH_ABC_TYPE_WRITE_REPLACE)) == SWITCH_TRUE) {
						write_frame = bp->write_replace_frame_out;
					}
				}
			}

			if (bp->stop_time && bp->stop_time <= switch_epoch_time_now(NULL)) {
				ok = SWITCH_FALSE;
			}


			if (ok == SWITCH_FALSE) {
				switch_set_flag(bp, SMBF_PRUNE);
				prune++;
			}
		}
		switch_thread_rwlock_unlock(session->bug_rwlock);
		if (prune) {
			switch_core_media_bug_prune(session);
		}
	}

	if (do_bugs) {
		do_write = TRUE;
		write_frame = frame;
		goto done;
	}

	if (session->write_codec) {
		if (!ptime_mismatch && write_frame->codec && write_frame->codec->implementation &&
			write_frame->codec->implementation->decoded_bytes_per_packet == session->write_impl.decoded_bytes_per_packet) {
			perfect = TRUE;
		}



		if (perfect) {

			if (write_frame->datalen < session->write_impl.decoded_bytes_per_packet) {
				memset(write_frame->data, 255, session->write_impl.decoded_bytes_per_packet - write_frame->datalen);
				write_frame->datalen = session->write_impl.decoded_bytes_per_packet;
			}

			enc_frame = write_frame;
			session->enc_write_frame.datalen = session->enc_write_frame.buflen;
			session->write_codec->cur_frame = frame;
			frame->codec->cur_frame = frame;
			switch_assert(enc_frame->datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
			switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
			status = switch_core_codec_encode(session->write_codec,
											  frame->codec,
											  enc_frame->data,
											  enc_frame->datalen,
											  session->write_impl.actual_samples_per_second,
											  session->enc_write_frame.data, &session->enc_write_frame.datalen, &session->enc_write_frame.rate, &flag);

			switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);

			session->write_codec->cur_frame = NULL;
			frame->codec->cur_frame = NULL;
			switch (status) {
			case SWITCH_STATUS_RESAMPLE:
				resample++;
				/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Fixme 2\n"); */
			case SWITCH_STATUS_SUCCESS:
				session->enc_write_frame.codec = session->write_codec;
				session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t) / session->write_impl.number_of_channels;
				session->enc_write_frame.channels = session->write_impl.number_of_channels;
				if (frame->codec->implementation->samples_per_packet != session->write_impl.samples_per_packet) {
					session->enc_write_frame.timestamp = 0;
				} else {
					session->enc_write_frame.timestamp = frame->timestamp;
				}
				session->enc_write_frame.payload = session->write_impl.ianacode;
				session->enc_write_frame.m = frame->m;
				session->enc_write_frame.ssrc = frame->ssrc;
				session->enc_write_frame.seq = frame->seq;
				session->enc_write_frame.flags = 0;
				write_frame = &session->enc_write_frame;
				break;
			case SWITCH_STATUS_NOOP:
				enc_frame->codec = session->write_codec;
				enc_frame->samples = enc_frame->datalen / sizeof(int16_t) / session->write_impl.number_of_channels;
				enc_frame->channels = session->write_impl.number_of_channels;
				enc_frame->timestamp = frame->timestamp;
				enc_frame->m = frame->m;
				enc_frame->seq = frame->seq;
				enc_frame->ssrc = frame->ssrc;
				enc_frame->payload = enc_frame->codec->implementation->ianacode;
				write_frame = enc_frame;
				break;
			case SWITCH_STATUS_NOT_INITALIZED:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec init error!\n");
				write_frame = NULL;
				goto error;
			default:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec %s encoder error!\n",
								  session->read_codec->codec_interface->interface_name);
				write_frame = NULL;
				goto error;
			}
			if (flag & SFF_CNG) {
				switch_set_flag(write_frame, SFF_CNG);
			}

			status = perform_write(session, write_frame, flags, stream_id);
			goto error;
		} else {
			if (!session->raw_write_buffer) {
				switch_size_t bytes_per_packet = session->write_impl.decoded_bytes_per_packet;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
								  "Engaging Write Buffer at %u bytes to accommodate %u->%u\n",
								  (uint32_t) bytes_per_packet, write_frame->datalen, session->write_impl.decoded_bytes_per_packet);
				if ((status = switch_buffer_create_dynamic(&session->raw_write_buffer,
														   bytes_per_packet * SWITCH_BUFFER_BLOCK_FRAMES,
														   bytes_per_packet * SWITCH_BUFFER_START_FRAMES, 0)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Write Buffer Failed!\n");
					goto error;
				}

				/* Need to retrain the recording data */
				switch_core_media_bug_flush_all(session);
			}

			if (!(switch_buffer_write(session->raw_write_buffer, write_frame->data, write_frame->datalen))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Write Buffer %u bytes Failed!\n", write_frame->datalen);
				status = SWITCH_STATUS_MEMERR;
				goto error;
			}

			status = SWITCH_STATUS_SUCCESS;

			while (switch_buffer_inuse(session->raw_write_buffer) >= session->write_impl.decoded_bytes_per_packet) {
				int rate;

				if (switch_channel_down(session->channel) || !session->raw_write_buffer) {
					goto error;
				}
				if ((session->raw_write_frame.datalen = (uint32_t)
					 switch_buffer_read(session->raw_write_buffer, session->raw_write_frame.data, session->write_impl.decoded_bytes_per_packet)) == 0) {
					goto error;
				}

				enc_frame = &session->raw_write_frame;
				session->raw_write_frame.rate = session->write_impl.actual_samples_per_second;
				session->enc_write_frame.datalen = session->enc_write_frame.buflen;
				session->enc_write_frame.timestamp = 0;


				if (frame->codec && frame->codec->implementation && switch_core_codec_ready(frame->codec)) {
					rate = frame->codec->implementation->actual_samples_per_second;
				} else {
					rate = session->write_impl.actual_samples_per_second;
				}

				session->write_codec->cur_frame = frame;
				frame->codec->cur_frame = frame;
				switch_assert(enc_frame->datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
				switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
				status = switch_core_codec_encode(session->write_codec,
												  frame->codec,
												  enc_frame->data,
												  enc_frame->datalen,
												  rate,
												  session->enc_write_frame.data, &session->enc_write_frame.datalen, &session->enc_write_frame.rate, &flag);

				switch_assert(session->enc_read_frame.datalen <= SWITCH_RECOMMENDED_BUFFER_SIZE);

				session->write_codec->cur_frame = NULL;
				frame->codec->cur_frame = NULL;
				switch (status) {
				case SWITCH_STATUS_RESAMPLE:
					resample++;
					session->enc_write_frame.codec = session->write_codec;
					session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t) / session->write_impl.number_of_channels;
					session->enc_write_frame.channels = session->write_impl.number_of_channels;
					session->enc_write_frame.m = frame->m;
					session->enc_write_frame.ssrc = frame->ssrc;
					session->enc_write_frame.payload = session->write_impl.ianacode;
					write_frame = &session->enc_write_frame;
					if (!session->write_resampler) {
						switch_mutex_lock(session->resample_mutex);
						if (!session->write_resampler) {
							status = switch_resample_create(&session->write_resampler,
															frame->codec->implementation->actual_samples_per_second,
															session->write_impl.actual_samples_per_second,
															session->write_impl.decoded_bytes_per_packet, SWITCH_RESAMPLE_QUALITY,
															session->write_impl.number_of_channels);
						}
						switch_mutex_unlock(session->resample_mutex);



						if (status != SWITCH_STATUS_SUCCESS) {
							goto done;
						} else {
							switch_core_session_message_t msg = { 0 };
							msg.numeric_arg = 1;
							msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
							switch_core_session_receive_message(session, &msg);


							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Activating write resampler\n");
						}
					}
					break;
				case SWITCH_STATUS_SUCCESS:
					session->enc_write_frame.codec = session->write_codec;
					session->enc_write_frame.samples = enc_frame->datalen / sizeof(int16_t) / session->write_impl.number_of_channels;
					session->enc_write_frame.channels = session->write_impl.number_of_channels;
					session->enc_write_frame.m = frame->m;
					session->enc_write_frame.ssrc = frame->ssrc;
					session->enc_write_frame.payload = session->write_impl.ianacode;
					session->enc_write_frame.flags = 0;
					write_frame = &session->enc_write_frame;
					break;
				case SWITCH_STATUS_NOOP:
					if (session->write_resampler) {
						switch_core_session_message_t msg = { 0 };
						int ok = 0;

						switch_mutex_lock(session->resample_mutex);
						if (session->write_resampler) {
							switch_resample_destroy(&session->write_resampler);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Deactivating write resampler\n");
							ok = 1;
						}
						switch_mutex_unlock(session->resample_mutex);

						if (ok) {
							msg.numeric_arg = 0;
							msg.message_id = SWITCH_MESSAGE_RESAMPLE_EVENT;
							switch_core_session_receive_message(session, &msg);
						}

					}
					enc_frame->codec = session->write_codec;
					enc_frame->samples = enc_frame->datalen / sizeof(int16_t) / session->read_impl.number_of_channels;
					enc_frame->channels = session->read_impl.number_of_channels;
					enc_frame->m = frame->m;
					enc_frame->ssrc = frame->ssrc;
					enc_frame->payload = enc_frame->codec->implementation->ianacode;
					write_frame = enc_frame;
					break;
				case SWITCH_STATUS_NOT_INITALIZED:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec init error!\n");
					write_frame = NULL;
					goto error;
				default:
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec %s encoder error %d!\n",
									  session->read_codec->codec_interface->interface_name, status);
					write_frame = NULL;
					goto error;
				}

				if (!did_write_resample && session->read_resampler) {
					short *data = write_frame->data;
					switch_mutex_lock(session->resample_mutex);
					if (session->read_resampler) {
						switch_resample_process(session->read_resampler, data, write_frame->datalen / 2 / session->read_resampler->channels);
						memcpy(data, session->read_resampler->to, session->read_resampler->to_len * 2 * session->read_resampler->channels);
						write_frame->samples = session->read_resampler->to_len;
						write_frame->channels = session->read_resampler->channels;
						write_frame->datalen = session->read_resampler->to_len * 2 * session->read_resampler->channels;
						write_frame->rate = session->read_resampler->to_rate;
					}
					switch_mutex_unlock(session->resample_mutex);

				}

				if (flag & SFF_CNG) {
					switch_set_flag(write_frame, SFF_CNG);
				}

				if (ptime_mismatch || resample) {
					write_frame->timestamp = 0;
				}

				if ((status = perform_write(session, write_frame, flags, stream_id)) != SWITCH_STATUS_SUCCESS) {
					break;
				}

			}

			goto error;
		}
	}





  done:

	if (ptime_mismatch || resample) {
		write_frame->timestamp = 0;
	}

	if (do_write) {
		status = perform_write(session, write_frame, flags, stream_id);
	}

  error:

	switch_mutex_unlock(session->write_codec->mutex);
	switch_mutex_unlock(frame->codec->mutex);
	switch_mutex_unlock(session->codec_write_mutex);

	return status;
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
