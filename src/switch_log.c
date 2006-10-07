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
 * switch_log.c -- Logging
 *
 */
#include <switch.h>


static const char *LEVELS[] = {
	"EMERG"  , 
	"ALERT"  , 
	"CRIT"   , 
	"ERR"    , 
	"WARNING", 
	"NOTICE" , 
	"INFO"   , 
	"DEBUG"  , 
	"CONSOLE",
	NULL
};

struct switch_log_binding {
	switch_log_function_t function;
	switch_log_level_t level;
	struct switch_log_binding *next;
};

typedef struct switch_log_binding switch_log_binding_t;

static switch_memory_pool_t *LOG_POOL = NULL;
static switch_log_binding_t *BINDINGS = NULL;
static switch_mutex_t *BINDLOCK = NULL;
static switch_queue_t *LOG_QUEUE = NULL;
static int8_t THREAD_RUNNING = 0;
static uint8_t MAX_LEVEL = 0;

SWITCH_DECLARE(const char *) switch_log_level2str(switch_log_level_t level)
{
	return LEVELS[level];
}

SWITCH_DECLARE(switch_log_level_t) switch_log_str2level(const char *str)
{
	int x = 0;
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	for(x = 0;;x++) {
		if (!LEVELS[x]) {
			break;
		}
		if (!strcasecmp(LEVELS[x], str)) {
			level = (switch_log_level_t) x;
			break;
		}
	}

	return level;
}

SWITCH_DECLARE(switch_status_t) switch_log_bind_logger(switch_log_function_t function, switch_log_level_t level)
{
	switch_log_binding_t *binding = NULL, *ptr = NULL;
	assert(function != NULL);

	if (!(binding = switch_core_alloc(LOG_POOL, sizeof(*binding)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if ((uint8_t)level > MAX_LEVEL) {
		MAX_LEVEL = level;
	}

	binding->function = function;
	binding->level = level;

	switch_mutex_lock(BINDLOCK);
	for (ptr = BINDINGS; ptr && ptr->next; ptr = ptr->next);

	if (ptr) {
		ptr->next = binding;
	} else {
		BINDINGS = binding;
	}
	switch_mutex_unlock(BINDLOCK);

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC log_thread(switch_thread_t *thread, void *obj)
{

	/* To Be or Not To Be */
	assert(obj == NULL || obj != NULL);
	THREAD_RUNNING = 1;

	while(THREAD_RUNNING == 1) {
		void *pop = NULL;
		switch_log_node_t *node = NULL;
		switch_log_binding_t *binding;
		
		if (switch_queue_pop(LOG_QUEUE, &pop) != SWITCH_STATUS_SUCCESS) {
			break;
		}
		
		if (!pop) {
			break;
		}
		
		node = (switch_log_node_t *) pop;

		switch_mutex_lock(BINDLOCK);
		for(binding = BINDINGS; binding; binding = binding->next) {
			if (binding->level >= node->level) {
				binding->function(node, node->level);
			}
		}
		switch_mutex_unlock(BINDLOCK);
		if (node) {
			if (node->data) {
				free(node->data);
			}

			if (node->file) {
				free(node->file);
			}

			if (node->func) {
				free(node->func);
			}

			free(node);
		}
	}

	THREAD_RUNNING = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Logger Ended.\n");
	return NULL;
}

SWITCH_DECLARE(void) switch_log_printf(switch_text_channel_t channel, char *file, const char *func, int line, switch_log_level_t level, char *fmt, ...)
{
	char *data = NULL;
	char *new_fmt = NULL;
	int ret = 0;
	va_list ap;
	FILE *handle;
	char *filep = switch_cut_path(file);
	char *content = NULL;
	switch_time_t now = switch_time_now();
	uint32_t len;
	const char *extra_fmt = "%s [%s] %s:%d %s()%c%s";
	va_start(ap, fmt);

	handle = switch_core_data_channel(channel);

	if (channel != SWITCH_CHANNEL_ID_LOG_CLEAN) {
		char date[80] = "";
		switch_size_t retsize;
		switch_time_exp_t tm;

		switch_time_exp_lt(&tm, now);
		switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
		
		len = (uint32_t)(strlen(extra_fmt) + strlen(date) + strlen(filep) + 32 + strlen(func) + strlen(fmt));
		new_fmt = malloc(len+1);
		snprintf(new_fmt, len, extra_fmt, date, LEVELS[level], filep, line, func, 128, fmt);
		fmt = new_fmt;
	}

#ifdef HAVE_VASPRINTF
	ret = vasprintf(&data, fmt, ap);
#else
	data = (char *) malloc(2048);
	vsnprintf(data, 2048, fmt, ap);
#endif
	va_end(ap);
	if (ret == -1) {
		fprintf(stderr, "Memory Error\n");
	} else {

		if (channel == SWITCH_CHANNEL_ID_LOG_CLEAN) {
			content = data;
		} else {
			if ((content = strchr(data, 128))) {
				*content = ' ';
			}
		}

		if (channel == SWITCH_CHANNEL_ID_EVENT) {
				switch_event_t *event;
				if (switch_event_running() == SWITCH_STATUS_SUCCESS && switch_event_create(&event, SWITCH_EVENT_LOG) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Data", "%s", data);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-File", "%s", filep);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Function", "%s", func);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Line", "%d", line);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Level", "%d", (int)level);
					switch_event_fire(&event);
				}
		} else {
			if (level == SWITCH_LOG_CONSOLE || !LOG_QUEUE || !THREAD_RUNNING) {
				fprintf(handle, "%s", data);
				free(data);
			} else if (level <= MAX_LEVEL) {
				switch_log_node_t *node;

				if ((node = malloc(sizeof(*node)))) {
					node->data = data;
					node->file = strdup(filep);
					node->func = strdup(func);
					node->line = line;
					node->level = level;
					node->content = content;
					node->timestamp = now;
					switch_queue_push(LOG_QUEUE, node);
				}
			} 
		}
	}

	if (new_fmt) {
		free(new_fmt);
	}

	fflush(handle);
}


SWITCH_DECLARE(switch_status_t) switch_log_init(switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;

	assert(pool != NULL);

	LOG_POOL = pool;

	switch_threadattr_create(&thd_attr, LOG_POOL);
	switch_threadattr_detach_set(thd_attr, 1);


	switch_queue_create(&LOG_QUEUE, SWITCH_CORE_QUEUE_LEN, LOG_POOL);
	switch_mutex_init(&BINDLOCK, SWITCH_MUTEX_NESTED, LOG_POOL);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, log_thread, NULL, LOG_POOL);

	while (!THREAD_RUNNING) {
		switch_yield(1000);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_log_shutdown(void)
{
	THREAD_RUNNING = -1;
	switch_queue_push(LOG_QUEUE, NULL);
	while (THREAD_RUNNING) {
		switch_yield(1000);
	}
	return SWITCH_STATUS_SUCCESS;
}

