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

skinny_globals_t globals;

struct soft_key_template_definition soft_key_template_default[] = {
	{ "\200\001", SOFTKEY_REDIAL },
	{ "\200\002", SOFTKEY_NEWCALL },
	{ "\200\003", SOFTKEY_HOLD },
	{ "\200\004", SOFTKEY_TRANSFER },
	{ "\200\005", SOFTKEY_CFWDALL },
	{ "\200\006", SOFTKEY_CFWDBUSY },
	{ "\200\007", SOFTKEY_CFWDNOANSWER },
	{ "\200\010", SOFTKEY_BACKSPACE },
	{ "\200\011", SOFTKEY_ENDCALL },
	{ "\200\012", SOFTKEY_RESUME },
	{ "\200\013", SOFTKEY_ANSWER },
	{ "\200\014", SOFTKEY_INFO },
	{ "\200\015", SOFTKEY_CONFRM },
	{ "\200\016", SOFTKEY_PARK },
	{ "\200\017", SOFTKEY_JOIN },
	{ "\200\020", SOFTKEY_MEETMECONFRM },
	{ "\200\021", SOFTKEY_CALLPICKUP },
	{ "\200\022", SOFTKEY_GRPCALLPICKUP },
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

	if (!listener_is_ready(listener)) {
		return SWITCH_STATUS_FALSE;
	}

	ptr = mbuf;

	while (listener_is_ready(listener)) {
		uint8_t do_sleep = 1;
		if(bytes < SKINNY_MESSAGE_FIELD_SIZE) {
			/* We have nothing yet, get length header field */
			mlen = SKINNY_MESSAGE_FIELD_SIZE - bytes;
		} else {
			/* We now know the message size */
			mlen = request->length + 2*SKINNY_MESSAGE_FIELD_SIZE - bytes;
		}

		status = switch_socket_recv(listener->sock, ptr, &mlen);

		if (!listener_is_ready(listener) || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Socket break.\n");
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
#ifdef SKINNY_MEGA_DEBUG
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
						"Got complete request: length=%d,reserved=%x,type=%x,data=%d\n",
						request->length,request->reserved,request->type,request->data.as_char);
#endif
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
switch_status_t skinny_send_call_info(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	private_t *tech_pvt;
	switch_channel_t *channel;

	char calling_party_name[40] = "UNKNOWN";
	char calling_party[24] = "0000000000";
	char called_party_name[40] = "UNKNOWN";
	char called_party[24] = "0000000000";

	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	switch_assert(tech_pvt->caller_profile != NULL);

	if(	switch_channel_test_flag(channel, CF_OUTBOUND) ) {
	    struct line_stat_res_message *button = NULL;

	    skinny_line_get(listener, line_instance, &button);

	    if (button) {
		    strncpy(calling_party_name, button->displayname, 40);
		    strncpy(calling_party, button->name, 24);
	    }	
		strncpy(called_party_name, tech_pvt->caller_profile->caller_id_name, 40);
		strncpy(called_party, tech_pvt->caller_profile->caller_id_number, 24);
	} else {
		strncpy(calling_party_name, tech_pvt->caller_profile->caller_id_name, 40);
		strncpy(calling_party, tech_pvt->caller_profile->caller_id_number, 24);
		/* TODO called party */
	}
	send_call_info(listener,
		calling_party_name, /* char calling_party_name[40], */
		calling_party, /* char calling_party[24], */
		called_party_name, /* char called_party_name[40], */
		called_party, /* char called_party[24], */
		line_instance, /* uint32_t line_instance, */
		tech_pvt->call_id, /* uint32_t call_id, */
		SKINNY_OUTBOUND_CALL, /* uint32_t call_type, */
		"", /* TODO char original_called_party_name[40], */
		"", /* TODO char original_called_party[24], */
		"", /* TODO char last_redirecting_party_name[40], */
		"", /* TODO char last_redirecting_party[24], */
		0, /* TODO uint32_t original_called_party_redirect_reason, */
		0, /* TODO uint32_t last_redirecting_reason, */
		"", /* TODO char calling_party_voice_mailbox[24], */
		"", /* TODO char called_party_voice_mailbox[24], */
		"", /* TODO char original_called_party_voice_mailbox[24], */
		"", /* TODO char last_redirecting_voice_mailbox[24], */
		1, /* uint32_t call_instance, */
		1, /* uint32_t call_security_status, */
		0 /* uint32_t party_pi_restriction_bits */
	);
	return SWITCH_STATUS_SUCCESS;
}

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
struct skinny_ring_lines_helper {
	private_t *tech_pvt;
	uint32_t lines_count;
};

int skinny_ring_lines_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_ring_lines_helper *helper = pArg;
	char *tmp;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	listener_t *listener = NULL;

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, 
	    device_name, device_instance, &listener);
	if(listener) {
		helper->lines_count++;
		skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_RING_IN);
		send_select_soft_keys(listener, line_instance, helper->tech_pvt->call_id, SKINNY_KEY_SET_RING_IN, 0xffff);
	    if ((tmp = switch_mprintf("\200\027%s", helper->tech_pvt->caller_profile->destination_number))) {
	        send_display_prompt_status(listener, 0, tmp, line_instance, helper->tech_pvt->call_id);
		    switch_safe_free(tmp);
	    }
	    if ((tmp = switch_mprintf("\005\000\000\000%s", helper->tech_pvt->caller_profile->destination_number))) {
		    send_display_pri_notify(listener, 10 /* message_timeout */, 5 /* priority */, tmp);
		    switch_safe_free(tmp);
	    }
		skinny_send_call_info(helper->tech_pvt->session, listener, line_instance);
		send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_BLINK);
		send_set_ringer(listener, SKINNY_RING_INSIDE, SKINNY_RING_FOREVER, 0, helper->tech_pvt->call_id);
	}
	return 0;
}

switch_call_cause_t skinny_ring_lines(private_t *tech_pvt)
{
	switch_status_t status;
	struct skinny_ring_lines_helper helper = {0};

	switch_assert(tech_pvt);
	switch_assert(tech_pvt->profile);
	switch_assert(tech_pvt->session);

	helper.tech_pvt = tech_pvt;
	helper.lines_count = 0;

	status = skinny_session_walk_lines(tech_pvt->profile,
	    switch_core_session_get_uuid(tech_pvt->session), skinny_ring_lines_callback, &helper);
	if(status != SWITCH_STATUS_SUCCESS) {
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	} else if(helper.lines_count == 0) {
		return SWITCH_CAUSE_UNALLOCATED_NUMBER;
	} else {
		return SWITCH_CAUSE_SUCCESS;
	}
}

/*****************************************************************************/
switch_status_t skinny_create_ingoing_session(listener_t *listener, uint32_t *line_instance_p, switch_core_session_t **session)
{
	uint32_t line_instance;
	switch_core_session_t *nsession;
	switch_channel_t *channel;
	private_t *tech_pvt;
	char name[128];
	char *sql;
	struct line_stat_res_message *button = NULL;

	line_instance = *line_instance_p;
	if((nsession = skinny_profile_find_session(listener->profile, listener, line_instance_p, 0))) {
	    switch_core_session_rwunlock(nsession);
	    if(skinny_line_get_state(listener, *line_instance_p, 0) == SKINNY_OFF_HOOK) {
	        /* Reuse existing session */
	        *session = nsession;
	        return SWITCH_STATUS_SUCCESS;
	    }
	    skinny_session_hold_line(nsession, listener, *line_instance_p);
	}
	*line_instance_p = line_instance;
	if(*line_instance_p == 0) {
	    *line_instance_p = 1;
	}

	skinny_line_get(listener, *line_instance_p, &button);

	if (!button || !button->shortname) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Line %d not found on device %s %d\n",
		    *line_instance_p, listener->device_name, listener->device_instance);
		goto error;
	}

	if (!(nsession = switch_core_session_request(skinny_get_endpoint_interface(),
	        SWITCH_CALL_DIRECTION_INBOUND, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Creating Session\n");
		goto error;
	}

	if (!(tech_pvt = (struct private_object *) switch_core_session_alloc(nsession, sizeof(*tech_pvt)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(nsession), SWITCH_LOG_CRIT, 
		    "Error Creating Session private object\n");
		goto error;
	}

	switch_core_session_add_stream(nsession, NULL);

	tech_init(tech_pvt, listener->profile, nsession);

	channel = switch_core_session_get_channel(nsession);

	snprintf(name, sizeof(name), "SKINNY/%s/%s:%d/%d", listener->profile->name, 
	    listener->device_name, listener->device_instance, *line_instance_p);
	switch_channel_set_name(channel, name);

	if (switch_core_session_thread_launch(nsession) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(nsession), SWITCH_LOG_CRIT, 
		    "Error Creating Session thread\n");
		goto error;
	}

	if (!(tech_pvt->caller_profile = switch_caller_profile_new(switch_core_session_get_pool(nsession),
													          NULL, listener->profile->dialplan, 
													          button->shortname, button->name, 
													          listener->remote_ip, NULL, NULL, NULL,
													          "skinny" /* modname */,
													          listener->profile->context, 
													          "")) != 0) {
	    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(nsession), SWITCH_LOG_CRIT, 
	                        "Error Creating Session caller profile\n");
		goto error;
	}

	switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);

	if ((sql = switch_mprintf(
			"INSERT INTO skinny_active_lines "
				"(device_name, device_instance, line_instance, channel_uuid, call_id, call_state) "
				"SELECT device_name, device_instance, line_instance, '%s', %d, %d "
				"FROM skinny_lines "
				"WHERE value='%s'",
			switch_core_session_get_uuid(nsession), tech_pvt->call_id, SKINNY_ON_HOOK, button->shortname
			))) {
		skinny_execute_sql(listener->profile, sql, listener->profile->sql_mutex);
		switch_safe_free(sql);
	}

	send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, tech_pvt->call_id);
	send_set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	send_set_lamp(listener, SKINNY_BUTTON_LINE, *line_instance_p, SKINNY_LAMP_ON);
	skinny_line_set_state(listener, *line_instance_p, tech_pvt->call_id, SKINNY_OFF_HOOK);
	send_select_soft_keys(listener, *line_instance_p, tech_pvt->call_id, SKINNY_KEY_SET_OFF_HOOK, 0xffff);
	send_display_prompt_status(listener, 0, "\200\000",
		*line_instance_p, tech_pvt->call_id);
	send_activate_call_plane(listener, *line_instance_p);

	goto done;
error:
	if (nsession) {
		switch_core_session_destroy(&nsession);
	}

	return SWITCH_STATUS_FALSE;

done:
	*session = nsession;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_process_dest(switch_core_session_t *session, listener_t *listener, uint32_t line_instance, char *dest, char append_dest, uint32_t backspace)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);
	
	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	if(!dest) {
	    if(append_dest == '\0') {/* no digit yet */
		    send_start_tone(listener, SKINNY_TONE_DIALTONE, 0, line_instance, tech_pvt->call_id);
	    } else {
	        if(strlen(tech_pvt->caller_profile->destination_number) == 0) {/* first digit */
		        send_stop_tone(listener, line_instance, tech_pvt->call_id);
	            send_select_soft_keys(listener, line_instance, tech_pvt->call_id,
	                SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT, 0xffff);
	        }
	        tech_pvt->caller_profile->destination_number = switch_core_sprintf(tech_pvt->caller_profile->pool,
	            "%s%c", tech_pvt->caller_profile->destination_number, append_dest);
	    }
	} else {
	    tech_pvt->caller_profile->destination_number = switch_core_strdup(tech_pvt->caller_profile->pool,
	        dest);
	}
	/* TODO Number is complete -> check against dialplan */
	if((strlen(tech_pvt->caller_profile->destination_number) >= 4) || dest) {
		send_dialed_number(listener, tech_pvt->caller_profile->destination_number, line_instance, tech_pvt->call_id);
	    skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_PROCEED);
	    skinny_send_call_info(session, listener, line_instance);
	    skinny_session_start_media(session, listener, line_instance);
	}

	switch_core_session_rwunlock(session);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_ring_out(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);
	
	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_RING_OUT);
	send_select_soft_keys(listener, line_instance, tech_pvt->call_id,
	    SKINNY_KEY_SET_RING_OUT, 0xffff);
	send_display_prompt_status(listener, 0, "\200\026",
		line_instance, tech_pvt->call_id);
	skinny_send_call_info(session, listener, line_instance);

	switch_core_session_rwunlock(session);

	return SWITCH_STATUS_SUCCESS;
}


struct skinny_session_answer_helper {
	private_t *tech_pvt;
	listener_t *listener;
	uint32_t line_instance;
};

int skinny_session_answer_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct skinny_session_answer_helper *helper = pArg;
	listener_t *listener = NULL;

	char *device_name = argv[0];
	uint32_t device_instance = atoi(argv[1]);
	/* uint32_t position = atoi(argv[2]); */
	uint32_t line_instance = atoi(argv[3]);
	/* char *label = argv[4]; */
	/* char *value = argv[5]; */
	/* char *caller_name = argv[6]; */
	/* uint32_t ring_on_idle = atoi(argv[7]); */
	/* uint32_t ring_on_active = atoi(argv[8]); */
	/* uint32_t busy_trigger = atoi(argv[9]); */
	/* char *forward_all = argv[10]; */
	/* char *forward_busy = argv[11]; */
	/* char *forward_noanswer = argv[12]; */
	/* uint32_t noanswer_duration = atoi(argv[13]); */
	/* char *channel_uuid = argv[14]; */
	/* uint32_t call_id = atoi(argv[15]); */
	/* uint32_t call_state = atoi(argv[16]); */

	skinny_profile_find_listener_by_device_name_and_instance(helper->tech_pvt->profile, device_name, device_instance, &listener);
	if(listener) {
	    if(!strcmp(device_name, helper->listener->device_name) 
	            && (device_instance == helper->listener->device_instance)
	            && (line_instance == helper->line_instance)) {/* the answering line */
	       	
	       	
	        send_set_ringer(listener, SKINNY_RING_OFF, SKINNY_RING_FOREVER, 0, helper->tech_pvt->call_id);
	        send_set_speaker_mode(listener, SKINNY_SPEAKER_ON);
	        send_set_lamp(listener, SKINNY_BUTTON_LINE, line_instance, SKINNY_LAMP_ON);
	        skinny_line_set_state(listener, line_instance, helper->tech_pvt->call_id, SKINNY_OFF_HOOK);
	        /* send_select_soft_keys(listener, line_instance, helper->tech_pvt->call_id, SKINNY_KEY_SET_OFF_HOOK, 0xffff); */
	        /* display_prompt_status(listener, 0, "\200\000",
		        line_instance, tech_pvt->call_id); */
	        send_activate_call_plane(listener, line_instance);
	    }
	}
	return 0;
}

switch_status_t skinny_session_answer(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	struct skinny_session_answer_helper helper = {0};
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);
	
	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	helper.tech_pvt = tech_pvt;
	helper.listener = listener;
	helper.line_instance = line_instance;

	skinny_session_walk_lines(tech_pvt->profile, switch_core_session_get_uuid(session), skinny_session_answer_callback, &helper);

	skinny_session_start_media(session, listener, line_instance);

	switch_core_session_rwunlock(session);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_start_media(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);
	
	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);
	 
	send_stop_tone(listener, line_instance, tech_pvt->call_id);
	send_open_receive_channel(listener,
	    tech_pvt->call_id, /* uint32_t conference_id, */
	    tech_pvt->call_id, /* uint32_t pass_thru_party_id, */
	    20, /* uint32_t packets, */
	    SKINNY_CODEC_ULAW_64K, /* uint32_t payload_capacity, */
	    0, /* uint32_t echo_cancel_type, */
	    0, /* uint32_t g723_bitrate, */
	    0, /* uint32_t conference_id2, */
	    0 /* uint32_t reserved[10] */
	);
	skinny_line_set_state(listener, line_instance, tech_pvt->call_id, SKINNY_CONNECTED);
	send_select_soft_keys(listener, line_instance, tech_pvt->call_id,
	    SKINNY_KEY_SET_CONNECTED, 0xffff);
	send_display_prompt_status(listener,
	    0,
	    "\200\030",
	    line_instance,
	    tech_pvt->call_id);
	skinny_send_call_info(session, listener, line_instance);

	switch_core_session_rwunlock(session);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_hold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(session);
	switch_assert(listener);
	switch_assert(listener->profile);
	
	channel = switch_core_session_get_channel(session);
	tech_pvt = switch_core_session_get_private(session);

	/* TODO */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Hold is not implemented yet. Hanging up the line.\n");

	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

	switch_core_session_rwunlock(session);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_session_unhold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance)
{
	/* TODO */
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
	    tm.tm_yday + 1,
	    tm.tm_hour,
	    tm.tm_min,
	    tm.tm_sec + 1,
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
	char display[32],
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
		send_register_reject(listener, "A device is already registred on this listener");
		return SWITCH_STATUS_FALSE;
	}

	/* Check directory */
	skinny_device_event(listener, &params, SWITCH_EVENT_REQUEST_PARAMS, SWITCH_EVENT_SUBCLASS_ANY);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "skinny-auth");

	if (switch_xml_locate_user("id", request->data.reg.device_name, profile->domain, "", &xroot, &xdomain, &xuser, &xgroup, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't find device [%s@%s]\n"
					  "You must define a domain called '%s' in your directory and add a user with id=\"%s\".\n"
					  , request->data.reg.device_name, profile->domain, profile->domain, request->data.reg.device_name);
		send_register_reject(listener, "Device not found");
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
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}


	strncpy(listener->device_name, request->data.reg.device_name, 16);
	listener->device_instance = request->data.reg.instance;

	xskinny = switch_xml_child(xuser, "skinny");
	if (xskinny) {
		xbuttons = switch_xml_child(xskinny, "buttons");
		if (xbuttons) {
			uint32_t line_instance = 1;
			for (xbutton = switch_xml_child(xbuttons, "button"); xbutton; xbutton = xbutton->next) {
				uint32_t position = atoi(switch_xml_attr_soft(xbutton, "position"));
				uint32_t type = skinny_str2button(switch_xml_attr_soft(xbutton, "type"));
				const char *label = switch_xml_attr_soft(xbutton, "label");
				const char *value = switch_xml_attr_soft(xbutton, "value");
				if(type ==  SKINNY_BUTTON_LINE) {
					const char *caller_name = switch_xml_attr_soft(xbutton, "caller-name");
					uint32_t ring_on_idle = atoi(switch_xml_attr_soft(xbutton, "ring-on-idle"));
					uint32_t ring_on_active = atoi(switch_xml_attr_soft(xbutton, "ring-on-active"));
					uint32_t busy_trigger = atoi(switch_xml_attr_soft(xbutton, "busy-trigger"));
					const char *forward_all = switch_xml_attr_soft(xbutton, "forward-all");
					const char *forward_busy = switch_xml_attr_soft(xbutton, "forward-busy");
					const char *forward_noanswer = switch_xml_attr_soft(xbutton, "forward-noanswer");
					uint32_t noanswer_duration = atoi(switch_xml_attr_soft(xbutton, "noanswer-duration"));
					if ((sql = switch_mprintf(
							"INSERT INTO skinny_lines "
								"(device_name, device_instance, position, line_instance, "
								"label, value, caller_name, "
								"ring_on_idle, ring_on_active, busy_trigger, "
	  							"forward_all, forward_busy, forward_noanswer, noanswer_duration) "
								"VALUES('%s', %d, %d, %d, '%s', '%s', '%s', %d, %d, %d, '%s', '%s', '%s', %d)",
							request->data.reg.device_name, request->data.reg.instance, position, line_instance++,
							label, value, caller_name,
							ring_on_idle, ring_on_active, busy_trigger,
	  						forward_all, forward_busy, forward_noanswer, noanswer_duration))) {
						skinny_execute_sql(profile, sql, profile->sql_mutex);
						switch_safe_free(sql);
					}
				} else {
					const char *settings = switch_xml_attr_soft(xbutton, "settings");
					if ((sql = switch_mprintf(
							"INSERT INTO skinny_buttons "
								"(device_name, device_instance, position, type, label, value, settings) "
								"VALUES('%s', %d, %d, %d, '%s', '%s', '%s')",
							request->data.reg.device_name,
							request->data.reg.instance,
							position,
							type,
							label,
							value,
							settings))) {
						skinny_execute_sql(profile, sql, profile->sql_mutex);
						switch_safe_free(sql);
					}
				}
			}
		}
	}

	status = SWITCH_STATUS_SUCCESS;

	/* Reply with RegisterAckMessage */
	send_register_ack(listener, profile->keep_alive, profile->date_format, "", profile->keep_alive, "");

	/* Send CapabilitiesReqMessage */
	send_capabilities_req(listener);

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

	strncpy(message->data.config_res.device_name, device_name, 16);
	message->data.config_res.user_id = user_id;
	message->data.config_res.instance = instance;
	strncpy(message->data.config_res.user_name, user_name, 40);
	strncpy(message->data.config_res.server_name, server_name, 40);
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
				"(SELECT COUNT(*) FROM skinny_lines WHERE device_name='%s' AND device_instance=%d) AS number_lines, "
				"(SELECT COUNT(*) FROM skinny_buttons WHERE device_name='%s' AND device_instance=%d AND type=%d) AS number_speed_dials "
				"FROM skinny_devices WHERE name='%s' ",
			listener->device_name,
			listener->device_instance,
			listener->device_name,
			listener->device_instance,
			SKINNY_BUTTON_SPEED_DIAL,
			listener->device_name
			))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_config_stat_res_callback, message);
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
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
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
			"UPDATE skinny_devices SET port=%d WHERE name='%s' and instance=%d",
			request->data.port.port,
			listener->device_name,
			listener->device_instance
			))) {
		skinny_execute_sql(profile, sql, profile->sql_mutex);
		switch_safe_free(sql);
	}
	return SWITCH_STATUS_SUCCESS;
}

struct button_template_helper {
	skinny_message_t *message;
	int count[SKINNY_BUTTON_UNDEFINED+1];
	int max_position;
};

int skinny_handle_button_template_request_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct button_template_helper *helper = pArg;
	skinny_message_t *message = helper->message;
	/* char *device_name = argv[0]; */
	/* uint32_t device_instance = argv[1]; */
	int position = atoi(argv[2]);
	uint32_t type = atoi(argv[3]);
	/* int relative_position = atoi(argv[4]); */

	message->data.button_template.btn[position-1].instance_number = ++helper->count[type];
	message->data.button_template.btn[position-1].button_definition = type;

	message->data.button_template.button_count++;
	message->data.button_template.total_button_count++;
	if(position > helper->max_position) {
		helper->max_position = position;
	}

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
			"SELECT device_name, device_instance, position, MIN(type, %d) AS type "
				"FROM skinny_buttons "
				"WHERE device_name='%s' AND device_instance=%d "
				"ORDER BY position",
			SKINNY_BUTTON_UNDEFINED,
			listener->device_name, listener->device_instance
			))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}

	/* Add lines */
	if ((sql = switch_mprintf(
			"SELECT device_name, device_instance, position, %d AS type "
				"FROM skinny_lines "
				"WHERE device_name='%s' AND device_instance=%d "
				"ORDER BY position",
			SKINNY_BUTTON_LINE,
			listener->device_name, listener->device_instance
			))) {
		skinny_execute_sql_callback(profile, profile->sql_mutex, sql, skinny_handle_button_template_request_callback, &helper);
		switch_safe_free(sql);
	}

	/* Fill remaining buttons with Undefined */
	for(int i = 0; i+1 < helper.max_position; i++) {
		if(message->data.button_template.btn[i].button_definition == SKINNY_BUTTON_UNKNOWN) {
			message->data.button_template.btn[i].instance_number = ++helper.count[SKINNY_BUTTON_UNDEFINED];
			message->data.button_template.btn[i].button_definition = SKINNY_BUTTON_UNDEFINED;
			message->data.button_template.button_count++;
			message->data.button_template.total_button_count++;
		}
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
	message->data.soft_key_set.soft_key_set[SKINNY_KEY_SET_ON_HOOK].soft_key_template_index[0] = SOFTKEY_REDIAL;
	message->data.soft_key_set.soft_key_set[SKINNY_KEY_SET_ON_HOOK].soft_key_template_index[1] = SOFTKEY_NEWCALL;
	message->data.soft_key_set.soft_key_set[SKINNY_KEY_SET_CONNECTED].soft_key_template_index[0] = SOFTKEY_ENDCALL;
	message->data.soft_key_set.soft_key_set[SKINNY_KEY_SET_RING_IN].soft_key_template_index[0] = SOFTKEY_ENDCALL;

	skinny_send_reply(listener, message);

	/* Init the states */
	send_select_soft_keys(listener, 0, 0, SKINNY_KEY_SET_ON_HOOK, 0xffff);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_line_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct line_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.line_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.line_res));
	message->type = LINE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.line_res);

	skinny_line_get(listener, request->data.line_req.number, &button);

	memcpy(&message->data.line_res, button, sizeof(struct line_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_speed_dial_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct speed_dial_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.speed_dial_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.speed_dial_res));
	message->type = SPEED_DIAL_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.speed_dial_res);

	skinny_speed_dial_get(listener, request->data.speed_dial_req.number, &button);

	memcpy(&message->data.speed_dial_res, button, sizeof(struct speed_dial_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_service_url_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct service_url_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.service_url_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.service_url_res));
	message->type = SERVICE_URL_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.service_url_res);

	skinny_service_url_get(listener, request->data.service_url_req.service_url_index, &button);

	memcpy(&message->data.service_url_res, button, sizeof(struct service_url_stat_res_message));

	skinny_send_reply(listener, message);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_feature_stat_request(listener_t *listener, skinny_message_t *request)
{
	skinny_message_t *message;
	struct feature_stat_res_message *button = NULL;

	skinny_check_data_length(request, sizeof(request->data.feature_req));

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.feature_res));
	message->type = FEATURE_STAT_RES_MESSAGE;
	message->length = 4 + sizeof(message->data.feature_res);

	skinny_feature_get(listener, request->data.feature_req.feature_index, &button);

	memcpy(&message->data.feature_res, button, sizeof(struct feature_stat_res_message));

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
	return send_define_current_time_date(listener);
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
	uint32_t line_instance = 0;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	private_t *tech_pvt = NULL;

	switch_assert(listener);
	switch_assert(listener->profile);
	
	skinny_check_data_length(request, sizeof(request->data.soft_key_event));

	line_instance = request->data.soft_key_event.line_instance;

	switch(request->data.soft_key_event.event) {
		case SOFTKEY_REDIAL:
	        status = skinny_create_ingoing_session(listener, &line_instance, &session);

		    skinny_session_process_dest(session, listener, line_instance, "redial", '\0', 0);
			break;
		case SOFTKEY_NEWCALL:
	        status = skinny_create_ingoing_session(listener, &line_instance, &session);
		    tech_pvt = switch_core_session_get_private(session);
		    assert(tech_pvt != NULL);

		    skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 0);
			break;
		case SOFTKEY_HOLD:
	        session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.soft_key_event.call_id);

		    if(session) {
	            status = skinny_session_hold_line(session, listener, line_instance);
	        }
			break;
		case SOFTKEY_ENDCALL:
	        session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.soft_key_event.call_id);

		    if(session) {
			    channel = switch_core_session_get_channel(session);

			    switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	        }
			break;
		case SOFTKEY_RESUME:
	        session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.soft_key_event.call_id);

		    if(session) {
	            status = skinny_session_unhold_line(session, listener, line_instance);
	        }
			break;
		case SOFTKEY_ANSWER:
	        session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.soft_key_event.call_id);

		    if(session) {
				status = skinny_session_answer(session, listener, line_instance);
		    }
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
				"Unknown SoftKeyEvent type while busy: %d.\n", request->data.soft_key_event.event);
	}

	if(session) {
		switch_core_session_rwunlock(session);
	}

	return status;
}

switch_status_t skinny_handle_off_hook_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line_instance;
	switch_core_session_t *session = NULL;
	private_t *tech_pvt = NULL;

	skinny_check_data_length(request, sizeof(request->data.off_hook));

	if(request->data.off_hook.line_instance > 0) {
		line_instance = request->data.off_hook.line_instance;
	} else {
		line_instance = 1;
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.off_hook.call_id);

	if(session) { /*answering a call */
		skinny_session_answer(session, listener, line_instance);
	} else { /* start a new call */
		skinny_create_ingoing_session(listener, &line_instance, &session);
	    tech_pvt = switch_core_session_get_private(session);
	    assert(tech_pvt != NULL);

	    skinny_session_process_dest(session, listener, line_instance, NULL, '\0', 0);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_stimulus_message(listener_t *listener, skinny_message_t *request)
{
	struct speed_dial_stat_res_message *button = NULL;
	uint32_t line_instance = 0;
	uint32_t call_id = 0;
	switch_core_session_t *session = NULL;

	skinny_check_data_length(request, sizeof(request->data.stimulus)-sizeof(request->data.stimulus.call_id));

	if(skinny_check_data_length_soft(request, sizeof(request->data.stimulus))) {
	    call_id = request->data.stimulus.call_id;
	}

	switch(request->data.stimulus.instance_type) {
		case SKINNY_BUTTON_LAST_NUMBER_REDIAL:
		    skinny_create_ingoing_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, "redial", '\0', 0);
			break;
		case SKINNY_BUTTON_VOICEMAIL:
		    skinny_create_ingoing_session(listener, &line_instance, &session);
			skinny_session_process_dest(session, listener, line_instance, "vmain", '\0', 0);
			break;
		case SKINNY_BUTTON_SPEED_DIAL:
			skinny_speed_dial_get(listener, request->data.stimulus.instance, &button);
			if(strlen(button->line) > 0) {
		        skinny_create_ingoing_session(listener, &line_instance, &session);
			    skinny_session_process_dest(session, listener, line_instance, button->line, '\0', 0);
			}
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown Stimulus Type Received [%d]\n", request->data.stimulus.instance_type);
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_open_receive_channel_ack_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	uint32_t line_instance = 0;
	switch_core_session_t *session;

	skinny_check_data_length(request, sizeof(request->data.open_receive_channel_ack));

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.open_receive_channel_ack.pass_thru_party_id);

	if(session) {
		const char *err = NULL;
		private_t *tech_pvt = NULL;
		switch_channel_t *channel = NULL;
		struct in_addr addr;

		tech_pvt = switch_core_session_get_private(session);
		channel = switch_core_session_get_channel(session);

		/* Codec */
		tech_pvt->iananame = "PCMU"; /* TODO */
		tech_pvt->codec_ms = 10; /* TODO */
		tech_pvt->rm_rate = 8000; /* TODO */
		tech_pvt->rm_fmtp = NULL; /* TODO */
		tech_pvt->agreed_pt = (switch_payload_t) 0; /* TODO */
		tech_pvt->rm_encoding = switch_core_strdup(switch_core_session_get_pool(session), "");
		skinny_tech_set_codec(tech_pvt, 0);
		if ((status = skinny_tech_set_codec(tech_pvt, 0)) != SWITCH_STATUS_SUCCESS) {
			goto end;
		}

		/* Request a local port from the core's allocator */
		if (!(tech_pvt->local_sdp_audio_port = switch_rtp_request_port(listener->profile->ip))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_CRIT, "No RTP ports available!\n");
			return SWITCH_STATUS_FALSE;
		}
		tech_pvt->local_sdp_audio_ip = switch_core_strdup(switch_core_session_get_pool(session), listener->profile->ip);

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
											   switch_core_session_get_pool(session));
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
		send_start_media_transmission(listener,
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
	    if (switch_channel_get_state(channel) == CS_NEW) {
		    switch_channel_set_state(channel, CS_INIT);
	    }
		switch_channel_mark_answered(channel);

		switch_core_session_rwunlock(session);
	}
end:
	return status;
}

switch_status_t skinny_handle_keypad_button_message(listener_t *listener, skinny_message_t *request)
{
	uint32_t line_instance = 0;
	switch_core_session_t *session;

	skinny_check_data_length(request, sizeof(request->data.keypad_button));
	
	if(request->data.keypad_button.line_instance) {
	    line_instance = request->data.keypad_button.line_instance;
	} else {
	    line_instance = 1;
	}

	session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.keypad_button.call_id);

	if(session) {
		switch_channel_t *channel = NULL;
		private_t *tech_pvt = NULL;
		char digit = '\0';

		channel = switch_core_session_get_channel(session);
		tech_pvt = switch_core_session_get_private(session);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SEND DTMF ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);

		if (request->data.keypad_button.button == 14) {
			digit = '*';
		} else if (request->data.keypad_button.button == 15) {
			digit = '#';
		} else if (request->data.keypad_button.button >= 0 && request->data.keypad_button.button <= 9) {
			digit = '0' + request->data.keypad_button.button;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "UNKNOW DTMF RECEIVED ON CALL %d [%d]\n", tech_pvt->call_id, request->data.keypad_button.button);
		}

		/* TODO check call_id and line */

		if((skinny_line_get_state(listener, line_instance, tech_pvt->call_id) == SKINNY_OFF_HOOK)) {
	
		    skinny_session_process_dest(session, listener, line_instance, NULL, digit, 0);
		} else {
			if(digit != '\0') {
				switch_dtmf_t dtmf = { 0, switch_core_default_dtmf_duration(0)};
				dtmf.digit = digit;
				switch_channel_queue_dtmf(channel, &dtmf);
			}
		}
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_on_hook_message(listener_t *listener, skinny_message_t *request)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_core_session_t *session = NULL;
	uint32_t line_instance = 0;

	skinny_check_data_length(request, sizeof(request->data.on_hook));

	line_instance = request->data.on_hook.line_instance;
	
	session = skinny_profile_find_session(listener->profile, listener, &line_instance, request->data.on_hook.call_id);

	if(session) {
		switch_channel_t *channel = NULL;

		channel = switch_core_session_get_channel(session);

		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

		switch_core_session_rwunlock(session);
	}
	return status;
}

switch_status_t skinny_handle_unregister(listener_t *listener, skinny_message_t *request)
{
	switch_event_t *event = NULL;
	skinny_message_t *message;

	/* skinny::unregister event */
	skinny_device_event(listener, &event, SWITCH_EVENT_CUSTOM, SKINNY_EVENT_UNREGISTER);
	switch_event_fire(&event);

	message = switch_core_alloc(listener->pool, 12+sizeof(message->data.unregister_ack));
	message->type = UNREGISTER_ACK_MESSAGE;
	message->length = 4 + sizeof(message->data.unregister_ack);
	message->data.unregister_ack.unregister_status = 0; /* OK */
	skinny_send_reply(listener, message);

	/* Close socket */
	switch_clear_flag_locked(listener, LFLAG_RUNNING);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t skinny_handle_request(listener_t *listener, skinny_message_t *request)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
		"Received %s (type=%x,length=%d).\n", skinny_message_type2str(request->type), request->type, request->length);
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
			return skinny_handle_speed_dial_stat_request(listener, request);
		case SERVICE_URL_STAT_REQ_MESSAGE:
			return skinny_handle_service_url_stat_request(listener, request);
		case FEATURE_STAT_REQ_MESSAGE:
			return skinny_handle_feature_stat_request(listener, request);
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
		case STIMULUS_MESSAGE:
			return skinny_handle_stimulus_message(listener, request);
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
				"Unhandled request %s (type=%x,length=%d).\n", skinny_message_type2str(request->type), request->type, request->length);
			return SWITCH_STATUS_SUCCESS;
	}
}

switch_status_t skinny_perform_send_reply(listener_t *listener, const char *file, const char *func, int line, skinny_message_t *reply)
{
	char *ptr;
	switch_size_t len;
	switch_assert(reply != NULL);
	len = reply->length+8;
	ptr = (char *) reply;

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_DEBUG,
		"Sending %s (type=%x,length=%d).\n",
		skinny_message_type2str(reply->type), reply->type, reply->length);
	switch_socket_send(listener->sock, ptr, &len);

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

