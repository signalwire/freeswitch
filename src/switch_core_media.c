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
	switch_rtp_t *rtp_session;
	switch_media_type_t type;
} switch_rtp_engine_t;


struct switch_media_handle_s {
	switch_core_session_t *session;
	switch_core_media_NDLB_t ndlb;
	smh_flag_t flags;
	switch_rtp_engine_t engines[SWITCH_MEDIA_TYPE_TOTAL];
};


SWITCH_DECLARE(const char *) switch_core_sesson_local_crypto_key(switch_core_session_t *session, switch_media_type_t type)
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
		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}

SWITCH_DECLARE(void) switch_media_handle_set_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);

	smh->flags |= flag;
	
}

SWITCH_DECLARE(void) switch_media_handle_clear_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);

	smh->flags &= ~flag;
}

SWITCH_DECLARE(int32_t) switch_media_handle_test_ndlb(switch_media_handle_t *smh, switch_core_media_NDLB_t flag)
{
	switch_assert(smh);
	return (smh->flags & flag);
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
