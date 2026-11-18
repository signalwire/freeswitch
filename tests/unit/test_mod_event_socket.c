/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2026, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Dmitry Verenitsin <dmitry.verenitsin@signalwire.com>
 *
 *
 * test_mod_event_socket.c -- Tests for mod_event_socket
 *
 */

#include <switch.h>
#include <test/switch_test.h>

#define ESL_TEST_HOST "127.0.0.1"
#define ESL_TEST_PORT 38021

/* Must match MAX_CONTENT_LENGTH in src/mod/event_handlers/mod_event_socket/mod_event_socket.c */
#define ESL_MAX_CONTENT_LENGTH (16 * 1024 * 1024)

static switch_status_t esl_connect(switch_socket_t **sock_out, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *addr = NULL;
	switch_socket_t *sock = NULL;
	int attempts;

	if (switch_sockaddr_info_get(&addr, ESL_TEST_HOST, SWITCH_UNSPEC,
								 ESL_TEST_PORT, 0, pool) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	for (attempts = 0; attempts < 50; attempts++) {
		if (switch_socket_create(&sock, switch_sockaddr_get_family(addr),
								 SOCK_STREAM, SWITCH_PROTO_TCP, pool) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		switch_socket_opt_set(sock, SWITCH_SO_TCP_NODELAY, 1);

		if (switch_socket_connect(sock, addr) == SWITCH_STATUS_SUCCESS) {
			*sock_out = sock;
			return SWITCH_STATUS_SUCCESS;
		}

		switch_socket_close(sock);
		sock = NULL;
		switch_yield(100000);
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t send_all(switch_socket_t *sock, const char *buf, switch_size_t len)
{
	switch_size_t remaining = len;
	const char *p = buf;

	while (remaining > 0) {
		switch_size_t n = remaining;
		if (switch_socket_send(sock, p, &n) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		if (n == 0) {
			return SWITCH_STATUS_FALSE;
		}
		p += n;
		remaining -= n;
	}
	return SWITCH_STATUS_SUCCESS;
}

/* Read a single ESL frame, terminating at "\n\n" or buffer-full. */
static switch_size_t recv_frame(switch_socket_t *sock, char *out, switch_size_t cap)
{
	switch_size_t got = 0;

	while (got < cap - 1) {
		switch_size_t want = 1;
		if (switch_socket_recv(sock, out + got, &want) != SWITCH_STATUS_SUCCESS || want == 0) {
			break;
		}
		got++;
		if (got >= 2 && out[got - 1] == '\n' && out[got - 2] == '\n') {
			break;
		}
	}
	out[got] = '\0';
	return got;
}

/* Drain whatever the server has to say until the socket closes. */
static void recv_until_close(switch_socket_t *sock)
{
	char tmp[256];

	for (;;) {
		switch_size_t want = sizeof(tmp);
		if (switch_socket_recv(sock, tmp, &want) != SWITCH_STATUS_SUCCESS || want == 0) {
			break;
		}
	}
}

/* Connect, read auth/request greeting. Returns connected socket or NULL. */
static switch_socket_t *esl_connect_and_greet(switch_memory_pool_t *pool)
{
	switch_socket_t *sock = NULL;
	char buf[256] = { 0 };
	switch_size_t got;

	if (esl_connect(&sock, pool) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	got = recv_frame(sock, buf, sizeof(buf));
	if (got == 0 || !strstr(buf, "auth/request")) {
		switch_socket_close(sock);
		return NULL;
	}

	return sock;
}

/* Send an auth packet carrying the supplied Content-Length value, wait for
 * the server to drop the connection, then reconnect. Returns
 * SWITCH_STATUS_SUCCESS only when the reconnect succeeds and the listener
 * answers with the auth/request greeting — i.e. the daemon is still alive
 * and serving connections. */
static switch_status_t probe_bad_content_length(switch_memory_pool_t *pool, const char *clen_value)
{
	switch_socket_t *sock = NULL;
	switch_socket_t *probe = NULL;
	char packet[256];
	switch_size_t pkt_len;
	switch_status_t status = SWITCH_STATUS_FALSE;

	sock = esl_connect_and_greet(pool);
	if (!sock) {
		goto probe_bad_clen_done;
	}

	pkt_len = switch_snprintf(packet, sizeof(packet),
		"auth ClueCon\n"
		"Content-Length: %s\n"
		"\n",
		clen_value);

	if (send_all(sock, packet, pkt_len) != SWITCH_STATUS_SUCCESS) {
		goto probe_bad_clen_done;
	}

	/* Server is expected to drop the connection on a malformed
	 * Content-Length. Drain until close so the next probe starts clean. */
	recv_until_close(sock);

	/* The reconnect is the liveness check: if the listener thread or
	 * the whole daemon went down, this retries 50 times then fails. */
	probe = esl_connect_and_greet(pool);
	if (probe) {
		status = SWITCH_STATUS_SUCCESS;
	}

probe_bad_clen_done:
	if (sock) switch_socket_close(sock);
	if (probe) switch_socket_close(probe);
	return status;
}

FST_CORE_DB_BEGIN("./conf_event_socket")
{
	FST_SUITE_BEGIN(test_mod_event_socket)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_event_socket");
			switch_yield(500000);
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(content_length_validation)
		{
			switch_socket_t *sock = NULL;
			char val[32];
			char reply[256] = { 0 };

			/* INT_MAX, above-cap, negative, and atoi-overflow values must
			 * all be rejected without taking the daemon down. */
			fst_xcheck(probe_bad_content_length(fst_pool, "2147483647") == SWITCH_STATUS_SUCCESS,
				"daemon must still be alive after Content-Length: INT_MAX");

			switch_snprintf(val, sizeof(val), "%d", ESL_MAX_CONTENT_LENGTH + 1);
			fst_xcheck(probe_bad_content_length(fst_pool, val) == SWITCH_STATUS_SUCCESS,
				"daemon must still be alive after Content-Length above cap");

			fst_xcheck(probe_bad_content_length(fst_pool, "-1") == SWITCH_STATUS_SUCCESS,
				"daemon must still be alive after Content-Length: -1");

			fst_xcheck(probe_bad_content_length(fst_pool, "99999999999999999") == SWITCH_STATUS_SUCCESS,
				"daemon must still be alive after Content-Length atoi-overflow value");

			/* Content-Length: 0 is a valid no-body packet and must be
			 * accepted — auth completes normally. */
			do {
				sock = esl_connect_and_greet(fst_pool);
				if (!sock) {
					fst_fail("could not connect to event_socket listener");
					break;
				}

				if (send_all(sock, "auth ClueCon\nContent-Length: 0\n\n",
							 strlen("auth ClueCon\nContent-Length: 0\n\n")) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send auth packet");
					break;
				}

				recv_frame(sock, reply, sizeof(reply));
				fst_check_string_has(reply, "+OK accepted");
			} while (0);

			if (sock) switch_socket_close(sock);
		}
		FST_TEST_END()

		/* A packet with a valid non-zero Content-Length must be parsed
		 * end-to-end: command, headers, and body fully consumed before
		 * the server replies. */
		FST_TEST_BEGIN(content_length_with_body)
		{
			switch_socket_t *sock = NULL;
			char reply[256] = { 0 };
			const char *packet = "auth ClueCon\nContent-Length: 5\n\n12345";

			do {
				sock = esl_connect_and_greet(fst_pool);
				if (!sock) {
					fst_fail("could not connect to event_socket listener");
					break;
				}

				if (send_all(sock, packet, strlen(packet)) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send auth packet with body");
					break;
				}

				recv_frame(sock, reply, sizeof(reply));
				fst_check_string_has(reply, "+OK accepted");
			} while (0);

			if (sock) switch_socket_close(sock);
		}
		FST_TEST_END()

		/* A `sendevent` with a body must dispatch an event whose body
		 * bytes match what was sent. Exercises the authenticated
		 * command-loop and end-to-end body delivery via the listener's
		 * event subscription. */
		FST_TEST_BEGIN(sendevent_body_roundtrip)
		{
			switch_socket_t *sock = NULL;
			char buf[8192] = { 0 };
			switch_size_t got;
			const char *send_pkt =
				"sendevent CUSTOM\n"
				"Event-Subclass: test_body_roundtrip\n"
				"Content-Length: 11\n"
				"\n"
				"hello world";

			do {
				sock = esl_connect_and_greet(fst_pool);
				if (!sock) {
					fst_fail("could not connect to event_socket listener");
					break;
				}

				if (send_all(sock, "auth ClueCon\n\n",
							 strlen("auth ClueCon\n\n")) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send auth");
					break;
				}
				recv_frame(sock, buf, sizeof(buf));
				if (!strstr(buf, "+OK accepted")) {
					fst_fail("auth not accepted");
					break;
				}

				/* Subscribe to our specific CUSTOM subclass so that
				 * unrelated custom events on the wire don't leak into
				 * the assertions below. */
				if (send_all(sock, "event plain CUSTOM test_body_roundtrip\n\n",
							 strlen("event plain CUSTOM test_body_roundtrip\n\n")) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send event subscription");
					break;
				}
				memset(buf, 0, sizeof(buf));
				recv_frame(sock, buf, sizeof(buf));
				if (!strstr(buf, "+OK event listener enabled")) {
					fst_fail("event subscription not acked");
					break;
				}

				if (send_all(sock, send_pkt, strlen(send_pkt)) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send sendevent packet");
					break;
				}

				/* Drain reply + dispatched event into a single buffer.
				 * The recv timeout bounds the wait when the listener
				 * has nothing more to send. */
				memset(buf, 0, sizeof(buf));
				switch_socket_timeout_set(sock, 500 * 1000);
				got = 0;
				while (got < sizeof(buf) - 1) {
					switch_size_t want = sizeof(buf) - 1 - got;
					if (switch_socket_recv(sock, buf + got, &want) != SWITCH_STATUS_SUCCESS) break;
					if (want == 0) break;
					got += want;
				}
				buf[got] = '\0';

				fst_check_string_has(buf, "Event-Name: CUSTOM");
				fst_check_string_has(buf, "Event-Subclass: test_body_roundtrip");
				fst_check_string_has(buf, "hello world");
			} while (0);

			if (sock) switch_socket_close(sock);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
