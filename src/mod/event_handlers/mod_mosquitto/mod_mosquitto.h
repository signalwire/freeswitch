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

#ifndef MOD_MOSQUITTO_H
#define MOD_MOSQUITTO_H

#include <switch.h>
#include <switch_utils.h>

#define DEBUG SWITCH_LOG_DEBUG
#define INFO SWITCH_LOG_INFO
#define NOTICE SWITCH_LOG_NOTICE
#define WARNING SWITCH_LOG_WARNING
#define ERROR SWITCH_LOG_ERROR
#define CRIT SWITCH_LOG_CRIT
#define ALERT SWITCH_LOG_ALERT
#define CONSOLE SWITCH_LOG_CONSOLE

#define log(severity, ...) \
	if (severity <= mosquitto_globals.loglevel) { \
		 switch_log_printf(SWITCH_CHANNEL_LOG, (severity), __VA_ARGS__); \
	}

#define MOSQUITTO_CONFIG_FILE		"mosquitto.conf"
#define MOSQUITTO_DEFAULT_PROFILE	"default"
#define EVENT_QUEUE_SIZE			50000
#define	UNIQUE_STRING_LENGTH		32

typedef struct mosquitto_profile_s mosquitto_profile_t;
typedef struct mosquitto_connection_s mosquitto_connection_t;
typedef struct mosquitto_publisher_s mosquitto_publisher_t;
typedef struct mosquitto_subscriber_s mosquitto_subscriber_t;
typedef struct mosquitto_topic_s mosquitto_topic_t;
typedef struct mosquitto_event_userdata_s mosquitto_event_userdata_t;
typedef struct mosquitto_mosq_userdata_s mosquitto_mosq_userdata_t;

struct mosquitto_bgapi_job {
    char *cmd;
    char *arg;
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
    switch_memory_pool_t *pool;
};
typedef struct mosquitto_bgapi_job mosquitto_bgapi_job_t;


struct mosquitto_lib_s {
	int major;
	int minor;
	int revision;
};
typedef struct mosquitto_lib_s mosquitto_lib_t;


struct mosquitto_will_s {
	char *name;
	char *topic;
	int payloadlen;
	char *payload;
	int qos;
	switch_bool_t retain;
};
typedef struct mosquitto_will_s mosquitto_will_t;

struct mosquitto_event_s {
	char *name;
	switch_event_types_t event_type;
	switch_event_node_t *node;
	mosquitto_event_userdata_t *userdata;
	char event_id[100];
	switch_bool_t bound;
	unsigned count;
};
typedef struct mosquitto_event_s mosquitto_event_t;

struct mosquitto_topic_s {
	char *name;
	char *profile_name;
	char *publisher_name;
	char *subscriber_name;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;

	switch_mutex_t *events_mutex;
	switch_hash_t *events;

	switch_bool_t enable;
	switch_bool_t subscribed;
	char *pattern;
	int *mid;
	int qos;
	switch_bool_t retain;
	char *connection_name;
	switch_bool_t originate_authorized;
	switch_bool_t bgapi_authorized;
	unsigned count;
};
//typedef struct mosquitto_topic_s mosquitto_topic_t;

struct mosquitto_subscriber_s {
	char *name;
	char *profile_name;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;

	switch_mutex_t *topics_mutex;
	switch_hash_t *topics;

	switch_bool_t enable;
};
//typedef struct mosquitto_subscriber_s mosquitto_subscriber_t;

struct mosquitto_publisher_s {
	char *name;
	char *profile_name;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;

	switch_mutex_t *topics_mutex;
	switch_hash_t *topics;

	switch_bool_t enable;
	unsigned count;
};
//typedef struct mosquitto_publisher_s mosquitto_publisher_t;

struct mosquitto_connection_s {
	char *name;
	char *profile_name;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;

	switch_bool_t enable;
	switch_bool_t connected;
	char *protocol_version;
	int send_maximum;
	int receive_maximum;
	char *host;
	int port;
	int keepalive;
	char *username;
	char *password;
	char *client_id;
	switch_bool_t clean_session;
	char *bind_address;
	int reconnect_delay;
	int reconnect_delay_max;
	switch_bool_t reconnect_exponential_backoff;
	int max_inflight_messages;
	int message_retry;
	int retries;
	int retry_count;
	unsigned count;
	switch_bool_t srv;
	mosquitto_mosq_userdata_t *userdata;
	struct mosquitto *mosq;
};
//typedef struct mosquitto_connection_s mosquitto_connection_t;

struct mosquitto_profile_s {
	char *name;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;

	switch_mutex_t *connections_mutex;
	switch_hash_t *connections;
	switch_mutex_t *publishers_mutex;
	switch_hash_t *publishers;
	switch_mutex_t *subscribers_mutex;
	switch_hash_t *subscribers;

	switch_bool_t enable;
};
//typedef struct mosquitto_profile_s mosquitto_profile_t;

struct mosquitto_event_userdata_s {
	mosquitto_profile_t *profile;
	mosquitto_connection_t *connection;
	mosquitto_publisher_t *publisher;
	mosquitto_subscriber_t *subscriber;
	mosquitto_topic_t *topic;
};
//typedef struct mosquitto_event_userdata_s mosquitto_event_userdata_t;

struct mosquitto_mosq_userdata_s {
	mosquitto_profile_t *profile;
	mosquitto_connection_t *connection;
	mosquitto_publisher_t *publisher;
	mosquitto_subscriber_t *subscriber;
	mosquitto_topic_t *topic;
};
//typedef struct mosquitto_mosq_userdata_s mosquitto_mosq_userdata_t;

struct globals_s {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *bgapi_rwlock;

	switch_mutex_t *profiles_mutex;
	switch_hash_t *profiles;

	switch_queue_t *event_queue;
	size_t event_queue_size;
	mosquitto_lib_t mosquitto_lib;

	int running;
	switch_log_level_t loglevel;
	switch_bool_t enable_profiles;
	switch_bool_t enable_publishers;
	switch_bool_t enable_subscribers;
	switch_bool_t enable_connections;
	switch_bool_t enable_topics;
	switch_bool_t enable_events;
	size_t unique_string_length;
} mosquitto_globals;

#endif /* MOD_MOSQUITTO_H */

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

