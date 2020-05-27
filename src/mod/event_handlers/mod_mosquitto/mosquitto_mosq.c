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
 * Norm Brandinger <n.brandinger@gmail.com>
 *
 * mod_mosquitto: Interface to an MQTT broker using Mosquitto
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


/**
 * @brief      This is a threaded bgapi execution routine.
 *
 * @details    This routine is called 'in a new thread' to process a bgapi command.
 *
 * @param[in]  *thread  Pointer to a FreeSWITCH thread structure that this routine is called in
 * @param[in]  *obj	Pointer to the bgapi command and arguments
 */

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
		reply = (char *)stream.data;
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


/**
 * @brief   This routine is called when an 'originate' message is received by a subscription
 *
 * @details This routine will set up and execute an originate command
 *
 * @param[in]   *userdata	Pointer to a userdata structure set up when the connection associated with this subscription was performed.
 * @param[in]   *payload_string	Pointer to a local copy of the subscribed (received) message
 * @param[in]   *message	Pointer to the received message in a Mosquitto message data structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

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


	mosquitto_profile_t *profile = userdata->profile;
	mosquitto_connection_t *connection = userdata->connection;
	mosquitto_topic_t *topic = NULL;

	switch_event_create(&originate_vars, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(originate_vars);
	switch_event_add_header(originate_vars, SWITCH_STACK_BOTTOM, "originate_timeout", "%d", 30);
	switch_separate_string(payload_string, ' ', argv, 3);

	topic = locate_connection_topic(profile, connection, message->topic);

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Unknown topic: messsage topic %s within profile %s and connection %s\n", message->topic, profile->name, connection->name);
		return status;
	} else {
		log(SWITCH_LOG_DEBUG, "Matched topic topic %s\n", topic->name);
	}

	if (!topic->originate_authorized) {
		log(SWITCH_LOG_ERROR, "Topic %s not authorized to originate calls within profile %s and connection %s\n", message->topic, profile->name, connection->name);
		return status;
	}

	if (zstr(argv[1])) {
		log(SWITCH_LOG_ERROR, "Aleg passed in from the originate is empty\n");
		return status;
	}

	if (zstr(argv[2])) {
		log(SWITCH_LOG_ERROR, "Bleg passed in from the originate is empty\n");
		return status;
	}

	aleg = argv[1];
	bleg = argv[2];

	//status = switch_ivr_originate(session, NULL, &cause, NULL, timeout, NULL, cid_name, cid_number, NULL, originate_vars, SOF_NONE, NULL);
	status = switch_ivr_originate(NULL, &session, &cause, aleg, timeout, NULL, NULL, NULL, NULL, originate_vars, SOF_NONE, NULL, NULL);
	if (status != SWITCH_STATUS_SUCCESS || !session) {
		log(SWITCH_LOG_WARNING, "Originate to [%s] failed, cause: %s\n", aleg, switch_channel_cause2str(cause));
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
			log(SWITCH_LOG_CRIT, "Memory Error!\n");
			abort();
		}
		switch_caller_extension_add_application(session, extension, app_name, arg);
		switch_channel_set_caller_extension(channel, extension);
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		char *dp = "XML";
		char *context = "default";
		switch_ivr_session_transfer(session, bleg, dp, context);
	}

	return status;
}


/**
 * @brief   This routine is called when an 'bgapi' message is received by a subscription
 *
 * @details This routine will set up and execute a bgapi command
 *
 * @param[in]   *userdata	Pointer to a userdata structure set up when the connection associated with this subscription was performed.
 * @param[in]   *payload_string	Pointer to a local copy of the subscribed (received) message
 * @param[in]   *message	Pointer to the received message in a Mosquitto message data structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

static switch_status_t process_bgapi_message(mosquitto_mosq_userdata_t *userdata, char *payload_string, const struct mosquitto_message *message)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_bgapi_job_t *job = NULL;
	switch_uuid_t uuid;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	const char *arg = payload_string;
	char my_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";

	if (!strncasecmp(payload_string, "uuid:", 5)) {
		const char *p = payload_string + 5;
		if ((arg = strchr(p, ' ')) && *arg++) {
			switch_copy_string(my_uuid, p, arg - p);
		}
	}

	if (zstr(arg)) {
		log(SWITCH_LOG_ERROR, "-ERR Invalid syntax arg empty\n");
		return status;
	}

	switch_core_new_memory_pool(&pool);
	job = (mosquitto_bgapi_job_t *)switch_core_alloc(pool, sizeof(*job));
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
	log(SWITCH_LOG_INFO, "+OK Job-UUID: %s\n", job->uuid_str);
	switch_thread_create(&thread, thd_attr, bgapi_exec, job, job->pool);

	return status;
}


/**
 * @brief   	This routine sets up callbacks for associated with a connection to an MQTT broker
 *
 * @details		Set the logging callback.  This should be used if you want event logging information from the client library.
 *
 * @param[in]   *connection	Pointer to a connection hash that these callbacks will be associated with
 */

void mosq_callbacks_set(mosquitto_connection_t *connection)
{
	mosquitto_log_callback_set(connection->mosq, mosq_log_callback);
	mosquitto_connect_callback_set(connection->mosq, mosq_connect_callback);
	mosquitto_message_callback_set(connection->mosq, mosq_message_callback);
	mosquitto_subscribe_callback_set(connection->mosq, mosq_subscribe_callback);
	mosquitto_publish_callback_set(connection->mosq, mosq_publish_callback);
	mosquitto_disconnect_callback_set(connection->mosq, mosq_disconnect_callback);
}


/**
 * @brief   This routine is called when a disconnect request is processed
 *
 * @details This callback performs cleanup housekeeping
 *
 * @param[in]   *mosq		Pointer to the mosquitto structure associated with the request
 * @param[in]   *user_data	Pointer to userdata that was set up when the connection was created
 * @param[in]   *rc			Return code associated with the disconnect request
 */

void mosq_disconnect_callback(struct mosquitto *mosq, void *user_data, int rc)
{
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;
	mosquitto_mosq_userdata_t *userdata = NULL;

	if (!user_data) {
		log(SWITCH_LOG_ERROR, "disconnect userdata NULL rc:%d\n", rc);
		return;
	} else {
		userdata = (mosquitto_mosq_userdata_t *)user_data;
	}

	if (!userdata->connection) {
		log(SWITCH_LOG_ERROR, "disconnect connection NULL rc:%d\n", rc);
		return;
	} else {
		connection = userdata->connection;
	}
	if (!userdata->profile) {
		log(SWITCH_LOG_ERROR, "disconnect profile NULL rc:%d\n", rc);
		return;
	} else {
		profile = userdata->profile;
	}

	log(SWITCH_LOG_DEBUG, "Profile %s connection %s rc %d disconnected", profile->name, connection->name, rc);
	//connection->connected = SWITCH_FALSE;
	//mosq_loop_stop(connection, SWITCH_TRUE);
	log(SWITCH_LOG_ALERT, "Reconnect rc %d\n", mosquitto_reconnect(connection->mosq));
}


/**
 * @brief   This routine is called when a publish request is processed
 *
 * @details This callback currently only logs the published message
 *
 * @param[in]   *mosq		Pointer to the mosquitto structure associated with the request
 * @param[in]   *user_data	Pointer to userdata that was set up when the connection was created
 * @param[in]   *message_id	Message ID of the published message
 */

void mosq_publish_callback(struct mosquitto *mosq, void *user_data, int message_id)
{
	mosquitto_mosq_userdata_t *userdata;
	mosquitto_profile_t *profile;
	mosquitto_connection_t *connection;

	userdata = (mosquitto_mosq_userdata_t *)user_data;
	profile = userdata->profile;
	connection = userdata->connection;

	log(SWITCH_LOG_DEBUG, "Profile %s connection %s message id %d published\n",
		profile->name, connection->name, message_id);

	mosquitto_log(SWITCH_LOG_INFO, "Profile %s connection %s message id %d published\n",
		profile->name, connection->name, message_id);

	profile_log(SWITCH_LOG_INFO, profile, "Profile %s connection %s message id %d published\n",
		profile->name, connection->name, message_id);
}


/**
 * @brief   This routine is called when a message is received from an MQTT broker
 *
 * @details This callback processes and possbily takes action based on the content of the received message
 *
 * @param[in]   *mosq		Pointer to the mosquitto structure associated with the request
 * @param[in]   *user_data	Pointer to userdata that was set up when the connection was created
 * @param[in]   *message	Pointer to the mosquitto message structure
 */

void mosq_message_callback(struct mosquitto *mosq, void *user_data, const struct mosquitto_message *message)
{
	char *payload_string = NULL;
	mosquitto_mosq_userdata_t *userdata = NULL;

	if (!message->payloadlen) {
		log(SWITCH_LOG_DEBUG, "mosq_message_callback(): Received topic %s NULL message exiting.\n", (char *)message->topic);
		return;
	}

	if (!user_data) {
		log(SWITCH_LOG_DEBUG, "mosq_message_callback(): Received topic %s user_data NULL exiting.\n", (char *)message->topic);
		return;
	}

	userdata = (mosquitto_mosq_userdata_t *)user_data;

	if (!(payload_string = strndup((char *)message->payload, message->payloadlen))) {
		log(SWITCH_LOG_ERROR, "mosq_message_callback(): Out of memory trying to duplicate %s\n", (char *)message->payload);
		return;
	}

	log(SWITCH_LOG_DEBUG, "Profile %s received topic %s payloadlen %d message %s\n", userdata->profile->name, (char *)message->topic, message->payloadlen, payload_string);

	if (mosquitto_globals.log.details) {
		mosquitto_log(SWITCH_LOG_INFO, "Profile %s received topic %s payloadlen %d message %s\n", userdata->profile->name, (char *)message->topic, message->payloadlen, payload_string);
	} else {
		mosquitto_log(SWITCH_LOG_INFO, "Profile %s received topic %s\n", userdata->profile->name, (char *)message->topic);

	}

	if (userdata->profile->log->details) {
		profile_log(SWITCH_LOG_INFO, userdata->profile, "Profile %s received topic %s payloadlen %d message %s\n", userdata->profile->name, (char *)message->topic, message->payloadlen, payload_string);
	} else {
		profile_log(SWITCH_LOG_INFO, userdata->profile, "Profile %s received topic %s\n", userdata->profile->name, (char *)message->topic);
	}

	if (!strncasecmp(payload_string, "bgapi", 5)) {
		process_bgapi_message(userdata, payload_string, message);
	} else if (!strncasecmp(payload_string, "originate", 9)) {
		process_originate_message(userdata, payload_string, message);
	}

	switch_safe_free(payload_string);

}


/**
 * @brief   This routine is called when a subscribe has been processed
 *
 * @details This callback currently logs information related to the subscription
 *
 * @param[in]   *mosq			Pointer to the mosquitto structure associated with the request
 * @param[in]   *userdata		Pointer to userdata that was set up when the connection was created
 * @param[in]   *mid			Message ID
 * @param[in]   *qos_count		Quality of Service
 * @param[in]   *granted_qos	Granted Quality of Service
 */

void mosq_subscribe_callback(struct mosquitto *mosq, void *user_data, int mid, int qos_count, const int *granted_qos)
{
	mosquitto_mosq_userdata_t *userdata;
	mosquitto_profile_t *profile;
	mosquitto_connection_t *connection;

	userdata = (mosquitto_mosq_userdata_t *)user_data;
	profile = userdata->profile;
	connection = userdata->connection;

	log(SWITCH_LOG_DEBUG, "Profile %s connection %s message id %d qos %d subscribed\n", profile->name, connection->name, mid, granted_qos[0]);

	mosquitto_log(SWITCH_LOG_INFO, "Profile %s connection %s message id %d qos %d subscribed\n", profile->name, connection->name, mid, granted_qos[0]);

	profile_log(SWITCH_LOG_INFO, profile, "Profile %s connection %s message id %d qos %d subscribed\n", profile->name, connection->name, mid, granted_qos[0]);

}


/**
 * @brief   This routine is called for ALL messages, regardless of level
 *
 * @details This callback currently logs the message
 *
 * @param[in]	mosq		The mosquitto instance making the callback.
 * @param[in]	userdata	The user data provided in mosquitto_new
 * @param[in]	level		The log message level from the values: MOSQ_LOG_INFO MOSQ_LOG_NOTICE MOSQ_LOG_WARNING MOSQ_LOG_ERR MOSQ_LOG_SWITCH_LOG_DEBUG
 * @param[in]	str			The message string.
 */

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	switch_log_level_t	log_level = SWITCH_LOG_DEBUG;

	switch(level) {
		case MOSQ_LOG_INFO:
			log_level = SWITCH_LOG_INFO;
			break;
		case MOSQ_LOG_NOTICE:
			log_level = SWITCH_LOG_NOTICE;
			break;
		case MOSQ_LOG_WARNING:
			log_level = SWITCH_LOG_WARNING;
			break;
		case MOSQ_LOG_ERR:
			log_level = SWITCH_LOG_ERROR;
			break;
		case MOSQ_LOG_DEBUG:
			log_level = SWITCH_LOG_DEBUG;
			break;
	}

	/* Print log messages after having converted the mosquitto levels into FreeSWITCH levels */
	log(log_level, "mosq_log_callback() %s\n", str);
}


/**
 * @brief   This routine is called when a connect request has been processed
 *
 * @details This callback performs housekeeping related to the connect request
 *
 * @param[in]   *mosq			Pointer to the mosquitto structure associated with the request
 * @param[in]   *user_data		Pointer to userdata that was set up when the connection was created
 * @param[in]   *result			Result of the connection
 */

void mosq_connect_callback(struct mosquitto *mosq, void *user_data, int result)
{
	mosquitto_mosq_userdata_t *userdata = NULL;
	mosquitto_connection_t *connection = NULL;

	log(SWITCH_LOG_DEBUG, "mosq_connect_callback() result %d\n", result);

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

		log(SWITCH_LOG_CONSOLE, "mosq_connect_callback() Profile %s connection %s successful\n", connection->profile_name, connection->name);
		connection->retry_count = 0;
		connection->connected = SWITCH_TRUE;
		profile = locate_profile(connection->profile_name);
		profile_activate(profile);
		//mosquitto_subscribe(mosq, NULL, "FreeSWITCH/command", 2);
		//mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
	} else {
		if (connection->retries && (connection->retry_count == connection->retries)) {
			log(SWITCH_LOG_CONSOLE, "mosq_connect_callback() Profile %s connection to %s retried %d times, stopping\n", connection->profile_name, connection->name, connection->retry_count);
			mosquitto_disconnect(connection->mosq);
			mosq_destroy(connection);
		}
		connection->retry_count++;
	}
}


/**
 * @brief   This routine is called to set some initialization options before a connection is requested
 *
 * @details This routine sets up various options, such as the version of the MQTT protocol to used with the connection
 *
 *			MOSQ_OPT_PROTOCOL_VERSION - Value must be set to either MQTT_PROTOCOL_V31, MQTT_PROTOCOL_V311, or MQTT_PROTOCOL_V5.
 *			Must be set before the client connects.  Defaults to MQTT_PROTOCOL_V311.
 *
 *			MOSQ_OPT_RECEIVE_MAXIMUM - Value can be set between 1 and 65535 inclusive, and represents the maximum number of
 *			incoming QoS 1 and QoS 2 messages that this client wants to process at once.
 *			Defaults to 20.  This option is not valid for MQTT v3.1 or v3.1.1 clients.
 *			Note that if the MQTT_PROP_RECEIVE_MAXIMUM property is in the proplist passed to mosquitto_connect_v5(),
 *			then that property will override this option.  Using this option is the recommended method however.
 *
 *			MOSQ_OPT_SEND_MAXIMUM - Value can be set between 1 and 65535 inclusive, and represents the maximum number of
 *			outgoing QoS 1 and QoS 2 messages that this client will attempt to have “in flight” at once.
 *			Defaults to 20.  This option is not valid for MQTT v3.1 or v3.1.1 clients.  Note that if the broker being connected to
 *			sends a MQTT_PROP_RECEIVE_MAXIMUM property that has a lower value than this option, then the broker provided value will be used.
 *
 *			MOSQ_OPT_SSL_CTX_WITH_DEFAULTS - If value is set to a non zero value, then the user specified SSL_CTX passed in using
 *			MOSQ_OPT_SSL_CTX will have the default options applied to it.  This means that you only need to change the values that
 *			are relevant to you.  If you use this option then you must configure the TLS options as normal, i.e.  you should use
 *			mosquitto_tls_set to configure the cafile/capath as a minimum.  This option is only available for openssl 1.1.0 and higher.
 *
 *			MOSQ_OPT_TLS_OCSP_REQUIRED - Set whether OCSP checking on TLS connections is required.  Set to 1 to enable checking,
 *			or 0 (the default) for no checking.
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_int_option(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	/*
	if (!strncasecmp(connection->protocol_version, "V311", 4)) {
		protocol_version = MQTT_PROTOCOL_V311;
	}
	*/

	/*
	* mosq	A valid mosquitto instance.
	* option	The option to set.
	* value	The option specific value.
	*/
	rc = mosquitto_int_option(connection->mosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V311);
	log(SWITCH_LOG_DEBUG, "mosquitto_init_option() for Profile %s connection %s protocol version %s rc %d\n", connection->profile_name, connection->name, connection->protocol_version, rc);

	/*
	rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_RECEIVE_MAXIMUM, connection->receive_maximum);
	rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_SEND_MAXIMUM, connection->send_maximum);
	rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_SSL_CTX_WITH_DEFAULTS, connection->ssl_ctx_with_defaults);
	rc = mosquitto_init_option(connection->mosq, MOSQ_OPT_TLS_OCSP_REQUIRED, connection->tls_ocsp_required);
	*/

	return status;
}


/**
 * @brief   This routine is called to set the reconnect options for the connection
 *
 * @details This routine sets the reconnect_delay, reconnect_delay_max and reconnect_exponential_backup values for the connection
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_reconnect_delay_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	rc = mosquitto_reconnect_delay_set(connection->mosq, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff);
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_DEBUG, "Succeeded setting reconnect delay for profile %s connection %s delay %d delay_max %d backoff %s\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled");
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_DEBUG, "Failed setting reconnect delay for profile %s connection %s delay %d delay_max %d backoff %s invalid parameters\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled");
			return SWITCH_STATUS_GENERR;
		default:
			log(SWITCH_LOG_DEBUG, "Failed setting reconnect delay for profile %s connection %s delay %d delay_max %d backoff %s unknown return code %d\n", connection->profile_name, connection->name, connection->reconnect_delay, connection->reconnect_delay_max, connection->reconnect_exponential_backoff ? "enabled" : "disabled", rc);
			return SWITCH_STATUS_GENERR;
	}

	return status;
}


/**
 * @brief   This routine sets the number of times that a message will be retried (if unsuccessful)
 *
 * @details This routine sets the message_retry count to the user specified value
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_message_retry_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (connection->message_retry > 0) {
		mosquitto_message_retry_set(connection->mosq, connection->message_retry);
		log(SWITCH_LOG_DEBUG, "Message retry set to %d for profile %s connection %s\n", connection->message_retry, connection->profile_name, connection->name);
	}

	return status;
}


/**
 * @brief   This routine sets the number of inflight messages to an MQTT broker
 *
 * @details This routine sets the mad_inflight_messages count to the user specified value
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_max_inflight_messages_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (connection->max_inflight_messages > 0) {
		int rc = mosquitto_max_inflight_messages_set(connection->mosq, connection->max_inflight_messages);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(SWITCH_LOG_DEBUG, "Max inflight messages set to %d for profile %s connection %s\n", connection->max_inflight_messages, connection->profile_name, connection->name);
				break;
			case MOSQ_ERR_INVAL:
				log(SWITCH_LOG_DEBUG, "Max inflight messages set to %d for profile %s connection %s resulted in invalid parameter input\n", connection->max_inflight_messages, connection->profile_name, connection->name);
				return SWITCH_STATUS_GENERR;
		}
	}

	return status;
}

/**
 * @brief   This routine sets the username and password used to connect to the MQTT broker
 *
 * @details Configure username and password for a mosquitto instance.
 *			By default, no username or password will be sent.
 *			For v3.1 and v3.1.1 clients, if username is NULL, the password argument is ignored.
 *			This is must be called before calling mosquitto_connect.
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_username_pw_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (connection->username && connection->password) {
		/*
		* mosq		A valid mosquitto instance.
		* username	The username to send as a string, or NULL to disable authentication.
		* password	The password to send as a string.  Set to NULL when username is valid in order to send just a username.
		*/
		int rc = mosquitto_username_pw_set(connection->mosq, connection->username, connection->password);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(SWITCH_LOG_DEBUG, "Client username set to %s\n", connection->username);
				break;
			case MOSQ_ERR_INVAL:
				log(SWITCH_LOG_ERROR, "Setting username/pw %s failed invalid parameters\n", connection->username);
				return SWITCH_STATUS_GENERR;
			case MOSQ_ERR_NOMEM:
				log(SWITCH_LOG_ERROR, "Setting username/pw %s failed out of memory\n", connection->username);
				return SWITCH_STATUS_GENERR;
			default:
				log(SWITCH_LOG_ERROR, "Setting username/pw %s unknown return code %d\n", connection->username, rc);
				return SWITCH_STATUS_GENERR;
		}
	}
	return status;
}


/*
 * @brief	This routine configures the client for certificate based SSL/TLS support.
 *
 * @details	The TLS configuration options specified in the 'connection' section define various
 *			parameters related to setting up a TLS connection to a broker.  This routine takes
 *			the settings and add them to the client mosq structure (prior to connecting to a broker).
 *
 * @param[in]	*connection Pointer to a connections structure
 *
 * @retval	status returned by the mosquitto library of the attempt to set the TLS parameters
 */

switch_status_t mosq_tls_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Cannot execute mosquitto_tls_set because connection name is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	/*
	* mosq		a valid mosquitto instance.
	* cafile		path to a file containing the PEM encoded trusted CA certificate files.  Either cafile or capath must not be NULL.
	* capath		path to a directory containing the PEM encoded trusted CA certificate files.
	*				See mosquitto.conf for more details on configuring this directory.  Either cafile or capath must not be NULL.
	* certfile	path to a file containing the PEM encoded certificate file for this client.
	*				If NULL, keyfile must also be NULL and no client certificate will be used.
	* keyfile		path to a file containing the PEM encoded private key for this client.
	*				If NULL, certfile must also be NULL and no client certificate will be used.
	* pw_callback	if keyfile is encrypted, set pw_callback to allow your client to pass the correct password for decryption.
	*				If set to NULL, the password must be entered on the command line.
	*				Your callback must write the password into “buf”, which is “size” bytes long.
	*				The return value must be the length of the password.  “userdata” will be set to the calling mosquitto instance.
	*				The mosquitto userdata member variable can be retrieved using mosquitto_userdata.
	*/
	rc = mosquitto_tls_set(connection->mosq, connection->tls.cafile, connection->tls.capath, connection->tls.certfile, connection->tls.certfile, NULL);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_INFO, "mosquitto_tls_set TLS profile: %s connection: %s %s:%d\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_set profile: %s connection: %s %s:%d input parameters were invalid\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_set profile: %s connection: %s %s:%d out of memory condition occurred\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
	}

	if (connection->tls.advanced_options == SWITCH_TRUE) {
		mosq_tls_opts_set(connection);
	}

	return status;
}


/*
 * @brief	This routine configures a client for pre-shared-key based TLS support
 *
 * @details	Configure the client for pre-shared-key based TLS support.
 *			Must be called before mosquitto_connect.
 *			Cannot be used in conjunction with mosquitto_tls_set.
 *
 * @param[in]	*connection Pointer to a connections structure
 *
 * @retval	status returned by the mosquitto library of the attempt to set the pre-shared-key based TLS parameters
 */

switch_status_t mosq_tls_psk_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Cannot execute mosquitto_tls_psk_set because connection name is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	/*
	* mosq		a valid mosquitto instance.
	* psk			the pre-shared-key in hex format with no leading “0x”.
	* identity	the identity of this client.  May be used as the username depending on the server settings.
	* ciphers		a string describing the PSK ciphers available for use.
	*				See the “openssl ciphers” tool for more information.  If NULL, the default ciphers will be used.
	*/
	rc = mosquitto_tls_psk_set(connection->mosq, connection->tls.psk, connection->tls.identity, connection->tls.psk_ciphers);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_INFO, "mosquitto_tls_psk_set profile: %s connection %s %s:%d\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_psk_set: profile: %s connection %s %s:%d input parameters were invalid\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_psk_set: profile: %s connection %s %s:%d out of memory condition occurred\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
	}


	if (connection->tls.advanced_options == SWITCH_TRUE) {
		mosq_tls_opts_set(connection);
	}

	return status;
}


/*
 * @brief	This routine configures a client for advacned SSL/TLS options.
 *
 * @details	Configure the client for advanced SSL/TLS options.
 *			Must be called before mosquitto_connect.
 *
 * @param[in]	*connection Pointer to a connection structure
 *
 * @retval	status returned by the mosquitto library of the attempt to set advanced SSL/TLS options.
 */

switch_status_t mosq_tls_opts_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Cannot execute mosquitto_tls_opts_set because connection name is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	/*
	* mosq		a valid mosquitto instance.
	* cert_reqs	an integer defining the verification requirements the client will impose on the server.  This can be one of:
	*				SSL_VERIFY_NONE (0): the server will not be verified in any way.
	*				SSL_VERIFY_PEER (1): the server certificate will be verified and the connection aborted if the verification fails.
	*				The default and recommended value is SSL_VERIFY_PEER.  Using SSL_VERIFY_NONE provides no security.
	* version		the version of the SSL/TLS protocol to use as a string.  If NULL, the default value is used.  The default value and the available values depend on the version of openssl that the library was compiled against.  For openssl >= 1.0.1, the available options are tlsv1.2, tlsv1.1 and tlsv1, with tlv1.2 as the default.  For openssl < 1.0.1, only tlsv1 is available.
	* ciphers		a string describing the ciphers available for use.  See the “openssl ciphers” tool for more information.  If NULL, the default ciphers will be used.
	*/
	rc = mosquitto_tls_opts_set(connection->mosq, connection->tls.cert_reqs, connection->tls.version, connection->tls.opts_ciphers);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_INFO, "mosquitto_tls_opts_set profile: %s connection: %s %s:%d\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_opts_set: profile: %s connection %s %s:%d input parameters were invalid\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_ERROR, "mosquitto_tls_opts_set: profile: %s connection %s %s:%d out of memory condition occurred\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
	}

	return status;
}


/*
 * @brief	This routine configures a Last Will and Testament (will) for a client.
 *
 * @details	Configure will information for a mosquitto instance.
 *		  By default, clients do not have a will.
 *		  This must be called before calling mosquitto_connect.
 *
 * @param[in]	*connection Pointer to a connection structure
 *
 * @retval	status returned by the mosquitto library of the attempt to set a will.
 */

switch_status_t mosq_will_set(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;
	int payloadlen = 0;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Cannot execute mosquitto_will_set() because connection name is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	payloadlen = strlen(connection->will.payload);

	/*
	* mosq		A valid mosquitto instance.
	* topic		The topic on which to publish the will.
	* payloadlen	The size of the payload (bytes).
	*			 Valid values are between 0 and 268,435,455.
	* payload		Pointer to the data to send.
	*				If payloadlen > 0 this must be a valid memory location.
	* qos			Integer value 0, 1 or 2 indicating the Quality of Service to be used for the will.
	* retain		Set to true to make the will a retained message.
	*/
	rc = mosquitto_will_set(connection->mosq, connection->will.topic, payloadlen, connection->will.payload, connection->will.qos, connection->will.retain);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_INFO, "mosquitto_will_set profile %s connection: %s %s:%d\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "mosquitto_will_set profile %s connection %s %s:%d input parameters were invalid\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_ERROR, "mosquitto_will_set profile %s connection %s %s:%d out of memory condition occurred\n", connection->profile_name, connection->name, connection->host, connection->port);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_PAYLOAD_SIZE:
			log(SWITCH_LOG_ERROR, "mosquitto_will_set profile %s connection %s %s:%d payload size %d is too large\n", connection->profile_name, connection->name, connection->host, connection->port, payloadlen);
			status = SWITCH_STATUS_GENERR;
			return status;
		case MOSQ_ERR_MALFORMED_UTF8:
			log(SWITCH_LOG_ERROR, "mosquitto_will_set profile %s connection %s %s:%d the topic %s is not valid UTF-8\n", connection->profile_name, connection->name, connection->host, connection->port, connection->will.topic);
			status = SWITCH_STATUS_GENERR;
			return status;
	}

	return status;
}


/**
 * @brief   This routine performs the actual connect attempt to an MQTT broker
 *
 * @details This routine uses the configuration defined settings to attempt a connection with an MQTT broker.
 *			SRV lookups are supported, also is the setting of the local bind address
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_connect(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int rc;
	int loop;
	unsigned port;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "mosq_connect() failed because connection name is NULL\n");
		return SWITCH_STATUS_GENERR;
	}

	mosq_callbacks_set(connection);

	connection->connected = SWITCH_FALSE;
	connection->retry_count = 0;
	port = connection->port;

	if (connection->tls.enable) {
		if (!strncasecmp(connection->tls.support, "certificate", 11)) {
			mosq_tls_set(connection);
		} else if (!strncasecmp(connection->tls.support, "psk", 3)) {
			mosq_tls_psk_set(connection);
		}
		if (connection->tls.port) {
			port = connection->tls.port;
		}
	}

	if (connection->will.enable) {
		mosq_will_set(connection);
	} else {
		mosquitto_will_clear(connection->mosq);
	}

	if (!connection->bind_address) {
		/*
		* mosq		A valid mosquitto instance.
		* host		The hostname or ip address of the broker to connect to.
		* port		The network port to connect to.  Usually 1883.
		* keepalive	The number of seconds after which the broker should send a PING message to the client
		*				if no other messages have been exchanged in that time.
		*/
		rc = mosquitto_connect(connection->mosq, connection->host, port, connection->keepalive);
	} else {
		if (!connection->srv) {
			/*
			* mosq			A valid mosquitto instance.
			* host			The hostname or ip address of the broker to connect to.
			* port			The network port to connect to.  Usually 1883.
			* keepalive		The number of seconds after which the broker should send a PING message to the client
			*					if no other messages have been exchanged in that time.
			* bind_address	The hostname or ip address of the local network interface to bind to.
			*/
			rc = mosquitto_connect_bind(connection->mosq, connection->host, port, connection->keepalive, connection->bind_address);
		} else {
			rc = mosquitto_connect_srv(connection->mosq, connection->host, connection->keepalive, connection->bind_address);
		}
	}
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_DEBUG, "Attempting to connect to profile %s %s:%d keepalive:%d bind_address:%s SRV: %s\n", connection->profile_name, connection->host, port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled");
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "Failed connection to profile %s %s:%d keepalive:%d bind_address:%s SRV: %s with invalid parameters\n", connection->profile_name, connection->host, port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled");
			mosq_destroy(connection);
			return SWITCH_STATUS_GENERR;
		case MOSQ_ERR_ERRNO:
			mosq_destroy(connection);
			return SWITCH_STATUS_GENERR;
		default:
			log(SWITCH_LOG_ERROR, "Failed connection to profile %s %s:%d keepalive:%d bind_address: %s SRV: %s unknown return code %d\n", connection->profile_name, connection->host, port, connection->keepalive, connection->bind_address, connection->srv ? "enabled" : "disabled", rc);
			mosq_destroy(connection);
	}

	loop = mosquitto_loop_start(connection->mosq);
	if (loop != MOSQ_ERR_SUCCESS) {
		log(SWITCH_LOG_ERROR, "Unable to start loop %i\n", loop);
	}

	return status;
}


/**
 * @brief   This routine stops the send/receive loop associated with a connection
 *
 * @details This routine is called after a connection disconnect request to stop the the send/receive loop
 *
 * @param[in]   *connection	Pointer to a connection structure
 * @param[in]   *force		A flag to stop the loop even if a disconnect has not been performed
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_loop_stop(mosquitto_connection_t *connection, switch_bool_t force)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	/*
	* mosq	A valid mosquitto instance.
	* force	Set to true to force thread cancellation.
	*			If false, mosquitto_disconnect must have already been called.
	*/
	int rc = mosquitto_loop_stop(connection->mosq, force);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_INFO, "Shutting down profile %s connection: %s mosquitto_loop_stop() %d successful\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_SUCCESS;
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_INFO, "Shutting down profile %s connection: %s mosquitto_loop_stop() %d input parameters were invalid\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NOT_SUPPORTED:
			log(SWITCH_LOG_INFO, "Shutting down profile %s connection: %si mosquitto_loop_stop() %d thread support is not available\n", connection->profile_name, connection->name, rc);
			status = SWITCH_STATUS_GENERR;
			break;
	}

	return status;
}

/**
 * @brief   This routine disconnects a connection
 *
 * @details This routine dssconnects a connection, stops the mosquitto send/receive loop and clears the pointer to the mosquitto client structure
 *
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_disconnect(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "mosq_disconnect() called with NULL connection\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection->connected) {
		log(SWITCH_LOG_ERROR, "Tried to disconnect a connection that is NOT connected\n");
	} else {
		int rc = mosquitto_disconnect(connection->mosq);
		switch (rc) {
			case MOSQ_ERR_SUCCESS:
				log(SWITCH_LOG_DEBUG, "Disconnected profile %s connection %s from the broker\n", connection->profile_name, connection->name);
				connection->connected = SWITCH_FALSE;
				status = SWITCH_STATUS_SUCCESS;
				break;
			case MOSQ_ERR_INVAL:
				log(SWITCH_LOG_DEBUG, "Disconnection for profile %s connection %s returned: input parameters were invalid \n", connection->profile_name, connection->name);
				status = SWITCH_STATUS_GENERR;
				break;
			case MOSQ_ERR_NO_CONN:
				log(SWITCH_LOG_DEBUG, "Tried to disconnect profile %s connection %s but there was no connection to the broker\n", connection->profile_name, connection->name);
				connection->connected = SWITCH_FALSE;
				status = SWITCH_STATUS_GENERR;
				break;
			default:
				log(SWITCH_LOG_DEBUG, "Tried to disconnect profile %s connection %s, received an unknown return code %d\n", connection->profile_name, connection->name, rc);
				connection->connected = SWITCH_FALSE;
				status = SWITCH_STATUS_GENERR;
		}
	}
	mosq_loop_stop(connection, SWITCH_TRUE);
	connection->connected = SWITCH_FALSE;
	return status;
}


/**
 * @brief   This routine creates a new mosquitto client data structure
 *
 * @details This routine sets up the userdata structure associated with this client connection
 *
 * @param[in]   *profile	Pointer to the profile associated to with this connection
 * @param[in]   *connection	Pointer to a connection structure
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_new(mosquitto_profile_t *profile, mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_mosq_userdata_t *userdata = NULL;
	switch_bool_t clean_session = SWITCH_TRUE;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "mosq_new() called with NULL connection\n");
		return SWITCH_STATUS_GENERR;
	}

	if (connection->mosq) {
		log(SWITCH_LOG_DEBUG, "mosq_new() called, but the connection has an existing mosq structure exiting\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!profile) {
		log(SWITCH_LOG_ERROR, "mosq_new() called with NULL profile\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!profile->enable || !connection->enable) {
		log(SWITCH_LOG_DEBUG, "mosq_new() Profile %s %s connection %s %s\n", profile->name, profile->enable ? "enabled" : "disabled", connection->name, connection->enable ? "enabled" : "disabled");
		return SWITCH_STATUS_SUCCESS;
	}

	/*
	if (!(userdata = (mosquitto_mosq_userdata_t *)switch_core_alloc(profile->pool, sizeof(mosquitto_mosq_userdata_t)))) {
		log(SWITCH_LOG_CRIT, "mosq_new() Failed to allocate memory for mosquitto_new() userdata structure profile %s connection %s\n", profile->name, connection->name);
		return SWITCH_STATUS_GENERR;
	} else {
		connection->userdata = userdata;
		userdata->profile = profile;
		userdata->connection = connection;
	}
	*/

	switch_malloc(userdata, sizeof(mosquitto_mosq_userdata_t));
	connection->userdata = userdata;
	userdata->profile = profile;
	userdata->connection = connection;

	if (connection->client_id == NULL) {
		if (connection->clean_session == SWITCH_FALSE) {
			log(SWITCH_LOG_INFO, "mosquitto_new() profile %s connection %s called with NULL client_id, forcing clean_session to TRUE\n", profile->name, connection->name);
			clean_session = SWITCH_TRUE;
		}
	}
	clean_session = connection->clean_session;

	log(SWITCH_LOG_DEBUG, "mosquitto_new() being called with profile %s connection %s clean_session %s client_id %s\n", profile->name, connection->name, clean_session ? "True" : "False", connection->client_id);

	/*
	* id				String to use as the client id.  If NULL, a random client id will be generated.  If id is NULL, clean_session must be true.
	* clean_session	Set to true to instruct the broker to clean all messages and subscriptions on disconnect, false to instruct it to keep them.
	*					See the man page mqtt(7) for more details.  Note that a client will never discard its own outgoing messages on disconnect.
	*					Calling mosquitto_connect or mosquitto_reconnect will cause the messages to be resent.
	*					Use mosquitto_reinitialise to reset a client to its original state.  Must be set to true if the id parameter is NULL.
	* obj				A user pointer that will be passed as an argument to any callbacks that are specified.
	*/
	connection->mosq = mosquitto_new(connection->client_id, clean_session, userdata);

	if (connection->mosq == NULL) {
		switch (errno) {
			case ENOMEM:
				log(SWITCH_LOG_ERROR, "mosquitto_new(%s, %d, NULL) out of memory\n", connection->client_id, connection->clean_session);
				return SWITCH_STATUS_GENERR;
			case EINVAL:
				log(SWITCH_LOG_ERROR, "mosquitto_new(%s, %d, NULL) invalid input parameters\n", connection->client_id, connection->clean_session);
				return SWITCH_STATUS_GENERR;
			default:
				log(SWITCH_LOG_ERROR, "mosquitto_new(%s, %d, NULL) errno(%s)\n", connection->client_id, connection->clean_session, mosquitto_strerror(errno));
				return SWITCH_STATUS_GENERR;
		}
	}

	return status;
}


/**
 * @brief   This routine frees memory associated with a mosquitto client instance
 *
 * @details This routine frees memowy associated with a mosquitto client instance and also frees memory allocated 
 *          for the userdata structure associated with this client instance
 *
 * @param[in]   *mosq	Pointer to the mosquitto client instance
 *
 * @retval	SWITCH_STATUS_SUCCESS
 */

switch_status_t mosq_destroy(mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_mosq_userdata_t *userdata = NULL;
	mosquitto_profile_t *profile = NULL;

	if (!connection) {
		log(SWITCH_LOG_ERROR, "mosq_destroy() called with NULL connection\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection->mosq) {
		log(SWITCH_LOG_ERROR, "mosq_destroy() called with NULL mosquitto client instance\n");
		return SWITCH_STATUS_GENERR;
	}
	
	userdata = mosquitto_userdata(connection->mosq);
	if (!userdata) {
		log(SWITCH_LOG_ERROR, "mosq_destroy() called with NULL userdata pointer\n");
		return SWITCH_STATUS_GENERR;
	}

	profile = (mosquitto_profile_t *)userdata->profile;

	log(SWITCH_LOG_DEBUG, "mosq_destroy(): profile %s connection %s\n", profile->name, connection->name);

	switch_safe_free(userdata);
	mosquitto_destroy(connection->mosq);
	connection->userdata = NULL;
	connection->mosq = NULL;
	connection->connected = SWITCH_FALSE;


	return status;
}


/**
 * @brief   This routine creates a new subscription
 *
 * @details This routine performs some sanity checks and then attempts to subscribe to a topic (pattern)
 *
 * @param[in]   *profile	Pointer to the profile associated to with this subscription
 * @param[in]   *subscriber	Pointer to the subscriber associated with this subscription
 * @param[in]   *topic		Pointer to the topic being subscribed to
 *
 * @retval	SWITCH_STATUS_SUCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t mosq_subscribe(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_connection_t *connection;
	int rc;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to mosq_subscribe()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!subscriber) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber not passed to mosq_subscribe()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber %s topic not passed to mosq_subscribe()\n", profile->name, subscriber->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!(connection = locate_connection(profile, topic->connection_name))) {
		log(SWITCH_LOG_ERROR, "Cannot subscribe to topic %s because connection %s (profile %s) is invalid\n", topic->name, topic->connection_name, profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!connection->mosq) {
		if (connection->enable) {
			log(SWITCH_LOG_WARNING, "Trying to initialize the connection\n");
			connection_initialize(profile, connection);
		} else {
			log(SWITCH_LOG_ERROR, "Cannot subscribe to topic %s because connection %s (profile %s) mosq is NULL\n", topic->name, connection->name, profile->name)
			return SWITCH_STATUS_GENERR;
		}
	}

	/*
	* mosq		A valid mosquitto instance.
	*	mid			A pointer to an int.  If not NULL, the function will set this to the message id of this particular message.
	*				This can be then used with the subscribe callback to determine when the message has been sent.
	*	sub			The subscription pattern.
	* qos			The requested Quality of Service for this subscription.
	*/
	rc = mosquitto_subscribe(connection->mosq, &topic->mid, topic->pattern, topic->qos);

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			topic->subscribed = SWITCH_TRUE;
			log(SWITCH_LOG_DEBUG, "Profile %s connection %s subscriber %s topic %s pattern %s message id %d queued\n",
				profile->name, connection->name, subscriber->name, topic->name, topic->pattern, topic->mid);
			mosquitto_log(SWITCH_LOG_DEBUG, "Profile %s connection %s subscriber %s topic %s pattern %s message id %d queued\n",
				profile->name, connection->name, subscriber->name, topic->name, topic->pattern, topic->mid);
			profile_log(SWITCH_LOG_DEBUG, profile, "Profile %s connection %s subscriber %s topic %s pattern %s message id %d queued\n",
				profile->name, connection->name, subscriber->name, topic->name, topic->pattern, topic->mid);
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_ERROR, "Profile %s subscriber %s connection %s topic %s pattern %s the input parameters were invalid\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_CRIT, "Profile %s subscriber %s connection %s topic %s pattern %s an out of memory condition occurred\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_NO_CONN:
			log(SWITCH_LOG_WARNING, "Profile %s subscriber %s connection %s topic %s pattern %s not connected to an MQTT broker\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_MALFORMED_UTF8:
			log(SWITCH_LOG_ERROR, "Profile %s subscriber %s connection %s topic %s pattern %s the pattern is not valid UTF-8\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
		case MOSQ_ERR_OVERSIZE_PACKET:
			log(SWITCH_LOG_ERROR, "Profile %s subscriber %s connection %s topic %s pattern %s the pattern larger than the MQTT broker can support\n", profile->name, subscriber->name, connection->name, topic->name, topic->pattern);
			topic->subscribed = SWITCH_FALSE;
			return SWITCH_STATUS_GENERR;
			break;
	}

	return status;
}


/**
 * @brief   This routine handle responses from publish requests
 *
 * @details This routine logs the results of publish requests and may attempt to fix the cause of a failure
 *			such as reinitializing a connection
 *
 * @param[in]   *profile	Pointer to the profile associated to with this subscription
 * @param[in]   *subscriber	Pointer to the subscriber associated with this subscription
 * @param[in]   *topic		Pointer to the topic being subscribed to
 * @param[in]   *rc			Return code from the publish request
 */

void mosq_publish_results(mosquitto_profile_t *profile, mosquitto_connection_t *connection, mosquitto_topic_t *topic, int rc)
{

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to mosq_publish_results()\n");
		return;
	}

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Profile %s connection not passed to mosq_publish_results()\n", profile->name);
		return;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s connection %s topic not passed to mosq_publish_results()\n", profile->name, connection->name);
		return;
	}

	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(SWITCH_LOG_DEBUG, "Event handler: published to %s %s %s\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_INVAL:
			log(SWITCH_LOG_WARNING, "Event handler: failed to publish to %s %s %s invalid input parameters\n", profile->name, connection->name, topic->pattern);
			if (connection->enable) {
				log(SWITCH_LOG_WARNING, "Event handler: failed to publish to %s %s %s trying to initialize the connection\n", profile->name, connection->name, topic->pattern);
				connection_initialize(profile, connection);
			}
			break;
		case MOSQ_ERR_NOMEM:
			log(SWITCH_LOG_DEBUG, "Event handler: failed to publish to %s %s %s out of memory\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_NO_CONN:
			log(SWITCH_LOG_DEBUG, "Event handler: failed to publish to %s %s %s not connected to broker\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_PROTOCOL:
			log(SWITCH_LOG_DEBUG, "Event handler: failed to publish to %s %s %s protocol error communicating with the broker\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_PAYLOAD_SIZE:
			log(SWITCH_LOG_DEBUG, "Event handler: failed to publish to %s %s %s payload is too large\n", profile->name, connection->name, topic->pattern);
			break;
		default:
			log(SWITCH_LOG_DEBUG, "Event handler: unknown return code %d from publish\n", rc);
			break;
	}
}


/**
 * @brief   This routine handles mod_mosquitto startup processing
 *
 * @details This routine is called by the mod_mosquitto initialization function
 *
 */

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
	log(SWITCH_LOG_DEBUG, "Mosquitto library rc=%d, version %d.%d.%d initialized\n", rc, major, minor, revision);

	mosquitto_globals.mosquitto_lib.major = major;
	mosquitto_globals.mosquitto_lib.minor = minor;
	mosquitto_globals.mosquitto_lib.revision = revision;
	switch_mutex_unlock(mosquitto_globals.mutex);

	switch_mutex_init(&mosquitto_globals.log.mutex, SWITCH_MUTEX_DEFAULT, mosquitto_globals.pool);
	status = switch_file_open(&mosquitto_globals.log.logfile, mosquitto_globals.log.name, SWITCH_FOPEN_WRITE|SWITCH_FOPEN_APPEND|SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, mosquitto_globals.pool);
	if (status != SWITCH_STATUS_SUCCESS) {
		log(SWITCH_LOG_ERROR, "Failed to open mosquitto log %s\n", mosquitto_globals.log.name);
		return SWITCH_STATUS_FALSE;
	}

	status = initialize_profiles();

	return status;
}


/**
 * @brief   This routine handles mod_mosquitto termination processing
 *
 * @details This routine is called by the mod_mosquitto shutdown function
 *
 */

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

	switch_mutex_lock(mosquitto_globals.log.mutex);
	if (mosquitto_globals.log.logfile == NULL) {
		log(SWITCH_LOG_ERROR, "Unable to close %s file handle is NULL\n", mosquitto_globals.log.name);
	} else if ((status = switch_file_close(mosquitto_globals.log.logfile)) != SWITCH_STATUS_SUCCESS) {
		log(SWITCH_LOG_ERROR, "Failed to close %s\n", mosquitto_globals.log.name);
	}
	switch_mutex_unlock(mosquitto_globals.log.mutex);
	switch_mutex_destroy(mosquitto_globals.log.mutex);

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
