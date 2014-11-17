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
 * $Id: main.c 2204 2014-10-31 01:01:42Z achaloyan@gmail.com $
 */

#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include "demo_framework.h"
#include "apt_pool.h"
#include "apt_log.h"
#include "uni_version.h"

typedef struct {
	const char   *root_dir_path;
	const char   *dir_layout_conf;
	const char   *log_priority;
	const char   *log_output;
} client_options_t;

static apt_bool_t demo_framework_cmdline_process(demo_framework_t *framework, char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);

	if(strcasecmp(name,"run") == 0) {
		char *app_name = apr_strtok(NULL, " ", &last);
		if(app_name) {
			char *profile_name = apr_strtok(NULL, " ", &last);
			if(!profile_name) {
				profile_name = "uni2";
			}
			demo_framework_app_run(framework,app_name,profile_name);
		}
	}
	else if(strcasecmp(name,"loglevel") == 0) {
		char *priority = apr_strtok(NULL, " ", &last);
		if(priority) {
			apt_log_priority_set(atol(priority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcasecmp(name,"help") == 0) {
		printf("usage:\n"
		       "\n- run [app_name] [profile_name] (run demo application)\n"
			   "       app_name is one of 'synth', 'recog', 'bypass', 'discover'\n"
			   "       profile_name is one of 'uni2', 'uni1', ...\n"
			   "\n       examples: \n"
			   "           run synth\n"
			   "           run recog\n"
			   "           run synth uni1\n"
			   "           run recog uni1\n"
		       "\n- loglevel [level] (set loglevel, one of 0,1...7)\n"
		       "\n- quit, exit\n");
	}
	else {
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

static apt_bool_t demo_framework_cmdline_run(demo_framework_t *framework)
{
	apt_bool_t running = TRUE;
	char cmdline[1024];
	apr_size_t i;
	do {
		printf(">");
		memset(&cmdline, 0, sizeof(cmdline));
		for(i = 0; i < sizeof(cmdline); i++) {
			cmdline[i] = (char) getchar();
			if(cmdline[i] == '\n') {
				cmdline[i] = '\0';
				break;
			}
		}
		if(*cmdline) {
			running = demo_framework_cmdline_process(framework,cmdline);
		}
	}
	while(running != 0);
	return TRUE;
}

static void usage(void)
{
	printf(
		"\n"
		" * "UNI_COPYRIGHT"\n"
		" *\n"
		UNI_LICENSE"\n"
		"\n"
		"Usage:\n"
		"\n"
		"  unimrcpclient [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -r [--root-dir] path     : Set the path to the project root directory.\n"
		"\n"
		"   -c [--dir-layout] path   : Set the path to the dir layout config file.\n"
		"                              (takes the precedence over --root-dir option)\n"
		"\n"
		"   -l [--log-prio] priority : Set the log priority.\n"
		"                              (0-emergency, ..., 7-debug)\n"
		"\n"
		"   -o [--log-output] mode   : Set the log output mode.\n"
		"                              (0-none, 1-console only, 2-file only, 3-both)\n"
		"\n"
		"   -v [--version]           : Show the version.\n"
		"\n"
		"   -h [--help]              : Show the help.\n"
		"\n");
}

static apt_bool_t demo_framework_options_load(client_options_t *options, int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t *opt = NULL;
	int optch;
	const char *optarg;

	const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },         /* -r arg or --root-dir arg */
		{ "dir-layout",  'c', TRUE,  "path to dir layout conf" },  /* -c arg or --dir-layout arg */
		{ "log-prio",    'l', TRUE,  "log priority" },             /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },          /* -o arg or --log-output arg */
		{ "version",     'v', FALSE, "show version" },             /* -v or --version */
		{ "help",        'h', FALSE, "show help" },                /* -h or --help */
		{ NULL, 0, 0, NULL },                                      /* end */
	};

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		return FALSE;
	}

	/* reset the options */
	options->root_dir_path = NULL;
	options->dir_layout_conf = NULL;
	options->log_priority = NULL;
	options->log_output = NULL;

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'r':
				options->root_dir_path = optarg;
				break;
			case 'c':
				options->dir_layout_conf = optarg;
				break;
			case 'l':
				options->log_priority = optarg;
				break;
			case 'o':
				options->log_output = optarg;
				break;
			case 'v':
				printf(UNI_VERSION_STRING);
				return FALSE;
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
	apr_pool_t *pool;
	client_options_t options;
	const char *log_conf_path;
	demo_framework_t *framework;
	apt_dir_layout_t *dir_layout = NULL;

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

	/* load options */
	if(demo_framework_options_load(&options,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	if(options.dir_layout_conf) {
		/* create and load directories layout from the configuration file */
		dir_layout = apt_dir_layout_create(pool);
		if(dir_layout)
			apt_dir_layout_load(dir_layout,options.dir_layout_conf,pool);
	}
	else {
		/* create default directories layout */
		dir_layout = apt_default_dir_layout_create(options.root_dir_path,pool);
	}

	if(!dir_layout) {
		printf("Failed to Create Directories Layout\n");
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* get path to logger configuration file */
	log_conf_path = apt_confdir_filepath_get(dir_layout,"logger.xml",pool);
	/* create and load singleton logger */
	apt_log_instance_load(log_conf_path,pool);

	if(options.log_priority) {
		/* override the log priority, if specified in command line */
		apt_log_priority_set(atoi(options.log_priority));
	}
	if(options.log_output) {
		/* override the log output mode, if specified in command line */
		apt_log_output_mode_set(atoi(options.log_output));
	}

	if(apt_log_output_mode_check(APT_LOG_OUTPUT_FILE) == TRUE) {
		/* open the log file */
		const char *log_dir_path = apt_dir_layout_path_get(dir_layout,APT_LAYOUT_LOG_DIR);
		apt_log_file_open(log_dir_path,"unimrcpclient",MAX_LOG_FILE_SIZE,MAX_LOG_FILE_COUNT,FALSE,pool);
	}

	/* create demo framework */
	framework = demo_framework_create(dir_layout);
	if(framework) {
		/* run command line  */
		demo_framework_cmdline_run(framework);
		/* destroy demo framework */
		demo_framework_destroy(framework);
	}

	/* destroy singleton logger */
	apt_log_instance_destroy();
	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return 0;
}
