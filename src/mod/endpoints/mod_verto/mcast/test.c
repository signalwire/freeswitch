#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "mcast.h"

int main(int argc, char *argv[])
{
	mcast_handle_t handle;

	if (argc < 2) {
		printf("WTF\n");
		exit(-1);
	}

	mcast_socket_create("231.3.3.7", 1337, &handle, MCAST_SEND | MCAST_RECV | MCAST_TTL_HOST);
	perror("create");
	
	if (!strcmp(argv[1], "send")) {
		mcast_socket_send(&handle, argv[2], strlen(argv[2]));
		exit(0);
	}

	for(;;) {
		int r = mcast_socket_recv(&handle, NULL, 0);
		printf("RECV %d [%s]\n", r, (char *)handle.buffer);
	}

}
