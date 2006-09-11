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
#define SERVICENAME "Freeswitch"

#ifdef __ICC
#pragma warning (disable:167)
#endif


#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
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
	//nice(-20);
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
	switch_core_set_globals();
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

#ifdef WIN32
SERVICE_STATUS_HANDLE hStatus;
SERVICE_STATUS status;

void WINAPI ServiceCtrlHandler( DWORD control )
{
    switch( control )
    {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        // do shutdown stuff here
		switch_core_destroy();
        status.dwCurrentState = SERVICE_STOPPED;
        status.dwWin32ExitCode = 0;
        status.dwCheckPoint = 0;
        status.dwWaitHint = 0;
        break;
    case SERVICE_CONTROL_INTERROGATE:
        // just set the current state to whatever it is...
        break;
    }

    SetServiceStatus( hStatus, &status );
}

void WINAPI service_main( DWORD numArgs, char **args )
{
	const char *err = NULL;
    // we have to initialize the service-specific stuff
    memset( &status, 0, sizeof(SERVICE_STATUS) );
    status.dwServiceType = SERVICE_WIN32;
    status.dwCurrentState = SERVICE_START_PENDING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    hStatus = RegisterServiceCtrlHandler( SERVICENAME, &ServiceCtrlHandler );

    SetServiceStatus( hStatus, &status );
	set_high_priority();
	if (switch_core_init_and_modload(lfile, &err) != SWITCH_STATUS_SUCCESS) {
	    status.dwCurrentState = SERVICE_STOPPED;
	} else {
		status.dwCurrentState = SERVICE_RUNNING;
	}

    SetServiceStatus( hStatus, &status );
}

#endif

int main(int argc, char *argv[])
{
	char path[256] = "";
	char *ppath = NULL;
	const char *err = NULL;
	int bg = 0;
	FILE *f;
	pid_t pid = 0;

#ifdef WIN32
    SERVICE_TABLE_ENTRY dispatchTable[] =
    {
        { SERVICENAME, &service_main },
        { NULL, NULL }
    };

	if (argv[1] && !strcmp(argv[1], "-service")) {
		if(StartServiceCtrlDispatcher( dispatchTable ) == 0 )
		{
			//Not loaded as a service
			fprintf(stderr, "Error Freeswitch loaded as a console app with -service option\n");
			fprintf(stderr, "To install the service load freeswitch with -install\n");
		}
		exit(0);
	}
	if (argv[1] && !strcmp(argv[1], "-install")) {
		char exePath[1024];
	    char servicePath[1024];

		SC_HANDLE handle = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
		GetModuleFileName( NULL, exePath, 1024 );
		snprintf(servicePath, sizeof(servicePath), "%s -service", exePath);
		CreateService(
			handle,
			SERVICENAME,
			SERVICENAME,
			GENERIC_READ | GENERIC_EXECUTE,
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START,
			SERVICE_ERROR_IGNORE,
			servicePath,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
		);
		exit(0);
	}
	if (argv[1] && !strcmp(argv[1], "-uninstall")) {
		SC_HANDLE handle = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
		SC_HANDLE service = OpenService( handle, SERVICENAME, DELETE );
		if( service != NULL )
		{
			// remove the service!
			DeleteService( service );
		}
		exit(0);
	}
#endif

	set_high_priority();

	if (argv[1] && !strcmp(argv[1], "-stop")) {
		return freeswitch_kill_background();
	}

	if (argv[1] && !strcmp(argv[1], "-nc")) {
		bg++;
	}

	if (bg) {
		ppath = lfile;

		signal(SIGHUP, (void *) handle_SIGHUP);
		signal(SIGTERM, (void *) handle_SIGHUP);

#ifdef WIN32
		FreeConsole();
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", getpid());
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

	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);
	if ((f = fopen(path, "w")) == 0) {
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}

	fprintf(f, "%d", pid = getpid());
	fclose(f);

	freeswitch_runtime_loop(bg);

	return switch_core_destroy();
}
