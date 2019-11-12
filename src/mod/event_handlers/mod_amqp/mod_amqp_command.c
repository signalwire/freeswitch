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

switch_status_t mod_amqp_command_destroy(mod_amqp_command_profile_t **prof)
{
	switch_status_t ret;
	mod_amqp_connection_t *conn = NULL, *conn_next = NULL;
	switch_memory_pool_t *pool;
	mod_amqp_command_profile_t *profile;

	if (!prof || !*prof) {
		return SWITCH_STATUS_SUCCESS;
	}

	profile = *prof;
	pool = profile->pool;

	if (profile->name) {
		switch_core_hash_delete(mod_amqp_globals.command_hash, profile->name);
	}

	profile->running = 0;

	if (profile->command_thread) {
		switch_thread_join(&ret, profile->command_thread);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] closing AMQP socket...\n", profile->name);

	for (conn = profile->conn_root; conn; conn = conn_next) {
		mod_amqp_connection_destroy(&conn);
	}

	profile->conn_active = NULL;
	profile->conn_root = NULL;

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	*prof = NULL;

	return SWITCH_STATUS_SUCCESS;
}


static char *mod_amqp_expand_header(switch_memory_pool_t *pool, switch_event_t *event, char *val)
{
        char *expanded;
        char *dup = NULL;

        expanded = switch_event_expand_headers(event, val);
        dup = switch_core_strdup(pool, expanded);

        if (expanded != val) {
                free(expanded);
        }

        return dup;
}


switch_status_t mod_amqp_command_create(char *name, switch_xml_t cfg)
{
	mod_amqp_command_profile_t *profile = NULL;
	switch_xml_t params, param, connections, connection;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	switch_event_t *event;
	char *exchange = NULL, *binding_key = NULL, *queue = NULL;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		goto err;
	}

	profile = switch_core_alloc(pool, sizeof(mod_amqp_command_profile_t));

	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, name);
	profile->running = 1;
	profile->reconnect_interval_ms = 1000;

        if (switch_event_create(&event, SWITCH_EVENT_GENERAL) != SWITCH_STATUS_SUCCESS) {
                goto err;
	}

	/* Default queue properties, set to match formerly hardcoded values */
	profile->passive = SWITCH_FALSE;
	profile->durable = SWITCH_FALSE;
	profile->exclusive = SWITCH_FALSE;
	profile->auto_delete = SWITCH_TRUE;

	if ((params = switch_xml_child(cfg, "params")) != NULL) {
		for (param = switch_xml_child(params, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!var) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] param missing 'name' attribute\n", profile->name);
				continue;
			}

			if (!val) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] param[%s] missing 'value' attribute\n", profile->name, var);
				continue;
			}

			if (!strncmp(var, "reconnect_interval_ms", 21)) {
				int interval = atoi(val);
				if ( interval && interval > 0 ) {
					profile->reconnect_interval_ms = interval;
				}
			} else if (!strncmp(var, "exchange-name", 13)) {
				exchange = mod_amqp_expand_header(profile->pool, event, val);
			} else if (!strncmp(var, "queue-name", 10)) {
				queue = mod_amqp_expand_header(profile->pool, event, val);
			} else if (!strncmp(var, "binding_key", 11)) {
				binding_key = mod_amqp_expand_header(profile->pool, event, val);
			} else if (!strncmp(var, "queue-passive", 13)) {
				profile->passive = switch_true(val);
			} else if (!strncmp(var, "queue-durable", 13)) {
				profile->durable = switch_true(val);
			} else if (!strncmp(var, "queue-exclusive", 15)) {
				profile->exclusive = switch_true(val);
			} else if (!strncmp(var, "queue-auto-delete", 17)) {
				profile->auto_delete = switch_true(val);
			}
		}
	}

        switch_event_destroy(&event);

	/* Handle defaults of string types */
	profile->exchange = exchange ? exchange : switch_core_strdup(profile->pool, "TAP.Commands");
	profile->queue = queue ? queue : NULL;
	profile->binding_key = binding_key ? binding_key : switch_core_strdup(profile->pool, "commandBindingKey");

	if ((connections = switch_xml_child(cfg, "connections")) != NULL) {
		for (connection = switch_xml_child(connections, "connection"); connection; connection = connection->next) {
			if ( ! profile->conn_root ) { /* Handle first root node */
				if (mod_amqp_connection_create(&(profile->conn_root), connection, profile->pool) != SWITCH_STATUS_SUCCESS) {
					/* Handle connection create failure */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_root;
			} else {
				if (mod_amqp_connection_create(&(profile->conn_active->next), connection, profile->pool) != SWITCH_STATUS_SUCCESS) {
					/* Handle connection create failure */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to create connection\n", profile->name);
					continue;
				}
				profile->conn_active = profile->conn_active->next;
			}
		}
	}
	profile->conn_active = NULL;
	/* We are not going to open the command queue connection on create, but instead wait for the running thread to open it */

	/* Start the worker threads */
	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	if (switch_thread_create(&profile->command_thread, thd_attr, mod_amqp_command_thread, profile, profile->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create 'amqp event sender' thread!\n");
		goto err;
	}

	if ( switch_core_hash_insert(mod_amqp_globals.command_hash, name, (void *) profile) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to insert new profile [%s] into mod_amqp profile hash\n", name);
		goto err;
	}

	return SWITCH_STATUS_SUCCESS;

 err:
	/* Cleanup */
	mod_amqp_command_destroy(&profile);
	return SWITCH_STATUS_GENERR;
}

static void mod_amqp_command_response(mod_amqp_command_profile_t *profile, char *command, switch_stream_handle_t stream,
									  char *fs_resp_exchange, char *fs_resp_key, switch_status_t status)
{
	char *json_output = NULL;
	amqp_basic_properties_t props;
	cJSON *message = NULL;
	int amqp_status = AMQP_STATUS_OK;

	if (! profile->conn_active) {
		/* No connection, so we can not send the message. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] not active\n", profile->name);
		return;
	}

	/* Construct the api response */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Preparing api command response: [%s]\n", (char *)stream.data);
	message = cJSON_CreateObject();

	cJSON_AddItemToObject(message, "output", cJSON_CreateString((const char *) stream.data));
	cJSON_AddItemToObject(message, "command", cJSON_CreateString(command));
	cJSON_AddItemToObject(message, "status", cJSON_CreateNumber((double) status));

	json_output = cJSON_Print(message);
	cJSON_Delete(message);

	memset(&props, 0, sizeof(amqp_basic_properties_t));

	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
	props.content_type = amqp_cstring_bytes("text/json");

	amqp_status = amqp_basic_publish(
								profile->conn_active->state,
								1,
								amqp_cstring_bytes(fs_resp_exchange),
								amqp_cstring_bytes(fs_resp_key),
								0,
								0,
								&props,
								amqp_cstring_bytes(json_output));

	switch_safe_free(json_output);

	if (amqp_status != AMQP_STATUS_OK) {
		const char *errstr = amqp_error_string2(-amqp_status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] failed to send event on connection[%s]: %s\n",
						  profile->name, profile->conn_active->name, errstr);

		/* This is bad, we couldn't send the message. Clear up any connection */
		mod_amqp_connection_close(profile->conn_active);
		profile->conn_active = NULL;
		return;
	}

	return;
}

void * SWITCH_THREAD_FUNC mod_amqp_command_thread(switch_thread_t *thread, void *data)
{
	mod_amqp_command_profile_t *profile = (mod_amqp_command_profile_t *) data;

	while (profile->running) {
		amqp_queue_declare_ok_t *recv_queue;
		amqp_bytes_t queueName = { 0, NULL };

		/* Ensure we have an AMQP connection */
		if (!profile->conn_active) {
			switch_status_t status;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Amqp no connection- reconnecting...\n");

			status = mod_amqp_connection_open(profile->conn_root, &(profile->conn_active), profile->name, profile->custom_attr);
			if ( status	!= SWITCH_STATUS_SUCCESS ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to connect with code(%d), sleeping for %dms\n",
								  profile->name, status, profile->reconnect_interval_ms);
				switch_sleep(profile->reconnect_interval_ms * 1000);
				continue;
			}

			/* Check if exchange already exists */
#if AMQP_VERSION_MAJOR == 0 && AMQP_VERSION_MINOR >= 6
 			amqp_exchange_declare(profile->conn_active->state, 1,
								  amqp_cstring_bytes(profile->exchange),
								  amqp_cstring_bytes("topic"),
								  0, /* passive */
								  1, /* durable */
								  0, /* auto-delete */
								  0,
								  amqp_empty_table);
#else
			amqp_exchange_declare(profile->conn_active->state, 1,
								  amqp_cstring_bytes(profile->exchange),
								  amqp_cstring_bytes("topic"),
								  0, /* passive */
								  1, /* durable */
								  amqp_empty_table);
#endif

			if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Checking for command exchange\n")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to create missing command exchange\n", profile->name);
				continue;
			}

			/* Ensure we have a queue */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Creating command queue\n");
			recv_queue = amqp_queue_declare(profile->conn_active->state, // state
											1,                           // channel
											profile->queue ? amqp_cstring_bytes(profile->queue) : amqp_empty_bytes, // queue name
											profile->passive,
											profile->durable,
											profile->exclusive,
											profile->auto_delete,
											amqp_empty_table);           // args

			if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Declaring queue\n")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to connect with code(%d), sleeping for %dms\n",
								  profile->name, status, profile->reconnect_interval_ms);
				switch_sleep(profile->reconnect_interval_ms * 1000);
				continue;
			}

			if (queueName.bytes) {
				amqp_bytes_free(queueName);
			}

			queueName = amqp_bytes_malloc_dup(recv_queue->queue);

			if (!queueName.bytes) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory while copying queue name");
				break;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Created command queue %.*s\n", (int)queueName.len, (char *)queueName.bytes);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Binding command queue to exchange %s\n", profile->exchange);

			/* Bind the queue to the exchange */
			amqp_queue_bind(profile->conn_active->state,                   // state
							1,                                             // channel
							queueName,                                     // queue
							amqp_cstring_bytes(profile->exchange),         // exchange
							amqp_cstring_bytes(profile->binding_key),      // routing key
							amqp_empty_table);                             // args

			if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Binding queue")) {
				mod_amqp_connection_close(profile->conn_active);
				profile->conn_active = NULL;
				switch_sleep(profile->reconnect_interval_ms * 1000);
				continue;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Amqp reconnect successful- connected\n");
			continue;
		}

		// Start a command
		amqp_basic_consume(profile->conn_active->state,     // state
						   1,                               // channel
						   queueName,                       // queue
						   amqp_empty_bytes,                // command tag
						   0, 1, 0,                         // no_local, no_ack, exclusive
						   amqp_empty_table);               // args

		if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Creating a command")) {
			mod_amqp_connection_close(profile->conn_active);
			profile->conn_active = NULL;
			switch_sleep(profile->reconnect_interval_ms * 1000);
			continue;
		}

		while (profile->running && profile->conn_active) {
			amqp_rpc_reply_t res;
			amqp_envelope_t envelope;
			struct timeval timeout = {0};
			char command[1024];
			enum ECommandFormat {
				COMMAND_FORMAT_UNKNOWN,
				COMMAND_FORMAT_PLAINTEXT
			} commandFormat = COMMAND_FORMAT_PLAINTEXT;
			char *fs_resp_exchange = NULL, *fs_resp_key = NULL;

			amqp_maybe_release_buffers(profile->conn_active->state);

			timeout.tv_usec = 500 * 1000;
			res = amqp_consume_message(profile->conn_active->state, &envelope, &timeout, 0);

			if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
				if (res.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
					/* Unexpected frame. Discard it then continue */
					amqp_frame_t decoded_frame;
					amqp_simple_wait_frame(profile->conn_active->state, &decoded_frame);
				}

				if (res.library_error == AMQP_STATUS_SOCKET_ERROR) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "A socket error occurred. Tearing down and reconnecting\n");
					break;
				}

				if (res.library_error == AMQP_STATUS_CONNECTION_CLOSED) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMQP connection was closed. Tearing down and reconnecting\n");
					break;
				}

				if (res.library_error == AMQP_STATUS_TCP_ERROR) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "A TCP error occurred. Tearing down and reconnecting\n");
					break;
				}

				if (res.library_error == AMQP_STATUS_TIMEOUT) {
					// nop
				}

				/* Try consuming again */
				continue;
			}

			if (res.reply_type != AMQP_RESPONSE_NORMAL) {
				break;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Delivery:%u, exchange:%.*s routingkey:%.*s\n",
							  (unsigned) envelope.delivery_tag, (int) envelope.exchange.len, (char *) envelope.exchange.bytes,
							  (int) envelope.routing_key.len, (char *) envelope.routing_key.bytes);

			if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Content-type: %.*s\n",
								  (int) envelope.message.properties.content_type.len, (char *) envelope.message.properties.content_type.bytes);

				if (strncasecmp("text/plain", envelope.message.properties.content_type.bytes, strlen("text/plain")) == 0) {
					commandFormat = COMMAND_FORMAT_PLAINTEXT;
				} else {
					commandFormat = COMMAND_FORMAT_UNKNOWN;
				}
			}

			if (envelope.message.properties.headers.num_entries) {
				int x = 0;

				for ( x = 0; x < envelope.message.properties.headers.num_entries; x++) {
					char *header_key = (char *)envelope.message.properties.headers.entries[x].key.bytes;
					char *header_value = (char *)envelope.message.properties.headers.entries[x].value.value.bytes.bytes;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "AMQP message custom header key[%s] value[%s]\n", header_key, header_value);

					if ( !strncmp(header_key, "x-fs-api-resp-exchange", 22)) {
						fs_resp_exchange = header_value;
					} else if (!strncmp(header_key, "x-fs-api-resp-key", 17)) {
						fs_resp_key = header_value;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ignoring unrecognized event header [%s]\n", header_key);
					}
				}
			}

			if (commandFormat == COMMAND_FORMAT_PLAINTEXT) {
				switch_stream_handle_t stream = { 0 }; /* Collects the command output */

				/* Convert amqp bytes to c-string */
				snprintf(command, sizeof(command), "%.*s", (int) envelope.message.body.len, (char *) envelope.message.body.bytes);

				/* Execute the command */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Executing: %s\n", command);

				SWITCH_STANDARD_STREAM(stream);

				if ( fs_resp_exchange && fs_resp_key ) {
					switch_status_t status = switch_console_execute(command, 0, &stream);
					mod_amqp_command_response(profile, command, stream, fs_resp_exchange, fs_resp_key, status);
				} else {
					if (switch_console_execute(command, 0, &stream) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Remote command failed:\n%s\n", (char *) stream.data);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Remote command succeeded:\n%s\n", (char *) stream.data);
					}
				}
				switch_safe_free(stream.data);
			}

			/* Tidy up */
			amqp_destroy_envelope(&envelope);
		}

		amqp_bytes_free(queueName);
		queueName.bytes = NULL;

		mod_amqp_connection_close(profile->conn_active);
		profile->conn_active = NULL;

		if (profile->running) {
			/* We'll reconnect, but sleep to avoid hammering resources */
			switch_sleep(500);
		}
	}

	/* Terminate the thread */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Command listener thread stopped\n");
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
