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
 * switch.c -- Main
 *
 */

#include <switch.h>

#define PIDFILE "freeswitch.pid"
#define LOGFILE "freeswitch.log"
static int RUNNING = 0;
static char *lfile = LOGFILE;
static char *pfile = PIDFILE;

#ifdef __ICC
#pragma warning (disable:167)
#endif


#ifdef WIN32
static HANDLE shutdown_event;
#endif

	static int handle_SIGHUP(int sig)
{
	if(sig);
	RUNNING = 0;
	return 0;
}

static void set_high_priority()
{
#ifdef WIN32
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
	nice(-20);
#endif
}

static void freeswitch_runtime_loop(int bg)
{
	if (bg) {
		bg = 0;
#ifdef WIN32
		WaitForSingleObject(shutdown_event, INFINITE);
#else
		RUNNING = 1;
		while(RUNNING) {
			switch_yield(10000);
		}
#endif
	}  else {
		/* wait for console input */
		switch_console_loop();
	}
}

static int freeswitch_kill_background()
{
	FILE *f;
	char path[256] = "";
	pid_t pid = 0;
	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);
	if ((f = fopen(path, "r")) == 0) {
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}
	fscanf(f, "%d", &pid);
	if (pid > 0) {
		fprintf(stderr, "Killing: %d\n", (int) pid);
#ifdef WIN32
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", pid);
		shutdown_event = OpenEvent(EVENT_MODIFY_STATE, FALSE, path);
		if (!shutdown_event) {
			/* we can't get the event, so we can't signal the process to shutdown */
			fprintf(stderr, "ERROR: Can't Shutdown: %d\n", (int) pid);
		} else {
			SetEvent(shutdown_event);
		}
		CloseHandle(shutdown_event);
#else
		kill(pid, SIGTERM);
#endif
	}

	fclose(f);
	return 0;
}

int main(int argc, char *argv[])
{
	char path[256] = "";
	char *ppath = NULL;
	const char *err = NULL;
	int bg = 0;
	FILE *f;
	pid_t pid = 0;

	set_high_priority();
	switch_core_set_globals();

	if (argv[1] && !strcmp(argv[1], "-stop")) {
		return freeswitch_kill_background();
	}

	if (argv[1] && !strcmp(argv[1], "-nc")) {
		bg++;
	}

	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);
	if ((f = fopen(path, "w")) == 0) {
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}

	fprintf(f, "%d", pid = getpid());
	fclose(f);

	if (bg) {
		ppath = lfile;

		signal(SIGHUP, (void *) handle_SIGHUP);
		signal(SIGTERM, (void *) handle_SIGHUP);

#ifdef WIN32
		FreeConsole();
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", pid);
		shutdown_event = CreateEvent(NULL, FALSE, FALSE, path);		
#else
		if ((pid = fork())) {
			fprintf(stderr, "%d Backgrounding.\n", (int)pid);
			exit(0);
		}
#endif
	}

	if (switch_core_init_and_modload(ppath, &err) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot Initilize [%s]\n", err);
		return 255;
	}

	freeswitch_runtime_loop(bg);

	return switch_core_destroy();
}
