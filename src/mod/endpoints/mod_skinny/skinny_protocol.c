/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * skinny_protocol.c -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#include <switch.h>
#include "mod_skinny.h"
#include "skinny_protocol.h"
#include "skinny_tables.h"
#include "skinny_labels.h"

/*****************************************************************************/
/* SKINNY FUNCTIONS */
/*****************************************************************************/
char* skinny_codec2string(enum skinny_codecs skinnycodec)
{
	switch (skinnycodec) {
		case SKINNY_CODEC_ALAW_64K:
		case SKINNY_CODEC_ALAW_56K:
			return "PCMA";
		case SKINNY_CODEC_ULAW_64K:
		case SKINNY_CODEC_ULAW_56K:
			return "PCMU";
		case SKINNY_CODEC_G722_64K:
		case SKINNY_CODEC_G722_56K:
		case SKINNY_CODEC_G722_48K:
			return "G722";
		case SKINNY_CODEC_G723_1:
			return "G723";
		case SKINNY_CODEC_G728:
			return "G728";
		case SKINNY_CODEC_G729:
		case SKINNY_CODEC_G729A:
			return "G729";
		case SKINNY_CODEC_IS11172:
			return "IS11172";
		case SKINNY_CODEC_IS13818:
			return "IS13818";
		case SKINNY_CODEC_G729B:
		case SKINNY_CODEC_G729AB:
			return "G729";
		case SKINNY_CODEC_GSM_FULL:
		case SKINNY_CODEC_GSM_HALF:
		case SKINNY_CODEC_GSM_EFULL:
			return "GSM";
		case SKINNY_CODEC_WIDEBAND_256K:
			return "WIDEBAND";
		case SKINNY_CODEC_DATA_64K:
		case SKINNY_CODEC_DATA_56K:
			return "DATA";
		case SKINNY_CODEC_GSM:
			return "GSM";
		case SKINNY_CODEC_ACTIVEVOICE:
			return "ACTIVEVOICE";
		case SKINNY_CODEC_G726_32K:
		case SKINNY_CODEC_G726_24K:
		case SKINNY_CODEC_G726_16K:
			return "G726";
		case SKINNY_CODEC_G729B_BIS:
		case SKINNY_CODEC_G729B_LOW:
			return "G729";
		case SKINNY_CODEC_H261:
			return "H261";
		case SKINNY_CODEC_H263:
			return "H263";
		case SKINNY_CODEC_VIDEO:
			return "VIDEO";
		case SKINNY_CODEC_T120:
			return "T120";
		case SKINNY_CODEC_H224:
			return "H224";
		case SKINNY_CODEC_RFC2833_DYNPAYLOAD:
			return "RFC2833_DYNPAYLOAD";
		default:
			return "";
	}
}

/*****************************************************************************/
switch_status_t skinny_read_packet(listener_t *listener, skinny_message_t **req)
{
	skinny_message_t *request;
	switch_size_t mlen, bytes = 0;
	char mbuf[SKINNY_MESSAGE_MAXSIZE] = "";
	char *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	request = switch_core_alloc(listener->pool, SKINNY_MESSAGE_MAXSIZE);

	if (!request) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to allocate memory.\n");
		return SWITCH_STATUS_MEMERR;
	}

	ptr = mbuf;

	while (listener_is_ready(listener)) {
		uint8_t do_sleep = 1;
		if (listener->expire_time && listener->expire_time < switch_epoch_time_now(NULL)) {
			return SWITCH_STATUS_TIMEOUT;
		}
		if(bytes < SKINNY_MESSAGE_FIELD_SIZE) {
			/* We have nothing yet, get length header field */
			mlen = SKINNY_MESSAGE_FIELD_SIZE - bytes;
		} else {
			/* We now know the message size */
			mlen = request->length + 2*SKINNY_MESSAGE_FIELD_SIZE - bytes;
		}

		status = switch_socket_recv(listener->sock, ptr, &mlen);

		if (listener->expire_time && listener->expire_time < switch_epoch_time_now(NULL)) {
			return SWITCH_STATUS_TIMEOUT;
		}

		if (!listener_is_ready(listener)) {
			break;
		}
		if (!switch_status_is_timeup(status) && !SWITCH_STATUS_IS_BREAK(status) && (status != SWITCH_STATUS_SUCCESS)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket break with status=%d.\n", status);
			return SWITCH_STATUS_FALSE;
		}

		if(mlen) {
			bytes += mlen;

			if(bytes >= SKINNY_MESSAGE_FIELD_SIZE) {
				do_sleep = 0;
				ptr += mlen;
				memcpy(request, mbuf, bytes);
#ifdef SKINNY_MEGA_DEBUG
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						"Got request: length=%d,reserved=%x,type=%x\n",
						request->length,request->reserved,request->type);
#endif
				if(request->length < SKINNY_MESSAGE_FIELD_SIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Skinny client sent invalid data. Length should be greater than 4 but got %d.\n",
							request->length);
					return SWITCH_STATUS_FALSE;
				}
				if(request->length + 2*SKINNY_MESSAGE_FIELD_SIZE > SKINNY_MESSAGE_MAXSIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							"Skinny client sent too huge data. Got %d which is above threshold %d.\n",
							request->length, SKINNY_MESSAGE_MAXSIZE - 2*SKINNY_MESSAGE_FIELD_SIZE);
					return SWITCH_STATUS_FALSE;
				}
				if(bytes >= request->length + 2*SKINNY_MESSAGE_FIELD_SIZE) {
					/* Message body */
					*req = request;
					return  SWITCH_STATUS_SUCCESS;
				}
			}
		}
		if (do_sleep) {
			switch_cond_next();
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************/
int skinny_device_event_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *event = (switch_event_t *) pArg;

	char *profile_name = argv[0];
	char *device_name = argv[1];
	char *user_id = argv[2];
	char *device_instance = argv[3];
	char *ip = argv[4];
	char *device_type = argv[5];
	char *max_streams = argv[6];
	char *port = argv[7];
	char *codec_string = argv[8];

	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Profile-Name", "%s", profile_name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Device-Name", "%s", device_name);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Station-User-Id", "%s", user_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Station-Instance", "%s", device_instance);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-IP-Address", "%s", ip);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Device-Type", "%s", device_type);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Max-Streams", "%s", max_streams);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Port", "%s", port);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Codecs", "%s", codec_string);

	return 0;
}

switch_status_t skinny_device_event(listener_t *listener, switch_event_t **ev, switch_event_types_t event_id, const char *subclass_name)
{
	switch_event_t *event = NULL;
	char *sql;
	skinny_profile_t *profile;
	assert(listener->profile);
	profile = listener->profile;

	switch_event_create_subclass(&event, event_id, subclass_name);
	switch_assert(event);
	if ((sql = switch_mprintf("SELECT '%s', name, user_id, instance, ip, type, max_streams, port, codec_string "
					"FROM skinny_devices "
					"WHERE name='%s' AND instance=%d",
					listener->profile->name,
					listener->device_name, listener->device_instance))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_device_event_callback, event);
		switch_safe_free(sql);
	}

	*ev = event;
	return SWITCH_STATUS_SUCCESS;
}


/*****************************************************************************/
/*****************************************************************************/
switch_status_t skinny_session_walk_lines(skinny_profile_t *profile, char *channel_uuid, switch_core_db_callback_func_t callback, void *data)
{
	char *sql;
	if ((sql = switch_mprintf(
					"SELECT skinny_lines.*, channel_uuid, call_id, call_state "
					"FROM skinny_active_lines "
					"INNER JOIN skinny_lines "
					"ON skinny_active_lines.device_name = skinny_lines.device_name "
					"AND skinny_active_lines.device_instance = skinny_lines.device_instance "
					"AND skinny_active_lines.line_instance = skinny_lines.line_instance "
					"WHERE channel_uuid='%s'",
					channel_uuid))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, callback, data);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_walk_lines_by_call_id(skinny_profile_t *profile, uint32_t call_id, switch_core_db_callback_func_t callback, void *data)
{
	char *sql;
	if ((sql = switch_mprintf(
					"SELECT skinny_lines.*, channel_uuid, call_id, call_state "
					"FROM skinny_active_lines "
					"INNER JOIN skinny_lines "
					"ON skinny_active_lines.device_name = skinny_lines.device_name "
					"AND skinny_active_lines.device_instance = skinny_lines.device_instance "
					"AND skinny_active_lines.line_instance = skinny_lines.line_instance "
					"WHERE call_id='%d'",
					call_id))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, callback, data);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************/
/* SKINNY BUTTONS */
/*****************************************************************************/
struct line_get_helper {
	uint32_t pos;
	struct line_stat_res_message *button;
};

int skinny_line_get_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct line_get_helper *helper = pArg;

	helper->pos++;
	if (helper->pos == atoi(argv[0])) { /* wanted_position */
		helper->button->number = helper->pos;
		strncpy(helper->button->name,  argv[2], 24); /* label */
		strncpy(helper->button->shortname,  argv[3], 40); /* value */
		strncpy(helper->button->displayname,  argv[4], 44); /* caller_name */
	}
	return 0;
}

void skinny_line_get(listener_t *listener, uint32_t instance, struct line_stat_res_message **button)
{
	struct line_get_helper helper = {0};
	char *sql;

	switch_assert(listener);
	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	helper.button = switch_core_alloc(listener->pool, sizeof(struct line_stat_res_message));

	if ((sql = switch_mprintf(
					"SELECT '%d' AS wanted_position, position, label, value, caller_name "
					"FROM skinny_lines "
					"WHERE device_name='%s' AND device_instance=%d "
					"ORDER BY position",
					instance,
					listener->device_name, listener->device_instance
				 ))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_line_get_callback, &helper);
		switch_safe_free(sql);
	}
	*button = helper.button;
}

struct speed_dial_get_helper {
	uint32_t pos;
	struct speed_dial_stat_res_message *button;
};

int skinny_speed_dial_get_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct speed_dial_get_helper *helper = pArg;

	helper->pos++;
	if (helper->pos == atoi(argv[0])) { /* wanted_position */
		helper->button->number = helper->pos; /* value */
		strncpy(helper->button->line,  argv[3], 24); /* value */
		strncpy(helper->button->label,  argv[2], 40); /* label */
	}
	return 0;
}

void skinny_speed_dial_get(listener_t *listener, uint32_t instance, struct speed_dial_stat_res_message **button)
{
	struct speed_dial_get_helper helper = {0};
	char *sql;

	switch_assert(listener);
	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	helper.button = switch_core_alloc(listener->pool, sizeof(struct speed_dial_stat_res_message));

	if ((sql = switch_mprintf(
					"SELECT '%d' AS wanted_position, position, label, value, settings "
					"FROM skinny_buttons "
					"WHERE device_name='%s' AND device_instance=%d AND type=%d "
					"ORDER BY position",
					instance,
					listener->device_name, listener->device_instance,
					SKINNY_BUTTON_SPEED_DIAL
				 ))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_speed_dial_get_callback, &helper);
		switch_safe_free(sql);
	}
	*button = helper.button;
}

struct service_url_get_helper {
	uint32_t pos;
	struct service_url_stat_res_message *button;
};

int skinny_service_url_get_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct service_url_get_helper *helper = pArg;

	helper->pos++;
	if (helper->pos == atoi(argv[0])) { /* wanted_position */
		helper->button->index = helper->pos;
		strncpy(helper->button->url, argv[3], 256); /* value */
		strncpy(helper->button->display_name,  argv[2], 40); /* label */
	}
	return 0;
}

void skinny_service_url_get(listener_t *listener, uint32_t instance, struct service_url_stat_res_message **button)
{
	struct service_url_get_helper helper = {0};
	char *sql;

	switch_assert(listener);
	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	helper.button = switch_core_alloc(listener->pool, sizeof(struct service_url_stat_res_message));

	if ((sql = switch_mprintf(
					"SELECT '%d' AS wanted_position, position, label, value, settings "
					"FROM skinny_buttons "
					"WHERE device_name='%s' AND device_instance=%d AND type=%d "
					"ORDER BY position",
					instance,
					listener->device_name,
					listener->device_instance,
					SKINNY_BUTTON_SERVICE_URL
				 ))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_service_url_get_callback, &helper);
		switch_safe_free(sql);
	}
	*button = helper.button;
}

struct feature_get_helper {
	uint32_t pos;
	struct feature_stat_res_message *button;
};

int skinny_feature_get_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct feature_get_helper *helper = pArg;

	helper->pos++;
	if (helper->pos == atoi(argv[0])) { /* wanted_position */
		helper->button->index = helper->pos;
		helper->button->id = helper->pos;
		strncpy(helper->button->text_label,  argv[2], 40); /* label */
		helper->button->status = atoi(argv[3]); /* value */
	}
	return 0;
}

void skinny_feature_get(listener_t *listener, uint32_t instance, struct feature_stat_res_message **button)
{
	struct feature_get_helper helper = {0};
	char *sql;

	switch_assert(listener);
	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	helper.button = switch_core_alloc(listener->pool, sizeof(struct feature_stat_res_message));

	if ((sql = switch_mprintf(
					"SELECT '%d' AS wanted_position, position, label, value, settings "
					"FROM skinny_buttons "
					"WHERE device_name='%s' AND device_instance=%d AND NOT (type=%d OR type=%d) "
					"ORDER BY position",
					instance,
					listener->device_name,
					listener->device_instance,
					SKINNY_BUTTON_SPEED_DIAL, SKINNY_BUTTON_SERVICE_URL
				 ))) {
		skinny_execute_sql_callback(listener->profile, listener->profile->sql_mutex, sql, skinny_feature_get_callback, &helper);
		switch_safe_free(sql);
	}
	*button = helper.button;
}

/*****************************************************************************/
/* SKINNY MESSAGE SENDER */
/*****************************************************************************/
switch_status_t send_register_ack(listener_t *listener,
		uint32_t keep_alive,
		char *date_format,
		char *reserved,
		uint32_t secondary_keep_alive,
		char *reserved2)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_ack));
	message->type = REGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.reg_ack);
	message->data.reg_ack.keep_alive = keep_alive;
	strncpy(message->data.reg_ack.date_format, date_format, 6);
	strncpy(message->data.reg_ack.reserved, reserved, 2);
	message->data.reg_ack.secondary_keep_alive = keep_alive;
	strncpy(message->data.reg_ack.reserved2, reserved2, 4);
	return skinny_send_reply(listener, message);
}

switch_status_t send_start_tone(listener_t *listener,
		uint32_t tone,
		uint32_t reserved,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.start_tone));
	message->type = START_TONE_MESSAGE;
	message->length = 4 + sizeof(message->data.start_tone);
	message->data.start_tone.tone = tone;
	message->data.start_tone.reserved = reserved;
	message->data.start_tone.line_instance = line_instance;
	message->data.start_tone.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_stop_tone(listener_t *listener,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.stop_tone));
	message->type = STOP_TONE_MESSAGE;
	message->length = 4 + sizeof(message->data.stop_tone);
	message->data.stop_tone.line_instance = line_instance;
	message->data.stop_tone.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_set_ringer(listener_t *listener,
		uint32_t ring_type,
		uint32_t ring_mode,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.ringer));
	message->type = SET_RINGER_MESSAGE;
	message->length = 4 + sizeof(message->data.ringer);
	message->data.ringer.ring_type = ring_type;
	message->data.ringer.ring_mode = ring_mode;
	message->data.ringer.line_instance = line_instance;
	message->data.ringer.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_set_lamp(listener_t *listener,
		uint32_t stimulus,
		uint32_t stimulus_instance,
		uint32_t mode)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.lamp));
	message->type = SET_LAMP_MESSAGE;
	message->length = 4 + sizeof(message->data.lamp);
	message->data.lamp.stimulus = stimulus;
	message->data.lamp.stimulus_instance = stimulus_instance;
	message->data.lamp.mode = mode;
	return skinny_send_reply(listener, message);
}

switch_status_t send_set_speaker_mode(listener_t *listener,
		uint32_t mode)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speaker_mode));
	message->type = SET_SPEAKER_MODE_MESSAGE;
	message->length = 4 + sizeof(message->data.speaker_mode);
	message->data.speaker_mode.mode = mode;
	return skinny_send_reply(listener, message);
}

switch_status_t send_start_media_transmission(listener_t *listener,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t remote_ip,
		uint32_t remote_port,
		uint32_t ms_per_packet,
		uint32_t payload_capacity,
		uint32_t precedence,
		uint32_t silence_suppression,
		uint16_t max_frames_per_packet,
		uint32_t g723_bitrate)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.start_media));
	message->type = START_MEDIA_TRANSMISSION_MESSAGE;
	message->length = 4 + sizeof(message->data.start_media);
	message->data.start_media.conference_id = conference_id;
	message->data.start_media.pass_thru_party_id = pass_thru_party_id;
	message->data.start_media.remote_ip = remote_ip;
	message->data.start_media.remote_port = remote_port;
	message->data.start_media.ms_per_packet = ms_per_packet;
	message->data.start_media.payload_capacity = payload_capacity;
	message->data.start_media.precedence = precedence;
	message->data.start_media.silence_suppression = silence_suppression;
	message->data.start_media.max_frames_per_packet = max_frames_per_packet;
	message->data.start_media.g723_bitrate = g723_bitrate;
	/* ... */
	return skinny_send_reply(listener, message);
}

switch_status_t send_stop_media_transmission(listener_t *listener,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t conference_id2)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.stop_media));
	message->type = STOP_MEDIA_TRANSMISSION_MESSAGE;
	message->length = 4 + sizeof(message->data.stop_media);
	message->data.stop_media.conference_id = conference_id;
	message->data.stop_media.pass_thru_party_id = pass_thru_party_id;
	message->data.stop_media.conference_id2 = conference_id2;
	/* ... */
	return skinny_send_reply(listener, message);
}

switch_status_t skinny_send_call_info(listener_t *listener,
		const char *calling_party_name,
		const char *calling_party,
		const char *called_party_name,
		const char *called_party,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t call_type,
		const char *original_called_party_name,
		const char *original_called_party,
		const char *last_redirecting_party_name,
		const char *last_redirecting_party,
		uint32_t original_called_party_redirect_reason,
		uint32_t last_redirecting_reason,
		const char *calling_party_voice_mailbox,
		const char *called_party_voice_mailbox,
		const char *original_called_party_voice_mailbox,
		const char *last_redirecting_voice_mailbox,
		uint32_t call_instance,
		uint32_t call_security_status,
		uint32_t party_pi_restriction_bits)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.call_info));
	message->type = CALL_INFO_MESSAGE;
	message->length = 4 + sizeof(message->data.call_info);
	strncpy(message->data.call_info.calling_party_name, calling_party_name, 40);
	strncpy(message->data.call_info.calling_party, calling_party, 24);
	strncpy(message->data.call_info.called_party_name, called_party_name, 40);
	strncpy(message->data.call_info.called_party, called_party, 24);
	message->data.call_info.line_instance = line_instance;
	message->data.call_info.call_id = call_id;
	message->data.call_info.call_type = call_type;
	strncpy(message->data.call_info.original_called_party_name, original_called_party_name, 40);
	strncpy(message->data.call_info.original_called_party, original_called_party, 24);
	strncpy(message->data.call_info.last_redirecting_party_name, last_redirecting_party_name, 40);
	strncpy(message->data.call_info.last_redirecting_party, last_redirecting_party, 24);
	message->data.call_info.original_called_party_redirect_reason = original_called_party_redirect_reason;
	message->data.call_info.last_redirecting_reason = last_redirecting_reason;
	strncpy(message->data.call_info.calling_party_voice_mailbox, calling_party_voice_mailbox, 24);
	strncpy(message->data.call_info.called_party_voice_mailbox, called_party_voice_mailbox, 24);
	strncpy(message->data.call_info.original_called_party_voice_mailbox, original_called_party_voice_mailbox, 24);
	strncpy(message->data.call_info.last_redirecting_voice_mailbox, last_redirecting_voice_mailbox, 24);
	message->data.call_info.call_instance = call_instance;
	message->data.call_info.call_security_status = call_security_status;
	message->data.call_info.party_pi_restriction_bits = party_pi_restriction_bits;
	return skinny_send_reply(listener, message);
}

switch_status_t send_define_time_date(listener_t *listener,
		uint32_t year,
		uint32_t month,
		uint32_t day_of_week, /* monday = 1 */
		uint32_t day,
		uint32_t hour,
		uint32_t minute,
		uint32_t seconds,
		uint32_t milliseconds,
		uint32_t timestamp)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.define_time_date));
	message->type = DEFINE_TIME_DATE_MESSAGE;
	message->length = 4+sizeof(message->data.define_time_date);
	message->data.define_time_date.year = year;
	message->data.define_time_date.month = month;
	message->data.define_time_date.day_of_week = day_of_week;
	message->data.define_time_date.day = day;
	message->data.define_time_date.hour = hour;
	message->data.define_time_date.minute = minute;
	message->data.define_time_date.seconds = seconds;
	message->data.define_time_date.milliseconds = milliseconds;
	message->data.define_time_date.timestamp = timestamp;
	return skinny_send_reply(listener, message);
}

switch_status_t send_define_current_time_date(listener_t *listener)
{
	switch_time_t ts;
	switch_time_exp_t tm;
	ts = switch_micro_time_now();
	switch_time_exp_lt(&tm, ts);
	return send_define_time_date(listener,
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_wday,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec,
			tm.tm_usec / 1000,
			ts / 1000000);
}

switch_status_t send_capabilities_req(listener_t *listener)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12);
	message->type = CAPABILITIES_REQ_MESSAGE;
	message->length = 4;
	return skinny_send_reply(listener, message);
}

switch_status_t send_version(listener_t *listener,
		char *version)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.version));
	message->type = VERSION_MESSAGE;
	message->length = 4+ sizeof(message->data.version);
	strncpy(message->data.version.version, version, 16);
	return skinny_send_reply(listener, message);
}

switch_status_t send_register_reject(listener_t *listener,
		char *error)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
	message->type = REGISTER_REJECT_MESSAGE;
	message->length = 4 + sizeof(message->data.reg_rej);
	strncpy(message->data.reg_rej.error, error, 33);
	return skinny_send_reply(listener, message);
}

switch_status_t send_open_receive_channel(listener_t *listener,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t packets,
		uint32_t payload_capacity,
		uint32_t echo_cancel_type,
		uint32_t g723_bitrate,
		uint32_t conference_id2,
		uint32_t reserved[10])
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.open_receive_channel));
	message->type = OPEN_RECEIVE_CHANNEL_MESSAGE;
	message->length = 4 + sizeof(message->data.open_receive_channel);
	message->data.open_receive_channel.conference_id = conference_id;
	message->data.open_receive_channel.pass_thru_party_id = pass_thru_party_id;
	message->data.open_receive_channel.packets = packets;
	message->data.open_receive_channel.payload_capacity = payload_capacity;
	message->data.open_receive_channel.echo_cancel_type = echo_cancel_type;
	message->data.open_receive_channel.g723_bitrate = g723_bitrate;
	message->data.open_receive_channel.conference_id2 = conference_id2;
	/*
	   message->data.open_receive_channel.reserved[0] = reserved[0];
	   message->data.open_receive_channel.reserved[1] = reserved[1];
	   message->data.open_receive_channel.reserved[2] = reserved[2];
	   message->data.open_receive_channel.reserved[3] = reserved[3];
	   message->data.open_receive_channel.reserved[4] = reserved[4];
	   message->data.open_receive_channel.reserved[5] = reserved[5];
	   message->data.open_receive_channel.reserved[6] = reserved[6];
	   message->data.open_receive_channel.reserved[7] = reserved[7];
	   message->data.open_receive_channel.reserved[8] = reserved[8];
	   message->data.open_receive_channel.reserved[9] = reserved[9];
	 */
	return skinny_send_reply(listener, message);
}

switch_status_t send_close_receive_channel(listener_t *listener,
		uint32_t conference_id,
		uint32_t pass_thru_party_id,
		uint32_t conference_id2)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.close_receive_channel));
	message->type = CLOSE_RECEIVE_CHANNEL_MESSAGE;
	message->length = 4 + sizeof(message->data.close_receive_channel);
	message->data.close_receive_channel.conference_id = conference_id;
	message->data.close_receive_channel.pass_thru_party_id = pass_thru_party_id;
	message->data.close_receive_channel.conference_id2 = conference_id2;
	return skinny_send_reply(listener, message);
}

switch_status_t send_select_soft_keys(listener_t *listener,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t soft_key_set,
		uint32_t valid_key_mask)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.select_soft_keys));
	message->type = SELECT_SOFT_KEYS_MESSAGE;
	message->length = 4 + sizeof(message->data.select_soft_keys);
	message->data.select_soft_keys.line_instance = line_instance;
	message->data.select_soft_keys.call_id = call_id;
	message->data.select_soft_keys.soft_key_set = soft_key_set;
	message->data.select_soft_keys.valid_key_mask = valid_key_mask;
	return skinny_send_reply(listener, message);
}

switch_status_t send_call_state(listener_t *listener,
		uint32_t call_state,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.call_state));
	message->type = CALL_STATE_MESSAGE;
	message->length = 4 + sizeof(message->data.call_state);
	message->data.call_state.call_state = call_state;
	message->data.call_state.line_instance = line_instance;
	message->data.call_state.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_display_prompt_status(listener_t *listener,
		uint32_t timeout,
		const char *display,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.display_prompt_status));
	message->type = DISPLAY_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.display_prompt_status);
	message->data.display_prompt_status.timeout = timeout;
	strncpy(message->data.display_prompt_status.display, display, 32);
	message->data.display_prompt_status.line_instance = line_instance;
	message->data.display_prompt_status.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_clear_prompt_status(listener_t *listener,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.clear_prompt_status));
	message->type = CLEAR_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.clear_prompt_status);
	message->data.clear_prompt_status.line_instance = line_instance;
	message->data.clear_prompt_status.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_activate_call_plane(listener_t *listener,
		uint32_t line_instance)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.activate_call_plane));
	message->type = ACTIVATE_CALL_PLANE_MESSAGE;
	message->length = 4 + sizeof(message->data.activate_call_plane);
	message->data.activate_call_plane.line_instance = line_instance;
	return skinny_send_reply(listener, message);
}

switch_status_t send_back_space_request(listener_t *listener,
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.back_space_req));
	message->type = BACK_SPACE_REQ_MESSAGE;
	message->length = 4 + sizeof(message->data.back_space_req);
	message->data.back_space_req.line_instance = line_instance;
	message->data.back_space_req.call_id = call_id;
	return skinny_send_reply(listener, message);

}

switch_status_t send_dialed_number(listener_t *listener,
		char called_party[24],
		uint32_t line_instance,
		uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.dialed_number));
	message->type = DIALED_NUMBER_MESSAGE;
	message->length = 4 + sizeof(message->data.dialed_number);
	strncpy(message->data.dialed_number.called_party, called_party, 24);
	message->data.dialed_number.line_instance = line_instance;
	message->data.dialed_number.call_id = call_id;
	return skinny_send_reply(listener, message);
}

switch_status_t send_display_pri_notify(listener_t *listener,
		uint32_t message_timeout,
		uint32_t priority,
		char *notify)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.display_pri_notify));
	message->type = DISPLAY_PRI_NOTIFY_MESSAGE;
	message->length = 4 + sizeof(message->data.display_pri_notify);
	message->data.display_pri_notify.message_timeout = message_timeout;
	message->data.display_pri_notify.priority = priority;
	strncpy(message->data.display_pri_notify.notify, notify, 32);
	return skinny_send_reply(listener, message);
}


switch_status_t send_reset(listener_t *listener, uint32_t reset_type)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reset));
	message->type = RESET_MESSAGE;
	message->length = 4 + sizeof(message->data.reset);
	message->data.reset.reset_type = reset_type;
	return skinny_send_reply(listener, message);
}

switch_status_t send_data(listener_t *listener, uint32_t message_type,
		uint32_t application_id,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t transaction_id,
		uint32_t data_length,
		const char *data)
{
	skinny_message_t *message;
	switch_assert(data_length == strlen(data));
	/* data_length should be a multiple of 4 */
	if ((data_length % 4) != 0) {
		data_length = (data_length / 4 + 1) * 4;
	}
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.data)+data_length-1);
	message->type = message_type;
	message->length = 4 + sizeof(message->data.data)+data_length-1;
	message->data.data.application_id = application_id;
	message->data.data.line_instance = line_instance;
	message->data.data.call_id = call_id;
	message->data.data.transaction_id = transaction_id;
	message->data.data.data_length = data_length;
	strncpy(message->data.data.data, data, data_length);
	return skinny_send_reply(listener, message);
}

switch_status_t send_extended_data(listener_t *listener, uint32_t message_type,
		uint32_t application_id,
		uint32_t line_instance,
		uint32_t call_id,
		uint32_t transaction_id,
		uint32_t data_length,
		uint32_t sequence_flag,
		uint32_t display_priority,
		uint32_t conference_id,
		uint32_t app_instance_id,
		uint32_t routing_id,
		const char *data)
{
	skinny_message_t *message;
	switch_assert(data_length == strlen(data));
	/* data_length should be a multiple of 4 */
	if ((data_length % 4) != 0) {
		data_length = (data_length / 4 + 1) * 4;
	}
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.extended_data)+data_length-1);
	message->type = message_type;
	message->length = 4 + sizeof(message->data.extended_data)+data_length-1;
	message->data.extended_data.application_id = application_id;
	message->data.extended_data.line_instance = line_instance;
	message->data.extended_data.call_id = call_id;
	message->data.extended_data.transaction_id = transaction_id;
	message->data.extended_data.data_length = data_length;
	message->data.extended_data.sequence_flag = sequence_flag;
	message->data.extended_data.display_priority = display_priority;
	message->data.extended_data.conference_id = conference_id;
	message->data.extended_data.app_instance_id = app_instance_id;
	message->data.extended_data.routing_id = routing_id;
	strncpy(message->data.extended_data.data, data, data_length);
	return skinny_send_reply(listener, message);
}

switch_status_t skinny_perform_send_reply(listener_t *listener, const char *file, const char *func, int line, skinny_message_t *reply)
{
	char *ptr;
	switch_size_t len;
	switch_assert(reply != NULL);
	len = reply->length+8;
	ptr = (char *) reply;

	if (listener_is_ready(listener)) {
		if (listener->profile->debug >= 10 || reply->type != KEEP_ALIVE_ACK_MESSAGE) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG,
					"Sending %s (type=%x,length=%d) to %s:%d at %s:%d.\n",
					skinny_message_type2str(reply->type), reply->type, reply->length,
					listener->device_name, listener->device_instance, listener->remote_ip, listener->remote_port);
		}
		return switch_socket_send(listener->sock, ptr, &len);
	} else {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_WARNING,
				"Not sending %s (type=%x,length=%d) to %s:%d while not ready.\n",
				skinny_message_type2str(reply->type), reply->type, reply->length,
				listener->device_name, listener->device_instance);
		return SWITCH_STATUS_FALSE;
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

