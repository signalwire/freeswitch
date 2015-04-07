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

void mod_amqp_producer_msg_destroy(mod_amqp_message_t **msg)
{
	if (!msg || !*msg) return;
	switch_safe_free((*msg)->pjson);
	switch_safe_free(*msg);
}

switch_status_t mod_amqp_producer_routing_key(char routingKey[MAX_AMQP_ROUTING_KEY_LENGTH], switch_event_t* evt, char* routingKeyEventHeaderNames[])
{
	int i = 0, idx = 0;

	for (i = 0; i < MAX_ROUTING_KEY_FORMAT_FIELDS && idx < MAX_AMQP_ROUTING_KEY_LENGTH; i++) {
		if (routingKeyEventHeaderNames[i]) {
			if (idx) {
				routingKey[idx++] = '.';
			}
			if (routingKeyEventHeaderNames[i][0] == '#') {
				strncpy(routingKey + idx, routingKeyEventHeaderNames[i] + 1, MAX_AMQP_ROUTING_KEY_LENGTH - idx);
			} else {
				char *value = switch_event_get_header(evt, routingKeyEventHeaderNames[i]);
				strncpy(routingKey + idx, value ? value : "", MAX_AMQP_ROUTING_KEY_LENGTH - idx);

				/* Replace dots with underscores so that the routing key does not get corrupted */
				switch_replace_char(routingKey + idx, '.', '_', 0);
			}
			idx += strlen(routingKey + idx);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

void mod_amqp_producer_event_handler(switch_event_t* evt)
{
	mod_amqp_message_t *amqp_message;
	mod_amqp_producer_profile_t *profile = (mod_amqp_producer_profile_t *)evt->bind_user_data;
	switch_time_t now = switch_time_now();
	switch_time_t reset_time;

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event without a profile %p %p\n", (void *)evt, (void *)evt->event_user_data);
		return;
	}

	/* If the mod is disabled ignore the event */
	if (!profile->running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] not running\n", profile->name);
		return;
	}

	/* If the circuit breaker is active, ignore the event */
	reset_time = profile->circuit_breaker_reset_time;
	if (now < reset_time) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] circuit breaker hit[%d] (%d)\n", profile->name, (int) now, (int) reset_time);
		return;
	}

	switch_malloc(amqp_message, sizeof(mod_amqp_message_t));

	switch_event_serialize_json(evt, &amqp_message->pjson);
	mod_amqp_producer_routing_key(amqp_message->routing_key, evt, profile->format_fields);

	/* Queue the message to be sent by the worker thread, errors are reported only once per circuit breaker interval */
	if (switch_queue_trypush(profile->send_queue, amqp_message) != SWITCH_STATUS_SUCCESS) {
		unsigned int queue_size = switch_queue_size(profile->send_queue);

		/* Trip the circuit breaker for a short period to stop recurring error messages (time is measured in uS) */
		profile->circuit_breaker_reset_time = now + profile->circuit_breaker_ms * 1000;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "AMQP message queue full. Messages will be dropped for %.1fs! (Queue capacity %d)",
						  profile->circuit_breaker_ms / 1000.0, queue_size);

		mod_amqp_producer_msg_destroy(&amqp_message);
	}
}

switch_status_t mod_amqp_producer_destroy(mod_amqp_producer_profile_t **prof) {
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_connection_t *conn = NULL, *conn_next = NULL;
	switch_memory_pool_t *pool;
	mod_amqp_producer_profile_t *profile;

	if (!prof || !*prof) {
		return SWITCH_STATUS_SUCCESS;
	}

	profile = *prof;
	pool = profile->pool;

	if (profile->name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] shutting down...\n", profile->name);
		switch_core_hash_delete(globals.producer_hash, profile->name);
	}

	profile->running = 0;

	if (profile->producer_thread) {
		switch_thread_join(&status, profile->producer_thread);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Profile[%s] closing AMQP socket...\n", profile->name);

	for (conn = profile->conn_root; conn; conn = conn_next) {
		conn_next = conn->next;
		mod_amqp_connection_destroy(&conn);
	}

	profile->conn_active = NULL;
	profile->conn_root = NULL;

	while (profile->send_queue && switch_queue_trypop(profile->send_queue, (void**)&msg) == SWITCH_STATUS_SUCCESS) {
		mod_amqp_producer_msg_destroy(&msg);
	}

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	*prof = NULL;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_amqp_producer_create(char *name, switch_xml_t cfg)
{
	mod_amqp_producer_profile_t *profile = NULL;
	int arg = 0, i = 0;
	char  *argv[SWITCH_EVENT_ALL];
	switch_xml_t params, param, connections, connection;
	switch_threadattr_t *thd_attr = NULL;
	char *exchange = NULL, *exchange_type = NULL;
	switch_memory_pool_t *pool;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		goto err;
	}

	profile = switch_core_alloc(pool, sizeof(mod_amqp_producer_profile_t));
	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, name);
	profile->running = 1;
	memset(profile->format_fields, 0, MAX_ROUTING_KEY_FORMAT_FIELDS + 1);
	profile->event_ids[0] = SWITCH_EVENT_ALL;
	profile->event_subscriptions = 1;
	profile->conn_root   = NULL;
	profile->conn_active = NULL;

	/* Set reasonable defaults which may change if more reasonable defaults are found */
	/* Handle defaults of non string types */
	profile->circuit_breaker_ms = 10000;
	profile->reconnect_interval_ms = 1000;
	profile->send_queue_size = 5000;

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
			} else if (!strncmp(var, "circuit_breaker_ms", 18)) {
				int interval = atoi(val);
				if ( interval && interval > 0 ) {
					profile->circuit_breaker_ms = interval;
				}
			} else if (!strncmp(var, "send_queue_size", 15)) {
				int interval = atoi(val);
				if ( interval && interval > 0 ) {
					profile->send_queue_size = interval;
				}
			} else if (!strncmp(var, "format_fields", 13)) {
				int size = 0;
				if ((size = mod_amqp_count_chars(val, ',')) >= MAX_ROUTING_KEY_FORMAT_FIELDS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "You can have only %d routing fields in the routing key.\n",
									  MAX_ROUTING_KEY_FORMAT_FIELDS);
					goto err;
				}

				/* increment size because the count returned the number of separators, not number of fields */
				size++;

				switch_separate_string(val, ',', profile->format_fields, size);
				profile->format_fields[size] = NULL;
			} else if (!strncmp(var, "event_filter", 12)) {
				/* Parse new events */
				profile->event_subscriptions = switch_separate_string(val, ',', argv, (sizeof(argv) / sizeof(argv[0])));

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Found %d subscriptions\n", profile->event_subscriptions);

				for (arg = 0; arg < profile->event_subscriptions; arg++) {
					if (switch_name_event(argv[arg], &(profile->event_ids[arg])) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The switch event %s was not recognised.\n", argv[arg]);
					}
				}
			}
		} /* params for loop */
	}

	/* Handle defaults of string types */
	profile->exchange = exchange ? exchange : switch_core_strdup(profile->pool, "TAP.Events");
	profile->exchange_type = exchange_type ? exchange_type : switch_core_strdup(profile->pool, "topic");

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

	if ( mod_amqp_connection_open(profile->conn_root, &(profile->conn_active), profile->name, profile->custom_attr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] was unable to connect to any connection\n", profile->name);
	}

	/* Create a bounded FIFO queue for sending messages */
	if (switch_queue_create(&(profile->send_queue), profile->send_queue_size, profile->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create send queue of size %d!\n", profile->send_queue_size);
		goto err;
	}

	/* Start the event send thread. This will set up the initial connection */
	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&profile->producer_thread, thd_attr, mod_amqp_producer_thread, profile, profile->pool)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create 'amqp event sender' thread!\n");
		goto err;
	}

	/* Subscribe events */
	for (i = 0; i < profile->event_subscriptions; i++) {
		if (switch_event_bind_removable("AMQP",
										profile->event_ids[i],
										SWITCH_EVENT_SUBCLASS_ANY,
										mod_amqp_producer_event_handler,
										profile,
										&(profile->event_nodes[i])) != SWITCH_STATUS_SUCCESS) {

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot bind to event handler %d!\n",(int)profile->event_ids[i]);
			goto err;
		}
	}

	if ( switch_core_hash_insert(globals.producer_hash, name, (void *) profile) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to insert new profile [%s] into mod_amqp profile hash\n", name);
		goto err;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile[%s] Successfully started\n", profile->name);
	return SWITCH_STATUS_SUCCESS;

 err:
	/* Cleanup */
	mod_amqp_producer_destroy(&profile);
	return SWITCH_STATUS_GENERR;

}

/* This should only be called in a single threaded context from the producer profile send thread */
switch_status_t mod_amqp_producer_send(mod_amqp_producer_profile_t *profile, mod_amqp_message_t *msg)
{
	amqp_table_entry_t messageTableEntries[1];
	amqp_basic_properties_t props;
	int status;

	if (! profile->conn_active) {
		/* No connection, so we can not send the message. */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] not active\n", profile->name);
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_TIMESTAMP_FLAG | AMQP_BASIC_HEADERS_FLAG;
	props.content_type = amqp_cstring_bytes("text/json");
	props.delivery_mode = 1; /* non persistent delivery mode */
	props.timestamp = (uint64_t)time(NULL);

	props.headers.num_entries = 1;
	props.headers.entries = messageTableEntries;

	messageTableEntries[0].key = amqp_cstring_bytes("x_Liquid_MessageSentTimeStamp");
	messageTableEntries[0].value.kind = AMQP_FIELD_KIND_TIMESTAMP;
	messageTableEntries[0].value.value.u64 = (uint64_t)switch_micro_time_now();

	status = amqp_basic_publish(
								profile->conn_active->state,
								1,
								amqp_cstring_bytes(profile->exchange),
								amqp_cstring_bytes(msg->routing_key),
								0,
								0,
								&props,
								amqp_cstring_bytes(msg->pjson));

	if (status < 0) {
		const char *errstr = amqp_error_string2(-status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Profile[%s] failed to send event on connection[%s]: %s\n",
						  profile->name, profile->conn_active->name, errstr);

		/* This is bad, we couldn't send the message. Clear up any connection */
		mod_amqp_connection_close(profile->conn_active);
		profile->conn_active = NULL;
		return SWITCH_STATUS_SOCKERR;
	}

	return SWITCH_STATUS_SUCCESS;
}

void * SWITCH_THREAD_FUNC mod_amqp_producer_thread(switch_thread_t *thread, void *data)
{
	mod_amqp_message_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mod_amqp_producer_profile_t *profile = (mod_amqp_producer_profile_t *)data;
	amqp_boolean_t passive = 0;
	amqp_boolean_t durable = 1;

	while (profile->running) {

		if (!profile->conn_active) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Amqp no connection- reconnecting...\n");

			status = mod_amqp_connection_open(profile->conn_root, &(profile->conn_active), profile->name, profile->custom_attr);
			if ( status	== SWITCH_STATUS_SUCCESS ) {
				// Ensure that the exchange exists, and is of the correct type
				amqp_exchange_declare(profile->conn_active->state, 1,
									  amqp_cstring_bytes(profile->exchange),
									  amqp_cstring_bytes(profile->exchange_type),
									  passive,
									  durable,
									  amqp_empty_table);

				if (mod_amqp_log_if_amqp_error(amqp_get_rpc_reply(profile->conn_active->state), "Declaring exchange")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Amqp reconnect successful- connected\n");
					continue;
				}
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Profile[%s] failed to connect with code(%d), sleeping for %dms\n",
							  profile->name, status, profile->reconnect_interval_ms);
			switch_sleep(profile->reconnect_interval_ms * 1000);
			continue;
		}

		if (!msg && switch_queue_pop_timeout(profile->send_queue, (void**)&msg, 1000000) != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		if (msg) {
#ifdef MOD_AMQP_DEBUG_TIMING
			long times[TIME_STATS_TO_AGGREGATE];
			static unsigned int thistime = 0;
			switch_time_t start = switch_time_now();
#endif
			switch (mod_amqp_producer_send(profile, msg)) {
			case SWITCH_STATUS_SUCCESS:
				/* Success: prepare for next message */
				mod_amqp_producer_msg_destroy(&msg);
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

#ifdef MOD_AMQP_DEBUG_TIMING
			times[thistime++] = switch_time_now() - start;
			if (thistime >= TIME_STATS_TO_AGGREGATE) {
				int i;
				long min_time, max_time, avg_time;

				/* Calculate aggregate times */
				min_time = max_time = avg_time = times[0];
				for (i = 1; i < TIME_STATS_TO_AGGREGATE; ++i) {

					avg_time += times[i];
					if (times[i] < min_time) min_time = times[i];
					if (times[i] > max_time) max_time = times[i];
				}

				avg_time /= TIME_STATS_TO_AGGREGATE;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Microseconds to send last %d messages: Min %ld  Max %ld  Avg %ld\n",
								  TIME_STATS_TO_AGGREGATE, min_time, max_time, avg_time);
				thistime = 0;
			}
#endif
		}
	}

	/* Abort the current message */
	mod_amqp_producer_msg_destroy(&msg);

	// Terminate the thread
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Event sender thread stopped\n");
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
