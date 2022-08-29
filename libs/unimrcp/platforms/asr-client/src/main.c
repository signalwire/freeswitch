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

#include <stdio.h>
#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_file_info.h>
#include <apr_thread_proc.h>
#include "asr_engine.h"
#include "asr_engine_common.h"

#define DEFAULT_GRAMMAR_FILE "grammar.xml"
#define DEFAULT_INPUT_FILE "one-8kHz.pcm"
#define DEFAULT_PROFILE "uni2"

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
	const char        *params_file;
	apt_bool_t         send_set_params;
	apt_bool_t         send_get_params;

	apr_thread_t      *thread;
	apr_pool_t        *pool;
} asr_params_t;

/** Thread function to run ASR scenario in */
static void* APR_THREAD_FUNC asr_session_run(apr_thread_t *thread, void *data)
{
	const char *result;
	asr_params_t *params = data;
	asr_session_t *session = asr_session_create(params->engine,params->profile);
	if(session) {
		ParameterSet* initial_params = NULL;
		ParameterSet* final_params = NULL;
		if(params->send_get_params) {
			// Get all default parameters from session
			initial_params = asr_session_get_all_params(session);
		}

		if(params->send_set_params) {
			// Set parameters from param file
			asr_session_set_param(session,params->params_file,NULL,NULL);
		}

		// Do recognition
		result = asr_session_file_recognize(session,params->grammar_file,params->input_file,params->params_file,params->send_set_params);
		if(result) {
			printf("Recog Result [%s]",result);
		}

		if(params->send_get_params) {
			// Get all session parameters after recognition
			final_params = asr_session_get_all_params(session);

			if(initial_params->n_best_list_length != final_params->n_best_list_length) {
				// this is just a sample. The returned ParameterSet may be useful in different test scenarios.
				printf("Parameters do not match.");
			}
		}
		asr_session_destroy(session);
	}

	/* destroy pool params allocated from */
	apr_pool_destroy(params->pool);
	return NULL;
}

/** Launch demo ASR session */
static apt_bool_t asr_session_launch(asr_engine_t *engine, const char *grammar_file, const char *input_file, const char *profile, const char* params_file, apt_bool_t send_set_params, apt_bool_t send_get_params)
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
		params->grammar_file = DEFAULT_GRAMMAR_FILE;
	}

	if(input_file) {
		params->input_file = apr_pstrdup(pool,input_file);
	}
	else {
		params->input_file = DEFAULT_INPUT_FILE;
	}

	if(profile && profile[0] != '-') {
		params->profile = apr_pstrdup(pool,profile);
	}
	else {
		params->profile = DEFAULT_PROFILE;
	}

	if(params_file && params_file[0] != '-') {
		params->params_file = apr_pstrdup(pool,params_file);
	}
	else {
		params->params_file = NULL;
	}

	params->send_set_params = send_set_params;
	params->send_get_params = send_get_params;

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
	if(!name)
		return running;

	if(strcasecmp(name,"run") == 0) {
		apt_bool_t send_set_params = FALSE;
		apt_bool_t send_get_params = FALSE;

		char *grammar = apr_strtok(NULL, " ", &last);
		char *input = apr_strtok(NULL, " ", &last);
		char *profile = apr_strtok(NULL, " ", &last);
		char *params_file = apr_strtok(NULL, " ", &last);
		char *str_send_set_params = apr_strtok(NULL, " ", &last);
		char *str_send_get_params = apr_strtok(NULL, " ", &last);

		if(str_send_set_params != NULL && strcasecmp(str_send_set_params,"SET") == 0) {
			send_set_params = TRUE;
		}
		if(str_send_get_params != NULL && strcasecmp(str_send_get_params, "GET") == 0) {
			send_get_params = TRUE;
		}

		asr_session_launch(engine,grammar,input,profile,params_file,send_set_params,send_get_params);
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
			"Run demo ASR client\n"
			"Run grammar_uri_list audio_input_file [profile_name | -] [params_file | -] [set | -] [get | -]\n"
			"\n"
			"    grammar_uri_list is the name of a grammar file (path is relative to data dir)\n"
			"      (default is " DEFAULT_GRAMMAR_FILE ")\n"
			"      or a comma separated list of grammar uris, where grammar_uri may be one of the following:\n"
			"       - http:// or https:// - grammars hosted on a web server and accessible by http/s\n"
			"       - builtin: - grammar is a VoiceXML builtin grammar like builtin:grammar/boolean\n"
			"       - supported prorpietary grammar URIs\n"
			"       - (no URI prefix) - grammar file is read from local data folder.\n"
			"      or a comma separated list of weighted grammars, each in text/grammar-ref-list format\n"
			"\n"
			"    audio_input_file is the name of audio file to process, (path is relative to data dir). \n"
			"      headerless PCM files are supported as well as WAV files with RIFF headers.\n"
			"      (default is " DEFAULT_INPUT_FILE ")\n"
			"\n"
			"    profile_name is the configured client-profile, like one of 'uni2', 'uni1', ...\n"
			"      (default is " DEFAULT_PROFILE ")\n"
			"\n"
			"    params_file is a path to a file of MRCP headers. A dash (-) may be used to skip this parameter.\n"
			"        Example headers are:\n"
			"          N-Best-List-Length: 3\n"
			"          No-Input-Timeout: 3000\n"
			"          Speech-Complete-Timeout: 500\n"
			"      (default is no parameter file. In this case the ASR defaults are used)\n"
			"\n"
			"   set - send parameter_file as headers in MRCP SET-PARAMS method, otherwise\n"
			"        parameters are sent as headers in the MRCP RECOGNIZE method \n"
			"\n"
			"   get - send MRCP GET-PARAMS method before and after recognition\n"
			"\n"
			"   example: \n"
			"      run grammar.xml one-8kHz.pcm uni2 params_default.txt set get\n"
			"   other examples: \n"
			"      run grammar.xml one.wav\n"
			"      run builtin:grammar/boolean yes.wav\n"
			"      run grammar.xml,builtin:grammar/boolean,http://example.com/operator.grxml speak_to_representative.wav\n"
			"      run operator.grxml speak_to_representative.wav uni2\n"
			"      run builtin:grammar/boolean yes.pcm uni2\n"
			"      run builtin:grammar/boolean yes.wav - params.txt set get\n"
			"      run http://example.com/grammars/grammar.grxml one.wav uni2\n"
			"      run <http://localhost/grammars/grammar.grxml>;weight=\"2.0\",<builtin:grammar/boolean>;weight=\"0.75\"\n"
			"\n"
			"- loglevel [level] (set loglevel, one of 0,1...7)\n"
			"\n"
			"- quit, exit\n");
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
