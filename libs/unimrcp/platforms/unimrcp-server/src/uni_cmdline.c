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
#include "unimrcp_server.h"
#include "apt_log.h"

static apt_bool_t cmdline_process(mrcp_server_t *server, char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);
	if(!name)
		return running;

	if(strcasecmp(name,"loglevel") == 0) {
		char *priority = apr_strtok(NULL, " ", &last);
		if(priority) {
			apt_log_priority_set(atol(priority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcasecmp(name,"offline") == 0) {
		mrcp_server_offline(server);
	}
	else if(strcasecmp(name,"online") == 0) {
		mrcp_server_online(server);
	}
	else if(strcasecmp(name,"help") == 0) {
		printf("usage:\n");
		printf("- loglevel [level] (set loglevel, one of 0,1...7)\n");
		printf("- offline (take server offline)\n");
		printf("- online (bring server online)\n");
		printf("- quit, exit\n");
	}
	else {
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

apt_bool_t uni_cmdline_run(apt_dir_layout_t *dir_layout, apr_pool_t *pool)
{
	apt_bool_t running = TRUE;
	char cmdline[1024];
	apr_size_t i;
	mrcp_server_t *server;

	/* start server */
	server = unimrcp_server_start(dir_layout);
	if(!server) {
		return FALSE;
	}

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
			running = cmdline_process(server, cmdline);
		}
	}
	while(running != 0);

	/* shutdown server */
	unimrcp_server_shutdown(server);
	return TRUE;
}
