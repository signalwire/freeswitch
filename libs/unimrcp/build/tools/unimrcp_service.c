/*
 * Copyright 2008-2015 Arsen Chaloyan
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
#include <apr_file_info.h>
#include <apr_strings.h>
#include "apt.h"
#include "apt_pool.h"

#define WIN_SERVICE_NAME "unimrcp"

/** UniMRCP service register command enumeration */
typedef enum uni_service_register_e {
	USR_NONE, USR_REGISTER, USR_UNREGISTER
} uni_service_register_e;

/** UniMRCP service control command enumeration */
typedef enum uni_service_control_e {
	USC_NONE, USC_START, USC_STOP
} uni_service_control_e;


/** Display error message with Windows error code and description */
static void winerror(const char *msg)
{
	char buf[128];
	DWORD err = GetLastError();
	int ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	printf("%s: %lu %.*s\n", msg, err, ret, buf);
}

/** Register/install service in SCM */
static apt_bool_t uni_service_register(const char *root_dir_path, apr_pool_t *pool,
                                       const char *name,
                                       apt_bool_t autostart,
                                       unsigned long recover,
                                       int log_priority,
                                       const char *disp_name,
                                       const char *description)
{
	apr_status_t status;
	char buf[4096];
	static const apr_size_t len = sizeof(buf);
	apr_size_t pos = 0;
	char *root_dir;
	SERVICE_DESCRIPTION desc;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager;

	/* Normalize root directory path and make it absolute */
	status = apr_filepath_merge(&root_dir, NULL, root_dir_path,
		APR_FILEPATH_NOTRELATIVE | APR_FILEPATH_NATIVE | APR_FILEPATH_TRUENAME, pool);
	if (status != APR_SUCCESS) {
		printf("Error making root directory absolute: %d %.512s\n", status,
			apr_strerror(status, buf, 512));
		return FALSE;
	}
	buf[pos++] = '"';
	pos = apr_cpystrn(buf + pos, root_dir, len - pos) - buf;
	if ((buf[pos - 1] != '\\') && (pos < len))
		/* Add trailing backslash */
		buf[pos++] = '\\';
	pos = apr_cpystrn(buf + pos, "bin\\unimrcpserver.exe\" --service -o 2", len - pos) - buf;
	if (log_priority >= 0) {
		pos = apr_cpystrn(buf + pos, " -l ", len - pos) - buf;
		if (pos < len - 34)
			pos += strlen(itoa(log_priority, buf + pos, 10));
	}
	if (name) {
		pos = apr_cpystrn(buf + pos, " --name \"", len - pos) - buf;
		pos = apr_cpystrn(buf + pos, name, len - pos) - buf;
		if ((buf[pos - 1] == '\\') && (pos < len))
			/* `\"' might be misinterpreted as escape, so replace `\' with `\\' */
			buf[pos++] = '\\';
		if (pos < len)
			buf[pos++] = '"';
	}
	pos = apr_cpystrn(buf + pos, " --root-dir \"", len - pos) - buf;
	pos = apr_cpystrn(buf + pos, root_dir, len - pos) - buf;
	if ((buf[pos - 1] == '\\') && (pos < len))
		/* `\"' might be misinterpreted as escape, so replace `\' with `\\' */
		buf[pos++] = '\\';
	if (pos < len)
		buf[pos++] = '"';
	if (pos < len)
		buf[pos] = 0;
	else {
		puts("Service Command Too Long");
		return FALSE;
	}
	if (!disp_name || !*disp_name) {
		if (name)
			disp_name = apr_pstrcat(pool, name, " ", "UniMRCP Server", NULL);
		else
			disp_name = "UniMRCP Server";
	}
	if (!description || !*description)
		description = "Launches UniMRCP Server";

	sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}
	sch_service = CreateService(
					sch_manager,
					name ? name : WIN_SERVICE_NAME,
					disp_name,
					GENERIC_EXECUTE | SERVICE_CHANGE_CONFIG,
					SERVICE_WIN32_OWN_PROCESS,
					autostart ? SERVICE_AUTO_START : SERVICE_DEMAND_START,
					SERVICE_ERROR_NORMAL,
					buf,0,0,0,0,0);
	if(!sch_service) {
		winerror("Failed to Create Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	desc.lpDescription = (char *) description;
	if(!ChangeServiceConfig2(sch_service,SERVICE_CONFIG_DESCRIPTION,&desc)) {
		winerror("Failed to Set Service Description");
	}

	if (recover) {
		SERVICE_FAILURE_ACTIONS sfa;
		SC_ACTION action;
		sfa.dwResetPeriod = 0;
		sfa.lpCommand = "";
		sfa.lpRebootMsg = "";
		sfa.cActions = 1;
		sfa.lpsaActions = &action;
		action.Delay = recover * 1000;
		action.Type = SC_ACTION_RESTART;
		if (!ChangeServiceConfig2(sch_service,SERVICE_CONFIG_FAILURE_ACTIONS,&sfa)) {
			winerror("Failed to Set Service Restart on Failure");
		}
	}

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	printf("UniMRCP service %s registered\n", name ? name : WIN_SERVICE_NAME);
	return TRUE;
}

/** Unregister/uninstall service from SCM */
static apt_bool_t uni_service_unregister(const char *name)
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if (!name) name = WIN_SERVICE_NAME;
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,name,DELETE|SERVICE_STOP);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status);
	if(!DeleteService(sch_service)) {
		winerror("Failed to Delete Service");
		status = FALSE;
	} else
		printf("UniMRCP service %s unregistered\n", name);
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Start service */
static apt_bool_t uni_service_start(const char *name)
{
	apt_bool_t status = TRUE;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if (!name) name = WIN_SERVICE_NAME;
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,name,SERVICE_START);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!StartService(sch_service,0,NULL)) {
		winerror("Failed to Start Service");
		status = FALSE;
	} else
		printf("UniMRCP service %s started\n", name);
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Stop service */
static apt_bool_t uni_service_stop(const char *name)
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if (!name) name = WIN_SERVICE_NAME;
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,name,SERVICE_STOP);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status)) {
		winerror("Failed to Stop Service");
		status = FALSE;
	} else
		printf("UniMRCP service %s stopped\n", name);

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}


static void usage()
{
	static apt_bool_t written = FALSE;
	if (written) return;
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
		"   -n [--name] svcname     : Service name (default: unimrcp)\n"
		"\n"
		"   -a [--autostart]        : Start service after boot-up\n"
		"\n"
		"   -f [--fail-restart] n   : If crashed, restart after n secs\n"
		"\n"
		"   -l [--log-prio] priority: Set the log priority.\n"
		"                             (0-emergency, ..., 7-debug)\n"
		"   -p [--disp-name] title  : Set service display name\n"
		"                             (default: [svcname] UniMRCP Server)\n"
		"   -c [--description] desc : Set service description\n"
		"                             (default: Launches UniMRCP Server)\n"
		"   -h [--help]             : Show the help.\n"
		"\n");
	written = TRUE;
}

int main(int argc, const char * const *argv)
{
	apr_pool_t *pool;
	apr_status_t rv;
	apr_getopt_t *opt;
	apt_bool_t ret = TRUE;
	uni_service_register_e reg = USR_NONE;
	uni_service_control_e control = USC_NONE;
	const char *root_dir = "..";
	const char *name = NULL;
	apt_bool_t autostart = FALSE;
	unsigned long recover = 0;
	int log_priority = -1;
	const char *disp_name = NULL;
	const char *description = NULL;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "register",    'r', TRUE,  "register service" },   /* -r or --register arg */
		{ "unregister",  'u', FALSE, "unregister service" }, /* -u or --unregister */
		{ "start",       's', FALSE, "start service" },      /* -s or --start */
		{ "stop",        't', FALSE, "stop service" },       /* -t or --stop */
		{ "name",        'n', TRUE,  "service name" },       /* -n or --name arg */
		{ "autostart",   'a', FALSE, "start automatically" },/* -a or --autostart */
		{ "fail-restart",'f', TRUE,  "restart if fails" },   /* -f or --fail-restart arg */
		{ "log-prio",    'l', TRUE,  "log priority" },       /* -l arg or --log-prio arg */
		{ "disp-name",   'p', TRUE,  "display name" },       /* -p arg or --disp-name arg */
		{ "description", 'c', TRUE,  "description" },        /* -c arg or --description arg */
		{ "help",        'h', FALSE, "show help" },          /* -h or --help */
		{ NULL, 0, 0, NULL },                                /* end */
	};

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 1;
	}

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		apr_terminate();
		return 1;
	}

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv == APR_SUCCESS) {
		int optch;
		const char *optarg;
		while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
			switch(optch) {
				case 'r':
					if ((reg == USR_NONE) || (reg == USR_REGISTER)) {
						reg = USR_REGISTER;
						root_dir = optarg;
					} else {
						puts("Incosistent arguments");
						ret = FALSE;
					}
					break;
				case 'u':
					if ((reg == USR_NONE) || (reg == USR_UNREGISTER))
						reg = USR_UNREGISTER;
					else {
						puts("Incosistent arguments");
						ret = FALSE;
					}
					break;
				case 's':
					if ((control == USC_NONE) || (control == USC_START))
						control = USC_START;
					else {
						puts("Incosistent arguments");
						ret = FALSE;
					}
					break;
				case 't':
					if ((control == USC_NONE) || (control == USC_STOP))
						control = USC_STOP;
					else {
						puts("Incosistent arguments");
						ret = FALSE;
					}
					break;
				case 'n':
					name = optarg;
					break;
				case 'a':
					autostart = TRUE;
					break;
				case 'f':
					if (sscanf(optarg, "%lu", &recover) != 1) {
						puts("Invalid value for param --fail-restart");
						ret = FALSE;
					}
					break;
				case 'l':
					if ((sscanf(optarg, "%d", &log_priority) != 1) ||
						(log_priority < 0) || (log_priority > 7))
					{
						puts("Invalid value for param --log-prio");
						ret = FALSE;
					}
					break;
				case 'p':
					disp_name = optarg;
					break;
				case 'c':
					description = optarg;
					break;
				case 'h':
					usage();
					break;
			}
			if (!ret) break;
		}
		if (ret &&
				(((reg == USR_REGISTER) && (control == USC_STOP)) ||
				((reg == USR_UNREGISTER) && (control == USC_START)))) {
			ret = FALSE;
			puts("Inconsistent arguments");
		}
		if((rv != APR_EOF) || !ret || (!reg && !control)) {
			ret = FALSE;
			usage();
		}
	}

	while (ret) {  /* No problem so far */
		if (reg == USR_REGISTER)
			ret = uni_service_register(root_dir, pool, name, autostart, recover, log_priority, disp_name, description);
		if (!ret) break;

		if (control == USC_START)
			ret = uni_service_start(name);
		if (!ret) break;

		if (control == USC_STOP)
			ret = uni_service_stop(name);
		/* Do not break here, stop failure should not matter before unregistration */

		if (reg == USR_UNREGISTER)
			ret = uni_service_unregister(name);
		break;
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return ret ? 0 : 1;
}
