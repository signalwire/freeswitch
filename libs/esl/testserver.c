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
	


	esl_execute(&handle, "answer", NULL, NULL);
	esl_execute(&handle, "playback", "/ram/swimp.raw", NULL);
	
	sleep(30);

	esl_disconnect(&handle);
}

int main(void)
{
	esl_global_set_default_logger(7);
	esl_listen("localhost", 8084, mycallback);
	
	return 0;
}
