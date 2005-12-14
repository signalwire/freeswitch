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

static switch_event *EVENT_QUEUE_HEAD;
static switch_event *EVENT_QUEUE_WORK;
static switch_event_node *EVENT_NODES[SWITCH_EVENT_ALL+1] = {NULL};
static switch_mutex_t *ELOCK = NULL;
static switch_mutex_t *MUTEX = NULL;
static switch_memory_pool *EPOOL = NULL;
switch_thread_cond_t *COND;

static int THREAD_RUNNING = 0;

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

static void * SWITCH_THREAD_FUNC switch_event_thread(switch_thread *thread, void *obj) 
{
	switch_event_node *enp;
	switch_event *event = NULL, *out_event = NULL;
	switch_event_t e;

	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	THREAD_RUNNING = 1;
	while(THREAD_RUNNING == 1) {
		switch_thread_cond_wait(COND, MUTEX);

		if (THREAD_RUNNING == 1) {
			switch_mutex_lock(ELOCK);
			/* <LOCKED> -----------------------------------------------*/
			EVENT_QUEUE_WORK = EVENT_QUEUE_HEAD;
			EVENT_QUEUE_HEAD = NULL;
			switch_mutex_unlock(ELOCK);
			/* </LOCKED> -----------------------------------------------*/
		}

		for(event = EVENT_QUEUE_WORK; event;) {
			out_event = event;
			event = event->next;
			out_event->next = NULL;
			if (THREAD_RUNNING == 1) {
				for(e = out_event->event;; e = SWITCH_EVENT_ALL) {
					for(enp = EVENT_NODES[e]; enp; enp = enp->next) {
						if ((enp->event == out_event->event || enp->event == SWITCH_EVENT_ALL) && (enp->subclass == out_event->subclass || enp->subclass < 0)) {
							enp->callback(out_event);
						}
					}
		
					if (e == SWITCH_EVENT_ALL) {
						break;
					}
				}
			}
			free(out_event->data);
			free(out_event);
		}
	}
	THREAD_RUNNING = 0;
	return NULL;
}


SWITCH_DECLARE(char *) switch_event_name(switch_event_t event)
{
	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	return EVENT_NAMES[event];
}

SWITCH_DECLARE(switch_status) switch_event_shutdown(void)
{
	THREAD_RUNNING = -1;

	switch_thread_cond_signal(COND);

	while(THREAD_RUNNING) {
		switch_yield(100);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_event_init(switch_memory_pool *pool)
{
    switch_thread *thread;
    switch_threadattr_t *thd_attr;;
    switch_threadattr_create(&thd_attr, pool);
    switch_threadattr_detach_set(thd_attr, 1);


	assert(pool != NULL);
	EPOOL = pool;
	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Eventing Engine.\n");
	switch_mutex_init(&ELOCK, SWITCH_MUTEX_NESTED, EPOOL);

	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);
	switch_thread_cond_create(&COND, pool);	
	switch_mutex_lock(MUTEX);
	
    switch_thread_create(&thread,
						 thd_attr,
						 switch_event_thread,
						 NULL,
						 pool
						 );


	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status) switch_event_fire_subclass(switch_event_t event, int subclass, char *data)
{

	switch_event *new_event;

	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	if (!(new_event = malloc(sizeof(*new_event)))) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(new_event, 0, sizeof(*new_event));
	new_event->event = event;
	new_event->subclass = subclass;
	new_event->data = strdup(data ? data : "");
	
	switch_mutex_lock(ELOCK);
	/* <LOCKED> -----------------------------------------------*/
	if (EVENT_QUEUE_HEAD) {
		new_event->next = EVENT_QUEUE_HEAD;
	}
	EVENT_QUEUE_HEAD = new_event;
	switch_mutex_unlock(ELOCK);
	/* </LOCKED> -----------------------------------------------*/
	
	switch_thread_cond_signal(COND);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_event_bind(char *id, switch_event_t event, int subclass, switch_event_callback_t callback)
{
	switch_event_node *event_node;

	assert(ELOCK != NULL);
	assert(EPOOL != NULL);

	if (event <= SWITCH_EVENT_ALL && (event_node = switch_core_alloc(EPOOL, sizeof(switch_event_node)))) {
		switch_mutex_lock(ELOCK);
		/* <LOCKED> -----------------------------------------------*/
		event_node->id = switch_core_strdup(EPOOL, id);
		event_node->event = event;
		event_node->subclass = subclass;
		event_node->callback = callback;
		if (EVENT_NODES[event]) {
			event_node->next = EVENT_NODES[event];
		}
		EVENT_NODES[event] = event_node;
		switch_mutex_unlock(ELOCK);
		/* </LOCKED> -----------------------------------------------*/
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

