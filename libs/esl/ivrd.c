/*
 * Copyright (c) 2009, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <esl.h>


static void mycallback(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in *addr)
{
	esl_handle_t handle = {{0}};
	char path_buffer[1024] = { 0 };
	const char *path;
	
	if (fork()) {
		close(client_sock);
		return;
	}
	

	esl_attach_handle(&handle, client_sock, addr);

	if (!(path = esl_event_get_header(handle.info_event, "variable_ivr_path"))) {
		esl_disconnect(&handle);
		esl_log(ESL_LOG_ERROR, "Missing ivr_path param!\n");
		exit(0);
	}

	strncpy(path_buffer, path, sizeof(path_buffer) - 1);
	
	/* hotwire the socket to STDIN/STDOUT */
	dup2(client_sock, STDIN_FILENO);
	dup2(client_sock, STDOUT_FILENO);

	/* close the handle but leak the socket on purpose cos the child will need it open */
	handle.sock = -1;
	esl_disconnect(&handle);
	
	execl(path_buffer, path_buffer, (char *)NULL);
	//system(path_buffer);
	close(client_sock);
	exit(0);
}

int main(int argc, char *argv[])
{
	int i;
	char *ip = NULL;
	int port = 0;
	
	for (i = 1; i + 1 < argc; ) {
		if (!strcasecmp(argv[i], "-h")) {
			ip = argv[++i];
		} else if (!strcasecmp(argv[i], "-p")) {
			port = atoi(argv[++i]);
		} else {
			i++;
		}
	}

	if (!(ip && port)) {
		fprintf(stderr, "Usage %s -h <host> -p <port>\n", argv[0]);
		return -1;
	}

	signal(SIGCHLD, SIG_IGN);

	esl_listen(ip, port, mycallback);
	
	return 0;
}
