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

#include "mosquitto_utils.h"
#include "mod_mosquitto.h"
#include "mosquitto_events.h"
#include "mosquitto_config.h"
#include "mosquitto_mosq.h"


/**
 * @brief This function initializes all of the configured profiles.
 *
 * @details	The profiles are stored in a hash.  The address of the hash is stored in a global variable.
 *		  Standard hash looping is performed to locate each profile.
 *
 * @param[in]	void	No input parameters
 *
 * @retval		Always returns SWITCH_STATUS_SUCCESS
 */
switch_status_t initialize_profiles(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(mosquitto_globals.profiles_mutex);
	for (switch_hash_index_t *profiles_hi = switch_core_hash_first(mosquitto_globals.profiles); profiles_hi; profiles_hi = switch_core_hash_next(&profiles_hi)) {
		mosquitto_profile_t *profile = NULL;
		void *val;
		switch_core_hash_this(profiles_hi, NULL, NULL, &val);
		profile = (mosquitto_profile_t *)val;
		if (profile->enable) {
			log(SWITCH_LOG_INFO, "Profile %s activation in progress\n", profile->name);
			status = profile_activate(profile);
		} else {
			log(SWITCH_LOG_INFO, "Profile %s deactivation in progress\n", profile->name);
			status = profile_deactivate(profile);
		}
	}
	switch_mutex_unlock(mosquitto_globals.profiles_mutex);

	return status;
}


/**
 * @brief   This routine starts all of the connections, publishers, subscribers within a profile
 *
 * @details This routine loops thru all the hashes associated with connection, publishers and subscribers
 *			attempting to start them (if enabled)
 *
 * @param[in]   *profile	Pointer to the profile hash
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t profile_activate(mosquitto_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to profile_activate()\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_lock(profile->mutex);

	switch_mutex_lock(profile->log->mutex);
	if (profile->log->enable) {
		if (zstr(profile->log->name)) {
			log(SWITCH_LOG_ERROR, "Unable to open log for profile %s name is NULL\n", profile->name);
		} else {
			status = switch_file_open(&profile->log->logfile, profile->log->name, SWITCH_FOPEN_WRITE|SWITCH_FOPEN_APPEND|SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, profile->pool);
			if (status != SWITCH_STATUS_SUCCESS) {
				log(SWITCH_LOG_ERROR, "Failed to open log for profile %s name %s\n", profile->name, profile->log->name);
			}
		}
	}
	switch_mutex_unlock(profile->log->mutex);

	switch_mutex_lock(profile->publishers_mutex);
	for (switch_hash_index_t *publishers_hi = switch_core_hash_first(profile->publishers); publishers_hi; publishers_hi = switch_core_hash_next(&publishers_hi)) {
		mosquitto_publisher_t *publisher = NULL;
		void *val;
		switch_core_hash_this(publishers_hi, NULL, NULL, &val);
		publisher = (mosquitto_publisher_t *)val;
		if (publisher->enable) {
			log(SWITCH_LOG_INFO, "Profile %s publisher %s activation in progress\n", profile->name, publisher->name);
			status = publisher_activate(profile, publisher);
		} else {
			log(SWITCH_LOG_INFO, "Profile %s publisher %s deactivation in progress\n", profile->name, publisher->name);
			status = publisher_deactivate(profile, publisher);
		}
	}
	switch_mutex_unlock(profile->publishers_mutex);

	switch_mutex_lock(profile->subscribers_mutex);
	for (switch_hash_index_t *subscribers_hi = switch_core_hash_first(profile->subscribers); subscribers_hi; subscribers_hi = switch_core_hash_next(&subscribers_hi)) {
		mosquitto_subscriber_t *subscriber = NULL;
		void *val;
		switch_core_hash_this(subscribers_hi, NULL, NULL, &val);
		subscriber = (mosquitto_subscriber_t *)val;
		if (subscriber->enable) {
			log(SWITCH_LOG_INFO, "Profile %s subscriber %s activation in progress\n", profile->name, subscriber->name);
			status = subscriber_activate(profile, subscriber);
		} else {
			log(SWITCH_LOG_INFO, "Profile %s subscriber %s deactivation in progress\n", profile->name, subscriber->name);
			status = subscriber_deactivate(profile, subscriber);
		}
	}
	switch_mutex_unlock(profile->subscribers_mutex);

	switch_mutex_lock(profile->connections_mutex);
	for (switch_hash_index_t *connections_hi = switch_core_hash_first(profile->connections); connections_hi; connections_hi = switch_core_hash_next(&connections_hi)) {
		mosquitto_connection_t *connection = NULL;
		void *val;
		switch_core_hash_this(connections_hi, NULL, NULL, &val);
		connection = (mosquitto_connection_t *)val;
		if (connection->enable) {
			log(SWITCH_LOG_INFO, "Profile %s connection %s activation in progress\n", profile->name, connection->name);
			status = client_connect(profile, connection);
		} else {
			log(SWITCH_LOG_INFO, "Profile %s connection %s deactivation in progress\n", profile->name, connection->name);
			status = mosq_disconnect(connection);
			connection->userdata = NULL;
			connection->mosq = NULL;
		}
	}
	switch_mutex_unlock(profile->connections_mutex);

	switch_mutex_unlock(profile->mutex);
	return status;
}

/**
 * @brief   TODO: This routine stops all of the connections, publishers, subscribers within a profile
 *
 * @details TODO: This routine loops thru all the hashes associated with connection, publishers and subscribers
 *			attempting to deactivate them
 *
 * @param[in]   *profile	Pointer to the profile hash
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t profile_deactivate(mosquitto_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to profile_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_INFO, "Profile %s deactivate in progress\n", profile->name);

	switch_mutex_lock(profile->log->mutex);
	if (zstr(profile->log->name)) {
		log(SWITCH_LOG_ERROR, "Unable to close log for profile %s name is NULL\n", profile->name);
	} else if (profile->log->logfile == NULL) {
		log(SWITCH_LOG_ERROR, "Unable to close log for profile %s name %s file handle is NULL\n", profile->name, profile->log->name);
	} else if ((status = switch_file_close(profile->log->logfile)) != SWITCH_STATUS_SUCCESS) {
	  log(SWITCH_LOG_ERROR, "Failed to close log for profile %s name %s\n", profile->name, profile->log->name);
	}
	switch_mutex_unlock(profile->log->mutex);

	return status;
}


/**
 * @brief   This routine starts a publisher associated with a profile
 *
 * @details This routine will also bind any events to topics associated with the publisher
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *publisher  Pointer to the publisher associated with profile
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t publisher_activate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to publisher_activate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher not passed to publisher_activate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_INFO, "Profile %s publisher %s activation in progress\n", profile->name, publisher->name);

	switch_mutex_lock(publisher->topics_mutex);
	for (switch_hash_index_t *topics_hi = switch_core_hash_first(publisher->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
		mosquitto_topic_t *topic = NULL;
		void *val;
		switch_core_hash_this(topics_hi, NULL, NULL, &val);
		topic = (mosquitto_topic_t *)val;
		if (publisher->enable) {
			if (topic->enable) {
				status = publisher_topic_activate(profile, publisher, topic);
			} else {
				status = publisher_topic_deactivate(profile, publisher, topic);
			}
		} else {
			status = publisher_topic_deactivate(profile, publisher, topic);
		}
	}
	switch_mutex_unlock(publisher->topics_mutex);

	return status;
}


/**
 * @brief   This routine binds the events associated with a topic, profile and publisher
 *
 * @details This routine loops thru the events hash binding each one that is enabled
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *publisher  Pointer to the publisher associated with profile
 * @param[in]   *topic		Pointer to the topic associated with profile
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t publisher_topic_activate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to publisher_topic_activate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher not passed to publisher_topic_activate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher %s topic not passed to publisher_topic_activate()\n", profile->name, publisher->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_NOTICE, "Profile %s publishser %s topic %s activation in progress\n", profile->name, publisher->name, topic->name);

	if (topic->enable) {
		switch_mutex_lock(topic->events_mutex);
		for (switch_hash_index_t *events_hi = switch_core_hash_first(topic->events); events_hi; events_hi = switch_core_hash_next(&events_hi)) {
			mosquitto_event_t *event = NULL;
			void *val;
			switch_core_hash_this(events_hi, NULL, NULL, &val);
			event = (mosquitto_event_t *)val;
			status = bind_event(profile, publisher, topic, event);
		}
		switch_mutex_unlock(topic->events_mutex);
	}

	return status;
}


/**
 * @brief   This routine deactivates and unbinds the events associated with a topic, profile and publisher
 *
 * @details This routine loops thru the events hash unbinding each one that is enabled
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *publisher  Pointer to the publisher associated with profile
 * @param[in]   *topic		Pointer to the topic associated with profile
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t publisher_topic_deactivate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to publisher_topic_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher not passed to publisher_topic_deactivate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher %s topic not passed to publisher_topic_deactivate()\n", profile->name, publisher->name);
		return SWITCH_STATUS_GENERR;
	}


	log(SWITCH_LOG_NOTICE, "Profile %s publisher %s topic %s deactivate in progress\n", profile->name, publisher->name, topic->name);

	if (topic->enable) {
		switch_mutex_lock(topic->events_mutex);
		for (switch_hash_index_t *events_hi = switch_core_hash_first(topic->events); events_hi; events_hi = switch_core_hash_next(&events_hi)) {
			mosquitto_event_t *event = NULL;
			void *val;
			switch_core_hash_this(events_hi, NULL, NULL, &val);
			event = (mosquitto_event_t *)val;
			log(SWITCH_LOG_NOTICE, "Profile %s publisher %s topic %s event %s unbind %d\n", profile->name, publisher->name, topic->name, event->name, status);
			//status = unbind_event(profile, publisher, topic, event);
		}
		switch_mutex_unlock(topic->events_mutex);
	}

	return status;
}


/**
 * @brief   TODO: This routine deactivates the publisher associated with the specified profile
 *
 * @details TODO: This routine loops thru all the topics associated with the publisher and unbinds any events
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *publisher  Pointer to the publisher associated with the specified profile
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t publisher_deactivate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to publisher_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(SWITCH_LOG_ERROR, "Profile %s publisher not passed to publisher_deactivate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_INFO, "Profile %s publisher %s deactivate in progress\n", profile->name, publisher->name);
	return status;
}

/**
 * @brief   This routine starts a subscriber associated with a profile
 *
 * @details This routine will also create subscribe to topics associated with the subscriber
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *subscriber Pointer to the subscriber associated with profile
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t subscriber_activate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to subscriber_activate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!subscriber) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber not passed to subscriber_activate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_NOTICE, "Profile %s subscriber %s activation in progress\n", profile->name, subscriber->name);

	switch_mutex_lock(subscriber->topics_mutex);
	for (switch_hash_index_t *topics_hi = switch_core_hash_first(subscriber->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
		mosquitto_topic_t *topic = NULL;
		void *val;
		switch_core_hash_this(topics_hi, NULL, NULL, &val);
		topic = (mosquitto_topic_t *)val;
		if (subscriber->enable) {
			if (topic->enable) {
				status = subscriber_topic_activate(profile, subscriber, topic);
			} else {
				status = subscriber_topic_deactivate(profile, subscriber, topic);
			}
		} else {
			status = subscriber_topic_deactivate(profile, subscriber, topic);
		}
	}
	switch_mutex_unlock(subscriber->topics_mutex);

	return status;
}


/**
 * @brief   This routine binds the events associated with a topic, profile and subscriber
 *
 * @details This routine loops thru the topics and subscribes to each one that is enabled
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *subscriber Pointer to the subscriber associated with profile
 * @param[in]   *topic	  Pointer to the topic associated with profile
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t subscriber_topic_activate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to subscriber_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!subscriber) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber not passed to subscriber_deactivate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber %s topic not passed to subscriber_deactivate()\n", profile->name, subscriber->name);
		return SWITCH_STATUS_GENERR;
	}

	if (topic->enable) {
		log(SWITCH_LOG_INFO, "Profile %s subscriber %s topic %s pattern %s qos %d activate in progress\n", profile->name, subscriber->name, topic->name, topic->pattern, topic->qos);
		status = mosq_subscribe(profile, subscriber, topic);
	}

	return status;
}


/**
 * @brief   TODO: This routine deactivates the subscriber associated with the specified profile
 *
 * @details TODO: This routine loops thru all the topics associated with the subscriber and removes any subscriptions
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *subscriber Pointer to the subscriber associated with the specified profile
 *
 * @retval	SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t subscriber_topic_deactivate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to subscriber_topic_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!subscriber) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber not passed to subscriber_topic_deactivate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!topic) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber %s topic not passed to subscriber_topic_deactivate()\n", profile->name, subscriber->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_INFO, "Profile %s subscriber %s topic %s pattern %s qos %d deactivate in progress\n", profile->name, subscriber->name, topic->name, topic->pattern, topic->qos);

	return status;
}

/**
 * @brief   TODO: This routine deactivates the subscriber associated with the specified profile
 *
 * @details TODO: This routine loops thru all the topics associated with the subscriber and unsubscribes to them
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *subscriber Pointer to the subscriber associated with the specified profile
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t subscriber_deactivate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to subscriber_deactivate()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!subscriber) {
		log(SWITCH_LOG_ERROR, "Profile %s subscriber not passed to subscriber_deactivate()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	log(SWITCH_LOG_INFO, "Profile %s subscriber %s deactivate in progress\n", profile->name, subscriber->name);
	return status;
}


/**
 * @brief   This routine attempts to connect to an MQTT broker
 *
 * @details This routine sets up connection properties/options and then calls the routine to perform the actual connection
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *connection Pointer to the connection associated with the specified profile
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t client_connect(mosquitto_profile_t *profile, mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to client_connect()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Profile %s connection not passed to client_connect()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (mosq_new(profile, connection) == SWITCH_STATUS_SUCCESS) {
		mosq_reconnect_delay_set(connection);
		mosq_message_retry_set(connection);
		mosq_max_inflight_messages_set(connection);
		mosq_username_pw_set(connection);

		status = mosq_connect(connection);
	}

	return status;
}


/**
 * @brief   This routine tries to locate the subscriber fromm an inbound message
 *
 * @details Received messages only have the connection and topic available.  This routine searches for the associated subscriber
 *			so that the message can be processes.
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *connection Pointer to the connection associated with the specified profile
 * @param[in]   *name		Pointer to the topic name associated with the received message
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

mosquitto_topic_t *locate_connection_topic(mosquitto_profile_t *profile, mosquitto_connection_t *connection, const char *name)
{
	mosquitto_topic_t *found_topic = NULL;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to locate_connection_topic()\n");
		return NULL;
	}

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Profile %s connection not passed to locate_connection_topic()\n", profile->name);
		return NULL;
	}

	if (zstr(name)) {
		log(SWITCH_LOG_ERROR, "Profile %s connection %s topic name is NULL for locate_connection_topic()\n", profile->name, connection->name);
		return found_topic;
	}

	switch_mutex_lock(profile->subscribers_mutex);
	for (switch_hash_index_t *subscribers_hi = switch_core_hash_first(profile->subscribers); subscribers_hi; subscribers_hi = switch_core_hash_next(&subscribers_hi)) {
		mosquitto_subscriber_t *subscriber = NULL;
		void *val;
		switch_core_hash_this(subscribers_hi, NULL, NULL, &val);
		subscriber = (mosquitto_subscriber_t *)val;
		if (!subscriber->enable) {
			continue;
		}
		switch_mutex_lock(subscriber->topics_mutex);
		for (switch_hash_index_t *topics_hi = switch_core_hash_first(subscriber->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
			mosquitto_topic_t *topic;
			void *val;
			switch_core_hash_this(topics_hi, NULL, NULL, &val);
			topic = (mosquitto_topic_t *)val;
			if (!topic->enable) {
				continue;
			}
			if (!strcasecmp(topic->connection_name, connection->name)) {
				if (!strcasecmp(topic->pattern, name)) {
					log(SWITCH_LOG_INFO, "Profile %s connection %s topic %s pattern %s found\n", profile->name,  connection->name, topic->name, topic->pattern);
					found_topic = topic;
					break;
				}
			}
		}
		switch_mutex_unlock(subscriber->topics_mutex);
	}
	switch_mutex_unlock(profile->subscribers_mutex);

	return found_topic;
}


/**
 * @brief   This routine attempts to reinitialize a connection associated with the specified profile
 *
 * @details The connection will either be attempted or disconnected based on it being enabled or not
 *			so that the message can be processes.
 *
 * @param[in]   *profile	Pointer to the profile hash
 * @param[in]   *connection Pointer to the connection associated with the specified profile
 *
 * @retval  SWITCH_STATUS_SUCCCESS or SWITCH_STATUS_GENERR
 */

switch_status_t connection_initialize(mosquitto_profile_t *profile, mosquitto_connection_t *connection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(SWITCH_LOG_ERROR, "Profile not passed to connection_initialize()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection) {
		log(SWITCH_LOG_ERROR, "Profile %s connection not passed to connection_initialize()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (connection->enable) {
		log(SWITCH_LOG_INFO, "Profile %s connection %s activation in progress\n", profile->name, connection->name);
		status = client_connect(profile, connection);
	} else {
		log(SWITCH_LOG_INFO, "Profile %s connection %s deactivation in progress\n", profile->name, connection->name);
		status = mosq_disconnect(connection);
		connection->userdata = NULL;
		connection->mosq = NULL;
	}

	return status;
}

int mosquitto_log(int severity, const char *format, ...)
{
	switch_time_exp_t tm;

	va_list ap;
	int ret;
	char *data;

	if (!mosquitto_globals.log.enable) {
		return -1;
	}

	if (severity > mosquitto_globals.log.level) {
		return -1;
	}

	if (mosquitto_globals.log.logfile == NULL) {
		return -1;
	}

	switch_mutex_lock(mosquitto_globals.log.mutex);

	switch_time_exp_lt(&tm, switch_micro_time_now());
	switch_file_printf(mosquitto_globals.log.logfile, "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);

	va_start(ap, format);

	if ((ret = switch_vasprintf(&data, format, ap)) != -1) {
		switch_size_t bytes = strlen(data);
		ret = switch_file_write(mosquitto_globals.log.logfile, data, &bytes);
		free(data);
	}

	va_end(ap);

	switch_mutex_unlock(mosquitto_globals.log.mutex);

	return ret;

}


int profile_log(int severity, mosquitto_profile_t *profile, const char *format, ...)
{
	switch_time_exp_t tm;

	va_list ap;
	int ret;
	char *data;

	if (!profile) {
		return -1;
	}

	if (!profile->log->enable) {
		return -1;
	}

	if (severity > profile->log->level) {
		return -1;
	}

	if (zstr(profile->log->name)) {
		return -1;
	}

	if (profile->log->logfile == NULL) {
		return -1;
	}

	switch_mutex_lock(profile->log->mutex);

	switch_time_exp_lt(&tm, switch_micro_time_now());
	switch_file_printf(profile->log->logfile, "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);

	va_start(ap, format);

	if ((ret = switch_vasprintf(&data, format, ap)) != -1) {
		switch_size_t bytes = strlen(data);
		ret = switch_file_write(profile->log->logfile, data, &bytes);
		free(data);
	}

	va_end(ap);

	switch_mutex_unlock(profile->log->mutex);

	return ret;

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
