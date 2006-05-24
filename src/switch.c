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
#include <switch_version.h>

static int RUNNING = 0;

static int handle_SIGPIPE(int sig)
{
	if(sig);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig Pipe!\n");
	return 0;
}
#ifdef TRAP_BUS
static int handle_SIGBUS(int sig)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Sig BUS!\n");
	return 0;
}
#endif

/* no ctl-c mofo */
static int handle_SIGINT(int sig)
{
	if (sig);
	return 0;
}


static int handle_SIGHUP(int sig)
{
	if(sig);
	RUNNING = 0;
	return 0;
}

int main(int argc, char *argv[])
{
	char *lfile = "freeswitch.log";
	char *pfile = "freeswitch.pid";
	char path[256] = "";
	char *ppath = NULL;
	char *err = NULL;
	switch_event_t *event;
	int bg = 0;
	FILE *f;
#ifdef WIN32
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#else
	int pid;
	//nice(-20);
#endif

#ifndef WIN32
	if (argv[1] && !strcmp(argv[1], "-stop")) {
		pid_t pid = 0;
		switch_core_set_globals();
		snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);
		if ((f = fopen(path, "r")) == 0) {
			fprintf(stderr, "Cannot open pid file %s.\n", path);
			return 255;
		}
		fscanf(f, "%d", &pid);
		if (pid > 0) {
			fprintf(stderr, "Killing %d\n", (int) pid);
			kill(pid, SIGTERM);
		}

		fclose(f);
		return 0;
	}
#endif

	if (argv[1] && !strcmp(argv[1], "-nc")) {
		bg++;
	}

	if (bg) {
		//snprintf(path, sizeof(path), "%s%c%s", SWITCH_GLOBAL_dirs.log_dir, sep, lfile);
		ppath = lfile;

		signal(SIGHUP, (void *) handle_SIGHUP);
		signal(SIGTERM, (void *) handle_SIGHUP);

#ifdef WIN32
		FreeConsole();
#else
		if ((pid = fork())) {
			fprintf(stderr, "%d Backgrounding.\n", (int)pid);
			exit(0);
		}
#endif
	
	}


	if (switch_core_init(ppath) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot Initilize\n");
		return 255;
	}


#ifdef __ICC
#pragma warning (disable:167)
#endif

	/* set signal handlers */
	signal(SIGINT, (void *) handle_SIGINT);
#ifdef SIGPIPE
	signal(SIGPIPE, (void *) handle_SIGPIPE);
#endif
#ifdef TRAP_BUS
	signal(SIGBUS, (void *) handle_SIGBUS);
#endif






	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);
	if ((f = fopen(path, "w")) == 0) {
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}

	fprintf(f, "%d", getpid());
	fclose(f);


	
	if (!err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Bringing up environment.\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Loading Modules.\n");
		if (switch_loadable_module_init() != SWITCH_STATUS_SUCCESS) {
			err = "Cannot load modules";
		}
	}

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Error: %s", err);
		exit(-1);
	}

	if (switch_event_create(&event, SWITCH_EVENT_STARTUP) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Ready");
		switch_event_fire(&event);
	}

#ifdef CRASH_PROT
#define __CP "ENABLED"
#else
#define __CP "DISABLED"
#endif

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "freeswitch Version %s Started. Crash Protection [%s]\n\n", SWITCH_VERSION_FULL, __CP);
	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);

	if (bg) {
		bg = 0;
		RUNNING = 1;
		while(RUNNING) {
#ifdef WIN32
		bg++;
		if(bg == 100) {
			if ((f = fopen(path, "r")) == 0) {
				break;
			}
			fclose(f);
			bg = 0;
		}
#endif
			switch_yield(10000);
		}
	}  else {
		/* wait for console input */
		switch_console_loop();
	}
	
	if (switch_event_create(&event, SWITCH_EVENT_SHUTDOWN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Event-Info", "System Shutting Down");
		switch_event_fire(&event);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Clean up modules.\n");
	switch_loadable_module_shutdown();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Tearing down environment.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Exiting Now.\n");
	switch_core_destroy();
	return 0;
}
