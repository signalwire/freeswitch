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

#include "mod_mosquitto.h"
#include "mosquitto_config.h"
#include "mosquitto_events.h"
#include "mosquitto_mosq.h"
#include "mosquitto_utils.h"

static mosquitto_profile_t *add_profile(const char *name);
static mosquitto_connection_t *add_connection(mosquitto_profile_t *profile, const char *name);
static mosquitto_publisher_t *add_publisher(mosquitto_profile_t *profile, const char *name);
static mosquitto_subscriber_t *add_subscriber(mosquitto_profile_t *profile, const char *name);
static mosquitto_topic_t *add_publisher_topic(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, const char *name);
static switch_status_t parse_settings(switch_xml_t cfg);
static switch_status_t parse_profiles(switch_xml_t cfg);
static switch_status_t parse_connections(switch_xml_t xconnection, mosquitto_profile_t *profile);
static switch_status_t parse_publishers(switch_xml_t xpublisher, mosquitto_profile_t *profile);
static switch_status_t parse_subscribers(switch_xml_t xsubscriber, mosquitto_profile_t *profile);
static switch_status_t parse_publisher_topics(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, switch_xml_t xpublisher);
static switch_status_t parse_subscriber_topics(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, switch_xml_t xsubscriber);
static mosquitto_event_t *add_publisher_topic_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, const char *name, switch_event_types_t event_type);
static void rand_str(char *dest, size_t length);
static switch_status_t parse_connection_tls(mosquitto_profile_t *profile, mosquitto_connection_t *connection, switch_xml_t xconnection);
static switch_status_t parse_connection_will(mosquitto_profile_t *profile, mosquitto_connection_t *connection, switch_xml_t xconnection);

/**
 * \brief   This function is used to add a new profile to the hash
 *
 * \details This function will allocate memory for a profile hash entry, initialize generic data types
 *			such as mutexes and create hashes for publishers, subscribers, connections associated with the new profile.
 *
 * \param[in]   *name	Name of the profile being created
 *
 * \retval		pointer to the newly defined profile hash entry or NULL if memory could not be allocated
 */

static mosquitto_profile_t *add_profile(const char *name)
{
	mosquitto_profile_t *profile = NULL;
	switch_memory_pool_t *pool;

	if (zstr(name)) {
		log(ERROR, "Profile name not passed to add_profile()\n");
		return profile;
	}

	if (!(profile = switch_core_alloc(mosquitto_globals.pool, sizeof(*profile)))) {
		log(ERROR, "Failed to allocate memory from pool for profile %s\n", name);
		return profile;
	}

	switch_core_new_memory_pool(&pool);
	profile = switch_core_alloc(pool, sizeof(*profile));
	profile->pool = pool;
	profile->name = switch_core_strdup(profile->pool, name);

	switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&profile->rwlock, profile->pool);

	switch_mutex_init(&profile->connections_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&profile->connections);
	switch_mutex_init(&profile->publishers_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&profile->publishers);
	switch_mutex_init(&profile->subscribers_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&profile->subscribers);

	switch_core_hash_insert_locked(mosquitto_globals.profiles, profile->name, profile, mosquitto_globals.profiles_mutex);
	log(INFO, "Profile %s successfully added\n", name);
	return profile;
}


/**
 * \brief   This function is used to locate a profile by name
 *
 * \details This function searches the global profiles hash in an attempt to find a profile with a matching name.
 *			The name is used as a key, so must be unique (no two profiles can have the same name)
 *
 * \param[in]   *name	Name of the profile being searched for
 *
 * \retval		pointer to the hash of the profile or NULL
 */

mosquitto_profile_t *locate_profile(const char *name)
{
	mosquitto_profile_t *profile = NULL;

	if (zstr(name)) {
		log(ERROR, "Profile name not passed to locate_profile()\n");
		return profile;
	}

	if (!(profile = switch_core_hash_find_locked(mosquitto_globals.profiles, name, mosquitto_globals.profiles_mutex))) {
		log(WARNING, "Unable to locate profile %s\n", name);
	}

	return profile;
}


/**
 * \brief   This function is used to add a new connection to the hash (within a profile hash)
 *
 * \details This function will allocate memory for a connection hash entry and initialize generic data types
 *			such as mutexes.
 *
 * \param[in]   *profile	Pointer to the profile hash where the new connection should be added
 * \param[in]   *name		Name of the connection being created
 *
 * \retval		pointer to the newly defined connection hash entry or NULL
 */

static mosquitto_connection_t *add_connection(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_connection_t *connection = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_connection()\n");
		return connection;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s connection name not passed to add_connection()\n", profile->name);
		return connection;
	}

	if (!(connection = switch_core_alloc(profile->pool, sizeof(*connection)))) {
		log(ERROR, "Failed to allocate memory from pool for connection %s\n", name);
		return connection;
	}

	connection->name = switch_core_strdup(profile->pool, name);
	connection->profile_name = switch_core_strdup(profile->pool, profile->name);
	switch_mutex_init(&connection->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&connection->rwlock, profile->pool);

	switch_core_hash_insert_locked(profile->connections, connection->name, connection, profile->connections_mutex);
	log(INFO, "Profile %s connection: %s successfully added\n", profile->name, name);

	return connection;
}


/**
 * \brief   This function is used to locate a connection by name
 *
 * \details This function searches the connections hash associated with the profile hash entry in an attempt
 *			to find a connection with a matching name.  the name is used as a key, so must be unique
 *			(two connections within the same profile cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *name		Name of the connection to locate
 *
 * \retval		pointer to the hash entry of the connection or NULL
 */

mosquitto_connection_t *locate_connection(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_connection_t *connection = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_connection()\n");
		return connection;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s connection name not passed to locate_connection()\n", profile->name);
		return connection;
	}

	if (!(connection = switch_core_hash_find_locked(profile->connections, name, profile->connections_mutex))) {
		log(WARNING, "Unable to locate connection: %s for profile %s\n", name, profile->name);
	}

	return connection;
}


/**
 * \brief   This function is used to remove a profile by name
 *
 * \details This function searches the profiles hash for a matching name and if found, removes all
 *			publishers, subscribers, connections, topics, etc associated with this profile. Once the profile
 *			has been removed, its memory pool will be destroyed (allowing the system to reclaim memory).
 *
 * \param[in]   *profile	Name of the profile to remove
 *
 * \retval		SWITCH_STATUS_GENERR if the profile could not be found
 */

switch_status_t remove_profile(const char *name)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_profile_t *profile = NULL;

	if (zstr(name)) {
		log(ERROR, "Profile name not passed to remove_profile()\n");
		return SWITCH_STATUS_GENERR;
	}

	profile = locate_profile(name);
	if (!profile) {
		log(WARNING, "Cannot remove unknown profile %s\n", name);
		return SWITCH_STATUS_GENERR;
	}

	log(NOTICE, "profile:%s shutting down publishers\n", profile->name);
	switch_mutex_lock(profile->publishers_mutex);
	for (switch_hash_index_t *publishers_hi = switch_core_hash_first(profile->publishers); publishers_hi; publishers_hi = switch_core_hash_next(&publishers_hi)) {
		mosquitto_publisher_t *publisher = NULL;
		void *val;
		switch_core_hash_this(publishers_hi, NULL, NULL, &val);
		publisher = (mosquitto_publisher_t *)val;
		switch_mutex_lock(publisher->topics_mutex);
		for (switch_hash_index_t *topics_hi = switch_core_hash_first(publisher->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
			mosquitto_topic_t *topic = NULL;
			void *val;
			switch_core_hash_this(topics_hi, NULL, NULL, &val);
			topic = (mosquitto_topic_t *)val;
			log(NOTICE, "shutting down publisher:%s  topic: %s\n", publisher->name, topic->name);
			publisher_topic_deactivate(profile, publisher, topic);
			switch_core_hash_delete(publisher->topics, topic->name);
		}
		switch_mutex_unlock(publisher->topics_mutex);
		log(NOTICE, "deleting publisher name:%s from profile hash\n", publisher->name);
		switch_core_hash_delete(profile->publishers, publisher->name);
	}
	switch_mutex_unlock(profile->publishers_mutex);

	log(NOTICE, "shutting down subscribers\n");
	switch_mutex_lock(profile->subscribers_mutex);
	for (switch_hash_index_t *subscribers_hi = switch_core_hash_first(profile->subscribers); subscribers_hi; subscribers_hi = switch_core_hash_next(&subscribers_hi)) {
		mosquitto_subscriber_t *subscriber = NULL;
		void *val;
		switch_core_hash_this(subscribers_hi, NULL, NULL, &val);
		subscriber = (mosquitto_subscriber_t *)val;
		switch_mutex_lock(subscriber->topics_mutex);
		for (switch_hash_index_t *topics_hi = switch_core_hash_first(subscriber->topics); topics_hi; topics_hi = switch_core_hash_next(&topics_hi)) {
			mosquitto_topic_t *topic = NULL;
			void *val;
			switch_core_hash_this(topics_hi, NULL, NULL, &val);
			topic = (mosquitto_topic_t *)val;
			log(WARNING, "shutting down subscriber topic: %s\n", topic->name);
			subscriber_topic_deactivate(profile, subscriber, topic);
			switch_core_hash_delete(subscriber->topics, topic->name);
		}
		switch_core_hash_delete(profile->subscribers, subscriber->name);
	}
	switch_mutex_unlock(profile->subscribers_mutex);

	log(NOTICE, "profile:%s shutting down connections\n", profile->name);
	switch_mutex_lock(profile->connections_mutex);
	for (switch_hash_index_t *connections_hi = switch_core_hash_first(profile->connections); connections_hi; connections_hi = switch_core_hash_next(&connections_hi)) {
		mosquitto_connection_t *connection = NULL;
		void *val;
		switch_core_hash_this(connections_hi, NULL, NULL, &val);
		connection = (mosquitto_connection_t *)val;
		log(NOTICE, "profile:%s connection:%s being disconnected\n", profile->name, connection->name);
		mosq_disconnect(connection);
		switch_core_hash_delete(profile->connections, connection->name);
	}
	switch_mutex_unlock(profile->connections_mutex);

	status = SWITCH_STATUS_SUCCESS;
	switch_core_hash_delete(mosquitto_globals.profiles, profile->name);
	switch_core_destroy_memory_pool(&profile->pool);
	return status;
}


/**
 * \brief   This function is used to remove a connection by name
 *
 * \details This function searches the connections hash associated with a specific profile for a matching name and if found,
 *			removes it from the hash.  If any connections to brokers are active, they are first disconnected.
 *
 * \param[in]   *profile	Pointer to the profile containing the connections hash
 * \param[in]   *name		Name of the connection to be removed
 *
 * \retval		SWITCH_STATUS_GENERR if the connection could could not be found
 */

switch_status_t remove_connection(mosquitto_profile_t *profile, const char *name)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	mosquitto_connection_t *connection = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to remove_connection()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s connection name not passed to remove_connection()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!(connection = locate_connection(profile, name))) {
		log(WARNING, "Cannot remove unknown connection: %s from profile %s\n", name, profile->name);
		return status;
	}

	status = mosq_disconnect(connection);

	switch_core_hash_delete_locked(profile->connections, connection->name, profile->connections_mutex);

	return status;
}


/**
 * \brief   This function is used to remove a publisher by name
 *
 * \details This function searches the publishers hash associated with a specific profile for a matching name and if found,
 *			removes it from the hash.
 *
 * \param[in]   *profile	Pointer to the profile containing the publishers hash
 * \param[in]   *name		Name of the publisher to be removed
 *
 * \retval		SWITCH_STATUS_GENERR if the publisher could could not be found
 */

switch_status_t remove_publisher(mosquitto_profile_t *profile, const char *name)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_publisher_t *publisher = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to remove_publisher()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher name not passed to remove_publisher()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!(publisher = locate_publisher(profile, name))) {
		log(WARNING, "Cannot remove unknown publisher: %s from profile %s\n", name, profile->name);
		return SWITCH_STATUS_GENERR;
	}

	switch_core_hash_delete_locked(profile->publishers, publisher->name, profile->publishers_mutex);

	return status;
}


/**
 * \brief   This function is used to remove a subscriber by name
 *
 * \details This function searches the subscribers hash associated with a specific profile for a matching name and if found,
 *			removes it from the hash.
 *
 * \param[in]   *profile	Pointer to the profile containing the subscribers hash
 * \param[in]   *name		Name of the subscriber to be removed
 *
 * \retval		SWITCH_STATUS_GENERR if the subscriber could could not be found
 */

switch_status_t remove_subscriber(mosquitto_profile_t *profile, const char *name)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	mosquitto_subscriber_t *subscriber = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to remove_subscriber()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s subscriber name not passed to remove_subscriber()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if (!(subscriber = locate_subscriber(profile, name))) {
		log(WARNING, "Cannot remove unknown subscriber: %s from profile %s\n", name, profile->name);
		return SWITCH_STATUS_GENERR;
	}

	switch_core_hash_delete_locked(profile->subscribers, subscriber->name, profile->subscribers_mutex);
	status = SWITCH_STATUS_SUCCESS;

	return status;
}


/**
 * \brief   This function is used to add a publisher to a profile
 *
 * \details This function created and adds a publisher has to the specified profile hash.
 *
 * \param[in]   *profile	Pointer to the profile containing the publishers hash
 * \param[in]   *name		Name of the publisher to be added
 *
 * \retval		Pointer to the newly added publisher or NULL
 */

static mosquitto_publisher_t *add_publisher(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_publisher_t *publisher = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_publisher()\n");
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher name not passed to add_publisher()\n", profile->name);
		return NULL;
	}

	if ((publisher = locate_publisher(profile, name))) {
		log(ERROR, "Publisher name: %s is already associated with profile %s and cannot be added\n", name, profile->name);
		return NULL;
	}

	if (!(publisher = switch_core_alloc(profile->pool, sizeof(*publisher)))) {
		log(ERROR, "Failed to allocate memory from pool for profile %s publisher: %s\n", profile->name, name);
		return publisher;
	}

	publisher->name = switch_core_strdup(profile->pool, name);
	publisher->profile_name =switch_core_strdup(profile->pool, profile->name);
	switch_mutex_init(&publisher->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&publisher->rwlock, profile->pool);

	switch_mutex_init(&publisher->topics_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&publisher->topics);

	switch_core_hash_insert_locked(profile->publishers, publisher->name, publisher, profile->publishers_mutex);
	log(INFO, "Profile %s publisher:%s successfully added\n", profile->name, name);

	return publisher;
}


/**
 * \brief   This function is used to locate a publisher by name
 *
 * \details This function searches the publishers hash associated with the profile hash entry in an attempt
 *			to find a publisher with a matching name.  the name is used as a key, so must be unique
 *			(two publishers within the same profile cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *name		Name of the publisher to locate
 *
 * \retval		pointer to the hash entry of the publisher or NULL
 */

mosquitto_publisher_t *locate_publisher(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_publisher_t *publisher = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_publisher()\n");
		return publisher;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher name not passed to locate_publisher()\n", profile->name);
		return publisher;
	}

	if (!(publisher = switch_core_hash_find_locked(profile->publishers, name, profile->publishers_mutex))) {
		log(WARNING, "Unable to locate publisher: %s for provider: %s\n", name, profile->name);
	}

	return publisher;
}


/**
 * \brief   This function is used to add a subscriber to a profile
 *
 * \details This function created and adds a subscriber has to the specified profile hash.
 *
 * \param[in]   *profile	Pointer to the profile containing the subscribers hash
 * \param[in]   *name		Name of the subscriber to be added
 *
 * \retval		Pointer to the newly added subscriber or NULL
 */

static mosquitto_subscriber_t *add_subscriber(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_subscriber_t *subscriber = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_subscriber()\n");
		return subscriber;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s subscriber name not passed to add_subscriber()\n", profile->name);
		return subscriber;
	}

	if ((subscriber = locate_subscriber(profile, name))) {
		log(ERROR, "Subscriber name: %s is already associated with profile %s and cannot be added\n", name, profile->name);
		return NULL;
	}

	if (!(subscriber = switch_core_alloc(profile->pool, sizeof(*subscriber)))) {
		log(ERROR, "Failed to allocate memory from pool for profile %s subscriber:%s\n", profile->name, name);
		return subscriber;
	}

	subscriber->name = switch_core_strdup(profile->pool, name);
	subscriber->profile_name = switch_core_strdup(profile->pool, profile->name);
	switch_mutex_init(&subscriber->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&subscriber->rwlock, profile->pool);

	switch_mutex_init(&subscriber->topics_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&subscriber->topics);

	switch_core_hash_insert_locked(profile->subscribers, subscriber->name, subscriber, profile->subscribers_mutex);
	log(INFO, "Profile %s subscriber: %s successfully added\n", profile->name, name);

	return subscriber;
}


/**
 * \brief   This function is used to locate a subscriber by name
 *
 * \details This function searches the subscribers hash associated with the profile hash entry in an attempt
 *			to find a subscriber with a matching name.  the name is used as a key, so must be unique
 *			(two subscribers within the same profile cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *name		Name of the subscriber to locate
 *
 * \retval		pointer to the hash entry of the subscriber or NULL
 */

mosquitto_subscriber_t *locate_subscriber(mosquitto_profile_t *profile, const char *name)
{
	mosquitto_subscriber_t *subscriber = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_subscriber()\n");
		return subscriber;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s subscriber name not passed to locate_subscriber()\n", profile->name);
		return subscriber;
	}

	if (!(subscriber = switch_core_hash_find_locked(profile->subscribers, name, profile->subscribers_mutex))) {
		log(WARNING, "Profile %s unable to locate subscriber: %s\n", profile->name, name);
	}

	return subscriber;
}


/**
 * \brief   This function is used to parse the subscribers from the configuration file
 *
 * \details	Subscribers are located within the profile section of the configuration file
 *
 * \param[in]   xprofile	Profile section of the configuration file
 * \param[in]   *profile	Pointer to the profile hash that the newly parsed subscribers will be added to
 *
 * \retval		SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_subscribers(switch_xml_t xprofile, mosquitto_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t xsubscribers, xsubscriber, param;

	if ((xsubscribers = switch_xml_child(xprofile, "subscribers"))) {
		for (xsubscriber = switch_xml_child(xsubscribers, "subscriber"); xsubscriber; xsubscriber = xsubscriber->next) {
			mosquitto_subscriber_t *subscriber;
			const char *name = switch_xml_attr(xsubscriber, "name");

			if (zstr(name)) {
				log(ERROR, "Required field name missing\n");
				continue;
			}

			if (locate_subscriber(profile, name)) {
				log(ERROR, "Profile %s subscriber %s already exists\n", profile->name, name);
				continue;
			}

			subscriber = add_subscriber(profile, name);

			for (param = switch_xml_child(xsubscriber, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					subscriber->enable = switch_true(val);
				}
			}

			parse_subscriber_topics(profile, subscriber, xsubscriber);

		}
	}

	return status;
}


/**
 * \brief   This function is used to parse the publishers from the configuration file
 *
 * \details	Publishers are located within the profile section of the configuration file
 *
 * \param[in]   xprofile	Profile section of the configuration file
 * \param[in]   *profile	Pointer to the profile hash that the newly parsed publishers will be added to
 *
 * \retval		SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_publishers(switch_xml_t xprofile, mosquitto_profile_t *profile)
{
	switch_xml_t xpublisher, xpublishers, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((xpublishers = switch_xml_child(xprofile, "publishers"))) {
		for (xpublisher = switch_xml_child(xpublishers, "publisher"); xpublisher; xpublisher = xpublisher->next) {
			mosquitto_publisher_t *publisher;
			const char *name = switch_xml_attr(xpublisher, "name");

			if (zstr(name)) {
				log(ERROR, "Required field name missing\n");
				continue;
			}

			if (locate_publisher(profile, name)) {
				log(ERROR, "Profile %s publisher %s already exists\n", profile->name, name);
				continue;
			}

			publisher = add_publisher(profile, name);

			for (param = switch_xml_child(xpublisher, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					publisher->enable = switch_true(val);
				} else if (!strncasecmp(var, "event", 5) && !zstr(val)) {
				} else {
				}
			}

			parse_publisher_topics(profile, publisher, xpublisher);

		}
	}

	return status;
}


/**
 * \brief   This function is used to add a new topic to an existing publisher
 *
 * \details	Each publisher can have one or more associated topics
 *
 * \param[in]   *profile	Pointer to the profile that the publisher belongs to
 * \param[in]   *publisher	Pointer to the publisher that the will have the topic added
 * \param[in]   name		Name of the topic to be added to the publisher
 *
 * \retval		Address of the newly added topic or NULL
 */

static mosquitto_topic_t *add_publisher_topic(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, const char *name)
{
	mosquitto_topic_t *topic = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_publisher_topic()\n");
		return NULL;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to add_publisher_topic()\n", profile->name);
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher %s topic name not passed to add_publisher_topic()\n", profile->name, publisher->name);
		return NULL;
	}

	if ((topic = locate_publisher_topic(profile, publisher, name))) {
		log(ERROR, "Profile %s publisher %s topic: %s exists and cannot be added\n", profile->name, publisher->name, name);
		return NULL;
	}

	if (!(topic = switch_core_alloc(profile->pool, sizeof(*topic)))) {
		log(ERROR, "Failed to allocate memory from profile %s publisher %s for topic %s\n", profile->name, publisher->name, name);
		return topic;
	}

	topic->name = switch_core_strdup(profile->pool, name);
	topic->publisher_name = switch_core_strdup(profile->pool, publisher->name);
	switch_mutex_init(&topic->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&topic->rwlock, profile->pool);

	switch_mutex_init(&topic->events_mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_core_hash_init(&topic->events);

	switch_core_hash_insert_locked(publisher->topics, topic->name, topic, publisher->topics_mutex);
	log(INFO, "Profile %s publisher %s topic %s successfully added\n", profile->name, publisher->name, topic->name);

	return topic;
}


/**
 * \brief   This function is used to add an event to a publishers topic
 *
 * \details	Each publisher topic can have one or more events bound to it
 *
 * \param[in]   *profile	Pointer to the profile that the publisher belongs to
 * \param[in]   *publisher	Pointer to the publisher that the topic belongs to
 * \param[in]   *topic		Pointer to the topic that will have the event added
 * \param[in]   name		Name of the event to be added to the topic
 *
 * \retval		Address of the newly added event or NULL
 */

static mosquitto_event_t *add_publisher_topic_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, const char *name, switch_event_types_t event_type)
{
	mosquitto_event_t *event = NULL;
	mosquitto_event_userdata_t *userdata = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_publisher_topic_event()\n");
		return NULL;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to add_publisher_topic_event()\n", profile->name);
		return NULL;
	}

	if (!topic) {
		log(ERROR, "Profile %s publisher %s topic not passed to add_publisher_topic_event()\n", profile->name, publisher->name);
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher %s topic %s event name not passed to add_publisher()\n", profile->name, publisher->name, topic->name);
		return NULL;
	}

	if (!(event = switch_core_alloc(profile->pool, sizeof(*event)))) {
		log(ERROR, "Failed to allocate memory from profile %s publisher %s for topic %s event %s\n", profile->name, publisher->name, topic->name, name);
		return event;
	}

	event->userdata = switch_core_alloc(profile->pool, sizeof(*userdata));

	event->name = switch_core_strdup(profile->pool, name);
	event->event_type = event_type;

	switch_core_hash_insert_locked(topic->events, event->name, event, topic->events_mutex);
	log(INFO, "Profile %s publisher %s topic %s event %s successfully added\n", profile->name, publisher->name, topic->name, event->name);

	return event;
}


/**
 * \brief   This function is used to locate a topic by name given a profile and publisher
 *
 * \details This function searches the topic hash asssociated with a publisher and profile in an attempt to
 *		  to find a matching name.  The name is used as a key, so must be unique
 *		  (two topics within the same profile and publisher cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *publisher  Pointer to a publisher hash entry
 * \param[in]   *name	   Name of the topic name to locate
 *
 * \retval	  pointer to the hash entry of the topic or NULL
 */

mosquitto_topic_t *locate_publisher_topic(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, const char *name)
{
	mosquitto_topic_t *topic = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_publisher_topic()\n");
		return NULL;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to locate_publisher_topic()\n", profile->name);
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s publisher %s topic name not passed to locate_publisher_topic()\n", profile->name, publisher->name);
		return NULL;
	}

	if (!(topic = switch_core_hash_find_locked(publisher->topics, name, publisher->topics_mutex))) {
		log(WARNING, "Profile %s publisher %s topic %s not found\n", publisher->profile_name, publisher->name, name);
	}

	return topic;
}


/**
 * \brief   This function is used to locate an event by name given a profile, publisher and topic
 *
 * \details This function searches the event hash asssociated with a publisher, profile and topic in an attempt to
 *		  to find a matching name.  The name is used as a key, so must be unique
 *		  (two events within the same profile, publisher and topic cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *publisher  Pointer to a publisher hash entry
 * \param[in]   *name	   Name of the topic name to locate
 *
 * \retval	  pointer to the hash entry of the topic or NULL
 */

mosquitto_event_t *locate_publisher_topic_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, const char *name)
{
	mosquitto_event_t *event = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_publisher_topic_event()\n");
		return NULL;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to locate_publisher_topic_event()\n", profile->name);
		return NULL;
	}

	if (!topic) {
		log(ERROR, "Profile %s publisher %s topic not passed to locate_publisher_topic_event()\n", profile->name, publisher->name);
		return NULL;
	}

	if (zstr(name)) {
		log(WARNING, "Profile %s publisher %s topic %s event name not passed to locate_publisher_topic_event()\n", profile->name, publisher->name, topic->name);
		return event;
	}

	if (!(event = switch_core_hash_find_locked(topic->events, name, topic->events_mutex))) {
		log(INFO, "Profile %s publisher %s topic %s event %s not found\n", profile->name, publisher->name, topic->name, name);
	}

	return event;
}


/**
 * \brief	This function is used to parse the connection will settings from the configuration file
 *
 * \details	will settings are located within the connection section of the configuration file
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *connection	Pointer to a connection hash entry
 * \param[in]   xconnection	Connection section of the configuration file
 *
 * \retval		SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_connection_will(mosquitto_profile_t *profile, mosquitto_connection_t *connection, switch_xml_t xconnection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t param;

	if (!profile) {
		log(ERROR, "Profile not passed to parse_connection_will()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection) {
		log(ERROR, "Profile %s connection not passed to parse_connection_will()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	//* Set default values for all the possible settings
	connection->will.enable = SWITCH_FALSE;
	connection->will.topic = NULL;
	connection->will.payload = NULL;
	connection->will.qos = 0;
	connection->will.retain = SWITCH_FALSE;

	for (param = switch_xml_child(xconnection, "param"); param; param = param->next) {
		char *var = NULL;
		char *val = NULL;
		var = (char *) switch_xml_attr_soft(param, "name");
		val = (char *) switch_xml_attr_soft(param, "value");

		if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
			connection->will.enable = switch_true(val);
		} else if (!strncasecmp(var, "topic", 5) && !zstr(val)) {
			connection->will.topic = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "payload", 7) && !zstr(val)) {
			connection->will.payload = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "qos", 3) && !zstr(val)) {
			connection->will.qos = atoi(val);
		} else if (!strncasecmp(var, "retain", 6) && !zstr(val)) {
			connection->will.retain = switch_true(val);
		}
	}

	return status;
}


/**
 * \brief   This function is used to parse the connection tls settings from the configuration file
 *
 * \details	tls settings are located within the connection section of the configuration file
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *connection	Pointer to a connection hash entry
 * \param[in]   xconnection	Connection section of the configuration file
 *
 * \retval		SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_connection_tls(mosquitto_profile_t *profile, mosquitto_connection_t *connection, switch_xml_t xconnection)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t param;

	if (!profile) {
		log(ERROR, "Profile not passed to parse_connection_tls()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!connection) {
		log(ERROR, "Profile %s connection not passed to parse_connection_tls()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	//* Set default values for all the possible settings
	connection->tls.enable = SWITCH_FALSE;
	connection->tls.advanced_options = SWITCH_FALSE;
	connection->tls.cafile = NULL;
	connection->tls.capath = NULL;
	connection->tls.certfile = NULL;
	connection->tls.keyfile = NULL;
	connection->tls.cert_reqs = SSL_VERIFY_NONE;
	connection->tls.version = NULL;
	connection->tls.opts_ciphers = NULL;
	connection->tls.psk_ciphers = NULL;
	connection->tls.psk = NULL;
	connection->tls.identity = NULL;
	connection->tls.port = 0;

	for (param = switch_xml_child(xconnection, "param"); param; param = param->next) {
		char *var = NULL;
		char *val = NULL;
		var = (char *) switch_xml_attr_soft(param, "name");
		val = (char *) switch_xml_attr_soft(param, "value");

		if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
			if (!strncasecmp(val, "certificate", 11)) {
				connection->tls.enable = TLS_CERT;
			} else if (!strncasecmp(val, "psk", 3)) {
				connection->tls.enable = TLS_PSK;
			}
		} else if (!strncasecmp(var, "port", 4) && !zstr(val)) {
			connection->tls.port = atoi(val);
			if (connection->tls.port > 65535) {
				connection->tls.port = 65535;
			}
		} else if (!strncasecmp(var, "advanced_options", 16) && !zstr(val)) {
			connection->tls.advanced_options = switch_true(val);
		} else if (!strncasecmp(var, "cafile", 6) && !zstr(val)) {
			connection->tls.cafile = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "capath", 6) && !zstr(val)) {
			connection->tls.capath = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "certfile", 8) && !zstr(val)) {
			connection->tls.certfile = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "keyfile", 7) && !zstr(val)) {
			connection->tls.keyfile = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "cert_reqs", 9) && !zstr(val)) {
			if (!strncasecmp(val, "SSL_VERIFY_NONE", 15)) {
				connection->tls.cert_reqs = SSL_VERIFY_NONE;
			} else if (!strncasecmp(val, "SSL_VERIFY_PEER", 15)) {
				connection->tls.cert_reqs = SSL_VERIFY_PEER;
			}
		} else if (!strncasecmp(var, "version", 7) && !zstr(val)) {
			connection->tls.version = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "opts_ciphers", 12) && !zstr(val)) {
			connection->tls.opts_ciphers = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "psk_ciphers", 11) && !zstr(val)) {
			connection->tls.psk_ciphers = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "psk", 3) && !zstr(val)) {
			connection->tls.psk = switch_core_strdup(profile->pool, val);
		} else if (!strncasecmp(var, "identity", 8) && !zstr(val)) {
			connection->tls.identity = switch_core_strdup(profile->pool, val);
		}
	}

	return status;
}

/**
 * \brief   This function is used to parse the publisher topics from the configuration file
 *
 * \details	topics are located within the publishers section of the configuration file
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *publisher	Pointer to a publisher hash entry
 * \param[in]   xpublisher	Publisher section of the configuration file
 *
 * \retval		SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_publisher_topics(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, switch_xml_t xpublisher)
{
	switch_xml_t xtopic, xtopics, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!profile) {
		log(ERROR, "Profile not passed to parse_publisher_topics()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to parse_publisher_topics()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}

	if ((xtopics = switch_xml_child(xpublisher, "topics"))) {
		for (xtopic = switch_xml_child(xtopics, "topic"); xtopic; xtopic = xtopic->next) {
			mosquitto_topic_t *topic = NULL;
			const char *name = switch_xml_attr(xtopic, "name");

			if (zstr(name)) {
				log(ERROR, "Required field name missing\n");
				continue;
			}

			if (locate_publisher_topic(profile, publisher, name)) {
				log(ERROR, "Profile %s publisher %s topic %s already exists\n", profile->name, publisher->name, name);
				continue;
			}

			topic = add_publisher_topic(profile, publisher, name);

			for (param = switch_xml_child(xtopic, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;
				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					topic->enable = switch_true(val);
				} else if (!strncasecmp(var, "connection_name", 15) && !zstr(val)) {
					topic->connection_name = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "pattern", 7) && !zstr(val)) {
					topic->pattern = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "qos", 3) && !zstr(val)) {
					topic->qos = atoi(val);
				} else if (!strncasecmp(var, "retain", 6) && !zstr(val)) {
					topic->retain = switch_true(val);
				} else if (!strncasecmp(var, "event", 5)) {
					mosquitto_event_t *event;
					switch_event_types_t event_type;

					if (switch_name_event(val, &event_type) != SWITCH_STATUS_SUCCESS) {
						log(CRIT, "Profile %s publisher %s topic %s event %s was not recognised.\n", profile->name, publisher->name, topic->name, val);
						continue;
					}

					if (locate_publisher_topic_event(profile, publisher, topic, val)) {
						log(ERROR, "Profile %s publisher %s topic %s event %s already exists\n", profile->name, publisher->name, topic->name, val);
						continue;
					}

					if (!(event = add_publisher_topic_event(profile, publisher, topic, val, event_type))) {
						log(ERROR, "Profile %s publisher %s topic %s event %si failed\n", profile->name, publisher->name, topic->name, event->name);
						continue;
					}
					log(INFO, "Profile %s publisher %s topic %s event %s\n", profile->name, publisher->name, topic->name, event->name);

				} else {
					log(ERROR, "Profile %s publisher %s topic %s unknown param %s\n", profile->name, publisher->name, topic->name, var);
				}
			}
		}
	}

	return status;
}


/**
 * \brief   This function is used to add a new topic to an existing subscriber
 *
 * \details Each subscriber can have one or more associated topics
 *
 * \param[in]   *profile	Pointer to the profile that the subscriber belongs to
 * \param[in]   *subscriber Pointer to the subscriber that the will have the topic added
 * \param[in]   name		Name of the topic to be added to the subscriber
 *
 * \retval	  Address of the newly added topic or NULL
 */

static mosquitto_topic_t *add_subscriber_topic(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, const char *name)
{
	mosquitto_topic_t *topic = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to add_subscriber_topic()\n");
		return NULL;
	}

	if (!subscriber) {
		log(ERROR, "Profile %s subscriber not passed to add_subscriber_topic()\n", profile->name);
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Profile %s subscriber %s topic name not passed to add_subsctiber_topic()\n", profile->name, subscriber->name);
		return NULL;
	}


	if (!(topic = switch_core_alloc(profile->pool, sizeof(*topic)))) {
		log(ERROR, "Failed to allocate memory from profile %s subscriber %s for topic %s\n", profile->name, subscriber->name, name);
		return topic;
	}

	topic->name = switch_core_strdup(profile->pool, name);
	topic->subscriber_name = switch_core_strdup(profile->pool, subscriber->name);
	switch_mutex_init(&topic->mutex, SWITCH_MUTEX_NESTED, profile->pool);
	switch_thread_rwlock_create(&topic->rwlock, profile->pool);

	switch_core_hash_insert_locked(subscriber->topics, topic->name, topic, subscriber->topics_mutex);
	log(INFO, "Profile %s subscriber %s topic %s successfully added\n", profile->name, subscriber->name, name);

	return topic;
}


/**
 * \brief   This function is used to locate a topic by name given a profile and subscriber
 *
 * \details This function searches the topic hash asssociated with a subscriber and profile in an attempt to
 *		  to find a matching name.  The name is used as a key, so must be unique
 *		  (two topics within the same profile and subscriber cannot have the same name)
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *subscriber Pointer to a subscriber hash entry
 * \param[in]   *name	   Name of the topic name to locate
 *
 * \retval	  pointer to the hash entry of the topic or NULL
 */

mosquitto_topic_t *locate_subscriber_topic(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, const char *name)
{
	mosquitto_topic_t *topic = NULL;

	if (!profile) {
		log(ERROR, "Profile not passed to locate_subscriber_topic()\n");
		return NULL;
	}

	if (!subscriber) {
		log(ERROR, "Profile %s subscriber not passed to locate_subscriber_topic()\n", profile->name);
		return NULL;
	}

	if (zstr(name)) {
		log(ERROR, "Topic name not passed to locate_subscriber_topic()\n");
		return topic;
	}

	if (!(topic = switch_core_hash_find_locked(subscriber->topics, name, subscriber->topics_mutex))) {
		log(INFO, "Unable to locate profile %s subscriber %s topic %s\n", subscriber->profile_name, subscriber->name, name);
	}

	return topic;
}


/**
 * \brief   This function is used to parse the subscriber topics from the configuration file
 *
 * \details topics are located within the subscribers section of the configuration file
 *
 * \param[in]   *profile	Pointer to a profile hash entry
 * \param[in]   *subscriber Pointer to a subscriber hash entry
 * \param[in]   xsubscriber Subscriber section of the configuration file
 *
 * \retval	  SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_subscriber_topics(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, switch_xml_t xsubscriber)
{
	switch_xml_t xtopic, xtopics, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((xtopics = switch_xml_child(xsubscriber, "topics"))) {
		for (xtopic = switch_xml_child(xtopics, "topic"); xtopic; xtopic = xtopic->next) {
			mosquitto_topic_t *topic = NULL;
			const char *name = switch_xml_attr(xtopic, "name");

			if (zstr(name)) {
				log(ERROR, "Profile %s subscriber %s Required field name missing\n", profile->name, subscriber->name);
				continue;
			}

			if (locate_subscriber_topic(profile, subscriber, name)) {
				log(ERROR, "Profile %s subscriber %s topic %s already exists\n", profile->name, subscriber->name, name);
				continue;
			}

			topic = add_subscriber_topic(profile, subscriber, name);

			for (param = switch_xml_child(xtopic, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;
				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					topic->enable = switch_true(val);
				} else if (!strncasecmp(var, "connection_name", 15) && !zstr(val)) {
					topic->connection_name = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "pattern", 7) && !zstr(val)) {
					topic->pattern = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "qos", 3) && !zstr(val)) {
					topic->qos = atoi(val);
				} else if (!strncasecmp(var, "retain", 6) && !zstr(val)) {
					topic->retain = switch_true(val);
				} else if (!strncasecmp(var, "originate_authorized", 20) && !zstr(val)) {
					topic->originate_authorized = switch_true(val);
				} else if (!strncasecmp(var, "bgapi_authorized", 16) && !zstr(val)) {
					topic->bgapi_authorized = switch_true(val);
				} else {
				}
			}
		}
	}

	return status;
}


/**
 * \brief   This function is used to parse the connections from the configuration file
 *
 * \details Connections located within the profile section of the configuration file
 *
 * \param[in]   xprofile	Profile section of the configuration file
 * \param[in]   *profile	Pointer to the profile hash that the newly parsed connections will be added to
 *
 * \retval	  SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_connections(switch_xml_t xprofile, mosquitto_profile_t *profile)
{
	switch_xml_t xconnection, xconnections, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((xconnections = switch_xml_child(xprofile, "connections"))) {
		for (param = switch_xml_child(xconnections, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;
			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");
			log(ERROR, "Connection defaults: %s %s\n", var, val);
		}

		for (xconnection = switch_xml_child(xconnections, "connection"); xconnection; xconnection = xconnection->next) {
			mosquitto_connection_t *connection;
			const char *name = switch_xml_attr(xconnection, "name");

			if (zstr(name)) {
				log(ERROR, "Required field name missing\n");
				continue;
			}

			if (locate_connection(profile, name)) {
				log(ERROR, "Profile %s connection %s already exists\n", profile->name, name);
				continue;
			}

			connection = add_connection(profile, name);

			parse_connection_tls(profile, connection, switch_xml_child(xconnection, "tls"));
			parse_connection_will(profile, connection, switch_xml_child(xconnection, "will"));

			for (param = switch_xml_child(xconnection, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					connection->enable = switch_true(val);
				} else if (!strncasecmp(var, "host", 4) && !zstr(val)) {
					connection->host = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "port", 4) && !zstr(val)) {
					connection->port = atoi(val);
					if (connection->port > 65535) {
						connection->port = 65535;
					}
				} else if (!strncasecmp(var, "keepalive", 9) && !zstr(val)) {
					connection->keepalive = atoi(val);
				} else if (!strncasecmp(var, "username", 8) && !zstr(val)) {
					connection->username = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "password", 8) && !zstr(val)) {
					connection->password = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "bind_address", 12) && !zstr(val)) {
					connection->bind_address = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "client_id", 9)) {
					char random_string[RANDOM_STRING_LENGTH];
					char destination_string[DESTINATION_STRING_LENGTH];
					memset(destination_string, '\0', sizeof(destination_string));
					if (!strncasecmp(val, "FreeSWITCH-Hostname-Unique", 26)) {
						rand_str(random_string, mosquitto_globals.unique_string_length);
						strcpy(destination_string, switch_core_get_hostname());
						strcat(destination_string, "-");
						strncat(destination_string, random_string, DESTINATION_STRING_LENGTH - 1);
						connection->client_id = switch_core_strdup(profile->pool, destination_string);
						memset(destination_string, '\0', sizeof(destination_string));
					} else if (!strncasecmp(val, "FreeSWITCH-Hostname", 19)) {
						connection->client_id = switch_core_strdup(profile->pool, switch_core_get_hostname());
					} else if (!strncasecmp(val, "FreeSWITCH-Switchname-Unique", 28)) {
						rand_str(random_string, mosquitto_globals.unique_string_length);
						strcpy(destination_string, switch_core_get_switchname());
						strcat(destination_string, "-");
						strncat(destination_string, random_string, DESTINATION_STRING_LENGTH - 1);
						connection->client_id = switch_core_strdup(profile->pool, destination_string);
						memset(destination_string, '\0', sizeof(destination_string));
					} else if (!strncasecmp(val, "FreeSWITCH-Switchname", 21)) {
						connection->client_id = switch_core_strdup(profile->pool, switch_core_get_switchname());
					} else if (!zstr(val)) {
						connection->client_id = switch_core_strdup(profile->pool, val);
					}
				} else if (!strncasecmp(var, "clean_session", 13) && !zstr(val)) {
					connection->clean_session = switch_true(val);
				} else if (!strncasecmp(var, "retries", 7) && !zstr(val)) {
					connection->retries = atoi(val);
				} else if (!strncasecmp(var, "max_inflight_messages", 21) && !zstr(val)) {
					connection->retries = atoi(val);
				} else if (!strncasecmp(var, "reconnect_delay", 15) && !zstr(val)) {
					connection->reconnect_delay = atoi(val);
				} else if (!strncasecmp(var, "reconnect_delay_max", 19) && !zstr(val)) {
					connection->reconnect_delay_max = atoi(val);
				} else if (!strncasecmp(var, "reconnect_exponential_backoff", 29) && !zstr(val)) {
					connection->reconnect_exponential_backoff = switch_true(val);
				} else if (!strncasecmp(var, "protocol_version", 16) && !zstr(val)) {
					connection->protocol_version = switch_core_strdup(profile->pool, val);
				} else if (!strncasecmp(var, "receive_maximum", 15) && !zstr(val)) {
					connection->receive_maximum = atoi(val);
				} else if (!strncasecmp(var, "send_maximum", 12) && !zstr(val)) {
					connection->send_maximum = atoi(val);
				} else {
					log(ERROR, "Connection %s unknown parameter: %s value: %s\n", name, var, val);
				}
			}

		}
	}

	return status;
}


/**
 * \brief   This function is used to parse the profiles from the configuration file
 *
 * \details Profiles are located at the top level of the configuration file
 *
 * \param[in]   cfg		The top of the configuration file
 *
 * \retval	  SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_profiles(switch_xml_t cfg)
{
	switch_xml_t xprofile, xprofiles, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((xprofiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
			mosquitto_profile_t *profile;
			const char *name = switch_xml_attr(xprofile, "name");

			if (zstr(name)) {
				log(ERROR, "Required field name missing\n");
				continue;
			}

			if (locate_profile(name) != NULL) {
				log(ERROR, "Profile %s already exists, cannot add a duplicate name.\n", name);
				continue;
			}

			profile = add_profile(name);

			for (param = switch_xml_child(xprofile, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				if (!strncasecmp(var, "enable", 6) && !zstr(val)) {
					profile->enable = switch_true(val);
				}
			}
			parse_connections(xprofile, profile);
			parse_publishers(xprofile, profile);
			parse_subscribers(xprofile, profile);
		}
	}

	return status;
}


/**
 * \brief   This function is used to parse the global settings from the configuration file
 *
 * \details The global settings are located at the top level of the configuration file
 *
 * \param[in]   cfg		The top of the configuration file
 *
 * \retval	  SWITCH_STATUS_SUCCESS indicates this routine completed
 */

static switch_status_t parse_settings(switch_xml_t cfg)
{
	switch_xml_t settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	mosquitto_globals.loglevel = SWITCH_LOG_DEBUG;
	mosquitto_globals.enable_profiles = SWITCH_FALSE;
	mosquitto_globals.enable_publishers = SWITCH_FALSE;
	mosquitto_globals.enable_subscribers = SWITCH_FALSE;
	mosquitto_globals.enable_connections = SWITCH_FALSE;
	mosquitto_globals.enable_topics = SWITCH_FALSE;
	mosquitto_globals.enable_events = SWITCH_FALSE;
	mosquitto_globals.unique_string_length = UNIQUE_STRING_LENGTH;
	mosquitto_globals.event_queue_size = EVENT_QUEUE_SIZE;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;

			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");

			if (!strncasecmp(var, "enable-profiles", 15) && !zstr(val)) {
				mosquitto_globals.enable_profiles = switch_true(val);
			} else if (!strncasecmp(var, "enable-publishers", 17) && !zstr(val)) {
				mosquitto_globals.enable_publishers = switch_true(val);
			} else if (!strncasecmp(var, "enable-subscribers", 18) && !zstr(val)) {
				mosquitto_globals.enable_subscribers = switch_true(val);
			} else if (!strncasecmp(var, "enable-connections", 18) && !zstr(val)) {
				mosquitto_globals.enable_connections = switch_true(val);
			} else if (!strncasecmp(var, "enable-topics", 13) && !zstr(val)) {
				mosquitto_globals.enable_topics = switch_true(val);
			} else if (!strncasecmp(var, "enable-events", 13) && !zstr(val)) {
				mosquitto_globals.enable_events = switch_true(val);
			} else if (!strncasecmp(var, "event-queue-size",16 ) && !zstr(val)) {
				size_t event_queue_size = atoi(val);
				if (event_queue_size) {
					mosquitto_globals.event_queue_size = event_queue_size;
				}
			} else if (!strncasecmp(var, "unique-string-length", 20) && !zstr(val)) {
				size_t len = atoi(val);
				if (!len) {
					len = UNIQUE_STRING_LENGTH;
				} else if (len > RANDOM_STRING_LENGTH) {
					len = RANDOM_STRING_LENGTH;
				}
				mosquitto_globals.unique_string_length = len;
			} else if (!strncasecmp(var, "loglevel", 9) && !zstr(val)) {
				if (!strncasecmp(val, "debug", 5)) {
					mosquitto_globals.loglevel = SWITCH_LOG_DEBUG;
				} else if (!strncasecmp(val, "info", 4)) {
					mosquitto_globals.loglevel = SWITCH_LOG_INFO;
				} else if (!strncasecmp(val, "notice", 6)) {
					mosquitto_globals.loglevel = SWITCH_LOG_NOTICE;
				} else if (!strncasecmp(val, "warning", 7)) {
					mosquitto_globals.loglevel = SWITCH_LOG_WARNING;
				} else if (!strncasecmp(val, "error", 5)) {
					mosquitto_globals.loglevel = SWITCH_LOG_ERROR;
				} else if (!strncasecmp(val, "critical", 8)) {
					mosquitto_globals.loglevel = SWITCH_LOG_CRIT;
				} else if (!strncasecmp(val, "alert", 5)) {
					mosquitto_globals.loglevel = SWITCH_LOG_ALERT;
				} else if (!strncasecmp(val, "console", 7)) {
					mosquitto_globals.loglevel = SWITCH_LOG_CONSOLE;
				}
			}
		}
	}
	return status;
}


/**
 * \brief   This function is the entry routine to parse the configuration file
 *
 * \details This routine is called during mod_mosquitto module load processing.
 *
 * \param[in]   cf	Name of the configuration file
 *
 * \retval	  SWITCH_STATUS_SUCCESS indicates this routine completed
 */

switch_status_t mosquitto_load_config(const char *cf)
{
	switch_xml_t cfg, xml;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		log(ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (parse_settings(cfg) != SWITCH_STATUS_SUCCESS) {
		log(ERROR, "Failed to successfully parse settings\n");
	} else if (parse_profiles(cfg) != SWITCH_STATUS_SUCCESS) {
		log(ERROR, "Failed to successfully parse profiles\n");
	}

	switch_xml_free(xml);

	return status;
}


/**
 * \brief   This function creates a string of random characters
 *
 * \details There is a requirement for uniqueness in client_id's connected to the same MQTT broker
 *			This helper routine creates random strings that are appended to non-unique values such
 *			as switchname or hostname.
 *
 * \param[in]   dest	Pointer to a char buffer that will contain the random characters
 * \param[in]	length	Number of random characters to create
 *
 */

static void rand_str(char *dest, size_t length)
{
	char charset[] = "0123456789"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	while (length-- > 0) {
		size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
		*dest++ = charset[index];
	}
	*dest = '\0';
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
