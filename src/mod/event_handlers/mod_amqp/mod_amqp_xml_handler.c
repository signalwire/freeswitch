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
	mod_amqp_message_t *amqp_message = NULL;
	switch_queue_t *response_queue = NULL;
	switch_memory_pool_t *req_pool = NULL;
	char *xml_str = NULL;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	mod_amqp_xml_handler_profile_t *profile = (mod_amqp_xml_handler_profile_t *)user_data;
	switch_bool_t params_local = SWITCH_FALSE;
	switch_bool_t registered = SWITCH_FALSE;

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML fetch called without profile\n");
		return NULL;
	}

	if (switch_time_now() < profile->circuit_breaker_reset_time) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						  "Profile[%s] circuit breaker active\n", profile->name);
		return NULL;
	}

	if (!profile->running || !profile->conn_active || !profile->recv_ready) {
		return NULL;
	}

	if (!params) {
		switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(params);
		params_local = SWITCH_TRUE;
	}

	switch_event_add_header_string(params, SWITCH_STACK_TOP, "section",   switch_str_nil(section));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "tag_name",  switch_str_nil(tag_name));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_name",  switch_str_nil(key_name));
	switch_event_add_header_string(params, SWITCH_STACK_TOP, "key_value", switch_str_nil(key_value));

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	switch_core_new_memory_pool(&req_pool);
	switch_queue_create(&response_queue, 1, req_pool);

	switch_malloc(amqp_message, sizeof(mod_amqp_message_t));
	mod_amqp_xml_handler_routing_key(profile, amqp_message->routing_key, params, profile->format_fields);
	memcpy(amqp_message->correlation_id, uuid_str, SWITCH_UUID_FORMATTED_LENGTH);
	amqp_message->correlation_id[SWITCH_UUID_FORMATTED_LENGTH] = '\0';
	switch_event_serialize_json(params, &amqp_message->pjson);

	/* Register BEFORE push to avoid race where reader gets response before we register */
	switch_mutex_lock(profile->response_hash_mutex);
	switch_core_hash_insert(profile->response_hash, uuid_str, response_queue);
	switch_mutex_unlock(profile->response_hash_mutex);
	registered = SWITCH_TRUE;

	if (switch_queue_trypush(profile->send_queue, amqp_message) != SWITCH_STATUS_SUCCESS) {
		unsigned int queue_size = switch_queue_size(profile->send_queue);
		profile->circuit_breaker_reset_time = switch_time_now() + profile->circuit_breaker_ms * 1000;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "AMQP message queue full. Messages will be dropped for %.1fs! (Queue capacity %d)",
						  profile->circuit_breaker_ms / 1000.0, queue_size);
		mod_amqp_util_msg_destroy(&amqp_message);
		goto done;
	}

	if (switch_queue_pop_timeout(response_queue, (void **)&xml_str, (switch_interval_time_t)profile->fetch_timeout_ms * 1000) == SWITCH_STATUS_SUCCESS && xml_str) {
		if (!(xml = switch_xml_parse_str_dynamic(xml_str, SWITCH_FALSE))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Profile[%s] failed to parse XML response\n", profile->name);
			free(xml_str);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						  "Profile[%s] XML fetch timed out for request %s\n", profile->name, uuid_str);
	}

done:
	if (registered) {
		switch_mutex_lock(profile->response_hash_mutex);
		switch_core_hash_delete(profile->response_hash, uuid_str);
		switch_mutex_unlock(profile->response_hash_mutex);
	}

	switch_core_destroy_memory_pool(&req_pool);

	if (params_local) { switch_event_destroy(&params); }

	return xml;
}

switch_status_t mod_amqp_xml_handler_destroy(mod_amqp_xml_handler_profile_t **prof)
{
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_connection_t *conn = NULL, *conn_next = NULL;
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
	if (profile->reader_thread)      { switch_thread_join(&status, profile->reader_thread); }

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] closing connections...\n", profile->name);

	for (conn = profile->conn_root; conn; conn = conn_next) {
		conn_next = conn->next;
		mod_amqp_connection_destroy(&conn);
	}

	if (profile->recv_conn) { mod_amqp_connection_close(profile->recv_conn); }

	profile->recv_conn   = NULL;
	profile->conn_active = NULL;
	profile->conn_root   = NULL;

	while (profile->send_queue && switch_queue_trypop(profile->send_queue, (void **)&msg) == SWITCH_STATUS_SUCCESS) {
		mod_amqp_util_msg_destroy(&msg);
	}

	if (profile->response_hash) { switch_core_hash_destroy(&profile->response_hash); }

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
	int exchange_durable = 1;
	switch_memory_pool_t *pool;
	char *format_fields[MAX_ROUTING_KEY_FORMAT_FIELDS + 1];
	int format_fields_size = 0;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) { goto err; }
	profile = switch_core_alloc(pool, sizeof(mod_amqp_xml_handler_profile_t));
	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, name);
	profile->running = 1;
	memset(profile->format_fields, 0, (MAX_ROUTING_KEY_FORMAT_FIELDS + 1) * sizeof(mod_amqp_keypart_t));
	profile->conn_root   = NULL;
	profile->conn_active = NULL;
	profile->recv_conn   = NULL;
	profile->recv_ready  = SWITCH_FALSE;
	profile->send_queue_size    = 5000;
	profile->fetch_timeout_ms   = 5000;
	profile->circuit_breaker_ms = 10000;

	switch_core_hash_init(&profile->response_hash);
	switch_mutex_init(&profile->response_hash_mutex, SWITCH_MUTEX_NESTED, profile->pool);

	if ((params = switch_xml_child(cfg, "params")) != NULL) {
		for (param = switch_xml_child(params, "param"); param; param = param->next) {
			char *var = (char *)switch_xml_attr_soft(param, "name");
			char *val = (char *)switch_xml_attr_soft(param, "value");

			if (!var) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
								  "Profile[%s] param missing 'name' attribute\n", profile->name);
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
			} else if (!strncmp(var, "fetch_timeout_ms", 16)) {
				int interval = atoi(val);
				if (interval && interval > 0) { profile->fetch_timeout_ms = interval; }
			} else if (!strncmp(var, "exchange-type", 13)) {
				exchange_type = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "exchange-name", 13)) {
				exchange = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "exchange-durable", 16)) {
				exchange_durable = switch_true(val);
			} else if (!strncmp(var, "xml-handler-bindings", 20)) {
				bindings = switch_core_strdup(profile->pool, val);
			} else if (!strncmp(var, "format_fields", 13)) {
				char *tmp = switch_core_strdup(profile->pool, val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "amqp format fields : %s\n", tmp);
				if ((format_fields_size = mod_amqp_count_chars(tmp, ',')) >= MAX_ROUTING_KEY_FORMAT_FIELDS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "You can have only %d routing fields in the routing key.\n",
									  MAX_ROUTING_KEY_FORMAT_FIELDS);
					goto err;
				}
				format_fields_size++;
				switch_separate_string(tmp, ',', format_fields, MAX_ROUTING_KEY_FORMAT_FIELDS);
				format_fields[format_fields_size] = NULL;
			}
		}
	}

	profile->bindings = bindings ? bindings : switch_core_strdup(profile->pool, "all");
	profile->exchange  = exchange  ? exchange  : switch_core_strdup(profile->pool, "TAP.XML_handler");
	profile->exchange_type = exchange_type ? exchange_type : switch_core_strdup(profile->pool, "topic");
	profile->exchange_durable = exchange_durable;

	for (i = 0; i < format_fields_size; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "amqp routing key %d : %s\n", i, format_fields[i]);
		if (profile->enable_fallback_format_fields) {
			profile->format_fields[i].size = switch_separate_string(
				format_fields[i], '|', profile->format_fields[i].name, MAX_ROUTING_KEY_FORMAT_FALLBACK_FIELDS);
			if (profile->format_fields[i].size > 1) {
				for (arg = 0; arg < profile->format_fields[i].size; arg++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
									  "amqp routing key %d : sub key %d : %s\n", i, arg,
									  profile->format_fields[i].name[arg]);
				}
			}
		} else {
			profile->format_fields[i].name[0] = format_fields[i];
			profile->format_fields[i].size = 1;
		}
	}

	if ((connections = switch_xml_child(cfg, "connections")) != NULL) {
		for (connection = switch_xml_child(connections, "connection"); connection; connection = connection->next) {
			if (!profile->conn_root) {
				if (mod_amqp_connection_create(&(profile->conn_root), connection, profile->pool) !=
					SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_root;
			} else {
				if (mod_amqp_connection_create(&(profile->conn_active->next), connection, profile->pool) !=
					SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_active->next;
			}
		}
	}
	profile->conn_active = NULL;

	/* Pre-allocate recv_conn struct from pool (reused across reconnects) */
	profile->recv_conn = switch_core_alloc(profile->pool, sizeof(mod_amqp_connection_t));

	if (switch_queue_create(&(profile->send_queue), profile->send_queue_size, profile->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create send queue of size %d!\n",
						  profile->send_queue_size);
		goto err;
	}

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&profile->xml_handler_thread, thd_attr, mod_amqp_xml_handler_thread, profile,
							 profile->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create sender thread!\n");
		goto err;
	}

	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&profile->reader_thread, thd_attr, mod_amqp_xml_handler_reader_thread, profile,
							 profile->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create reader thread!\n");
		goto err;
	}

	if (switch_core_hash_insert(mod_amqp_globals.xml_handler_hash, name, (void *)profile) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "Failed to insert new profile [%s] into mod_amqp profile hash\n", name);
		goto err;
	}

	switch_xml_bind_search_function(xml_amqp_fetch, switch_xml_parse_section_string(profile->bindings), profile);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Profile[%s] successfully started\n", profile->name);
	return SWITCH_STATUS_SUCCESS;

err:
	mod_amqp_xml_handler_destroy(&profile);
	return SWITCH_STATUS_GENERR;
}

/* Called only from xml_handler_thread (sender) */
switch_status_t mod_amqp_xml_handler_send(mod_amqp_xml_handler_profile_t *profile, mod_amqp_message_t *msg)
{
	amqp_basic_properties_t props;
	int status;
	char reply_to[256];

	if (!profile->conn_active) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] sender not active\n", profile->name);
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	if (!profile->recv_ready) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
						  "Profile[%s] recv connection not ready\n", profile->name);
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	/* Take a local copy so reader_thread reconnect doesn't race with amqp_basic_publish */
	strncpy(reply_to, profile->recv_queue_name, sizeof(reply_to) - 1);
	reply_to[sizeof(reply_to) - 1] = '\0';

	memset(&props, 0, sizeof(amqp_basic_properties_t));
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_REPLY_TO_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG;
	props.content_type   = amqp_cstring_bytes("application/json");
	props.reply_to       = amqp_cstring_bytes(reply_to);
	props.correlation_id = amqp_cstring_bytes(msg->correlation_id);

	status = amqp_basic_publish(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
								amqp_cstring_bytes(msg->routing_key), 0, 0, &props,
								amqp_cstring_bytes(msg->pjson));
	if (status < 0) {
		const char *errstr = amqp_error_string2(-status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
						  "Profile[%s] failed to send on connection[%s]: %s\n",
						  profile->name, profile->conn_active->name, errstr);
		mod_amqp_connection_close(profile->conn_active);
		profile->conn_active = NULL;
		return SWITCH_STATUS_SOCKERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

/* reader_thread: owns recv_conn, dispatches responses to waiting xml_amqp_fetch callers */
void *SWITCH_THREAD_FUNC mod_amqp_xml_handler_reader_thread(switch_thread_t *thread, void *data)
{
	mod_amqp_xml_handler_profile_t *profile = (mod_amqp_xml_handler_profile_t *)data;
	amqp_rpc_reply_t res;
	amqp_envelope_t envelope;
	struct timeval tv;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Profile[%s] reader thread started\n", profile->name);

	while (profile->running) {
		if (!profile->recv_conn->state) {
			mod_amqp_connection_t *candidate;
			mod_amqp_connection_t *new_recv = NULL;
			amqp_queue_declare_ok_t *recv_queue;

			profile->recv_ready = SWITCH_FALSE;

			/* Try each candidate: copy its params into the standalone recv_conn, then open */
			for (candidate = profile->conn_root; candidate; candidate = candidate->next) {
				profile->recv_conn->hostname        = candidate->hostname;
				profile->recv_conn->virtualhost     = candidate->virtualhost;
				profile->recv_conn->username        = candidate->username;
				profile->recv_conn->password        = candidate->password;
				profile->recv_conn->port            = candidate->port;
				profile->recv_conn->heartbeat       = candidate->heartbeat;
				profile->recv_conn->ssl_on          = candidate->ssl_on;
				profile->recv_conn->ssl_verify_peer = candidate->ssl_verify_peer;
				profile->recv_conn->next            = NULL;

				if (mod_amqp_connection_open(profile->recv_conn, &new_recv,
											 profile->name, profile->custom_attr) == SWITCH_STATUS_SUCCESS) {
					break;
				}
				/* mod_amqp_connection_open sets new_recv = NULL on failure; recv_conn itself is intact */
			}

			if (!new_recv) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "Profile[%s] reader: failed to connect, sleeping %dms\n",
								  profile->name, profile->reconnect_interval_ms);
				switch_sleep(profile->reconnect_interval_ms * 1000);
				continue;
			}

			/* Declare a broker-assigned exclusive auto-delete queue */
			recv_queue = amqp_queue_declare(profile->recv_conn->state, 1, amqp_empty_bytes,
											0, 0, 1, 1, amqp_empty_table);
			if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->recv_conn->state), "Declaring recv queue")) {
				mod_amqp_connection_close(profile->recv_conn);
				continue;
			}

			amqp_basic_consume(profile->recv_conn->state, 1, recv_queue->queue,
							   amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
			if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->recv_conn->state), "Basic consume")) {
				mod_amqp_connection_close(profile->recv_conn);
				continue;
			}

			/* Write queue name before setting recv_ready so sender sees a complete string */
			{
				amqp_bytes_t q = recv_queue->queue;
				size_t n = q.len < sizeof(profile->recv_queue_name) - 1
						   ? q.len : sizeof(profile->recv_queue_name) - 1;
				memcpy(profile->recv_queue_name, q.bytes, n);
				profile->recv_queue_name[n] = '\0';
			}
			profile->recv_ready = SWITCH_TRUE;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
							  "Profile[%s] reader: connected, queue[%s]\n",
							  profile->name, profile->recv_queue_name);
			continue;
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		amqp_maybe_release_buffers(profile->recv_conn->state);
		res = amqp_consume_message(profile->recv_conn->state, &envelope, &tv, 0);

		if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
			if (res.library_error == AMQP_STATUS_TIMEOUT) { continue; }

			if (res.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
				amqp_frame_t frame;
				amqp_simple_wait_frame(profile->recv_conn->state, &frame);
				continue;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Profile[%s] reader: AMQP error %s, reconnecting\n",
							  profile->name, amqp_error_string2(res.library_error));
			profile->recv_ready = SWITCH_FALSE;
			mod_amqp_connection_close(profile->recv_conn);
			continue;
		}

		if (res.reply_type != AMQP_RESPONSE_NORMAL) { continue; }

		{
			amqp_bytes_t corr = envelope.message.properties.correlation_id;
			if (corr.len > 0 && corr.len < SWITCH_UUID_FORMATTED_LENGTH) {
				char corr_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
				switch_queue_t *resp_queue = NULL;

				memcpy(corr_str, corr.bytes, corr.len);
				corr_str[corr.len] = '\0';

				switch_mutex_lock(profile->response_hash_mutex);
				resp_queue = switch_core_hash_find(profile->response_hash, corr_str);

				if (resp_queue) {
					char *xml_str = malloc(envelope.message.body.len + 1);
					if (xml_str) {
						memcpy(xml_str, envelope.message.body.bytes, envelope.message.body.len);
						xml_str[envelope.message.body.len] = '\0';
						if (switch_queue_trypush(resp_queue, xml_str) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
											  "Profile[%s] reader: no waiter for %s (timed out?)\n",
											  profile->name, corr_str);
							free(xml_str);
						}
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "Profile[%s] reader: no waiter for %s\n", profile->name, corr_str);
				}
				switch_mutex_unlock(profile->response_hash_mutex);
			}
		}

		amqp_destroy_envelope(&envelope);
	}

	profile->recv_ready = SWITCH_FALSE;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Profile[%s] reader thread stopped\n", profile->name);
	switch_thread_exit(thread, SWITCH_STATUS_SUCCESS);
	return NULL;
}

/* xml_handler_thread: sender only */
void *SWITCH_THREAD_FUNC mod_amqp_xml_handler_thread(switch_thread_t *thread, void *data)
{
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_xml_handler_profile_t *profile = (mod_amqp_xml_handler_profile_t *)data;
	amqp_boolean_t passive = 0;
	amqp_boolean_t durable = 1;

	while (profile->running) {
		if (!profile->conn_active) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Profile[%s] sender: no connection, reconnecting...\n", profile->name);

			status = mod_amqp_connection_open(profile->conn_root, &(profile->conn_active),
											  profile->name, profile->custom_attr);
			if (status == SWITCH_STATUS_SUCCESS) {
#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR >= 6
				amqp_exchange_declare(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
									  amqp_cstring_bytes(profile->exchange_type), passive, durable,
									  profile->exchange_auto_delete, 0, amqp_empty_table);
#else
				amqp_exchange_declare(profile->conn_active->state, 1, amqp_cstring_bytes(profile->exchange),
									  amqp_cstring_bytes(profile->exchange_type), passive, durable, amqp_empty_table);
#endif
				if (!mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Declaring exchange")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
									  "Profile[%s] sender: connected\n", profile->name);
					continue;
				}
				mod_amqp_connection_close(profile->conn_active);
				profile->conn_active = NULL;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Profile[%s] sender: failed to connect (code %d), sleeping %dms\n",
							  profile->name, status, profile->reconnect_interval_ms);
			switch_sleep(profile->reconnect_interval_ms * 1000);
			continue;
		}

		if (!msg && switch_queue_pop_timeout(profile->send_queue, (void **)&msg, 1000000) != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		if (msg) {
			switch (mod_amqp_xml_handler_send(profile, msg)) {
			case SWITCH_STATUS_SUCCESS:
				mod_amqp_util_msg_destroy(&msg);
				break;
			case SWITCH_STATUS_NOT_INITALIZED:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed: not initialised\n");
				break;
			case SWITCH_STATUS_SOCKERR:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed: socket error\n");
				break;
			default:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send failed: generic error\n");
				break;
			}
		}
	}

	mod_amqp_util_msg_destroy(&msg);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Profile[%s] sender thread stopped\n", profile->name);
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
