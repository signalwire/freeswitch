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
 * test_mod_verto.c -- Tests for mod_verto
 *
 */

#include <switch.h>
#include <test/switch_test.h>

#define VERTO_TEST_HOST "127.0.0.1"
#define VERTO_TEST_PORT 33081

/* Must match HTTP_POST_MAX_BODY in src/mod/endpoints/mod_verto/mod_verto.c */
#define VERTO_POST_MAX_BODY (10 * 1024 * 1024)

static switch_status_t verto_connect(switch_socket_t **sock_out, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *addr = NULL;
	switch_socket_t *sock = NULL;
	int attempts;

	if (switch_sockaddr_info_get(&addr, VERTO_TEST_HOST, SWITCH_UNSPEC,
								 VERTO_TEST_PORT, 0, pool) != SWITCH_STATUS_SUCCESS) {
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

static switch_size_t read_status_line(switch_socket_t *sock, char *out, switch_size_t cap)
{
	switch_size_t got = 0;

	while (got < cap - 1) {
		switch_size_t want = cap - 1 - got;
		if (switch_socket_recv(sock, out + got, &want) != SWITCH_STATUS_SUCCESS || want == 0) {
			break;
		}
		got += want;
		if (memchr(out, '\n', got)) break;
	}
	out[got] = '\0';
	return got;
}

FST_CORE_DB_BEGIN("./conf_verto")
{
	FST_SUITE_BEGIN(test_mod_verto)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_verto");
			switch_yield(500000);
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(post_at_cap_returns_413)
		{
			switch_memory_pool_t *pool = NULL;
			switch_socket_t *sock = NULL;
			char req[256];
			char resp[64] = { 0 };
			switch_size_t req_len;

			do {
				if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not allocate memory pool");
					break;
				}
				if (verto_connect(&sock, pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not connect to verto listener");
					break;
				}

				req_len = switch_snprintf(req, sizeof(req),
					"POST / HTTP/1.1\r\n"
					"Host: " VERTO_TEST_HOST "\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-Length: %d\r\n"
					"\r\n",
					VERTO_POST_MAX_BODY);

				if (send_all(sock, req, req_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send request");
					break;
				}

				read_status_line(sock, resp, sizeof(resp));
				fst_check_string_starts_with(resp, "HTTP/1.1 413");
			} while (0);

			if (sock) switch_socket_close(sock);
			if (pool) switch_core_destroy_memory_pool(&pool);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(post_small_body_parsed)
		{
			switch_memory_pool_t *pool = NULL;
			switch_socket_t *sock = NULL;
			const switch_size_t body_len = 32 * 1024;
			char *body = NULL;
			char req[256];
			char resp[64] = { 0 };
			switch_size_t req_len;

			do {
				if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not allocate memory pool");
					break;
				}
				if (verto_connect(&sock, pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not connect to verto listener");
					break;
				}

				body = malloc(body_len);
				if (!body) {
					fst_fail("could not allocate body buffer");
					break;
				}
				memset(body, 'x', body_len);

				req_len = switch_snprintf(req, sizeof(req),
					"POST / HTTP/1.1\r\n"
					"Host: " VERTO_TEST_HOST "\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-Length: %" SWITCH_SIZE_T_FMT "\r\n"
					"\r\n",
					body_len);

				if (send_all(sock, req, req_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send headers");
					break;
				}
				if (send_all(sock, body, body_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send body");
					break;
				}

				read_status_line(sock, resp, sizeof(resp));
				fst_check_string_starts_with(resp, "HTTP/1.1 ");
				fst_xcheck(strncmp(resp, "HTTP/1.1 413", 12) != 0,
					"server returned 413 below cap");
			} while (0);

			free(body);
			if (sock) switch_socket_close(sock);
			if (pool) switch_core_destroy_memory_pool(&pool);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(post_large_body_no_overflow)
		{
			switch_memory_pool_t *pool = NULL;
			switch_socket_t *sock = NULL;
			const switch_size_t body_len = 8 * 1024 * 1024;
			char *body = NULL;
			char req[256];
			char resp[64] = { 0 };
			switch_size_t req_len;

			do {
				if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not allocate memory pool");
					break;
				}
				if (verto_connect(&sock, pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not connect to verto listener");
					break;
				}

				body = malloc(body_len);
				if (!body) {
					fst_fail("could not allocate body buffer");
					break;
				}
				memset(body, 'x', body_len);

				req_len = switch_snprintf(req, sizeof(req),
					"POST / HTTP/1.1\r\n"
					"Host: " VERTO_TEST_HOST "\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-Length: %" SWITCH_SIZE_T_FMT "\r\n"
					"\r\n",
					body_len);

				if (send_all(sock, req, req_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send headers");
					break;
				}
				if (send_all(sock, body, body_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send body");
					break;
				}

				read_status_line(sock, resp, sizeof(resp));
				fst_check_string_starts_with(resp, "HTTP/1.1 ");
				fst_xcheck(strncmp(resp, "HTTP/1.1 413", 12) != 0,
					"server returned 413 below cap");
			} while (0);

			free(body);
			if (sock) switch_socket_close(sock);
			if (pool) switch_core_destroy_memory_pool(&pool);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(post_overflow_length_returns_413)
		{
			switch_memory_pool_t *pool = NULL;
			switch_socket_t *sock = NULL;
			char req[256];
			char resp[64] = { 0 };
			switch_size_t req_len;

			do {
				if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not allocate memory pool");
					break;
				}
				if (verto_connect(&sock, pool) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not connect to verto listener");
					break;
				}

				req_len = switch_snprintf(req, sizeof(req),
					"POST / HTTP/1.1\r\n"
					"Host: " VERTO_TEST_HOST "\r\n"
					"Content-Type: application/x-www-form-urlencoded\r\n"
					"Content-Length: 9999999999\r\n"
					"\r\n");

				if (send_all(sock, req, req_len) != SWITCH_STATUS_SUCCESS) {
					fst_fail("could not send request");
					break;
				}

				read_status_line(sock, resp, sizeof(resp));
				fst_check_string_starts_with(resp, "HTTP/1.1 413");
			} while (0);

			if (sock) switch_socket_close(sock);
			if (pool) switch_core_destroy_memory_pool(&pool);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
