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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_event.c -- Event System
 *
 */

#include <switch.h>
#include <switch_event.h>
#include "tpl.h"

//#define SWITCH_EVENT_RECYCLE
#define DISPATCH_QUEUE_LEN 100
//#define DEBUG_DISPATCH_QUEUES

/*! \brief A node to store binded events */
struct switch_event_node {
	/*! the id of the node */
	char *id;
	/*! the event id enumeration to bind to */
	switch_event_types_t event_id;
	/*! the event subclass to bind to for custom events */
	char *subclass_name;
	/*! a callback function to execute when the event is triggered */
	switch_event_callback_t callback;
	/*! private data */
	void *user_data;
	struct switch_event_node *next;
};

/*! \brief A registered custom event subclass  */
struct switch_event_subclass {
	/*! the owner of the subclass */
	char *owner;
	/*! the subclass name */
	char *name;
	/*! the subclass was reserved by a listener so it's ok for a module to reserve it still */
	int bind;
};

#define MAX_DISPATCH_VAL 64
static unsigned int MAX_DISPATCH = MAX_DISPATCH_VAL;
static unsigned int SOFT_MAX_DISPATCH = 0;
static char guess_ip_v4[80] = "";
static char guess_ip_v6[80] = "";
static switch_event_node_t *EVENT_NODES[SWITCH_EVENT_ALL + 1] = { NULL };
static switch_thread_rwlock_t *RWLOCK = NULL;
static switch_mutex_t *BLOCK = NULL;
static switch_mutex_t *POOL_LOCK = NULL;
static switch_memory_pool_t *RUNTIME_POOL = NULL;
static switch_memory_pool_t *THRUNTIME_POOL = NULL;
static switch_thread_t *EVENT_DISPATCH_QUEUE_THREADS[MAX_DISPATCH_VAL] = { 0 };
static uint8_t EVENT_DISPATCH_QUEUE_RUNNING[MAX_DISPATCH_VAL] = { 0 };
static switch_queue_t *EVENT_DISPATCH_QUEUE = NULL;
static switch_mutex_t *EVENT_QUEUE_MUTEX = NULL;
static switch_hash_t *CUSTOM_HASH = NULL;
static int THREAD_COUNT = 0;
static int DISPATCH_THREAD_COUNT = 0;
static int SYSTEM_RUNNING = 0;
static uint64_t EVENT_SEQUENCE_NR = 0;
#ifdef SWITCH_EVENT_RECYCLE
static switch_queue_t *EVENT_RECYCLE_QUEUE = NULL;
static switch_queue_t *EVENT_HEADER_RECYCLE_QUEUE = NULL;
#endif

static char *my_dup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = malloc(len);
	switch_assert(new);

	return (char *) memcpy(new, s, len);
}

#ifndef ALLOC
#define ALLOC(size) malloc(size)
#endif
#ifndef DUP
#define DUP(str) my_dup(str)
#endif
#ifndef FREE
#define FREE(ptr) switch_safe_free(ptr)
#endif

/* make sure this is synced with the switch_event_types_t enum in switch_types.h
   also never put any new ones before EVENT_ALL
*/
static char *EVENT_NAMES[] = {
	"CUSTOM",
	"CLONE",
	"CHANNEL_CREATE",
	"CHANNEL_DESTROY",
	"CHANNEL_STATE",
	"CHANNEL_CALLSTATE",
	"CHANNEL_ANSWER",
	"CHANNEL_HANGUP",
	"CHANNEL_HANGUP_COMPLETE",
	"CHANNEL_EXECUTE",
	"CHANNEL_EXECUTE_COMPLETE",
	"CHANNEL_HOLD",
	"CHANNEL_UNHOLD",
	"CHANNEL_BRIDGE",
	"CHANNEL_UNBRIDGE",
	"CHANNEL_PROGRESS",
	"CHANNEL_PROGRESS_MEDIA",
	"CHANNEL_OUTGOING",
	"CHANNEL_PARK",
	"CHANNEL_UNPARK",
	"CHANNEL_APPLICATION",
	"CHANNEL_ORIGINATE",
	"CHANNEL_UUID",
	"API",
	"LOG",
	"INBOUND_CHAN",
	"OUTBOUND_CHAN",
	"STARTUP",
	"SHUTDOWN",
	"PUBLISH",
	"UNPUBLISH",
	"TALK",
	"NOTALK",
	"SESSION_CRASH",
	"MODULE_LOAD",
	"MODULE_UNLOAD",
	"DTMF",
	"MESSAGE",
	"PRESENCE_IN",
	"NOTIFY_IN",
	"PRESENCE_OUT",
	"PRESENCE_PROBE",
	"MESSAGE_WAITING",
	"MESSAGE_QUERY",
	"ROSTER",
	"CODEC",
	"BACKGROUND_JOB",
	"DETECTED_SPEECH",
	"DETECTED_TONE",
	"PRIVATE_COMMAND",
	"HEARTBEAT",
	"TRAP",
	"ADD_SCHEDULE",
	"DEL_SCHEDULE",
	"EXE_SCHEDULE",
	"RE_SCHEDULE",
	"RELOADXML",
	"NOTIFY",
	"SEND_MESSAGE",
	"RECV_MESSAGE",
	"REQUEST_PARAMS",
	"CHANNEL_DATA",
	"GENERAL",
	"COMMAND",
	"SESSION_HEARTBEAT",
	"CLIENT_DISCONNECTED",
	"SERVER_DISCONNECTED",
	"SEND_INFO",
	"RECV_INFO",
	"RECV_RTCP_MESSAGE",
	"CALL_SECURE",
	"NAT",
	"RECORD_START",
	"RECORD_STOP",
	"PLAYBACK_START",
	"PLAYBACK_STOP",
	"CALL_UPDATE",
	"FAILURE",
	"SOCKET_DATA",
	"MEDIA_BUG_START",
	"MEDIA_BUG_STOP",
	"CONFERENCE_DATA_QUERY",
	"CONFERENCE_DATA",
	"CALL_SETUP_REQ",
	"CALL_SETUP_RESULT",
	"ALL"
};

static int switch_events_match(switch_event_t *event, switch_event_node_t *node)
{
	int match = 0;

	if (node->event_id == SWITCH_EVENT_ALL) {
		match++;

		if (!node->subclass_name) {
			return match;
		}
	}

	if (match || event->event_id == node->event_id) {

		if (event->subclass_name && node->subclass_name) {
			if (!strncasecmp(node->subclass_name, "file:", 5)) {
				char *file_header;
				if ((file_header = switch_event_get_header(event, "file")) != 0) {
					match = strstr(node->subclass_name + 5, file_header) ? 1 : 0;
				}
			} else if (!strncasecmp(node->subclass_name, "func:", 5)) {
				char *func_header;
				if ((func_header = switch_event_get_header(event, "function")) != 0) {
					match = strstr(node->subclass_name + 5, func_header) ? 1 : 0;
				}
			} else if (event->subclass_name && node->subclass_name) {
				match = strstr(event->subclass_name, node->subclass_name) ? 1 : 0;
			}
		} else if ((event->subclass_name && !node->subclass_name) || (!event->subclass_name && !node->subclass_name)) {
			match = 1;
		} else {
			match = 0;
		}
	}

	return match;
}

static void *SWITCH_THREAD_FUNC switch_event_dispatch_thread(switch_thread_t *thread, void *obj)
{
	switch_queue_t *queue = (switch_queue_t *) obj;
	int my_id = 0;

	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	THREAD_COUNT++;
	DISPATCH_THREAD_COUNT++;

	for (my_id = 0; my_id < MAX_DISPATCH_VAL; my_id++) {
		if (EVENT_DISPATCH_QUEUE_THREADS[my_id] == thread) {
			break;
		}
	}

	EVENT_DISPATCH_QUEUE_RUNNING[my_id] = 1;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);
	

	for (;;) {
		void *pop = NULL;
		switch_event_t *event = NULL;

		if (!SYSTEM_RUNNING) {
			break;
		}

		if (switch_queue_pop(queue, &pop) != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		if (!pop) {
			break;
		}

		event = (switch_event_t *) pop;
		switch_event_deliver(&event);
		switch_os_yield();
	}


	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	EVENT_DISPATCH_QUEUE_RUNNING[my_id] = 0;
	THREAD_COUNT--;
	DISPATCH_THREAD_COUNT--;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Dispatch Thread %d Ended.\n", my_id);
	return NULL;

}

static switch_status_t switch_event_queue_dispatch_event(switch_event_t **eventp)
{

	switch_event_t *event = *eventp;

	if (!SYSTEM_RUNNING) {
		return SWITCH_STATUS_FALSE;
	}
	
	while (event) {
		int launch = 0;

		switch_mutex_lock(EVENT_QUEUE_MUTEX);		

		if (switch_queue_size(EVENT_DISPATCH_QUEUE) > (unsigned int)(DISPATCH_QUEUE_LEN * DISPATCH_THREAD_COUNT)) {
			launch++;
		}

		switch_mutex_unlock(EVENT_QUEUE_MUTEX);
		
		if (launch) {
			if (SOFT_MAX_DISPATCH + 1 < MAX_DISPATCH) {
				switch_event_launch_dispatch_threads(SOFT_MAX_DISPATCH + 1);
			}
		}

		*eventp = NULL;
		switch_queue_push(EVENT_DISPATCH_QUEUE, event);
		event = NULL;
		
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_event_deliver(switch_event_t **event)
{
	switch_event_types_t e;
	switch_event_node_t *node;

	if (SYSTEM_RUNNING) {
		switch_thread_rwlock_rdlock(RWLOCK);
		for (e = (*event)->event_id;; e = SWITCH_EVENT_ALL) {
			for (node = EVENT_NODES[e]; node; node = node->next) {
				if (switch_events_match(*event, node)) {
					(*event)->bind_user_data = node->user_data;
					node->callback(*event);
				}
			}

			if (e == SWITCH_EVENT_ALL) {
				break;
			}
		}
		switch_thread_rwlock_unlock(RWLOCK);
	}

	switch_event_destroy(event);
}

SWITCH_DECLARE(switch_status_t) switch_event_running(void)
{
	return SYSTEM_RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(const char *) switch_event_name(switch_event_types_t event)
{
	switch_assert(BLOCK != NULL);
	switch_assert(RUNTIME_POOL != NULL);

	return EVENT_NAMES[event];
}

SWITCH_DECLARE(switch_status_t) switch_name_event(const char *name, switch_event_types_t *type)
{
	switch_event_types_t x;
	switch_assert(BLOCK != NULL);
	switch_assert(RUNTIME_POOL != NULL);

	for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
		if ((strlen(name) > 13 && !strcasecmp(name + 13, EVENT_NAMES[x])) || !strcasecmp(name, EVENT_NAMES[x])) {
			*type = x;
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_event_free_subclass_detailed(const char *owner, const char *subclass_name)
{
	switch_event_subclass_t *subclass;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(RUNTIME_POOL != NULL);
	switch_assert(CUSTOM_HASH != NULL);

	if ((subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name))) {
		if (!strcmp(owner, subclass->owner)) {
			switch_thread_rwlock_wrlock(RWLOCK);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Subclass reservation deleted for %s:%s\n", owner, subclass_name);
			switch_core_hash_delete(CUSTOM_HASH, subclass_name);
			FREE(subclass->owner);
			FREE(subclass->name);
			FREE(subclass);
			status = SWITCH_STATUS_SUCCESS;
			switch_thread_rwlock_unlock(RWLOCK);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Subclass reservation %s inuse by listeners, detaching..\n", subclass_name);
			subclass->bind = 1;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_event_reserve_subclass_detailed(const char *owner, const char *subclass_name)
{
	switch_event_subclass_t *subclass;

	switch_assert(RUNTIME_POOL != NULL);
	switch_assert(CUSTOM_HASH != NULL);

	if ((subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name))) {
		/* a listener reserved it for us, now we can lock it so nobody else can have it */
		if (subclass->bind) {
			subclass->bind = 0;
			return SWITCH_STATUS_SUCCESS;
		}
		return SWITCH_STATUS_INUSE;
	}

	switch_zmalloc(subclass, sizeof(*subclass));

	subclass->owner = DUP(owner);
	subclass->name = DUP(subclass_name);

	switch_core_hash_insert(CUSTOM_HASH, subclass->name, subclass);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_core_memory_reclaim_events(void)
{
#ifdef SWITCH_EVENT_RECYCLE

	void *pop;
	int size;
	size = switch_queue_size(EVENT_RECYCLE_QUEUE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Returning %d recycled event(s) %d bytes\n", size, (int) sizeof(switch_event_t) * size);
	size = switch_queue_size(EVENT_HEADER_RECYCLE_QUEUE);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Returning %d recycled event header(s) %d bytes\n",
					  size, (int) sizeof(switch_event_header_t) * size);

	while (switch_queue_trypop(EVENT_HEADER_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		free(pop);
	}
	while (switch_queue_trypop(EVENT_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		free(pop);
	}
#else
	return;
#endif

}

SWITCH_DECLARE(switch_status_t) switch_event_shutdown(void)
{
	uint32_t x = 0;
	int last = 0;
	switch_hash_index_t *hi;
	const void *var;
	void *val;

	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	SYSTEM_RUNNING = 0;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Stopping dispatch queues\n");

	
	for(x = 0; x < (uint32_t)DISPATCH_THREAD_COUNT; x++) {
		switch_queue_trypush(EVENT_DISPATCH_QUEUE, NULL);
	}

	switch_queue_interrupt_all(EVENT_DISPATCH_QUEUE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Stopping dispatch threads\n");

	for(x = 0; x < (uint32_t)DISPATCH_THREAD_COUNT; x++) {
		switch_status_t st;
		switch_thread_join(&st, EVENT_DISPATCH_QUEUE_THREADS[x]);
	}

	x = 0;
	while (x < 10000 && THREAD_COUNT) {
		switch_cond_next();
		if (THREAD_COUNT == last) {
			x++;
		}
		last = THREAD_COUNT;
	}

	{
		void *pop = NULL;
		switch_event_t *event = NULL;

		while (switch_queue_trypop(EVENT_DISPATCH_QUEUE, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			event = (switch_event_t *) pop;
			switch_event_destroy(&event);
		}
	}

	for (hi = switch_hash_first(NULL, CUSTOM_HASH); hi; hi = switch_hash_next(hi)) {
		switch_event_subclass_t *subclass;
		switch_hash_this(hi, &var, NULL, &val);
		if ((subclass = (switch_event_subclass_t *) val)) {
			FREE(subclass->name);
			FREE(subclass->owner);
			FREE(subclass);
		}
	}

	switch_core_hash_destroy(&CUSTOM_HASH);
	switch_core_memory_reclaim_events();

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_event_launch_dispatch_threads(uint32_t max)
{
	switch_threadattr_t *thd_attr;
	uint32_t index = 0;
	int launched = 0;
	uint32_t sanity = 200;

	switch_memory_pool_t *pool = RUNTIME_POOL;

	if (max > MAX_DISPATCH) {
		return;
	}

	if (max < SOFT_MAX_DISPATCH) {
		return;
	}

	for (index = SOFT_MAX_DISPATCH; index < max && index < MAX_DISPATCH; index++) {
		if (EVENT_DISPATCH_QUEUE_THREADS[index]) {
			continue;
		}

		switch_threadattr_create(&thd_attr, pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_thread_create(&EVENT_DISPATCH_QUEUE_THREADS[index], thd_attr, switch_event_dispatch_thread, EVENT_DISPATCH_QUEUE, pool);
		while(--sanity && !EVENT_DISPATCH_QUEUE_RUNNING[index]) switch_yield(10000);

		if (index == 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Create event dispatch thread %d\n", index);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Create additional event dispatch thread %d\n", index);
		}
		launched++;
	}

	SOFT_MAX_DISPATCH = index;
}

SWITCH_DECLARE(switch_status_t) switch_event_init(switch_memory_pool_t *pool)
{
	//switch_threadattr_t *thd_attr;

	/* 
	   This statement doesn't do anything commenting it out for now.

	   switch_assert(switch_arraylen(EVENT_NAMES)  == SWITCH_EVENT_ALL + 1);
	 */
	
	/* don't need any more dispatch threads than we have CPU's*/
	MAX_DISPATCH = (switch_core_cpu_count() / 2) + 1;
	if (MAX_DISPATCH < 2) {
		MAX_DISPATCH = 2;
	}

	switch_assert(pool != NULL);
	THRUNTIME_POOL = RUNTIME_POOL = pool;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activate Eventing Engine.\n");
	switch_thread_rwlock_create(&RWLOCK, RUNTIME_POOL);
	switch_mutex_init(&BLOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_mutex_init(&POOL_LOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_mutex_init(&EVENT_QUEUE_MUTEX, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_core_hash_init(&CUSTOM_HASH, RUNTIME_POOL);

	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	SYSTEM_RUNNING = -1;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);

	//switch_threadattr_create(&thd_attr, pool);
	switch_find_local_ip(guess_ip_v4, sizeof(guess_ip_v4), NULL, AF_INET);
	switch_find_local_ip(guess_ip_v6, sizeof(guess_ip_v6), NULL, AF_INET6);


	//switch_queue_create(&EVENT_QUEUE[0], POOL_COUNT_MAX + 10, THRUNTIME_POOL);
	//switch_queue_create(&EVENT_QUEUE[1], POOL_COUNT_MAX + 10, THRUNTIME_POOL);
	//switch_queue_create(&EVENT_QUEUE[2], POOL_COUNT_MAX + 10, THRUNTIME_POOL);
#ifdef SWITCH_EVENT_RECYCLE
	switch_queue_create(&EVENT_RECYCLE_QUEUE, 250000, THRUNTIME_POOL);
	switch_queue_create(&EVENT_HEADER_RECYCLE_QUEUE, 250000, THRUNTIME_POOL);
#endif

	//switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);


	switch_queue_create(&EVENT_DISPATCH_QUEUE, DISPATCH_QUEUE_LEN * MAX_DISPATCH, pool);
	switch_event_launch_dispatch_threads(1);

	//switch_thread_create(&EVENT_QUEUE_THREADS[0], thd_attr, switch_event_thread, EVENT_QUEUE[0], RUNTIME_POOL);
	//switch_thread_create(&EVENT_QUEUE_THREADS[1], thd_attr, switch_event_thread, EVENT_QUEUE[1], RUNTIME_POOL);
	//switch_thread_create(&EVENT_QUEUE_THREADS[2], thd_attr, switch_event_thread, EVENT_QUEUE[2], RUNTIME_POOL);

	while (!THREAD_COUNT) {
		switch_cond_next();
	}


	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	SYSTEM_RUNNING = 1;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_create_subclass_detailed(const char *file, const char *func, int line,
																	  switch_event_t **event, switch_event_types_t event_id, const char *subclass_name)
{
#ifdef SWITCH_EVENT_RECYCLE
	void *pop;
#endif

	*event = NULL;

	if ((event_id != SWITCH_EVENT_CLONE && event_id != SWITCH_EVENT_CUSTOM) && subclass_name) {
		return SWITCH_STATUS_GENERR;
	}
#ifdef SWITCH_EVENT_RECYCLE
	if (EVENT_RECYCLE_QUEUE && switch_queue_trypop(EVENT_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		*event = (switch_event_t *) pop;
	} else {
#endif
		*event = ALLOC(sizeof(switch_event_t));
		switch_assert(*event);
#ifdef SWITCH_EVENT_RECYCLE
	}
#endif

	memset(*event, 0, sizeof(switch_event_t));

	if (event_id == SWITCH_EVENT_REQUEST_PARAMS || event_id == SWITCH_EVENT_CHANNEL_DATA || event_id == SWITCH_EVENT_MESSAGE) {
		(*event)->flags |= EF_UNIQ_HEADERS;
	}

	if (event_id != SWITCH_EVENT_CLONE) {
		(*event)->event_id = event_id;
		switch_event_prep_for_delivery_detailed(file, func, line, *event);
	}

	if (subclass_name) {
		(*event)->subclass_name = DUP(subclass_name);
		switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, "Event-Subclass", subclass_name);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_set_priority(switch_event_t *event, switch_priority_t priority)
{
	event->priority = priority;
	switch_event_add_header_string(event, SWITCH_STACK_TOP, "priority", switch_priority_name(priority));
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_rename_header(switch_event_t *event, const char *header_name, const char *new_header_name)
{
	switch_event_header_t *hp;
	switch_ssize_t hlen = -1;
	unsigned long hash = 0;
	int x = 0;

	switch_assert(event);

	if (!header_name) {
		return SWITCH_STATUS_FALSE;
	}

	hash = switch_ci_hashfunc_default(header_name, &hlen);

	for (hp = event->headers; hp; hp = hp->next) {
		if ((!hp->hash || hash == hp->hash) && !strcasecmp(hp->name, header_name)) {
			FREE(hp->name);
			hp->name = DUP(new_header_name);
			hlen = -1;
			hp->hash = switch_ci_hashfunc_default(hp->name, &hlen);
			x++;
		}
	}

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_event_header_t *) switch_event_get_header_ptr(switch_event_t *event, const char *header_name)
{
	switch_event_header_t *hp;
	switch_ssize_t hlen = -1;
	unsigned long hash = 0;

	switch_assert(event);

	if (!header_name)
		return NULL;

	hash = switch_ci_hashfunc_default(header_name, &hlen);

	for (hp = event->headers; hp; hp = hp->next) {
		if ((!hp->hash || hash == hp->hash) && !strcasecmp(hp->name, header_name)) {
			return hp;
		}
	}
	return NULL;
}

SWITCH_DECLARE(char *) switch_event_get_header_idx(switch_event_t *event, const char *header_name, int idx)
{
	switch_event_header_t *hp;

	if ((hp = switch_event_get_header_ptr(event, header_name))) {
		if (idx > -1) {
			if (idx < hp->idx) {
				return hp->array[idx];
			} else {
				return NULL;
			}
		}

		return hp->value;			
	} else if (!strcmp(header_name, "_body")) {
		return event->body;
	}

	return NULL;
}

SWITCH_DECLARE(char *) switch_event_get_body(switch_event_t *event)
{
	return (event ? event->body : NULL);
}

SWITCH_DECLARE(switch_status_t) switch_event_del_header_val(switch_event_t *event, const char *header_name, const char *val)
{
	switch_event_header_t *hp, *lp = NULL, *tp;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x = 0;
	switch_ssize_t hlen = -1;
	unsigned long hash = 0;

	tp = event->headers;
	while (tp) {
		hp = tp;
		tp = tp->next;

		x++;
		switch_assert(x < 1000000);
		hash = switch_ci_hashfunc_default(header_name, &hlen);

		if ((!hp->hash || hash == hp->hash) && !strcasecmp(header_name, hp->name) && (zstr(val) || !strcmp(hp->value, val))) {
			if (lp) {
				lp->next = hp->next;
			} else {
				event->headers = hp->next;
			}
			if (hp == event->last_header || !hp->next) {
				event->last_header = lp;
			}
			FREE(hp->name);

			if (hp->idx) {
				int i = 0;

				for (i = 0; i < hp->idx; i++) {
					FREE(hp->array[i]);
				}
				FREE(hp->array);
			}

			FREE(hp->value);
			
			memset(hp, 0, sizeof(*hp));
#ifdef SWITCH_EVENT_RECYCLE
			if (switch_queue_trypush(EVENT_HEADER_RECYCLE_QUEUE, hp) != SWITCH_STATUS_SUCCESS) {
				FREE(hp);
			}
#else
			FREE(hp);
#endif
			status = SWITCH_STATUS_SUCCESS;
		} else {
			lp = hp;
		}
	}

	return status;
}

static switch_event_header_t *new_header(const char *header_name)
{
	switch_event_header_t *header;

#ifdef SWITCH_EVENT_RECYCLE
		void *pop;
		if (EVENT_HEADER_RECYCLE_QUEUE && switch_queue_trypop(EVENT_HEADER_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS) {
			header = (switch_event_header_t *) pop;
		} else {
#endif
			header = ALLOC(sizeof(*header));
			switch_assert(header);
#ifdef SWITCH_EVENT_RECYCLE
		}
#endif	

		memset(header, 0, sizeof(*header));
		header->name = DUP(header_name);

		return header;

}

SWITCH_DECLARE(int) switch_event_add_array(switch_event_t *event, const char *var, const char *val)
{
	char *data;
	char **array;
	int max = 0;
	int len;
	const char *p;
	int i;

	if (strlen(val) < 8) {
		return -1;
	}

	p = val + 7;

	max = 1;

	while((p = strstr(p, "|:"))) {
		max++;
		p += 2;
	}

	if (!max) {
		return -2;
	}

	data = strdup(val + 7);
	
	len = (sizeof(char *) * max) + 1;
	switch_assert(len);

	array = malloc(len);
	memset(array, 0, len);
	
	switch_separate_string_string(data, "|:", array, max);
	
	for(i = 0; i < max; i++) {
		switch_event_add_header_string(event, SWITCH_STACK_PUSH, var, array[i]);
	}

	free(array);
	free(data);

	return 0;
}

static switch_status_t switch_event_base_add_header(switch_event_t *event, switch_stack_t stack, const char *header_name, char *data)
{
	switch_event_header_t *header = NULL;
	switch_ssize_t hlen = -1;
	int exists = 0, fly = 0;
	char *index_ptr;
	int index = 0;
	char *real_header_name = NULL;


	if (!strcmp(header_name, "_body")) {
		switch_event_set_body(event, data);
	}

	if ((index_ptr = strchr(header_name, '['))) {
		index_ptr++;
		index = atoi(index_ptr);
		real_header_name = DUP(header_name);
		if ((index_ptr = strchr(real_header_name, '['))) {
			*index_ptr++ = '\0';
		}
		header_name = real_header_name;
	}

	if (index_ptr || (stack & SWITCH_STACK_PUSH) || (stack & SWITCH_STACK_UNSHIFT)) {
		
		if (!(header = switch_event_get_header_ptr(event, header_name)) && index_ptr) {

			header = new_header(header_name);

			if (switch_test_flag(event, EF_UNIQ_HEADERS)) {
				switch_event_del_header(event, header_name);
			}

			fly++;
		}
		
		if ((header = switch_event_get_header_ptr(event, header_name))) {
			
			if (index_ptr) {
				if (index > -1 && index <= 4000) {
					if (index < header->idx) {
						FREE(header->array[index]);
						header->array[index] = DUP(data);
					} else {
						int i;
						char **m;
					
						m = realloc(header->array, sizeof(char *) * (index + 1));
						switch_assert(m);
						header->array = m;
						for (i = header->idx; i < index; i++) {
							m[i] = DUP("");
						}
						m[index] = DUP(data);
						header->idx = index + 1;
						if (!fly) {
							exists = 1;
						}

						goto redraw;
					}
				}
				goto end;
			} else {
				if ((stack & SWITCH_STACK_PUSH) || (stack & SWITCH_STACK_UNSHIFT)) {
					exists++;
					stack &= ~(SWITCH_STACK_TOP | SWITCH_STACK_BOTTOM);
				} else {
					header = NULL;
				}
			}
		}
	}


	if (!header) {

		if (zstr(data)) {
			switch_event_del_header(event, header_name);
			FREE(data);
			goto end;
		}

		if (switch_test_flag(event, EF_UNIQ_HEADERS)) {
			switch_event_del_header(event, header_name);
		}

		if (strstr(data, "ARRAY::")) {
			switch_event_add_array(event, header_name, data);
			FREE(data);
			goto end;
		}


		header = new_header(header_name);
	}
	
	if ((stack & SWITCH_STACK_PUSH) || (stack & SWITCH_STACK_UNSHIFT)) {
		char **m = NULL;
		switch_size_t len = 0;
		char *hv;
		int i = 0, j = 0;

		if (header->value && !header->idx) {
			m = malloc(sizeof(char *));
			switch_assert(m);
			m[0] = header->value;
			header->value = NULL;
			header->array = m;
			header->idx++;
			m = NULL;
		}

		i = header->idx + 1;
		m = realloc(header->array, sizeof(char *) * i); 
		switch_assert(m);

		if ((stack & SWITCH_STACK_PUSH)) {
			m[header->idx] = data;
		} else if ((stack & SWITCH_STACK_UNSHIFT)) {
			for (j = header->idx; j > 0; j--) {
				m[j] = m[j-1];
			}
			m[0] = data;
		}

		header->idx++;		
		header->array = m;

	redraw:
		len = 0;
		for(j = 0; j < header->idx; j++) {
			len += strlen(header->array[j]) + 2;
		}

		if (len) {
			len += 8;
			hv = realloc(header->value, len);
			switch_assert(hv);
			header->value = hv;

			switch_snprintf(header->value, len, "ARRAY::");
			for(j = 0; j < header->idx; j++) {
				switch_snprintf(header->value + strlen(header->value), len - strlen(header->value), "%s%s", j == 0 ? "" : "|:", header->array[j]);
			}
		}

	} else {
		switch_safe_free(header->value);
		header->value = data;
	}

	if (!exists) {
		header->hash = switch_ci_hashfunc_default(header->name, &hlen);

		if ((stack & SWITCH_STACK_TOP)) {
			header->next = event->headers;
			event->headers = header;
			if (!event->last_header) {
				event->last_header = header;
			}
		} else {
			if (event->last_header) {
				event->last_header->next = header;
			} else {
				event->headers = header;
				header->next = NULL;
			}
			event->last_header = header;
		}
	}

 end:

	switch_safe_free(real_header_name);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_add_header(switch_event_t *event, switch_stack_t stack, const char *header_name, const char *fmt, ...)
{
	int ret = 0;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	}

	return switch_event_base_add_header(event, stack, header_name, data);
}

SWITCH_DECLARE(switch_status_t) switch_event_set_subclass_name(switch_event_t *event, const char *subclass_name)
{
	if (!event || !subclass_name)
		return SWITCH_STATUS_GENERR;

	switch_safe_free(event->subclass_name);
	event->subclass_name = DUP(subclass_name);
	switch_event_del_header(event, "Event-Subclass");
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Subclass", event->subclass_name);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_add_header_string(switch_event_t *event, switch_stack_t stack, const char *header_name, const char *data)
{
	if (data) {
		return switch_event_base_add_header(event, stack, header_name, (stack & SWITCH_STACK_NODUP) ? (char *)data : DUP(data));
	}
	return SWITCH_STATUS_GENERR;
}

SWITCH_DECLARE(switch_status_t) switch_event_set_body(switch_event_t *event, const char *body)
{
	switch_safe_free(event->body);

	if (body) {
		event->body = DUP(body);
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_add_body(switch_event_t *event, const char *fmt, ...)
{
	int ret = 0;
	char *data;

	va_list ap;
	if (fmt) {
		va_start(ap, fmt);
		ret = switch_vasprintf(&data, fmt, ap);
		va_end(ap);

		if (ret == -1) {
			return SWITCH_STATUS_GENERR;
		} else {
			switch_safe_free(event->body);
			event->body = data;
			return SWITCH_STATUS_SUCCESS;
		}
	} else {
		return SWITCH_STATUS_GENERR;
	}
}

SWITCH_DECLARE(void) switch_event_destroy(switch_event_t **event)
{
	switch_event_t *ep = *event;
	switch_event_header_t *hp, *this;

	if (ep) {
		for (hp = ep->headers; hp;) {
			this = hp;
			hp = hp->next;
			FREE(this->name);

			if (this->idx) {
				int i = 0;

				for (i = 0; i < this->idx; i++) {
					FREE(this->array[i]);
				}
				FREE(this->array);
			}

			FREE(this->value);
			

#ifdef SWITCH_EVENT_RECYCLE
			if (switch_queue_trypush(EVENT_HEADER_RECYCLE_QUEUE, this) != SWITCH_STATUS_SUCCESS) {
				FREE(this);
			}
#else
			FREE(this);
#endif


		}
		FREE(ep->body);
		FREE(ep->subclass_name);
#ifdef SWITCH_EVENT_RECYCLE
		if (switch_queue_trypush(EVENT_RECYCLE_QUEUE, ep) != SWITCH_STATUS_SUCCESS) {
			FREE(ep);
		}
#else
		FREE(ep);
#endif

	}
	*event = NULL;
}


SWITCH_DECLARE(void) switch_event_merge(switch_event_t *event, switch_event_t *tomerge)
{
	switch_event_header_t *hp;
	
	switch_assert(tomerge && event);

	for (hp = tomerge->headers; hp; hp = hp->next) {
		if (hp->idx) {
			int i;
			
			for(i = 0; i < hp->idx; i++) {
				switch_event_add_header_string(event, SWITCH_STACK_PUSH, hp->name, hp->array[i]);
			}
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, hp->name, hp->value);
		}
	}
}

SWITCH_DECLARE(switch_status_t) switch_event_dup(switch_event_t **event, switch_event_t *todup)
{
	switch_event_header_t *hp;

	if (switch_event_create_subclass(event, SWITCH_EVENT_CLONE, todup->subclass_name) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	(*event)->event_id = todup->event_id;
	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;
	(*event)->flags = todup->flags;
	for (hp = todup->headers; hp; hp = hp->next) {
		if (todup->subclass_name && !strcmp(hp->name, "Event-Subclass")) {
			continue;
		}
		
		if (hp->idx) {
			int i;
			for (i = 0; i < hp->idx; i++) {
				switch_event_add_header_string(*event, SWITCH_STACK_PUSH, hp->name, hp->array[i]);
			}
		} else {
			switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, hp->name, hp->value);
		}
	}

	if (todup->body) {
		(*event)->body = DUP(todup->body);
	}

	(*event)->key = todup->key;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_event_dup_reply(switch_event_t **event, switch_event_t *todup)
{
	switch_event_header_t *hp;
	char hname[1024] = "";
	char *p;

	if (switch_event_create_subclass(event, SWITCH_EVENT_CLONE, todup->subclass_name) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	(*event)->event_id = todup->event_id;
	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;
	(*event)->flags = todup->flags;

	for (hp = todup->headers; hp; hp = hp->next) {
		char *name = hp->name, *value = hp->value;
		
		if (todup->subclass_name && !strcmp(hp->name, "Event-Subclass")) {
			continue;
		}
		
		if (!strncasecmp(hp->name, "from_", 5)) {
			p = hp->name + 5;
			switch_snprintf(hname, sizeof(hname), "to_%s", p);
			name = hname;
		} else if (!strncasecmp(hp->name, "to_", 3)) {
			p = hp->name + 3;
			switch_snprintf(hname, sizeof(hname), "from_%s", p);
			name = hname;
		} else if (!strcasecmp(name, "to")) {
			name = "from";
		} else if (!strcasecmp(name, "from")) {
			name = "to";
		}
		
		if (hp->idx) {
			int i;
			for (i = 0; i < hp->idx; i++) {
				switch_event_add_header_string(*event, SWITCH_STACK_PUSH, name, hp->array[i]);
			}
		} else {
			switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, name, value);
		}
	}

	switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, "replying", "true");

	if (todup->body) {
		switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, "orig_body", todup->body);
	}

	(*event)->key = todup->key;

	return SWITCH_STATUS_SUCCESS;
}

#define SWITCH_SERIALIZED_EVENT_MAP "S(iiisss)A(S(ss))"

SWITCH_DECLARE(switch_status_t) switch_event_binary_deserialize(switch_event_t **eventp, void **data, switch_size_t len, switch_bool_t destroy)
{
	switch_event_t *event;
	tpl_node *tn;
	switch_serial_event_t e;
	switch_serial_event_header_t sh;
	int how = TPL_MEM;

	switch_event_create(&event, SWITCH_EVENT_CLONE);
	switch_assert(event);

	tn = tpl_map(SWITCH_SERIALIZED_EVENT_MAP, &e, &sh);

	if (!destroy) {
		how |= TPL_EXCESS_OK;
	}

	tpl_load(tn, how, data, len);

	tpl_unpack(tn, 0);

	event->event_id = e.event_id;
	event->priority = e.priority;
	event->flags = e.flags;

	event->owner = e.owner;
	event->subclass_name = e.subclass_name;
	event->body = e.body;


	while(tpl_unpack(tn, 1)) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, sh.name, sh.value);
	}

	*eventp = event;

	tpl_free(tn);

	if (destroy) {
		free(*data);
	}

	*data = NULL;

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_event_binary_serialize(switch_event_t *event, void **data, switch_size_t *len)
{
	tpl_node *tn;
	switch_serial_event_t e;
	switch_serial_event_header_t sh;
	switch_event_header_t *eh;
	int how = TPL_MEM;

	e.event_id = event->event_id;
	e.priority = event->priority;
	e.flags = event->flags;

	e.owner = event->owner;
	e.subclass_name = event->subclass_name;
	e.body = event->body;

	tn = tpl_map(SWITCH_SERIALIZED_EVENT_MAP, &e, &sh);

	tpl_pack(tn, 0);
	
	for (eh = event->headers; eh; eh = eh->next) {
		if (eh->idx) continue;  // no arrays yet
	
		sh.name = eh->name;
		sh.value = eh->value;
		
		tpl_pack(tn, 1);
	}

	if (*len > 0) {
		how |= TPL_PREALLOCD;
	}

	tpl_dump(tn, how, data, len);

	tpl_free(tn);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_event_serialize(switch_event_t *event, char **str, switch_bool_t encode)
{
	switch_size_t len = 0;
	switch_event_header_t *hp;
	switch_size_t llen = 0, dlen = 0, blocksize = 512, encode_len = 1536, new_len = 0;
	char *buf;
	char *encode_buf = NULL;	/* used for url encoding of variables to make sure unsafe things stay out of the serialized copy */

	*str = NULL;

	dlen = blocksize * 2;

	if (!(buf = malloc(dlen))) {
		abort();
	}

	/* go ahead and give ourselves some space to work with, should save a few reallocs */
	if (!(encode_buf = malloc(encode_len))) {
		abort();
	}

	/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hit serialized!.\n"); */
	for (hp = event->headers; hp; hp = hp->next) {
		/*
		 * grab enough memory to store 3x the string (url encode takes one char and turns it into %XX)
		 * so we could end up with a string that is 3 times the originals length, unlikely but rather
		 * be safe than destroy the string, also add one for the null.  And try to be smart about using 
		 * the memory, allocate and only reallocate if we need more.  This avoids an alloc, free CPU
		 * destroying loop.
		 */

		if (hp->idx) {
			int i;
			new_len = 0;
			for(i = 0; i < hp->idx; i++) {
				new_len += (strlen(hp->array[i]) * 3) + 1;
			}
		} else {
			new_len = (strlen(hp->value) * 3) + 1;
		}

		if (encode_len < new_len) {
			char *tmp;

			/* keep track of the size of our allocation */
			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				abort();
			}

			encode_buf = tmp;
		}

		/* handle any bad things in the string like newlines : etc that screw up the serialized format */


		if (encode) {
			switch_url_encode(hp->value, encode_buf, encode_len);
		} else {
			switch_snprintf(encode_buf, encode_len, "[%s]", hp->value);
		}


		llen = strlen(hp->name) + strlen(encode_buf) + 8;

		if ((len + llen) > dlen) {
			char *m = buf;
			dlen += (blocksize + (len + llen));
			if (!(buf = realloc(buf, dlen))) {
				buf = m;
				abort();
			}
		}

		switch_snprintf(buf + len, dlen - len, "%s: %s\n", hp->name, *encode_buf == '\0' ? "_undef_" : encode_buf);
		len = strlen(buf);
	}

	/* we are done with the memory we used for encoding, give it back */
	switch_safe_free(encode_buf);

	if (event->body) {
		int blen = (int) strlen(event->body);
		llen = blen;

		if (blen) {
			llen += 25;
		} else {
			llen += 5;
		}

		if ((len + llen) > dlen) {
			char *m = buf;
			dlen += (blocksize + (len + llen));
			if (!(buf = realloc(buf, dlen))) {
				buf = m;
				abort();
			}
		}

		if (blen) {
			switch_snprintf(buf + len, dlen - len, "Content-Length: %d\n\n%s", blen, event->body);
		} else {
			switch_snprintf(buf + len, dlen - len, "\n");
		}
	} else {
		switch_snprintf(buf + len, dlen - len, "\n");
	}

	*str = buf;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_event_create_brackets(char *data, char a, char b, char c, switch_event_t **event, char **new_data, switch_bool_t dup)
{
	char *vdata, *vdatap = NULL;
	char *end, *check_a, *check_b;
	switch_event_t *e = *event;
	char *var_array[1024] = { 0 };
	int var_count = 0;
	char *next = NULL, *vnext = NULL;

	if (dup) {
		vdatap = strdup(data);
		vdata = vdatap;
	} else {
		vdata = data;
	}

	end = switch_find_end_paren(vdata, a, b);
	
	check_a = end;
	
	while (check_a && (check_b = switch_strchr_strict(check_a, a, " "))) {
		if ((check_b = switch_find_end_paren(check_b, a, b))) {
			check_a = check_b;
		}
	}
	
	if (check_a) end = check_a;
	
	if (end) {
		next = end;
		vdata++;
		*end++ = '\0';
	} else {
		if (dup) {
			free(vdatap);
		}
		return SWITCH_STATUS_FALSE;
	}
	
	if (!e) {
		switch_event_create_plain(&e, SWITCH_EVENT_CHANNEL_DATA);
	}

	
	for (;;) {
		if (next) {
			char *pnext;

			*next++ = '\0';

			if ((pnext = switch_strchr_strict(next, a, " "))) {
				next = pnext + 1;
			}

			vnext = switch_find_end_paren(next, a, b);
			next = NULL;
		}
			

		if (vdata) {
			if (*vdata == '^' && *(vdata + 1) == '^') {
				vdata += 2;
				c = *vdata++;
			}
		}
			
		if ((var_count = switch_separate_string(vdata, c, var_array, (sizeof(var_array) / sizeof(var_array[0]))))) {
			int x = 0;
			for (x = 0; x < var_count; x++) {
				char *inner_var_array[2] = { 0 };
				int inner_var_count;

				if ((inner_var_count = switch_separate_string(var_array[x], '=',
															  inner_var_array, (sizeof(inner_var_array) / sizeof(inner_var_array[0])))) == 2) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Parsing variable [%s]=[%s]\n", inner_var_array[0], inner_var_array[1]);
					switch_event_add_header_string(e, SWITCH_STACK_BOTTOM, inner_var_array[0], inner_var_array[1]);
				}
			}
		}

		if (vnext) {
			vdata = vnext;
		} else {
			break;
		}

	}

	*event = e;

	if (dup) {
		*new_data = strdup(end);
		free(vdatap);
	} else {
		*new_data = end;
	}
	
	return SWITCH_STATUS_SUCCESS;

}



SWITCH_DECLARE(switch_status_t) switch_event_create_json(switch_event_t **event, const char *json)
{
	switch_event_t *new_event;
	cJSON *cj, *cjp;


	if (!(cj = cJSON_Parse(json))) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_event_create(&new_event, SWITCH_EVENT_CLONE) != SWITCH_STATUS_SUCCESS) {
		cJSON_Delete(cj);
		return SWITCH_STATUS_FALSE;
	}

	for (cjp = cj->child; cjp; cjp = cjp->next) {
		char *name = cjp->string;
		char *value = cjp->valuestring;

		if (name && value) {
			if (!strcasecmp(name, "_body")) {
				switch_event_add_body(new_event, value, SWITCH_VA_NONE);
			} else {
				if (!strcasecmp(name, "event-name")) {
					switch_event_del_header(new_event, "event-name");
					switch_name_event(value, &new_event->event_id);
				}

				switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, name, value);
			}

		} else if (name) {
			if (cjp->type == cJSON_Array) {
				int i, x = cJSON_GetArraySize(cjp);

				for (i = 0; i < x; i++) {
					cJSON *item = cJSON_GetArrayItem(cjp, i);

					if (item && item->type == cJSON_String && item->valuestring) {
						switch_event_add_header_string(new_event, SWITCH_STACK_PUSH, name, item->valuestring);
					}
				}
			}
		}
	}
	
	cJSON_Delete(cj);
	*event = new_event;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_serialize_json(switch_event_t *event, char **str)
{
	switch_event_header_t *hp;
	cJSON *cj;

	*str = NULL;
	
	cj = cJSON_CreateObject();

	for (hp = event->headers; hp; hp = hp->next) {
		if (hp->idx) {
			cJSON *a = cJSON_CreateArray();
			int i;

			for(i = 0; i < hp->idx; i++) {
				cJSON_AddItemToArray(a, cJSON_CreateString(hp->array[i]));
			}
			
			cJSON_AddItemToObject(cj, hp->name, a);
			
		} else {
			cJSON_AddItemToObject(cj, hp->name, cJSON_CreateString(hp->value));
		}
	}

	if (event->body) {
		int blen = (int) strlen(event->body);
		char tmp[25];

		switch_snprintf(tmp, sizeof(tmp), "%d", blen);

		cJSON_AddItemToObject(cj, "Content-Length", cJSON_CreateString(tmp));
		cJSON_AddItemToObject(cj, "_body", cJSON_CreateString(event->body));
	}

	*str = cJSON_Print(cj);
	cJSON_Delete(cj);
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_xml_t add_xml_header(switch_xml_t xml, char *name, char *value, int offset)
{
	switch_xml_t header = switch_xml_add_child_d(xml, name, offset);

	if (header) {
		switch_size_t encode_len = (strlen(value) * 3) + 1;
		char *encode_buf = malloc(encode_len);

		switch_assert(encode_buf);

		memset(encode_buf, 0, encode_len);
		switch_url_encode((char *) value, encode_buf, encode_len);
		switch_xml_set_txt_d(header, encode_buf);
		free(encode_buf);
	}

	return header;
}

SWITCH_DECLARE(switch_xml_t) switch_event_xmlize(switch_event_t *event, const char *fmt,...)
{
	switch_event_header_t *hp;
	char *data = NULL, *body = NULL;
	int ret = 0;
	switch_xml_t xml = NULL;
	uint32_t off = 0;
	va_list ap;
	switch_xml_t xheaders = NULL;

	if (!(xml = switch_xml_new("event"))) {
		return xml;
	}

	if (!zstr(fmt)) {
		va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
		ret = vasprintf(&data, fmt, ap);
#else
		data = (char *) malloc(2048);
		if (!data)
			return NULL;
		ret = vsnprintf(data, 2048, fmt, ap);
#endif
		va_end(ap);
		if (ret == -1) {
#ifndef HAVE_VASPRINTF
			free(data);
#endif
			return NULL;
		}
	}

	if ((xheaders = switch_xml_add_child_d(xml, "headers", off++))) {
		int hoff = 0;
		for (hp = event->headers; hp; hp = hp->next) {

			if (hp->idx) {
				int i;
				for (i = 0; i < hp->idx; i++) {
					add_xml_header(xheaders, hp->name, hp->array[i], hoff++);
				}
			} else {
				add_xml_header(xheaders, hp->name, hp->value, hoff++);
			}
		}
	}

	if (!zstr(data)) {
		body = data;
	} else if (event->body) {
		body = event->body;
	}

	if (body) {
		int blen = (int) strlen(body);
		char blena[25];
		switch_snprintf(blena, sizeof(blena), "%d", blen);
		if (blen) {
			switch_xml_t xbody = NULL;

			add_xml_header(xml, "Content-Length", blena, off++);
			if ((xbody = switch_xml_add_child_d(xml, "body", off++))) {
				switch_xml_set_txt_d(xbody, body);
			}
		}
	}

	if (data) {
		free(data);
	}

	return xml;
}

SWITCH_DECLARE(void) switch_event_prep_for_delivery_detailed(const char *file, const char *func, int line, switch_event_t *event)
{
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	switch_time_t ts = switch_micro_time_now();

	switch_mutex_lock(EVENT_QUEUE_MUTEX);
	EVENT_SEQUENCE_NR++;
	switch_mutex_unlock(EVENT_QUEUE_MUTEX);


	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Name", switch_event_name(event->event_id));
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Core-UUID", switch_core_get_uuid());
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FreeSWITCH-Hostname", switch_core_get_hostname());
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FreeSWITCH-Switchname", switch_core_get_switchname());
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FreeSWITCH-IPv4", guess_ip_v4);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FreeSWITCH-IPv6", guess_ip_v6);

	switch_time_exp_lt(&tm, ts);
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Date-Local", date);
	switch_rfc822_date(date, ts);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Date-GMT", date);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Date-Timestamp", "%" SWITCH_UINT64_T_FMT, (uint64_t) ts);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Calling-File", switch_cut_path(file));
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Calling-Function", func);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Calling-Line-Number", "%d", line);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Sequence", "%" SWITCH_UINT64_T_FMT, (uint64_t) EVENT_SEQUENCE_NR);


}

SWITCH_DECLARE(switch_status_t) switch_event_fire_detailed(const char *file, const char *func, int line, switch_event_t **event, void *user_data)
{

	switch_assert(BLOCK != NULL);
	switch_assert(RUNTIME_POOL != NULL);
	switch_assert(EVENT_QUEUE_MUTEX != NULL);
	switch_assert(RUNTIME_POOL != NULL);

	if (SYSTEM_RUNNING <= 0) {
		/* sorry we're closed */
		switch_event_destroy(event);
		return SWITCH_STATUS_SUCCESS;
	}

	if (user_data) {
		(*event)->event_user_data = user_data;
	}

	if (switch_event_queue_dispatch_event(event) != SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(event);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_bind_removable(const char *id, switch_event_types_t event, const char *subclass_name,
															switch_event_callback_t callback, void *user_data, switch_event_node_t **node)
{
	switch_event_node_t *event_node;
	switch_event_subclass_t *subclass = NULL;

	switch_assert(BLOCK != NULL);
	switch_assert(RUNTIME_POOL != NULL);

	if (node) {
		*node = NULL;
	}
	
	if (subclass_name) {
		if (!(subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name))) {
			switch_event_reserve_subclass_detailed(id, subclass_name);
			subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name);
			subclass->bind = 1;
		}
		if (!subclass) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not reserve subclass. '%s'\n", subclass_name);
			return SWITCH_STATUS_FALSE;
		}
	}

	if (event <= SWITCH_EVENT_ALL) {
		switch_zmalloc(event_node, sizeof(*event_node));
		switch_thread_rwlock_wrlock(RWLOCK);
		switch_mutex_lock(BLOCK);
		/* <LOCKED> ----------------------------------------------- */
		event_node->id = DUP(id);
		event_node->event_id = event;
		if (subclass_name) {
			event_node->subclass_name = DUP(subclass_name);
		}
		event_node->callback = callback;
		event_node->user_data = user_data;

		if (EVENT_NODES[event]) {
			event_node->next = EVENT_NODES[event];
		}

		EVENT_NODES[event] = event_node;
		switch_mutex_unlock(BLOCK);
		switch_thread_rwlock_unlock(RWLOCK);
		/* </LOCKED> ----------------------------------------------- */

		if (node) {
			*node = event_node;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}


SWITCH_DECLARE(switch_status_t) switch_event_bind(const char *id, switch_event_types_t event, const char *subclass_name,
												  switch_event_callback_t callback, void *user_data)
{
	return switch_event_bind_removable(id, event, subclass_name, callback, user_data, NULL);
}


SWITCH_DECLARE(switch_status_t) switch_event_unbind_callback(switch_event_callback_t callback)
{
	switch_event_node_t *n, *np, *lnp = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int id;

	switch_thread_rwlock_wrlock(RWLOCK);
	switch_mutex_lock(BLOCK);
	/* <LOCKED> ----------------------------------------------- */
	for (id = 0; id <= SWITCH_EVENT_ALL; id++) {
		lnp = NULL;

		for (np = EVENT_NODES[id]; np;) {
			n = np;
			np = np->next;
			if (n->callback == callback) {
				if (lnp) {
					lnp->next = n->next;
				} else {
					EVENT_NODES[n->event_id] = n->next;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Event Binding deleted for %s:%s\n", n->id, switch_event_name(n->event_id));
				FREE(n->subclass_name);
				FREE(n->id);
				FREE(n);
				status = SWITCH_STATUS_SUCCESS;
			} else {
				lnp = n;
			}
		}
	}
	switch_mutex_unlock(BLOCK);
	switch_thread_rwlock_unlock(RWLOCK);
	/* </LOCKED> ----------------------------------------------- */

	return status;
}



SWITCH_DECLARE(switch_status_t) switch_event_unbind(switch_event_node_t **node)
{
	switch_event_node_t *n, *np, *lnp = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	n = *node;

	if (!n) {
		return status;
	}

	switch_thread_rwlock_wrlock(RWLOCK);
	switch_mutex_lock(BLOCK);
	/* <LOCKED> ----------------------------------------------- */
	for (np = EVENT_NODES[n->event_id]; np; np = np->next) {
		if (np == n) {
			if (lnp) {
				lnp->next = n->next;
			} else {
				EVENT_NODES[n->event_id] = n->next;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Event Binding deleted for %s:%s\n", n->id, switch_event_name(n->event_id));
			FREE(n->subclass_name);
			FREE(n->id);
			FREE(n);
			*node = NULL;
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
		lnp = np;
	}
	switch_mutex_unlock(BLOCK);
	switch_thread_rwlock_unlock(RWLOCK);
	/* </LOCKED> ----------------------------------------------- */

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_event_create_pres_in_detailed(char *file, char *func, int line,
																	 const char *proto, const char *login,
																	 const char *from, const char *from_domain,
																	 const char *status, const char *event_type,
																	 const char *alt_event_type, int event_count,
																	 const char *unique_id, const char *channel_state,
																	 const char *answer_state, const char *call_direction)
{
	switch_event_t *pres_event;

	if (switch_event_create_subclass(&pres_event, SWITCH_EVENT_PRESENCE_IN, SWITCH_EVENT_SUBCLASS_ANY) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "proto", proto);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "login", login);
		switch_event_add_header(pres_event, SWITCH_STACK_TOP, "from", "%s@%s", from, from_domain);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "status", status);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "event_type", event_type);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "alt_event_type", alt_event_type);
		switch_event_add_header(pres_event, SWITCH_STACK_TOP, "event_count", "%d", event_count);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "unique-id", alt_event_type);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "channel-state", channel_state);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "answer-state", answer_state);
		switch_event_add_header_string(pres_event, SWITCH_STACK_TOP, "presence-call-direction", call_direction);
		switch_event_fire_detailed(file, func, line, &pres_event, NULL);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_MEMERR;
}

#define resize(l) {\
char *dp;\
olen += (len + l + block);\
cpos = c - data;\
if ((dp = realloc(data, olen))) {\
    data = dp;\
    c = data + cpos;\
    memset(c, 0, olen - cpos);\
 }}                           \

SWITCH_DECLARE(char *) switch_event_expand_headers_check(switch_event_t *event, const char *in, switch_event_t *var_list, switch_event_t *api_list, uint32_t recur)
{
	char *p, *c = NULL;
	char *data, *indup, *endof_indup;
	size_t sp = 0, len = 0, olen = 0, vtype = 0, br = 0, cpos, block = 128;
	const char *sub_val = NULL;
	char *cloned_sub_val = NULL, *expanded_sub_val = NULL;
	char *func_val = NULL;
	int nv = 0;
	char *gvar = NULL, *sb = NULL;

	if (recur > 100) {
		return (char *) in;
	}

	if (zstr(in)) {
		return (char *) in;
	}

	nv = switch_string_var_check_const(in) || switch_string_has_escaped_data(in);

	if (!nv) {
		return (char *) in;
	}

	nv = 0;
	olen = strlen(in) + 1;
	indup = strdup(in);
	endof_indup = end_of_p(indup) + 1;

	if ((data = malloc(olen))) {
		memset(data, 0, olen);
		c = data;
		for (p = indup; p && p < endof_indup && *p; p++) {
			int global = 0;
			vtype = 0;

			if (*p == '\\') {
				if (*(p + 1) == '$') {
					nv = 1;
					p++;
					if (*(p + 1) == '$') {
						p++;
					}
				} else if (*(p + 1) == '\'') {
					p++;
					continue;
				} else if (*(p + 1) == '\\') {
					*c++ = *p++;
					len++;
					continue;
				}
			}

			if (*p == '$' && !nv) {
				if (*(p + 1) == '$') {
					p++;
					global++;
				}

				if (*(p + 1)) {
					if (*(p + 1) == '{') {
						vtype = global ? 3 : 1;
					} else {
						nv = 1;
					}
				} else {
					nv = 1;
				}
			}

			if (nv) {
				*c++ = *p;
				len++;
				nv = 0;
				continue;
			}

			if (vtype) {
				char *s = p, *e, *vname, *vval = NULL;
				size_t nlen;

				s++;

				if ((vtype == 1 || vtype == 3) && *s == '{') {
					br = 1;
					s++;
				}

				e = s;
				vname = s;
				while (*e) {
					if (br == 1 && *e == '}') {
						br = 0;
						*e++ = '\0';
						break;
					}

					if (br > 0) {
						if (e != s && *e == '{') {
							br++;
						} else if (br > 1 && *e == '}') {
							br--;
						}
					}

					e++;
				}
				p = e > endof_indup ? endof_indup : e;

				vval = NULL;
				for(sb = vname; sb && *sb; sb++) {
					if (*sb == ' ') {
						vval = sb;
						break;
					} else if (*sb == '(') {
						vval = sb;
						br = 1;
						break;
					}
				}

				if (vval) {
					e = vval - 1;
					*vval++ = '\0';

					while (*e == ' ') {
						*e-- = '\0';
					}
					e = vval;

					while (e && *e) {
						if (*e == '(') {
							br++;
						} else if (br > 1 && *e == ')') {
							br--;
						} else if (br == 1 && *e == ')') {
							*e = '\0';
							break;
						}
						e++;
					}

					vtype = 2;
				}

				if (vtype == 1 || vtype == 3) {
					char *expanded = NULL;
					int offset = 0;
					int ooffset = 0;
					char *ptr;
					int idx = -1;
					
					if ((expanded = switch_event_expand_headers_check(event, (char *) vname, var_list, api_list, recur+1)) == vname) {
						expanded = NULL;
					} else {
						vname = expanded;
					}
					if ((ptr = strchr(vname, ':'))) {
						*ptr++ = '\0';
						offset = atoi(ptr);
						if ((ptr = strchr(ptr, ':'))) {
							ptr++;
							ooffset = atoi(ptr);
						}
					}

					if ((ptr = strchr(vname, '[')) && strchr(ptr, ']')) {
						*ptr++ = '\0';
						idx = atoi(ptr);
					}

					if (vtype == 3 || !(sub_val = switch_event_get_header_idx(event, vname, idx))) {
						switch_safe_free(gvar);
						if ((gvar = switch_core_get_variable_dup(vname))) {
							sub_val = gvar;
						}

						if (var_list && !switch_event_check_permission_list(var_list, vname)) {
							sub_val = "<Variable Expansion Permission Denied>";
						}


						if ((expanded_sub_val = switch_event_expand_headers_check(event, sub_val, var_list, api_list, recur+1)) == sub_val) {
							expanded_sub_val = NULL;
						} else {
							sub_val = expanded_sub_val;
						}
					}

					if (offset || ooffset) {
						cloned_sub_val = strdup(sub_val);
						switch_assert(cloned_sub_val);
						sub_val = cloned_sub_val;
					}
						
					if (offset >= 0) {
						sub_val += offset;
					} else if ((size_t) abs(offset) <= strlen(sub_val)) {
						sub_val = cloned_sub_val + (strlen(cloned_sub_val) + offset);
					}

					if (ooffset > 0 && (size_t) ooffset < strlen(sub_val)) {
						if ((ptr = (char *) sub_val + ooffset)) {
							*ptr = '\0';
						}
					}
					

					switch_safe_free(expanded);
				} else {
					switch_stream_handle_t stream = { 0 };
					char *expanded = NULL;

					SWITCH_STANDARD_STREAM(stream);

					if (stream.data) {
						char *expanded_vname = NULL;

						if ((expanded_vname = switch_event_expand_headers_check(event, (char *) vname, var_list, api_list, recur+1)) == vname) {
							expanded_vname = NULL;
						} else {
							vname = expanded_vname;
						}

						if ((expanded = switch_event_expand_headers_check(event, vval, var_list, api_list, recur+1)) == vval) {
							expanded = NULL;
						} else {
							vval = expanded;
						}

						if (!switch_core_test_flag(SCF_API_EXPANSION) || (api_list && !switch_event_check_permission_list(api_list, vname))) {
							func_val = NULL;
							sub_val = "<API execute Permission Denied>";
						} else {
							if (switch_api_execute(vname, vval, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
								func_val = stream.data;
								sub_val = func_val;
							} else {
								free(stream.data);
							}
						}

						switch_safe_free(expanded);
						switch_safe_free(expanded_vname);

					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						free(data);
						free(indup);
						return (char *) in;
					}
				}
				if ((nlen = sub_val ? strlen(sub_val) : 0)) {
					if (len + nlen >= olen) {
						resize(nlen);
					}

					len += nlen;
					strcat(c, sub_val);
					c += nlen;
				}

				switch_safe_free(func_val);
				switch_safe_free(cloned_sub_val);
				switch_safe_free(expanded_sub_val);
				sub_val = NULL;
				vname = NULL;
				vtype = 0;
				br = 0;
			}
			if (len + 1 >= olen) {
				resize(1);
			}

			if (sp) {
				*c++ = ' ';
				sp = 0;
				len++;
			}

			if (*p == '$') {
				p--;
			} else {
				*c++ = *p;
				len++;
			}
		}
	}
	free(indup);
	switch_safe_free(gvar);

	return data;
}

SWITCH_DECLARE(char *) switch_event_build_param_string(switch_event_t *event, const char *prefix, switch_hash_t *vars_map)
{
	switch_stream_handle_t stream = { 0 };
	switch_size_t encode_len = 1024, new_len = 0;
	char *encode_buf = NULL;
	const char *prof[12] = { 0 }, *prof_names[12] = {
	0};
	char *e = NULL;
	switch_event_header_t *hi;
	uint32_t x = 0;
	void *data = NULL;

	SWITCH_STANDARD_STREAM(stream);

	if (prefix) {
		stream.write_function(&stream, "%s&", prefix);
	}

	encode_buf = malloc(encode_len);
	switch_assert(encode_buf);



	for (x = 0; prof[x]; x++) {
		if (zstr(prof[x])) {
			continue;
		}
		new_len = (strlen(prof[x]) * 3) + 1;
		if (encode_len < new_len) {
			char *tmp;

			encode_len = new_len;

			if (!(tmp = realloc(encode_buf, encode_len))) {
				abort();
			}

			encode_buf = tmp;
		}
		switch_url_encode(prof[x], encode_buf, encode_len);
		stream.write_function(&stream, "%s=%s&", prof_names[x], encode_buf);
	}

	if (event) {
		if ((hi = event->headers)) {

			for (; hi; hi = hi->next) {
				char *var = hi->name;
				char *val = hi->value;

				if (vars_map != NULL) {
					if ((data = switch_core_hash_find(vars_map, var)) == NULL || strcasecmp(((char *) data), "enabled"))
						continue;

				}

				new_len = (strlen((char *) var) * 3) + 1;
				if (encode_len < new_len) {
					char *tmp;

					encode_len = new_len;

					tmp = realloc(encode_buf, encode_len);
					switch_assert(tmp);
					encode_buf = tmp;
				}

				switch_url_encode((char *) val, encode_buf, encode_len);
				stream.write_function(&stream, "%s=%s&", (char *) var, encode_buf);

			}
		}
	}

	e = (char *) stream.data + (strlen((char *) stream.data) - 1);

	if (e && *e == '&') {
		*e = '\0';
	}

	switch_safe_free(encode_buf);

	return stream.data;
}

SWITCH_DECLARE(int) switch_event_check_permission_list(switch_event_t *list, const char *name)
{
	const char *v;
	int r = 0;
	int default_allow = 0;

	if (!list) {
		return 1;
	}

	default_allow = switch_test_flag(list, EF_DEFAULT_ALLOW);

	if (!list->headers) {
		return default_allow;
	}

	if ((v = switch_event_get_header(list, name))) {
		if (*v == 'd') {
			r = 0;
		} else {
			r = 1;
		}
	} else {
		r = default_allow;
	}

	return r;	
}

SWITCH_DECLARE(void) switch_event_add_presence_data_cols(switch_channel_t *channel, switch_event_t *event, const char *prefix)
{
	const char *data;

	if (!prefix) prefix = "";
	
	if ((data = switch_channel_get_variable(channel, "presence_data_cols"))) {
		char *cols[128] = { 0 };
		char header_name[128] = "";
		int col_count = 0, i = 0;
		char *data_copy = NULL;

		data_copy = strdup(data);
	
		col_count = switch_split(data_copy, ':', cols);
	
		for (i = 0; i < col_count; i++) {
			const char *val = NULL;
			switch_snprintf(header_name, sizeof(header_name), "%s%s", prefix, cols[i]);
			
			val = switch_channel_get_variable(channel, cols[i]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, header_name, val);
		}
		
		switch_safe_free(data_copy);
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
