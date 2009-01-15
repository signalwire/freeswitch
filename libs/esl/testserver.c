#include <stdio.h>
#include <stdlib.h>
#include <esl.h>

static void mycallback(esl_socket_t server_sock, esl_socket_t client_sock, struct sockaddr_in addr)
{
	esl_handle_t handle = {{0}};
	
	if (fork()) {
		close(client_sock);
		return;
	}

	esl_attach_handle(&handle, client_sock, addr);

	printf("Connected! %d\n", handle.sock);

	esl_filter(&handle, "unique-id", esl_event_get_header(handle.info_event, "caller-unique-id"));
	esl_events(&handle, ESL_EVENT_TYPE_PLAIN, "SESSION_HEARTBEAT CHANNEL_ANSWER CHANNEL_ORIGINATE CHANNEL_PROGRESS CHANNEL_HANGUP "
			   "CHANNEL_BRIDGE CHANNEL_UNBRIDGE CHANNEL_OUTGOING CHANNEL_EXECUTE CHANNEL_EXECUTE_COMPLETE DTMF CUSTOM conference::maintenance");

	esl_execute(&handle, "answer", NULL, NULL);
	esl_execute(&handle, "conference", "3000@default", NULL);
	
	while(esl_recv(&handle) == ESL_SUCCESS);

	esl_disconnect(&handle);
}

int main(void)
{
	esl_global_set_default_logger(7);
	esl_listen("localhost", 8084, mycallback);
	
	return 0;
}
