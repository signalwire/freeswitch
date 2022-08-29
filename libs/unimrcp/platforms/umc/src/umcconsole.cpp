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
#include "umcconsole.h"
#include "umcframework.h"
#include "apt_pool.h"
#include "uni_revision.h"


UmcConsole::UmcConsole() :
	m_pFramework(NULL)
{
	m_pFramework = new UmcFramework;
}

UmcConsole::~UmcConsole()
{
	delete m_pFramework;
}

bool UmcConsole::Run(int argc, const char * const *argv)
{
	apr_pool_t* pool = NULL;
	apt_dir_layout_t* pDirLayout = NULL;
	const char *logConfPath;
	const char *logPrefix = "unimrcpclient";

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) 
	{
		apr_terminate();
		return false;
	}

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) 
	{
		apr_terminate();
		return false;
	}

	/* load options */
	if(!LoadOptions(argc,argv,pool))
	{
		apr_pool_destroy(pool);
		apr_terminate();
		return false;
	}

	if(m_Options.m_DirLayoutConf)
	{
		/* create and load directories layout from the configuration file */
		pDirLayout = apt_dir_layout_create(pool);
		if(pDirLayout)
			apt_dir_layout_load(pDirLayout,m_Options.m_DirLayoutConf,pool);
	}
	else
	{
		/* create default directories layout */
		pDirLayout = apt_default_dir_layout_create(m_Options.m_RootDirPath,pool);
	}

	if(!pDirLayout)
	{
		printf("Failed to Create Directories Layout\n");
		apr_pool_destroy(pool);
		apr_terminate();
		return false;
	}

	/* get path to logger configuration file */
	logConfPath = apt_confdir_filepath_get(pDirLayout,"logger.xml",pool);
	/* create and load singleton logger */
	apt_log_instance_load(logConfPath,pool);

	if(m_Options.m_LogPriority) 
	{
		/* override the log priority, if specified in command line */
		apt_log_priority_set((apt_log_priority_e)atoi(m_Options.m_LogPriority));
	}
	if(m_Options.m_LogOutput) 
	{
		/* override the log output mode, if specified in command line */
		apt_log_output_mode_set((apt_log_output_e)atoi(m_Options.m_LogOutput));
	}

	if(apt_log_output_mode_check(APT_LOG_OUTPUT_FILE) == TRUE) 
	{
		/* open the log file */
		const char *logDirPath = apt_dir_layout_path_get(pDirLayout,APT_LAYOUT_LOG_DIR);
		const char *logfileConfPath = apt_confdir_filepath_get(pDirLayout,"logfile.xml",pool);
		apt_log_file_open_ex(logDirPath,logPrefix,logfileConfPath,pool);
	}

	if(apt_log_output_mode_check(APT_LOG_OUTPUT_SYSLOG) == TRUE)
	{
		/* open the syslog */
		const char *logfileConfPath = apt_confdir_filepath_get(pDirLayout,"syslog.xml",pool);
		apt_syslog_open(logPrefix,logfileConfPath,pool);
	}

	/* create demo framework */
	if(m_pFramework->Create(pDirLayout,pool))
	{
		/* run command line  */
		RunCmdLine();
		/* destroy demo framework */
		m_pFramework->Destroy();
	}

	/* destroy singleton logger */
	apt_log_instance_destroy();
	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return true;
}

bool UmcConsole::ProcessCmdLine(char* pCmdLine)
{
	bool running = true;
	char *name;
	char *last;
	name = apr_strtok(pCmdLine, " ", &last);
	if(!name)
		return running;

	if(strcasecmp(name,"run") == 0)
	{
		char* pScenarioName = apr_strtok(NULL, " ", &last);
		if(pScenarioName) 
		{
			const char* pProfileName = apr_strtok(NULL, " ", &last);
			m_pFramework->RunSession(pScenarioName,pProfileName);
		}
	}
	else if(strcasecmp(name,"kill") == 0)
	{
		char* pID = apr_strtok(NULL, " ", &last);
		if(pID) 
		{
			m_pFramework->KillSession(pID);
		}
	}
	else if(strcasecmp(name,"stop") == 0)
	{
		char* pID = apr_strtok(NULL, " ", &last);
		if(pID) 
		{
			m_pFramework->StopSession(pID);
		}
	}
	else if(strcasecmp(name,"show") == 0)
	{
		char* pWhat = apr_strtok(NULL, " ", &last);
		if(pWhat) 
		{
			if(strcasecmp(pWhat,"sessions") == 0)
				m_pFramework->ShowSessions();
			else if(strcasecmp(pWhat,"scenarios") == 0)
				m_pFramework->ShowScenarios();
		}
	}
	else if(strcasecmp(name,"loglevel") == 0) 
	{
		char* pPriority = apr_strtok(NULL, " ", &last);
		if(pPriority) 
		{
			apt_log_priority_set((apt_log_priority_e)atol(pPriority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) 
	{
		running = false;
	}
	else if(strcasecmp(name,"help") == 0) 
	{
		printf("usage:\n"
		       "\n- run [scenario] [profile] (run new session)\n"
			   "       scenario is one of 'synth', 'recog', ... (use 'show scenarios')\n"
			   "       profile is one of 'uni2', 'uni1', ... (see unimrcpclient.xml)\n"
			   "\n       examples: \n"
			   "           run synth\n"
			   "           run recog\n"
			   "           run synth uni1\n"
			   "           run recog uni1\n"
		       "\n- kill [id] (kill session)\n"
			   "       id is a session identifier: 1, 2, ... (use 'show sessions')\n"
			   "\n       example: \n"
			   "           kill 1\n"
		       "\n- show [what] (show either available scenarios or in-progress sessions)\n"
			   "\n       examples: \n"
			   "           show scenarios\n"
			   "           show sessions\n"
		       "\n- loglevel [level] (set loglevel, one of 0,1...7)\n"
		       "\n- quit, exit\n");
	}
	else 
	{
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

bool UmcConsole::RunCmdLine()
{
	apt_bool_t running = true;
	char cmdline[1024];
	apr_size_t i;
	do 
	{
		printf(">");
		memset(&cmdline, 0, sizeof(cmdline));
		for(i = 0; i < sizeof(cmdline); i++) 
		{
			cmdline[i] = (char) getchar();
			if(cmdline[i] == '\n') 
			{
				cmdline[i] = '\0';
				break;
			}
		}
		if(*cmdline) 
		{
			running = ProcessCmdLine(cmdline);
		}
	}
	while(running != 0);
	return true;
}

void UmcConsole::Usage()
{
	printf(
		"\n"
		" * " UNI_COPYRIGHT "\n"
		" *\n"
		UNI_LICENSE "\n"
		"\n"
		"Usage:\n"
		"\n"
		"  umc [options]\n"
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

bool UmcConsole::LoadOptions(int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t* opt = NULL;
	int optch;
	const char* optarg;

	const apr_getopt_option_t opt_option[] = 
	{
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
	if(rv != APR_SUCCESS)
		return false;

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) 
	{
		switch(optch) 
		{
			case 'r':
				m_Options.m_RootDirPath = optarg;
				break;
			case 'c':
				m_Options.m_DirLayoutConf = optarg;
				break;
			case 'l':
				if(optarg) 
				m_Options.m_LogPriority = optarg;
				break;
			case 'o':
				if(optarg) 
				m_Options.m_LogOutput = optarg;
				break;
			case 'v':
				printf("%s", UNI_FULL_VERSION_STRING);
				return FALSE;
			case 'h':
				Usage();
				return FALSE;
		}
	}

	if(rv != APR_EOF) 
	{
		Usage();
		return false;
	}

	return true;
}
