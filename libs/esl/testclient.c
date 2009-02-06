#include <stdio.h>
#include <stdlib.h>
#include <esl.h>


int main(void)
{
	esl_handle_t handle = {{0}};

	esl_connect(&handle, "localhost", 8021, "ClueCon");

	esl_send_recv(&handle, "api conference 3100 play /root/sr8k.raw 1\n\n");
	
	esl_send_recv(&handle, "api uuid_transfer aa1cedb1-5abc-4154-b068-9a0d1be6a7b6 3101\n\n");
	
	esl_send_recv(&handle, "api conference 3101 play /root/sr8k.raw 2\n\n");
	
	esl_send_recv(&handle, "api uuid_transfer aa1cedb1-5abc-4154-b068-9a0d1be6a7b6 3102\n\n");
	
	esl_send_recv(&handle, "api conference 3102 play /root/sr8k.raw 3\n\n");
	
	esl_send_recv(&handle, "api uuid_transfer aa1cedb1-5abc-4154-b068-9a0d1be6a7b6 3103\n\n");
	
	esl_send_recv(&handle, "api conference 3103 play /root/sr8k.raw 4\n\n");
	
	esl_send_recv(&handle, "api uuid_transfer aa1cedb1-5abc-4154-b068-9a0d1be6a7b6 3104\n\n");

	esl_disconnect(&handle);
	
	return 0;
}
