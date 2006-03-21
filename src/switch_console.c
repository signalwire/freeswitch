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
#include <switch_console.h>
#include <switch.h>

static int switch_console_process(char *cmd)
{
	char *arg = NULL;
	char *retbuf = (char *)malloc(SWITCH_RECCOMMENDED_BUFFER_SIZE * 10);

#ifdef EMBED_PERL
	const char *perlhelp = "perl - execute some perl. (print to STDERR if you want to see it.)\n";
#else
	const char *perlhelp = "";
#endif
	if (!strcmp(cmd, "shutdown") || !strcmp(cmd, "...")) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Bye!\n");
		return 0;
	}
	if (!strcmp(cmd, "version")) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "FreeSwitch Version %s\n", SWITCH_VERSION_FULL);
		return 1;
	}
	if (!strcmp(cmd, "help")) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE,
							  "\n"
							  "Valid Commands:\n\n"
							  "version\n" "help - umm yeah..\n" "%sshutdown - stop the program\n\n", perlhelp);
		return 1;
	}

#ifdef EMBED_PERL
	if (!strncmp(cmd, "perl ", 5)) {
		cmd += 5;
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Executing perl code [%s]\n", cmd);
		switch_core_do_perl(cmd);
		return 1;
	}
#endif
	if ((arg = strchr(cmd, '\r')) != 0  || (arg = strchr(cmd, '\n')) != 0 )  {
		*arg = '\0';
		arg = NULL;
	}
	if ((arg = strchr(cmd, ' ')) != 0) {
		*arg++ = '\0';
	}
	if (switch_api_execute(cmd, arg, retbuf, sizeof(retbuf)) == SWITCH_STATUS_SUCCESS) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "API CALL [%s(%s)] output:\n%s\n", cmd, arg ? arg : "",
							  retbuf);
	} else {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Unknown Command: %s\n", cmd);
	}
	return 1;
}

SWITCH_DECLARE(void) switch_console_printf(switch_text_channel channel, char *file, const char *func, int line,
										   char *fmt, ...)
{
	char *data = NULL;
	int ret = 0;
	va_list ap;
	FILE *handle;
	char *filep = switch_cut_path(file);

	va_start(ap, fmt);

	handle = switch_core_data_channel(channel);

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
		char date[80] = "";

		if (channel == SWITCH_CHANNEL_ID_CONSOLE_CLEAN) {
			fprintf(handle, "%s", data);
		} else {
			size_t retsize;
			switch_time_exp_t tm;
			switch_event *event;
			switch_time_exp_lt(&tm, switch_time_now());
			switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

			if (channel == SWITCH_CHANNEL_ID_CONSOLE) {
				fprintf(handle, "[%d] %s %s:%d %s() %s", (int) getpid(), date, filep, line, func, data);
			}

			else if (channel == SWITCH_CHANNEL_ID_EVENT &&
					 switch_event_running() == SWITCH_STATUS_SUCCESS &&
					 switch_event_create(&event, SWITCH_EVENT_LOG) == SWITCH_STATUS_SUCCESS) {

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Data", "%s", data);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-File", "%s", filep);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Function", "%s", func);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Line", "%d", line);
				switch_event_fire(&event);
			}
		}
	}
	if(data) {
		free(data);
	}
	fflush(handle);
}

SWITCH_DECLARE(void) switch_console_loop(void)
{
	char hostname[256];
	char cmd[2048];
	int running = 1, x = 0, activity = 1;

	gethostname(hostname, sizeof(hostname));


	while (running) {
		if (activity) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "\nfreeswitch@%s> ", hostname);
		}
		//activity = switch_socket_waitfor(fileno(stdin), 100, POLLIN | POLLERR);

		if (activity == 0) {
			fflush(stdout);
			switch_sleep(100);
			continue;
		}

		memset(&cmd, 0, sizeof(cmd));
		for (x = 0; x < sizeof(cmd); x++) {
			cmd[x] = (char)getchar();
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
