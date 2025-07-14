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
 * Based on mod_skel by
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * Contributor(s):
 *
 * Daniel Bryars <danb@aeriandi.com>
 * Tim Brown <tim.brown@aeriandi.com>
 * Anthony Minessale II <anthm@freeswitch.org>
 * William King <william.king@quentustech.com>
 * Mike Jerris <mike@jerris.com>
 *
 * mod_amqp.c -- Sends FreeSWITCH events to an AMQP broker
 *
 */

#include "mod_amqp.h"

switch_status_t mod_amqp_xml_handler_routing_key(mod_amqp_xml_handler_profile_t *profile,
												 char routingKey[MAX_AMQP_ROUTING_KEY_LENGTH], switch_event_t *evt,
												 mod_amqp_keypart_t routingKeyEventHeaderNames[])
{
	int i = 0, idx = 0, x = 0;
	char keybuffer[MAX_AMQP_ROUTING_KEY_LENGTH];

	for (i = 0; i < MAX_ROUTING_KEY_FORMAT_FIELDS && idx < MAX_AMQP_ROUTING_KEY_LENGTH; i++) {
		if (routingKeyEventHeaderNames[i].size) {
			if (idx) { routingKey[idx++] = '.'; }
			for (x = 0; x < routingKeyEventHeaderNames[i].size; x++) {
				if (routingKeyEventHeaderNames[i].name[x][0] == '#') {
					strncpy(routingKey + idx, routingKeyEventHeaderNames[i].name[x] + 1,
							MAX_AMQP_ROUTING_KEY_LENGTH - idx);
					break;
				} else {
					char *value = switch_event_get_header(evt, routingKeyEventHeaderNames[i].name[x]);
					if (value) {
						amqp_util_encode(value, keybuffer);
						strncpy(routingKey + idx, keybuffer, MAX_AMQP_ROUTING_KEY_LENGTH - idx);
						break;
					}
				}
			}
			idx += strlen(routingKey + idx);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_t xml_amqp_fetch(const char *section, const char *tag_name, const char *key_name,
								   const char *key_value, switch_event_t *params, void *user_data)
{
	switch_xml_t xml = NULL;
	mod_amqp_message_t *amqp_message;
	amqp_basic_properties_t props;
	mod_amqp_aux_connection_t *conn = NULL, *conn_next = NULL, *conn_tmp = NULL;

	switch_uuid_t uuid;
	amqp_rpc_reply_t res;
	amqp_envelope_t envelope;
	struct timeval timeout = {0};
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	mod_amqp_xml_handler_profile_t *profile = (mod_amqp_xml_handler_profile_t *)user_data;
	switch_time_t now = switch_time_now();
	switch_time_t reset_time;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *amqp_body = NULL;
	int amqp_body_len;
	int i = 0;

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event without a profile %p %p\n", (void *)params,
						  (void *)params->event_user_data);
		return xml;
	}

	reset_time = profile->circuit_breaker_reset_time;
	if (now < reset_time) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] circuit breaker hit[%d] (%d)\n",
						  profile->name, (int)now, (int)reset_time);
		return xml;
	}

	if (!params) {
		switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(params);
	}

	switch_event_add_header_string(params, SWITCH_STACK_TOP, "section", switch_str_nil(section));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "tag_name", switch_str_nil(tag_name));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_name", switch_str_nil(key_name));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_value", switch_str_nil(key_value));

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if (profile->running && profile->conn_active) {
		switch_mutex_lock(profile->mutex);
		for (conn = profile->conn_aux; conn; conn = conn_next) {
			if (conn->locked == 1) {
				if (conn->next == NULL && i < profile->max_temp_conn) {
					conn->next = switch_core_alloc(profile->pool, sizeof(mod_amqp_aux_connection_t));
					status = mod_amqp_aux_connection_open(profile->conn_active, &(conn->next), profile->name,
														  profile->custom_attr);
					if (status == SWITCH_STATUS_SUCCESS) { conn->next->locked = 0; }
				}
				i++;
				conn_next = conn->next;
				continue;
			}
			conn_tmp = conn;
			conn_tmp->locked = 1;
			break;
		}

		switch_mutex_unlock(profile->mutex);

		if (conn_tmp) {
			props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_REPLY_TO_FLAG |
						   AMQP_BASIC_CORRELATION_ID_FLAG;
			props.content_type = amqp_cstring_bytes("application/json");
			props.reply_to = amqp_bytes_malloc_dup(conn_tmp->queueName);
			props.delivery_mode = 1;
			if (props.reply_to.bytes == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory while copying queue name");
				goto done;
			}
			props.correlation_id = amqp_cstring_bytes(uuid_str);

			switch_malloc(amqp_message, sizeof(mod_amqp_message_t));
			mod_amqp_xml_handler_routing_key(profile, amqp_message->routing_key, params, profile->format_fields);
			amqp_maybe_release_buffers(conn_tmp->state);
			// switch_event_add_header_string(params, SWITCH_STACK_TOP, "reply_queue", conn_tmp->uuid);
			switch_event_serialize_json(params, &amqp_message->pjson);
			amqp_message->props = props;
			if (switch_queue_trypush(profile->send_queue, amqp_message) != SWITCH_STATUS_SUCCESS) {
				unsigned int queue_size = switch_queue_size(profile->send_queue);

				/* Trip the circuit breaker for a short period to stop recurring error messages (time is measured in uS)
				 */
				profile->circuit_breaker_reset_time = now + profile->circuit_breaker_ms * 1000;

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "AMQP message queue full. Messages will be dropped for %.1fs! (Queue capacity %d)",
								  profile->circuit_breaker_ms / 1000.0, queue_size);

				mod_amqp_util_msg_destroy(&amqp_message);
				goto done;
			}

			// Start a command
			amqp_basic_consume(conn_tmp->state,		// state
							   1,					// channel
							   conn_tmp->queueName, // queue
							   amqp_empty_bytes,	// command tag
							   0, 1, 0,				// no_local, no_ack, exclusive
							   amqp_empty_table);	// args

			timeout.tv_usec = 5000 * 1000;
			for (;;) {
				char *correlation_id = NULL;
				amqp_basic_properties_t *p;
                                //amqp_maybe_release_buffers_on_channel(conn_tmp->state, 1);
				amqp_maybe_release_buffers(conn_tmp->state);
				res = amqp_consume_message(conn_tmp->state, &envelope, &timeout, 0);
				if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
					if (res.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
						/* Unexpected frame. Discard it then continue */
						amqp_frame_t decoded_frame;
						amqp_simple_wait_frame(conn_tmp->state, &decoded_frame);
					}

					if (res.library_error == AMQP_STATUS_SOCKET_ERROR) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
										  "A socket error occurred. Tearing down and reconnecting\n");
						break;
					}

					if (res.library_error == AMQP_STATUS_CONNECTION_CLOSED) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
										  "AMQP connection was closed. Tearing down and reconnecting\n");
						break;
					}

					if (res.library_error == AMQP_STATUS_TCP_ERROR) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
										  "A TCP error occurred. Tearing down and reconnecting\n");
						break;
					}

					if (res.library_error == AMQP_STATUS_TIMEOUT) { break; }

					/* Try consuming again */
					continue;
				}
				if (res.reply_type == AMQP_RESPONSE_NORMAL) {
                                        p = &envelope.message.properties;
					correlation_id = p->correlation_id.bytes;

					if (correlation_id && strcmp(correlation_id, uuid_str) == 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got my message. Trying to parse\n");
						break;
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Got wrong message, Trying the next one... %s vs %s\n", correlation_id, uuid_str);
					continue;
				}
			}

			amqp_body_len = (int) envelope.message.body.len + 1;
			amqp_body = malloc(amqp_body_len);
			snprintf(amqp_body, amqp_body_len, "%s", (char *) envelope.message.body.bytes);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "XML from AMQP msg:\n%s\n", amqp_body);
			if (res.reply_type != AMQP_RESPONSE_NORMAL ||
				!(xml = switch_xml_parse_str_dynamic(amqp_body, SWITCH_TRUE))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result! \n");
			}
			switch_safe_free(amqp_body);
			amqp_bytes_free(props.reply_to);
			amqp_destroy_envelope(&envelope);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No more aux amqp connections(%d). Increase max-temp-conn\n", i);
		}
	}
done:
	if (conn_tmp) {
		switch_mutex_lock(profile->mutex);
		amqp_maybe_release_buffers(conn_tmp->state);
		conn_tmp->locked = 0;
		switch_mutex_unlock(profile->mutex);
		conn_tmp = NULL;
	}

	return xml;
}

switch_status_t mod_amqp_xml_handler_destroy(mod_amqp_xml_handler_profile_t **prof)
{
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_connection_t *conn = NULL, *conn_next = NULL;
	mod_amqp_aux_connection_t *conn_aux = NULL, *conn_next_aux = NULL;
	switch_memory_pool_t *pool;
	mod_amqp_xml_handler_profile_t *profile;

	if (!prof || !*prof) { return SWITCH_STATUS_SUCCESS; }
	switch_xml_unbind_search_function_ptr(xml_amqp_fetch);
	profile = *prof;
	pool = profile->pool;

	if (profile->name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] shutting down...\n", profile->name);
		switch_core_hash_delete(mod_amqp_globals.xml_handler_hash, profile->name);
	}

	profile->running = 0;

	if (profile->xml_handler_thread) { switch_thread_join(&status, profile->xml_handler_thread); }

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] closing AMQP socket...\n", profile->name);

	for (conn = profile->conn_root; conn; conn = conn_next) {
		conn_next = conn->next;
		mod_amqp_connection_destroy(&conn);
	}

	for (conn_aux = profile->conn_aux; conn_aux; conn_aux = conn_next_aux) {
		conn_next_aux = conn_aux->next;
		mod_amqp_aux_connection_destroy(&conn_aux);
	}

	profile->conn_aux = NULL;
	profile->conn_active = NULL;
	profile->conn_root = NULL;

	while (profile->send_queue && switch_queue_trypop(profile->send_queue, (void **)&msg) == SWITCH_STATUS_SUCCESS) {
		mod_amqp_util_msg_destroy(&msg);
	}

	if (pool) { switch_core_destroy_memory_pool(&pool); }

	*prof = NULL;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_amqp_xml_handler_create(char *name, switch_xml_t cfg)
{
	int arg = 0, i = 0;
	mod_amqp_xml_handler_profile_t *profile = NULL;
	switch_xml_t params, param, connections, connection;
	switch_threadattr_t *thd_attr = NULL;
	char *exchange = NULL, *exchange_type = NULL;
	char *bindings = NULL;
	int exchange_durable = 1; /* durable */
	switch_memory_pool_t *pool;
	char *format_fields[MAX_ROUTING_KEY_FORMAT_FIELDS + 1];
	int format_fields_size = 0;
	int max_temp_conn = 0;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) { goto err; }
	profile = switch_core_alloc(pool, sizeof(mod_amqp_xml_handler_profile_t));
	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, name);
	profile->running = 1;
	memset(profile->format_fields, 0, (MAX_ROUTING_KEY_FORMAT_FIELDS + 1) * sizeof(mod_amqp_keypart_t));
	// memset(profile->temp_conn, 0, (MAX_TEMP_CONNECTIONS) * sizeof(mod_amqp_temp_conn_t));
	profile->conn_root = NULL;
	profile->conn_active = NULL;
	profile->send_queue_size = 5000;
	profile->circuit_breaker_ms = 10000;
	switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	if ((params = switch_xml_child(cfg, "params")) != NULL) {
		for (param = switch_xml_child(params, "param"); param; param = param->next) {
			char *var = (char *)switch_xml_attr_soft(param, "name");
			char *val = (char *)switch_xml_attr_soft(param, "value");

			if (!var) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] param missing 'name' attribute\n",
								  profile->name);
				continue;
			}

			if (!val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
								  "Profile[%s] param[%s] missing 'value' attribute\n", profile->name, var);
				continue;
			}

			if (!strncmp(var, "reconnect_interval_ms", 21)) {
				int interval = atoi(val);
				if (interval && interval > 0) { profile->reconnect_interval_ms = interval; }
			} else if (!strncmp(var, "send_queue_size", 15)) {
				int interval = atoi(val);
				if (interval && interval > 0) { profile->send_queue_size = interval; }
			} else if (!strncmp(var, "exchange-type", 13)) {
				exchange_type = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "exchange-name", 13)) {
				exchange = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "exchange-durable", 16)) {
				exchange_durable = switch_true(val);
			} else if (!strncmp(var, "xml-handler-bindings", 20)) {
				bindings = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "max-temp-conn", 13)) {
				max_temp_conn = atoi(val);
			} else if (!strncmp(var, "format_fields", 13)) {
				char *tmp = switch_core_strdup(profile->pool, val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "amqp format fields : %s\n", tmp);
				if ((format_fields_size = mod_amqp_count_chars(tmp, ',')) >= MAX_ROUTING_KEY_FORMAT_FIELDS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "You can have only %d routing fields in the routing key.\n",
									  MAX_ROUTING_KEY_FORMAT_FIELDS);
					goto err;
				}

				/* increment size because the count returned the number of separators, not number of fields */
				format_fields_size++;
				switch_separate_string(tmp, ',', format_fields, MAX_ROUTING_KEY_FORMAT_FIELDS);
				format_fields[format_fields_size] = NULL;
			}
		} /* params for loop */
	}
	/* Handle defaults of string types */
	profile->bindings = bindings ? bindings : switch_core_strdup(profile->pool, "");
	profile->exchange = exchange ? exchange : switch_core_strdup(profile->pool, "TAP.XML_handler");
	profile->exchange_type = exchange_type ? exchange_type : switch_core_strdup(profile->pool, "topic");
	profile->exchange_durable = exchange_durable;
	profile->active_channels = 1;
	if (max_temp_conn && max_temp_conn > 0 && max_temp_conn < 1000) {
		profile->max_temp_conn = max_temp_conn;
	} else {
		profile->max_temp_conn = MAX_TEMP_CONNECTIONS;
	}

	for (i = 0; i < format_fields_size; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "amqp routing key %d : %s\n", i, format_fields[i]);
		if (profile->enable_fallback_format_fields) {
			profile->format_fields[i].size = switch_separate_string(
				format_fields[i], '|', profile->format_fields[i].name, MAX_ROUTING_KEY_FORMAT_FALLBACK_FIELDS);
			if (profile->format_fields[i].size > 1) {
				for (arg = 0; arg < profile->format_fields[i].size; arg++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "amqp routing key %d : sub key %d : %s\n", i,
									  arg, profile->format_fields[i].name[arg]);
				}
			}
		} else {
			profile->format_fields[i].name[0] = format_fields[i];
			profile->format_fields[i].size = 1;
		}
	}

	if ((connections = switch_xml_child(cfg, "connections")) != NULL) {
		for (connection = switch_xml_child(connections, "connection"); connection; connection = connection->next) {
			if (!profile->conn_root) { /* Handle first root node */
				if (mod_amqp_connection_create(&(profile->conn_root), connection, profile->pool) !=
					SWITCH_STATUS_SUCCESS) {
					/* Handle connection create failure */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_root;
			} else {
				if (mod_amqp_connection_create(&(profile->conn_active->next), connection, profile->pool) !=
					SWITCH_STATUS_SUCCESS) {
					/* Handle connection create failure */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_active->next;
			}
		}
	}
	profile->conn_active = NULL;
	/* We are not going to open the xml_handler queue connection on create, but instead wait for the running thread to
	 * open it */
	/* Create a bounded FIFO queue for sending messages */
	if (switch_queue_create(&(profile->send_queue), profile->send_queue_size, profile->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create send queue of size %d!\n",
						  profile->send_queue_size);
		goto err;
	}
	/* Start the event send thread. This will set up the initial connection */
	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&profile->xml_handler_thread, thd_attr, mod_amqp_xml_handler_thread, profile,
							 profile->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create 'amqp event sender' thread!\n");
		goto err;
	}
	if (switch_core_hash_insert(mod_amqp_globals.xml_handler_hash, name, (void *)profile) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "Failed to insert new profile [%s] into mod_amqp profile hash\n", name);
		goto err;
	}

	switch_xml_bind_search_function(xml_amqp_fetch, switch_xml_parse_section_string(profile->bindings), profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Profile[%s] Successfully started\n", profile->name);
	return SWITCH_STATUS_SUCCESS;

err:
	/* Cleanup */
	mod_amqp_xml_handler_destroy(&profile);
	return SWITCH_STATUS_GENERR;
}

/* This should only be called in a single threaded context from the xml_handler profile send thread */
switch_status_t mod_amqp_xml_handler_send(mod_amqp_xml_handler_profile_t *profile, mod_amqp_message_t *msg)
{
//	amqp_basic_properties_t props;
	int status;
	if (!profile->conn_active) {
		/* No connection, so we can not send the message. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] not active\n", profile->name);
		return SWITCH_STATUS_NOT_INITALIZED;
	}
//	memset(&props, 0, sizeof(amqp_basic_properties_t));
//	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
//	props.content_type = amqp_cstring_bytes("application/json");

	status = amqp_basic_publish(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
								amqp_cstring_bytes(msg->routing_key), 0, 0, &msg->props, amqp_cstring_bytes(msg->pjson));
	if (status < 0) {
		const char *errstr = amqp_error_string2(-status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
						  "Profile[%s] failed to send event on connection[%s]: %s\n", profile->name,
						  profile->conn_active->name, errstr);

		/* This is bad, we couldn't send the message. Clear up any connection */
		mod_amqp_connection_close(profile->conn_active);
		profile->conn_active = NULL;
		return SWITCH_STATUS_SOCKERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

void *SWITCH_THREAD_FUNC mod_amqp_xml_handler_thread(switch_thread_t *thread, void *data)
{
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_xml_handler_profile_t *profile = (mod_amqp_xml_handler_profile_t *)data;
	mod_amqp_aux_connection_t *conn_aux = NULL, *conn_next = NULL;
	amqp_boolean_t passive = 0;
	amqp_boolean_t durable = 1;
	// init first temp connection (for outgoing msgs)
	if (!profile->conn_aux) {
		profile->conn_aux = switch_core_alloc(profile->pool, sizeof(mod_amqp_aux_connection_t));
	}
	while (profile->running) {
		if (!profile->conn_active) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Amqp no connection- reconnecting...\n");

			status = mod_amqp_connection_open(profile->conn_root, &(profile->conn_active), profile->name,
											  profile->custom_attr);
			if (status == SWITCH_STATUS_SUCCESS) {
				switch_mutex_lock(profile->mutex);
				for (conn_aux = profile->conn_aux; conn_aux; conn_aux = conn_next) {
					mod_amqp_aux_connection_open(profile->conn_active, &(conn_aux), profile->name, profile->custom_attr);
					conn_next = conn_aux->next;
				}
				switch_mutex_unlock(profile->mutex);
				// Ensure that the exchange exists, and is of the correct type
#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR >= 6
				amqp_exchange_declare(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
									  amqp_cstring_bytes(profile->exchange_type), passive, durable,
									  profile->exchange_auto_delete, 0, amqp_empty_table);
#else
				amqp_exchange_declare(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
									  amqp_cstring_bytes(profile->exchange_type), passive, durable, amqp_empty_table);
#endif

				if (!mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state),
												"Declaring exchange")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Amqp reconnect successful- connected\n");
					continue;
				}
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Profile[%s] failed to connect with code(%d), sleeping for %dms\n", profile->name, status,
							  profile->reconnect_interval_ms);
			switch_sleep(profile->reconnect_interval_ms * 1000);
			continue;
		}

		if (!msg && switch_queue_pop_timeout(profile->send_queue, (void **)&msg, 1000000) != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		if (msg) {
			switch (mod_amqp_xml_handler_send(profile, msg)) {
			case SWITCH_STATUS_SUCCESS:
				/* Success: prepare for next message */
				mod_amqp_util_msg_destroy(&msg);
				break;

			case SWITCH_STATUS_NOT_INITALIZED:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed with 'not initialised'\n");
				break;

			case SWITCH_STATUS_SOCKERR:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed with 'socket error'\n");
				break;

			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed with a generic error\n");

				/* Send failed and closed the connection; reconnect will happen at the beginning of the loop
				 * NB: do we need a delay here to prevent a fast reconnect-send-fail loop? */
				break;
			}
		}
	}

	/* Abort the current message */
	mod_amqp_util_msg_destroy(&msg);

	// Terminate the thread
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "XML handler sender thread has been stopped\n");
	switch_thread_exit(thread, SWITCH_STATUS_SUCCESS);
	return NULL;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */
