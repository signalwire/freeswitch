/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Pawel Pierscionek <pawel@voiceworks.pl>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 *
 * switch.c -- Main
 *
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifndef WIN32
#include <poll.h>
#ifdef HAVE_SETRLIMIT
#include <sys/resource.h>
#endif
#endif

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <switch.h>
#include "private/switch_core_pvt.h"

/* pid filename: Stores the process id of the freeswitch process */
#define PIDFILE "freeswitch.pid"
static char *pfile = PIDFILE;
static int system_ready = 0;

/* Picky compiler */
#ifdef __ICC
#pragma warning (disable:167)
#endif

#ifdef WIN32
/* If we are a windows service, what should we be called */
#define SERVICENAME_DEFAULT "FreeSWITCH"
#define SERVICENAME_MAXLEN 256
static char service_name[SERVICENAME_MAXLEN];
static switch_core_flag_t service_flags = SCF_NONE;
#include <winsock2.h>
#include <windows.h>

/* event to signal shutdown (for you unix people, this is like a pthread_cond) */
static HANDLE shutdown_event;

#ifndef PATH_MAX
#define PATH_MAX 256
#endif
#endif

/* signal handler for when freeswitch is running in background mode.
 * signal triggers the shutdown of freeswitch
# */
static void handle_SIGILL(int sig)
{
	int32_t arg = 0;
	if (sig) {};
	/* send shutdown signal to the freeswitch core */
	switch_core_session_ctl(SCSC_SHUTDOWN, &arg);
	return;
}

/* kill a freeswitch process running in background mode */
static int freeswitch_kill_background()
{
	FILE *f;					/* FILE handle to open the pid file */
	char path[PATH_MAX] = "";		/* full path of the PID file */
	pid_t pid = 0;				/* pid from the pid file */

	/* set the globals so we can use the global paths. */
	switch_core_set_globals();

	/* get the full path of the pid file. */
	switch_snprintf(path, sizeof(path), "%s%s%s", SWITCH_GLOBAL_dirs.run_dir, SWITCH_PATH_SEPARATOR, pfile);

	/* open the pid file */
	if ((f = fopen(path, "r")) == 0) {
		/* pid file does not exist */
		fprintf(stderr, "Cannot open pid file %s.\n", path);
		return 255;
	}

	/* pull the pid from the file */
	if (fscanf(f, "%d", (int *) (intptr_t) & pid) != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to get the pid!\n");
	}

	/* if we have a valid pid */
	if (pid > 0) {

		/* kill the freeswitch running at the pid we found */
		fprintf(stderr, "Killing: %d\n", (int) pid);
#ifdef WIN32
		/* for windows we need the event to signal for shutting down a background FreeSWITCH */
		snprintf(path, sizeof(path), "Global\\Freeswitch.%d", pid);

		/* open the event so we can signal it */
		shutdown_event = OpenEvent(EVENT_MODIFY_STATE, FALSE, path);

		/* did we successfully open the event */
		if (!shutdown_event) {
			/* we can't get the event, so we can't signal the process to shutdown */
			fprintf(stderr, "ERROR: Can't Shutdown: %d\n", (int) pid);
		} else {
			/* signal the event to shutdown */
			SetEvent(shutdown_event);
			/* cleanup */
			CloseHandle(shutdown_event);
		}
#else
		/* for unix, send the signal to kill. */
		kill(pid, SIGTERM);
#endif
	}

	/* be nice and close the file handle to the pid file */
	fclose(f);

	return 0;
}

#ifdef WIN32

/* we need these vars to handle the service */
SERVICE_STATUS_HANDLE hStatus;
SERVICE_STATUS status;

/* Handler function for service start/stop from the service */
void WINAPI ServiceCtrlHandler(DWORD control)
{
	switch (control) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		/* Shutdown freeswitch */
		switch_core_destroy();
		/* set service status values */
		status.dwCurrentState = SERVICE_STOPPED;
		status.dwWin32ExitCode = 0;
		status.dwCheckPoint = 0;
		status.dwWaitHint = 0;
		break;
	case SERVICE_CONTROL_INTERROGATE:
		/* we already set the service status every time it changes. */
		/* if there are other times we change it and don't update, we should do so here */
		break;
	}

	SetServiceStatus(hStatus, &status);
}

/* the main service entry point */
void WINAPI service_main(DWORD numArgs, char **args)
{
	switch_core_flag_t flags = SCF_USE_SQL | SCF_USE_AUTO_NAT | SCF_USE_NAT_MAPPING | SCF_CALIBRATE_CLOCK | SCF_USE_CLOCK_RT;
	const char *err = NULL;		/* error value for return from freeswitch initialization */

	/* Override flags if they have been set earlier */
	if (service_flags != SCF_NONE)
		flags = service_flags;

	/*  we have to initialize the service-specific stuff */
	memset(&status, 0, sizeof(SERVICE_STATUS));
	status.dwServiceType = SERVICE_WIN32;
	status.dwCurrentState = SERVICE_START_PENDING;
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	/* register our handler for service control messages */
	hStatus = RegisterServiceCtrlHandler(service_name, &ServiceCtrlHandler);

	/* update the service status */
	SetServiceStatus(hStatus, &status);

	switch_core_set_globals();

	/* attempt to initialize freeswitch and load modules */
	if (switch_core_init_and_modload(flags, SWITCH_FALSE, &err) != SWITCH_STATUS_SUCCESS) {
		/* freeswitch did not start successfully */
		status.dwCurrentState = SERVICE_STOPPED;
	} else {
		/* freeswitch started */
		status.dwCurrentState = SERVICE_RUNNING;
	}

	/* update the service status */
	SetServiceStatus(hStatus, &status);
}

#else

static int check_fd(int fd, int ms)
{
	struct pollfd pfds[2] = { { 0 } };
	int s, r = 0, i = 0;

	pfds[0].fd = fd;
	pfds[0].events = POLLIN | POLLERR;
	s = poll(pfds, 1, ms);

	if (s == 0 || s == -1) {
		r = s;
	} else {
		r = -1;

		if ((pfds[0].revents & POLLIN)) {
			if ((i = read(fd, &r, sizeof(r))) > -1) {
				(void)write(fd, &r, sizeof(r));
			}
		}
	}
	
	return r;
}

static void daemonize(int *fds)
{
	int fd;
	pid_t pid;
	unsigned int sanity = 60;

	if (!fds) {
		switch (fork()) {
		case 0:		/* child process */
			break;
		case -1:
			fprintf(stderr, "Error Backgrounding (fork)! %d - %s\n", errno, strerror(errno));
			exit(EXIT_SUCCESS);
			break;
		default:	/* parent process */
			exit(EXIT_SUCCESS);
		}

		if (setsid() < 0) {
			fprintf(stderr, "Error Backgrounding (setsid)! %d - %s\n", errno, strerror(errno));
			exit(EXIT_SUCCESS);
		}
	}

	pid = switch_fork();

	switch (pid) {
	case 0:		/* child process */
		if (fds) {
			close(fds[0]);
		}
		break;
	case -1:
		fprintf(stderr, "Error Backgrounding (fork2)! %d - %s\n", errno, strerror(errno));
		exit(EXIT_SUCCESS);
		break;
	default:	/* parent process */
		fprintf(stderr, "%d Backgrounding.\n", (int) pid);

		if (fds) {
			char *o;

			close(fds[1]);

			if ((o = getenv("FREESWITCH_BG_TIMEOUT"))) {
				int tmp = atoi(o);
				if (tmp > 0) {
					sanity = tmp;
				}
			}

			do {
				system_ready = check_fd(fds[0], 2000);

				if (system_ready == 0) {
					printf("FreeSWITCH[%d] Waiting for background process pid:%d to be ready.....\n", (int)getpid(), (int) pid);
				}

			} while (--sanity && system_ready == 0);

			shutdown(fds[0], 2);
			close(fds[0]);
			fds[0] = -1;

			
			if (system_ready < 0) {
				printf("FreeSWITCH[%d] Error starting system! pid:%d\n", (int)getpid(), (int) pid);
				kill(pid, 9);
				exit(EXIT_FAILURE);
			}

			printf("FreeSWITCH[%d] System Ready pid:%d\n", (int) getpid(), (int) pid);
		}

		exit(EXIT_SUCCESS);
	}

	if (fds) {
		setsid();
	}
	/* redirect std* to null */
	fd = open("/dev/null", O_RDONLY);
	switch_assert( fd >= 0 );
	if (fd != 0) {
		dup2(fd, 0);
		close(fd);
	}

	fd = open("/dev/null", O_WRONLY);
	switch_assert( fd >= 0 );
	if (fd != 1) {
		dup2(fd, 1);
		close(fd);
	}

	fd = open("/dev/null", O_WRONLY);
	switch_assert( fd >= 0 );
	if (fd != 2) {
		dup2(fd, 2);
		close(fd);
	}
	return;
}

static pid_t reincarnate_child = 0;
static void reincarnate_handle_sigterm (int sig) {
	if (!sig) return;
	if (reincarnate_child) kill(reincarnate_child, sig);
	return;
}

static void reincarnate_protect(char **argv) {
	int i; struct sigaction sa, sa_dfl, sa4_prev, sa15_prev, sa17_prev;
	memset(&sa, 0, sizeof(sa)); memset(&sa_dfl, 0, sizeof(sa_dfl));
	sa.sa_handler = reincarnate_handle_sigterm;
	sa_dfl.sa_handler = SIG_DFL;
 refork:
	if ((i=fork())) { /* parent */
		int s; pid_t r;
		reincarnate_child = i;
		sigaction(SIGILL, &sa, &sa4_prev);
		sigaction(SIGTERM, &sa, &sa15_prev);
		sigaction(SIGCHLD, &sa_dfl, &sa17_prev);
	rewait:
		r = waitpid(i, &s, 0);
		if (r == (pid_t)-1) {
			if (errno == EINTR) goto rewait;
			exit(EXIT_FAILURE);
		}
		if (r != i) goto rewait;
		if (WIFEXITED(s)
			&& (WEXITSTATUS(s) == EXIT_SUCCESS
				|| WEXITSTATUS(s) == EXIT_FAILURE)) {
			exit(WEXITSTATUS(s));
		}
		if (WIFEXITED(s) || WIFSIGNALED(s)) {
			sigaction(SIGILL, &sa4_prev, NULL);
			sigaction(SIGTERM, &sa15_prev, NULL);
			sigaction(SIGCHLD, &sa17_prev, NULL);
			if (argv) {
				if (execv(argv[0], argv) == -1) {
					char buf[256];
					fprintf(stderr, "Reincarnate execv() failed: %d %s\n", errno,
							switch_strerror_r(errno, buf, sizeof(buf)));
				}
				fprintf(stderr, "Trying reincarnate-reexec plan B...\n");
				if (execvp(argv[0], argv) == -1) {
					char buf[256];
					fprintf(stderr, "Reincarnate execvp() failed: %d %s\n", errno,
							switch_strerror_r(errno, buf, sizeof(buf)));
				}
				fprintf(stderr, "Falling back to normal reincarnate behavior...\n");
				goto refork;
			} else goto refork;
		}
		goto rewait;
	} else { /* child */
#ifdef __linux__
		prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
	}
}

#endif

static const char usage[] =
	"Usage: freeswitch [OPTIONS]\n\n"
	"These are the optional arguments you can pass to freeswitch:\n"
#ifdef WIN32
	"\t-service [name]        -- start freeswitch as a service, cannot be used if loaded as a console app\n"
	"\t-install [name]        -- install freeswitch as a service, with optional service name\n"
	"\t-uninstall             -- remove freeswitch as a service\n"
	"\t-monotonic-clock       -- use monotonic clock as timer source\n"
#else
	"\t-nf                    -- no forking\n"
	"\t-reincarnate           -- restart the switch on an uncontrolled exit\n"
	"\t-reincarnate-reexec    -- run execv on a restart (helpful for upgrades)\n"
	"\t-u [user]              -- specify user to switch to\n"
	"\t-g [group]             -- specify group to switch to\n"
#endif
#ifdef HAVE_SETRLIMIT
#ifndef FS_64BIT
	"\t-waste                 -- allow memory waste\n"
#endif
	"\t-core                  -- dump cores\n"
#endif
	"\t-help                  -- this message\n"
	"\t-version               -- print the version and exit\n"
	"\t-rp                    -- enable high(realtime) priority settings\n"
	"\t-lp                    -- enable low priority settings\n"
	"\t-np                    -- enable normal priority settings\n"
	"\t-vg                    -- run under valgrind\n"
	"\t-nosql                 -- disable internal sql scoreboard\n"
	"\t-heavy-timer           -- Heavy Timer, possibly more accurate but at a cost\n"
	"\t-nonat                 -- disable auto nat detection\n"
	"\t-nonatmap              -- disable auto nat port mapping\n"
	"\t-nocal                 -- disable clock calibration\n"
	"\t-nort                  -- disable clock clock_realtime\n"
	"\t-stop                  -- stop freeswitch\n"
	"\t-nc                    -- do not output to a console and background\n"
#ifndef WIN32
	"\t-ncwait                -- do not output to a console and background but wait until the system is ready before exiting (implies -nc)\n"
#endif
	"\t-c                     -- output to a console and stay in the foreground\n"
	"\n\tOptions to control locations of files:\n"
	"\t-base [basedir]         -- alternate prefix directory\n"
	"\t-cfgname [filename]     -- alternate filename for FreeSWITCH main configuration file\n"
	"\t-conf [confdir]         -- alternate directory for FreeSWITCH configuration files\n"
	"\t-log [logdir]           -- alternate directory for logfiles\n"
	"\t-run [rundir]           -- alternate directory for runtime files\n"
	"\t-db [dbdir]             -- alternate directory for the internal database\n"
	"\t-mod [moddir]           -- alternate directory for modules\n"
	"\t-htdocs [htdocsdir]     -- alternate directory for htdocs\n"
	"\t-scripts [scriptsdir]   -- alternate directory for scripts\n"
	"\t-temp [directory]       -- alternate directory for temporary files\n"
	"\t-grammar [directory]    -- alternate directory for grammar files\n"
	"\t-certs [directory]      -- alternate directory for certificates\n"
	"\t-recordings [directory] -- alternate directory for recordings\n"
	"\t-storage [directory]    -- alternate directory for voicemail storage\n"
	"\t-cache [directory]      -- alternate directory for cache files\n"
	"\t-sounds [directory]     -- alternate directory for sound files\n";


/**
 * Check if value string starts with "-"
 */
static switch_bool_t is_option(const char *p)
{
	/* skip whitespaces */
	while ((*p == 13) || (*p == 10) || (*p == 9) || (*p == 32) || (*p == 11)) p++;
	return (p[0] == '-');
}


/* the main application entry point */
int main(int argc, char *argv[])
{
	char pid_path[PATH_MAX] = "";	/* full path to the pid file */
	char pid_buffer[32] = "";	/* pid string */
	char old_pid_buffer[32] = "";	/* pid string */
	switch_size_t pid_len, old_pid_len;
	const char *err = NULL;		/* error value for return from freeswitch initialization */
#ifndef WIN32
	switch_bool_t nf = SWITCH_FALSE;				/* TRUE if we are running in nofork mode */
	switch_bool_t do_wait = SWITCH_FALSE;
	char *runas_user = NULL;
	char *runas_group = NULL;
	switch_bool_t reincarnate = SWITCH_FALSE, reincarnate_reexec = SWITCH_FALSE;
	int fds[2] = { 0, 0 };
#else
	switch_bool_t win32_service = SWITCH_FALSE;
#endif
	switch_bool_t nc = SWITCH_FALSE;				/* TRUE if we are running in noconsole mode */
	pid_t pid = 0;
	int i, x;
	char *opts;
	char opts_str[1024] = "";
	char *local_argv[1024] = { 0 };
	int local_argc = argc;
	char *arg_argv[128] = { 0 };
	int alt_dirs = 0, alt_base = 0, log_set = 0, run_set = 0, do_kill = 0;
	int priority = 0;
#ifdef __sun
	switch_core_flag_t flags = SCF_USE_SQL;
#else
	switch_core_flag_t flags = SCF_USE_SQL | SCF_USE_AUTO_NAT | SCF_USE_NAT_MAPPING | SCF_CALIBRATE_CLOCK | SCF_USE_CLOCK_RT;
#endif
	int ret = 0;
	switch_status_t destroy_status;
	switch_file_t *fd;
	switch_memory_pool_t *pool = NULL;
#ifdef HAVE_SETRLIMIT
#ifndef FS_64BIT
	switch_bool_t waste = SWITCH_FALSE;
#endif
#endif

	for (x = 0; x < argc; x++) {
		local_argv[x] = argv[x];
	}

	if ((opts = getenv("FREESWITCH_OPTS"))) {
		strncpy(opts_str, opts, sizeof(opts_str) - 1);
		i = switch_separate_string(opts_str, ' ', arg_argv, (sizeof(arg_argv) / sizeof(arg_argv[0])));
		for (x = 0; x < i; x++) {
			local_argv[local_argc++] = arg_argv[x];
		}
	}

	if (local_argv[0] && strstr(local_argv[0], "freeswitchd")) {
		nc = SWITCH_TRUE;
	}

	for (x = 1; x < local_argc; x++) {

		if (switch_strlen_zero(local_argv[x]))
			continue;

		if (!strcmp(local_argv[x], "-help") || !strcmp(local_argv[x], "-h") || !strcmp(local_argv[x], "-?")) {
			printf("%s\n", usage);
			exit(EXIT_SUCCESS);
		}
#ifdef WIN32
		if (x == 1 && !strcmp(local_argv[x], "-service")) {
			/* New installs will always have the service name specified, but keep a default for compat */
			x++;
			if (!switch_strlen_zero(local_argv[x])) {
				switch_copy_string(service_name, local_argv[x], SERVICENAME_MAXLEN);
			} else {
				switch_copy_string(service_name, SERVICENAME_DEFAULT, SERVICENAME_MAXLEN);
			}

			win32_service = SWITCH_TRUE;
			continue;
		}

		else if (x == 1 && !strcmp(local_argv[x], "-install")) {
			char servicePath[PATH_MAX];
			char exePath[PATH_MAX];
			SC_HANDLE hService;
			SC_HANDLE hSCManager;
			SERVICE_DESCRIPTION desc;
			desc.lpDescription = "The FreeSWITCH service.";

			x++;
			if (!switch_strlen_zero(local_argv[x])) {
				switch_copy_string(service_name, local_argv[x], SERVICENAME_MAXLEN);
			} else {
				switch_copy_string(service_name, SERVICENAME_DEFAULT, SERVICENAME_MAXLEN);
			}

			GetModuleFileName(NULL, exePath, sizeof(exePath));
			snprintf(servicePath, sizeof(servicePath), "%s -service %s", exePath, service_name);

			/* Perform service installation */

			hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!hSCManager) {
				fprintf(stderr, "Could not open service manager (%u).\n", GetLastError());
				exit(EXIT_FAILURE);
			}

			hService = CreateService(hSCManager, service_name, service_name, GENERIC_READ | GENERIC_EXECUTE | SERVICE_CHANGE_CONFIG, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_IGNORE,
						 servicePath, NULL, NULL, NULL, NULL, /* Service start name */ NULL);
			if (!hService) {
				fprintf(stderr, "Error creating freeswitch service (%u).\n", GetLastError());
				CloseServiceHandle(hSCManager);
				exit(EXIT_FAILURE);
			}

			/* Set desc, and don't care if it succeeds */
			if (!ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &desc)) {
				fprintf(stderr, "FreeSWITCH installed, but could not set the service description (%u).\n", GetLastError());
			}

			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			exit(EXIT_SUCCESS);
		}

		else if (x == 1 && !strcmp(local_argv[x], "-uninstall")) {
			SC_HANDLE hService;
			SC_HANDLE hSCManager;
			BOOL deleted;

			x++;
			if (!switch_strlen_zero(local_argv[x])) {
				switch_copy_string(service_name, local_argv[x], SERVICENAME_MAXLEN);
			} else {
				switch_copy_string(service_name, SERVICENAME_DEFAULT, SERVICENAME_MAXLEN);
			}

			/* Do the uninstallation */
			hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!hSCManager) {
				fprintf(stderr, "Could not open service manager (%u).\n", GetLastError());
				exit(EXIT_FAILURE);
			}

			hService = OpenService(hSCManager, service_name, DELETE);
			if (!hService) {
				fprintf(stderr, "Error opening service (%u).\n", GetLastError());
				CloseServiceHandle(hSCManager);
				exit(EXIT_FAILURE);
			}

			/* remove the service! */
			deleted = DeleteService(hService);
			if (!deleted) {
				fprintf(stderr, "Error deleting service (%u).\n", GetLastError());
			}

			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			exit(deleted ? EXIT_SUCCESS : EXIT_FAILURE);
		}

		else if (!strcmp(local_argv[x], "-monotonic-clock")) {
			flags |= SCF_USE_WIN32_MONOTONIC;
		}
#else
		else if (!strcmp(local_argv[x], "-u")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "Option '%s' requires an argument!\n", local_argv[x - 1]);
				exit(EXIT_FAILURE);
			}
			runas_user = local_argv[x];
		}

		else if (!strcmp(local_argv[x], "-g")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "Option '%s' requires an argument!\n", local_argv[x - 1]);
				exit(EXIT_FAILURE);
			}
			runas_group = local_argv[x];
		}

		else if (!strcmp(local_argv[x], "-nf")) {
			nf = SWITCH_TRUE;
		}

		else if (!strcmp(local_argv[x], "-reincarnate")) {
			reincarnate = SWITCH_TRUE;
		}
		else if (!strcmp(local_argv[x], "-reincarnate-reexec")) {
			reincarnate = SWITCH_TRUE;
			reincarnate_reexec = SWITCH_TRUE;
		}

		else if (!strcmp(local_argv[x], "-version")) {
			fprintf(stdout, "FreeSWITCH version: %s (%s)\n", switch_version_full(), switch_version_revision_human());
			exit(EXIT_SUCCESS);
		}
#endif
#ifdef HAVE_SETRLIMIT
		else if (!strcmp(local_argv[x], "-core")) {
			struct rlimit rlp;
			memset(&rlp, 0, sizeof(rlp));
			rlp.rlim_cur = RLIM_INFINITY;
			rlp.rlim_max = RLIM_INFINITY;
			setrlimit(RLIMIT_CORE, &rlp);
		}

		else if (!strcmp(local_argv[x], "-waste")) {
#ifndef FS_64BIT
			fprintf(stderr, "WARNING: Wasting up to 8 megs of memory per thread.\n");
			sleep(2);
			waste = SWITCH_TRUE;
#endif
		}

		else if (!strcmp(local_argv[x], "-no-auto-stack")) {
#ifndef FS_64BIT
			waste = SWITCH_TRUE;
#endif
		}
#endif
		else if (!strcmp(local_argv[x], "-hp") || !strcmp(local_argv[x], "-rp")) {
			priority = 2;
		}

		else if (!strcmp(local_argv[x], "-lp")) {
			priority = -1;
		}

		else if (!strcmp(local_argv[x], "-np")) {
			priority = 1;
		}

		else if (!strcmp(local_argv[x], "-nosql")) {
			flags &= ~SCF_USE_SQL;
		}

		else if (!strcmp(local_argv[x], "-nonat")) {
			flags &= ~SCF_USE_AUTO_NAT;
		}

		else if (!strcmp(local_argv[x], "-nonatmap")) {
			flags &= ~SCF_USE_NAT_MAPPING;
		}

		else if (!strcmp(local_argv[x], "-heavy-timer")) {
			flags |= SCF_USE_HEAVY_TIMING;
		}

		else if (!strcmp(local_argv[x], "-nort")) {
			flags &= ~SCF_USE_CLOCK_RT;
		}

		else if (!strcmp(local_argv[x], "-nocal")) {
			flags &= ~SCF_CALIBRATE_CLOCK;
		}

		else if (!strcmp(local_argv[x], "-vg")) {
			flags |= SCF_VG;
		}

		else if (!strcmp(local_argv[x], "-stop")) {
			do_kill = SWITCH_TRUE;
		}

		else if (!strcmp(local_argv[x], "-nc")) {
			nc = SWITCH_TRUE;
		}
#ifndef WIN32
		else if (!strcmp(local_argv[x], "-ncwait")) {
			nc = SWITCH_TRUE;
			do_wait = SWITCH_TRUE;
		}
#endif
		else if (!strcmp(local_argv[x], "-c")) {
			nc = SWITCH_FALSE;
		}

		else if (!strcmp(local_argv[x], "-conf")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -conf you must specify a config directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.conf_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.conf_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.conf_dir, local_argv[x]);
			alt_dirs++;
		}

		else if (!strcmp(local_argv[x], "-mod")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -mod you must specify a module directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.mod_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.mod_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.mod_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-log")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -log you must specify a log directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.log_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.log_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.log_dir, local_argv[x]);
			alt_dirs++;
			log_set = SWITCH_TRUE;
		}

		else if (!strcmp(local_argv[x], "-run")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -run you must specify a pid directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.run_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.run_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.run_dir, local_argv[x]);
			run_set = SWITCH_TRUE;
		}

		else if (!strcmp(local_argv[x], "-db")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -db you must specify a db directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.db_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.db_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.db_dir, local_argv[x]);
			alt_dirs++;
		}

		else if (!strcmp(local_argv[x], "-scripts")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -scripts you must specify a scripts directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.script_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.script_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.script_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-htdocs")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -htdocs you must specify a htdocs directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.htdocs_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.htdocs_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.htdocs_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-base")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -base you must specify a base directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.base_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.base_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.base_dir, local_argv[x]);
			alt_base = 1;
		}

		else if (!strcmp(local_argv[x], "-temp")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -temp you must specify a temp directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.temp_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.temp_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.temp_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-storage")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -storage you must specify a storage directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.storage_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.storage_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.storage_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-cache")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -cache you must specify a cache directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.cache_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.cache_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.cache_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-recordings")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -recordings you must specify a recording directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.recordings_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.recordings_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.recordings_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-grammar")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -grammar you must specify a grammar directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.grammar_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.grammar_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.grammar_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-certs")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -certs you must specify a certificates directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.certs_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.certs_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.certs_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-sounds")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -sounds you must specify a sounds directory\n");
				return 255;
			}

			SWITCH_GLOBAL_dirs.sounds_dir = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_dirs.sounds_dir) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_dirs.sounds_dir, local_argv[x]);
		}

		else if (!strcmp(local_argv[x], "-cfgname")) {
			x++;
			if (switch_strlen_zero(local_argv[x]) || is_option(local_argv[x])) {
				fprintf(stderr, "When using -cfgname you must specify a filename\n");
				return 255;
			}

			SWITCH_GLOBAL_filenames.conf_name = (char *) malloc(strlen(local_argv[x]) + 1);
			if (!SWITCH_GLOBAL_filenames.conf_name) {
				fprintf(stderr, "Allocation error\n");
				return 255;
			}
			strcpy(SWITCH_GLOBAL_filenames.conf_name, local_argv[x]);
		}

		/* Unknown option (always last!) */
		else {
			fprintf(stderr, "Unknown option '%s', see '%s -help' for a list of valid options\n",
				local_argv[x], local_argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (log_set && !run_set) {
		SWITCH_GLOBAL_dirs.run_dir = (char *) malloc(strlen(SWITCH_GLOBAL_dirs.log_dir) + 1);
		if (!SWITCH_GLOBAL_dirs.run_dir) {
			fprintf(stderr, "Allocation error\n");
			return 255;
		}
		strcpy(SWITCH_GLOBAL_dirs.run_dir, SWITCH_GLOBAL_dirs.log_dir);
	}

	if (do_kill) {
		return freeswitch_kill_background();
	}

	if (apr_initialize() != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "FATAL ERROR! Could not initialize APR\n");
		return 255;
	}

	if (alt_dirs && alt_dirs != 3 && !alt_base) {
		fprintf(stderr, "You must specify all or none of -conf, -log, and -db\n");
		return 255;
	}

#ifndef FS_64BIT
#if defined(HAVE_SETRLIMIT) && !defined(__sun)
	if (!waste && !(flags & SCF_VG)) {
		struct rlimit rlp;

		memset(&rlp, 0, sizeof(rlp));
		getrlimit(RLIMIT_STACK, &rlp);

		if (rlp.rlim_cur != SWITCH_THREAD_STACKSIZE) {
			char buf[1024] = "";
			int i = 0;

			memset(&rlp, 0, sizeof(rlp));
			rlp.rlim_cur = SWITCH_THREAD_STACKSIZE;
			rlp.rlim_max = SWITCH_SYSTEM_THREAD_STACKSIZE;
			setrlimit(RLIMIT_STACK, &rlp);

			apr_terminate();
			if (argv) ret = (int) execv(argv[0], argv);

			for (i = 0; i < argc; i++) {
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s ", argv[i]);
			}

			return system(buf);
		}
	}
#endif
#endif
	signal(SIGILL, handle_SIGILL);
	signal(SIGTERM, handle_SIGILL);
#ifndef WIN32
	if (do_wait) {
		if (pipe(fds)) {
			fprintf(stderr, "System Error!\n");
			exit(-1);
		}
	}
#endif

	if (nc) {
#ifdef WIN32
		FreeConsole();
#else
		if (!nf) {
			daemonize(do_wait ? fds : NULL);
		}
#endif
	}
#ifndef WIN32
	if (reincarnate)
		reincarnate_protect(reincarnate_reexec ? argv : NULL);
#endif

	switch (priority) {
	case 2:
		set_realtime_priority();
		break;
	case 1:
		set_normal_priority();
		break;
	case -1:
		set_low_priority();
		break;
	default:
		set_auto_priority();
		break;
	}
	
	switch_core_setrlimits();


#ifndef WIN32
	if (runas_user || runas_group) {
		if (change_user_group(runas_user, runas_group) < 0) {
			fprintf(stderr, "Failed to switch user [%s] / group [%s]\n",
				switch_strlen_zero(runas_user)  ? "-" : runas_user,
				switch_strlen_zero(runas_group) ? "-" : runas_group);
			return 255;
		}
	}
#else
	if (win32_service) {
		/* Attempt to start service */
		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{service_name, &service_main}
			,
			{NULL, NULL}
		};
		service_flags = flags; /* copy parsed flags for service startup */

		if (StartServiceCtrlDispatcher(dispatchTable) == 0) {
			/* Not loaded as a service */
			fprintf(stderr, "Error Freeswitch loaded as a console app with -service option\n");
			fprintf(stderr, "To install the service load freeswitch with -install\n");
		}
		exit(EXIT_SUCCESS);
	}
#endif

	switch_core_set_globals();

	pid = getpid();

	memset(pid_buffer, 0, sizeof(pid_buffer));
	switch_snprintf(pid_path, sizeof(pid_path), "%s%s%s", SWITCH_GLOBAL_dirs.run_dir, SWITCH_PATH_SEPARATOR, pfile);
	switch_snprintf(pid_buffer, sizeof(pid_buffer), "%d", pid);
	pid_len = strlen(pid_buffer);

	apr_pool_create(&pool, NULL);

	switch_dir_make_recursive(SWITCH_GLOBAL_dirs.run_dir, SWITCH_DEFAULT_DIR_PERMS, pool);

	if (switch_file_open(&fd, pid_path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, pool) == SWITCH_STATUS_SUCCESS) {

		old_pid_len = sizeof(old_pid_buffer) -1;
		switch_file_read(fd, old_pid_buffer, &old_pid_len);
		switch_file_close(fd);
	}

	if (switch_file_open(&fd,
						 pid_path,
						 SWITCH_FOPEN_WRITE | SWITCH_FOPEN_CREATE | SWITCH_FOPEN_TRUNCATE,
						 SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, pool) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot open pid file %s.\n", pid_path);
		return 255;
	}

	if (switch_file_lock(fd, SWITCH_FLOCK_EXCLUSIVE | SWITCH_FLOCK_NONBLOCK) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot lock pid file %s.\n", pid_path);
		old_pid_len = strlen(old_pid_buffer);
		if (strlen(old_pid_buffer)) {
			switch_file_write(fd, old_pid_buffer, &old_pid_len);
		}
		return 255;
	}

	switch_file_write(fd, pid_buffer, &pid_len);

	if (switch_core_init_and_modload(flags, nc ? SWITCH_FALSE : SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot Initialize [%s]\n", err);
		return 255;
	}

#ifndef WIN32
	if (do_wait) {
		if (fds[1] > -1) {
			int i, v = 1;

			if ((i = write(fds[1], &v, sizeof(v))) < 0) {
				fprintf(stderr, "System Error [%s]\n", strerror(errno));
			} else {
				(void)read(fds[1], &v, sizeof(v));
			}
		
			shutdown(fds[1], 2);
			close(fds[1]);
			fds[1] = -1;
		}
	}
#endif

	switch_core_runtime_loop(nc);

	destroy_status = switch_core_destroy();

	switch_file_close(fd);
	apr_pool_destroy(pool);

	if (unlink(pid_path) != 0) {
		fprintf(stderr, "Failed to delete pid file [%s]\n", pid_path);
	}

	if (destroy_status == SWITCH_STATUS_RESTART) {
		char buf[1024] = "";
		int j = 0;

		switch_sleep(1000000);
		if (!argv || execv(argv[0], argv) == -1) {
			fprintf(stderr, "Restart Failed [%s] resorting to plan b\n", strerror(errno));
			for (j = 0; j < argc; j++) {
				switch_snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s ", argv[j]);
			}
			ret = system(buf);
		}
	}

	return ret;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
