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
#include "mosquitto_utils.h"
#include "mosquitto_events.h"
#include "mosquitto_config.h"

static void mosq_publish_results(mosquitto_profile_t *profile, mosquitto_connection_t *connection, mosquitto_topic_t *topic, int rc);

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


switch_status_t bind_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, mosquitto_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	mosquitto_event_userdata_t *userdata = NULL;
	mosquitto_connection_t *connection = NULL;

	if (event->bound) {
		log(DEBUG, "Event already bound: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	}

	userdata = event->userdata;

	userdata->profile = profile;
	userdata->publisher = publisher;
	userdata->topic = topic;


	if (!(connection = locate_connection(profile, topic->connection_name))) {
		log(ERROR, "Cannot bind to topic %s because connection %s (profile %s) is invalid\n", topic->name, topic->connection_name, profile->name);
		return SWITCH_STATUS_GENERR;
	}

	userdata->connection = connection;

	snprintf(event->event_id, sizeof(event->event_id), "%s-%s-%s", profile->name, publisher->name, topic->name);

	if ((switch_event_bind_removable(event->event_id, event->event_type, SWITCH_EVENT_SUBCLASS_ANY, event_handler, userdata, &event->node)) != SWITCH_STATUS_SUCCESS) {
		log(DEBUG, "failed to bind event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		event->bound = SWITCH_FALSE;
		return SWITCH_STATUS_GENERR;
	} else {
		log(DEBUG, "Bound event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		event->bound = SWITCH_TRUE;
	}

	return status;
}


switch_status_t unbind_event(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic, mosquitto_event_t *event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!event->bound) {
		log(DEBUG, "Event already unbound: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_unbind(&event->node) != SWITCH_STATUS_SUCCESS) {
		log(DEBUG, "failed to unbind event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		return SWITCH_STATUS_GENERR;
	} else {
		log(DEBUG, "Unbound event: profile %s publisher %s topic %s event %s (%d)\n", profile->name, publisher->name, topic->name, event->name, (int)event->event_type);
		memset(event->event_id, '\0', sizeof(event->event_id));
		event->bound = SWITCH_FALSE;
	}

	return status;
}


static void mosq_publish_results(mosquitto_profile_t *profile, mosquitto_connection_t *connection, mosquitto_topic_t *topic, int rc)
{
	switch (rc) {
		case MOSQ_ERR_SUCCESS:
			log(DEBUG, "Event handler: published to [%s][%s] %s\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_INVAL:
			log(DEBUG, "Event handler: failed to publish to [%s][%s] %s invalid input parameters \n", profile->name, connection->name, topic->pattern);
			if (connection->enable) {
				connection_initialize(profile, connection);
			}
			break;
		case MOSQ_ERR_NOMEM:
			log(DEBUG, "Event handler: failed to publish to [%s][%s] %s out of memory\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_NO_CONN:
			log(DEBUG, "Event handler: failed to publish to [%s][%s] %s not connected to broker\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_PROTOCOL:
			log(DEBUG, "Event handler: failed to publish to [%s][%s] %s protocol error communicating with the broker\n", profile->name, connection->name, topic->pattern);
			break;
		case MOSQ_ERR_PAYLOAD_SIZE:
			log(DEBUG, "Event handler: failed to publish to [%s][%s] %s payload is too large\n", profile->name, connection->name, topic->pattern);
			break;
		default:
			log(DEBUG, "Event handler: unknown return code %d from publish\n", rc);
			break;
	}
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
