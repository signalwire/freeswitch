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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* kazoo.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#ifndef KAZOO_MESSAGE_H
#define KAZOO_MESSAGE_H

#include <switch.h>

typedef struct {
    uint64_t timestamp;
    uint64_t start;
    uint64_t filter;
    uint64_t serialize;
    uint64_t print;
    uint64_t rk;
} kazoo_message_times_t, *kazoo_message_times_ptr;

typedef struct {
    cJSON *JObj;
} kazoo_message_t, *kazoo_message_ptr;

void kazoo_cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
cJSON * kazoo_event_add_field_to_json(cJSON *dst, switch_event_t *src, kazoo_field_ptr field);
kazoo_message_ptr kazoo_message_create_event(switch_event_t* evt, kazoo_event_ptr event, kazoo_event_profile_ptr profile);
kazoo_message_ptr kazoo_message_create_fetch(switch_event_t* evt, kazoo_fetch_profile_ptr profile);

void kazoo_message_destroy(kazoo_message_ptr *msg);


#endif /* KAZOO_MESSAGE_H */

