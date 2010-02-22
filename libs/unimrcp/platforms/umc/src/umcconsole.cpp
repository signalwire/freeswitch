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

#include <stdio.h>
#include <stdlib.h>
#include <apr_getopt.h>
#include "umcconsole.h"
#include "umcframework.h"
#include "apt_pool.h"


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

	/* create the structure of default directories layout */
	pDirLayout = apt_default_dir_layout_create(m_Options.m_RootDirPath,pool);
	/* create singleton logger */
	apt_log_instance_create(m_Options.m_LogOutput,m_Options.m_LogPriority,pool);

	if((m_Options.m_LogOutput & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE) 
	{
		/* open the log file */
		apt_log_file_open(pDirLayout->log_dir_path,"unimrcpclient",MAX_LOG_FILE_SIZE,MAX_LOG_FILE_COUNT,pool);
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

	if(strcasecmp(name,"run") == 0)
	{
		char* pScenarioName = apr_strtok(NULL, " ", &last);
		if(pScenarioName) 
		{
			const char* pProfileName = apr_strtok(NULL, " ", &last);
			if(!pProfileName) 
			{
				pProfileName = "MRCPv2-Default";
			}
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
			   "       profile is one of 'MRCPv2-Default', 'MRCPv1-Default', ... (see unimrcpclient.xml)\n"
			   "\n       examples: \n"
			   "           run synth\n"
			   "           run recog\n"
			   "           run synth MRCPv1-Default\n"
			   "           run recog MRCPv1-Default\n"
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
	int i;
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

void UmcConsole::Usage() const
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  umc [options]\n"
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

bool UmcConsole::LoadOptions(int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t* opt = NULL;
	int optch;
	const char* optarg;

	const apr_getopt_option_t opt_option[] = 
	{
		/* long-option, short-option, has-arg flag, description */
		{ "root-dir",    'r', TRUE,  "path to root dir" },  /* -r arg or --root-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* set the default options */
	m_Options.m_RootDirPath = "../";
	m_Options.m_LogPriority = APT_PRIO_INFO;
	m_Options.m_LogOutput = APT_LOG_OUTPUT_CONSOLE;

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
			case 'l':
				if(optarg) 
				{
					m_Options.m_LogPriority = (apt_log_priority_e) atoi(optarg);
				}
				break;
			case 'o':
				if(optarg) 
				{
					m_Options.m_LogOutput = (apt_log_output_e) atoi(optarg);
				}
				break;
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
