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

skinny_globals_t globals;

struct soft_key_template_definition soft_key_template_default[] = {
	{ "\200\001", SOFTKEY_REDIAL },
	{ "\200\002", SOFTKEY_NEWCALL },
	{ "\200\003", SOFTKEY_HOLD },
	{ "\200\004", SOFTKEY_TRNSFER },
	{ "\200\005", SOFTKEY_CFWDALL },
	{ "\200\006", SOFTKEY_CFWDBUSY },
	{ "\200\007", SOFTKEY_CFWDNOANSWER },
	{ "\200\010", SOFTKEY_BKSPC },
	{ "\200\011", SOFTKEY_ENDCALL },
	{ "\200\012", SOFTKEY_RESUME },
	{ "\200\013", SOFTKEY_ANSWER },
	{ "\200\014", SOFTKEY_INFO },
	{ "\200\015", SOFTKEY_CONFRN },
	{ "\200\016", SOFTKEY_PARK },
	{ "\200\017", SOFTKEY_JOIN },
	{ "\200\020", SOFTKEY_MEETME },
	{ "\200\021", SOFTKEY_PICKUP },
	{ "\200\022", SOFTKEY_GPICKUP },
	{ "\200\077", SOFTKEY_DND },
	{ "\200\120", SOFTKEY_IDIVERT },
};

/*****************************************************************************/
/* SKINNY FUNCTIONS */
/*****************************************************************************/

char* skinny_codec2string(enum skinny_codecs skinnycodec)
{
	switch (skinnycodec) {
		case SKINNY_CODEC_ALAW_64K:
		case SKINNY_CODEC_ALAW_56K:
			return "ALAW";
		case SKINNY_CODEC_ULAW_64K:
		case SKINNY_CODEC_ULAW_56K:
			return "ULAW";
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
	
	if (!globals.running) {
		return SWITCH_STATUS_FALSE;
	}

	ptr = mbuf;

	while (listener->sock && globals.running) {
		uint8_t do_sleep = 1;
		if(bytes < SKINNY_MESSAGE_FIELD_SIZE) {
			/* We have nothing yet, get length header field */
			mlen = SKINNY_MESSAGE_FIELD_SIZE - bytes;
		} else {
			/* We now know the message size */
			mlen = request->length + 2*SKINNY_MESSAGE_FIELD_SIZE - bytes;
		}

		status = switch_socket_recv(listener->sock, ptr, &mlen);
		
		if (!globals.running || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket break.\n");
			return SWITCH_STATUS_FALSE;
		}
		
		if(mlen) {
			bytes += mlen;
			
			if(bytes >= SKINNY_MESSAGE_FIELD_SIZE) {
				do_sleep = 0;
				ptr += mlen;
				memcpy(request, mbuf, bytes);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					"Got request: length=%d,reserved=%x,type=%x\n",
					request->length,request->reserved,request->type);
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						"Got complete request: length=%d,reserved=%x,type=%x,data=%d\n",
						request->length,request->reserved,request->type,request->data.as_char);
					*req = request;
					return  SWITCH_STATUS_SUCCESS;
				}
			}
		}
		if (listener->expire_time && listener->expire_time < switch_epoch_time_now(NULL)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Listener timed out.\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			return SWITCH_STATUS_FALSE;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

int skinny_device_event_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_event_t *event = (switch_event_t *) pArg;

	char *device_name = argv[0];
	char *user_id = argv[1];
	char *instance = argv[2];
	char *ip = argv[3];
	char *device_type = argv[4];
	char *max_streams = argv[5];
	char *port = argv[6];
	char *codec_string = argv[7];

	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Device-Name", device_name);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-User-Id", "%s", user_id);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Instance", "%s", instance);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-IP", ip);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Device-Type", "%s", device_type);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Max-Streams", "%s", max_streams);
	switch_event_add_header(       event, SWITCH_STACK_BOTTOM, "Skinny-Port", "%s", port);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Skinny-Codecs", codec_string);

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
	if ((sql = switch_mprintf("SELECT * FROM skinny_devices WHERE name='%s'", listener->device_name))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_device_event_callback, event);
		switch_safe_free(sql);
	}

	*ev = event;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_send_call_info(switch_core_session_t *session)
{
	private_t *tech_pvt;
	listener_t *listener;
	
	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	
	listener = tech_pvt->listener;
	switch_assert(listener != NULL);

	send_call_info(tech_pvt->listener,
		"TODO", /* char calling_party_name[40], */
		"TODO", /* char calling_party[24], */
		"TODO", /* char called_party_name[40], */
		"TODO", /* char called_party[24], */
		tech_pvt->line, /* uint32_t line_instance, */
		tech_pvt->call_id, /* uint32_t call_id, */
		SKINNY_OUTBOUND_CALL, /* uint32_t call_type, */
		"TODO", /* char original_called_party_name[40], */
		"TODO", /* char original_called_party[24], */
		"TODO", /* char last_redirecting_party_name[40], */
		"TODO", /* char last_redirecting_party[24], */
		0, /* uint32_t original_called_party_redirect_reason, */
		0, /* uint32_t last_redirecting_reason, */
		"TODO", /* char calling_party_voice_mailbox[24], */
		"TODO", /* char called_party_voice_mailbox[24], */
		"TODO", /* char original_called_party_voice_mailbox[24], */
		"TODO", /* char last_redirecting_voice_mailbox[24], */
		1, /* uint32_t call_instance, */
		1, /* uint32_t call_security_status, */
		0 /* uint32_t party_pi_restriction_bits */
	);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_pick_up(listener_t *listener, uint32_t line)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	private_t *tech_pvt;

	if (!(session = switch_core_session_request(skinny_get_endpoint_interface(), SWITCH_CALL_DIRECTION_INBOUND, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(session, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Error Creating Session private object\n");
		goto error;
	}

	switch_core_session_add_stream(session, NULL);

	tech_pvt->listener = listener;
	tech_pvt->line = line;

	tech_init(tech_pvt, session);

	set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0);
	set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	set_lamp(listener, SKINNY_BUTTON_LINE, tech_pvt->line, SKINNY_LAMP_ON);
	send_call_state(listener,
		SKINNY_OFF_HOOK,
		tech_pvt->line,
		tech_pvt->call_id);
	skinny_line_set_state(listener, tech_pvt->line, SKINNY_KEY_SET_OFF_HOOK, tech_pvt->call_id);
	display_prompt_status(listener,
		0,
		"\200\000",
		tech_pvt->line,
		tech_pvt->call_id);
	activate_call_plane(listener, tech_pvt->line);
	start_tone(listener, SKINNY_TONE_DIALTONE, 0, tech_pvt->line, tech_pvt->call_id);

	channel = switch_core_session_get_channel(session);
	goto done;
error:
	if (session) {
		switch_core_session_destroy(&session);
	}

	return SWITCH_STATUS_FALSE;

done:
	listener->session[line] = session;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_answer(switch_core_session_t *session)
{
	private_t *tech_pvt;
	listener_t *listener;
	
	tech_pvt = switch_core_session_get_private(session);
	switch_assert(tech_pvt != NULL);
	
	listener = tech_pvt->listener;
	switch_assert(listener != NULL);

	set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0); /* TODO : here ? */
	stop_tone(listener, tech_pvt->line, tech_pvt->call_id);
	open_receive_channel(listener,
		tech_pvt->call_id, /* uint32_t conference_id, */
		0, /* uint32_t pass_thru_party_id, */
		20, /* uint32_t packets, */
		SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
		0, /* uint32_t echo_cancel_type, */
		0, /* uint32_t g723_bitrate, */
		0, /* uint32_t conference_id2, */
		0 /* uint32_t reserved[10] */
	);
	send_call_state(listener,
		SKINNY_CONNECTED,
		tech_pvt->line,
		tech_pvt->call_id);
	skinny_line_set_state(listener, tech_pvt->line, SKINNY_KEY_SET_CONNECTED, tech_pvt->call_id);
	display_prompt_status(listener,
		0,
		"\200\030",
		tech_pvt->line,
		tech_pvt->call_id);
	skinny_send_call_info(session);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_hangup(listener_t *listener, uint32_t line)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	channel = switch_core_session_get_channel(listener->session[line]);
	assert(channel != NULL);

	tech_pvt = switch_core_session_get_private(listener->session[line]);
	assert(tech_pvt != NULL);

	switch_clear_flag_locked(tech_pvt, TFLAG_IO);
	switch_clear_flag_locked(tech_pvt, TFLAG_VOICE);
	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	return SWITCH_STATUS_SUCCESS;
}

/*****************************************************************************/
/* SKINNY MESSAGE HELPER */
/*****************************************************************************/
switch_status_t start_tone(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t stop_tone(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.stop_tone));
	message->type = STOP_TONE_MESSAGE;
	message->length = 4 + sizeof(message->data.stop_tone);
	message->data.stop_tone.line_instance = line_instance;
	message->data.stop_tone.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t set_ringer(listener_t *listener,
	uint32_t ring_type,
	uint32_t ring_mode,
	uint32_t unknown)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.ringer));
	message->type = SET_RINGER_MESSAGE;
	message->length = 4 + sizeof(message->data.ringer);
	message->data.ringer.ring_type = ring_type;
	message->data.ringer.ring_mode = ring_mode;
	message->data.ringer.unknown = unknown;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t set_lamp(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t set_speaker_mode(listener_t *listener,
	uint32_t mode)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speaker_mode));
	message->type = SET_SPEAKER_MODE_MESSAGE;
	message->length = 4 + sizeof(message->data.speaker_mode);
	message->data.speaker_mode.mode = mode;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t start_media_transmission(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t stop_media_transmission(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t send_call_info(listener_t *listener,
	char calling_party_name[40],
	char calling_party[24],
	char called_party_name[40],
	char called_party[24],
	uint32_t line_instance,
	uint32_t call_id,
	uint32_t call_type,
	char original_called_party_name[40],
	char original_called_party[24],
	char last_redirecting_party_name[40],
	char last_redirecting_party[24],
	uint32_t original_called_party_redirect_reason,
	uint32_t last_redirecting_reason,
	char calling_party_voice_mailbox[24],
	char called_party_voice_mailbox[24],
	char original_called_party_voice_mailbox[24],
	char last_redirecting_voice_mailbox[24],
	uint32_t call_instance,
	uint32_t call_security_status,
	uint32_t party_pi_restriction_bits)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.call_info));
	message->type = CALL_INFO_MESSAGE;
	message->length = 4 + sizeof(message->data.call_info);
	strcpy(message->data.call_info.calling_party_name, calling_party_name);
	strcpy(message->data.call_info.calling_party, calling_party);
	strcpy(message->data.call_info.called_party_name, called_party_name);
	strcpy(message->data.call_info.called_party, called_party);
	message->data.call_info.line_instance = line_instance;
	message->data.call_info.call_id = call_id;
	message->data.call_info.call_type = call_type;
	strcpy(message->data.call_info.original_called_party_name, original_called_party_name);
	strcpy(message->data.call_info.original_called_party, original_called_party);
	strcpy(message->data.call_info.last_redirecting_party_name, last_redirecting_party_name);
	strcpy(message->data.call_info.last_redirecting_party, last_redirecting_party);
	message->data.call_info.original_called_party_redirect_reason = original_called_party_redirect_reason;
	message->data.call_info.last_redirecting_reason = last_redirecting_reason;
	strcpy(message->data.call_info.calling_party_voice_mailbox, calling_party_voice_mailbox);
	strcpy(message->data.call_info.called_party_voice_mailbox, called_party_voice_mailbox);
	strcpy(message->data.call_info.original_called_party_voice_mailbox, original_called_party_voice_mailbox);
	strcpy(message->data.call_info.last_redirecting_voice_mailbox, last_redirecting_voice_mailbox);
	message->data.call_info.call_instance = call_instance;
	message->data.call_info.call_security_status = call_security_status;
	message->data.call_info.party_pi_restriction_bits = party_pi_restriction_bits;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t open_receive_channel(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t close_receive_channel(listener_t *listener,
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
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
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t display_prompt_status(listener_t *listener,
	uint32_t timeout,
	char display[32],
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.display_prompt_status));
	message->type = DISPLAY_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.display_prompt_status);
	message->data.display_prompt_status.timeout = timeout;
	strcpy(message->data.display_prompt_status.display, display);
	message->data.display_prompt_status.line_instance = line_instance;
	message->data.display_prompt_status.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t clear_prompt_status(listener_t *listener,
	uint32_t line_instance,
	uint32_t call_id)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.clear_prompt_status));
	message->type = CLEAR_PROMPT_STATUS_MESSAGE;
	message->length = 4 + sizeof(message->data.clear_prompt_status);
	message->data.clear_prompt_status.line_instance = line_instance;
	message->data.clear_prompt_status.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t activate_call_plane(listener_t *listener,
	uint32_t line_instance)
{
	skinny_message_t *message;
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.activate_call_plane));
	message->type = ACTIVATE_CALL_PLANE_MESSAGE;
	message->length = 4 + sizeof(message->data.activate_call_plane);
	message->data.activate_call_plane.line_instance = line_instance;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
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
	strcpy(message->data.dialed_number.called_party, called_party);
	message->data.dialed_number.line_instance = line_instance;
	message->data.dialed_number.call_id = call_id;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

/* Message handling */
switch_status_t skinny_handle_alarm(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;

	skinny_check_data_length(request, sizeof(request->data.alarm));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Received alarm: Severity=%d, DisplayMessage=%s, Param1=%d, Param2=%d.\n",
		request->data.alarm.alarm_severity, request->data.alarm.display_message,
		request->data.alarm.alarm_param1, request->data.alarm.alarm_param2);
	/* skinny::alarm event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_ALARM);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Severity", "%d", request->data.alarm.alarm_severity);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-DisplayMessage", "%s", request->data.alarm.display_message);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param1", "%d", request->data.alarm.alarm_param1);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Skinny-Alarm-Param2", "%d", request->data.alarm.alarm_param2);
	switch_event_fire(&event);
	
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_register(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	skinny_message_t *message;
	skinny_profile_t *profile;
	switch_event_t *event = NULL;
	switch_event_t *params = NULL;
	switch_xml_t xroot, xdomain, xgroup, xuser, xskinny, xbuttons, xbutton;
	char *sql;
	assert(listener->profile);
	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.reg));

	if(!zstr(listener->device_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"A device is already registred on this listener.\n");
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "A device is already registred on this listener");
		skinny_send_reply(listener, message);
		return SWITCH_STATUS_FALSE;
	}

	/* Check directory */
	skinny_device_event(listener, &params, SWITCH_EVENT_REQUEST_PARAMS, SWITCH_EVENT_SUBCLASS_ANY);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "skinny-auth");

	if (switch_xml_locate_user("id", request->data.reg.device_name, profile->domain, "", &xroot, &xdomain, &xuser, &xgroup, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find device [%s@%s]\n"
					  "You must define a domain called '%s' in your directory and add a user with id=\"%s\".\n"
					  , request->data.reg.device_name, profile->domain, profile->domain, request->data.reg.device_name);
		message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_rej));
		message->type = REGISTER_REJ_MESSAGE;
		message->length = 4 + sizeof(message->data.reg_rej);
		strcpy(message->data.reg_rej.error, "Device not found");
		skinny_send_reply(listener, message);
		status =  SWITCH_STATUS_FALSE;
		goto end;
	}

	if ((sql = switch_mprintf(
			"INSERT INTO skinny_devices "
				"(name, user_id, instance, ip, type, max_streams, codec_string) "
				"VALUES ('%s','%d','%d', '%s', '%d', '%d', '%s')",
			request->data.reg.device_name,
			request->data.reg.user_id,
			request->data.reg.instance,
			inet_ntoa(request->data.reg.ip),
			request->data.reg.device_type,
			request->data.reg.max_streams,
			"" /* codec_string */
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}


	strcpy(listener->device_name, request->data.reg.device_name);

	xskinny = switch_xml_child(xuser, "skinny");
	if (xskinny) {
		xbuttons = switch_xml_child(xskinny, "buttons");
		if (xbuttons) {
			for (xbutton = switch_xml_child(xbuttons, "button"); xbutton; xbutton = xbutton->next) {
				const char *position = switch_xml_attr_soft(xbutton, "position");
				const char *type = switch_xml_attr_soft(xbutton, "type");
				const char *label = switch_xml_attr_soft(xbutton, "label");
				const char *value = switch_xml_attr_soft(xbutton, "value");
				const char *settings = switch_xml_attr_soft(xbutton, "settings");
				if ((sql = switch_mprintf(
						"INSERT INTO skinny_buttons "
							"(device_name, position, type, label, value, settings) "
							"VALUES('%s', '%s', '%s', '%s', '%s', '%s')",
						request->data.reg.device_name,
						position,
						type,
						label,
						value,
						settings))) {
					skinny_execute_sql(profile, sql, profile->listener_mutex);
					switch_safe_free(sql);
				}
			}
		}
	}

	status = SWITCH_STATUS_SUCCESS;

	/* Reply with RegisterAckMessage */
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.reg_ack));
	message->type = REGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.reg_ack);
	message->data.reg_ack.keepAlive = profile->keep_alive;
	memcpy(message->data.reg_ack.dateFormat, profile->date_format, 6);
	message->data.reg_ack.secondaryKeepAlive = profile->keep_alive;
	skinny_send_reply(listener, message);

	/* Send CapabilitiesReqMessage */
	message = switch_core_alloc(listener->pool, 12);
	message->type = CAPABILITIES_REQ_MESSAGE;
	message->length = 4;
	skinny_send_reply(listener, message);

	/* skinny::register event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_REGISTER);
	switch_event_fire(&event);
	
	keepalive_listener(listener, NULL);

end:
	if(params) {
		switch_event_destroy(&params);
	}
	
	return status;
}

switch_status_t skinny_headset_status_message(listener_t *listener, skinny_message_t *request)
{
	skinny_check_data_length(request, sizeof(request->data.headset_status));
	
	/* Nothing to do */
	return SWITCH_STATUS_SUCCESS;
}

int skinny_config_stat_res_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;
	char *device_name = argv[0];
	int user_id = atoi(argv[1]);
	int instance = atoi(argv[2]);
	char *user_name = argv[3];
	char *server_name = argv[4];
	int number_lines = atoi(argv[5]);
	int number_speed_dials = atoi(argv[6]);
	
	strcpy(message->data.config_res.device_name, device_name);
	message->data.config_res.user_id = user_id;
	message->data.config_res.instance = instance;
	strcpy(message->data.config_res.user_name, user_name);
	strcpy(message->data.config_res.server_name, server_name);
	message->data.config_res.number_lines = number_lines;
	message->data.config_res.number_speed_dials = number_speed_dials;

	return 0;
}

switch_status_t skinny_handle_config_stat_request(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.config_res));
	message->type = CONFIG_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.config_res);

	if ((sql = switch_mprintf(
			"SELECT name, user_id, instance, '' AS user_name, '' AS server_name, "
				"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND type='line') AS number_lines, "
				"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND type='speed-dial') AS number_speed_dials "
				"FROM skinny_devices WHERE name='%s' ",
			listener->device_name,
			listener->device_name,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_config_stat_res_callback, message);
		switch_safe_free(sql);
	}
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_capabilities_response(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	uint32_t i = 0;
	uint32_t n = 0;
	char *codec_order[SWITCH_MAX_CODECS];
	char *codec_string;
	
	size_t string_len, string_pos, pos;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count));

	n = request->data.cap_res.count;
	if (n > SWITCH_MAX_CODECS) {
		n = SWITCH_MAX_CODECS;
	}
	string_len = -1;

	skinny_check_data_length(request, sizeof(request->data.cap_res.count) + n * sizeof(request->data.cap_res.caps[0]));

	for (i = 0; i < n; i++) {
		char *codec = skinny_codec2string(request->data.cap_res.caps[i].codec);
		codec_order[i] = codec;
		string_len += strlen(codec)+1;
	}
	i = 0;
	pos = 0;
	codec_string = switch_core_alloc(listener->pool, string_len+1);
	for (string_pos = 0; string_pos < string_len; string_pos++) {
		char *codec = codec_order[i];
		switch_assert(i < n);
		if(pos == strlen(codec)) {
			codec_string[string_pos] = ',';
			i++;
			pos = 0;
		} else {
			codec_string[string_pos] = codec[pos++];
		}
	}
	codec_string[string_len] = '\0';
	if ((sql = switch_mprintf(
			"UPDATE skinny_devices SET codec_string='%s' WHERE name='%s'",
			codec_string,
			listener->device_name
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Codecs %s supported.\n", codec_string);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_port_message(listener_t *listener, skinny_message_t *request)
{
	char *sql;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.as_uint16));

	if ((sql = switch_mprintf(
			"UPDATE skinny_devices SET port='%d' WHERE name='%s'",
			request->data.as_uint16,
			listener->device_name
			))) {
		skinny_execute_sql(profile, sql, profile->listener_mutex);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

struct button_template_helper {
	skinny_message_t *message;
	int count[0xff+1];
};

int skinny_handle_button_template_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct button_template_helper *helper = pArg;
	skinny_message_t *message = helper->message;
	char *device_name = argv[0];
	int position = atoi(argv[1]);
	char *type = argv[2];
	int i;
	
	/* fill buttons between previous one and current one */
	for(i = message->data.button_template.button_count; i+1 < position; i++) {
		message->data.button_template.btn[i].instance_number = ++helper->count[SKINNY_BUTTON_UNDEFINED];
		message->data.button_template.btn[i].button_definition = SKINNY_BUTTON_UNDEFINED;
		message->data.button_template.button_count++;
		message->data.button_template.total_button_count++;
	}


	if (!strcasecmp(type, "line")) {
		message->data.button_template.btn[i].instance_number = ++helper->count[SKINNY_BUTTON_LINE];
		message->data.button_template.btn[position-1].button_definition = SKINNY_BUTTON_LINE;
	} else if (!strcasecmp(type, "speed-dial")) {
		message->data.button_template.btn[i].instance_number = ++helper->count[SKINNY_BUTTON_SPEED_DIAL];
		message->data.button_template.btn[position-1].button_definition = SKINNY_BUTTON_SPEED_DIAL;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
			"Unknown button type %s for device %s.\n", type, device_name);
	}
	message->data.button_template.button_count++;
	message->data.button_template.total_button_count++;

	return 0;
}

switch_status_t skinny_handle_button_template_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct button_template_helper helper = {0};
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.button_template));
	message->type = BUTTON_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.button_template);

	message->data.button_template.button_offset = 0;
	message->data.button_template.button_count = 0;
	message->data.button_template.total_button_count = 0;
	
	helper.message = message;
	/* Add buttons */
	if ((sql = switch_mprintf(
			"SELECT device_name, position, type "
				"FROM skinny_buttons WHERE device_name='%s' ORDER BY position",
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}
	
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_soft_key_template_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.soft_key_template));
	message->type = SOFT_KEY_TEMPLATE_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.soft_key_template);

	message->data.soft_key_template.soft_key_offset = 0;
	message->data.soft_key_template.soft_key_count = 21;
	message->data.soft_key_template.total_soft_key_count = 21;
	
	memcpy(message->data.soft_key_template.soft_key,
		soft_key_template_default,
		sizeof(soft_key_template_default));
	
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_soft_key_set_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.soft_key_set));
	message->type = SOFT_KEY_SET_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.soft_key_set);

	message->data.soft_key_set.soft_key_set_offset = 0;
	message->data.soft_key_set.soft_key_set_count = 11;
	message->data.soft_key_set.total_soft_key_set_count = 11;
	
	/* TODO fill the set */
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

int skinny_line_stat_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;

	message->data.line_res.number++;
	if (message->data.line_res.number == atoi(argv[0])) { /* wanted_position */
		strncpy(message->data.line_res.name, argv[2], 24); /* label */
		strncpy(message->data.line_res.shortname,  argv[3], 40); /* value */
		strncpy(message->data.line_res.displayname,  argv[4], 44); /* settings */
	}
	return 0;
}

switch_status_t skinny_handle_line_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.line_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.line_res));
	message->type = LINE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.line_res);
	message->data.line_res.number = 0;

	if ((sql = switch_mprintf(
			"SELECT '%d' AS wanted_position, position, label, value, settings "
				"FROM skinny_buttons WHERE device_name='%s' AND type='line' "
				"ORDER BY position",
			request->data.line_req.number,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_line_stat_request_callback, message);
		switch_safe_free(sql);
	}
	message->data.line_res.number = request->data.line_req.number;
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}


int skinny_handle_speed_dial_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	skinny_message_t *message = pArg;

	message->data.speed_dial_res.number++;
	if (message->data.speed_dial_res.number == atoi(argv[0])) { /* wanted_position */
		strncpy(message->data.speed_dial_res.line, argv[3], 24); /* value */
		strncpy(message->data.speed_dial_res.label,  argv[2], 40); /* label */
	}
	return 0;
}

switch_status_t skinny_handle_speed_dial_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	skinny_profile_t *profile;
	char *sql;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.speed_dial_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speed_dial_res));
	message->type = SPEED_DIAL_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.speed_dial_res);
	message->data.speed_dial_res.number = 0;
	if ((sql = switch_mprintf(
			"SELECT '%d' AS wanted_position, position, label, value, settings "
				"FROM skinny_buttons WHERE device_name='%s' AND type='speed-dial' "
				"ORDER BY position",
			request->data.speed_dial_req.number,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->listener_mutex, sql, skinny_handle_speed_dial_request_callback, message);
		switch_safe_free(sql);
	}
	message->data.speed_dial_res.number = request->data.speed_dial_req.number;
	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_register_available_lines_message(listener_t *listener, skinny_message_t *request)
{
	skinny_check_data_length(request, sizeof(request->data.reg_lines));

	/* Do nothing */
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_time_date_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	switch_time_t ts;
	switch_time_exp_t tm;
	
	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.define_time_date));
	message->type = DEFINE_TIME_DATE_MESSAGE;
	message->length = 4+sizeof(message->data.define_time_date);
	ts = switch_micro_time_now();
	switch_time_exp_lt(&tm, ts);
	message->data.define_time_date.year = tm.tm_year + 1900;
	message->data.define_time_date.month = tm.tm_mon + 1;
	message->data.define_time_date.day_of_week = tm.tm_wday;
	message->data.define_time_date.day = tm.tm_yday + 1;
	message->data.define_time_date.hour = tm.tm_hour;
	message->data.define_time_date.minute = tm.tm_min;
	message->data.define_time_date.seconds = tm.tm_sec + 1;
	message->data.define_time_date.milliseconds = tm.tm_usec / 1000;
	message->data.define_time_date.timestamp = ts / 1000000;
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_keep_alive_message(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;

	message = switch_core_alloc(listener->pool, 12);
	message->type = KEEP_ALIVE_ACK_MESSAGE;
	message->length = 4;
	keepalive_listener(listener, NULL);
	skinny_send_reply(listener, message);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_soft_key_event_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	skinny_profile_t *profile;
	uint32_t line;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.soft_key_event));

	if(request->data.soft_key_event.line_instance) {
		line = request->data.soft_key_event.line_instance;
	} else {
		line = 1;
	}

	if(!listener->session[line]) { /*the line is not busy */
		switch(request->data.soft_key_event.event) {
			case SOFTKEY_NEWCALL:
				skinny_pick_up(listener, line);
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					"Unknown SoftKeyEvent type while not busy: %d.\n", request->data.soft_key_event.event);
		}
	} else { /* the line is busy */
		switch(request->data.soft_key_event.event) {
			case SOFTKEY_ENDCALL:
				skinny_hangup(listener, line);
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					"Unknown SoftKeyEvent type while busy: %d.\n", request->data.soft_key_event.event);
		}
	}
	return status;
}

switch_status_t skinny_handle_off_hook_message(listener_t *listener, skinny_message_t *request)
{
	skinny_profile_t *profile;
	uint32_t line;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.off_hook));

	if(request->data.off_hook.line_instance) {
		line = request->data.off_hook.line_instance;
	} else {
		line = 1;
	}
	if(listener->session[line]) { /*answering a call */
		skinny_answer(listener->session[line]);
	} else { /* start a new call */
		skinny_pick_up(listener, line);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_open_receive_channel_ack_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	skinny_profile_t *profile;
	uint32_t line = 0;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.open_receive_channel_ack));

	for(int i = 0 ; i < SKINNY_MAX_BUTTON_COUNT ; i++) {
		if(listener->session[i]) {
			private_t *tech_pvt = NULL;
			tech_pvt = switch_core_session_get_private(listener->session[i]);
			
			if(tech_pvt->party_id == request->data.open_receive_channel_ack.pass_thru_party_id) {
				line = i;
			}
		}
	}

	if(listener->session[line]) {
		const char *err = NULL;
		private_t *tech_pvt = NULL;
		switch_channel_t *channel = NULL;
		struct in_addr addr;

		tech_pvt = switch_core_session_get_private(listener->session[line]);
		channel = switch_core_session_get_channel(listener->session[line]);

		/* Codec */
		tech_pvt->iananame = "PCMU"; /* TODO */
		tech_pvt->codec_ms = 10; /* TODO */
		tech_pvt->rm_rate = 8000; /* TODO */
		tech_pvt->rm_fmtp = NULL; /* TODO */
		tech_pvt->agreed_pt = (switch_payload_t) 0; /* TODO */
		tech_pvt->rm_encoding = switch_core_strdup(switch_core_session_get_pool(listener->session[line]), "");
		skinny_tech_set_codec(tech_pvt, 0);
		if ((status = skinny_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}
		
		/* Request a local port from the core's allocator */
		if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(listener->profile->ip))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
			return SWITCH_STATUS_FALSE;
		}
		tech_pvt->local_sdp_audio_ip = switch_core_strdup(switch_core_session_get_pool(listener->session[line]), listener->profile->ip);

		tech_pvt->remote_sdp_audio_ip = inet_ntoa(request->data.open_receive_channel_ack.ip);
		tech_pvt->remote_sdp_audio_port = request->data.open_receive_channel_ack.port;

		tech_pvt->rtp_session = switch_rtp_new(tech_pvt->local_sdp_audio_ip,
											   tech_pvt->local_sdp_audio_port,
											   tech_pvt->remote_sdp_audio_ip,
											   tech_pvt->remote_sdp_audio_port,
											   tech_pvt->agreed_pt,
											   tech_pvt->read_impl.samples_per_packet,
											   tech_pvt->codec_ms * 1000,
											   (switch_rtp_flag_t) 0, "soft", &err,
											   switch_core_session_get_pool(listener->session[line]));
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG,
						  "AUDIO RTP [%s] %s:%d->%s:%d codec: %u ms: %d [%s]\n",
						  switch_channel_get_name(channel),
						  tech_pvt->local_sdp_audio_ip,
						  tech_pvt->local_sdp_audio_port,
						  tech_pvt->remote_sdp_audio_ip,
						  tech_pvt->remote_sdp_audio_port,
						  tech_pvt->agreed_pt,
						  tech_pvt->read_impl.microseconds_per_packet / 1000,
						  switch_rtp_ready(tech_pvt->rtp_session) ? "SUCCESS" : err);
		inet_aton(tech_pvt->local_sdp_audio_ip, &addr);
		start_media_transmission(listener,
			tech_pvt->call_id, /* uint32_t conference_id, */
			tech_pvt->party_id, /* uint32_t pass_thru_party_id, */
			addr.s_addr, /* uint32_t remote_ip, */
			tech_pvt->local_sdp_audio_port, /* uint32_t remote_port, */
			20, /* uint32_t ms_per_packet, */
			SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
			184, /* uint32_t precedence, */
			0, /* uint32_t silence_suppression, */
			0, /* uint16_t max_frames_per_packet, */
			0 /* uint32_t g723_bitrate */
		);
		switch_channel_mark_answered(channel);
	}
end:
	return status;
}

switch_status_t skinny_handle_keypad_button_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line = 0;

	skinny_check_data_length(request, sizeof(request->data.keypad_button));

	if(request->data.keypad_button.line_instance) {
		line = request->data.keypad_button.line_instance;
	} else {
		/* Find first active line */
		for(int i = 0 ; i < SKINNY_MAX_BUTTON_COUNT ; i++) {
			if(listener->session[i]) {
				line = i;
				break;
			}
		}
	}
	if(listener->session[line]) {
		switch_channel_t *channel = NULL;
		private_t *tech_pvt = NULL;
		char digit = '\0';

		channel = switch_core_session_get_channel(listener->session[line]);
		assert(channel != NULL);

		tech_pvt = switch_core_session_get_private(listener->session[line]);
		assert(tech_pvt != NULL);
		
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session[line]), SWITCH_LOG_DEBUG, "SEND DTMF ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);

		if (request->data.keypad_button.button == 14) {
			digit = '*';
		} else if (request->data.keypad_button.button == 15) {
			digit = '#';
		} else if (request->data.keypad_button.button >= 0 && request->data.keypad_button.button <= 9) {
			digit = '0' + request->data.keypad_button.button;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session[line]), SWITCH_LOG_WARNING, "UNKNOW DTMF RECEIVED ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);
		}

		/* TODO check call_id and line */

		if((skinny_line_get_state(listener, tech_pvt->line) == SKINNY_KEY_SET_OFF_HOOK)
				|| (skinny_line_get_state(listener, tech_pvt->line) == SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT)) {
			char name[128];
			switch_channel_t *channel;
			char *cid_name = "TODO-soft_key_event"; /* TODO */
			char *cid_num = "00000"; /* TODO */
			if(strlen(tech_pvt->dest) == 0) {/* first digit */
				stop_tone(listener, tech_pvt->line, tech_pvt->call_id);
				skinny_line_set_state(listener, tech_pvt->line, SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT, tech_pvt->call_id);
			}
			
			tech_pvt->dest[strlen(tech_pvt->dest)] = digit;
			
			if(strlen(tech_pvt->dest) >= 4) { /* TODO Number is complete */
				if (!(tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(listener->session[line]),
																		  NULL, listener->profile->dialplan, cid_name, cid_num, listener->remote_ip, NULL, NULL, NULL, "skinny" /* modname */, listener->profile->context, tech_pvt->dest)) != 0) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session[line]), SWITCH_LOG_CRIT, "Error Creating Session caller profile\n");
					goto error;
				}

				channel = switch_core_session_get_channel(listener->session[line]);
				snprintf(name, sizeof(name), "SKINNY/%s/%s/%d", listener->profile->name, listener->device_name, line);
				switch_channel_set_name(channel, name);

				switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

				if (switch_channel_get_state(channel) == CS_NEW) {
					switch_channel_set_state(channel, CS_INIT);
				}

				if (switch_core_session_thread_launch(listener->session[line]) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session[line]), SWITCH_LOG_CRIT, "Error Creating Session thread\n");
					goto error;
				}

				skinny_line_set_state(listener, tech_pvt->line, SKINNY_KEY_SET_CONNECTED, tech_pvt->call_id);
				send_dialed_number(listener, tech_pvt->dest, tech_pvt->line, tech_pvt->call_id);
				skinny_answer(listener->session[line]);

				goto done;
error:
			return SWITCH_STATUS_FALSE;
done:
			return SWITCH_STATUS_SUCCESS;
			}
		} else {
			if(digit != '\0') {
				switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0)};
				dtmf.digit = digit;
				switch_channel_queue_dtmf(channel, &dtmf);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_on_hook_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	skinny_profile_t *profile;
	uint32_t line = 0;

	switch_assert(listener->profile);
	switch_assert(listener->device_name);

	profile = listener->profile;

	skinny_check_data_length(request, sizeof(request->data.on_hook));

	if(request->data.on_hook.line_instance) {
		line = request->data.on_hook.line_instance;
	} else {
		/* Find first active line */
		for(int i = 0 ; i < SKINNY_MAX_BUTTON_COUNT ; i++) {
			if(listener->session[i]) {
				line = i;
				break;
			}
		}
	}

	if(listener->session[line]) {
		skinny_hangup(listener, line);
	}
	return status;
}

switch_status_t skinny_handle_unregister(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	/* skinny::unregister event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_UNREGISTER);
	switch_event_fire(&event);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
		"Received message (type=%x,length=%d).\n", request->type, request->length);
	if(zstr(listener->device_name) && request->type != REGISTER_MESSAGE && request->type != ALARM_MESSAGE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"Device should send a register message first.\n");
		return SWITCH_STATUS_FALSE;
	}
	switch(request->type) {
		case ALARM_MESSAGE:
			return skinny_handle_alarm(listener, request);
		/* registering phase */
		case REGISTER_MESSAGE:
			return skinny_handle_register(listener, request);
		case HEADSET_STATUS_MESSAGE:
			return skinny_headset_status_message(listener, request);
		case CONFIG_STAT_REQ_MESSAGE:
			return skinny_handle_config_stat_request(listener, request);
		case CAPABILITIES_RES_MESSAGE:
			return skinny_handle_capabilities_response(listener, request);
		case PORT_MESSAGE:
			return skinny_handle_port_message(listener, request);
		case BUTTON_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_button_template_request(listener, request);
		case SOFT_KEY_TEMPLATE_REQ_MESSAGE:
			return skinny_handle_soft_key_template_request(listener, request);
		case SOFT_KEY_SET_REQ_MESSAGE:
			return skinny_handle_soft_key_set_request(listener, request);
		case LINE_STAT_REQ_MESSAGE:
			return skinny_handle_line_stat_request(listener, request);
		case SPEED_DIAL_STAT_REQ_MESSAGE:
			return skinny_handle_speed_dial_request(listener, request);
		case REGISTER_AVAILABLE_LINES_MESSAGE:
			return skinny_handle_register_available_lines_message(listener, request);
		case TIME_DATE_REQ_MESSAGE:
			return skinny_handle_time_date_request(listener, request);
		/* live phase */
		case KEEP_ALIVE_MESSAGE:
			return skinny_handle_keep_alive_message(listener, request);
		case SOFT_KEY_EVENT_MESSAGE:
			return skinny_handle_soft_key_event_message(listener, request);
		case OFF_HOOK_MESSAGE:
			return skinny_handle_off_hook_message(listener, request);
		case OPEN_RECEIVE_CHANNEL_ACK_MESSAGE:
			return skinny_handle_open_receive_channel_ack_message(listener, request);
		case KEYPAD_BUTTON_MESSAGE:
			return skinny_handle_keypad_button_message(listener, request);
		case ON_HOOK_MESSAGE:
			return skinny_handle_on_hook_message(listener, request);
		/* end phase */
		case UNREGISTER_MESSAGE:
			return skinny_handle_unregister(listener, request);
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Unknown request type: %x (length=%d).\n", request->type, request->length);
			return SWITCH_STATUS_SUCCESS;
	}
}

switch_status_t skinny_send_reply(listener_t *listener, skinny_message_t *reply)
{
	char *ptr;
	switch_size_t len;
	switch_assert(reply != NULL);
	len = reply->length+8;
	ptr = (char *) reply;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Sending reply (type=%x,length=%d).\n",
		reply->type, reply->length);
	switch_socket_send(listener->sock, ptr, &len);
	return SWITCH_STATUS_SUCCESS;
}

