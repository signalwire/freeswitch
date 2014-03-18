/*
 * Copyright (c) 2009-2012, Anthony Minessale II
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
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

static void handle_SIGCHLD(int sig)
{
	int status = 0;

	wait(&status);
	return;
}

static void my_forking_callback(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in *addr, void *user_data)
{
	esl_handle_t handle = {{0}};
	char path_buffer[1024] = { 0 };
	const char *path;
	char arg[64] = { 0 };

	signal(SIGCHLD, handle_SIGCHLD);

	if (fork()) {
		close(client_sock);
		return;
	}
	
	if (esl_attach_handle(&handle, client_sock, addr) != ESL_SUCCESS || !handle.info_event) {
		esl_log(ESL_LOG_ERROR, "Socket Error\n");
		exit(0);
	}

	if (!(path = esl_event_get_header(handle.info_event, "variable_ivr_path"))) {
		esl_disconnect(&handle);
		esl_log(ESL_LOG_ERROR, "Missing ivr_path param!\n");
		exit(0);
	}

	snprintf(arg, sizeof(arg), "%d", client_sock);

	strncpy(path_buffer, path, sizeof(path_buffer) - 1);
	
	/* hotwire the socket to STDIN/STDOUT */
	/* hotwire the socket to STDIN/STDOUT */
	if (!(dup2(client_sock, STDIN_FILENO)) && !(dup2(client_sock, STDOUT_FILENO))){
		esl_disconnect(&handle);
		esl_log(ESL_LOG_ERROR, "Socket Error hotwiring socket to STDIN/STDOUT!\n");
		return;
	}

	/* close the handle but leak the socket on purpose cos the child will need it open */
	handle.sock = -1;
	esl_disconnect(&handle);
	
	execl(path_buffer, path_buffer, arg, (char *)NULL);
	close(client_sock);
	exit(0);
}

static void mycallback(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in *addr, void *user_data)
{
	esl_handle_t handle = {{0}};
	const char *path;
	char path_buffer[1024] = { 0 };

	if (esl_attach_handle(&handle, client_sock, addr) != ESL_SUCCESS || !handle.info_event) {
		close(client_sock);
		esl_log(ESL_LOG_ERROR, "Socket Error\n");
		return;
	}

	if (!(path = esl_event_get_header(handle.info_event, "variable_ivr_path"))) {
		esl_disconnect(&handle);
		esl_log(ESL_LOG_ERROR, "Missing ivr_path param!\n");
		return;
	}

	snprintf(path_buffer, sizeof(path_buffer), "%s %d", path, client_sock);


	if (system(path_buffer)) {
		 esl_log(ESL_LOG_ERROR, "System Call Failed! [%s]\n", strerror(errno));
	}

	esl_disconnect(&handle);
	
}

int main(int argc, char *argv[])
{
	int i;
	char *ip = NULL;
	int port = 0, thread = 0;
	
	for (i = 1; i < argc; ) {
		int cont = 0;

		if (i + 1 < argc) {
			if (!strcasecmp(argv[i], "-h")) {
				ip = argv[++i]; cont++;
			} else if (!strcasecmp(argv[i], "-p")) {
				port = atoi(argv[++i]); cont++;
			}
		}

		if (cont) {
			i++;
			continue;
		}

		if (!strcasecmp(argv[i], "-t")) {
			thread++;
		}

		i++;
	}

	if (!(ip && port)) {
		fprintf(stderr, "Usage %s [-t] -h <host> -p <port>\n", argv[0]);
		return -1;
	}

	/*
	 * NOTE: fflush() stdout before entering esl_listen().
	 * Not doing so causes the fork()ed child to repeat the message for every incoming
	 * connection, if stdout has been redirected (into a logfile, for example).
	 */
	if (thread) {
		printf("Starting threaded listener.\n");
		fflush(stdout);
		esl_listen_threaded(ip, port, mycallback, NULL, 100000);
	} else {
		printf("Starting forking listener.\n");
		fflush(stdout);
		esl_listen(ip, port, my_forking_callback, NULL, NULL);
	}

	return 0;
}
