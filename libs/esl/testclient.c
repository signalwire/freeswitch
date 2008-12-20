#include <stdio.h>
#include <stdlib.h>
#include <esl.h>


int main(void)
{
	esl_handle_t handle = {0};

	handle.debug = 1;
	
	esl_connect(&handle, "localhost", 8021, "ClueCon");

	esl_send_recv(&handle, "api status\n\n");

	esl_disconnect(&handle);
	
	return 0;
}
