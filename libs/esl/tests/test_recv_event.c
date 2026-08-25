/*
 * test_recv_event.c
 *
 * Verifies that esl_recv_event() rejects out-of-range Content-Length
 * values: negative numbers and values above ESL_MAX_CONTENT_LENGTH must
 * cause the function to return ESL_FAIL and mark the handle as
 * disconnected, leaving no allocated state behind.
 *
 * POSIX-only: uses socketpair(2). Returns 77 on Windows so automake
 * marks the test as skipped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

int main(void)
{
	return 77;
}

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <esl.h>

#define TEST_ASSERT(cond) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "FAIL %s:%d: %s\n",			\
			__FILE__, __LINE__, #cond);			\
		exit(1);						\
	}								\
} while (0)

static void prepare_handle(esl_handle_t *h, esl_socket_t s)
{
	memset(h, 0, sizeof(*h));
	h->sock = s;
	h->connected = 1;
	TEST_ASSERT(esl_mutex_create(&h->mutex) == ESL_SUCCESS);
	TEST_ASSERT(esl_buffer_create(&h->packet_buf,
		BUF_CHUNK, BUF_START, 0) == ESL_SUCCESS);
}

static void expect_rejected(const char *frame, const char *desc)
{
	int sv[2];
	esl_handle_t h;
	size_t n = strlen(frame);
	ssize_t w;

	fprintf(stderr, "  case: %s\n", desc);

	TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	prepare_handle(&h, sv[0]);

	w = write(sv[1], frame, n);
	TEST_ASSERT(w == (ssize_t) n);
	close(sv[1]);

	TEST_ASSERT(esl_recv_event(&h, 0, NULL) == ESL_FAIL);
	TEST_ASSERT(h.connected == 0);

	esl_disconnect(&h);
}

int main(void)
{
	fprintf(stderr, "test_recv_event: invalid Content-Length is rejected\n");

	expect_rejected(
		"Content-Type: text/event-plain\n"
		"Content-Length: -1\n\n",
		"negative Content-Length: -1");

	expect_rejected(
		"Content-Type: text/event-plain\n"
		"Content-Length: -2\n\n",
		"negative Content-Length: -2");

	expect_rejected(
		"Content-Type: text/event-plain\n"
		"Content-Length: 99999999999\n\n",
		"Content-Length above ESL_MAX_CONTENT_LENGTH");

	fprintf(stderr, "OK\n");
	return 0;
}

#endif
