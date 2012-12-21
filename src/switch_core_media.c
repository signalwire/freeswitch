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
 * switch_core_media.c -- Core Media
 *
 */


#include <switch.h>
#include <switch_ssl.h>
#include <switch_stun.h>
#include <switch_nat.h>
#include <switch_version.h>
#include "private/switch_core_pvt.h"
#include <switch_curl.h>
#include <errno.h>



#define MAX_CODEC_CHECK_FRAMES 50//x:mod_sofia.h
#define MAX_MISMATCH_FRAMES 5//x:mod_sofia.h

typedef union {
	int32_t intval;
	uint32_t uintval;
	char *charval;
} scm_multi_t;


static scm_type_t typemap[SCM_MAX] = {
	/*SCM_INBOUND_CODEC_STRING*/  STYPE_CHARVAL, 
	/*SCM_OUTBOUND_CODEC_STRING*/ STYPE_CHARVAL,
	/*SCM_AUTO_RTP_BUGS */ STYPE_INTVAL,
	/*SCM_MANUAL_RTP_BUGS*/ STYPE_INTVAL
};

typedef enum {
	SMH_INIT = (1 << 0),
	SMH_READY = (1 << 1)
} smh_flag_t;


typedef struct secure_settings_s {
	int crypto_tag;
	unsigned char local_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	unsigned char remote_raw_key[SWITCH_RTP_MAX_CRYPTO_LEN];
	switch_rtp_crypto_key_type_t crypto_send_type;
	switch_rtp_crypto_key_type_t crypto_recv_type;
	switch_rtp_crypto_key_type_t crypto_type;
	char *local_crypto_key;
	char *remote_crypto_key;
} switch_secure_settings_t;

typedef struct codec_params_s {//x:tmp
	char *rm_encoding;
	char *iananame;
	switch_payload_t pt;
	unsigned long rm_rate;
	uint32_t codec_ms;
	uint32_t bitrate;
	char *remote_sdp_ip;
	char *rm_fmtp;
	switch_port_t remote_sdp_port;
	switch_payload_t agreed_pt;
	switch_payload_t recv_pt;
	char *fmtp_out;
} codec_params_t;


typedef struct switch_rtp_engine_s {
	switch_secure_settings_t ssec;
	switch_media_type_t type;

	switch_rtp_t *rtp_session;//x:tp
	switch_frame_t read_frame;//x:tp
	switch_codec_t read_codec;//x:tp
	switch_codec_t write_codec;//x:tp

	switch_codec_implementation_t read_impl;//x:tp
	switch_codec_implementation_t write_impl;//x:tp

	uint32_t codec_ms;//x:tp
	switch_size_t last_ts;//x:tp
	uint32_t check_frames;//x:tp
	uint32_t mismatch_count;//x:tp
	uint32_t last_codec_ms;//x:tp
	uint8_t codec_reinvites;//x:tp
	uint32_t max_missed_packets;//x:tp
	uint32_t max_missed_hold_packets;//x:tp



	/** ZRTP **/
	char *local_sdp_zrtp_hash;
	char *remote_sdp_zrtp_hash;


	codec_params_t codec_params;


} switch_rtp_engine_t;


struct switch_media_handle_s {
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_core_media_NDLB_t ndlb;
	switch_core_media_flag_t media_flags[SCMF_MAX];
	smh_flag_t flags;
	switch_rtp_engine_t engines[SWITCH_MEDIA_TYPE_TOTAL];
	scm_multi_t params[SCM_MAX];
	char *codec_order[SWITCH_MAX_CODECS];//x:tp
    int codec_order_last;//x:tp
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];//x:tp
    int num_codecs;//x:tp
	int payload_space;//x:tp
	char *origin;//x:tp
	int hold_laps;//x:tp
	switch_payload_t te;//x:tp
	switch_payload_t recv_te;//x:tp
	switch_payload_t cng_pt;//x:tp
	switch_core_media_dtmf_t dtmf_type;//x:tp
	const switch_codec_implementation_t *negotiated_codecs[SWITCH_MAX_CODECS];//x:tp
	int num_negotiated_codecs;//x:tp
	switch_payload_t ianacodes[SWITCH_MAX_CODECS];//x:tp
	int video_count;//x:tmp

};

SWITCH_DECLARE(void) switch_media_set_param(switch_media_handle_t *smh, scm_param_t param, ...)
{
	scm_multi_t *val = &smh->params[param];
	scm_type_t type = typemap[param];
	va_list ap;

	va_start(ap, param);

	switch(type) {
	case STYPE_INTVAL:
		val->intval = va_arg(ap, int);
		break;
	case STYPE_UINTVAL:
		val->uintval = va_arg(ap, unsigned int);
		break;
	case STYPE_CHARVAL:
		val->charval = switch_core_session_strdup(smh->session, va_arg(ap, char *));
		break;
	default:
		abort();
	}

	va_end(ap);
}

SWITCH_DECLARE(void *) switch_media_get_param(switch_media_handle_t *smh, scm_param_t param)
{
	scm_multi_t *val = &smh->params[param];
	scm_type_t type = typemap[param];

	switch(type) {
	case STYPE_INTVAL:
		return &val->intval;
		break;
	case STYPE_UINTVAL:
		return &val->uintval;
		break;
	case STYPE_CHARVAL:
		return val->charval;
		break;
	default:
		abort();
	}
	
	return NULL;
	
}

#define get_str(_o, _p) _o->params[_p].charval



static void _switch_core_media_pass_zrtp_hash2(switch_core_session_t *aleg_session, switch_core_session_t *bleg_session, switch_media_type_t type)
{
	switch_rtp_engine_t *aleg_engine;
	switch_rtp_engine_t *bleg_engine;

	if (!aleg_session->media_handle && bleg_session->media_handle) return;
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

SWITCH_DECLARE(void) switch_core_media_find_zrtp_hash(switch_core_session_t *session, sdp_session_t *sdp)
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


SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_process_udptl(switch_core_session_t *session, sdp_session_t *sdp, sdp_media_t *m)
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





SWITCH_DECLARE(switch_t38_options_t *) switch_core_media_extract_t38_options(switch_core_session_t *session, const char *r_sdp)
{
	sdp_media_t *m;
	sdp_parser_t *parser = NULL;
	sdp_session_t *sdp;
	//private_object_t *tech_pvt = switch_core_session_get_private(session);
	switch_t38_options_t *t38_options = NULL;

	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	//switch_assert(tech_pvt != NULL);

	for (m = sdp->sdp_media; m; m = m->m_next) {
		if (m->m_proto == sdp_proto_udptl && m->m_type == sdp_media_image && m->m_port) {
			t38_options = switch_core_media_process_udptl(session, sdp, m);
			break;
		}
	}

	sdp_parser_free(parser);

	return t38_options;

}





SWITCH_DECLARE(const char *)switch_core_media_get_codec_string(switch_core_session_t *session)
{
	const char *preferred = NULL, *fallback = NULL;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		preferred = "PCMU";
		fallback = "PCMU";
	} else {
	
		if (!(preferred = switch_channel_get_variable(session->channel, "absolute_codec_string"))) {
			preferred = switch_channel_get_variable(session->channel, "codec_string");
		}
	
		if (!preferred) {
			if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				preferred = get_str(smh, SCM_OUTBOUND_CODEC_STRING);
				fallback = get_str(smh, SCM_INBOUND_CODEC_STRING);

			} else {
				preferred = get_str(smh, SCM_INBOUND_CODEC_STRING);
				fallback = get_str(smh, SCM_OUTBOUND_CODEC_STRING);
			}
		}
	}

	return !zstr(preferred) ? preferred : fallback;
}


SWITCH_DECLARE(const char *) switch_core_session_local_crypto_key(switch_core_session_t *session, switch_media_type_t type)
{
	if (!session->media_handle) {
		return NULL;
	}

	return session->media_handle->engines[type].ssec.local_crypto_key;

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


static switch_status_t switch_core_media_build_crypto(switch_media_handle_t *smh,
											   switch_secure_settings_t *ssec, 
											   int index, switch_rtp_crypto_key_type_t type, switch_rtp_crypto_direction_t direction)
{
	unsigned char b64_key[512] = "";
	const char *type_str;
	unsigned char *key;
	const char *val;
	switch_channel_t *channel;
	char *p;

	switch_assert(smh);
	channel = switch_core_session_get_channel(smh->session);

	if (type == AES_CM_128_HMAC_SHA1_80) {
		type_str = SWITCH_RTP_CRYPTO_KEY_80;
	} else {
		type_str = SWITCH_RTP_CRYPTO_KEY_32;
	}

	if (direction == SWITCH_RTP_CRYPTO_SEND) {
		key = ssec->local_raw_key;
	} else {
		key = ssec->remote_raw_key;

	}

	switch_rtp_get_random(key, SWITCH_RTP_KEY_LEN);
	switch_b64_encode(key, SWITCH_RTP_KEY_LEN, b64_key, sizeof(b64_key));
	p = strrchr((char *) b64_key, '=');

	while (p && *p && *p == '=') {
		*p-- = '\0';
	}

	ssec->local_crypto_key = switch_core_session_sprintf(smh->session, "%d %s inline:%s", index, type_str, b64_key);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(smh->session), SWITCH_LOG_DEBUG, "Set Local Key [%s]\n", ssec->local_crypto_key);

	if (!(smh->ndlb & SM_NDLB_DISABLE_SRTP_AUTH) &&
		!((val = switch_channel_get_variable(channel, "NDLB_support_asterisk_missing_srtp_auth")) && switch_true(val))) {
		ssec->crypto_type = type;
	} else {
		ssec->crypto_type = AES_CM_128_NULL_AUTH;
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
				ssec->crypto_send_type = type;
				memcpy(ssec->local_raw_key, key, SWITCH_RTP_KEY_LEN);
			} else {
				ssec->crypto_recv_type = type;
				memcpy(ssec->remote_raw_key, key, SWITCH_RTP_KEY_LEN);
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


SWITCH_DECLARE(void) switch_core_session_get_recovery_crypto_key(switch_core_session_t *session, switch_media_type_t type, const char *varname)
{
	const char *tmp;
	switch_rtp_engine_t *engine;
	if (!session->media_handle) return;
	engine = &session->media_handle->engines[type];

	if ((tmp = switch_channel_get_variable(session->channel, varname))) {
		engine->ssec.remote_crypto_key = switch_core_session_strdup(session, tmp);
		switch_channel_set_flag(session->channel, CF_CRYPTO_RECOVER);
	}
}


SWITCH_DECLARE(void) switch_core_session_apply_crypto(switch_core_session_t *session, switch_media_type_t type, const char *varname)
{
	switch_rtp_engine_t *engine;
	if (!session->media_handle) return;
	engine = &session->media_handle->engines[type];

	
	if (engine->ssec.remote_crypto_key && switch_channel_test_flag(session->channel, CF_SECURE)) {
		switch_core_media_add_crypto(&engine->ssec, engine->ssec.remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);

		
		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, 1,
								  engine->ssec.crypto_type, engine->ssec.local_raw_key, SWITCH_RTP_KEY_LEN);

		switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, engine->ssec.crypto_tag,
								  engine->ssec.crypto_type, engine->ssec.remote_raw_key, SWITCH_RTP_KEY_LEN);

		switch_channel_set_variable(session->channel, varname, "true");
	}

}


SWITCH_DECLARE(int) switch_core_session_check_incoming_crypto(switch_core_session_t *session, 
															   const char *varname,
															   switch_media_type_t type, const char *crypto, int crypto_tag)
{
	int got_crypto = 0;

	switch_rtp_engine_t *engine;
	if (!session->media_handle) return 0;
	engine = &session->media_handle->engines[type];

	if (engine->ssec.remote_crypto_key && switch_rtp_ready(engine->rtp_session)) {
		/* Compare all the key. The tag may remain the same even if key changed */
		if (crypto && !strcmp(crypto, engine->ssec.remote_crypto_key)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Existing key is still valid.\n");
		} else {
			const char *a = switch_stristr("AES", engine->ssec.remote_crypto_key);
			const char *b = switch_stristr("AES", crypto);

			/* Change our key every time we can */
							
			if (switch_channel_test_flag(session->channel, CF_CRYPTO_RECOVER)) {
				switch_channel_clear_flag(session->channel, CF_CRYPTO_RECOVER);
			} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
				switch_channel_set_variable(session->channel, varname, SWITCH_RTP_CRYPTO_KEY_32);

				switch_core_media_build_crypto(session->media_handle, &engine->ssec, crypto_tag, AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
				switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), engine->ssec.crypto_type,
										  engine->ssec.local_raw_key, SWITCH_RTP_KEY_LEN);
			} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {

				switch_channel_set_variable(session->channel, varname, SWITCH_RTP_CRYPTO_KEY_80);
				switch_core_media_build_crypto(session->media_handle, &engine->ssec, crypto_tag, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
				switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_SEND, atoi(crypto), engine->ssec.crypto_type,
										  engine->ssec.local_raw_key, SWITCH_RTP_KEY_LEN);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
			}

			if (a && b && !strncasecmp(a, b, 23)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Change Remote key to [%s]\n", crypto);
				engine->ssec.remote_crypto_key = switch_core_session_strdup(session, crypto);
				switch_channel_set_variable(session->channel, "srtp_remote_audio_crypto_key", crypto);
				engine->ssec.crypto_tag = crypto_tag;
								
				if (switch_rtp_ready(engine->rtp_session) && switch_channel_test_flag(session->channel, CF_SECURE)) {
					switch_core_media_add_crypto(&engine->ssec, engine->ssec.remote_crypto_key, SWITCH_RTP_CRYPTO_RECV);
					switch_rtp_add_crypto_key(engine->rtp_session, SWITCH_RTP_CRYPTO_RECV, engine->ssec.crypto_tag,
											  engine->ssec.crypto_type, engine->ssec.remote_raw_key, SWITCH_RTP_KEY_LEN);
				}
				got_crypto++;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Ignoring unacceptable key\n");
			}
		}
	} else if (!switch_rtp_ready(engine->rtp_session)) {
		engine->ssec.remote_crypto_key = switch_core_session_strdup(session, crypto);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Remote Key [%s]\n", engine->ssec.remote_crypto_key);
		switch_channel_set_variable(session->channel, "srtp_remote_audio_crypto_key", crypto);
		engine->ssec.crypto_tag = crypto_tag;
		got_crypto++;

		if (zstr(engine->ssec.local_crypto_key)) {
			if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_32, crypto)) {
				switch_channel_set_variable(session->channel, varname, SWITCH_RTP_CRYPTO_KEY_32);
				switch_core_media_build_crypto(session->media_handle, &engine->ssec, crypto_tag, AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
			} else if (switch_stristr(SWITCH_RTP_CRYPTO_KEY_80, crypto)) {
				switch_channel_set_variable(session->channel, varname, SWITCH_RTP_CRYPTO_KEY_80);
				switch_core_media_build_crypto(session->media_handle, &engine->ssec, crypto_tag, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Crypto Setup Failed!.\n");
			}
		}
	}	

	return got_crypto;
}


SWITCH_DECLARE(void) switch_core_session_check_outgoing_crypto(switch_core_session_t *session, const char *sec_var)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *var;

	if (!switch_core_session_media_handle_ready(session)) return;
	
	if ((var = switch_channel_get_variable(channel, sec_var)) && !zstr(var)) {
		if (switch_true(var) || !strcasecmp(var, SWITCH_RTP_CRYPTO_KEY_32)) {
			switch_channel_set_flag(channel, CF_SECURE);
			switch_core_media_build_crypto(session->media_handle,
										   &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec, 1, AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
			switch_core_media_build_crypto(session->media_handle,
										   &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec, 1, AES_CM_128_HMAC_SHA1_32, SWITCH_RTP_CRYPTO_SEND);
		} else if (!strcasecmp(var, SWITCH_RTP_CRYPTO_KEY_80)) {
			switch_channel_set_flag(channel, CF_SECURE);
			switch_core_media_build_crypto(session->media_handle,
										   &session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].ssec, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
			switch_core_media_build_crypto(session->media_handle,
										   &session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].ssec, 1, AES_CM_128_HMAC_SHA1_80, SWITCH_RTP_CRYPTO_SEND);
		}
	}
	
}


SWITCH_DECLARE(switch_status_t) switch_media_handle_create(switch_media_handle_t **smhp, switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_media_handle_t *smh = NULL;
	
	*smhp = NULL;

	if ((session->media_handle = switch_core_session_alloc(session, (sizeof(*smh))))) {
		session->media_handle->session = session;
		*smhp = session->media_handle;
		switch_set_flag(session->media_handle, SMH_INIT);
		session->media_handle->media_flags[SCMF_RUNNING] = 1;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_AUDIO].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;
		session->media_handle->engines[SWITCH_MEDIA_TYPE_VIDEO].read_frame.buflen = SWITCH_RTP_MAX_BUF_LEN;

		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}

SWITCH_DECLARE(void) switch_media_handle_set_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);

	smh->ndlb |= flag;
	
}

SWITCH_DECLARE(void) switch_media_handle_clear_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);

	smh->ndlb &= ~flag;
}

SWITCH_DECLARE(int32_t) switch_media_handle_test_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);
	return (smh->ndlb & flag);
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
		smh->media_flags[i] = flags[i];
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
	if (session->media_handle && switch_test_flag(session->media_handle, SMH_INIT)) {
		return SWITCH_STATUS_SUCCESS;
	}
	
	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_media_handle_t *) switch_core_session_get_media_handle(switch_core_session_t *session)
{
	if (switch_core_session_media_handle_ready(session)) {
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


SWITCH_DECLARE(void) switch_core_media_prepare_codecs(switch_core_session_t *session, switch_bool_t force)
{
	const char *abs, *codec_string = NULL;
	const char *ocodec = NULL;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return;
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) || switch_channel_test_flag(session->channel, CF_PROXY_MEDIA)) {
		return;
	}

	if (force) {
		smh->num_codecs = 0;
	}

	if (smh->num_codecs) {
		return;
	}

	smh->payload_space = 0;

	switch_assert(smh->session != NULL);

	if ((abs = switch_channel_get_variable(session->channel, "absolute_codec_string"))) {
		/* inherit_codec == true will implicitly clear the absolute_codec_string 
		   variable if used since it was the reason it was set in the first place and is no longer needed */
		if (switch_true(switch_channel_get_variable(session->channel, "inherit_codec"))) {
			switch_channel_set_variable(session->channel, "absolute_codec_string", NULL);
		}
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
		smh->codec_order_last = switch_separate_string(tmp_codec_string, ',', smh->codec_order, SWITCH_MAX_CODECS);
		smh->num_codecs = switch_loadable_module_get_codecs_sorted(smh->codecs, SWITCH_MAX_CODECS, smh->codec_order, smh->codec_order_last);
	} else {
		smh->num_codecs = switch_loadable_module_get_codecs(smh->codecs, sizeof(smh->codecs) / sizeof(smh->codecs[0]));
	}
}



SWITCH_DECLARE(switch_status_t) switch_core_session_read_media_frame(switch_core_session_t *session, switch_frame_t **frame,
																	 switch_io_flag_t flags, int stream_id, switch_media_type_t type)
{
	switch_rtcp_frame_t rtcp_frame;
	switch_rtp_engine_t *engine;
	switch_status_t status;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(smh->flags & SCMF_RUNNING)) {
		return SWITCH_STATUS_FALSE;
	}

	engine = &smh->engines[type];

	engine->read_frame.datalen = 0;

	if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(engine->rtp_session != NULL);
	engine->read_frame.datalen = 0;

	
	while ((smh->flags & SCMF_RUNNING) && engine->read_frame.datalen == 0) {
		engine->read_frame.flags = SFF_NONE;

		status = switch_rtp_zerocopy_read_frame(engine->rtp_session, &engine->read_frame, flags);
			
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			if (status == SWITCH_STATUS_TIMEOUT) {

#if 0
				if (sofia_test_flag(engine, TFLAG_SIP_HOLD)) {
					switch_core_media_toggle_hold(engine, 0);
					sofia_clear_flag_locked(engine, TFLAG_SIP_HOLD);
					switch_channel_clear_flag(channel, CF_LEG_HOLDING);
				}
#endif

				if (switch_channel_get_variable(session->channel, "execute_on_media_timeout")) {
					*frame = &engine->read_frame;
					switch_set_flag((*frame), SFF_CNG);
					(*frame)->datalen = engine->read_impl.encoded_bytes_per_packet;
					memset((*frame)->data, 0, (*frame)->datalen);
					switch_channel_execute_on(session->channel, "execute_on_media_timeout");
					return SWITCH_STATUS_SUCCESS;
				}


				switch_channel_hangup(session->channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
			}
			return status;
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
			return SWITCH_STATUS_SUCCESS;
		}

		if (switch_rtp_has_dtmf(engine->rtp_session)) {
			switch_dtmf_t dtmf = { 0 };
			switch_rtp_dequeue_dtmf(engine->rtp_session, &dtmf);
			switch_channel_queue_dtmf(session->channel, &dtmf);
		}

		if (engine->read_frame.datalen > 0) {
			uint32_t bytes = 0;
			int frames = 1;

			if (!switch_test_flag((&engine->read_frame), SFF_CNG)) {
				if (!engine->read_codec.implementation || !switch_core_codec_ready(&engine->read_codec)) {
					*frame = NULL;
					return SWITCH_STATUS_GENERR;
				}

				if ((engine->read_frame.datalen % 10) == 0 &&
					(smh->media_flags[SCMF_AUTOFIX_TIMING]) && engine->check_frames < MAX_CODEC_CHECK_FRAMES) {
					engine->check_frames++;

					if (!engine->read_impl.encoded_bytes_per_packet) {
						engine->check_frames = MAX_CODEC_CHECK_FRAMES;
						goto skip;
					}

					if (engine->last_ts && engine->read_frame.datalen != engine->read_impl.encoded_bytes_per_packet) {
						uint32_t codec_ms = (int) (engine->read_frame.timestamp -
												   engine->last_ts) / (engine->read_impl.samples_per_second / 1000);

						if ((codec_ms % 10) != 0 || codec_ms > engine->read_impl.samples_per_packet * 10) {
							engine->last_ts = 0;
							goto skip;
						}


						if (engine->last_codec_ms && engine->last_codec_ms == codec_ms) {
							engine->mismatch_count++;
						}

						engine->last_codec_ms = codec_ms;

						if (engine->mismatch_count > MAX_MISMATCH_FRAMES) {
							if (switch_rtp_ready(engine->rtp_session) && codec_ms != engine->codec_ms) {
								const char *val;
								int rtp_timeout_sec = 0;
								int rtp_hold_timeout_sec = 0;

								if (codec_ms > 120) {	/* yeah right */
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
													  "Your phone is trying to send timestamps that suggest an increment of %dms per packet\n"
													  "That seems hard to believe so I am going to go on ahead and um ignore that, mmkay?\n",
													  (int) codec_ms);
									engine->check_frames = MAX_CODEC_CHECK_FRAMES;
									goto skip;
								}

								engine->read_frame.datalen = 0;

								if (codec_ms != engine->codec_ms) {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
													  "Asynchronous PTIME not supported, changing our end from %d to %d\n",
													  (int) engine->codec_ms,
													  (int) codec_ms
													  );

									switch_channel_set_variable_printf(session->channel, "sip_h_X-Broken-PTIME", "Adv=%d;Sent=%d",
																	   (int) engine->codec_ms, (int) codec_ms);

									engine->codec_ms = codec_ms;
								}

#if fixmeXXXXX
								if (switch_core_media_tech_set_codec(engine, 2) != SWITCH_STATUS_SUCCESS) {
									*frame = NULL;
									return SWITCH_STATUS_GENERR;
								}
#endif
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


								engine->check_frames = 0;
								engine->last_ts = 0;

								*frame = &engine->read_frame;
								switch_set_flag((*frame), SFF_CNG);
								(*frame)->datalen = engine->read_impl.encoded_bytes_per_packet;
								memset((*frame)->data, 0, (*frame)->datalen);
								return SWITCH_STATUS_SUCCESS;
							}

						}

					} else {
						engine->mismatch_count = 0;
					}

					engine->last_ts = engine->read_frame.timestamp;


				} else {
					engine->mismatch_count = 0;
					engine->last_ts = 0;
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
		return SWITCH_STATUS_GENERR;
	}

	*frame = &engine->read_frame;

	return SWITCH_STATUS_SUCCESS;
}


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


SWITCH_DECLARE(switch_status_t) switch_core_media_get_offered_pt(switch_core_session_t *session, const switch_codec_implementation_t *mimp, switch_payload_t *pt)
{
	int i = 0;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}


	for (i = 0; i < smh->num_codecs; i++) {
		const switch_codec_implementation_t *imp = smh->codecs[i];

		if (!strcasecmp(imp->iananame, mimp->iananame)) {
			*pt = smh->ianacodes[i];

			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

//?
switch_status_t switch_core_media_set_video_codec(switch_core_session_t *session, int force)
{
	switch_media_handle_t *smh;
	switch_rtp_engine_t *v_engine;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];


	if (!v_engine->codec_params.rm_encoding) {
		return SWITCH_STATUS_FALSE;
	}

	if (v_engine->read_codec.implementation && switch_core_codec_ready(&v_engine->read_codec)) {
		if (!force) {
			return SWITCH_STATUS_SUCCESS;
		}
		if (strcasecmp(v_engine->read_codec.implementation->iananame, v_engine->codec_params.rm_encoding) ||
			v_engine->read_codec.implementation->samples_per_second != v_engine->codec_params.rm_rate) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Changing Codec from %s to %s\n",
							  v_engine->read_codec.implementation->iananame, v_engine->codec_params.rm_encoding);
			switch_core_codec_destroy(&v_engine->read_codec);
			switch_core_codec_destroy(&v_engine->write_codec);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Already using %s\n",
							  v_engine->read_codec.implementation->iananame);
			return SWITCH_STATUS_SUCCESS;
		}
	}



	if (switch_core_codec_init(&v_engine->read_codec,
							   v_engine->codec_params.rm_encoding,
							   v_engine->codec_params.rm_fmtp,
							   v_engine->codec_params.rm_rate,
							   0,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	} else {
		if (switch_core_codec_init(&v_engine->write_codec,
								   v_engine->codec_params.rm_encoding,
								   v_engine->codec_params.rm_fmtp,
								   v_engine->codec_params.rm_rate,
								   0,
								   1,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
			return SWITCH_STATUS_FALSE;
		} else {
			v_engine->read_frame.rate = v_engine->codec_params.rm_rate;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set VIDEO Codec %s %s/%ld %d ms\n",
							  switch_channel_get_name(session->channel), v_engine->codec_params.rm_encoding, 
							  v_engine->codec_params.rm_rate, v_engine->codec_params.codec_ms);
			v_engine->read_frame.codec = &v_engine->read_codec;

			v_engine->write_codec.fmtp_out = switch_core_session_strdup(session, v_engine->write_codec.fmtp_out);

			v_engine->write_codec.agreed_pt = v_engine->codec_params.agreed_pt;
			v_engine->read_codec.agreed_pt = v_engine->codec_params.agreed_pt;
			switch_core_session_set_video_read_codec(session, &v_engine->read_codec);
			switch_core_session_set_video_write_codec(session, &v_engine->write_codec);


			if (switch_rtp_ready(v_engine->rtp_session)) {
				switch_core_session_message_t msg = { 0 };

				msg.from = __FILE__;
				msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;

				switch_rtp_set_default_payload(v_engine->rtp_session, v_engine->codec_params.agreed_pt);
				
				if (v_engine->codec_params.recv_pt != v_engine->codec_params.agreed_pt) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
									  "%s Set video receive payload to %u\n", switch_channel_get_name(session->channel), v_engine->codec_params.recv_pt);
					
					switch_rtp_set_recv_pt(v_engine->rtp_session, v_engine->codec_params.recv_pt);
				} else {
					switch_rtp_set_recv_pt(v_engine->rtp_session, v_engine->codec_params.agreed_pt);
				}

				switch_core_session_receive_message(session, &msg);


			}

			switch_channel_set_variable(session->channel, "sip_use_video_codec_name", v_engine->codec_params.rm_encoding);
			switch_channel_set_variable(session->channel, "sip_use_video_codec_fmtp", v_engine->codec_params.rm_fmtp);
			switch_channel_set_variable_printf(session->channel, "sip_use_video_codec_rate", "%d", v_engine->codec_params.rm_rate);
			switch_channel_set_variable_printf(session->channel, "sip_use_video_codec_ptime", "%d", 0);

		}
	}
	return SWITCH_STATUS_SUCCESS;
}


//?
switch_status_t switch_core_media_set_codec(switch_core_session_t *session, int force, uint32_t codec_flags)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int resetting = 0;
	switch_media_handle_t *smh;
	switch_rtp_engine_t *a_engine;

	if (!(smh = session->media_handle)) {
		return SWITCH_STATUS_FALSE;
	}
	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];

	if (!a_engine->codec_params.iananame) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No audio codec available\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (switch_core_codec_ready(&a_engine->read_codec)) {
		if (!force) {
			switch_goto_status(SWITCH_STATUS_SUCCESS, end);
		}
		if (strcasecmp(a_engine->read_impl.iananame, a_engine->codec_params.iananame) ||
			a_engine->read_impl.samples_per_second != a_engine->codec_params.rm_rate ||
			a_engine->codec_ms != (uint32_t) a_engine->read_impl.microseconds_per_packet / 1000) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
							  "Changing Codec from %s@%dms@%dhz to %s@%dms@%luhz\n",
							  a_engine->read_impl.iananame, a_engine->read_impl.microseconds_per_packet / 1000,
							  a_engine->read_impl.samples_per_second,
							  a_engine->codec_params.rm_encoding, 
							  a_engine->codec_params.codec_ms,
							  a_engine->codec_params.rm_rate);
			
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
											a_engine->codec_params.iananame,
											a_engine->codec_params.rm_fmtp,
											a_engine->codec_params.rm_rate,
											a_engine->codec_params.codec_ms,
											1,
											a_engine->codec_params.bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	
	a_engine->read_codec.session = session;


	if (switch_core_codec_init_with_bitrate(&a_engine->write_codec,
											a_engine->codec_params.iananame,
											a_engine->codec_params.rm_fmtp,
											a_engine->codec_params.rm_rate,
											a_engine->codec_params.codec_ms,
											1,
											a_engine->codec_params.bitrate,
											SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE | codec_flags,
											NULL, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	a_engine->write_codec.session = session;

	switch_channel_set_variable(session->channel, "sip_use_codec_name", a_engine->codec_params.iananame);
	switch_channel_set_variable(session->channel, "sip_use_codec_fmtp", a_engine->codec_params.rm_fmtp);
	switch_channel_set_variable_printf(session->channel, "sip_use_codec_rate", "%d", a_engine->codec_params.rm_rate);
	switch_channel_set_variable_printf(session->channel, "sip_use_codec_ptime", "%d", a_engine->codec_params.codec_ms);


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

	a_engine->read_frame.rate = a_engine->codec_params.rm_rate;

	if (!switch_core_codec_ready(&a_engine->read_codec)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Codec %s %s/%ld %d ms %d samples %d bits\n",
					  switch_channel_get_name(session->channel), a_engine->codec_params.iananame, a_engine->codec_params.rm_rate, 
					  a_engine->codec_params.codec_ms,
					  a_engine->read_impl.samples_per_packet, a_engine->read_impl.bits_per_second);
	a_engine->read_frame.codec = &a_engine->read_codec;

	a_engine->write_codec.agreed_pt = a_engine->codec_params.agreed_pt;
	a_engine->read_codec.agreed_pt = a_engine->codec_params.agreed_pt;

	if (force != 2) {
		switch_core_session_set_real_read_codec(session, &a_engine->read_codec);
		switch_core_session_set_write_codec(session, &a_engine->write_codec);
	}

	a_engine->codec_params.fmtp_out = switch_core_session_strdup(session, a_engine->write_codec.fmtp_out);

	if (switch_rtp_ready(a_engine->rtp_session)) {
		switch_rtp_set_default_payload(a_engine->rtp_session, a_engine->codec_params.pt);
	}

 end:
	if (resetting) {
		switch_core_session_unlock_codec_write(session);
		switch_core_session_unlock_codec_read(session);
	}

	switch_core_media_set_video_codec(session, force);

	return status;
}

//?
void switch_core_media_check_video_codecs(switch_core_session_t *session)
{
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return;
	}

	if (smh->num_codecs && !switch_channel_test_flag(session->channel, CF_VIDEO_POSSIBLE)) {
		int i;
		smh->video_count = 0;
		for (i = 0; i < smh->num_codecs; i++) {
			
			if (smh->codecs[i]->codec_type == SWITCH_CODEC_TYPE_VIDEO) {
				smh->video_count++;
			}
		}
		if (smh->video_count) {
			switch_channel_set_flag(session->channel, CF_VIDEO_POSSIBLE);
		}
	}
}



//?
SWITCH_DECLARE(uint8_t) switch_core_media_negotiate_sdp(switch_core_session_t *session, const char *r_sdp, uint8_t *proceed, 
														int reinvite, int codec_flags, switch_payload_t default_te)
{
	uint8_t match = 0;
	switch_payload_t best_te = 0, te = 0, cng_pt = 0;
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
	switch_rtp_engine_t *a_engine, *v_engine;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];


	codec_array = smh->codecs;
	total_codecs = smh->num_codecs;


	if (!(parser = sdp_parse(NULL, r_sdp, (int) strlen(r_sdp), 0))) {
		return 0;
	}

	if (!(sdp = sdp_session(parser))) {
		sdp_parser_free(parser);
		return 0;
	}

	if (proceed) *proceed = 1;

	greedy = !!switch_media_handle_test_media_flag(smh, SCMF_CODEC_GREEDY);
	scrooge = !!switch_media_handle_test_media_flag(smh, SCMF_CODEC_SCROOGE);

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

	if ((smh->origin = switch_core_session_strdup(session, (char *) sdp->sdp_origin->o_username))) {

		if ((smh->params[SCM_AUTO_RTP_BUGS].intval & RTP_BUG_CISCO_SKIP_MARK_BIT_2833)) {

			if (strstr(smh->origin, "CiscoSystemsSIP-GW-UserAgent")) {
				smh->params[SCM_MANUAL_RTP_BUGS].intval |= RTP_BUG_CISCO_SKIP_MARK_BIT_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activate Buggy RFC2833 Mode!\n");
			}
		}

		if ((smh->params[SCM_AUTO_RTP_BUGS].intval & RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833)) {
			if (strstr(smh->origin, "Sonus_UAC")) {
				smh->params[SCM_MANUAL_RTP_BUGS].intval |= RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
								  "Hello,\nI see you have a Sonus!\n"
								  "FYI, Sonus cannot follow the RFC on the proper way to send DTMF.\n"
								  "Sadly, my creator had to spend several hours figuring this out so I thought you'd like to know that!\n"
								  "Don't worry, DTMF will work but you may want to ask them to fix it......\n");
			}
		}
	}

	if ((val = switch_channel_get_variable(session->channel, "sip_liberal_dtmf")) && switch_true(val)) {
		switch_channel_set_flag(session->channel, CF_LIBERAL_DTMF);
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
			switch_channel_set_variable(session->channel, "media_audio_mode", "recvonly");
		} else if (!strcasecmp(attr->a_name, "inactive")) {
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
			dptime = atoi(attr->a_value);
		} else if (!strcasecmp(attr->a_name, "maxptime")) {
			dmaxptime = atoi(attr->a_value);
		}
	}

	if (sendonly != 1 && recvonly != 1) {
		switch_channel_set_variable(session->channel, "media_audio_mode", NULL);
	}


	if (switch_media_handle_test_media_flag(smh, SCMF_DISABLE_HOLD) ||
		((val = switch_channel_get_variable(session->channel, "sip_disable_hold")) && switch_true(val))) {
		sendonly = 0;
	} else {

		if (!smh->hold_laps) {
			smh->hold_laps++;
			if (switch_core_media_toggle_hold(session, sendonly)) {
				reneg = switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_HOLD);
				
				if ((val = switch_channel_get_variable(session->channel, "sip_renegotiate_codec_on_hold"))) {
					reneg = switch_true(val);
				}
			}
			
		}
	}

	if (reneg) {
		reneg = switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_REINVITE);
		
		if ((val = switch_channel_get_variable(session->channel, "sip_renegotiate_codec_on_reinvite"))) {
			reneg = switch_true(val);
		}
	}

	if (!reneg && smh->num_negotiated_codecs) {
		codec_array = smh->negotiated_codecs;
		total_codecs = smh->num_negotiated_codecs;
	} else if (reneg) {
		smh->num_codecs = 0;
		switch_core_media_prepare_codecs(session, SWITCH_FALSE);
		codec_array = smh->codecs;
		total_codecs = smh->num_codecs;
	}

	if (switch_stristr("T38FaxFillBitRemoval:", r_sdp) || switch_stristr("T38FaxTranscodingMMR:", r_sdp) || 
		switch_stristr("T38FaxTranscodingJBIG:", r_sdp)) {
		switch_channel_set_variable(session->channel, "t38_broken_boolean", "true");
	}

	switch_core_media_find_zrtp_hash(session, sdp);
	switch_core_media_pass_zrtp_hash(session);

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
				int pass = switch_media_handle_test_media_flag(smh, SCMF_T38_PASSTHRU);


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

				if ((pass == 2 && switch_media_handle_test_media_flag(smh, SCMF_T38_PASSTHRU)) 
					|| !reinvite ||
					
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

					if (switch_true(switch_channel_get_variable(session->channel, "t38_broken_boolean")) && 
						switch_true(switch_channel_get_variable(session->channel, "t38_pass_broken_boolean"))) {
						switch_channel_set_variable(other_channel, "t38_broken_boolean", "true");
					}
					
					a_engine->codec_params.remote_sdp_ip = switch_core_session_strdup(session, t38_options->remote_ip);
					a_engine->codec_params.remote_sdp_port = t38_options->remote_port;

					if (remote_host && remote_port && !strcmp(remote_host, a_engine->codec_params.remote_sdp_ip) && remote_port == a_engine->codec_params.remote_sdp_port) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params are unchanged for %s.\n",
										  switch_channel_get_name(session->channel));
					} else {
						const char *err = NULL;

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Audio params changed for %s from %s:%d to %s:%d\n",
										  switch_channel_get_name(session->channel),
										  remote_host, remote_port, a_engine->codec_params.remote_sdp_ip, a_engine->codec_params.remote_sdp_port);
						
						switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->codec_params.remote_sdp_port);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->codec_params.remote_sdp_ip);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);

						if (switch_rtp_set_remote_address(a_engine->rtp_session, a_engine->codec_params.remote_sdp_ip,
														  a_engine->codec_params.remote_sdp_port, 0, SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "AUDIO RTP REPORTS ERROR: [%s]\n", err);
							switch_channel_hangup(channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
						}
						
					}

					

					switch_core_media_copy_t38_options(t38_options, other_session);

					switch_media_handle_set_media_flag(smh, SCMF_T38_PASSTHRU);
					switch_media_handle_set_media_flag(other_session->media_handle, SCMF_T38_PASSTHRU);

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
					switch_channel_set_variable(session->channel, "sip_remote_audio_rtcp_port", attr->a_value);
				} else if (!strcasecmp(attr->a_name, "ptime") && attr->a_value) {
					ptime = atoi(attr->a_value);
				} else if (!strcasecmp(attr->a_name, "maxptime") && attr->a_value) {
					maxptime = atoi(attr->a_value);
				} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
					int crypto_tag;

					if (!(smh->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
						!switch_true(switch_channel_get_variable(session->channel, "sip_allow_crypto_in_avp"))) {
						if (m->m_proto != sdp_proto_srtp) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
							match = 0;
							goto done;
						}
					}

					crypto = attr->a_value;
					crypto_tag = atoi(crypto);

					got_crypto = switch_core_session_check_incoming_crypto(session, 
																		   "rtp_has_crypto", SWITCH_MEDIA_TYPE_AUDIO, crypto, crypto_tag);

				}
			}

			if (got_crypto && !got_avp) {
				switch_channel_set_variable(session->channel, "rtp_crypto_mandatory", "true");
				switch_channel_set_variable(session->channel, "rtp_secure_media", "true");
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

			if (a_engine->codec_params.rm_encoding && !(switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || 
														switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))) {	// && !reinvite) {
				char *remote_host = a_engine->codec_params.remote_sdp_ip;
				switch_port_t remote_port = a_engine->codec_params.remote_sdp_port;
				int same = 0;

				if (switch_rtp_ready(a_engine->rtp_session)) {
					remote_host = switch_rtp_get_remote_host(a_engine->rtp_session);
					remote_port = switch_rtp_get_remote_port(a_engine->rtp_session);
				}

				for (map = m->m_rtpmaps; map; map = map->rm_next) {
					if ((zstr(map->rm_encoding) || (smh->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
						match = (map->rm_pt == a_engine->codec_params.pt) ? 1 : 0;
					} else {
						match = strcasecmp(switch_str_nil(map->rm_encoding), a_engine->codec_params.iananame) ? 0 : 1;
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
									  a_engine->codec_params.rm_encoding, a_engine->codec_params.remote_sdp_ip, a_engine->codec_params.remote_sdp_port);
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
					if (!best_te || map->rm_rate == a_engine->codec_params.rm_rate) {
						best_te = (switch_payload_t) map->rm_pt;
					}
				}
				
				if (!switch_media_handle_test_media_flag(smh, SCMF_SUPPRESS_CNG) && !cng_pt && !strcasecmp(rm_encoding, "CN")) {
					cng_pt = (switch_payload_t) map->rm_pt;
					if (a_engine->rtp_session) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set comfort noise payload to %u\n", cng_pt);
						switch_rtp_set_cng_pt(a_engine->rtp_session, smh->cng_pt);
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
					last = smh->num_codecs;
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
					if ((zstr(map->rm_encoding) || (smh->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
					const char *mirror = switch_channel_get_variable(session->channel, "rtp_mirror_remote_audio_codec_payload");

					a_engine->codec_params.rm_encoding = switch_core_session_strdup(session, (char *) map->rm_encoding);
					a_engine->codec_params.iananame = switch_core_session_strdup(session, (char *) mimp->iananame);
					a_engine->codec_params.pt = (switch_payload_t) map->rm_pt;
					a_engine->codec_params.rm_rate = mimp->samples_per_second;
					a_engine->codec_params.codec_ms = mimp->microseconds_per_packet / 1000;
					a_engine->codec_params.bitrate = mimp->bits_per_second;
					a_engine->codec_params.remote_sdp_ip = switch_core_session_strdup(session, (char *) connection->c_address);
					a_engine->codec_params.rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
					a_engine->codec_params.remote_sdp_port = (switch_port_t) m->m_port;
					a_engine->codec_params.agreed_pt = (switch_payload_t) map->rm_pt;
					smh->num_negotiated_codecs = 0;
					smh->negotiated_codecs[smh->num_negotiated_codecs++] = mimp;
					switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->codec_params.remote_sdp_port);
					switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE, a_engine->codec_params.remote_sdp_ip);
					switch_channel_set_variable(session->channel, SWITCH_REMOTE_MEDIA_PORT_VARIABLE, tmp);
					a_engine->codec_params.recv_pt = (switch_payload_t)map->rm_pt;
					
					if (!switch_true(mirror) && 
						switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND && 
						(!reinvite || switch_media_handle_test_media_flag(smh, SCMF_RENEG_ON_REINVITE))) {
						switch_core_media_get_offered_pt(session, mimp, &a_engine->codec_params.recv_pt);
					}
					
					switch_snprintf(tmp, sizeof(tmp), "%d", a_engine->codec_params.recv_pt);
					switch_channel_set_variable(session->channel, "rtp_audio_recv_pt", tmp);
					
				}
				
				if (match) {
					if (switch_core_media_set_codec(session, 1, codec_flags) == SWITCH_STATUS_SUCCESS) {
						got_audio = 1;
					} else {
						match = 0;
					}
				}
			}

			if (!best_te && (switch_media_handle_test_media_flag(smh, SCMF_LIBERAL_DTMF) || switch_channel_test_flag(session->channel, CF_LIBERAL_DTMF))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
								  "No 2833 in SDP. Liberal DTMF mode adding %d as telephone-event.\n", default_te);
				best_te = default_te;
			}

			if (best_te) {
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
					te = smh->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send payload to %u\n", best_te);
					if (a_engine->rtp_session) {
						switch_rtp_set_telephony_event(a_engine->rtp_session, (switch_payload_t) best_te);
						switch_channel_set_variable_printf(session->channel, "sip_2833_send_payload", "%d", best_te);
					}
				} else {
					te = smh->recv_te = smh->te = (switch_payload_t) best_te;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set 2833 dtmf send/recv payload to %u\n", te);
					if (a_engine->rtp_session) {
						switch_rtp_set_telephony_event(a_engine->rtp_session, te);
						switch_channel_set_variable_printf(session->channel, "sip_2833_send_payload", "%d", te);
						switch_rtp_set_telephony_recv_event(a_engine->rtp_session, te);
						switch_channel_set_variable_printf(session->channel, "sip_2833_recv_payload", "%d", te);
					}
				}
			} else {
				/* by default, use SIP INFO if 2833 is not in the SDP */
				if (!switch_false(switch_channel_get_variable(channel, "sip_info_when_no_2833"))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No 2833 in SDP.  Disable 2833 dtmf and switch to INFO\n");
					switch_channel_set_variable(session->channel, "dtmf_type", "info");
					smh->dtmf_type = DTMF_INFO;
					te = smh->recv_te = smh->te = 0;
				} else {
					switch_channel_set_variable(session->channel, "dtmf_type", "none");
					smh->dtmf_type = DTMF_NONE;
					te = smh->recv_te = smh->te = 0;
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

				for (attr = m->m_attributes; attr; attr = attr->a_next) {
					if (!strcasecmp(attr->a_name, "framerate") && attr->a_value) {
						//framerate = atoi(attr->a_value);
					}
					if (!strcasecmp(attr->a_name, "rtcp") && attr->a_value) {
						switch_channel_set_variable(session->channel, "sip_remote_video_rtcp_port", attr->a_value);

					} else if (!got_crypto && !strcasecmp(attr->a_name, "crypto") && !zstr(attr->a_value)) {
						int crypto_tag;
						
						if (!(smh->ndlb & SM_NDLB_ALLOW_CRYPTO_IN_AVP) && 
							!switch_true(switch_channel_get_variable(session->channel, "sip_allow_crypto_in_avp"))) {
							if (m->m_proto != sdp_proto_srtp) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "a=crypto in RTP/AVP, refer to rfc3711\n");
								match = 0;
								goto done;
							}
						}
						
						crypto = attr->a_value;
						crypto_tag = atoi(crypto);
						
						got_video_crypto = switch_core_session_check_incoming_crypto(session, 
																					 "rtp_has_video_crypto", 
																					 SWITCH_MEDIA_TYPE_VIDEO, crypto, crypto_tag);
					
					}
				}

				if (got_video_crypto && !got_video_avp) {
					switch_channel_set_variable(session->channel, "rtp_crypto_mandatory", "true");
					switch_channel_set_variable(session->channel, "rtp_secure_media", "true");
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
					if ((zstr(map->rm_encoding) || (smh->ndlb & SM_NDLB_ALLOW_BAD_IANANAME)) && map->rm_pt < 96) {
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
					if ((v_engine->codec_params.rm_encoding = switch_core_session_strdup(session, (char *) rm_encoding))) {
						char tmp[50];
						const char *mirror = switch_channel_get_variable(session->channel, "sip_mirror_remote_video_codec_payload");

						v_engine->codec_params.pt = (switch_payload_t) map->rm_pt;
						v_engine->codec_params.rm_rate = map->rm_rate;
						v_engine->codec_params.codec_ms = mimp->microseconds_per_packet / 1000;
						v_engine->codec_params.remote_sdp_ip = switch_core_session_strdup(session, (char *) connection->c_address);
						v_engine->codec_params.rm_fmtp = switch_core_session_strdup(session, (char *) map->rm_fmtp);
						v_engine->codec_params.remote_sdp_port = (switch_port_t) m->m_port;
						v_engine->codec_params.agreed_pt = (switch_payload_t) map->rm_pt;
						switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->codec_params.remote_sdp_port);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_VIDEO_IP_VARIABLE, v_engine->codec_params.remote_sdp_ip);
						switch_channel_set_variable(session->channel, SWITCH_REMOTE_VIDEO_PORT_VARIABLE, tmp);
						switch_channel_set_variable(session->channel, "sip_video_fmtp", v_engine->codec_params.rm_fmtp);
						switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->codec_params.agreed_pt);
						switch_channel_set_variable(session->channel, "sip_video_pt", tmp);
						switch_core_media_check_video_codecs(session);

						v_engine->codec_params.recv_pt = (switch_payload_t)map->rm_pt;
						
						if (!switch_true(mirror) && switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
							switch_core_media_get_offered_pt(session, mimp, &v_engine->codec_params.recv_pt);
						}

						switch_snprintf(tmp, sizeof(tmp), "%d", v_engine->codec_params.recv_pt);
						switch_channel_set_variable(session->channel, "sip_video_recv_pt", tmp);
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

	smh->cng_pt = cng_pt;

	return match;
}

//?

SWITCH_DECLARE(int) switch_core_media_toggle_hold(switch_core_session_t *session, int sendonly)
{
	int changed = 0;
	switch_rtp_engine_t *a_engine;//, *v_engine;
	switch_media_handle_t *smh;

	if (!(smh = session->media_handle)) {
		return 0;
	}

	a_engine = &smh->engines[SWITCH_MEDIA_TYPE_AUDIO];
	//v_engine = &smh->engines[SWITCH_MEDIA_TYPE_VIDEO];

	if (switch_channel_test_flag(session->channel, CF_SLA_BARGE) || switch_channel_test_flag(session->channel, CF_SLA_BARGING)) {
		switch_channel_mark_hold(session->channel, sendonly);
		return 0;
	}

	if (sendonly && switch_channel_test_flag(session->channel, CF_ANSWERED)) {
		if (!switch_channel_test_flag(session->channel, CF_PROTO_HOLD)) {
			const char *stream;
			const char *msg = "hold";
			const char *info = switch_channel_get_variable(session->channel, "presence_call_info");

			if (info) {
				if (switch_stristr("private", info)) {
					msg = "hold-private";
				}
			}
			

			switch_channel_set_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_TRUE);
			switch_channel_presence(session->channel, "unknown", msg, NULL);
			changed = 1;

			if (a_engine->max_missed_hold_packets) {
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_hold_packets);
			}

			if (!(stream = switch_channel_get_hold_music(session->channel))) {
				stream = "local_stream://moh";
			}

			if (stream && strcasecmp(stream, "silence")) {
				if (!strcasecmp(stream, "indicate_hold")) {
					switch_channel_set_flag(session->channel, CF_SUSPEND);
					switch_channel_set_flag(session->channel, CF_HOLD);
					switch_ivr_hold_uuid(switch_channel_get_partner_uuid(session->channel), NULL, 0);
				} else {
					switch_ivr_broadcast(switch_channel_get_partner_uuid(session->channel), stream,
										 SMF_ECHO_ALEG | SMF_LOOP | SMF_PRIORITY);
					switch_yield(250000);
				}
			}
		}
	} else {
		if (switch_channel_test_flag(session->channel, CF_HOLD_LOCK)) {
			switch_channel_set_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_TRUE);
			changed = 1;
		}

		switch_channel_clear_flag(session->channel, CF_HOLD_LOCK);

		if (switch_channel_test_flag(session->channel, CF_PROTO_HOLD)) {
			const char *uuid;
			switch_core_session_t *b_session;

			switch_yield(250000);

			if (a_engine->max_missed_packets) {
				switch_rtp_reset_media_timer(a_engine->rtp_session);
				switch_rtp_set_max_missed_packets(a_engine->rtp_session, a_engine->max_missed_packets);
			}

			if ((uuid = switch_channel_get_partner_uuid(session->channel)) && (b_session = switch_core_session_locate(uuid))) {
				switch_channel_t *b_channel = switch_core_session_get_channel(b_session);

				if (switch_channel_test_flag(session->channel, CF_HOLD)) {
					switch_ivr_unhold(b_session);
					switch_channel_clear_flag(session->channel, CF_SUSPEND);
					switch_channel_clear_flag(session->channel, CF_HOLD);
				} else {
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
				}
				switch_core_session_rwunlock(b_session);
			}

			switch_channel_clear_flag(session->channel, CF_PROTO_HOLD);
			switch_channel_mark_hold(session->channel, SWITCH_FALSE);
			switch_channel_presence(session->channel, "unknown", "unhold", NULL);
			changed = 1;
		}
	}

	return changed;
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
