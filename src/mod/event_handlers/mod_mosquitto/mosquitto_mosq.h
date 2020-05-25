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

#ifndef MOSQUITTO_MOSQ_H
#define MOSQUITTO_MOSQ_H

#include "mosquitto.h"
#include "mod_mosquitto.h"

void mosq_callbacks_set(mosquitto_connection_t *connection);
void mosq_disconnect_callback(struct mosquitto *mosq, void *user_data, int rc);
void mosq_publish_callback(struct mosquitto *mosq, void *user_data, int message_id);
void mosq_message_callback(struct mosquitto *mosq, void *user_data, const struct mosquitto_message *message);
void mosq_subscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos);
void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str);
void mosq_connect_callback(struct mosquitto *mosq, void *user_data, int result);
void mosq_publish_results(mosquitto_profile_t *profile, mosquitto_connection_t *connection, mosquitto_topic_t *topic, int rc);
void *SWITCH_THREAD_FUNC bgapi_exec(switch_thread_t *thread, void *obj);

switch_status_t mosq_startup(void);
switch_status_t mosq_shutdown(void);
switch_status_t mosq_reconnect_delay_set(mosquitto_connection_t *connection);
switch_status_t mosq_message_retry_set(mosquitto_connection_t *connection);
switch_status_t mosq_max_inflight_messages_set(mosquitto_connection_t *connection);
switch_status_t mosq_username_pw_set(mosquitto_connection_t *connection);
switch_status_t mosq_tls_set(mosquitto_connection_t *connection);
switch_status_t mosq_tls_opts_set(mosquitto_connection_t *connection);
switch_status_t mosq_tls_psk_set(mosquitto_connection_t *connection);
switch_status_t mosq_will_set(mosquitto_connection_t *connection);
switch_status_t mosq_connect(mosquitto_connection_t *connection);
switch_status_t mosq_disconnect(mosquitto_connection_t *connection);
switch_status_t mosq_new(mosquitto_profile_t *profile, mosquitto_connection_t *connection);
switch_status_t mosq_loop_stop(mosquitto_connection_t *connection, switch_bool_t force);
switch_status_t mosq_int_option(mosquitto_connection_t *connection);
switch_status_t mosq_subscribe(mosquitto_profile_t *profile, mosquitto_subscriber_t *subscriber, mosquitto_topic_t *topic);

#endif //MOSQUITTO_MOSQ_H


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
