/*
 * Copyright 2008-2014 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * $Id: uni_service.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <windows.h>
#include <apr_lib.h>
#include "unimrcp_server.h"
#include "apt_log.h"

#define WIN_SERVICE_NAME "unimrcp"

static SERVICE_STATUS_HANDLE win_service_status_handle = NULL;
static SERVICE_STATUS win_service_status;

static mrcp_server_t *server = NULL;
static apt_dir_layout_t *service_dir_layout = NULL;
static const char *svcname = NULL;

/** Display error message with Windows error code and description */
static void winerror(const char *file, int line, const char *msg)
{
	char buf[128];
	DWORD err = GetLastError();
	int ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	apt_log(file, line, APT_PRIO_WARNING, "%s: %lu %.*s\n", msg, err, ret, buf);
}

/** SCM state change handler */
static void WINAPI win_service_handler(DWORD control)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Service Handler 0x%02lx",control);
	switch (control)
	{
		case SERVICE_CONTROL_INTERROGATE:
			break;
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			if(server) {
				win_service_status.dwCurrentState = SERVICE_STOP_PENDING; 
				if(!SetServiceStatus(win_service_status_handle, &win_service_status)) { 
					winerror(APT_LOG_MARK, "Failed to Set Service Status");
				}

				/* shutdown server */
				unimrcp_server_shutdown(server);
			}
			win_service_status.dwCurrentState = SERVICE_STOPPED; 
			win_service_status.dwCheckPoint = 0; 
			win_service_status.dwWaitHint = 0; 
			break;
	}

	if(!SetServiceStatus(win_service_status_handle, &win_service_status)) {
		winerror(APT_LOG_MARK, "Failed to Set Service Status");
	}
}

static void WINAPI win_service_main(DWORD argc, LPTSTR *argv)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Service Main");
	win_service_status.dwServiceType = SERVICE_WIN32;
	win_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	win_service_status.dwWin32ExitCode = 0;
	win_service_status.dwServiceSpecificExitCode = 0;
	win_service_status.dwCheckPoint = 0;
	win_service_status.dwWaitHint = 0;

	win_service_status_handle = RegisterServiceCtrlHandler(svcname, win_service_handler);
	if(win_service_status_handle == (SERVICE_STATUS_HANDLE)0) {
		winerror(APT_LOG_MARK, "Failed to Register Service Control Handler");
		return;
	} 

	win_service_status.dwCurrentState = SERVICE_START_PENDING;
	if(!SetServiceStatus(win_service_status_handle, &win_service_status)) {
		winerror(APT_LOG_MARK, "Failed to Set Service Status");
	} 

	/* start server */
	server = unimrcp_server_start(service_dir_layout);

	win_service_status.dwCurrentState =  server ? SERVICE_RUNNING : SERVICE_STOPPED;
	if(!SetServiceStatus(win_service_status_handle, &win_service_status)) {
		winerror(APT_LOG_MARK, "Failed to Set Service Status");
	} 
}

/** Run SCM service */
apt_bool_t uni_service_run(const char *name, apt_dir_layout_t *dir_layout, apr_pool_t *pool)
{
	SERVICE_TABLE_ENTRY win_service_table[2];
	svcname = name ? name : WIN_SERVICE_NAME;
	memset(&win_service_table, 0, sizeof(win_service_table));
	win_service_table->lpServiceName = (char *) svcname;
	win_service_table->lpServiceProc = win_service_main;

	service_dir_layout = dir_layout;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Run as Service %s",svcname);
	if(!StartServiceCtrlDispatcher(win_service_table)) {
		winerror(APT_LOG_MARK, "Failed to Connect to SCM");
	}
	return TRUE;
}
