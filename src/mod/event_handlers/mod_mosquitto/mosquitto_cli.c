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

#include "mod_mosquitto.h"
#include "mosquitto_cli.h"
#include "mosquitto_config.h"
#include "mosquitto_utils.h"
#include "mosquitto_mosq.h"

#define MOSQUITTO_DESC "Mosquitto API"
#define MOSQUITTO_SYNTAX "<command> [<args>]"

typedef switch_status_t (*mosquitto_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

/**
 * \brief	This function is called by the fs_cli command: mosquitto bgapi <command> [<arg>]
 *
 * \details	The bgapi command will be executed in a new thread. Note that the output of the command will
 *			not be displayed on the console.  This is consistent with how the fs_cli bgapi command works.
 *			This command is primarily used for testing both the mod_mosquitto bgapi_exec routine and
 *			the BACKGROUND_JOB event that results.  BACKGROUND_JOB is an event that can be published to
 *			a MQTT broker.
 *
 * \param[in]	*cmd	command entered into the fs_api console
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the routine
 *
 */

static switch_status_t cmd_bgapi(const char *cmd, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	mosquitto_bgapi_job_t *job = NULL;
	switch_uuid_t uuid;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	const char *p, *arg = cmd;
	char my_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";

	if (!cmd) {
		stream->write_function(stream, "-ERR Invalid syntax cmd empty\n");
		return status;
	}

	if (!strncasecmp(cmd, "uuid:", 5)) {
		p = cmd + 5;
		if ((arg = strchr(p, ' ')) && *arg++) {
			switch_copy_string(my_uuid, p, arg - p);
		}
	}

	if (zstr(arg)) {
		stream->write_function(stream, "-ERR Invalid syntax arg empty\n");
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
	stream->write_function(stream, "+OK Job-UUID: %s\n", job->uuid_str);
	switch_thread_create(&thread, thd_attr, bgapi_exec, job, job->pool);

	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto loglevel [debug|info|notice|warning|error|critical|alert|console]
 *
 * \details	This function dynamically changes the level of logging that mod_mosquitto performs.
 *			The levels are listed in increasing severity (decreasing volume of messages logged).
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_loglevel(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	if (argc == 1) {
		stream->write_function(stream, "mosquitto loglevel: %s\n", switch_log_level2str(mosquitto_globals.loglevel));
		return status;
	}

	if (argc >= 2) {
		if (!strncasecmp(argv[1], "debug", 5)) {
			mosquitto_globals.loglevel = SWITCH_LOG_DEBUG;
		} else if (!strncasecmp(argv[1], "info", 4)) {
			mosquitto_globals.loglevel = SWITCH_LOG_INFO;
		} else if (!strncasecmp(argv[1], "notice", 6)) {
			mosquitto_globals.loglevel = SWITCH_LOG_NOTICE;
		} else if (!strncasecmp(argv[1], "warning", 7)) {
			mosquitto_globals.loglevel = SWITCH_LOG_WARNING;
		} else if (!strncasecmp(argv[1], "error", 5)) {
			mosquitto_globals.loglevel = SWITCH_LOG_ERROR;
		} else if (!strncasecmp(argv[1], "critical", 8)) {
			mosquitto_globals.loglevel = SWITCH_LOG_CRIT;
		} else if (!strncasecmp(argv[1], "alert", 5)) {
			mosquitto_globals.loglevel = SWITCH_LOG_ALERT;
		} else if (!strncasecmp(argv[1], "console", 7)) {
			mosquitto_globals.loglevel = SWITCH_LOG_CONSOLE;
		}
		stream->write_function(stream, "mosquitto loglevel set to: %s\n", switch_log_level2str(mosquitto_globals.loglevel));
	}

	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto enable|disable [profile|connection|publisher|subscriber] <name>
 *
 * \details	This function is used to both enable or disable entries associated with the primary hashes that mod_mosquitto uses.
 *			The logic is currently similar for both operations to one function was written to handle them both.  If their operation
 *			diverges in the future, it may be reasonable to split this function into both enable/disable versions.
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_enable_disable(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;
	mosquitto_publisher_t *publisher = NULL;
	mosquitto_subscriber_t *subscriber = NULL;

	char *enable_disable = NULL;
	switch_bool_t enable_disable_bool = SWITCH_FALSE;

	enable_disable = strncasecmp(argv[0], "enable", 6) ? "disable" : "enable";
	enable_disable_bool = strncasecmp(argv[0], "enable", 6) ? SWITCH_FALSE : SWITCH_TRUE;

	if (argc == 2) {
		stream->write_function(stream, "mosquitto %s command requires: profile <name>\n", enable_disable);
		return status;
	}

	if (argc == 3) {
		if (!strncasecmp(argv[1], "profile", 7)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto %s profile %s failed: profile not found\n", enable_disable, argv[2]);
			} else {
				stream->write_function(stream, "mosquitto %s profile %s successful\n", enable_disable, argv[2]);
				profile->enable = enable_disable_bool;
			}
		}
	}

	if (argc == 4) {
		stream->write_function(stream, "mosquitto %s command requires: profile <name> [connection|publisher|subscriber] <name>\n", enable_disable);
		return status;
	}

	if (argc > 4) {
		if (!strncasecmp(argv[1], "profile", 7)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto %s profile %s failed: profile not found\n", enable_disable, argv[2]);
			} else {
				if (!strncasecmp(argv[3], "connection", 10)) {
					if (!(connection = locate_connection(profile, argv[4]))) {
						stream->write_function(stream, "mosquitto %s profile %s connection %s failed: connection not found\n", enable_disable, argv[2], argv[4]);
					} else {
						stream->write_function(stream, "mosquitto %s profile %s connection %s successful\n", enable_disable, argv[2], argv[4]);
						connection->enable = enable_disable_bool;
						connection_initialize(profile, connection);
					}
				} else if (!strncasecmp(argv[3], "publisher", 9)) {
					if (!(publisher = locate_publisher(profile, argv[4]))) {
						stream->write_function(stream, "mosquitto %s profile %s publisher %s failed: publisher not found\n", enable_disable, argv[2], argv[4]);
					} else {
						stream->write_function(stream, "mosquitto %s profile %s publisher %s successful\n", enable_disable, argv[2], argv[4]);
						publisher->enable = enable_disable_bool;
						publisher_activate(profile, publisher);
					}
				} else if (!strncasecmp(argv[3], "subscriber", 10)) {
					if (!(subscriber = locate_subscriber(profile, argv[4]))) {
						stream->write_function(stream, "mosquitto %s profile %s subscriber %s failed: subscriber not found\n", enable_disable, argv[2], argv[4]);
					} else {
						stream->write_function(stream, "mosquitto %s profile %s subscriber %s successful\n", enable_disable, argv[2], argv[4]);
						subscriber->enable = enable_disable_bool;
						subscriber_activate(profile, subscriber);
					}
				}
			}
		} else {
			stream->write_function(stream, "mosquitto %s command requires: profile <name> [connection|publisher|subscriber] <name>\n", enable_disable);
		}
	}
	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto connect profile <profile-name> connection <connection-name>
 *
 * \details	This function is used to connect to an MQTT broker.  The connection definitions are located within the profile hash.
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_connect(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;

	if (argc == 1 || argc == 2 || argc == 3 || argc == 4) {
		stream->write_function(stream, "mosquitto connect command requires: profile <name> connection <connection-name>\n");
		return status;
	}

	if (argc == 5) {
		if (!strncasecmp(argv[1], "profile", 7)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto connect profile %s failed: profile not found\n", argv[2]);
				return status;
			} else {
				if (!strncasecmp(argv[3], "connection", 10)) {
					if (!(connection = locate_connection(profile, argv[4]))) {
						stream->write_function(stream, "mosquitto connect profile %s connection %s failed: connection not found\n", argv[2], argv[4]);
						return status;
					}
					if (profile->enable != SWITCH_TRUE) {
						stream->write_function(stream, "mosquitto connect profile %s not enabled\n", argv[2]);
						return status;
					}
					if (connection->enable != SWITCH_TRUE) {
						stream->write_function(stream, "mosquitto connect profile %s connection %s - connection not enabled\n", argv[2], argv[4]);
						return status;
					}
					if (mosq_new(profile, connection) == SWITCH_STATUS_SUCCESS) {
						mosq_reconnect_delay_set(connection);
						mosq_message_retry_set(connection);
						mosq_max_inflight_messages_set(connection);
						mosq_username_pw_set(connection);
						if (mosq_connect(connection) == SWITCH_STATUS_SUCCESS) {
							log(INFO, "Succesfully connected to broker using profile: %s connection: %s", profile->name, connection->name);
						} else {
							log(WARNING, "Failed to connect to broker using profile: %s connection: %s", profile->name, connection->name);
						}
					}
				}
			}
		}
	}

	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto disconnect profile <profile-name> connection <connection-name>
 *
 * \details	This function is used to disconnect from an MQTT broker.  The connection definitions are located within the profile hash.
 *			The enable|disable parameter is used to change the behaviour of future connect operations.  Connect attempts will not be
 *			made to a connection that has been disabled.  This is useful for controlling any looping caused by auto retrying connect
 *			operations made to an MQTT broker that is down for more than a transient period.
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_disconnect(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;

	stream->write_function(stream, "argc %d\n", argc);
	for (int i=0; i<argc; i++) {
		stream->write_function(stream, "argv[%d]: %s\n", i, argv[i]);
	}

	if (argc <= 4) {
		stream->write_function(stream, "mosquitto disconnect command requires: profile <profile-name> connection <connection-name> [enable|disable\\n");
		return status;
	}

	if (argc <= 6) {
		if (!strncasecmp(argv[1], "profile", 7)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto disconnect profile %s failed: profile not found\n", argv[2]);
				return status;
			} else {
				if (!strncasecmp(argv[3], "connection", 10)) {
					if (!(connection = locate_connection(profile, argv[4]))) {
						stream->write_function(stream, "mosquitto disconnect profile %s connection %s failed: connection not found\n", argv[2], argv[4]);
						return status;
					}
					if ((argc == 6) && (!strncasecmp(argv[5], "enable", 6))) {
						connection->enable = SWITCH_TRUE;
					} else if ((argc == 6) && (!strncasecmp(argv[5], "disable", 7))) {
						connection->enable = SWITCH_FALSE;
					}
					if (mosq_disconnect(connection) == SWITCH_STATUS_SUCCESS) {
						log(DEBUG, "Succesfully disconnected from  broker using profile: %s connection: %s", profile->name, connection->name);
					} else {
						log(DEBUG, "Failed to disconnect from broker using profile: %s connection: %s", profile->name, connection->name);
					}
				}
			}
		}
	}

	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto remove profile <profile-name> [connection|publisher|subscriber] <name>
 *
 * \details	This function is used to remove one of the primary has types either by entry or completely. This allows for dynamic reconfiguration
 *			of the the in-memory strucures, it does not update the configuration file itself..
 *
 *	\note	Excessive use of this function will result in increased memory consumption because some data is stored in pools tied to each profile
 *			This storage is not freed until the associated memory pool is destroyed.
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_remove(char **argv, int argc, switch_stream_handle_t *stream)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;
	mosquitto_publisher_t *publisher = NULL;
	mosquitto_subscriber_t *subscriber = NULL;

	stream->write_function(stream, "argc %d\n", argc);
	for (int i=0; i<argc; i++) {
		stream->write_function(stream, "argv[%d]: %s\n", i, argv[i]);
	}

	if (argc == 1 || argc == 2) {
		stream->write_function(stream, "mosquitto remove command requires: profile <name>\n");
		return status;
	}

	if (argc == 3) {
		if (!strncasecmp(argv[1], "profile", 7)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto remove profile %s failed: profile not found\n", argv[2]);
			} else {

				switch_thread_rwlock_wrlock(profile->rwlock);
				switch_core_hash_delete_multi(profile->connections, NULL, NULL);
				switch_core_hash_delete_multi(profile->publishers, NULL, NULL);
				switch_core_hash_delete_multi(profile->subscribers, NULL, NULL);
				switch_thread_rwlock_unlock(profile->rwlock);

				switch_core_hash_delete_locked(mosquitto_globals.profiles, profile->name, mosquitto_globals.profiles_mutex);
				switch_core_destroy_memory_pool(&profile->pool);
				stream->write_function(stream, "mosquitto remove profile %s successful\n", argv[2]);
			}
		}
	}

	if (argc == 4) {
		stream->write_function(stream, "mosquitto remove command requires: profile <name> [connection|publisher|subscriber] <name>\n");
		return status;
	}

	if (argc == 5) {
		if (!strncasecmp(argv[1], "profile", 7) && !strncasecmp(argv[3], "connection", 10)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto remove profile %s connection: %s failed: profile not found\n", argv[2], argv[4]);
				return status;
			}
			if (!(connection = locate_connection(profile, argv[4]))) {
				stream->write_function(stream, "mosquitto remove profile %s connection: %s failed: connection not found\n", argv[2], argv[4]);
				return status;
			}
			switch_mutex_lock(profile->connections_mutex);
			mosq_disconnect(connection);
			mosq_loop_stop(connection, SWITCH_FALSE);
			switch_core_hash_delete(profile->connections, connection->name);
			switch_mutex_unlock(profile->connections_mutex);
			stream->write_function(stream, "mosquitto remove profile %s connection: %s completed\n", argv[2], argv[4]);
		} else if (!strncasecmp(argv[1], "profile", 7) && !strncasecmp(argv[3], "publisher", 9)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto remove profile %s publisher: %s failed: profile not found\n", argv[2], argv[4]);
				return status;
			}
			if (!(publisher = locate_publisher(profile, argv[4]))) {
				stream->write_function(stream, "mosquitto remove profile %s publisher: %s failed: connection not found\n", argv[2], argv[4]);
				return status;
			}
			switch_mutex_lock(profile->publishers_mutex);
			switch_core_hash_delete(profile->publishers, publisher->name);
			switch_mutex_unlock(profile->publishers_mutex);
			stream->write_function(stream, "mosquitto remove profile %s publisher: %s completed\n", argv[2], argv[4]);
		} else if (!strncasecmp(argv[1], "profile", 7) && !strncasecmp(argv[3], "subscriber", 10)) {
			if (!(profile = locate_profile(argv[2]))) {
				stream->write_function(stream, "mosquitto remove profile %s subscriber: %s failed: profile not found\n", argv[2], argv[4]);
				return status;
			}
			if (!(subscriber = locate_subscriber(profile, argv[4]))) {
				stream->write_function(stream, "mosquitto remove profile %s subscriber: %s failed: connection not found\n", argv[2], argv[4]);
				return status;
			}
			switch_mutex_lock(profile->subscribers_mutex);
			switch_core_hash_delete(profile->subscribers, subscriber->name);
			switch_mutex_unlock(profile->publishers_mutex);
			stream->write_function(stream, "mosquitto remove profile %s subscriber: %s completed\n", argv[2], argv[4]);
		}
	}

	return status;
}


/**
 * \brief	This function is called by the fs_cli command: mosquitto status
 *
 * \details	This function displayes on the fs_cli console a complete status of each of the primary configuration hashes.
 *
 * \note	It may be reasonable at some point to break this function down into smaller routines that focus on a single primary hash
 *			such as profile, publisher, subscriber, connection or topic.
 *
 * \param[in]	**argv	Standard C argument value list
 * \param[in]	argc	Standard C argument count
 * \param[in]	*stream	output handle used for writing messages to the fs_api console
 *
 * \retval		SWITCH_STATUS_SUCCESS	Successful completion of the command
 *
 */

static switch_status_t cmd_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	mosquitto_profile_t *profile = NULL;
	mosquitto_connection_t *connection = NULL;
	mosquitto_publisher_t *publisher = NULL;
	mosquitto_subscriber_t *subscriber = NULL;
	mosquitto_topic_t *topic = NULL;
	mosquitto_event_t *event = NULL;
	void *val;
	const char *line = "=================================================================================================";

	stream->write_function(stream, "%s\n", line);
	stream->write_function(stream, "mosquitto library version: %d.%d.%d\n", mosquitto_globals.mosquitto_lib.major, mosquitto_globals.mosquitto_lib.minor, mosquitto_globals.mosquitto_lib.revision);
	stream->write_function(stream, "settings\n");
	stream->write_function(stream, "  loglevel: %s\n", switch_log_level2str(mosquitto_globals.loglevel));
	stream->write_function(stream, "  enable-profiles: %s\n", mosquitto_globals.enable_profiles ? "True" : "False");
	stream->write_function(stream, "  enable-publishers: %s\n", mosquitto_globals.enable_publishers ? "True" : "False");
	stream->write_function(stream, "  enable-subscribers: %s\n", mosquitto_globals.enable_subscribers ? "True" : "False");
	stream->write_function(stream, "  enable-connections: %s\n", mosquitto_globals.enable_connections ? "True" : "False");
	stream->write_function(stream, "  enable-topics: %s\n", mosquitto_globals.enable_topics ? "True" : "False");
	stream->write_function(stream, "  unique-string-length: %d\n", mosquitto_globals.unique_string_length);
	stream->write_function(stream, "  event-queue-size: %d\n", mosquitto_globals.event_queue_size);

	stream->write_function(stream, "profiles\n");

	switch_mutex_lock(mosquitto_globals.mutex);
	switch_mutex_lock(mosquitto_globals.profiles_mutex);
	for (switch_hash_index_t *profiles_hi = switch_core_hash_first(mosquitto_globals.profiles); profiles_hi; profiles_hi = switch_core_hash_next(&profiles_hi)) {
		switch_core_hash_this(profiles_hi, NULL, NULL, &val);
		profile = (mosquitto_profile_t *)val;
		stream->write_function(stream, "  profile name: %s\n", profile->name);
		stream->write_function(stream, "	enable: %s\n", profile->enable ? "True" : "False");
		stream->write_function(stream, "	connections\n");
		for (switch_hash_index_t *connections_hi = switch_core_hash_first(profile->connections); connections_hi; connections_hi = switch_core_hash_next(&connections_hi)) {
			switch_core_hash_this(connections_hi, NULL, NULL, &val);
			connection = (mosquitto_connection_t *)val;
			stream->write_function(stream, "	  connection name: %s connected: %s messages sent: %d\n", connection->name, connection->connected ? "True" : "False", connection->count);
			stream->write_function(stream, "		enable: %s\n", connection->enable ? "True" : "False");
			stream->write_function(stream, "		host: %s\n", connection->host);
			stream->write_function(stream, "		port: %d\n", connection->port);
			stream->write_function(stream, "		keepalive: %d\n", connection->keepalive);
			stream->write_function(stream, "		username: %s\n", connection->username);
			stream->write_function(stream, "		password: %s\n", connection->password);
			stream->write_function(stream, "		client_id: %s\n", connection->client_id);
			stream->write_function(stream, "		clean_session: %s\n", connection->clean_session ? "True" : "False");
			stream->write_function(stream, "		retries: %d\n", connection->retries);
			stream->write_function(stream, "		max_inflight_messages: %d\n", connection->max_inflight_messages);
			stream->write_function(stream, "		reconnect_delay: %d\n", connection->reconnect_delay);
			stream->write_function(stream, "		reconnect_delay_max: %d\n", connection->reconnect_delay_max);
			stream->write_function(stream, "		reconnect_exponential_backoff: %s\n", connection->reconnect_exponential_backoff ? "True" : "False");
		}
		stream->write_function(stream, "	publishers\n");
		for (switch_hash_index_t *publishers_hi = switch_core_hash_first(profile->publishers); publishers_hi; publishers_hi = switch_core_hash_next(&publishers_hi)) {
			switch_core_hash_this(publishers_hi, NULL, NULL, &val);
			publisher = (mosquitto_publisher_t *)val;
			stream->write_function(stream, "	  publisher name: %s messages sent: %d\n", publisher->name, publisher->count);
			stream->write_function(stream, "		enable: %s\n", publisher->enable ? "True" : "False");
			for (switch_hash_index_t *topics_hi = switch_core_hash_first(publisher->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
				switch_core_hash_this(topics_hi, NULL, NULL, &val);
				topic = (mosquitto_topic_t *)val;
				stream->write_function(stream, "		topic name: %s messages sent: %d\n", topic->name, topic->count);
				stream->write_function(stream, "		  enable: %s\n", topic->enable ? "True" : "False");
				stream->write_function(stream, "		  connection_name: %s\n", topic->connection_name);
				stream->write_function(stream, "		  pattern: %s\n", topic->pattern);
				stream->write_function(stream, "		  qos: %d\n", topic->qos);
				stream->write_function(stream, "		  retain: %s\n", topic->retain ? "True" : "False");
				for (switch_hash_index_t *events_hi = switch_core_hash_first(topic->events); events_hi; events_hi = switch_core_hash_next(&events_hi)) {
					switch_core_hash_this(events_hi, NULL, NULL, &val);
					event = (mosquitto_event_t *)val;
					stream->write_function(stream, "		  event: %s [%d]\n", switch_event_name(event->event_type), (int)event->event_type);
				}
			}
		}
		stream->write_function(stream, "	subscribers\n");
		for (switch_hash_index_t *subscribers_hi = switch_core_hash_first(profile->subscribers); subscribers_hi; subscribers_hi = switch_core_hash_next(&subscribers_hi)) {
			switch_core_hash_this(subscribers_hi, NULL, NULL, &val);
			subscriber = (mosquitto_subscriber_t *)val;
			stream->write_function(stream, "	  subscriber name: %s\n", subscriber->name);
			stream->write_function(stream, "		enable: %s\n", subscriber->enable ? "True" : "False");
			for (switch_hash_index_t *topics_hi = switch_core_hash_first(subscriber->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
				switch_core_hash_this(topics_hi, NULL, NULL, &val);
				topic = (mosquitto_topic_t *)val;
				stream->write_function(stream, "		topic name: %s\n", topic->name);
				stream->write_function(stream, "		  enable: %s\n", topic->enable ? "True" : "False");
				stream->write_function(stream, "		  connection_name: %s\n", topic->connection_name);
				stream->write_function(stream, "		  pattern: %s\n", topic->pattern);
				stream->write_function(stream, "		  qos: %d\n", topic->qos);
				stream->write_function(stream, "		  retain: %s\n", topic->retain ? "True" : "False");
				stream->write_function(stream, "		  originate_authorized: %s\n", topic->originate_authorized ? "True" : "False");
			}
		}
	}
	switch_mutex_unlock(mosquitto_globals.profiles_mutex);
	switch_mutex_unlock(mosquitto_globals.mutex);

	stream->write_function(stream, "%s\n", line);

	return SWITCH_STATUS_SUCCESS;
}


/**
 * \brief   This function is called when a command entered in the fs_cli console.
 *
 * \details The definition of this function is performed by the macro SWITCH_STANDARD_API that expands to
 *          switch_status_t exec_api_cmd(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)
 *
 * \param[in]   *cmd	command entered on the fs_cli console
 * \param[in]   *session
 * \param[in]   *stream	output handle used for writing messages to the fs_api console
 *
 * \retval      switch_status_t
 *
 */

SWITCH_STANDARD_API(exec_api_cmd)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_command_t func = NULL;

	static const char usage_string[] = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"mosquitto [help]\n"
		"mosquitto status\n"
		"mosquitto loglevel [debug|info|notice|warning|error|critical|alert|console]\n"
		"mosquitto enable profile <profile-name> [connection|publisher|subscriber] <name>\n"
		"mosquitto disable profile <profile-name> [connection|publisher|subscriber] <name>\n"
		"mosquitto remove profile <profile-name> [connection|publisher|subscriber] <name>\n"
		"mosquitto connect profile <profile-name> connection <connection-name>\n"
		"mosquitto disconnect profile <profile-name> connection <connection-name>\n"
		"mosquitto bgapi <command> [<arg>]\n"
		"--------------------------------------------------------------------------------\n";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strncasecmp(argv[0], "help", 4)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	} else if (!strncasecmp(argv[0], "status", 6)) {
		func = cmd_status;
	} else if (!strncasecmp(argv[0], "loglevel", 8)) {
		func = cmd_loglevel;
	} else if (!strncasecmp(argv[0], "enable", 6)) {
		func = cmd_enable_disable;
	} else if (!strncasecmp(argv[0], "disable", 7)) {
		func = cmd_enable_disable;
	} else if (!strncasecmp(argv[0], "remove", 6)) {
		func = cmd_remove;
	} else if (!strncasecmp(argv[0], "connect", 7)) {
		func = cmd_connect;
	} else if (!strncasecmp(argv[0], "disconnect", 10)) {
		func = cmd_disconnect;
	} else if (!strncasecmp(argv[0], "bgapi", 5)) {
		status = cmd_bgapi(cmd, stream);
		switch_safe_free(mycmd)
			return status;
	}

	if (func) {
		status = func(&argv[0], argc, stream);
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

done:
	switch_safe_free(mycmd)
		return status;

}


/**
 * \brief   This function is called to request that FreeSWITCH add a new API command.
 *
 * \details Each new command created by this module should be added here.
 *
 * \param[in]   **module_interface
 * \param[in]   *api_interface
 *
 */

void add_cli_api(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface)
{
	SWITCH_ADD_API(api_interface, "mosquitto", MOSQUITTO_DESC, exec_api_cmd, MOSQUITTO_SYNTAX);
	switch_console_set_complete("add mosquitto help");
	switch_console_set_complete("add mosquitto loglevel [debug|info|notice|warning|error|critical|alert|console]");
	switch_console_set_complete("add mosquitto status");
	switch_console_set_complete("add mosquitto enable [profile|connection|publisher|subscriber] name");
	switch_console_set_complete("add mosquitto disable [profile|connection|publisher|subscriber] name");
	switch_console_set_complete("add mosquitto remove profile profile-name [connection|publisher|subscriber] name");
	switch_console_set_complete("add mosquitto connect profile profile-name connection connection-name");
	switch_console_set_complete("add mosquitto disconnect profile profile-name connection connection-name [enable|disable]");
	switch_console_set_complete("add mosquitto bgapi <command> [<arg>]");

}


/**
 * \brief   This function is called when mod_mosquitto is being unloaded to remove the API commands.
 *
 * \details	This will remove the command from being displayed by the fs_cli help function
 *
 *
 */
void remove_cli_api()
{
	switch_console_set_complete("del mosquitto");
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
