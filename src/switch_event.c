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
#include <switch.h>
#include <switch_event.h>

static switch_event_node_t *EVENT_NODES[SWITCH_EVENT_ALL + 1] = { NULL };
static switch_mutex_t *BLOCK = NULL;
static switch_mutex_t *POOL_LOCK = NULL;
static switch_memory_pool_t *RUNTIME_POOL = NULL;
//static switch_memory_pool_t *APOOL = NULL;
//static switch_memory_pool_t *BPOOL = NULL;
static switch_memory_pool_t *THRUNTIME_POOL = NULL;
static switch_queue_t *EVENT_QUEUE[3] = {0,0,0};
static int POOL_COUNT_MAX = 2000;

static switch_hash_t *CUSTOM_HASH = NULL;
static int THREAD_RUNNING = 0;

#if 0
static void *locked_alloc(switch_size_t len)
{
	void *mem;

	switch_mutex_lock(POOL_LOCK);
	/* <LOCKED> ----------------------------------------------- */
	mem = switch_core_alloc(THRUNTIME_POOL, len);
	switch_mutex_unlock(POOL_LOCK);
	/* </LOCKED> ---------------------------------------------- */

	return mem;
}

static void *locked_dup(char *str)
{
	char *dup;

	switch_mutex_lock(POOL_LOCK);
	/* <LOCKED> ----------------------------------------------- */
	dup = switch_core_strdup(THRUNTIME_POOL, str);
	switch_mutex_unlock(POOL_LOCK);
	/* </LOCKED> ---------------------------------------------- */

	return dup;
}

#define ALLOC(size) locked_alloc(size)
#define DUP(str) locked_dup(str)
#endif

#ifndef ALLOC
#define ALLOC(size) malloc(size)
#endif
#ifndef DUP
#define DUP(str) strdup(str)
#endif
#ifndef FREE
#define FREE(ptr) if (ptr) free(ptr)
#endif

/* make sure this is synced with the switch_event_types_t enum in switch_types.h
also never put any new ones before EVENT_ALL
*/
static char *EVENT_NAMES[] = {
	"CUSTOM",
	"CHANNEL_CREATE",
	"CHANNEL_DESTROY",
	"CHANNEL_STATE",
	"CHANNEL_ANSWER",
	"CHANNEL_HANGUP",
	"CHANNEL_EXECUTE",
	"CHANNEL_BRIDGE",
	"CHANNEL_UNBRIDGE",
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
	"DTMF",
	"ALL"
};


static int switch_events_match(switch_event_t *event, switch_event_node_t *node)
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
				if ((file_header = switch_event_get_header(event, "file")) != 0) {
					match = strstr(node->subclass->name + 5, file_header) ? 1 : 0;
				}
			} else if (!strncasecmp(node->subclass->name, "func:", 5)) {
				char *func_header;
				if ((func_header = switch_event_get_header(event, "function")) != 0) {
					match = strstr(node->subclass->name + 5, func_header) ? 1 : 0;
				}
			} else {
				match = strstr(event->subclass->name, node->subclass->name) ? 1 : 0;
			}
		} else if ((event->subclass && !node->subclass) || (!event->subclass && !node->subclass)) {
			match = 1;
		} else {
			match = 0;
		}
	}

	return match;
}

static void *SWITCH_THREAD_FUNC switch_event_thread(switch_thread_t *thread, void *obj)
{
	switch_event_t *out_event = NULL;
	switch_queue_t *queue = NULL;
	switch_queue_t *queues[3] = {0,0,0};
	void *pop;
	int i, len[3] = {0,0,0};

	assert(thread != NULL);
	assert(obj == NULL);
	assert(POOL_LOCK != NULL);
	assert(RUNTIME_POOL != NULL);
	THREAD_RUNNING = 1;
	
	queues[0] = EVENT_QUEUE[SWITCH_PRIORITY_HIGH];
	queues[1] = EVENT_QUEUE[SWITCH_PRIORITY_NORMAL];
	queues[2] = EVENT_QUEUE[SWITCH_PRIORITY_LOW];
	
	for(;;) {
		int any;

		len[1] = switch_queue_size(EVENT_QUEUE[SWITCH_PRIORITY_NORMAL]);
		len[2] = switch_queue_size(EVENT_QUEUE[SWITCH_PRIORITY_LOW]);
		len[0] = switch_queue_size(EVENT_QUEUE[SWITCH_PRIORITY_HIGH]);
		any = len[1] + len[2] + len[0];

		if (!any) {
			if (THREAD_RUNNING != 1) {
				break;
			}
			switch_yield(1000);
			continue;
		}

		for(i = 0; i < 3; i++) {
			if (len[i]) {
				queue = queues[i];
				while(queue) {
					if (switch_queue_trypop(queue, &pop) == SWITCH_STATUS_SUCCESS) {
						out_event = pop;
						switch_event_deliver(&out_event);
					} else {
						break;
					}
				}
			}
		}

		if (THREAD_RUNNING < 0) {
			THREAD_RUNNING--;
		}
	}


	THREAD_RUNNING = 0;
	return NULL;
}

SWITCH_DECLARE(void) switch_event_deliver(switch_event_t **event)
{
	switch_event_types_t e;
	switch_event_node_t *node;

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

	switch_event_destroy(event);
}


SWITCH_DECLARE(switch_status_t) switch_event_running(void)
{
	return THREAD_RUNNING ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(char *) switch_event_name(switch_event_types_t event)
{
	assert(BLOCK != NULL);
	assert(RUNTIME_POOL != NULL);

	return EVENT_NAMES[event];
}

SWITCH_DECLARE(switch_status_t) switch_event_reserve_subclass_detailed(char *owner, char *subclass_name)
{

	switch_event_subclass_t *subclass;

	assert(RUNTIME_POOL != NULL);
	assert(CUSTOM_HASH != NULL);

	if (switch_core_hash_find(CUSTOM_HASH, subclass_name)) {
		return SWITCH_STATUS_INUSE;
	}

	if ((subclass = switch_core_alloc(RUNTIME_POOL, sizeof(*subclass))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	subclass->owner = switch_core_strdup(RUNTIME_POOL, owner);
	subclass->name = switch_core_strdup(RUNTIME_POOL, subclass_name);

	switch_core_hash_insert(CUSTOM_HASH, subclass->name, subclass);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_event_shutdown(void)
{
	int x = 0, last = 0;

	if (THREAD_RUNNING > 0) {
		THREAD_RUNNING = -1;

		while (x < 100 && THREAD_RUNNING) {
			switch_yield(1000);
			if (THREAD_RUNNING == last) {
				x++;
			}
			last = THREAD_RUNNING;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_init(switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);

	assert(pool != NULL);
	RUNTIME_POOL = pool;


	if (switch_core_new_memory_pool(&THRUNTIME_POOL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory pool\n");
		return SWITCH_STATUS_MEMERR;
	}
	/*
	if (switch_core_new_memory_pool(&BPOOL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Could not allocate memory pool\n");
		return SWITCH_STATUS_MEMERR;
	}
	*/
	//THRUNTIME_POOL = APOOL;
	switch_queue_create(&EVENT_QUEUE[0], POOL_COUNT_MAX + 10, THRUNTIME_POOL);
	switch_queue_create(&EVENT_QUEUE[1], POOL_COUNT_MAX + 10, THRUNTIME_POOL);
	switch_queue_create(&EVENT_QUEUE[2], POOL_COUNT_MAX + 10, THRUNTIME_POOL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activate Eventing Engine.\n");
	switch_mutex_init(&BLOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_mutex_init(&POOL_LOCK, SWITCH_MUTEX_NESTED, RUNTIME_POOL);
	switch_core_hash_init(&CUSTOM_HASH, RUNTIME_POOL);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, switch_event_thread, NULL, RUNTIME_POOL);

	while (!THREAD_RUNNING) {
		switch_yield(1000);
	}
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_event_create_subclass(switch_event_t **event,
														   switch_event_types_t event_id,
														   char *subclass_name)
{

	if (event_id != SWITCH_EVENT_CUSTOM && subclass_name) {
		return SWITCH_STATUS_GENERR;
	}

	if ((*event = ALLOC(sizeof(switch_event_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}
	memset(*event, 0, sizeof(switch_event_t));

	(*event)->event_id = event_id;

	if (subclass_name) {
		(*event)->subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_set_priority(switch_event_t *event, switch_priority_t priority)
{
	event->priority = priority;
	switch_event_add_header(event, SWITCH_STACK_TOP, "priority", switch_priority_name(priority));
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(char *) switch_event_get_header(switch_event_t *event, char *header_name)
{
	switch_event_header_t *hp;
	if (header_name) {
		for (hp = event->headers; hp; hp = hp->next) {
			if (!strcasecmp(hp->name, header_name)) {
				return hp->value;
			}
		}
	}
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_event_add_header(switch_event_t *event, switch_stack_t stack, char *header_name,
													  char *fmt, ...)
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
		switch_event_header_t *header, *hp;

		if ((header = ALLOC(sizeof(*header))) == 0) {
			return SWITCH_STATUS_MEMERR;
		}

		memset(header, 0, sizeof(*header));

		header->name = DUP(header_name);
		header->value = DUP(data);
		if (stack == SWITCH_STACK_TOP) {
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
}


SWITCH_DECLARE(switch_status_t) switch_event_add_body(switch_event_t *event, char *fmt, ...)
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
}

SWITCH_DECLARE(void) switch_event_destroy(switch_event_t **event)
{
	switch_event_t *ep = *event;
	switch_event_header_t *hp, *this;

	for (hp = ep->headers; hp;) {
		this = hp;
		hp = hp->next;
		FREE(this->name);
		FREE(this->value);
		FREE(this);
	}
	FREE(ep->body);
	FREE(ep);
	*event = NULL;
}

SWITCH_DECLARE(switch_status_t) switch_event_dup(switch_event_t **event, switch_event_t *todup)
{
	switch_event_header_t *header, *hp, *hp2;

	if (switch_event_create_subclass(event, todup->event_id, todup->subclass->name) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	(*event)->subclass = todup->subclass;
	(*event)->event_user_data = todup->event_user_data;
	(*event)->bind_user_data = todup->bind_user_data;

	for (hp = todup->headers; hp && hp->next;) {
		if ((header = ALLOC(sizeof(*header))) == 0) {
			return SWITCH_STATUS_MEMERR;
		}

		memset(header, 0, sizeof(*header));

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

SWITCH_DECLARE(switch_status_t) switch_event_serialize(switch_event_t *event, char *buf, switch_size_t buflen, char *fmt, ...)
{
	switch_size_t len = 0;
	switch_event_header_t *hp;
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
		snprintf(buf + len, buflen - len, "%s: %s\n", hp->name, hp->value);
		len = strlen(buf);

	}

	if (data) {
		body = data;
	} else if (event->body) {
		body = event->body;
	}

	if (body) {
		int blen = (int) strlen(body);
		if (blen) {
			snprintf(buf + len, buflen - len, "Content-Length: %d\n\n%s", blen, body);
		} else {
			snprintf(buf + len, buflen - len, "\n");
		}
	} else {
		snprintf(buf + len, buflen - len, "\n");
	}

	if (data) {
		free(data);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_event_fire_detailed(char *file, char *func, int line, switch_event_t **event,
														 void *user_data)
{

	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;

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

	switch_queue_push(EVENT_QUEUE[(*event)->priority], *event);
	*event = NULL;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_event_bind(char *id, switch_event_types_t event, char *subclass_name,
												switch_event_callback_t callback, void *user_data)
{
	switch_event_node_t *event_node;
	switch_event_subclass_t *subclass = NULL;

	assert(BLOCK != NULL);
	assert(RUNTIME_POOL != NULL);

	if (subclass_name) {
		if ((subclass = switch_core_hash_find(CUSTOM_HASH, subclass_name)) == 0) {
			if ((subclass = switch_core_alloc(RUNTIME_POOL, sizeof(*subclass))) == 0) {
				return SWITCH_STATUS_MEMERR;
			} else {
				subclass->owner = switch_core_strdup(RUNTIME_POOL, id);
				subclass->name = switch_core_strdup(RUNTIME_POOL, subclass_name);
			}
		}
	}

	if (event <= SWITCH_EVENT_ALL && (event_node = switch_core_alloc(RUNTIME_POOL, sizeof(switch_event_node_t))) != 0) {
		switch_mutex_lock(BLOCK);
		/* <LOCKED> ----------------------------------------------- */
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
		/* </LOCKED> ----------------------------------------------- */
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}
