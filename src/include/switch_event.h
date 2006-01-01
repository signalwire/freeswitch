/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_event.h -- Event System
 *
 */
/*! \file switch_event.h
    \brief Event System
*/

#ifndef SWITCH_EVENT_H
#define SWITCH_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct switch_event_header {
	char *name;
	char *value;
	struct switch_event_header *next;
};

struct switch_event_subclass {
	char *owner;
	char *name;
};

struct switch_event {
	switch_event_t event_id;
	char *owner;
	switch_event_subclass *subclass;
	struct switch_event_header *headers;
	char *body;
	void *bind_user_data;
	void *event_user_data;
	struct switch_event *next;
};

struct switch_event_node {
	char *id;
	switch_event_t event_id;
	switch_event_subclass *subclass;
	switch_event_callback_t callback;
	void *user_data;
	struct switch_event_node *next;
};

#define SWITCH_EVENT_SUBCLASS_ANY NULL

SWITCH_DECLARE(switch_status) switch_event_shutdown(void);
SWITCH_DECLARE(switch_status) switch_event_init(switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_event_create_subclass(switch_event **event, switch_event_t event_id, char *subclass_name);
SWITCH_DECLARE(char *) switch_event_get_header(switch_event *event, char *header_name);
SWITCH_DECLARE(switch_status) switch_event_add_header(switch_event *event, switch_stack_t stack, char *header_name, char *fmt, ...);
SWITCH_DECLARE(void) switch_event_destroy(switch_event **event);
SWITCH_DECLARE(switch_status) switch_event_dup(switch_event **event, switch_event *todup);
SWITCH_DECLARE(switch_status) switch_event_fire_detailed(char *file, char *func, int line, switch_event **event, void *user_data);
SWITCH_DECLARE(switch_status) switch_event_bind(char *id, switch_event_t event, char *subclass_name, switch_event_callback_t callback, void *user_data);
SWITCH_DECLARE(char *) switch_event_name(switch_event_t event);
SWITCH_DECLARE(switch_status) switch_event_reserve_subclass_detailed(char *owner, char *subclass_name);
SWITCH_DECLARE(switch_status) switch_event_serialize(switch_event *event, char *buf, size_t buflen, char *fmt, ...);
SWITCH_DECLARE(switch_status) switch_event_running(void);
SWITCH_DECLARE(switch_status) switch_event_add_body(switch_event *event, char *fmt, ...);

#define switch_event_reserve_subclass(subclass_name) switch_event_reserve_subclass_detailed(__FILE__, subclass_name)
#define switch_event_create(event, id) switch_event_create_subclass(event, id, SWITCH_EVENT_SUBCLASS_ANY)
#define switch_event_fire(event) switch_event_fire_detailed(__FILE__, (char * )__FUNCTION__, __LINE__, event, NULL)
#define switch_event_fire_data(event, data) switch_event_fire_detailed(__FILE__, (char * )__FUNCTION__, __LINE__, event, data)
#endif
