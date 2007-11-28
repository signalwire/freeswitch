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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 *
 *
 * switch_core.c -- Main Core Library
 *
 */

#include <switch.h>
#include <switch_version.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE_DATA switch_directories SWITCH_GLOBAL_dirs = { 0 };

/* The main runtime obj we keep this hidden for ourselves */
struct switch_runtime runtime;

static void send_heartbeat(void)
{
	switch_event_t *event;
	switch_core_time_duration_t duration;

	switch_core_measure_time(switch_core_uptime(), &duration);

	if (switch_event_create(&event, SWITCH_EVENT_HEARTBEAT) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Ready");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Up-Time",
								"%u year%s, "
								"%u day%s, "
								"%u hour%s, "
								"%u minute%s, "
								"%u second%s, "
								"%u millisecond%s, "
								"%u microsecond%s",
								duration.yr, duration.yr == 1 ? "" : "s",
								duration.day, duration.day == 1 ? "" : "s",
								duration.hr, duration.hr == 1 ? "" : "s",
								duration.min, duration.min == 1 ? "" : "s",
								duration.sec, duration.sec == 1 ? "" : "s",
								duration.ms, duration.ms == 1 ? "" : "s", duration.mms, duration.mms == 1 ? "" : "s");

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Session-Count", "%u", switch_core_session_count());
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_SCHED_FUNC(heartbeat_callback)
{
	send_heartbeat();

	/* reschedule this task */
	task->runtime = time(NULL) + 20;
}

SWITCH_DECLARE(switch_time_t) switch_timestamp_now(void)
{
	return runtime.timestamp ? runtime.timestamp : switch_time_now();
}

SWITCH_DECLARE(switch_status_t) switch_core_set_console(const char *console)
{
	if ((runtime.console = fopen(console, "a")) == 0) {
		fprintf(stderr, "Cannot open output file %s.\n", console);
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(FILE *) switch_core_get_console(void)
{
	return runtime.console;
}

SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel_t channel)
{
	FILE *handle = stdout;

	switch (channel) {
	case SWITCH_CHANNEL_ID_LOG:
	case SWITCH_CHANNEL_ID_LOG_CLEAN:
		handle = runtime.console;
		break;
	default:
		handle = runtime.console;
		break;
	}

	return handle;
}

SWITCH_DECLARE(int) switch_core_add_state_handler(const switch_state_handler_table_t *state_handler)
{
	int index = runtime.state_handler_index++;

	if (runtime.state_handler_index >= SWITCH_MAX_STATE_HANDLERS) {
		return -1;
	}

	runtime.state_handlers[index] = state_handler;
	return index;
}

SWITCH_DECLARE(const switch_state_handler_table_t *) switch_core_get_state_handler(int index)
{

	if (index > SWITCH_MAX_STATE_HANDLERS || index > runtime.state_handler_index) {
		return NULL;
	}

	return runtime.state_handlers[index];
}


SWITCH_DECLARE(char *) switch_core_get_variable(const char *varname)
{
	char *val;
	switch_mutex_lock(runtime.throttle_mutex);
	val = (char *) switch_core_hash_find(runtime.global_vars, varname);
	switch_mutex_unlock(runtime.throttle_mutex);
	return val;
}

SWITCH_DECLARE(void) switch_core_set_variable(const char *varname, const char *value)
{
	char *val;

	if (varname) {
		switch_mutex_lock(runtime.throttle_mutex);
		val = (char *) switch_core_hash_find(runtime.global_vars, varname);
		if (val) {
			free(val);
		}
		if (value) {
			switch_core_hash_insert(runtime.global_vars, varname, strdup(value));
		} else {
			switch_core_hash_delete(runtime.global_vars, varname);
		}
		switch_mutex_unlock(runtime.throttle_mutex);
	}
}

SWITCH_DECLARE(char *) switch_core_get_uuid(void)
{
	return runtime.uuid_str;
}


static void *switch_core_service_thread(switch_thread_t * thread, void *obj)
{
	switch_core_thread_session_t *data = obj;
	switch_core_session_t *session = data->objs[0];
	int *stream_id_p = data->objs[1];
	switch_channel_t *channel;
	switch_frame_t *read_frame;
	int stream_id = *stream_id_p;

	assert(thread != NULL);
	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_set_flag(channel, CF_SERVICE);
	while (data->running > 0) {
		switch (switch_core_session_read_frame(session, &read_frame, -1, stream_id)) {
		case SWITCH_STATUS_SUCCESS:
		case SWITCH_STATUS_TIMEOUT:
		case SWITCH_STATUS_BREAK:
			break;
		default:
			data->running = -1;
			continue;
		}
	}

	switch_channel_clear_flag(channel, CF_SERVICE);
	data->running = 0;
	return NULL;
}

/* Either add a timeout here or make damn sure the thread cannot get hung somehow (my preference) */
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session_t *thread_session)
{
	if (thread_session->running > 0) {
		thread_session->running = -1;

		while (thread_session->running) {
			switch_yield(1000);
		}
	}
}

SWITCH_DECLARE(void) switch_core_service_session(switch_core_session_t *session, switch_core_thread_session_t *thread_session, int stream_id)
{
	thread_session->running = 1;
	thread_session->objs[0] = session;
	thread_session->objs[1] = &stream_id;
	switch_core_session_launch_thread(session, switch_core_service_thread, thread_session);
}


/* This function abstracts the thread creation for modules by allowing you to pass a function ptr and
   a void object and trust that that the function will be run in a thread with arg  This lets
   you request and activate a thread without giving up any knowledge about what is in the thread
   neither the core nor the calling module know anything about each other.

   This thread is expected to never exit until the application exits so the func is responsible
   to make sure that is the case.

   The typical use for this is so switch_loadable_module.c can start up a thread for each module
   passing the table of module methods as a session obj into the core without actually allowing
   the core to have any clue and keeping switch_loadable_module.c from needing any thread code.

*/

SWITCH_DECLARE(void) switch_core_launch_thread(switch_thread_start_t func, void *obj, switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_core_thread_session_t *ts;
	int mypool;

	mypool = pool ? 0 : 1;

	if (!pool && switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory pool\n");
		return;
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);

	if ((ts = switch_core_alloc(pool, sizeof(*ts))) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not allocate memory\n");
	} else {
		if (mypool) {
			ts->pool = pool;
		}
		ts->objs[0] = obj;
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, func, ts, pool);
	}

}

SWITCH_DECLARE(void) switch_core_set_globals(void)
{
	char *dir_path;
#define BUFSIZE 1024
#ifdef WIN32
	char lpPathBuffer[BUFSIZE];
	DWORD dwBufSize = BUFSIZE;
	char base_dir[1024];
	char *lastbacklash;
	GetModuleFileName(NULL, base_dir, BUFSIZE);
	lastbacklash = strrchr(base_dir, '\\');
	base_dir[(lastbacklash - base_dir)] = '\0';
#else
	char base_dir[1024] = SWITCH_PREFIX_DIR;
#endif

	if (!SWITCH_GLOBAL_dirs.base_dir && (SWITCH_GLOBAL_dirs.base_dir = (char *) malloc(BUFSIZE))) {
		switch_snprintf(SWITCH_GLOBAL_dirs.base_dir, BUFSIZE, "%s", base_dir);
	}

	if (!SWITCH_GLOBAL_dirs.mod_dir && (SWITCH_GLOBAL_dirs.mod_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_MOD_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.mod_dir, BUFSIZE, "%s", SWITCH_MOD_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.mod_dir, BUFSIZE, "%s%smod", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.conf_dir && (SWITCH_GLOBAL_dirs.conf_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_CONF_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.conf_dir, BUFSIZE, "%s", SWITCH_CONF_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.conf_dir, BUFSIZE, "%s%sconf", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.log_dir && (SWITCH_GLOBAL_dirs.log_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_LOG_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.log_dir, BUFSIZE, "%s", SWITCH_LOG_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.log_dir, BUFSIZE, "%s%slog", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.storage_dir && (SWITCH_GLOBAL_dirs.storage_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_STORAGE_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.storage_dir, BUFSIZE, "%s", SWITCH_STORAGE_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.storage_dir, BUFSIZE, "%s%sstorage", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.db_dir && (SWITCH_GLOBAL_dirs.db_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_DB_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.db_dir, BUFSIZE, "%s", SWITCH_DB_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.db_dir, BUFSIZE, "%s%sdb", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.script_dir && (SWITCH_GLOBAL_dirs.script_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_SCRIPT_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.script_dir, BUFSIZE, "%s", SWITCH_SCRIPT_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.script_dir, BUFSIZE, "%s%sscripts", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.htdocs_dir && (SWITCH_GLOBAL_dirs.htdocs_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_HTDOCS_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.htdocs_dir, BUFSIZE, "%s", SWITCH_HTDOCS_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.htdocs_dir, BUFSIZE, "%s%shtdocs", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.grammar_dir && (SWITCH_GLOBAL_dirs.grammar_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_GRAMMAR_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.grammar_dir, BUFSIZE, "%s", SWITCH_GRAMMAR_DIR);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.grammar_dir, BUFSIZE, "%s%sgrammar", base_dir, SWITCH_PATH_SEPARATOR);
#endif
	}

	if (!SWITCH_GLOBAL_dirs.temp_dir && (SWITCH_GLOBAL_dirs.temp_dir = (char *) malloc(BUFSIZE))) {
#ifdef SWITCH_TEMP_DIR
		switch_snprintf(SWITCH_GLOBAL_dirs.temp_dir, BUFSIZE, "%s", SWITCH_TEMP_DIR);
#else
#ifdef WIN32
		GetTempPath(dwBufSize, lpPathBuffer);
		switch_snprintf(SWITCH_GLOBAL_dirs.temp_dir, BUFSIZE, "%s", lpPathBuffer);
#else
		switch_snprintf(SWITCH_GLOBAL_dirs.temp_dir, BUFSIZE, "%s", "/tmp/");
#endif
#endif
	}

	
	dir_path = switch_mprintf("%s%ssounds", SWITCH_GLOBAL_dirs.base_dir, SWITCH_PATH_SEPARATOR);
	switch_dir_make_recursive(dir_path,
							  SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE | SWITCH_FPROT_UEXECUTE | SWITCH_FPROT_GREAD | SWITCH_FPROT_GEXECUTE,
							  runtime.memory_pool);
	switch_safe_free(dir_path);
}


SWITCH_DECLARE(int32_t) set_high_priority(void)
{
#ifdef WIN32
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else

#ifdef USE_SETRLIMIT
	struct rlimit lim = { RLIM_INFINITY, RLIM_INFINITY };
#endif

#ifdef USE_SCHED_SETSCHEDULER
	/*
	 * Try to use a round-robin scheduler
	 * with a fallback if that does not work
	 */
	struct sched_param sched = { 0 };
	sched.sched_priority = 1;
	if (sched_setscheduler(0, SCHED_RR, &sched)) {
		sched.sched_priority = 0;
		if (sched_setscheduler(0, SCHED_OTHER, &sched)) {
			return -1;
		}
	}
#endif

#ifdef HAVE_SETPRIORITY
	/*
	 * setpriority() works on FreeBSD (6.2), nice() doesn't
	 */
	if (setpriority(PRIO_PROCESS, getpid(), -10) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not set nice level\n");
	}
#else
	if (nice(-10) != -10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not set nice level\n");
	}
#endif

#ifdef USE_SETRLIMIT
	/*
	 * The amount of memory which can be mlocked is limited for non-root users.
	 * FS will segfault (= hitting the limit) soon after mlockall has been called
	 * and we've switched to a different user.
	 * So let's try to remove the mlock limit here...
	 */
	if (setrlimit(RLIMIT_MEMLOCK, &lim) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
			 "Failed to disable memlock limit, application may crash if run as non-root user!\n");
	}
#endif

#ifdef USE_MLOCKALL
	/*
	 * Pin memory pages to RAM to prevent being swapped to disk
	 */
	mlockall(MCL_CURRENT | MCL_FUTURE);
#endif

#endif
	return 0;
}

SWITCH_DECLARE(int32_t) change_user_group(const char *user, const char *group)
{
#ifndef WIN32
	uid_t runas_uid = 0;
	gid_t runas_gid = 0;
	struct passwd *runas_pw = NULL;

	if (user) {
		/*
		 * Lookup user information in the system's db
		 */
		runas_pw = getpwnam( user );
		if (!runas_pw) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unknown user \"%s\"\n", user);
			return -1;
		}
		runas_uid = runas_pw->pw_uid;
	}

	if (group) {
		struct group *gr = NULL;

		/*
		 * Lookup group information in the system's db
		 */
		gr = getgrnam( group );
		if (!gr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unknown group \"%s\"\n", group);
			return -1;
		}
		runas_gid = gr->gr_gid;
	}

	if (runas_uid) {
#ifdef HAVE_SETGROUPS
		/*
		 * Drop all group memberships prior to changing anything
		 * or else we're going to inherit the parent's list of groups
		 * (which is not what we want...)
		 */
		if (setgroups(0, NULL) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to drop group access list\n");
			return -1;
		}
#endif
		if (runas_gid) {
			/*
			 * A group has been passed, switch to it
			 * (without loading the user's other groups)
			 */
			if (setgid(runas_gid) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to change gid!\n");
				return -1;
			}
		} else {
			/*
			 * No group has been passed, use the user's primary group in this case
			 */
			if (setgid(runas_pw->pw_gid) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to change gid!\n");
				return -1;
			}

#ifdef HAVE_INITGROUPS
			/*
			 * Set all the other groups the user is a member of
			 * (This can be really useful for fine-grained access control)
			 */
			if (initgroups(runas_pw->pw_name, runas_pw->pw_gid) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to set group access list for user\n");
				return -1;
			}
#endif
		}

		/*
		 * Finally drop all privileges by switching to the new userid
		 */
		if (setuid(runas_uid) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to change uid!\n");
			return -1;
		}
	}
#endif
	return 0;
}

SWITCH_DECLARE(void) switch_core_runtime_loop(int bg)
{
#ifdef WIN32
	HANDLE shutdown_event;
	char path[256] = "";
#endif
	if (bg) {
		bg = 0;
#ifdef WIN32
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", getpid());
		shutdown_event = CreateEvent(NULL, FALSE, FALSE, path);
		WaitForSingleObject(shutdown_event, INFINITE);
#else
		runtime.running = 1;
		while (runtime.running) {
			switch_yield(1000000);
		}
#endif
	} else {
		/* wait for console input */
		switch_console_loop();
	}
}

SWITCH_DECLARE(const char *) switch_core_mime_ext2type(const char *ext)
{
	if (!ext) {
		return NULL;
	}
	return (const char *) switch_core_hash_find(runtime.mime_types, ext);
}

SWITCH_DECLARE(switch_status_t) switch_core_mime_add_type(const char *type, const char *ext)
{
	const char *check = (const char *) switch_core_hash_find(runtime.mime_types, ext);
	switch_status_t status = SWITCH_STATUS_FALSE;

	assert(type);
	assert(ext);

	if (!check) {
		char *ptype = switch_core_permanent_strdup(type);
		char *ext_list = strdup(ext);
		int argc = 0;
		char *argv[20] = { 0 };
		int x;

		assert(ext_list);

		if ((argc = switch_separate_string(ext_list, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {

			for (x = 0; x < argc; x++) {
				switch_core_hash_insert(runtime.mime_types, argv[x], ptype);
			}
			
			status = SWITCH_STATUS_SUCCESS;
		}
		
		free(ext_list);
	}

	return status;
}

static void load_mime_types(void) 
{
	char *cf = "mime.types";
	int fd = -1;
	char line_buf[1024] = "";
	char *mime_path = NULL;

	mime_path = switch_mprintf("%s/%s", SWITCH_GLOBAL_dirs.conf_dir, cf);
	assert(mime_path);

	if (!(fd = open(mime_path, O_RDONLY))) {
		return;
	}

	while((switch_fd_read_line(fd, line_buf, sizeof(line_buf)))) {
		char *p;
		char *type = line_buf;

		if (*line_buf == '#') {
			continue;
		}

		if ((p = strchr(line_buf, '\r')) || (p = strchr(line_buf, '\n'))) {
			*p = '\0';
		}

		if ((p = strchr(type, '\t')) || (p = strchr(type, ' '))) {
			*p++ = '\0';

			while(*p == ' ' || *p == '\t') {
				p++;
			}

			switch_core_mime_add_type(type, p);
		}
		
	}

	if (fd > -1) {
		close(fd);
		fd = -1;
	}

}	

SWITCH_DECLARE(switch_status_t) switch_core_init(const char *console, switch_core_flag_t flags, const char **err)
{
	switch_xml_t xml = NULL, cfg = NULL;
	switch_uuid_t uuid;
	char guess_ip[256];

	memset(&runtime, 0, sizeof(runtime));

	switch_set_flag((&runtime), SCF_NO_NEW_SESSIONS);
	runtime.hard_log_level = SWITCH_LOG_DEBUG;
	runtime.mailer_app = "sendmail";
	runtime.mailer_app_args = "-t";

	/* INIT APR and Create the pool context */
	if (apr_initialize() != SWITCH_STATUS_SUCCESS) {
		*err = "FATAL ERROR! Could not initilize APR\n";
		return SWITCH_STATUS_MEMERR;
	}

	if (!(runtime.memory_pool = switch_core_memory_init())) {
		*err = "FATAL ERROR! Could noat allocate memory pool\n";
		return SWITCH_STATUS_MEMERR;
	}
	assert(runtime.memory_pool != NULL);
	switch_mutex_init(&runtime.throttle_mutex, SWITCH_MUTEX_NESTED, runtime.memory_pool);
	switch_core_set_globals();
	switch_core_session_init(runtime.memory_pool);
	switch_core_hash_init(&runtime.global_vars, runtime.memory_pool);
	switch_core_hash_init(&runtime.mime_types, runtime.memory_pool);
	load_mime_types();
	runtime.flags = flags;
	runtime.sps_total = 30;

	switch_find_local_ip(guess_ip, sizeof(guess_ip), AF_INET);
	switch_core_set_variable("local_ip_v4", guess_ip);
	switch_find_local_ip(guess_ip, sizeof(guess_ip), AF_INET6);
	switch_core_set_variable("local_ip_v6", guess_ip);
	

	if (switch_xml_init(runtime.memory_pool, err) != SWITCH_STATUS_SUCCESS) {
		apr_terminate();
		return SWITCH_STATUS_MEMERR;
	}
	
	if ((xml = switch_xml_open_cfg("switch.conf", &cfg, NULL))) {
		switch_xml_t settings, param;

		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "crash-protection")) {
					if (switch_true(val)) {
						switch_set_flag((&runtime), SCF_CRASH_PROT);
					}
				} else if (!strcasecmp(var, "loglevel")) {
                    int level;                                                                                                                                  
                    if (*val > 47 && *val < 58) {
                        level = atoi(val);
                    } else {
                        level = switch_log_str2level(val);
                    }

                    switch_core_session_ctl(SCSC_LOGLEVEL, &level);
					
				} else if (!strcasecmp(var, "mailer-app")) {
					runtime.mailer_app = switch_core_strdup(runtime.memory_pool, val);
				} else if (!strcasecmp(var, "mailer-app-args")) {
					runtime.mailer_app_args = switch_core_strdup(runtime.memory_pool, val);
				} else if (!strcasecmp(var, "sessions-per-second")) {
					switch_core_sessions_per_second(atoi(val));
				} else if (!strcasecmp(var, "max-sessions")) {
					switch_core_session_limit(atoi(val));
				}
				else if (!strcasecmp(var, "rtp-start-port")) {
					switch_rtp_set_start_port((switch_port_t)atoi(val));
				}
				else if (!strcasecmp(var, "rtp-end-port")) {
					switch_rtp_set_end_port((switch_port_t)atoi(val));
				}
			}
		}

		if ((settings = switch_xml_child(cfg, "variables"))) {
			for (param = switch_xml_child(settings, "variable"); param; param = param->next) {
				const char *var = switch_xml_attr_soft(param, "name");
				const char *val = switch_xml_attr_soft(param, "value");
				char *varr = NULL, *vall = NULL;

				varr = switch_core_strdup(runtime.memory_pool, var);
				vall = switch_core_strdup(runtime.memory_pool, val);
				switch_core_hash_insert(runtime.global_vars, varr, vall);
			}
		}
		switch_xml_free(xml);
	}

	switch_core_state_machine_init(runtime.memory_pool);

	*err = NULL;

	if (console) {
		if (*console != '/') {
			char path[265];
			snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, console);
			console = path;
		}
		if (switch_core_set_console(console) != SWITCH_STATUS_SUCCESS) {
			*err = "FATAL ERROR! Could not open console\n";
			apr_terminate();
			return SWITCH_STATUS_GENERR;
		}
	} else {
		runtime.console = stdout;
	}

	assert(runtime.memory_pool != NULL);
	switch_log_init(runtime.memory_pool);
	switch_event_init(runtime.memory_pool);

	if (switch_test_flag((&runtime), SCF_USE_SQL)) {
		switch_core_sqldb_start(runtime.memory_pool);
	}
	switch_rtp_init(runtime.memory_pool);
	runtime.running = 1;

	switch_scheduler_task_thread_start();
	runtime.initiated = switch_time_now();

	switch_scheduler_add_task(time(NULL), heartbeat_callback, "heartbeat", "core", 0, NULL, SSHF_NONE | SSHF_NO_DEL);


	switch_uuid_get(&uuid);
	switch_uuid_format(runtime.uuid_str, &uuid);

	return SWITCH_STATUS_SUCCESS;
}

#ifdef SIGPIPE
static void handle_SIGPIPE(int sig)
{
	if (sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig Pipe!\n");
	return;
}
#endif

#ifdef SIGPOLL
static void handle_SIGPOLL(int sig)
{
	if (sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig Poll!\n");
	return;
}
#endif

#ifdef SIGIO
static void handle_SIGIO(int sig)
{
	if (sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig I/O!\n");
	return;
}
#endif

#ifdef TRAP_BUS
static void handle_SIGBUS(int sig)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig BUS!\n");
	return;
}
#endif

/* no ctl-c mofo */
static void handle_SIGINT(int sig)
{
	if (sig);
	return;
}
SWITCH_DECLARE(switch_status_t) switch_core_init_and_modload(const char *console, switch_core_flag_t flags, const char **err)
{
	switch_event_t *event;
	if (switch_core_init(console, flags, err) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	/* set signal handlers */
	signal(SIGINT, handle_SIGINT);
#ifdef SIGPIPE
	signal(SIGPIPE, handle_SIGPIPE);
#endif
#ifdef SIGPOLL
	signal(SIGPOLL, handle_SIGPOLL);
#endif
#ifdef SIGIO
	signal(SIGIO, handle_SIGIO);
#endif
#ifdef TRAP_BUS
	signal(SIGBUS, handle_SIGBUS);
#endif

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Bringing up environment.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Loading Modules.\n");
	if (switch_loadable_module_init() != SWITCH_STATUS_SUCCESS) {
		*err = "Cannot load modules";
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Error: %s\n", *err);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_create(&event, SWITCH_EVENT_STARTUP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Ready");
		switch_event_fire(&event);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,
					  "\nFreeSWITCH Version %s Started.\nCrash Protection [%s]\nMax Sessions[%u]\nSession Rate[%d]\nSQL [%s]\n", SWITCH_VERSION_FULL, 
					  switch_test_flag((&runtime), SCF_CRASH_PROT) ? "Enabled" : "Disabled",
					  switch_core_session_limit(0),
					  switch_core_sessions_per_second(0),
					  switch_test_flag((&runtime), SCF_USE_SQL) ? "Enabled" : "Disabled"
					  );

	switch_clear_flag((&runtime), SCF_NO_NEW_SESSIONS);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(void) switch_core_measure_time(switch_time_t total_ms, switch_core_time_duration_t *duration)
{
	switch_time_t temp = total_ms / 1000;
	memset(duration, 0, sizeof(*duration));
	duration->mms = (uint32_t) (total_ms % 1000);
	duration->ms = (uint32_t) (temp % 1000);
	temp = temp / 1000;
	duration->sec = (uint32_t) (temp % 60);
	temp = temp / 60;
	duration->min = (uint32_t) (temp % 60);
	temp = temp / 60;
	duration->hr = (uint32_t) (temp % 24);
	temp = temp / 24;
	duration->day = (uint32_t) (temp % 365);
	duration->yr = (uint32_t) (temp / 365);
}

SWITCH_DECLARE(switch_time_t) switch_core_uptime(void)
{
	return switch_time_now() - runtime.initiated;
}

SWITCH_DECLARE(int32_t) switch_core_session_ctl(switch_session_ctl_t cmd, int32_t * val)
{
	if (switch_test_flag((&runtime), SCF_SHUTTING_DOWN)) {
		return -1;
	}

	switch (cmd) {
	case SCSC_PAUSE_INBOUND:
		if (*val) {
			switch_set_flag((&runtime), SCF_NO_NEW_SESSIONS);
		} else {
			switch_clear_flag((&runtime), SCF_NO_NEW_SESSIONS);
		}
		break;
	case SCSC_HUPALL:
		switch_core_session_hupall(SWITCH_CAUSE_MANAGER_REQUEST);
		break;
	case SCSC_SHUTDOWN:
		runtime.running = 0;
		break;
	case SCSC_CHECK_RUNNING:
		*val = runtime.running;
		break;
	case SCSC_LOGLEVEL:
		if (*val > -1) {
			runtime.hard_log_level = *val;
		}

		if (runtime.hard_log_level > SWITCH_LOG_DEBUG) {
			runtime.hard_log_level = SWITCH_LOG_DEBUG;
		}
		*val = runtime.hard_log_level;
		break;
	case SCSC_MAX_SESSIONS:
		*val = switch_core_session_limit(*val);
		break;
	case SCSC_LAST_SPS:
		*val = runtime.sps_last;
		break;
	case SCSC_SPS:
		switch_mutex_lock(runtime.throttle_mutex);
		if (*val > 0) {
			runtime.sps_total = *val;
		}
		*val = runtime.sps_total;
		switch_mutex_unlock(runtime.throttle_mutex);
		break;

	case SCSC_RECLAIM:
		switch_core_memory_reclaim_all();
		*val = 0;
		break;
	}

	return 0;
}


SWITCH_DECLARE(switch_core_flag_t) switch_core_flags(void)
{
	return runtime.flags;
}

SWITCH_DECLARE(switch_bool_t) switch_core_ready(void)
{
	return (switch_test_flag((&runtime), SCF_SHUTTING_DOWN) || switch_test_flag((&runtime), SCF_NO_NEW_SESSIONS)) ? SWITCH_FALSE : SWITCH_TRUE;
}


SWITCH_DECLARE(switch_status_t) switch_core_destroy(void)
{
	switch_event_t *event;
	if (switch_event_create(&event, SWITCH_EVENT_SHUTDOWN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Shutting Down");
		switch_event_fire(&event);
	}

	switch_set_flag((&runtime), SCF_NO_NEW_SESSIONS);
	switch_set_flag((&runtime), SCF_SHUTTING_DOWN);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "End existing sessions\n");
	switch_core_session_hupall(SWITCH_CAUSE_SYSTEM_SHUTDOWN);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Clean up modules.\n");
	switch_loadable_module_shutdown();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Closing Event Engine.\n");
	switch_event_shutdown();

	if (switch_test_flag((&runtime), SCF_USE_SQL)) {
		switch_core_sqldb_stop();
	}
	switch_scheduler_task_thread_stop();

	switch_xml_destroy();
	switch_core_memory_stop();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Finalizing Shutdown.\n");
	switch_log_shutdown();



	if (runtime.console != stdout && runtime.console != stderr) {
		fclose(runtime.console);
		runtime.console = NULL;
	}

	switch_safe_free(SWITCH_GLOBAL_dirs.base_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.mod_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.conf_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.log_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.db_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.script_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.htdocs_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.grammar_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.temp_dir);


	switch_core_hash_destroy(&runtime.global_vars);
	switch_core_hash_destroy(&runtime.mime_types);

	if (runtime.memory_pool) {
		apr_pool_destroy(runtime.memory_pool);
		/* apr_terminate(); */
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_core_memory_reclaim_all(void)
{
	switch_core_memory_reclaim_logger();	
	switch_core_memory_reclaim_events();
	switch_core_memory_reclaim();
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
