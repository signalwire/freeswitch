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
#ifndef SWITCH_EVENT_H
#define SWITCH_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct switch_event {
	switch_event_t event;
	int subclass;
	char *data;
	struct switch_event *next;
};

struct switch_event_node {
	char *id;
	switch_event_t event;
	int subclass;
	switch_event_callback_t callback;
	struct switch_event_node *next;
};

SWITCH_DECLARE(switch_status) switch_event_shutdown(void);
SWITCH_DECLARE(switch_status) switch_event_init(switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_event_fire_subclass(switch_event_t event, int subclass, char *data);
SWITCH_DECLARE(switch_status) switch_event_bind(char *id, switch_event_t event, int subclass, switch_event_callback_t callback);
SWITCH_DECLARE(char *) switch_event_name(switch_event_t event);
#define switch_event_fire(event, data) switch_event_fire_subclass(event, 0, data);

#endif
