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
 * mod_mosquitto -- Interface to an MQTT broker using Mosquitto
 *				  Implements a Publish/Subscribe (pub/sub) messaging pattern using the Mosquitto API library
 *				  Publishes FreeSWITCH events to one more more MQTT brokers
 *				  Subscribes to topics located on one more more MQTT brokers
 *
 * MQTT http://mqtt.org/
 * Mosquitto https://mosquitto.org/
 *
 */

#ifndef MOSQUITTO_UTILS_H
#define MOSQUITTO_UTILS_H

#include "mosquitto.h"
#include "mod_mosquitto.h"

switch_status_t client_connect(mosquitto_profile_t *profile, mosquitto_connection_t *connection);
switch_status_t initialize_profiles(void);
switch_status_t profile_activate(mosquitto_profile_t *profile);
switch_status_t profile_deactivate(mosquitto_profile_t *profile);
switch_status_t publisher_activate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher);
switch_status_t publisher_deactivate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher);
switch_status_t publisher_topic_activate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic);
switch_status_t publisher_topic_deactivate(mosquitto_profile_t *profile, mosquitto_publisher_t *publisher, mosquitto_topic_t *topic);
switch_status_t subscriber_activate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber);
switch_status_t subscriber_deactivate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber);
switch_status_t subscriber_topic_activate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic);
switch_status_t subscriber_topic_deactivate(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic);
mosquitto_topic_t *locate_connection_topic(mosquitto_profile_t *profile, mosquitto_connection_t *connection, const char *name);
switch_status_t connection_initialize(mosquitto_profile_t *profile, mosquitto_connection_t *connection);
void mosq_log(switch_log_level_t severity, char *fmt, ...);


#endif //MOSQUITTO_UTILS_H

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
