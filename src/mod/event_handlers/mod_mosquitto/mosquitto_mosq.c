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
 * Norm Brandinger <norm@goes.com>
 *
 * mod_mosquitto -- Interface to an MQTT broker using Mosquitto
 *				  Implements a Publish/Subscribe (pub/sub) messaging pattern using the Mosquitto API library
 *				  Publishes FreeSWITCH events to one more more MQTT brokers
 *				  Subscribes to topics located on one more more MQTT brokers
 *
 * MQTT http://mqtt.org/
 * Mosquitto https://mosquitto.org/
 *
 */

#include <switch.h>

#include "mosquitto_mosq.h"
#include "mosquitto_utils.h"
#include "mod_mosquitto.h"
#include "mosquitto_events.h"
#include "mosquitto_config.h"
#include "mosquitto_cli.h"

static switch_status_t process_originate_message(mosquitto_mosq_userdata_t *userdata, char *payload_string, const struct mosquitto_message *message);
static switch_status_t process_bgapi_message(mosquitto_mosq_userdata_t *userdata, char *payload_string, const struct mosquitto_message *message);


void *SWITCH_THREAD_FUNC bgapi_exec(switch_thread_t *thread, void *obj)
{
	mosquitto_bgapi_job_t *job = NULL;
    switch_stream_handle_t stream = { 0 };
    switch_status_t status;
    char *reply, *freply = NULL;
    switch_event_t *event;
    char *arg;
    switch_memory_pool_t *pool;

	job = (mosquitto_bgapi_job_t *)obj;

    if (!job) {
        return NULL;
    }

	switch_thread_rwlock_rdlock(mosquitto_globals.bgapi_rwlock);

	pool = job->pool;

	SWITCH_STANDARD_STREAM(stream);

	if ((arg = strchr(job->cmd, ' '))) {
		*arg++ = '\0';
	}

	if ((status = switch_api_execute(job->cmd, arg, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
		reply = stream.data;
	} else {
		freply = switch_mprintf("%s: Command not found!\n", job->cmd);
		reply = freply;
	}

	if (!reply) {
		reply = "Command returned no output!";
	}

	if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", job->uuid_str);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", job->cmd);
		if (arg) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", arg);
		}

		switch_event_add_body(event, "%s", reply);
		switch_event_fire(&event);
	}

	switch_safe_free(stream.data);
	switch_safe_free(freply);

	job = NULL;
	switch_core_destroy_memory_pool(&pool);
	pool = NULL;

	switch_thread_rwlock_unlock(mosquitto_globals.bgapi_rwlock);

	return NULL;
}


static switch_status_t process_originate_message(mosquitto_mosq_userdata_t *userdata, char *payload_string, const struct mosquitto_message *message)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *argv[3] = { 0 };
	switch_channel_t *channel = NULL;
	switch_core_session_t *session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	int timeout = 30;
	switch_event_t *originate_vars = NULL;
	//char *cid_name = NULL;
	//char *cid_number = NULL;
	char *aleg = NULL;
	char *bleg = NULL;
	char *dp = "XML";
	char *context = "default";

	mosquitto_profile_t *profile = userdata->profile;
	mosquitto_connection_t *connection = userdata->connection;
	mosquitto_topic_t *topic = NULL;

	switch_event_create(&originate_vars, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(originate_vars);
	switch_event_add_header(originate_vars, SWITCH_STACK_BOTTOM, "originate_timeout", "%d", 30);
	switch_separate_string(payload_string, ' ', argv, 3);

	topic = locate_connection_topic(profile, connection, message->topic);

	if (!topic) {
		log(ERROR, "Unknown topic: messsage topic %s within profile %s and connection %s\n", message->topic, profile->name, connection->name);
		return status;
	} else {
		log(DEBUG, "Matched topic topic %s\n", topic->name);
	}

	if (!topic->originate_authorized) {
		log(ERROR, "Topic %s not authorized to originate calls within profile %s and connection %s\n", message->topic, profile->name, connection->name);
		return status;
	}

	if (zstr(argv[1])) {
		log(ERROR, "Aleg passed in from the originate is empty\n");
		return status;
	}

	if (zstr(argv[2])) {
		log(ERROR, "Bleg passed in from the originate is empty\n");
		return status;
	}

	aleg = argv[1];
	bleg = argv[2];

	//status = switch_ivr_originate(session, NULL, &cause, NULL, timeout, NULL, cid_name, cid_number, NULL, originate_vars, SOF_NONE, NULL);
	status = switch_ivr_originate(NULL, &session, &cause, aleg, timeout, NULL, NULL, NULL, NULL, originate_vars, SOF_NONE, NULL, NULL);
	if (status != SWITCH_STATUS_SUCCESS || !session) {
		log(WARNING, "Originate to [%s] failed, cause: %s\n", aleg, switch_channel_cause2str(cause));
		return status;
	}

	channel = switch_core_session_get_channel(session);

	if (*bleg == '&' && *(bleg + 1)) {
		switch_caller_extension_t *extension = NULL;
		char *app_name = switch_core_session_strdup(session, (bleg + 1));
		char *arg = NULL, *e;

		if ((e = strchr(app_name, ')'))) {
			*e = '\0';
		}

		if ((arg = strchr(app_name, '('))) {
			*arg++ = '\0';
		}

		if ((extension = switch_caller_extension_new(session, app_name, arg)) == 0) {
			log(CRIT, "Memory Error!\n");
			abort();
		}
		switch_caller_extension_add_application(session, extension, app_name, arg);
		switch_channel_set_caller_extension(channel, extension);
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		switch_ivr_session_transfer(session, bleg, dp, context);
	}

	return status;
}


static switch_status_t process_bgapi_message(mosquitto_mosq_userdata_t *userdata, char *payload_string, const struct mosquitto_message *message)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_bgapi_job_t *job = NULL;
	switch_uuid_t uuid;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	const char *p, *arg = payload_string;
	char my_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";

	if (!strncasecmp(payload_string, "uuid:", 5)) {
		p = payload_string + 5;
		if ((arg = strchr(p, ' ')) && *arg++) {
			switch_copy_string(my_uuid, p, arg - p);
		}
	}

	if (zstr(arg)) {
		log(ERROR, "-ERR Invalid syntax arg empty\n");
		return status;
	}

	switch_core_new_memory_pool(&pool);
	job = switch_core_alloc(pool, sizeof(*job));
	job->cmd = switch_core_strdup(pool, arg);
	job->pool = pool;

	if (*my_uuid) {
		switch_copy_string(job->uuid_str, my_uuid, strlen(my_uuid)+1);
	} else {
		switch_uuid_get(&uuid);
		switch_uuid_format(job->uuid_str, &uuid);
	}

	switch_threadattr_create(&thd_attr, job->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	log(INFO, "+OK Job-UUID: %s\n", job->uuid_str);
	switch_thread_create(&thread, thd_attr, bgapi_exec, job, job->pool);

	return status;
}


void mosq_callbacks_set(mosquitto_connection_t *connection)
{
	mosquitto_log_callback_set(connection->mosq, mosq_log_callback);
	mosquitto_connect_callback_set(connection->mosq, mosq_connect_callback);
	mosquitto_message_callback_set(connection->mosq, mosq_message_callback);
	mosquitto_subscribe_callback_set(connection->mosq, mosq_subscribe_callback);
	mosquitto_publish_callback_set(connection->mosq, mosq_publish_callback);
	mosquitto_disconnect_callback_set(connection->mosq, mosq_disconnect_callback);
}


void mosq_disconnect_callback(struct mosquitto *mosq, void *user_data, int rc)
{
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;
	mosquitto_mosq_userdata_t *userdata = NULL;

	if (!user_data) {
		log(ERROR, "disconnect userdata NULL rc:%d\n", rc);
		return;
	} else {
		userdata = (mosquitto_mosq_userdata_t *)user_data;
	}

	if (!userdata->connection) {
		log(ERROR, "disconnect connection NULL rc:%d\n", rc);
		return;
	} else {
		connection = userdata->connection;
	}
	if (!userdata->profile) {
		log(ERROR, "disconnect profile NULL rc:%d\n", rc);
		return;
	} else {
		profile = userdata->profile;
	}

	log(DEBUG, "profile:%s connection:%s rc:%d disconnected", profile->name, connection->name, rc);
	connection->connected = SWITCH_FALSE;
	//log(DEBUG, "Reconnect rc: %d\n", mosquitto_reconnect(connection->mosq));
}


void mosq_publish_callback(struct mosquitto *mosq, void *user_data, int message_id)
{
	log(DEBUG, "published message id: %d", message_id);
}


void mosq_message_callback(struct mosquitto *mosq, void *user_data, const struct mosquitto_message *message)
{
	char *payload_string = NULL;
	mosquitto_mosq_userdata_t *userdata = NULL;

	if (!message->payloadlen) {
		log(DEBUG, "mosq_message_callback(): Received topic: %s NULL message exiting.\n", (char *)message->topic);
		return;
	}

	if (!user_data) {
		log(DEBUG, "mosq_message_callback(): Received topic: %s user_data NULL exiting.\n", (char *)message->topic);
		return;
	}

	log(DEBUG, "mosq_message_callback(): Received topic: %s payloadlen: %d message: %s\n", (char *)message->topic, message->payloadlen, (char *)message->payload);

	userdata = (mosquitto_mosq_userdata_t *)user_data;

	if (!(payload_string = strndup((char *)message->payload, message->payloadlen))) {
		log(ERROR, "mosq_message_callback(): Out of memory trying to duplicate %s\n", (char *)message->payload);
		return;
	}

	if (!strncasecmp(payload_string, "bgapi", 5)) {
		process_bgapi_message(userdata, payload_string, message);
	} else if (!strncasecmp(payload_string, "originate", 9)) {
		process_originate_message(userdata, payload_string, message);
	}

	switch_safe_free(payload_string);

}


void mosq_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{

	log(DEBUG, "mosq_subscribe_callback(): Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(int i=1; i<qos_count; i++){
		log(DEBUG, ", %d", granted_qos[i]);
	}
}


void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Print all log messages regardless of level. */
	log(DEBUG, "mosq_log_callback(): %s\n", str);
}


void mosq_connect_callback(struct mosquitto *mosq, void *user_data, int result)
{
	mosquitto_mosq_userdata_t *userdata = NULL;
	mosquitto_connection_t *connection = NULL;

	log(DEBUG, "mosq_connect_callback(): result: %d\n", result);

	if (!user_data) {
		return;
	} else {
		userdata = (mosquitto_mosq_userdata_t *)user_data;
	}

	if (!userdata->connection) {
		return;
	} else {
		connection = userdata->connection;
	}

	if (!result) {
		mosquitto_profile_t *profile = NULL;
		log(CONSOLE, "mosq_connect_callback(): profile %s connection %s successful\n", connection->profile_name, connection->name);
		connection->retry_count = 0;
		connection->connected = SWITCH_TRUE;
		profile = locate_profile(connection->profile_name);
		profile_activate(profile);
		//mosquitto_subscribe(mosq, NULL, "FreeSWITCH/command", 2);
		//mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
	} else {
		if (connection->retries && (connection->retry_count == connection->retries)) {
			log(CONSOLE, "mosq_connect_callback(): profile %s connection to %s retried %d times, stopping\n", connection->profile_name, connection->name, connection->retry_count);
			mosquitto_disconnect(connection->mosq);
			mosquitto_destroy(connection->mosq);
			connection->mosq = NULL;
			connection->connected = SWITCH_FALSE;
		}
		connection->retry_count++;
	}
}

switch_status_t mosq_int_option(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;


	//if (!strncasecmp(connection->protocol_version, "V311", 4)) {
	//	protocol_version = MQTT_PROTOCOL_V311;
	//}

	rc = mosquitto_int_option(connection->mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V311);
	log(DEBUG, "mosquitto_init_option() for profile [%s] connection [%s] Protocol Version [%s] rc %d\n", connection->profile_name, connection->name, connection->protocol_version, rc);
	//rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_RECEIVE_MAXIMUM, connection->receive_maximum);
	//rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_SEND_MAXIMUM, connection->send_maximum);
	//rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_SSL_CTX_WITH_DEFAULTS, connection->ssl_ctx_with_defaults);
	//rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_TLS_OCSP_REQUIRED, connection->tls_ocsp_required);

	return status;
}

switch_status_t mosq_reconnect_delay_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	rc = mosquitto_reconnect_delay_set(connection->mosq, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff);
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(DEBUG, "Succeeded setting reconnect delay for profile [%s] connection [%s] delay [%d] delay_max [%d] backoff [%s]\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled");
			break;
		case MOSQ_ERR_INVAL:
			log(DEBUG, "Failed setting reconnect delay for profile [%s] connection [%s] delay [%d] delay_max [%d] backoff [%s] invalid parameters\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled");
			return SWITCH_STATUS_GENERR;
		default:
			log(DEBUG, "Failed setting reconnect delay for profile [%s] connection [%s] delay [%d] delay_max [%d] backoff [%s] unknown return code (%d)\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled", rc);
			return SWITCH_STATUS_GENERR;
	}

	return status;
}


switch_status_t mosq_message_retry_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (connection->message_retry > 0) {
		mosquitto_message_retry_set(connection->mosq, connection->message_retry);
		log(DEBUG, "Message retry set to %d for profile [%s] connection [%s]\n", connection->message_retry, connection->profile_name, connection->name);
	}

	return status;
}


switch_status_t mosq_max_inflight_messages_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	if (connection->max_inflight_messages > 0) {
		rc = mosquitto_max_inflight_messages_set(connection->mosq, connection->max_inflight_messages);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(DEBUG, "Max inflight messages set to %d for profile [%s] connection [%s]\n", connection->max_inflight_messages, connection->profile_name, connection->name);
				break;
			case MOSQ_ERR_INVAL:
				log(DEBUG, "Max inflight messages set to %d for profile [%s] connection [%s] resulted in invalid parameter input\n", connection->max_inflight_messages, connection->profile_name, connection->name);
				return SWITCH_STATUS_GENERR;
		}
	}

	return status;
}


switch_status_t mosq_username_pw_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	if (connection->username && connection->password) {
		rc = mosquitto_username_pw_set(connection->mosq, connection->username, connection->password);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(DEBUG, "Client username set to [%s]\n", connection->username);
				break;
			case MOSQ_ERR_INVAL:
				log(ERROR, "Setting username/pw [%s] failed invalid parameters\n", connection->username);
				return SWITCH_STATUS_GENERR;
			case MOSQ_ERR_NOMEM:
				log(ERROR, "Setting username/pw [%s] failed out of memory\n", connection->username);
				return SWITCH_STATUS_GENERR;
			default:
				log(ERROR, "Setting username/pw [%s] unknown return code (%d)\n", connection->username, rc);
				return SWITCH_STATUS_GENERR;
		}
	}
	return status;
}


switch_status_t mosq_connect(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;
	int loop;

	mosq_callbacks_set(connection);

	connection->connected = SWITCH_FALSE;
	connection->retry_count = 0;
	if (!connection->bind_address) {
		rc = mosquitto_connect(connection->mosq, connection->host, connection->port, connection->keepalive);
		log(DEBUG, "mosquitto_connect() rc:%d\n", rc);
	} else {
		if (!connection->srv) {
			rc = mosquitto_connect_bind(connection->mosq, connection->host, connection->port, connection->keepalive, connection->bind_address);
		} else {
			rc = mosquitto_connect_srv(connection->mosq, connection->host, connection->keepalive, connection->bind_address);
		}
	}
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(DEBUG, "Connected to: %s:%d keepalive:%d bind_address:%s SRV: %s\n", connection->host, connection->port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled");
			connection->connected = true;
			break;
		case MOSQ_ERR_INVAL:
			log(ERROR, "Failed connection to: %s:%d keepalive:%d bind_address:%s SRV: %s with invalid parameters\n", connection->host, connection->port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled");
			mosquitto_destroy(connection->mosq);
			connection->mosq = NULL;
			return SWITCH_STATUS_GENERR;
		case MOSQ_ERR_ERRNO:
			mosquitto_destroy(connection->mosq);
			connection->mosq = NULL;
			return SWITCH_STATUS_GENERR;
		default:
			log(ERROR, "Failed connection to: %s:%d keepalive:%d bind_address: %s SRV: %s unknown return code (%d)\n", connection->host, connection->port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled", rc);
			mosquitto_destroy(connection->mosq);
			connection->mosq = NULL;
			return SWITCH_STATUS_GENERR;
	}


	loop = mosquitto_loop_start(connection->mosq);
	if (loop != MOSQ_ERR_SUCCESS){
		log(ERROR, "Unable to start loop: %i\n", loop);
	}
	return status;

}


switch_status_t mosq_loop_stop(mosquitto_connection_t *connection, switch_bool_t force)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int rc = 0;

	rc = mosquitto_loop_stop(connection->mosq, force);
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(INFO, "Shutting down profile: %s connection: %s mosquitto_loop_stop() %d successful\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(INFO, "Shutting down profile: %s connection: %s mosquitto_loop_stop() %d input parameters were invalid\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NOT_SUPPORTED:
			log(INFO, "Shutting down profile: %s connection: %si mosquitto_loop_stop() %d thread support is not available\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_GENERR;
			break;
	}

	return status;
}

switch_status_t mosq_disconnect(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!connection) {
		log(ERROR, "mosq_disconnect() called with NULL connection\n");
		return SWITCH_STATUS_GENERR;
	}

	mosquitto_disconnect(connection->mosq);
	mosq_loop_stop(connection, SWITCH_TRUE);
	mosquitto_destroy(connection->mosq);
	connection->mosq = NULL;

	if (connection->connected) {
		int rc = mosquitto_disconnect(connection->mosq);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(DEBUG, "Disconnected profile %s connection %s from the broker\n", connection->profile_name, connection->name);
				connection->connected = false;
				break;
			case MOSQ_ERR_INVAL:
				log(DEBUG, "Disconnection for profile %s connection %s returned: input parameters were invalid \n", connection->profile_name, connection->name);
				return SWITCH_STATUS_GENERR;
			case MOSQ_ERR_NO_CONN:
				log(DEBUG, "Tried to disconnect profile %s connection %s but there was no connection to the broker\n", connection->profile_name, connection->name);
				connection->connected = false;
				return SWITCH_STATUS_GENERR;
			default:
				log(DEBUG, "Tried to disconnect profile %s connection %s, received an unknown return code (%d)\n", connection->profile_name, connection->name, rc);
				connection->connected = false;
				return SWITCH_STATUS_GENERR;
		}
	}

	return status;
}


switch_status_t mosq_new(mosquitto_profile_t *profile, mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_mosq_userdata_t *userdata = NULL;
	switch_bool_t clean_session = connection->clean_session;

	if (connection->mosq) {
		log(ERROR, "mosq_new() called, but the connection has an existing mosq structure exiting\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!profile) {
		log(ERROR, "mosq_new() called with NULL profile\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection) {
		log(ERROR, "mosq_new() called with NULL connection\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!profile->enable || !connection->enable) {
		log(DEBUG, "mosq_new_clint() profile: %s %s connection: %s %s\n", profile->name, profile->enable ? "enabled" : "disabled", connection->name, connection->enable ? "enabled" : "disabled");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(userdata = switch_core_alloc(profile->pool, sizeof(mosquitto_mosq_userdata_t)))) {
		log(CRIT, "mosq_new() failed to allocate memory for mosquitto_new() userdata structure profile: %s connection: %s\n", profile->name, connection->name);
		return SWITCH_STATUS_GENERR;
	} else {
		connection->userdata = userdata;

		userdata->profile = profile;
		userdata->connection = connection;
	}

	if (connection->client_id == NULL) {
		if (connection->clean_session == SWITCH_FALSE) {
			log(INFO, "mosquitto_new() profile: %s connection: %s called with NULL client_id, forcing clean_session to TRUE\n", profile->name, connection->name);
			clean_session = SWITCH_TRUE;
		}
	}

	log(DEBUG, "mosquitto_new() being called with profile: %s connection: %s clean_session: %s client_id: %s\n", profile->name, connection->name, clean_session ? "True" : "False", connection->client_id);
	connection->mosq = mosquitto_new(connection->client_id, clean_session, userdata);

	if (connection->mosq == NULL) {
		switch (errno) {
			case ENOMEM:
				log(ERROR, "mosquitto_new(%s, %d, NULL) out of memory\n", connection->client_id, connection->clean_session);
				return SWITCH_STATUS_GENERR;
			case EINVAL:
				log(ERROR, "mosquitto_new(%s, %d, NULL) invalid input parameters\n", connection->client_id, connection->clean_session);
				return SWITCH_STATUS_GENERR;
			default:
				log(ERROR, "mosquitto_new(%s, %d, NULL) errno(%s)\n", connection->client_id, connection->clean_session, mosquitto_strerror(errno));
				return SWITCH_STATUS_GENERR;
		}
	}

	return status;
}


switch_status_t mosq_subscribe(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_connection_t *connection;
	int rc;

	if (!(connection = locate_connection(profile, topic->connection_name))) {
		log(ERROR, "Cannot subscribe to topic %s because connection %s (profile %s) is invalid\n", topic->name, topic->connection_name, profile->name);
		return SWITCH_STATUS_GENERR;
	}
	if (!connection->mosq) {
		log(ERROR, "Cannot subscribe to topic %s because connection %s (profile %s) mosq is NULL\n", topic->name, connection->name, profile->name);
		return SWITCH_STATUS_GENERR;
	}

	rc = mosquitto_subscribe(connection->mosq, topic->mid, topic->pattern, topic->qos);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(INFO, "profile %s subscriber %s connection %s topic %s subscribed to pattern: %s\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_TRUE;
			break;
		case MOSQ_ERR_INVAL:
			log(ERROR, "profile %s subscriber %s connection %s topic %s pattern: %s the input parameters were invalid\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NOMEM:
			log(CRIT, "profile %s subscriber %s connection %s topic %s pattern: %s an out of memory condition occurred\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NO_CONN:
			log(WARNING, "profile %s subscriber %s connection %s topic %s pattern: %s not connected to an MQTT broker\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_MALFORMED_UTF8:
			log(ERROR, "profile %s subscriber %s connection %s topic %s pattern: %s the pattern is not valid UTF-8\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_OVERSIZE_PACKET:
			log(ERROR, "profile %s subscriber %s connection %s topic %s pattern: %s the pattern larger than the MQTT broker can support\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
	}

	return status;
}


switch_status_t mosq_startup(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	int rc;
	int major;
	int minor;
	int revision;

	switch_mutex_lock(mosquitto_globals.mutex);
	switch_queue_create(&mosquitto_globals.event_queue, mosquitto_globals.event_queue_size, mosquitto_globals.pool);

	/* mosquitto_lib_init() is NOT thread safe */
	mosquitto_lib_init();

	rc = mosquitto_lib_version(&major, &minor, &revision);
	log(DEBUG, "Library rc=%d, version %d.%d.%d initialized\n", rc, major, minor, revision);

	mosquitto_globals.mosquitto_lib.major = major;
	mosquitto_globals.mosquitto_lib.minor = minor;
	mosquitto_globals.mosquitto_lib.revision = revision;
	switch_mutex_unlock(mosquitto_globals.mutex);

	status = initialize_profiles();

	return status;
}


switch_status_t mosq_shutdown(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(mosquitto_globals.profiles_mutex);
	for (switch_hash_index_t *profiles_hi = switch_core_hash_first(mosquitto_globals.profiles); profiles_hi; profiles_hi = switch_core_hash_next(&profiles_hi)) {
		mosquitto_profile_t *profile = NULL;
		void *val;
		switch_core_hash_this(profiles_hi, NULL, NULL, &val);
		profile = (mosquitto_profile_t *)val;
		remove_profile(profile->name);
	}
	switch_mutex_unlock(mosquitto_globals.profiles_mutex);

	mosquitto_lib_cleanup();

	return status;
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
