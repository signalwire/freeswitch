/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * switch_log.c -- Logging
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

static const char *LEVELS[] = {
	"CONSOLE",
	"ALERT",
	"CRIT",
	"ERR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
};

struct switch_log_binding {
	switch_log_function_t function;
	switch_log_level_t level;
	int is_console;
	struct switch_log_binding *next;
};

typedef struct switch_log_binding switch_log_binding_t;

static switch_memory_pool_t *LOG_POOL = NULL;
static switch_log_binding_t *BINDINGS = NULL;
static switch_mutex_t *BINDLOCK = NULL;
static switch_queue_t *LOG_QUEUE = NULL;
#ifdef SWITCH_LOG_RECYCLE
static switch_queue_t *LOG_RECYCLE_QUEUE = NULL;
#endif
static int8_t THREAD_RUNNING = 0;
static uint8_t MAX_LEVEL = 0;
static int mods_loaded = 0;
static int console_mods_loaded = 0;
static switch_bool_t COLORIZE = SWITCH_FALSE;

#ifdef WIN32
static HANDLE hStdout;
static WORD wOldColorAttrs;
static CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

static WORD
#else
static const char *
#endif
 
	
	
	
	COLORS[] =
	{ SWITCH_SEQ_DEFAULT_COLOR, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FMAGEN, SWITCH_SEQ_FCYAN, SWITCH_SEQ_FGREEN,
SWITCH_SEQ_FYELLOW };


static switch_log_node_t *switch_log_node_alloc()
{
	switch_log_node_t *node = NULL;
#ifdef SWITCH_LOG_RECYCLE
	void *pop = NULL;

	if (switch_queue_trypop(LOG_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS) {
		node = (switch_log_node_t *) pop;
	} else {
#endif
		node = malloc(sizeof(*node));
		switch_assert(node);
#ifdef SWITCH_LOG_RECYCLE
	}
#endif
	return node;
}

SWITCH_DECLARE(switch_log_node_t *) switch_log_node_dup(const switch_log_node_t *node)
{
	switch_log_node_t *newnode = switch_log_node_alloc();

	*newnode = *node;

	if (!zstr(node->data)) {
		newnode->data = strdup(node->data);
		switch_assert(node->data);
	}

	if (!zstr(node->userdata)) {
		newnode->userdata = strdup(node->userdata);
		switch_assert(node->userdata);
	}

	return newnode;
}

SWITCH_DECLARE(void) switch_log_node_free(switch_log_node_t **pnode)
{
	switch_log_node_t *node;

	if (!pnode) {
		return;
	}

	node = *pnode;

	if (node) {
		switch_safe_free(node->userdata);
		switch_safe_free(node->data);
#ifdef SWITCH_LOG_RECYCLE
		if (switch_queue_trypush(LOG_RECYCLE_QUEUE, node) != SWITCH_STATUS_SUCCESS) {
			free(node);
		}
#else
		free(node);
#endif
	}
	*pnode = NULL;
}

SWITCH_DECLARE(const char *) switch_log_level2str(switch_log_level_t level)
{
	if (level > SWITCH_LOG_DEBUG) {
		level = SWITCH_LOG_DEBUG;
	}
	return LEVELS[level];
}

SWITCH_DECLARE(uint32_t) switch_log_str2mask(const char *str)
{
	int argc = 0, x = 0;
	char *argv[10] = { 0 };
	uint32_t mask = 0;
	char *p = strdup(str);
	switch_log_level_t level;

	switch_assert(p);

	if ((argc = switch_separate_string(p, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		for (x = 0; x < argc && argv[x]; x++) {
			if (!strcasecmp(argv[x], "all")) {
				mask = 0xFF;
				break;
			} else {
				level = switch_log_str2level(argv[x]);
				if (level != SWITCH_LOG_INVALID) {
					mask |= (1 << level);
				}
			}
		}
	}

	free(p);

	return mask;
}

SWITCH_DECLARE(switch_log_level_t) switch_log_str2level(const char *str)
{
	int x = 0;
	switch_log_level_t level = SWITCH_LOG_INVALID;

	if (switch_is_number(str)) {
		x = atoi(str);

		if (x > SWITCH_LOG_INVALID) {
			return SWITCH_LOG_INVALID - 1;
		} else if (x < 0) {
			return 0;
		} else {
			return x;
		}
	}


	for (x = 0;; x++) {
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

SWITCH_DECLARE(switch_status_t) switch_log_unbind_logger(switch_log_function_t function)
{
	switch_log_binding_t *ptr = NULL, *last = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(BINDLOCK);
	for (ptr = BINDINGS; ptr; ptr = ptr->next) {
		if (ptr->function == function) {
			if (last) {
				last->next = ptr->next;
			} else {
				BINDINGS = ptr->next;
			}
			status = SWITCH_STATUS_SUCCESS;
			mods_loaded--;
			if (ptr->is_console) {
				console_mods_loaded--;
			}
			break;
		}
		last = ptr;
	}
	switch_mutex_unlock(BINDLOCK);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_log_bind_logger(switch_log_function_t function, switch_log_level_t level, switch_bool_t is_console)
{
	switch_log_binding_t *binding = NULL, *ptr = NULL;
	switch_assert(function != NULL);

	if (!(binding = switch_core_alloc(LOG_POOL, sizeof(*binding)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if ((uint8_t) level > MAX_LEVEL) {
		MAX_LEVEL = level;
	}

	binding->function = function;
	binding->level = level;
	binding->is_console = is_console;

	switch_mutex_lock(BINDLOCK);
	for (ptr = BINDINGS; ptr && ptr->next; ptr = ptr->next);

	if (ptr) {
		ptr->next = binding;
	} else {
		BINDINGS = binding;
	}
	if (is_console) {
		console_mods_loaded++;
	}
	mods_loaded++;
	switch_mutex_unlock(BINDLOCK);

	return SWITCH_STATUS_SUCCESS;
}

static switch_thread_t *thread;

static void *SWITCH_THREAD_FUNC log_thread(switch_thread_t *t, void *obj)
{

	if (!obj) {
		obj = NULL;
	}
	THREAD_RUNNING = 1;

	while (THREAD_RUNNING == 1) {
		void *pop = NULL;
		switch_log_node_t *node = NULL;
		switch_log_binding_t *binding;

		if (switch_queue_pop(LOG_QUEUE, &pop) != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (!pop) {
			THREAD_RUNNING = -1;
			break;
		}

		node = (switch_log_node_t *) pop;
		switch_mutex_lock(BINDLOCK);
		for (binding = BINDINGS; binding; binding = binding->next) {
			if (binding->level >= node->level) {
				binding->function(node, node->level);
			}
		}
		switch_mutex_unlock(BINDLOCK);

		switch_log_node_free(&node);

	}

	THREAD_RUNNING = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Logger Ended.\n");
	return NULL;
}

SWITCH_DECLARE(void) switch_log_printf(switch_text_channel_t channel, const char *file, const char *func, int line,
									   const char *userdata, switch_log_level_t level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	switch_log_vprintf(channel, file, func, line, userdata, level, fmt, ap);
	va_end(ap);
}

#define do_mods (LOG_QUEUE && THREAD_RUNNING)
SWITCH_DECLARE(void) switch_log_vprintf(switch_text_channel_t channel, const char *file, const char *func, int line,
										const char *userdata, switch_log_level_t level, const char *fmt, va_list ap)
{
	char *data = NULL;
	char *new_fmt = NULL;
	int ret = 0;
	FILE *handle;
	const char *filep = (file ? switch_cut_path(file) : "");
	const char *funcp = (func ? func : "");
	char *content = NULL;
	switch_time_t now = switch_micro_time_now();
	uint32_t len;
#ifdef SWITCH_FUNC_IN_LOG
	const char *extra_fmt = "%s [%s] %s:%d %s()%c%s";
#else
	const char *extra_fmt = "%s [%s] %s:%d%c%s";
#endif
	switch_log_level_t limit_level = runtime.hard_log_level;
	switch_log_level_t special_level = SWITCH_LOG_UNINIT;

	if (channel == SWITCH_CHANNEL_ID_SESSION && userdata) {
		switch_core_session_t *session = (switch_core_session_t *) userdata;
		special_level = session->loglevel;
		if (limit_level < session->loglevel) {
			limit_level = session->loglevel;
		}
	}

	if (level > 100) {
		if ((uint32_t) (level - 100) > runtime.debug_level) {
			return;
		}

		level = 1;
	}

	if (level > limit_level) {
		return;
	}

	switch_assert(level < SWITCH_LOG_INVALID);

	handle = switch_core_data_channel(channel);

	if (channel != SWITCH_CHANNEL_ID_LOG_CLEAN) {
		char date[80] = "";
		//switch_size_t retsize;
		switch_time_exp_t tm;

		switch_time_exp_lt(&tm, now);
		switch_snprintf(date, sizeof(date), "%0.4d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d.%0.6d",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec);

		//switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

#ifdef SWITCH_FUNC_IN_LOG
		len = (uint32_t) (strlen(extra_fmt) + strlen(date) + strlen(filep) + 32 + strlen(funcp) + strlen(fmt));
#else
		len = (uint32_t) (strlen(extra_fmt) + strlen(date) + strlen(filep) + 32 + strlen(fmt));
#endif
		new_fmt = malloc(len + 1);
		switch_assert(new_fmt);
#ifdef SWITCH_FUNC_IN_LOG
		switch_snprintf(new_fmt, len, extra_fmt, date, switch_log_level2str(level), filep, line, funcp, 128, fmt);
#else
		switch_snprintf(new_fmt, len, extra_fmt, date, switch_log_level2str(level), filep, line, 128, fmt);
#endif

		fmt = new_fmt;
	}

	ret = switch_vasprintf(&data, fmt, ap);

	if (ret == -1) {
		fprintf(stderr, "Memory Error\n");
		goto end;
	}

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
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-Data", data);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-File", filep);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-Function", funcp);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Line", "%d", line);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Level", "%d", (int) level);
			if (!zstr(userdata)) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "User-Data", userdata);
			}
			switch_event_fire(&event);
			data = NULL;
		}

		goto end;
	}

	if (console_mods_loaded == 0 || !do_mods) {
		if (handle) {
			int aok = 1;
#ifndef WIN32

			fd_set can_write;
			int fd;
			struct timeval to;

			fd = fileno(handle);
			memset(&to, 0, sizeof(to));
			FD_ZERO(&can_write);
			FD_SET(fd, &can_write);
			to.tv_sec = 0;
			to.tv_usec = 100000;
			if (select(fd + 1, NULL, &can_write, NULL, &to) > 0) {
				aok = FD_ISSET(fd, &can_write);
			} else {
				aok = 0;
			}
#endif
			if (aok) {
				if (COLORIZE) {

#ifdef WIN32
					SetConsoleTextAttribute(hStdout, COLORS[level]);
					WriteFile(hStdout, data, (DWORD) strlen(data), NULL, NULL);
					SetConsoleTextAttribute(hStdout, wOldColorAttrs);
#else
					fprintf(handle, "%s%s%s", COLORS[level], data, SWITCH_SEQ_DEFAULT_COLOR);
#endif
				} else {
					fprintf(handle, "%s", data);
				}
			}
		}
	}

	if (do_mods && level <= MAX_LEVEL) {
		switch_log_node_t *node = switch_log_node_alloc();

		node->data = data;
		data = NULL;
		switch_set_string(node->file, filep);
		switch_set_string(node->func, funcp);
		node->line = line;
		node->level = level;
		node->slevel = special_level;
		node->content = content;
		node->timestamp = now;
		node->channel = channel;
		if (channel == SWITCH_CHANNEL_ID_SESSION) {
			switch_core_session_t *session = (switch_core_session_t *) userdata;
			node->userdata = userdata ? strdup(switch_core_session_get_uuid(session)) : NULL;
		} else {
			node->userdata = !zstr(userdata) ? strdup(userdata) : NULL;
		}

		if (switch_queue_trypush(LOG_QUEUE, node) != SWITCH_STATUS_SUCCESS) {
			switch_log_node_free(&node);
		}
	}

  end:

	switch_safe_free(data);
	switch_safe_free(new_fmt);

}

SWITCH_DECLARE(switch_status_t) switch_log_init(switch_memory_pool_t *pool, switch_bool_t colorize)
{
	switch_threadattr_t *thd_attr;;

	switch_assert(pool != NULL);

	LOG_POOL = pool;

	switch_threadattr_create(&thd_attr, LOG_POOL);

	switch_queue_create(&LOG_QUEUE, SWITCH_CORE_QUEUE_LEN, LOG_POOL);
#ifdef SWITCH_LOG_RECYCLE
	switch_queue_create(&LOG_RECYCLE_QUEUE, SWITCH_CORE_QUEUE_LEN, LOG_POOL);
#endif
	switch_mutex_init(&BINDLOCK, SWITCH_MUTEX_NESTED, LOG_POOL);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, log_thread, NULL, LOG_POOL);

	while (!THREAD_RUNNING) {
		switch_cond_next();
	}

	if (colorize) {
#ifdef WIN32
		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (switch_core_get_console() == stdout && hStdout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hStdout, &csbiInfo)) {
			wOldColorAttrs = csbiInfo.wAttributes;
			COLORIZE = SWITCH_TRUE;
		}
#else
		COLORIZE = SWITCH_TRUE;
#endif
	}


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_core_memory_reclaim_logger(void)
{
#ifdef SWITCH_LOG_RECYCLE
	void *pop;
	int size = switch_queue_size(LOG_RECYCLE_QUEUE);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CONSOLE, "Returning %d recycled log node(s) %d bytes\n", size,
					  (int) sizeof(switch_log_node_t) * size);
	while (switch_queue_trypop(LOG_RECYCLE_QUEUE, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_log_node_free(&pop);
	}
#else
	return;
#endif

}

SWITCH_DECLARE(switch_status_t) switch_log_shutdown(void)
{
	switch_status_t st;


	switch_queue_push(LOG_QUEUE, NULL);
	while (THREAD_RUNNING) {
		switch_cond_next();
	}

	switch_thread_join(&st, thread);

	switch_core_memory_reclaim_logger();

	return SWITCH_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
