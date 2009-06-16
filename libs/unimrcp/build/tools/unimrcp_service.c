/*
 * Copyright 2008 Arsen Chaloyan
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
 */

#include <windows.h>
#include <apr_getopt.h>
#include "apt.h"
#include "apt_pool.h"

#define WIN_SERVICE_NAME "unimrcp"


/** Register/install service in SCM */
static apt_bool_t uni_service_register(const char *root_dir_path, apr_pool_t *pool)
{
	char *bin_path;
	SERVICE_DESCRIPTION desc;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		printf("Failed to Open SCManager %d\n", GetLastError());
		return FALSE;
	}

	bin_path = apr_psprintf(pool,"%s\\bin\\unimrcpserver.exe --service --root-dir \"%s\" -o 2",
					root_dir_path,
					root_dir_path);
	sch_service = CreateService(
					sch_manager,
					WIN_SERVICE_NAME,
					"UniMRCP Server",
					GENERIC_EXECUTE | SERVICE_CHANGE_CONFIG,
					SERVICE_WIN32_OWN_PROCESS,
					SERVICE_DEMAND_START,
					SERVICE_ERROR_NORMAL,
					bin_path,0,0,0,0,0);
	if(!sch_service) {
		printf("Failed to Create Service %d\n", GetLastError());
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	desc.lpDescription = "Launches UniMRCP Server";
	if(!ChangeServiceConfig2(sch_service,SERVICE_CONFIG_DESCRIPTION,&desc)) {
		printf("Failed to Set Service Description %d\n", GetLastError());
	}

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return TRUE;
}

/** Unregister/uninstall service from SCM */
static apt_bool_t uni_service_unregister()
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		printf("Failed to Open SCManager %d\n", GetLastError());
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,DELETE|SERVICE_STOP);
	if(!sch_service) {
		printf("Failed to Open Service %d\n", GetLastError());
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status);
	if(!DeleteService(sch_service)) {
		printf("Failed to Delete Service %d\n", GetLastError());
		status = FALSE;
	}
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Start service */
static apt_bool_t uni_service_start()
{
	apt_bool_t status = TRUE;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		printf("Failed to Open SCManager %d\n", GetLastError());
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,SERVICE_START);
	if(!sch_service) {
		printf("Failed to Open Service %d\n", GetLastError());
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!StartService(sch_service,0,NULL)) {
		printf("Failed to Start Service %d\n", GetLastError());
		status = FALSE;
	}
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Stop service */
static apt_bool_t uni_service_stop()
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		printf("Failed to Open SCManager %d\n", GetLastError());
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,SERVICE_STOP);
	if(!sch_service) {
		printf("Failed to Open Service %d\n", GetLastError());
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status)) {
		printf("Failed to Stop Service %d\n", GetLastError());
		status = FALSE;
	}

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}


static void usage()
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  unimrcpservice [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -r [--register] rootdir : Register the Windows service.\n"
		"\n"
		"   -u [--unregister]       : Unregister the Windows service.\n"
		"\n"
		"   -s [--start]            : Start the Windows service.\n"
		"\n"
		"   -t [--stop]             : Stop the Windows service.\n"
		"\n"
		"   -h [--help]             : Show the help.\n"
		"\n");
}

int main(int argc, const char * const *argv)
{
	apr_pool_t *pool;
	apr_status_t rv;
	apr_getopt_t *opt;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "register",   'r', TRUE,  "register service" },  /* -r or --register arg */
		{ "unregister", 'u', FALSE, "unregister service" },/* -u or --unregister */
		{ "start",      's', FALSE, "start service" },     /* -s or --start */
		{ "stop",       't', FALSE, "stop service" },      /* -t or --stop */
		{ "help",       'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		apr_terminate();
		return 0;
	}

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv == APR_SUCCESS) {
		int optch;
		const char *optarg;
		while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
			switch(optch) {
				case 'r':
					uni_service_register(optarg,pool);
					break;
				case 'u':
					uni_service_unregister();
					break;
				case 's':
					uni_service_start();
					break;
				case 't':
					uni_service_stop();
					break;
				case 'h':
					usage();
					break;
			}
		}
		if(rv != APR_EOF) {
			usage();
		}
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return 0;
}
