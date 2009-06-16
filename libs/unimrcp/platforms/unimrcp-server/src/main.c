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

#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include "apt_pool.h"
#include "apt_dir_layout.h"
#include "apt_log.h"

typedef struct {
	const char        *root_dir_path;
	apt_bool_t         foreground;
	apt_log_priority_e log_priority;
	apt_log_output_e   log_output;
} server_options_t;

#ifdef WIN32
apt_bool_t uni_service_run(apt_dir_layout_t *dir_layout, apr_pool_t *pool);
#else
apt_bool_t uni_daemon_run(apt_dir_layout_t *dir_layout, apr_pool_t *pool);
#endif

apt_bool_t uni_cmdline_run(apt_dir_layout_t *dir_layout, apr_pool_t *pool);


static void usage()
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  unimrcpserver [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -r [--root-dir] path     : Set the project root directory path.\n"
		"\n"
		"   -l [--log-prio] priority : Set the log priority.\n"
		"                              (0-emergency, ..., 7-debug)\n"
		"\n"
		"   -o [--log-output] mode   : Set the log output mode.\n"
		"                              (0-none, 1-console only, 2-file only, 3-both)\n"
		"\n"
#ifdef WIN32
		"   -s [--service]           : Run as the Windows service.\n"
		"\n"
#else
		"   -d [--daemon]            : Run as the daemon.\n"
		"\n"
#endif
		"   -h [--help]              : Show the help.\n"
		"\n");
}

static apt_bool_t options_load(server_options_t *options, int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t *opt = NULL;
	int optch;
	const char *optarg;

	const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },  /* -r arg or --root-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
#ifdef WIN32
		{ "service",     's', FALSE, "run as service" },    /* -s or --service */
#else
		{ "daemon",      'd', FALSE, "start as daemon" },   /* -d or --daemon */
#endif
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		return FALSE;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'r':
				options->root_dir_path = optarg;
				break;
			case 'l':
				if(optarg) {
					options->log_priority = atoi(optarg);
				}
				break;
			case 'o':
				if(optarg) {
					options->log_output = atoi(optarg);
				}
				break;
#ifdef WIN32
			case 's':
				options->foreground = FALSE;
				break;
#else
			case 'd':
				options->foreground = FALSE;
				break;
#endif
			case 'h':
				usage();
				return FALSE;
		}
	}

	if(rv != APR_EOF) {
		usage();
		return FALSE;
	}

	return TRUE;
}

int main(int argc, const char * const *argv)
{
	apr_pool_t *pool = NULL;
	server_options_t options;
	apt_dir_layout_t *dir_layout;

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

	/* set the default options */
	options.root_dir_path = "../";
	options.foreground = TRUE;
	options.log_priority = APT_PRIO_INFO;
	options.log_output = APT_LOG_OUTPUT_CONSOLE;

	/* load options */
	if(options_load(&options,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* create the structure of default directories layout */
	dir_layout = apt_default_dir_layout_create(options.root_dir_path,pool);
	/* create singleton logger */
	apt_log_instance_create(options.log_output,options.log_priority,pool);

	if((options.log_output & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE) {
		/* open the log file */
		apt_log_file_open(dir_layout->log_dir_path,"unimrcpserver",MAX_LOG_FILE_SIZE,MAX_LOG_FILE_COUNT,pool);
	}

	if(options.foreground == TRUE) {
		/* run command line */
		uni_cmdline_run(dir_layout,pool);
	}
#ifdef WIN32
	else {
		/* run as windows service */
		uni_service_run(dir_layout,pool);
	}
#else
	else {
		/* run as daemon */
		uni_daemon_run(dir_layout,pool);
	}
#endif

	/* destroy singleton logger */
	apt_log_instance_destroy();
	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return 0;
}
