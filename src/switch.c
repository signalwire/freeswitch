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
 *
 *
 * switch.c -- Main
 *
 */

#include <switch.h>

//pid filename: Stores the process id of the freeswitch process
#define PIDFILE "freeswitch.pid"
static char *pfile = PIDFILE;

//log filename: Filename of the freeswitch log file to be used if we are in background mode
#define LOGFILE "freeswitch.log"
static char *lfile = LOGFILE;

//If we are a windows service, what should we be called
#define SERVICENAME "Freeswitch"

//Picky compiler
#ifdef __ICC
#pragma warning (disable:167)
#endif

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>

//event to signal shutdown (for you unix people, this is like a pthread_cond)
static HANDLE shutdown_event;
#endif

//signal handler for when freeswitch is running in background mode.
//signal triggers the shutdown of freeswitch
static void handle_SIGHUP(int sig)
{
	uint32_t arg = 0;
	if(sig);
	//send shutdown signal to the freeswitch core
	switch_core_session_ctl(SCSC_SHUTDOWN, &arg);
	return;
}

//kill a freeswitch process running in background mode
static int freeswitch_kill_background()
{
	FILE *f;				//FILE handle to open the pid file
	char path[256] = "";	//full path of the PID file
	pid_t pid = 0;			//pid from the pid file	

	//set the globals so we can use the global paths.
	switch_core_set_globals();

	//get the full path of the pid file.
	snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, pfile);

	//open the pid file
	if ((f = fopen(path, "r")) == 0) {
		//pid file does not exist
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}

	//pull the pid from the file
	fscanf(f, "%d", &pid);

	//if we have a valid pid
	if (pid > 0) {

		//kill the freeswitch running at the pid we found
		fprintf(stderr, "Killing: %d\n", (int) pid);
#ifdef WIN32
		//for windows we need the event to signal for shutting down a background freewitc
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", pid);

		//open the event so we can signal it
		shutdown_event = OpenEvent(EVENT_MODIFY_STATE, FALSE, path);

		//did we sucessfully open the event
		if (!shutdown_event) {
			/* we can't get the event, so we can't signal the process to shutdown */
			fprintf(stderr, "ERROR: Can't Shutdown: %d\n", (int) pid);
		} else {
			//signal the event to shutdown
			SetEvent(shutdown_event);
		}
		//cleanup
		CloseHandle(shutdown_event);
#else
		//for unix, send the signal to kill.
		kill(pid, SIGTERM);
#endif
	}

	//be nice and close the file handle to the pid file
	fclose(f);

	return 0;
}

#ifdef WIN32

//we need these vars to handle the service
SERVICE_STATUS_HANDLE hStatus;
SERVICE_STATUS status;

//Handler function for service start/stop from the service 
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
	int x, die = 0;

#ifdef WIN32
	SERVICE_TABLE_ENTRY dispatchTable[] =
	{
		{ SERVICENAME, &service_main },
		{ NULL, NULL }
	};
#endif

	for (x = 1; x < argc; x++) {
#ifdef WIN32
		if (x == 1) {
			if (argv[x] && !strcmp(argv[x], "-service")) {
				if(StartServiceCtrlDispatcher( dispatchTable ) == 0 ) {
					//Not loaded as a service
					fprintf(stderr, "Error Freeswitch loaded as a console app with -service option\n");
					fprintf(stderr, "To install the service load freeswitch with -install\n");
				}
				exit(0);
			}
			if (argv[x] && !strcmp(argv[x], "-install")) {
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
			if (argv[x] && !strcmp(argv[x], "-uninstall")) {
				SC_HANDLE handle = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
				SC_HANDLE service = OpenService( handle, SERVICENAME, DELETE );
				if( service != NULL ) {
					// remove the service!
					DeleteService( service );
				}
				exit(0);
			}
		}
#endif
		if (argv[x] && !strcmp(argv[x], "-hp")) {
			set_high_priority();
		}

		if (argv[x] && !strcmp(argv[x], "-stop")) {
			die++;
		}

		if (argv[x] && !strcmp(argv[x], "-nc")) {
			bg++;
		}
	}

	if (die) {
		return freeswitch_kill_background();
	}

	if (bg) {
		ppath = lfile;

		signal(SIGHUP, handle_SIGHUP);
		signal(SIGTERM, handle_SIGHUP);

#ifdef WIN32
		FreeConsole();
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

	switch_core_runtime_loop(bg);

	return switch_core_destroy();
}
