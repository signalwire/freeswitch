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

typedef union {
	int32_t intval;
	uint32_t uintval;
	char *charval;
} scm_multi_t;


static scm_type_t typemap[SCM_MAX] = {
	/*SCM_INBOUND_CODEC_STRING*/  STYPE_CHARVAL, 
	/*SCM_OUTBOUND_CODEC_STRING*/ STYPE_CHARVAL,
	/*SCM_TEST*/ STYPE_INTVAL
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

typedef struct switch_rtp_engine_s {
	switch_secure_settings_t ssec;
	switch_rtp_t *rtp_session;//tp
	switch_frame_t read_frame;//tp
	switch_media_type_t type;
} switch_rtp_engine_t;


struct switch_media_handle_s {
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_core_media_NDLB_t ndlb;
	switch_core_media_flag_t media_flags;
	smh_flag_t flags;
	switch_rtp_engine_t engines[SWITCH_MEDIA_TYPE_TOTAL];
	scm_multi_t params[SCM_MAX];
	char *codec_order[SWITCH_MAX_CODECS];//tp
    int codec_order_last;//tp
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];//tp
    int num_codecs;//tp
	int payload_space;//tp
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


switch_status_t switch_core_media_build_crypto(switch_media_handle_t *smh,
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

	smh->media_flags |= flag;
	
}

SWITCH_DECLARE(void) switch_media_handle_clear_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag)
{
	switch_assert(smh);

	smh->media_flags &= ~flag;
}

SWITCH_DECLARE(int32_t) switch_media_handle_test_media_flag(switch_media_handle_t *smh, switch_core_media_flag_t flag)
{
	switch_assert(smh);
	return (smh->media_flags & flag);
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




#if 0
SWITCH_DECLARE(switch_status_t) switch_core_session_read_media_frame(switch_core_session_t *session, switch_frame_t **frame,
																	 switch_io_flag_t flags, int stream_id, switch_media_type_t type)
{
	switch_channel_t *channel;
	uint32_t sanity = 1000;
	switch_rtcp_frame_t rtcp_frame;
	switch_rtp_engine_t *engine;

	if (!session->media_handle) return;

	engine = &session->media_handle->engines[type];
	channel = switch_core_session_get_channel(session);


	switch_assert(tech_pvt != NULL);


	tech_pvt->read_frame.datalen = 0;
	sofia_set_flag_locked(tech_pvt, TFLAG_READING);

	if (sofia_test_flag(tech_pvt, TFLAG_HUP) || sofia_test_flag(tech_pvt, TFLAG_BYE) || !tech_pvt->read_codec.implementation ||
		!switch_core_codec_ready(&tech_pvt->read_codec)) {
		return SWITCH_STATUS_FALSE;
	}

	if (sofia_test_flag(tech_pvt, TFLAG_IO)) {
		switch_status_t status;

		if (!sofia_test_flag(tech_pvt, TFLAG_RTP)) {
			return SWITCH_STATUS_GENERR;
		}

		switch_assert(tech_pvt->rtp_session != NULL);
		tech_pvt->read_frame.datalen = 0;


		if (sofia_test_flag(tech_pvt, TFLAG_SIMPLIFY) && sofia_test_flag(tech_pvt, TFLAG_GOT_ACK)) {
			if (sofia_glue_tech_simplify(tech_pvt)) {
				sofia_clear_flag(tech_pvt, TFLAG_SIMPLIFY);
			}
		}

		while (sofia_test_flag(tech_pvt, TFLAG_IO) && tech_pvt->read_frame.datalen == 0) {
			tech_pvt->read_frame.flags = SFF_NONE;

			status = switch_rtp_zerocopy_read_frame(tech_pvt->rtp_session, &tech_pvt->read_frame, flags);
			
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				if (status == SWITCH_STATUS_TIMEOUT) {

					if (sofia_test_flag(tech_pvt, TFLAG_SIP_HOLD)) {
						sofia_glue_toggle_hold(tech_pvt, 0);
						sofia_clear_flag_locked(tech_pvt, TFLAG_SIP_HOLD);
						switch_channel_clear_flag(channel, CF_LEG_HOLDING);
					}
					
					if (switch_channel_get_variable(tech_pvt->channel, "execute_on_media_timeout")) {
						*frame = &tech_pvt->read_frame;
						switch_set_flag((*frame), SFF_CNG);
						(*frame)->datalen = tech_pvt->read_impl.encoded_bytes_per_packet;
						memset((*frame)->data, 0, (*frame)->datalen);
						switch_channel_execute_on(tech_pvt->channel, "execute_on_media_timeout");
						return SWITCH_STATUS_SUCCESS;
					}


					switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
				}
				return status;
			}

			/* Try to read an RTCP frame, if successful raise an event */
			if (switch_rtcp_zerocopy_read_frame(tech_pvt->rtp_session, &rtcp_frame) == SWITCH_STATUS_SUCCESS) {
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

					snprintf(value, sizeof(value), "%" SWITCH_SIZE_T_FMT, tech_pvt->read_frame.timestamp);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Last-RTP-Timestamp", value);

					snprintf(value, sizeof(value), "%u", tech_pvt->read_frame.rate);
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
			if (switch_test_flag((&tech_pvt->read_frame), SFF_PROXY_PACKET)) {
				sofia_clear_flag_locked(tech_pvt, TFLAG_READING);
				*frame = &tech_pvt->read_frame;
				return SWITCH_STATUS_SUCCESS;
			}

			if (switch_rtp_has_dtmf(tech_pvt->rtp_session)) {
				switch_dtmf_t dtmf = { 0 };
				switch_rtp_dequeue_dtmf(tech_pvt->rtp_session, &dtmf);
				switch_channel_queue_dtmf(channel, &dtmf);
			}

			if (tech_pvt->read_frame.datalen > 0) {
				uint32_t bytes = 0;
				int frames = 1;

				if (!switch_test_flag((&tech_pvt->read_frame), SFF_CNG)) {
					if (!tech_pvt->read_codec.implementation || !switch_core_codec_ready(&tech_pvt->read_codec)) {
						*frame = NULL;
						return SWITCH_STATUS_GENERR;
					}

					if ((tech_pvt->read_frame.datalen % 10) == 0 &&
						sofia_test_pflag(tech_pvt->profile, PFLAG_AUTOFIX_TIMING) && tech_pvt->check_frames < MAX_CODEC_CHECK_FRAMES) {
						tech_pvt->check_frames++;

						if (!tech_pvt->read_impl.encoded_bytes_per_packet) {
							tech_pvt->check_frames = MAX_CODEC_CHECK_FRAMES;
							goto skip;
						}

						if (tech_pvt->last_ts && tech_pvt->read_frame.datalen != tech_pvt->read_impl.encoded_bytes_per_packet) {
							uint32_t codec_ms = (int) (tech_pvt->read_frame.timestamp -
													   tech_pvt->last_ts) / (tech_pvt->read_impl.samples_per_second / 1000);

							if ((codec_ms % 10) != 0 || codec_ms > tech_pvt->read_impl.samples_per_packet * 10) {
								tech_pvt->last_ts = 0;
								goto skip;
							}


							if (tech_pvt->last_codec_ms && tech_pvt->last_codec_ms == codec_ms) {
								tech_pvt->mismatch_count++;
							}

							tech_pvt->last_codec_ms = codec_ms;

							if (tech_pvt->mismatch_count > MAX_MISMATCH_FRAMES) {
								if (switch_rtp_ready(tech_pvt->rtp_session) && codec_ms != tech_pvt->codec_ms) {
									const char *val;
									int rtp_timeout_sec = 0;
									int rtp_hold_timeout_sec = 0;

									if (codec_ms > 120) {	/* yeah right */
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "Your phone is trying to send timestamps that suggest an increment of %dms per packet\n"
														  "That seems hard to believe so I am going to go on ahead and um ignore that, mmkay?\n",
														  (int) codec_ms);
										tech_pvt->check_frames = MAX_CODEC_CHECK_FRAMES;
										goto skip;
									}

									tech_pvt->read_frame.datalen = 0;

									if (codec_ms != tech_pvt->codec_ms) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "Asynchronous PTIME not supported, changing our end from %d to %d\n",
														  (int) tech_pvt->codec_ms,
														  (int) codec_ms
														  );

										switch_channel_set_variable_printf(channel, "sip_h_X-Broken-PTIME", "Adv=%d;Sent=%d",
																		   (int) tech_pvt->codec_ms, (int) codec_ms);

										tech_pvt->codec_ms = codec_ms;
									}

									if (sofia_glue_tech_set_codec(tech_pvt, 2) != SWITCH_STATUS_SUCCESS) {
										*frame = NULL;
										return SWITCH_STATUS_GENERR;
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
										tech_pvt->max_missed_packets = (tech_pvt->read_impl.samples_per_second * rtp_timeout_sec) /
											tech_pvt->read_impl.samples_per_packet;

										switch_rtp_set_max_missed_packets(tech_pvt->rtp_session, tech_pvt->max_missed_packets);
										if (!rtp_hold_timeout_sec) {
											rtp_hold_timeout_sec = rtp_timeout_sec * 10;
										}
									}

									if (rtp_hold_timeout_sec) {
										tech_pvt->max_missed_hold_packets = (tech_pvt->read_impl.samples_per_second * rtp_hold_timeout_sec) /
											tech_pvt->read_impl.samples_per_packet;
									}


									tech_pvt->check_frames = 0;
									tech_pvt->last_ts = 0;

									/* inform them of the codec they are actually sending */
#if 0
									if (++tech_pvt->codec_reinvites > 2) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
														  "Ok, some devices *cough* X-lite *cough*\n"
														  "seem to continue to lie over and over again so I guess we'll\n"
														  "leave well-enough alone and let them lie\n");
									} else {
										sofia_glue_do_invite(session);
									}
#endif
									*frame = &tech_pvt->read_frame;
									switch_set_flag((*frame), SFF_CNG);
									(*frame)->datalen = tech_pvt->read_impl.encoded_bytes_per_packet;
									memset((*frame)->data, 0, (*frame)->datalen);
									return SWITCH_STATUS_SUCCESS;
								}

							}

						} else {
							tech_pvt->mismatch_count = 0;
						}

						tech_pvt->last_ts = tech_pvt->read_frame.timestamp;


					} else {
						tech_pvt->mismatch_count = 0;
						tech_pvt->last_ts = 0;
					}
				  skip:

					if ((bytes = tech_pvt->read_impl.encoded_bytes_per_packet)) {
						frames = (tech_pvt->read_frame.datalen / bytes);
					}
					tech_pvt->read_frame.samples = (int) (frames * tech_pvt->read_impl.samples_per_packet);

					if (tech_pvt->read_frame.datalen == 0) {
						continue;
					}
				}
				break;
			}
		}
	}

	sofia_clear_flag_locked(tech_pvt, TFLAG_READING);

	if (tech_pvt->read_frame.datalen == 0) {
		*frame = NULL;
		return SWITCH_STATUS_GENERR;
	}

	*frame = &tech_pvt->read_frame;

	return SWITCH_STATUS_SUCCESS;
}
#endif



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
		if (!codec_string || (smh->media_flags & SCMF_DISABLE_TRANSCODING)) {
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
