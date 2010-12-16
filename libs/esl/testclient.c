#include <stdio.h>
#include <stdlib.h>
#include <esl.h>


int main(void)
{
	esl_handle_t handle = {{0}};

	esl_connect(&handle, "localhost", 8021, NULL, "ClueCon");

	esl_send_recv(&handle, "api status\n\n");

	if (handle.last_sr_event && handle.last_sr_event->body) {
		printf("%s\n", handle.last_sr_event->body);
	} else {
		// this is unlikely to happen with api or bgapi (which is hardcoded above) but prefix but may be true for other commands
		printf("%s\n", handle.last_sr_reply);
	}

	esl_disconnect(&handle);
	
	return 0;
}
