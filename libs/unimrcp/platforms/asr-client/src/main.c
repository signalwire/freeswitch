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
 * $Id: main.c 2213 2014-11-06 03:02:45Z achaloyan@gmail.com $
 */

#include <stdio.h>
#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include <apr_thread_proc.h>
#include "asr_engine.h"

typedef struct {
	const char        *root_dir_path;
	apt_log_priority_e log_priority;
	apt_log_output_e   log_output;
	apr_pool_t        *pool;
} client_options_t;

typedef struct {
	asr_engine_t      *engine;
	const char        *grammar_file;
	const char        *input_file;
	const char        *profile;
	
	apr_thread_t      *thread;
	apr_pool_t        *pool;
} asr_params_t;

/** Thread function to run ASR scenario in */
static void* APR_THREAD_FUNC asr_session_run(apr_thread_t *thread, void *data)
{
	asr_params_t *params = data;
	asr_session_t *session = asr_session_create(params->engine,params->profile);
	if(session) {
		const char *result = asr_session_file_recognize(session,params->grammar_file,params->input_file);
		if(result) {
			printf("Recog Result [%s]",result);
		}

		asr_session_destroy(session);
	}

	/* destroy pool params allocated from */
	apr_pool_destroy(params->pool);
	return NULL;
}

/** Launch demo ASR session */
static apt_bool_t asr_session_launch(asr_engine_t *engine, const char *grammar_file, const char *input_file, const char *profile)
{
	apr_pool_t *pool;
	asr_params_t *params;
	
	/* create pool to allocate params from */
	apr_pool_create(&pool,NULL);
	params = apr_palloc(pool,sizeof(asr_params_t));
	params->pool = pool;
	params->engine = engine;

	if(grammar_file) {
		params->grammar_file = apr_pstrdup(pool,grammar_file);
	}
	else {
		params->grammar_file = "grammar.xml";
	}

	if(input_file) {
		params->input_file = apr_pstrdup(pool,input_file);
	}
	else {
		params->input_file = "one-8kHz.pcm";
	}
	
	if(profile) {
		params->profile = apr_pstrdup(pool,profile);
	}
	else {
		params->profile = "uni2";
	}

	/* Launch a thread to run demo ASR session in */
	if(apr_thread_create(&params->thread,NULL,asr_session_run,params,pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return FALSE;
	}
	
	return TRUE;
}

static apt_bool_t cmdline_process(asr_engine_t *engine, char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);

	if(strcasecmp(name,"run") == 0) {
		char *grammar = apr_strtok(NULL, " ", &last);
		char *input = apr_strtok(NULL, " ", &last);
		char *profile = apr_strtok(NULL, " ", &last);
		asr_session_launch(engine,grammar,input,profile);
	}
	else if(strcasecmp(name,"loglevel") == 0) {
		char *priority = apr_strtok(NULL, " ", &last);
		if(priority) {
			asr_engine_log_priority_set(atol(priority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcasecmp(name,"help") == 0) {
		printf("usage:\n"
			"\n- run [grammar_file] [audio_input_file] [profile_name] (run demo asr client)\n"
			"       grammar_file is the name of grammar file, (path is relative to data dir)\n"
			"       audio_input_file is the name of audio file, (path is relative to data dir)\n"
			"       profile_name is one of 'uni2', 'uni1', ...\n"
			"\n       examples: \n"
			"           run\n"
			"           run grammar.xml one.pcm\n"
			"           run grammar.xml one.pcm uni1\n"
		    "\n- loglevel [level] (set loglevel, one of 0,1...7)\n"
		    "\n- quit, exit\n");
	}
	else {
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

static apt_bool_t cmdline_run(asr_engine_t *engine)
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
			running = cmdline_process(engine,cmdline);
		}
	}
	while(running != 0);
	return TRUE;
}

static void usage(void)
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  asrclient [options]\n"
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
		"   -h [--help]              : Show the help.\n"
		"\n");
}

static void options_destroy(client_options_t *options)
{
	if(options->pool) {
		apr_pool_destroy(options->pool);
	}
}

static client_options_t* options_load(int argc, const char * const *argv)
{
	apr_status_t rv;
	apr_getopt_t *opt = NULL;
	int optch;
	const char *optarg;
	apr_pool_t *pool;
	client_options_t *options;

	const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },  /* -r arg or --root-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* create APR pool to allocate options from */
	apr_pool_create(&pool,NULL);
	if(!pool) {
		return NULL;
	}
	options = apr_palloc(pool,sizeof(client_options_t));
	options->pool = pool;
	/* set the default options */
	options->root_dir_path = NULL;
	options->log_priority = APT_PRIO_INFO;
	options->log_output = APT_LOG_OUTPUT_CONSOLE;


	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		options_destroy(options);
		return NULL;
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
			case 'h':
				usage();
				return FALSE;
		}
	}

	if(rv != APR_EOF) {
		usage();
		options_destroy(options);
		return NULL;
	}

	return options;
}

int main(int argc, const char * const *argv)
{
	client_options_t *options;
	asr_engine_t *engine;

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* load options */
	options = options_load(argc,argv);
	if(!options) {
		apr_terminate();
		return 0;
	}

	/* create asr engine */
	engine = asr_engine_create(
				options->root_dir_path,
				options->log_priority,
				options->log_output);
	if(engine) {
		/* run command line  */
		cmdline_run(engine);
		/* destroy demo framework */
		asr_engine_destroy(engine);
	}

	/* destroy options */
	options_destroy(options);

	/* APR global termination */
	apr_terminate();
	return 0;
}
