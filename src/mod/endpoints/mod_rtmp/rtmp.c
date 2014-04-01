/* 
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2012, Barracuda Networks Inc.
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
 * The Original Code is mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Barracuda Networks Inc.
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Rene <mrene@avgs.ca>
 * Joao Mesquita <jmesquita@freeswitch.org>
 * William King <william.king@quentustech.com>
 *
 * rtmp.c -- RTMP Protocol Handler
 *
 */

#include "mod_rtmp.h"

typedef struct {
	unsigned char *buf;
	size_t pos;
	size_t len;
} buffer_helper_t;

size_t my_buffer_read(void * out_buffer, size_t size, void * user_data)
{
	buffer_helper_t *helper = (buffer_helper_t*)user_data;
	size_t len = (helper->len - helper->pos) < size ? (helper->len - helper->pos) : size;
	if (len <= 0) {
		return 0;
	}
	memcpy(out_buffer, helper->buf + helper->pos, len);
	helper->pos += len;
	return len;
}

size_t my_buffer_write(const void *buffer, size_t size, void * user_data)
{
	buffer_helper_t *helper = (buffer_helper_t*)user_data;
	size_t len = (helper->len - helper->pos) < size ? (helper->len - helper->pos) : size;
	if (len <= 0) {
		return 0;
	}
	memcpy(helper->buf + helper->pos, buffer, len);
	helper->pos += len;
	return len;
}

void rtmp_handle_control(rtmp_session_t *rsession, int amfnumber)
{
	rtmp_state_t *state = &rsession->amfstate[amfnumber];
	char buf[200] = { 0 };
	char *p = buf;
	int type = state->buf[0] << 8 | state->buf[1];
	int i;
	
	for (i = 2; i < state->origlen; i++) {
		p += sprintf(p, "%02x ", state->buf[i] & 0xFF);
	}
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Control (%d): %s\n", type, buf);
	
	switch(type) {
		case RTMP_CTRL_STREAM_BEGIN:
			break;
		case RTMP_CTRL_PING_REQUEST:
			{
				unsigned char buf[] = {
					INT16(RTMP_CTRL_PING_RESPONSE),
					state->buf[2], state->buf[3], state->buf[4], state->buf[5]
				};
				rtmp_send_message(rsession, amfnumber, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Ping request\n");
			}
			break;
		case RTMP_CTRL_PING_RESPONSE:
			{
				uint32_t now = ((switch_micro_time_now()/1000) & 0xFFFFFFFF);
				uint32_t sent = state->buf[2] << 24 | state->buf[3] << 16 | state->buf[4] << 8 | state->buf[5];
				
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Ping reply: %d ms\n", (int)(now - sent));
			}
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "[amfnumber=%d] Unhandled control packet (type=0x%x)\n",
				amfnumber, type);
	}
}

void rtmp_handle_invoke(rtmp_session_t *rsession, int amfnumber)
{
	rtmp_state_t *state = &rsession->amfstate[amfnumber];
#ifdef RTMP_DEBUG_IO
	amf0_data *dump;
#endif
	int i = 0;
	buffer_helper_t helper = { state->buf, 0, state->origlen };
	int64_t transaction_id;
	const char *command;	
	int argc = 0;
	amf0_data *argv[100] = { 0 };
	rtmp_invoke_function_t function;

#if 0
	printf(">>>>> BEGIN INVOKE MSG (num=0x%02x, type=0x%02x, stream_id=0x%x)\n", amfnumber, state->type, state->stream_id);
	while((dump = amf0_data_read(my_buffer_read, &helper))) {
		amf0_data *dump2;
		printf("ELM> ");
		amf0_data_dump(stdout, dump, 0);
		printf("\n");
		while ((dump2 = amf0_data_read(my_buffer_read, &helper))) {
			printf("ELM> ");
			amf0_data_dump(stdout, dump2, 0);
			printf("\n");
			amf0_data_free(dump2);
		}
		amf0_data_free(dump);
	}
	printf("<<<<< END AMF MSG\n");	
#endif
	
#ifdef RTMP_DEBUG_IO
	{
		helper.pos = 0;

		fprintf(rsession->io_debug_in, ">>>>> BEGIN INVOKE MSG (chunk_stream=0x%02x, type=0x%02x, stream_id=0x%x)\n", amfnumber, state->type, state->stream_id);
		while((dump = amf0_data_read(my_buffer_read, &helper))) {
			amf0_data *dump2;
			fprintf(rsession->io_debug_in, "ELM> ");
			amf0_data_dump(rsession->io_debug_in, dump, 0);
			fprintf(rsession->io_debug_in, "\n");
			while ((dump2 = amf0_data_read(my_buffer_read, &helper))) {
				fprintf(rsession->io_debug_in, "ELM> ");
				amf0_data_dump(rsession->io_debug_in, dump2, 0);
				fprintf(rsession->io_debug_in, "\n");
				amf0_data_free(dump2);
			}
			amf0_data_free(dump);
		}
		fprintf(rsession->io_debug_in, "<<<<< END AMF MSG\n");
		fflush(rsession->io_debug_in);
	}
#endif	
	
	helper.pos = 0;
	while (argc < switch_arraylen(argv) && (argv[argc++] = amf0_data_read(my_buffer_read, &helper)));

	if (!(command = amf0_get_string(argv[i++]))) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Bogus INVOKE request\n");
		return;
	}
	
	transaction_id = amf0_get_number(argv[i++]);
	
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "[amfnumber=%d] Got INVOKE for %s\n", amfnumber, 
		command);

	if ((function = (rtmp_invoke_function_t)(intptr_t)switch_core_hash_find(rtmp_globals.invoke_hash, command))) {
		function(rsession, state, amfnumber, transaction_id, argc - 2, argv + 2);
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Unhandled invoke for \"%s\"\n",
			command);
	}

	/* Free all the AMF data we've read */
	for (i = 0; i < argc; i++) {
		amf0_data_free(argv[i]);
	}
}

switch_status_t rtmp_check_auth(rtmp_session_t *rsession, const char *user, const char *domain, const char *authmd5)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *auth;
	char md5[SWITCH_MD5_DIGEST_STRING_SIZE];
	switch_xml_t xml = NULL, x_param, x_params;
	switch_bool_t allow_empty_password = SWITCH_FALSE;
	const char *passwd = NULL;
    switch_bool_t disallow_multiple_registration = SWITCH_FALSE;
	switch_event_t *locate_params;
	
	switch_event_create(&locate_params, SWITCH_EVENT_GENERAL);
	switch_assert(locate_params);
	switch_event_add_header_string(locate_params, SWITCH_STACK_BOTTOM, "source", "mod_rtmp");
	
	/* Locate user */
	if (switch_xml_locate_user_merged("id", user, domain, NULL, &xml, locate_params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Authentication failed. No such user %s@%s\n", user, domain);
		goto done;
	}

	if ((x_params = switch_xml_child(xml, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr_soft(x_param, "value");

			if (!strcasecmp(var, "password")) {
				passwd = val;
			}
			if (!strcasecmp(var, "allow-empty-password")) {
				allow_empty_password = switch_true(val);
			}
            if (!strcasecmp(var, "disallow-multiple-registration")) {
				disallow_multiple_registration = switch_true(val);
			}
		}
	}
	
	if (zstr(passwd)) {
		if (allow_empty_password) {
			status = SWITCH_STATUS_SUCCESS;	
		}  else {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Authentication failed for %s@%s: empty password not allowed\n", user, switch_str_nil(domain));
		}
		goto done;
	}
	
	auth = switch_core_sprintf(rsession->pool, "%s:%s@%s:%s", rsession->uuid, user, domain, passwd);
	switch_md5_string(md5, auth, strlen(auth));
	
	if (!strncmp(md5, authmd5, SWITCH_MD5_DIGEST_STRING_SIZE)) {
		status = SWITCH_STATUS_SUCCESS;
	} else {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Authentication failed for %s@%s\n", user, domain);
	}
    
    if (disallow_multiple_registration) {
        switch_hash_index_t *hi;
        switch_thread_rwlock_rdlock(rsession->profile->session_rwlock);
        for (hi = switch_core_hash_first(rsession->profile->session_hash); hi; hi = switch_core_hash_next(&hi)) {
            void *val;	
            const void *key;
            switch_ssize_t keylen;
            rtmp_session_t *item;
            switch_core_hash_this(hi, &key, &keylen, &val);
            
            item = (rtmp_session_t *)val;
            if (rtmp_session_check_user(item, user, domain) == SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Logging out %s@%s on RTMP sesssion [%s]\n", user, domain, item->uuid);
                if (rtmp_session_logout(item, user, domain) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Unable to logout %s@%s on RTMP sesssion [%s]\n", user, domain, item->uuid);
                }
            }
            
        }
        switch_thread_rwlock_unlock(rsession->profile->session_rwlock);
    }
	
done:
	if (xml) {
		switch_xml_free(xml);
	}

	switch_event_destroy(&locate_params);

	return status;
}

switch_status_t amf_object_to_event(amf0_data *obj, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	if (obj && obj->type == AMF0_TYPE_OBJECT) {
		amf0_node *node;
		if (!*event) {
			if ((status = switch_event_create(event, SWITCH_EVENT_CUSTOM)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
		}

		for (node = amf0_object_first(obj); node; node = amf0_object_next(node)) {
			const char *name = amf0_get_string(amf0_object_get_name(node));
			const char *value = amf0_get_string(amf0_object_get_data(node));
			
			if (!zstr(name) && !zstr(value)) {
				if (!strcmp(name, "_body")) {
					switch_event_add_body(*event, "%s", value);
				} else {
					switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, name, value);
				}
			}
		}
	} else {
		status = SWITCH_STATUS_FALSE;
	}
	
	return status;
}

switch_status_t amf_event_to_object(amf0_data **obj, switch_event_t *event) 
{
	switch_event_header_t *hp;
	const char *body;
	
	switch_assert(event);
	switch_assert(obj);
	
	if (!*obj) {
		*obj = amf0_object_new();
	}
	
	for (hp = event->headers; hp; hp = hp->next) {
		amf0_object_add(*obj, hp->name, amf0_str(hp->value));
	}
	
	body = switch_event_get_body(event);
	if (!zstr(body)) {
		amf0_object_add(*obj, "_body", amf0_str(body));
	}
	
	return SWITCH_STATUS_SUCCESS;
}

void rtmp_set_chunksize(rtmp_session_t *rsession, uint32_t chunksize)
{
	if (rsession->out_chunksize != chunksize) {
		unsigned char buf[] = { 
			INT32(chunksize)
		};
		
		rtmp_send_message(rsession, 2 /*amfnumber*/, 0, RTMP_TYPE_CHUNKSIZE, 0, buf, sizeof(buf), MSG_FULLHEADER);
		rsession->out_chunksize = chunksize;
	}
}

void rtmp_get_user_variables(switch_event_t **event, switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_header_t *he;
	
	if (!*event && switch_event_create(event, SWITCH_EVENT_CLONE) != SWITCH_STATUS_SUCCESS) {
		return;
	}
	
	if ((he = switch_channel_variable_first(channel))) {
		for (; he; he = he->next) {
			if (!strncmp(he->name, RTMP_USER_VARIABLE_PREFIX, strlen(RTMP_USER_VARIABLE_PREFIX))) {
				switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, he->name, he->value);
			}
		}
		switch_channel_variable_last(channel);
	}
}


void rtmp_get_user_variables_event(switch_event_t **event, switch_event_t *var_event)
{
	switch_event_header_t *he;
	
	if (!*event && switch_event_create(event, SWITCH_EVENT_CLONE) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	if ((he = var_event->headers)) {
		for (; he; he = he->next) {
			if (!strncmp(he->name, RTMP_USER_VARIABLE_PREFIX, strlen(RTMP_USER_VARIABLE_PREFIX))) {
				switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, he->name, he->value);
			}
		}
	}
}


void rtmp_session_send_onattach(rtmp_session_t *rsession)
{
	const char *uuid = "";
	
	if (rsession->tech_pvt) {
		uuid = switch_core_session_get_uuid(rsession->tech_pvt->session);
	}

	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("onAttach"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(uuid), NULL);
	
}

void rtmp_send_display_update(switch_core_session_t *session)
{
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;

	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("displayUpdate"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(switch_core_session_get_uuid(session)),
		amf0_str(switch_str_nil(tech_pvt->display_callee_id_name)),
		amf0_str(switch_str_nil(tech_pvt->display_callee_id_number)), NULL);
}

void rtmp_send_incoming_call(switch_core_session_t *session, switch_event_t *var_event)
{
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	switch_event_t *event = NULL;
	amf0_data *obj = NULL;
	
	if (var_event) {
		rtmp_get_user_variables_event(&event, var_event);
	} else {
		rtmp_get_user_variables(&event, session);
	}
	
	if (event) {
		amf_event_to_object(&obj, event);
		switch_event_destroy(&event);
	}
	
	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("incomingCall"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(switch_core_session_get_uuid(session)),
		amf0_str(switch_str_nil(caller_profile->caller_id_name)),
		amf0_str(switch_str_nil(caller_profile->caller_id_number)),
		!zstr(tech_pvt->auth) ? amf0_str(tech_pvt->auth) : amf0_null_new(),
		obj ? obj : amf0_null_new(), NULL);
}

void rtmp_send_onhangup(switch_core_session_t *session)
{
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("onHangup"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(switch_core_session_get_uuid(session)),
		amf0_str(switch_channel_cause2str(switch_channel_get_cause(channel))), NULL);
}

void rtmp_send_event(rtmp_session_t *rsession, switch_event_t *event)
{
	amf0_data *obj = NULL;
	
	switch_assert(event != NULL);
	switch_assert(rsession != NULL);
	
	if (amf_event_to_object(&obj, event) == SWITCH_STATUS_SUCCESS) {
		rtmp_send_invoke_free(rsession, 3, 0, 0, amf0_str("event"), amf0_number_new(0), amf0_null_new(), obj, NULL);	
	}
}

void rtmp_ping(rtmp_session_t *rsession)
{
 	uint32_t now = (uint32_t)((switch_micro_time_now() / 1000) & 0xFFFFFFFF);
	unsigned char buf[] = {
		INT16(RTMP_CTRL_PING_REQUEST),
		INT32(now)
	};
	rtmp_send_message(rsession, 2, 0, RTMP_TYPE_USERCTRL, 0, buf, sizeof(buf), 0);
}

void rtmp_notify_call_state(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *state = switch_channel_callstate2str(switch_channel_get_callstate(channel));
	rtmp_private_t *tech_pvt = switch_core_session_get_private(session);
	rtmp_session_t *rsession = tech_pvt->rtmp_session;

	rtmp_send_invoke_free(rsession, 3, 0, 0,
		amf0_str("callState"),
		amf0_number_new(0),
		amf0_null_new(),
		amf0_str(switch_core_session_get_uuid(session)),
		amf0_str(state), NULL);
}

switch_status_t rtmp_send_invoke(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...)
{
	switch_status_t s;
	va_list list;
	va_start(list, stream_id);
	s = rtmp_send_invoke_v(rsession, amfnumber, RTMP_TYPE_INVOKE, timestamp, stream_id, list, SWITCH_FALSE);
	va_end(list);
	return s;
}

switch_status_t rtmp_send_invoke_free(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...)
{
	switch_status_t s;
	va_list list;
	va_start(list, stream_id);
	s = rtmp_send_invoke_v(rsession, amfnumber, RTMP_TYPE_INVOKE, timestamp, stream_id, list, SWITCH_TRUE);
	va_end(list);
	return s;
}

switch_status_t rtmp_send_notify(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...)
{
	switch_status_t s;
	va_list list;
	va_start(list, stream_id);
	s = rtmp_send_invoke_v(rsession, amfnumber, RTMP_TYPE_NOTIFY, timestamp, stream_id, list, SWITCH_FALSE);
	va_end(list);
	return s;
}

switch_status_t rtmp_send_notify_free(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint32_t stream_id, ...)
{
	switch_status_t s;
	va_list list;
	va_start(list, stream_id);
	s = rtmp_send_invoke_v(rsession, amfnumber, RTMP_TYPE_NOTIFY, timestamp, stream_id, list, SWITCH_TRUE);
	va_end(list);
	return s;
}



switch_status_t rtmp_send_invoke_v(rtmp_session_t *rsession, uint8_t amfnumber, uint8_t type, uint32_t timestamp, uint32_t stream_id, va_list list, switch_bool_t freethem)
{
	amf0_data *data;
	unsigned char buf[AMF_MAX_SIZE];
	buffer_helper_t helper = { buf, 0, AMF_MAX_SIZE };
	
	while ((data = va_arg(list, amf0_data*))) {
		//amf0_data_dump(stdout, data, 0);
		//printf("\n");
		amf0_data_write(data, my_buffer_write, &helper);
		if (freethem) {
			amf0_data_free(data);
		}
	}
	return rtmp_send_message(rsession, amfnumber, timestamp, type, stream_id, buf, helper.pos, 0);
}

/* Break message down into 128 bytes chunks, add the appropriate headers and send it out */
switch_status_t rtmp_send_message(rtmp_session_t *rsession, uint8_t amfnumber, uint32_t timestamp, uint8_t type, uint32_t stream_id, const unsigned char *message, switch_size_t len, uint32_t flags)
{
	switch_size_t pos = 0;
	uint8_t header[12] =  { amfnumber & 0x3F, INT24(0), INT24(len), type, INT32_LE(stream_id) };
	switch_size_t chunksize;
	uint8_t microhdr = (3 << 6) | amfnumber;
	switch_size_t hdrsize = 1;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	rtmp_state_t *state = &rsession->amfstate_out[amfnumber];
	
	if ((rsession->send_ack + rsession->send_ack_window) < rsession->send && 
			(type == RTMP_TYPE_VIDEO || type == RTMP_TYPE_AUDIO)) { 
		/* We're sending too fast, drop the frame */
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, 
						  "DROP %s FRAME [amfnumber=%d type=0x%x stream_id=0x%x] len=%"SWITCH_SIZE_T_FMT" \n",
						  type == RTMP_TYPE_AUDIO ? "AUDIO" : "VIDEO", amfnumber, type, stream_id, len);
		return SWITCH_STATUS_SUCCESS;
	}

	if (type != RTMP_TYPE_AUDIO && type != RTMP_TYPE_VIDEO && type != RTMP_TYPE_ACK) {
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, 
						  "[amfnumber=%d type=0x%x stream_id=0x%x] len=%"SWITCH_SIZE_T_FMT" \n", amfnumber, type, stream_id, len);	
	}
	
#ifdef RTMP_DEBUG_IO
	{
		fprintf(rsession->io_debug_out, "[amfnumber=%d type=0x%x stream_id=0x%x] len=%"SWITCH_SIZE_T_FMT" \n", amfnumber, type, stream_id, len);
		if (type == RTMP_TYPE_INVOKE || type == RTMP_TYPE_NOTIFY) {
				buffer_helper_t helper = { (unsigned char*)message, 0, len };
				amf0_data *dump;
				while((dump = amf0_data_read(my_buffer_read, &helper))) {
					amf0_data *dump2;
					fprintf(rsession->io_debug_out, "ELM> ");
					amf0_data_dump(rsession->io_debug_out, dump, 0);
					fprintf(rsession->io_debug_out, "\n");
					while ((dump2 = amf0_data_read(my_buffer_read, &helper))) {
						fprintf(rsession->io_debug_out, "ELM> ");
						amf0_data_dump(rsession->io_debug_out, dump2, 0);
						fprintf(rsession->io_debug_out, "\n");
						amf0_data_free(dump2);
					}
					amf0_data_free(dump);
				}
				fprintf(rsession->io_debug_out, "<<<<< END AMF MSG\n");
		}
		fflush(rsession->io_debug_out);
		
	}
#endif
	
	/* Find out what is the smallest header we can use */
	if (!(flags & MSG_FULLHEADER) && stream_id > 0 && state->stream_id == stream_id && timestamp >= state->ts) {
		if (state->type == type && state->origlen == (int)len) {
			if (state->ts == timestamp) {
				/* Type 3: no header! */
				hdrsize = 1;
				header[0] |= 3 << 6;
			} else {
				uint32_t delta = timestamp - state->ts;
				/* Type 2: timestamp delta */
				hdrsize = 4;
				header[0] |= 2 << 6;
				header[1] = (delta >> 16) & 0xFF;
				header[2] = (delta >> 8) & 0xFF;
				header[3] = delta & 0xFF;
			}
		} else {
			/* Type 1: ts delta + msg len + type */
			uint32_t delta = timestamp - state->ts;
			hdrsize = 8;
			header[0] |= 1 << 6;
			header[1] = (delta >> 16) & 0xFF;
			header[2] = (delta >> 8) & 0xFF;
			header[3] = delta & 0xFF;
		}
	} else {
		hdrsize = 12; /* Type 0, full header */
		header[1] = (timestamp >> 16) & 0xFF;
		header[2] = (timestamp >> 8) & 0xFF;
		header[3] = timestamp & 0xFF;
	}
	
	state->ts = timestamp;
	state->type = type;
	state->origlen = len;
	state->stream_id = stream_id;

	switch_mutex_lock(rsession->socket_mutex);
	chunksize = (len - pos) < rsession->out_chunksize ? (len - pos) : rsession->out_chunksize;
	if (rsession->profile->io->write(rsession, (unsigned char*)header, &hdrsize) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	rsession->send += hdrsize;
	
	/* Write one chunk of data */
	if (rsession->profile->io->write(rsession, (unsigned char*)message, &chunksize) != SWITCH_STATUS_SUCCESS) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}
	rsession->send += chunksize;
	pos += chunksize;
	
	/* Send more chunks if we need to */
	while (((signed)len - (signed)pos) > 0) {
		switch_mutex_unlock(rsession->socket_mutex);
		/* Let other threads send data on the socket */
		switch_mutex_lock(rsession->socket_mutex);
		hdrsize = 1;
		if (rsession->profile->io->write(rsession, (unsigned char*)&microhdr, &hdrsize) != SWITCH_STATUS_SUCCESS) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		rsession->send += hdrsize;
		
		chunksize = (len - pos) < rsession->out_chunksize ? (len - pos) : rsession->out_chunksize;
				
		if (rsession->profile->io->write(rsession, message + pos, &chunksize) != SWITCH_STATUS_SUCCESS) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
		rsession->send += chunksize;
		pos += chunksize;
	}
end:
	switch_mutex_unlock(rsession->socket_mutex);
	return status;
}

/* Returns SWITCH_STATUS_SUCCESS of the connection is still active or SWITCH_STATUS_FALSE to tear it down */
switch_status_t rtmp_handle_data(rtmp_session_t *rsession)
{
	uint8_t buf[RTMP_TCP_READ_BUF];
	switch_size_t s = RTMP_TCP_READ_BUF;

	if (rsession->state == RS_HANDSHAKE) {
		s = 1537 - rsession->hspos;
		
		if (rsession->profile->io->read(rsession, rsession->hsbuf + rsession->hspos, &s) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Disconnected from flash client\n");
			return SWITCH_STATUS_FALSE;
		}
		
		rsession->hspos += s;
		
		/* Receive C0 and C1 */
		if (rsession->hspos < 1537) {
			/* Not quite there yet */
			return SWITCH_STATUS_SUCCESS;
		}
		
		/* Send reply (S0 + S1) */
		memset(buf, 0, sizeof(buf));
		*buf = '\x03';
		s = 1537;
		rsession->profile->io->write(rsession, (unsigned char*)buf, &s);
		
		/* Send S2 */
		s = 1536;
		rsession->profile->io->write(rsession, rsession->hsbuf, &s);
		
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Sent handshake response\n");
		
		rsession->state++;
		rsession->hspos = 0;
	} else if (rsession->state == RS_HANDSHAKE2) {
		s = 1536 - rsession->hspos;
		
		/* Receive C2 */
		if (rsession->profile->io->read(rsession, rsession->hsbuf + rsession->hspos, &s) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Disconnected from flash client\n");
			return SWITCH_STATUS_FALSE;
		}
		
		rsession->hspos += s;
		
		if (rsession->hspos < 1536) {
			/* Not quite there yet */
			return SWITCH_STATUS_SUCCESS;
		}
		
		rsession->state++;
		
		//s = 1536;
		//rsession->profile->io->write(rsession, (char*)buf, &s);
		
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Done with handshake\n");
		
		
		return SWITCH_STATUS_SUCCESS;
	}  else if (rsession->state == RS_ESTABLISHED) {
		/* Process RTMP packet */
		switch(rsession->parse_state) {
			case 0:
				// Read the header's first byte
				s = 1;
				if (rsession->profile->io->read(rsession, (unsigned char*)buf, &s) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Disconnected from flash client\n");
					return SWITCH_STATUS_FALSE;
				}
				
				rsession->recv += s;
			
				switch(buf[0] >> 6) {
					case 0:
						rsession->hdrsize = 12;
						break;
					case 1:
						rsession->hdrsize = 8;
						break;
					case 2:
						rsession->hdrsize = 4;
						break;
					case 3:
						rsession->hdrsize = 1;
						break;
					default:
						rsession->hdrsize = 0;
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_CRIT, "WTF hdrsize 0x%02x %d\n", *buf, *buf >> 6);
						return SWITCH_STATUS_FALSE;
				}
				rsession->amfnumber = buf[0] & 0x3F;	/* Get rid of the 2 first bits */
				if (rsession->amfnumber > 64) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Protocol error\n");
					return SWITCH_STATUS_FALSE;
				} 
				//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Header size: %d AMF Number: %d\n", rsession->hdrsize, rsession->amfnumber);
				rsession->parse_state++;
				if (rsession->hdrsize == 1) {
					/* Skip header fetch on one-byte headers since we have it already */
					rsession->parse_state++;
				}
				rsession->parse_remain = 0;
				break;
			
			case 1:
			{
				/* Read full header and decode */
				rtmp_state_t *state = &rsession->amfstate[rsession->amfnumber];
				uint8_t *hdr = (uint8_t*)state->header.sz;
				unsigned char *readbuf = (unsigned char*)hdr;
				
				if (!rsession->parse_remain) {
					rsession->parse_remain = s = rsession->hdrsize - 1;	
				} else {
					s = rsession->parse_remain;
					readbuf += (rsession->hdrsize - 1) - s;
				}
				
				if ( !(s < 12 && s > 0) ) { /** XXX **/
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Protocol error: Invalid header size\n");
					return SWITCH_STATUS_FALSE;
				}
				
				if (rsession->profile->io->read(rsession, readbuf, &s) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Disconnected from flash client\n");
					return SWITCH_STATUS_FALSE;
				}
				
				rsession->parse_remain -= s;
				if (rsession->parse_remain > 0) {
					/* More data please */
					return SWITCH_STATUS_SUCCESS;
				}
				
				rsession->recv += s;
				
				if (rsession->hdrsize == 12) {
					state->ts = (hdr[0] << 16) | (hdr[1] << 8) | (hdr[2]);
					state->ts_delta = 0;
				} else if (rsession->hdrsize >= 4) {
					/* Save the timestamp delta since we have to re-use it with type 3 headers */
					state->ts_delta = (hdr[0] << 16) | (hdr[1] << 8) | (hdr[2]);
					state->ts += state->ts_delta;
				} else if (rsession->hdrsize == 1) {
					/* Type 3: Re-use timestamp delta if we have one */
					state->ts += state->ts_delta;
				}
				
				if (rsession->hdrsize >= 8) {
					/* Reset length counter since its included in the header */
					state->remainlen = state->origlen = (hdr[3] << 16) | (hdr[4] << 8) | (hdr[5]);
					state->buf_pos = 0;
					state->type = hdr[6];
				}
				if (rsession->hdrsize == 12) {
					state->stream_id = (hdr[10] << 24) | (hdr[9] << 16) | (hdr[8] << 8) | hdr[7];
				}
				
				if (rsession->hdrsize >= 8 && state->origlen == 0) {
					/* Happens we sometimes get a 0 length packet */
					rsession->parse_state = 0;
					return SWITCH_STATUS_SUCCESS;
				}
				
				/* FIXME: Handle extended timestamps */
				if (state->ts == 0x00ffffff) {
					return SWITCH_STATUS_FALSE;
				}
				
				rsession->parse_state++;
			}
			case 2: 
			{
				rtmp_state_t *state = &rsession->amfstate[rsession->amfnumber];
				
				if (rsession->parse_remain > 0) {
					s = rsession->parse_remain;
				} else {
					s = state->remainlen < rsession->in_chunksize ? state->remainlen : rsession->in_chunksize;
					rsession->parse_remain = s;		
				}
				
				if (!s) {
					/* Restart from beginning */
					s = state->remainlen = state->origlen;
					rsession->parse_remain = s;
					if (!s) {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Protocol error, forcing big read\n");
						s = sizeof(state->buf);
						rsession->profile->io->read(rsession, state->buf, &s);
						return SWITCH_STATUS_FALSE;
					}
				}
				
				/* Sanity check */
				if ((state->buf_pos + s) > AMF_MAX_SIZE) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "WTF %"SWITCH_SIZE_T_FMT" %"SWITCH_SIZE_T_FMT"\n",
						state->buf_pos, s);
					
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Protocol error: exceeding max AMF packet size\n");
					return SWITCH_STATUS_FALSE;
				}

				if (s > rsession->in_chunksize) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_ERROR, "Protocol error: invalid chunksize\n");
					return SWITCH_STATUS_FALSE;					
				}
				
				if (rsession->profile->io->read(rsession, state->buf + state->buf_pos, &s) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Disconnected from flash client\n");
					return SWITCH_STATUS_FALSE;
				}
				rsession->recv += s;
				
				state->remainlen -= s;
				rsession->parse_remain -= s;
				state->buf_pos += s;
				
				if (rsession->parse_remain > 0) {
					/* Need more data */
					return SWITCH_STATUS_SUCCESS;
				}

				if (state->remainlen == 0) {

					if (state->type != RTMP_TYPE_AUDIO && state->type != RTMP_TYPE_VIDEO && state->type != RTMP_TYPE_ACK) {
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "[chunk_stream=%d type=0x%x ts=%d stream_id=0x%x] len=%d\n", rsession->amfnumber, state->type, (int)state->ts, state->stream_id, state->origlen);
					}
#ifdef RTMP_DEBUG_IO
					fprintf(rsession->io_debug_in, "[chunk_stream=%d type=0x%x ts=%d stream_id=0x%x] len=%d\n", rsession->amfnumber, state->type, (int)state->ts, state->stream_id, state->origlen);
#endif
					switch(state->type) {
						case RTMP_TYPE_CHUNKSIZE:
							rsession->in_chunksize = state->buf[0] << 24 | state->buf[1] << 16 | state->buf[2] << 8 | state->buf[3];
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "SET CHUNKSIZE=%d\n", (int)rsession->in_chunksize);
							break;
						case RTMP_TYPE_USERCTRL:
							rtmp_handle_control(rsession, rsession->amfnumber);
							break;
						case RTMP_TYPE_INVOKE:
							rtmp_handle_invoke(rsession, rsession->amfnumber);
							break;
						case RTMP_TYPE_AUDIO: /* Audio data */
							switch_thread_rwlock_wrlock(rsession->rwlock);
							if (rsession->tech_pvt) {
								uint16_t len = state->origlen;
								
								if (!rsession->tech_pvt->readbuf) {
									switch_thread_rwlock_unlock(rsession->rwlock);
									return SWITCH_STATUS_FALSE;
								}

								
								switch_mutex_lock(rsession->tech_pvt->readbuf_mutex);
								if (rsession->tech_pvt->maxlen && switch_buffer_inuse(rsession->tech_pvt->readbuf) > (switch_size_t)(rsession->tech_pvt->maxlen * 40)) {
									rsession->tech_pvt->over_size++;
								} else {
									rsession->tech_pvt->over_size = 0;
								}
								if (rsession->tech_pvt->over_size > 10) {
									switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, 
													  "%s buffer > %u for 10 consecutive packets... Flushing buffer\n", 
													  switch_core_session_get_name(rsession->tech_pvt->session), rsession->tech_pvt->maxlen * 40);
									switch_buffer_zero(rsession->tech_pvt->readbuf);
									#ifdef RTMP_DEBUG_IO
									fprintf(rsession->io_debug_in, "[chunk_stream=%d type=0x%x ts=%d stream_id=0x%x] FLUSH BUFFER [exceeded %u]\n", rsession->amfnumber, state->type, (int)state->ts, state->stream_id, rsession->tech_pvt->maxlen * 5);
									#endif
								}
								switch_buffer_write(rsession->tech_pvt->readbuf, &len, 2);
								switch_buffer_write(rsession->tech_pvt->readbuf, state->buf, len);
								if (len > rsession->tech_pvt->maxlen) {
									rsession->tech_pvt->maxlen = len;
								}
								switch_mutex_unlock(rsession->tech_pvt->readbuf_mutex);
							}
							switch_thread_rwlock_unlock(rsession->rwlock);
							break;
						case RTMP_TYPE_VIDEO: /* Video data */
						case RTMP_TYPE_METADATA: /* Metadata */
							break;
						case RTMP_TYPE_WINDOW_ACK_SIZE:
							rsession->send_ack_window = (state->buf[0] << 24) | (state->buf[1] << 16) | (state->buf[2] << 8) | (state->buf[3]);
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Set window size: %lu bytes\n", (long unsigned int)rsession->send_ack_window);
							break;
						case RTMP_TYPE_ACK:
						{
							switch_time_t now = switch_micro_time_now();
							uint32_t ack = (state->buf[0] << 24) | (state->buf[1] << 16) | (state->buf[2] << 8) | (state->buf[3]);
							uint32_t delta = rsession->send_ack_ts == 0 ? 0 : now - rsession->send_ack_ts;
							
							delta /= 1000000; /* microseconds -> seconds */
							
							if (delta) {
								rsession->send_bw  = (ack - rsession->send_ack) / delta;
							}
							
							rsession->send_ack = ack;
							rsession->send_ack_ts = switch_micro_time_now();								
							break;
						}
						default:
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_WARNING, "Cannot handle message type 0x%x\n", state->type);
							break;
					}
					state->buf_pos = 0;
				}

				rsession->parse_state = 0;
				
				/* Send an ACK if we need to */
				if (rsession->recv - rsession->recv_ack_sent >= rsession->recv_ack_window) {
					unsigned char ackbuf[] = { INT32(rsession->recv) };

					rtmp_send_message(rsession, 2/*chunkstream*/, 0/*ts*/, RTMP_TYPE_ACK, 0/*msg stream id */, ackbuf, sizeof(ackbuf), 0 /*flags*/);
					rsession->recv_ack_sent = rsession->recv;
				}
				
			}
		}	
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
