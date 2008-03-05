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
 * switch_console.c -- Simple Console
 *
 */

#include <switch.h>
#include <switch_console.h>
#include <switch_version.h>
#define CMD_BUFLEN 1024;

/*
 * store a strdup() of the string configured in XML
 * bound to each of the 12 function key
 */
static char *console_fnkeys[12];

/*
 * Load from console.conf XML file the section:
 * <keybindings>
 * <key name="1" value="show calls"/>
 * </keybindings>
 */
static switch_status_t console_xml_config(void)
{
	char *cf = "switch.conf";
	switch_xml_t cfg, xml, settings, param;

    /* clear the keybind array */
    int i;

    for (i = 0; i < 12; i++) {
        console_fnkeys[i] = NULL;
    }

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "cli-keybindings"))) {
		for (param = switch_xml_child(settings, "key"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
            int i = atoi(var);
            if ((i < 1) || (i > 12)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "keybind %s is invalid, range is from 1 to 12\n", var);
            } else {
                // Add the command to the fnkey array
                console_fnkeys[i - 1] = switch_core_permanent_strdup(val);
            }
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen)
{
	switch_size_t nwrite;
	FILE *out = switch_core_get_console();

	if (out) {
		nwrite = fwrite(data, datalen, 1, out);
		if (nwrite != datalen) {
			return SWITCH_STATUS_FALSE;
		}
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *buf = handle->data;
	char *end = handle->end;
	int ret = 0;
	char *data = NULL;

	if (handle->data_len >= handle->data_size) {
		return SWITCH_STATUS_FALSE;
	}

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		switch_size_t remaining = handle->data_size - handle->data_len;
		switch_size_t need = strlen(data) + 1;

		if ((remaining < need) && handle->alloc_len) {
			switch_size_t new_len;
			void *new_data;

			new_len = handle->data_size + need + handle->alloc_chunk;
			if ((new_data = realloc(handle->data, new_len))) {
				handle->data_size = handle->alloc_len = new_len;
				handle->data = new_data;
				buf = handle->data;
				remaining = handle->data_size - handle->data_len;
				handle->end = (uint8_t *) (handle->data) + handle->data_len;
				end = handle->end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				free(data);
				return SWITCH_STATUS_FALSE;
			}
		}

		if (remaining < need) {
			ret = -1;
		} else {
			ret = 0;
			switch_snprintf(end, remaining, "%s", data);
			handle->data_len = strlen(buf);
			handle->end = (uint8_t *) (handle->data) + handle->data_len;
		}
		free(data);
	}

	return ret ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

static int switch_console_process(char *cmd)
{
	char *arg = NULL;
	switch_stream_handle_t stream = { 0 };

	if (!strcmp(cmd, "shutdown") || !strcmp(cmd, "...")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Bye!\n");
		return 0;
	}
	if (!strcmp(cmd, "version")) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "FreeSwitch Version %s\n", SWITCH_VERSION_FULL);
		return 1;
	}
	if ((arg = strchr(cmd, '\r')) != 0 || (arg = strchr(cmd, '\n')) != 0) {
		*arg = '\0';
		arg = NULL;
	}
	if ((arg = strchr(cmd, ' ')) != 0) {
		*arg++ = '\0';
	}

	SWITCH_STANDARD_STREAM(stream);
	if (stream.data) {
		if (switch_api_execute(cmd, arg, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "API CALL [%s(%s)] output:\n%s\n", cmd, arg ? arg : "", (char *) stream.data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Unknown Command: %s\n", cmd);
		}
		free(stream.data);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
	}

	return 1;
}

SWITCH_DECLARE(void) switch_console_printf(switch_text_channel_t channel, const char *file, const char *func, int line, const char *fmt, ...)
{
	char *data = NULL;
	int ret = 0;
	va_list ap;
	FILE *handle = switch_core_data_channel(channel);
	const char *filep = switch_cut_path(file);
	char date[80] = "";
	switch_size_t retsize;
	switch_time_exp_t tm;
	switch_event_t *event;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		fprintf(stderr, "Memory Error\n");
		goto done;
	} 

	if (channel == SWITCH_CHANNEL_ID_LOG_CLEAN) {
		fprintf(handle, "%s", data);
		goto done;
	}

	switch_time_exp_lt(&tm, switch_timestamp_now());
	switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

	if (channel == SWITCH_CHANNEL_ID_LOG) {
		fprintf(handle, "[%d] %s %s:%d %s() %s", (int) getpid(), date, filep, line, func, data);
		goto done;
	} 

	if (channel == SWITCH_CHANNEL_ID_EVENT &&
		switch_event_running() == SWITCH_STATUS_SUCCESS && 
		switch_event_create(&event, SWITCH_EVENT_LOG) == SWITCH_STATUS_SUCCESS) {

		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Data", "%s", data);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-File", "%s", filep);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Function", "%s", func);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Line", "%d", line);
		switch_event_fire(&event);
	}

done:
	if (data) {
		free(data);
	}
	fflush(handle);
}

static char hostname[256] = "";
static int32_t running = 1;

#ifdef SWITCH_HAVE_LIBEDIT
#include <histedit.h>
static char prompt_str[512] = "";

/*
 * If a fnkey is configured then process the command
 */
static unsigned char console_fnkey_pressed(int i) {
	char *c, *cmd;

    assert((i > 0) && (i <= 12));

    c = console_fnkeys[i-1];

    // This new line is necessary to avoid output to begin after the ">" of the CLI's prompt
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE,"\n");

    if (c == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "FUNCTION KEY F%d IS NOT BOUND, please edit switch.conf XML file\n", i);
        return CC_REDISPLAY;
    }
	
	cmd = strdup(c);
	switch_console_process(cmd);
	free(cmd);

    return CC_REDISPLAY;
}

static unsigned char console_f1key(EditLine *el, int ch) {
    return console_fnkey_pressed(1);
}
static unsigned char console_f2key(EditLine *el, int ch) {
    return console_fnkey_pressed(2);
}
static unsigned char console_f3key(EditLine *el, int ch) {
    return console_fnkey_pressed(3);
}
static unsigned char console_f4key(EditLine *el, int ch) {
    return console_fnkey_pressed(4);
}
static unsigned char console_f5key(EditLine *el, int ch) {
    return console_fnkey_pressed(5);
}
static unsigned char console_f6key(EditLine *el, int ch) {
    return console_fnkey_pressed(6);
}
static unsigned char console_f7key(EditLine *el, int ch) {
    return console_fnkey_pressed(7);
}
static unsigned char console_f8key(EditLine *el, int ch) {
    return console_fnkey_pressed(8);
}
static unsigned char console_f9key(EditLine *el, int ch) {
    return console_fnkey_pressed(9);
}
static unsigned char console_f10key(EditLine *el, int ch) {
    return console_fnkey_pressed(10);
}
static unsigned char console_f11key(EditLine *el, int ch) {
    return console_fnkey_pressed(11);
}
static unsigned char console_f12key(EditLine *el, int ch) {
    return console_fnkey_pressed(12);
}


char * prompt(EditLine *e) {
	if (*prompt_str == '\0') {
		gethostname(hostname, sizeof(hostname));
		switch_snprintf(prompt_str, sizeof(prompt_str), "freeswitch@%s> ", hostname);
	}	

	return prompt_str;
}

static EditLine *el;
static History *myhistory;
static HistEvent ev;
static char *hfile = NULL;

static void *SWITCH_THREAD_FUNC console_thread(switch_thread_t *thread, void *obj)
{
	int count;
	const char *line;
	switch_memory_pool_t *pool = (switch_memory_pool_t *) obj;

	while (running) {
		int32_t arg;

		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}

		line = el_gets(el, &count);

		if (count > 1) {
			if (!switch_strlen_zero(line)) {
				char *cmd = strdup(line);
				char *p;

				if ((p = strrchr(cmd, '\r')) || (p = strrchr(cmd, '\n'))) {
					*p = '\0';
				}
				assert(cmd != NULL);
				history(myhistory, &ev, H_ENTER, line);
				running = switch_console_process(cmd);
				free(cmd);
			}
		}
		switch_yield(1000);
	}
	
  	switch_core_destroy_memory_pool(&pool);	
	return NULL;
}


SWITCH_DECLARE(void) switch_console_loop(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return;
	}

	el = el_init(__FILE__, switch_core_get_console(), switch_core_get_console(), switch_core_get_console());
	el_set(el, EL_PROMPT, &prompt);
	el_set(el, EL_EDITOR, "emacs");
    /* AGX: Bind Keyboard function keys. This has been tested with:
     * - linux console keyabord
     * - putty.exe connected via ssh to linux
     */
    /* Load/Init the config first */
    console_xml_config();
    /* Bind the functions to the key */
    el_set(el, EL_ADDFN, "f1-key", "F1 KEY PRESS", console_f1key );
    el_set(el, EL_ADDFN, "f2-key", "F2 KEY PRESS", console_f2key );
    el_set(el, EL_ADDFN, "f3-key", "F3 KEY PRESS", console_f3key );
    el_set(el, EL_ADDFN, "f4-key", "F4 KEY PRESS", console_f4key );
    el_set(el, EL_ADDFN, "f5-key", "F5 KEY PRESS", console_f5key );
    el_set(el, EL_ADDFN, "f6-key", "F6 KEY PRESS", console_f6key );
    el_set(el, EL_ADDFN, "f7-key", "F7 KEY PRESS", console_f7key );
    el_set(el, EL_ADDFN, "f8-key", "F8 KEY PRESS", console_f8key );
    el_set(el, EL_ADDFN, "f9-key", "F9 KEY PRESS", console_f9key );
    el_set(el, EL_ADDFN, "f10-key", "F10 KEY PRESS", console_f10key );
    el_set(el, EL_ADDFN, "f11-key", "F11 KEY PRESS", console_f11key );
    el_set(el, EL_ADDFN, "f12-key", "F12 KEY PRESS", console_f12key );

    el_set(el, EL_BIND, "\033OP", "f1-key", NULL);
    el_set(el, EL_BIND, "\033OQ", "f2-key", NULL);
    el_set(el, EL_BIND, "\033OR", "f3-key", NULL);
    el_set(el, EL_BIND, "\033OS", "f4-key", NULL);

	
    el_set(el, EL_BIND, "\033[11~", "f1-key", NULL);
    el_set(el, EL_BIND, "\033[12~", "f2-key", NULL);
    el_set(el, EL_BIND, "\033[13~", "f3-key", NULL);
    el_set(el, EL_BIND, "\033[14~", "f4-key", NULL);
    el_set(el, EL_BIND, "\033[15~", "f5-key", NULL);
    el_set(el, EL_BIND, "\033[17~", "f6-key", NULL);
    el_set(el, EL_BIND, "\033[18~", "f7-key", NULL);
    el_set(el, EL_BIND, "\033[19~", "f8-key", NULL);
    el_set(el, EL_BIND, "\033[20~", "f9-key", NULL);
    el_set(el, EL_BIND, "\033[21~", "f10-key", NULL);
    el_set(el, EL_BIND, "\033[23~", "f11-key", NULL);
    el_set(el, EL_BIND, "\033[24~", "f12-key", NULL);

	myhistory = history_init();
	if (myhistory == 0) {
		fprintf(stderr, "history could not be initialized\n");
		return;
	}

	hfile = switch_mprintf("%s%sfreeswitch.history", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	assert(hfile != NULL);

	history(myhistory, &ev, H_SETSIZE, 800);
	el_set(el, EL_HIST, history, myhistory);
	history(myhistory, &ev, H_LOAD, hfile);

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, console_thread, pool, pool);
	
	while (running) {
		int32_t arg = 0;
		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}
		switch_yield(1000000);
	}

	history(myhistory, &ev, H_SAVE, hfile);
	free(hfile);
	
	/* Clean up our memory */
	history_end(myhistory);
	el_end(el);
}

#else

SWITCH_DECLARE(void) switch_console_loop(void)
{

	char cmd[2048];
	int32_t activity = 1;
	switch_size_t x = 0;

	gethostname(hostname, sizeof(hostname));

	while (running) {
		int32_t arg;
#ifndef _MSC_VER
		fd_set rfds, efds;
		struct timeval tv = { 0, 20000 };
#endif

		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}

		if (activity) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "\nfreeswitch@%s> ", hostname);
		}
#ifdef _MSC_VER
		activity = WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE), 20);

		if (activity == 102) {
			fflush(stdout);
			continue;
		}
#else
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(fileno(stdin), &rfds);
		FD_SET(fileno(stdin), &efds);
		if ((activity = select(fileno(stdin) + 1, &rfds, NULL, &efds, &tv)) < 0) {
			break;
		}

		if (activity == 0) {
			fflush(stdout);
			continue;
		}
#endif
		memset(&cmd, 0, sizeof(cmd));
		for (x = 0; x < (sizeof(cmd) - 1); x++) {
			int c = getchar();
			if (c < 0) {
				int y = read(fileno(stdin), cmd, sizeof(cmd));
				cmd[y - 1] = '\0';
				break;
			}

			cmd[x] = (char) c;

			if (cmd[x] == '\n') {
				cmd[x] = '\0';
				break;
			}
		}

		if (cmd[0]) {
			running = switch_console_process(cmd);
		}
	}
}
#endif

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
