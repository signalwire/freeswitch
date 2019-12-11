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
#include "mosquitto_utils.h"
#include "mosquitto_events.h"
#include "mosquitto_config.h"
#include "mosquitto_mosq.h"


/**
 * \brief   This is the primary event handler.  It is called when a bound FreeSWITCH event is fired.
 *
 * \details This routine is called for ALL events regardless of the profile or publisher used.
 *			Details around how to process the event are located in the userdata structure passed with event.
 *
 * \param[in]   *event	Pointer to an event structure
 */

void event_handler(switch_event_t *event)
{
	mosquitto_event_userdata_t *userdata = NULL;
	mosquitto_profile_t *profile = NULL;
	mosquitto_publisher_t *publisher = NULL;
	mosquitto_topic_t *topic = NULL;
	mosquitto_connection_t *connection = NULL;
	char *buf;
	int rc;
	const char *event_name = switch_event_get_header(event, "Event-Name");

	if (!mosquitto_globals.running) {
		return;
	}

	if (event->bind_user_data) {
		userdata = (mosquitto_event_userdata_t *)event->bind_user_data;
	} else {
		return;
	}

	if (!(profile = userdata->profile)) {
		log(DEBUG, "Event handler: userdata has NULL profile\n");
		return;
	}

	if (!profile->enable) {
		log(DEBUG, "Event handler: cannot publish because profile %s is disabled\n", profile->name);
		return;
	}

	if (!(publisher = userdata->publisher)) {
		log(DEBUG, "Event handler: userdata has NULL publisher\n");
		return;
	}

	if (!publisher->enable) {
		log(DEBUG, "Event handler: cannot publish because profile %s publisher %s is disabled\n", profile->name, publisher->name);
		return;
	}

	if (!(topic = userdata->topic)) {
		log(DEBUG, "Event handler: userdata has NULL topic\n");
		return;
	}

	if (!topic->enable) {
		log(DEBUG, "Event handler: cannot publish because profile %s publisher %s itopic %s is disabled\n", profile->name, publisher->name, topic->name);
		return;
	}

	if (!(connection = userdata->connection)) {
		log(DEBUG, "Event handler: userdata has NULL connection\n");
		return;
	} else {
		if (!(locate_connection(profile, connection->name))) {
			log(ERROR, "Cannot publish to topic %s because connection %s (profile %s) is invalid\n", topic->name, topic->connection_name, profile->name);
			return;
		} else if (!connection->enable) {
			log(ERROR, "Cannot publish to topic %s because connection %s (profile %s) is disabled\n", topic->name, connection->name, profile->name);
			return;
		} else if (!connection->connected) {
			log(ERROR, "Cannot publish to topic %s because connection %s (profile %s) is not connected\n", topic->name, connection->name, profile->name);
			log(ERROR, "Confirm MQTT broker is reachable, then try command: mosquitto enable profile %s connection %s\n", profile->name, connection->name);
			log(ERROR, "Attempting to automatically initialize the connection to profile %s connection %s\n", profile->name, connection->name);
			connection_initialize(profile, connection);
			return;
		}
	}

	switch_event_serialize_json(event, &buf);
	log(DEBUG, "event_handler(): %s\n", buf);

	rc = mosquitto_publish(connection->mosq, NULL, topic->pattern, strlen(buf)+1, buf, topic->qos, topic->retain);
	log(DEBUG, "Event %s published to Topic %s for profile %s publisher %s connection %s rc %d\n", event_name, topic->pattern, profile->name, publisher->name, connection->name, rc);
	mosq_publish_results(profile, connection, topic, rc);
	switch_safe_free(buf);

	if (rc == 0) {
		connection->count++;
		publisher->count++;
		topic->count++;
	}

}

/**
 * \brief   This functions binds an event to FreeSWITCH.
 *
 * \details This routine sets up the userdata structure and binds to an event.  The userdata allows the fired event to
 *			be processed by the correct publisher.
 *
 * \param[in]   *profile	Pointer to the profile associated with this event
 * \param[in]   *publisher	Pointer to the publisher associated with this event
 * \param[in]   *topic		Pointer to the topic associated with this event
 * \param[in]	*event		Pointer to an event structure
 *
 * \retval SWITCH_STATUS_GENERR if there was a problem binding to the event
 *
 */

switch_status_t bind_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, mosquitto_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_event_userdata_t *userdata = NULL;
	mosquitto_connection_t *connection = NULL;

	if (event->bound) {
		log(DEBUG, "Event already bound: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	}

	if (!profile) {
		log(ERROR, "Profile not passed to bind_event()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to bind_event()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}
	if (!topic) {
		log(ERROR, "Profile %s publisher %s topic not passed to bind_event()\n", profile->name, publisher->name);
		return SWITCH_STATUS_GENERR;
	}
	if (!event) {
		log(ERROR, "Profile %s publisher %s topic %s event not passed to bind_event()\n", profile->name, publisher->name, topic->name);
		return SWITCH_STATUS_GENERR;
	}

	userdata = event->userdata;

	userdata->profile = profile;
	userdata->publisher = publisher;
	userdata->topic = topic;

	if (!(connection = locate_connection(profile, topic->connection_name))) {
		log(ERROR, "Profile %s topic %s connection %s not found for bind_event()\n", profile->name, topic->name, topic->connection_name);
		return SWITCH_STATUS_GENERR;
	}

	userdata->connection = connection;

	snprintf(event->event_id, sizeof(event->event_id), "%s-%s-%s", profile->name, publisher->name, topic->name);

	if ((switch_event_bind_removable(event->event_id, event->event_type, SWITCH_EVENT_SUBCLASS_ANY, event_handler, userdata, &event->node)) != SWITCH_STATUS_SUCCESS) {
		log(ERROR, "failed to bind event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		event->bound = SWITCH_FALSE;
		return SWITCH_STATUS_GENERR;
	} else {
		log(INFO, "Bound event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		event->bound = SWITCH_TRUE;
	}

	return status;
}


/**
 * \brief   This functions unbinds an event to FreeSWITCH.
 *
 * \details This routine sets up the userdata structure and binds to an event.  The userdata allows the fired event to
 *			be processed by the correct publisher.
 *
 * \param[in]   *profile	Pointer to the profile associated with this event
 * \param[in]   *publisher	Pointer to the publisher associated with this event
 * \param[in]   *topic		Pointer to the topic associated with this event
 * \param[in]	*event		Pointer to an event structure
 *
 * \retval SWITCH_STATUS_GENERR if there was a problem binding to the event
 *
 */

switch_status_t unbind_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, mosquitto_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!event->bound) {
		log(DEBUG, "Event already unbound: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	}

	if (!profile) {
		log(ERROR, "Profile not passed to unbind_event()\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!publisher) {
		log(ERROR, "Profile %s publisher not passed to unbind_event()\n", profile->name);
		return SWITCH_STATUS_GENERR;
	}
	if (!topic) {
		log(ERROR, "Profile %s publisher %s topic not passed to unbind_event()\n", profile->name, publisher->name);
		return SWITCH_STATUS_GENERR;
	}
	if (!event) {
		log(ERROR, "Profile %s publisher %s topic %s event not passed to unbind_event()\n", profile->name, publisher->name, topic->name);
		return SWITCH_STATUS_GENERR;
	}


	if (switch_event_unbind(&event->node) != SWITCH_STATUS_SUCCESS) {
		log(ERROR, "failed to unbind event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	} else {
		log(INFO, "Unbound event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		memset(event->event_id, '\0', sizeof(event->event_id));
		event->bound = SWITCH_FALSE;
	}

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
