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

static switch_event_node *EVENT_NODES[SWITCH_EVENT_ALL+1] = {NULL};
static switch_mutex_t *BLOCK = NULL;
static switch_mutex_t *POOL_LOCK = NULL;
static switch_memory_pool *RUNTIME_POOL = NULL;
static switch_memory_pool *APOOL = NULL;
static switch_memory_pool *BPOOL = NULL;
static switch_memory_pool *THRUNTIME_POOL = NULL;
static switch_queue_t *EVENT_QUEUE = NULL;
//#define MALLOC_EVENTS

#ifdef MALLOC_EVENTS
static int POOL_COUNT = 0;
#endif

static int POOL_COUNT_MAX = 100;

static switch_hash *CUSTOM_HASH = NULL;
static int THREAD_RUNNING = 0;

#ifdef MALLOC_EVENTS
#define ALLOC(size) malloc(size)
#define DUP(str) strdup(str)
#else
static void *locked_alloc(size_t len)
{
	void *mem;

	switch_mutex_lock(POOL_LOCK);
	/* <LOCKED> -----------------------------------------------*/
	mem = switch_core_alloc(THRUNTIME_POOL, len);
	switch_mutex_unlock(POOL_LOCK);
	/* </LOCKED> ----------------------------------------------*/

	return mem;
}

static void *locked_dup(char *str)
{
	char *dup;

	switch_mutex_lock(POOL_LOCK);
	/* <LOCKED> -----------------------------------------------*/
	dup = switch_core_strdup(THRUNTIME_POOL, str);
	switch_mutex_unlock(POOL_LOCK);
	/* </LOCKED> ----------------------------------------------*/

	return dup;
}
#define ALLOC(size) locked_alloc(size)
#define DUP(str) locked_dup(str)
#endif

/* make sure this is synced with the switch_event_t enum in switch_types.h
also never put any new ones before EVENT_ALL
*/
static char *EVENT_NAMES[] = {
	"CUSTOM",
	"CHANNEL_STATE",
	"CHANNEL_ANSWER",
	"CHANNEL_HANGUP",
	"API",
	"LOG",
	"INBOUND_CHAN",
	"OUTBOUND_CHAN",
	"STARTUP",
	"SHUTDOWN",
	"ALL"
};


static int switch_events_match(switch_event *event, switch_event_node *node)
{
	int match = 0;


	if (node->event_id == SWITCH_EVENT_ALL) {
		match++;

		if (!node->subclass) {
			return match;
		}
	}

	if (match || event->event_id == node->event_id) {
		if (event->subclass && node->subclass) {
			if (!strncasecmp(node->subclass->name, "file:", 5)) {
				char *file_header;
				if ((file_header = switch_event_get_header(event, "file"))) {
					match = strstr(node->subclass->name + 5, file_header) ? 1 : 0;
				}
			} else if (!strncasecmp(node->subclass->name, "func:", 5)) {
				char *func_header;
				if ((func_header = switch_event_get_header(event, "function"))) {
					match = strstr(node->subclass->name + 5, func_header) ? 1 : 0;
				}
			} else {
				match = strstr(event->subclass->name, node->subclass->name) ? 1 : 0;
			}
		} else if (event->subclass && !node->subclass) {
			match = 1;
		} else {
			match = 0;
		}
	}

	return match;
}

static void * SWITCH_THREAD_FUNC switch_event_thread(switch_thread *thread, void *obj) 
{
	switch_event_node *node;
	switch_event *out_event = NULL;
	switch_event_t e;
	void *pop;

	assert(POOL_LOCK != NULL);
	assert(RUNTIME_POOL != NULL);
	THREAD_RUNNING = 1;
	while(THREAD_RUNNING == 1 || switch_queue_size(EVENT_QUEUE)) {

#ifdef MALLOC_EVENTS
		switch_mutex_lock(POOL_LOCK);
		/* <LOCKED> -----------------------------------------------*/
		if (POOL_COUNT >= POOL_COUNT_MAX) {
			if (THRUNTIME_POOL == APOOL) {
				THRUNTIME_POOL = BPOOL;
			} else {
				THRUNTIME_POOL = APOOL;
			}
			switch_pool_clear(THRUNTIME_POOL);
			POOL_COUNT = 0;
		}
		switch_mutex_unlock(POOL_LOCK);
		/* </LOCKED> -----------------------------------------------*/
#endif

		while (switch_queue_trypop(EVENT_QUEUE, &pop) == SWITCH_STATUS_SUCCESS) {
			out_event = pop;

			for(e = out_event->event_id;; e = SWITCH_EVENT_ALL) {
				for(node = EVENT_NODES[e]; node; node = node->next) {
					if (switch_events_match(out_event, node)) {
						out_event->bind_user_data = node->user_data;
						node->callback(out_event);
					}
				}

				if (e == SWITCH_EVENT_ALL) {
					break;
				}
			}

			switch_event_destroy(&out_event);
		}

		switch_yield(1000);
	}
	THREAD_RUNNING = 0;
	return NULL;
}

SWITCH_DECLARE(switch_status) switch_event_running(void)
{
	return THREAD_RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(char *) switch_event_name(switch_event_t event)
{
	assert(BLOCK != NULL);
	assert(RUNTIME_POOL != NULL);

	return EVENT_NAMES[event];
}

SWITCH_DECLARE(switch_status) switch_event_reserve_subclass_detailed(char *owner, char *subclass_name)
{

	switch_event_subclass *subclass;

	assert(RUNTIME_POOL != NULL);
	assert(CUSTOM_HASH != NULL);

	if (switch_core_hash_find(CUSTOM_HASH, subclass_name)) {
		return SWITCH_STATUS_INUSE;
	}

	if (!(subclass = switch_core_alloc(RUNTIME_POOL, sizeof(*subclass)))) {
		return SWITCH_STATUS_MEMERR;
	}

	subclass->owner = switch_core_strdup(RUNTIME_POOL, owner);
	subclass->name = switch_core_strdup(RUNTIME_POOL, subclass_name);

	switch_core_hash_insert(CUSTOM_HASH, subclass->name, subclass);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status) switch_event_shutdown(void)
{
	THREAD_RUNNING = -1;

	while(THREAD_RUNNING) {
		switch_yield(1000);
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
	RUNTIME_POOL = pool;


	if (switch_core_new_memory_pool(&APOOL) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory pool\n");
		return SWITCH_STATUS_MEMERR;
	}
	if (switch_core_new_memory_pool(&BPOOL) != SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Could not allocate memory pool\n");
		return SWITCH_STATUS_MEMERR;
	}

	THRUNTIME_POOL = APOOL;
	switch_queue_create(&EVENT_QUEUE, POOL_COUNT_MAX + 10, THRUNTIME_POOL);

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Activate Eventing Engine.\n");
	switch_mutex_init(&BLOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_mutex_init(&POOL_LOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_core_hash_init(&CUSTOM_HASH, RUNTIME_POOL);
	switch_thread_create(&thread,
		thd_attr,
		switch_event_thread,
		NULL,
		RUNTIME_POOL
		);

	while(!THREAD_RUNNING) {
		switch_yield(1000);
	}
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status) switch_event_create_subclass(switch_event **event, switch_event_t event_id, char *subclass_name)
{

	if (event_id != SWITCH_EVENT_CUSTOM && subclass_name) {
		return SWITCH_STATUS_GENERR;
	}

	if(!(*event = ALLOC(sizeof(switch_event)))) {
		return SWITCH_STATUS_MEMERR;
	}

#ifdef MALLOC_EVENTS
	memset(*event, 0, sizeof(switch_event));
#endif

	(*event)->event_id = event_id;

	if (subclass_name) {
		(*event)->subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(char *) switch_event_get_header(switch_event *event, char *header_name)
{
	switch_event_header *hp;
	if (header_name) {
		for(hp = event->headers; hp; hp = hp->next) {
			if (!strcasecmp(hp->name, header_name)) {
				return hp->value;
			}
		}
	}
	return NULL;
}

SWITCH_DECLARE(switch_status) switch_event_add_header(switch_event *event, switch_stack_t stack, char *header_name, char *fmt, ...)
{
	int ret = 0;
	char data[2048];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(data, sizeof(data), fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	} else {
		switch_event_header *header, *hp;

		if (!(header = ALLOC(sizeof(*header)))) {
			return SWITCH_STATUS_MEMERR;
		}
#ifdef MALLOC_EVENTS
		memset(header, 0, sizeof(*header));
#endif

		header->name = DUP(header_name);
		header->value = DUP(data);
		if ((stack = SWITCH_STACK_TOP)) {
			header->next = event->headers;
			event->headers = header;
		} else {
			for (hp = event->headers; hp && hp->next; hp = hp->next);

			if (hp) {
				hp->next = header;
			} else {
				event->headers = header;
			}
		}
		return SWITCH_STATUS_SUCCESS;

	}

	return (ret >= 0) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_GENERR;

}


SWITCH_DECLARE(switch_status) switch_event_add_body(switch_event *event, char *fmt, ...)
{
	int ret = 0;
	char data[2048];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(data, sizeof(data), fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	} else {
		event->body = DUP(data);
		return SWITCH_STATUS_SUCCESS;
	}

	return (ret >= 0) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_GENERR;

}

SWITCH_DECLARE(void) switch_event_destroy(switch_event **event)
{
#ifdef MALLOC_EVENTS
	switch_event_header *hp, *tofree;

	for (hp = (*event)->headers; hp && hp->next;) {
		tofree = hp;
		hp = hp->next;
		free(tofree->name);
		free(tofree->value);
		free(tofree);
	}

	free((*event));
#endif
	*event = NULL;
}

SWITCH_DECLARE(switch_status) switch_event_dup(switch_event **event, switch_event *todup)
{
	switch_event_header *header, *hp, *hp2;

	if (switch_event_create_subclass(event, todup->event_id, todup->subclass->name) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	(*event)->subclass = todup->subclass;
	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;

	for (hp = todup->headers; hp && hp->next;) {
		if (!(header = ALLOC(sizeof(*header)))) {
			return SWITCH_STATUS_MEMERR;
		}
#ifdef MALLOC_EVENTS
		memset(header, 0, sizeof(*header));
#endif
		header->name = DUP(hp->name);
		header->value = DUP(hp->value);

		for (hp2 = todup->headers; hp2 && hp2->next; hp2 = hp2->next);

		if (hp2) {
			hp2->next = header;
		} else {
			(*event)->headers = header;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_event_serialize(switch_event *event, char *buf, size_t buflen, char *fmt, ...)
{
	size_t len = 0;
	switch_event_header *hp;
	char *data = NULL, *body = NULL;
	int ret = 0;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
		ret = vasprintf(&data, fmt, ap);
#else
		data = (char *) malloc(2048);
		vsnprintf(data, 2048, fmt, ap);
#endif
		va_end(ap);
		if (ret == -1) {
			return SWITCH_STATUS_MEMERR;
		}
	}

	for (hp = event->headers; hp; hp = hp->next) {
		snprintf(buf+len, buflen-len, "%s: %s\n", hp->name, hp->value);
		len = strlen(buf);

	}

	if (data) {
		body = data;
	} else if (event->body) {
		body = event->body;
	}

	if (body) {
		int blen = (int)strlen(body);
		if (blen) {
			snprintf(buf+len, buflen-len, "Content-Length: %d\n\n%s", blen, body);
		} else {
			snprintf(buf+len, buflen-len, "\n");
		}
	} else {
		snprintf(buf+len, buflen-len, "\n");
	}

	if (data) {
		free(data);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status) switch_event_fire_detailed(char *file, char *func, int line, switch_event **event, void *user_data)
{

	switch_time_exp_t tm;
	char date[80] = "";
	size_t retsize;

	assert(BLOCK != NULL);
	assert(RUNTIME_POOL != NULL);

	if (THREAD_RUNNING <= 0) {
		/* sorry we're closed */
		switch_event_destroy(event);
		return SWITCH_STATUS_FALSE;
	}


	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Calling-Line-Number", "%d", line);
	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Calling-Function", func);
	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Calling-File", switch_cut_path(file));
	switch_time_exp_lt(&tm, switch_time_now());
	switch_strftime(date, &retsize, sizeof(date), "%a, %d-%b-%Y %X", &tm);
	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Date-Local", date);
	switch_rfc822_date(date, switch_time_now());
	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Date-GMT", date);
	if ((*event)->subclass) {
		switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Subclass", (*event)->subclass->name);
		switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Subclass-Owner", (*event)->subclass->owner);
	}
	switch_event_add_header(*event, SWITCH_STACK_TOP, "Event-Name", switch_event_name((*event)->event_id));


	if (user_data) {
		(*event)->event_user_data = user_data;
	}

	switch_queue_push(EVENT_QUEUE, *event);
	*event = NULL;

#ifdef MALLOC_EVENTS
	POOL_COUNT++;
#endif

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_event_bind(char *id, switch_event_t event, char *subclass_name, switch_event_callback_t callback, void *user_data)
{
	switch_event_node *event_node;
	switch_event_subclass *subclass = NULL;

	assert(BLOCK != NULL);
	assert(RUNTIME_POOL != NULL);

	if (subclass_name) {
		if (!(subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name))) {
			if (!(subclass = switch_core_alloc(RUNTIME_POOL, sizeof(*subclass)))) {
				return SWITCH_STATUS_MEMERR;
			} else {
				subclass->owner = switch_core_strdup(RUNTIME_POOL, id);
				subclass->name = switch_core_strdup(RUNTIME_POOL, subclass_name);
			}
		}
	}

	if (event <= SWITCH_EVENT_ALL && (event_node = switch_core_alloc(RUNTIME_POOL, sizeof(switch_event_node)))) {
		switch_mutex_lock(BLOCK);
		/* <LOCKED> -----------------------------------------------*/
		event_node->id = switch_core_strdup(RUNTIME_POOL, id);
		event_node->event_id = event;
		event_node->subclass = subclass;
		event_node->callback = callback;
		event_node->user_data = user_data;

		if (EVENT_NODES[event]) {
			event_node->next = EVENT_NODES[event];
		}

		EVENT_NODES[event] = event_node;
		switch_mutex_unlock(BLOCK);
		/* </LOCKED> -----------------------------------------------*/
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

