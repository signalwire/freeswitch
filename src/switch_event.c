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
 * switch_event.c -- Event System
 *
 */
#include <switch_event.h>

static switch_event_node *EVENT_NODES[SWITCH_EVENT_ALL+1] = {};
static switch_mutex_t *ELOCK = NULL;
static switch_memory_pool *EPOOL = NULL;

/* make sure this is synced with the switch_event_t enum in switch_types.h
   also never put any new ones before EVENT_ALL
*/
static char *EVENT_NAMES[] = {
	"CUSTOM",
	"INBOUND_CHAN",
	"OUTBOUND_CHAN",
	"ANSWER_CHAN",
	"HANGUP_CHAN",
	"STARTUP",
	"ALL"
};

SWITCH_DECLARE(char *) switch_event_name(switch_event_t event)
{
	return EVENT_NAMES[event];
}

SWITCH_DECLARE(switch_status) switch_event_init(switch_memory_pool *pool)
{
	assert(pool != NULL);
	EPOOL = pool;
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Eventing Engine.\n");
	return switch_mutex_init(&ELOCK, SWITCH_MUTEX_NESTED, EPOOL);
}

SWITCH_DECLARE(switch_status) switch_event_fire_subclass(switch_event_t event, int subclass, char *data)
{
	switch_event_node *enp;
	switch_event_t e;

	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	for(e = event;; e = SWITCH_EVENT_ALL) {
		for(enp = EVENT_NODES[e]; enp; enp = enp->next) {
			if ((enp->event == event || enp->event == SWITCH_EVENT_ALL) && (enp->subclass == subclass || enp->subclass < 0)) {
				enp->callback(event, subclass, data);
			}
		}
		
		if (e == SWITCH_EVENT_ALL) {
			break;
		}

	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_event_bind(char *id, switch_event_t event, int subclass, switch_event_callback_t callback)
{
	switch_event_node *event_node;

	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	if (event <= SWITCH_EVENT_ALL && (event_node = switch_core_alloc(EPOOL, sizeof(switch_event_node)))) {
		event_node->id = switch_core_strdup(EPOOL, id);
		event_node->event = event;
		event_node->subclass = subclass;
		event_node->callback = callback;
		if (EVENT_NODES[event]) {
			event_node->next = EVENT_NODES[event];
		}
		EVENT_NODES[event] = event_node;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

