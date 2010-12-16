#include <stdio.h>
#include <stdlib.h>
#include <esl.h>


int main(void)
{
	esl_handle_t handle = {{0}};
	esl_buffer_t *buffer;
	char doh[65536];

	esl_buffer_create(&buffer, 32 * 1024, 32 * 1024, 0);

	snprintf(doh, sizeof(doh), "TEST 1 FOO BAR 1234\n");
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	snprintf(doh, sizeof(doh), "TEST 1 END\n\n");
	esl_buffer_write(buffer, doh, strlen(doh));



	snprintf(doh, sizeof(doh), "TEST 2 BAR FOO 4321\n");
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	snprintf(doh, sizeof(doh), "TEST 2 END\n\n");
	esl_buffer_write(buffer, doh, strlen(doh));

	snprintf(doh, sizeof(doh), "TEST 2 BAR FOO 4321\n");
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	esl_buffer_write(buffer, doh, strlen(doh));
	snprintf(doh, sizeof(doh), "TEST 2 END\n\n");
	esl_buffer_write(buffer, doh, strlen(doh));

	printf("COUNT %ld\n", esl_buffer_packet_count(buffer));

	memset(doh, 0, sizeof(doh));
	esl_buffer_read_packet(buffer, doh, sizeof(doh));
	printf("TEST: [%s]\n", doh);

	memset(doh, 0, sizeof(doh));


	esl_buffer_read_packet(buffer, doh, sizeof(doh));
	printf("TEST2: [%s]\n", doh);

	return 0;

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
