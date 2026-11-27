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
 * Mohan Cao <mohancao@yahoo.com.au>
 *
 *
 * test_mod_httapi.c -- Tests for mod_httapi
 *
 * Boots a local HTTP server and drives the httapi application through the
 * loopback endpoint, verifying that the User-Agent header configured in the
 * httapi profile is sent on each request - including after the XML tree is
 * rebuilt by switch_xml_reload(), which frees the tree the profile pointers
 * were captured from.
 *
 */

#include <switch.h>
#include <test/switch_test.h>

#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define MOCK_HOST "127.0.0.1"
#define MOCK_PORT 38180
#define EXPECTED_UA "fst-httapi-ua/1.2.3"

static pthread_mutex_t ua_mutex = PTHREAD_MUTEX_INITIALIZER;
static char recorded_ua[512];
static int ua_recorded;
static pthread_t server_thread;
static int server_started;

static void record_ua(const char *ua)
{
	pthread_mutex_lock(&ua_mutex);
	switch_copy_string(recorded_ua, ua ? ua : "", sizeof(recorded_ua));
	recorded_ua[sizeof(recorded_ua) - 1] = '\0';
	ua_recorded = 1;
	pthread_mutex_unlock(&ua_mutex);
}

static void reset_ua(void)
{
	pthread_mutex_lock(&ua_mutex);
	recorded_ua[0] = '\0';
	ua_recorded = 0;
	pthread_mutex_unlock(&ua_mutex);
}

static int wait_ua(int timeout_ms)
{
	int waited = 0;

	while (waited < timeout_ms) {
		int got;
		pthread_mutex_lock(&ua_mutex);
		got = ua_recorded;
		pthread_mutex_unlock(&ua_mutex);
		if (got) {
			return 1;
		}
		switch_sleep(50 * 1000);
		waited += 50;
	}

	return 0;
}

/* Extract the value of a header (case insensitive) from a raw HTTP request */
static void extract_header(const char *buf, const char *name, char *out, size_t outlen)
{
	size_t nlen = strlen(name);
	const char *p = buf;

	out[0] = '\0';

	while ((p = strstr(p, "\r\n")) != NULL) {
		p += 2;
		if (!strncasecmp(p, name, nlen) && p[nlen] == ':') {
			const char *v = p + nlen + 1;
			const char *e;
			while (*v == ' ' || *v == '\t') {
				v++;
			}
			e = strchr(v, '\r');
			if (!e) {
				e = v + strlen(v);
			}
			if ((size_t)(e - v) >= outlen) {
				e = v + outlen - 1;
			}
			memcpy(out, v, e - v);
			out[e - v] = '\0';
			return;
		}
	}
}

static void *mock_server_thread(void *arg)
{
	static const char *resp_body =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<document type=\"freeswitch/httapi\">\n"
		"  <work>\n"
		"    <hangup/>\n"
		"  </work>\n"
		"</document>\n";
	int sfd = -1;
	struct sockaddr_in addr;
	int one = 1;
	char header[16384];
	ssize_t r;

	(void)arg;

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return NULL;
	}

	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(MOCK_PORT);

	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(sfd, 8) < 0) {
		close(sfd);
		return NULL;
	}

	for (;;) {
		int cfd;
		size_t total = 0;
		char *end;
		char resp[1024];

		if ((cfd = accept(sfd, NULL, NULL)) < 0) {
			continue;
		}

		/* read until end of headers */
		while (total < sizeof(header) - 1) {
			r = recv(cfd, header + total, 1, 0);
			if (r <= 0) {
				break;
			}
			total += r;
			header[total] = '\0';
			if (total >= 4 && (end = strstr(header, "\r\n\r\n")) != NULL) {
				/* consume any remaining body */
				int clen = 0;
				char clenbuf[64];

				extract_header(header, "Content-Length", clenbuf, sizeof(clenbuf));
				if (*clenbuf) {
					clen = atoi(clenbuf);
				}
				{
					size_t body_read = total - (size_t)(end + 4 - header);
					char discard[4096];
					while ((ssize_t)body_read < clen && (r = recv(cfd, discard, sizeof(discard), 0)) > 0) {
						body_read += r;
					}
				}

				{
					char ua[512];

					extract_header(header, "User-Agent", ua, sizeof(ua));
					record_ua(ua);
				}

				snprintf(resp, sizeof(resp),
						 "HTTP/1.1 200 OK\r\n"
						 "Content-Type: text/xml\r\n"
						 "Content-Length: %d\r\n"
						 "Connection: close\r\n"
						 "\r\n%s", (int)strlen(resp_body), resp_body);
				send(cfd, resp, strlen(resp), 0);
				break;
			}
		}

		close(cfd);
	}
}

FST_CORE_BEGIN("./conf_httapi")
	{
		FST_SUITE_BEGIN(test_mod_httapi)
		{
			FST_SETUP_BEGIN()
			{
				fst_requires_module("mod_httapi");
				fst_requires_module("mod_loopback");
				if (!server_started) {
					server_started = 1;
					if (pthread_create(&server_thread, NULL, mock_server_thread, NULL) == 0) {
						pthread_detach(server_thread);
					}
				}
				reset_ua();
			}
			FST_SETUP_END()

			FST_TEARDOWN_BEGIN()
			{
			}
			FST_TEARDOWN_END()

			/* the user-agent configured in the httapi profile must be sent on requests */
			FST_TEST_BEGIN(httapi_profile_user_agent_sent)
			{
				switch_core_session_t *session = NULL;
				switch_call_cause_t cause = SWITCH_CAUSE_NONE;

				/* the call completes via the httapi <hangup/> response, so expect NORMAL_CLEARING */
				switch_ivr_originate(NULL, &session, &cause, "loopback/1234", 30, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
				fst_check(cause == SWITCH_CAUSE_NORMAL_CLEARING);
				if (session) {
					switch_core_session_rwunlock(session);
				}

				fst_check(wait_ua(15000));
				fst_check(strcmp(recorded_ua, EXPECTED_UA) == 0);
			}
			FST_TEST_END()

			/* the same must hold after switch_xml_reload() frees the XML tree the
			   profile user-agent string was captured from */
			FST_TEST_BEGIN(httapi_profile_user_agent_survives_xml_reload)
			{
				switch_core_session_t *session = NULL;
				switch_call_cause_t cause = SWITCH_CAUSE_NONE;
				switch_status_t status;
				const char *reload_err = NULL;

				status = switch_xml_reload(&reload_err);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				reset_ua();

				/* the call completes via the httapi <hangup/> response, so expect NORMAL_CLEARING */
				switch_ivr_originate(NULL, &session, &cause, "loopback/1234", 30, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
				fst_check(cause == SWITCH_CAUSE_NORMAL_CLEARING);
				if (session) {
					switch_core_session_rwunlock(session);
				}

				fst_check(wait_ua(15000));
				fst_check(strcmp(recorded_ua, EXPECTED_UA) == 0);
			}
			FST_TEST_END()
		}
		FST_SUITE_END()
	}
	FST_CORE_END()

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
