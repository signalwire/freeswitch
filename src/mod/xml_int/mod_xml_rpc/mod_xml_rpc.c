/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_xml_rpc.c -- XML RPC
 *
 */
#include <switch.h>
#include <switch_version.h>
#ifdef _MSC_VER
#pragma warning(disable:4142)
#endif

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>
#include "../../libs/xmlrpc-c/lib/abyss/src/token.h"
#include "http.h"
#include "session.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_rpc_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_rpc_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_xml_rpc_runtime);
SWITCH_MODULE_DEFINITION(mod_xml_rpc, mod_xml_rpc_load, mod_xml_rpc_shutdown, mod_xml_rpc_runtime);

static abyss_bool HTTPWrite(TSession * s, char *buffer, uint32_t len);

static struct {
	uint16_t port;
	uint8_t running;
	char *realm;
	char *user;
	char *pass;
	TServer abyssServer;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_realm, globals.realm);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_user, globals.user);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_pass, globals.pass);

static switch_status_t do_config(void)
{
	char *cf = "xml_rpc.conf";
	switch_xml_t cfg, xml, settings, param;
	char *realm, *user, *pass;

	realm = user = pass = NULL;
	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "auth-realm")) {
				realm = val;
			} else if (!strcasecmp(var, "auth-user")) {
				user = val;
			} else if (!strcasecmp(var, "auth-pass")) {
				pass = val;
			} else if (!strcasecmp(var, "http-port")) {
				globals.port = (uint16_t) atoi(val);
			}
		}
	}

	if (!globals.port) {
		globals.port = 8080;
	}
	if (user && pass && realm) {
		set_global_realm(realm);
		set_global_user(user);
		set_global_pass(pass);
	}
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_rpc_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	do_config();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t http_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen)
{
	TSession *r = handle->data;

	return HTTPWrite(r, (char *) data, (uint32_t) datalen) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;

}

static switch_status_t http_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	TSession *r = handle->data;
	int ret = 0;
	char *data;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		ret = 0;
		HTTPWrite(r, data, (uint32_t) strlen(data));
		free(data);
	}

	return ret ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

static abyss_bool http_directory_auth(TSession * r, char *domain_name)
{
	char *p;
	char *x;
	char z[256], t[80];
	char user[512];
	char *pass;
	const char *mypass1 = NULL, *mypass2 = NULL;
	switch_xml_t x_domain, x_domain_root = NULL, x_user, x_params, x_param;
	const char *box;
	int at = 0;
	switch_event_t *params = NULL;

	p = RequestHeaderValue(r, "authorization");

	if (p) {
		NextToken((const char **const) &p);
		x = GetToken(&p);
		if (x) {
			if (!strcasecmp(x, "basic")) {


				NextToken((const char **const) &p);
				switch_b64_decode(p, user, sizeof(user));
				if ((pass = strchr(user, ':'))) {
					*pass++ = '\0';
				}

				if (!domain_name) {
					if ((domain_name = strchr(user, '@'))) {
						*domain_name++ = '\0';
						at++;
					} else {
						domain_name = (char *) r->requestInfo.host;
						if (!strncasecmp(domain_name, "www.", 3)) {
							domain_name += 4;
						}
					}
				}

				if (!domain_name) {
					goto fail;
				}

				switch_snprintf(z, sizeof(z), "%s:%s", globals.user, globals.pass);
				Base64Encode(z, t);

				if (!strcmp(p, t)) {
					r->requestInfo.user = strdup(user);
					goto authed;
				}

				switch_event_create(&params, SWITCH_EVENT_MESSAGE);
				switch_assert(params);
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "mailbox", "check");

				if (switch_xml_locate_user("id", user, domain_name, NULL, &x_domain_root, &x_domain, &x_user, params) != SWITCH_STATUS_SUCCESS) {
					switch_event_destroy(&params);
					goto fail;
				}

				switch_event_destroy(&params);
				box = switch_xml_attr_soft(x_user, "mailbox");

				for (x_param = switch_xml_child(x_domain, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "password")) {
						mypass1 = val;
					} else if (!strcasecmp(var, "vm-password")) {
						mypass2 = val;
					} else if (!strncasecmp(var, "http-", 5)) {
						ResponseAddField(r, (char *) var, (char *) val);
					}
				}

				if (!(x_params = switch_xml_child(x_user, "params"))) {
					goto authed;
				}

				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "password")) {
						mypass1 = val;
					} else if (!strcasecmp(var, "vm-password")) {
						mypass2 = val;
					} else if (!strncasecmp(var, "http-", 5)) {
						ResponseAddField(r, (char *) var, (char *) val);
					}
				}

				if (!(mypass1 && mypass2)) {
					r->requestInfo.user = strdup(user);
					goto authed;
				} else {
					if (mypass1) {
						if (at) {
							switch_snprintf(z, sizeof(z), "%s@%s:%s", user, domain_name, mypass1);
						} else {
							switch_snprintf(z, sizeof(z), "%s:%s", user, mypass1);
						}
						Base64Encode(z, t);

						if (!strcmp(p, t)) {
							r->requestInfo.user = strdup(box ? box : user);
							goto authed;
						}
					}

					if (mypass2) {
						if (at) {
							switch_snprintf(z, sizeof(z), "%s@%s:%s", user, domain_name, mypass2);
						} else {
							switch_snprintf(z, sizeof(z), "%s:%s", user, mypass2);
						}
						Base64Encode(z, t);

						if (!strcmp(p, t)) {
							r->requestInfo.user = strdup(box ? box : user);
							goto authed;
						}
					}

					if (box) {
						if (mypass1) {
							if (at) {
								switch_snprintf(z, sizeof(z), "%s@%s:%s", box, domain_name, mypass1);
							} else {
								switch_snprintf(z, sizeof(z), "%s:%s", box, mypass1);
							}
							Base64Encode(z, t);

							if (!strcmp(p, t)) {
								r->requestInfo.user = strdup(box);
								goto authed;
							}
						}

						if (mypass2) {
							if (at) {
								switch_snprintf(z, sizeof(z), "%s@%s:%s", box, domain_name, mypass2);
							} else {
								switch_snprintf(z, sizeof(z), "%s:%s", box, mypass2);
							}

							Base64Encode(z, t);

							if (!strcmp(p, t)) {
								r->requestInfo.user = strdup(box);
								goto authed;
							}
						}
					}
				}
				goto fail;

			  authed:

				ResponseAddField(r, "freeswitch-user", r->requestInfo.user);
				ResponseAddField(r, "freeswitch-domain", domain_name);

				if (x_domain_root) {
					switch_xml_free(x_domain_root);
				}

				return TRUE;
			}
		}
	}

  fail:

	if (x_domain_root) {
		switch_xml_free(x_domain_root);
	}

	switch_snprintf(z, sizeof(z), "Basic realm=\"%s\"", domain_name ? domain_name : globals.realm);
	ResponseAddField(r, "WWW-Authenticate", z);
	ResponseStatus(r, 401);
	return FALSE;
}

abyss_bool auth_hook(TSession * r)
{
	char *domain_name, *e;
	abyss_bool ret = FALSE;

	if (!strncmp(r->requestInfo.uri, "/domains/", 9)) {
		domain_name = strdup(r->requestInfo.uri + 9);
		switch_assert(domain_name);

		if ((e = strchr(domain_name, '/'))) {
			*e++ = '\0';
		}

		if (!strcmp(domain_name, "this")) {
			free(domain_name);
			domain_name = strdup(r->requestInfo.host);
		}

		ret = !http_directory_auth(r, domain_name);

		free(domain_name);
	} else {
		char tmp[512];
		const char *list[2] = { "index.html", "index.txt" };
		int x;

		if (!strncmp(r->requestInfo.uri, "/pub", 4)) {
			char *p = (char *) r->requestInfo.uri;
			char *new_uri = p + 4;
			if (!new_uri) {
				new_uri = "/";
			}

			switch_snprintf(tmp, sizeof(tmp), "%s%s", SWITCH_GLOBAL_dirs.htdocs_dir, new_uri);

			if (switch_directory_exists(tmp, NULL) == SWITCH_STATUS_SUCCESS) {
				for (x = 0; x < 2; x++) {
					switch_snprintf(tmp, sizeof(tmp), "%s%s%s%s",
									SWITCH_GLOBAL_dirs.htdocs_dir, new_uri, end_of(new_uri) == *SWITCH_PATH_SEPARATOR ? "" : SWITCH_PATH_SEPARATOR, list[x]
						);

					if (switch_file_exists(tmp, NULL) == SWITCH_STATUS_SUCCESS) {
						switch_snprintf(tmp, sizeof(tmp), "%s%s%s", new_uri, end_of(new_uri) == '/' ? "" : "/", list[x]
							);
						new_uri = tmp;
						break;
					}
				}
			}

			r->requestInfo.uri = strdup(new_uri);
			free(p);

		} else {
			if (globals.realm && strncmp(r->requestInfo.uri, "/pub", 4)) {
				ret = !http_directory_auth(r, NULL);
			}
		}
	}
	return ret;
}


static abyss_bool HTTPWrite(TSession * s, char *buffer, uint32_t len)
{
	if (s->chunkedwrite && s->chunkedwritemode) {
		char t[16];

		if (ConnWrite(s->conn, t, sprintf(t, "%x" CRLF, len)))
			if (ConnWrite(s->conn, buffer, len))
				return ConnWrite(s->conn, CRLF, 2);

		return FALSE;
	}

	return ConnWrite(s->conn, buffer, len);
}

static abyss_bool HTTPWriteEnd(TSession * s)
{
	if (!s->chunkedwritemode)
		return TRUE;

	if (s->chunkedwrite) {
		/* May be one day trailer dumping will be added */
		s->chunkedwritemode = FALSE;
		return ConnWrite(s->conn, "0" CRLF CRLF, 5);
	}

	s->requestInfo.keepalive = FALSE;
	return TRUE;
}

abyss_bool handler_hook(TSession * r)
{
	//char *mime = "text/html";
	char buf[80] = "HTTP/1.1 200 OK\n";
	switch_stream_handle_t stream = { 0 };
	char *command;
	int i, j = 0;
	TTableItem *ti;
	char *dup = NULL;
	int auth = 0;
	char *fs_user = NULL, *fs_domain = NULL;
	char *path_info = NULL;
	abyss_bool ret = TRUE;
	int html = 0;

	stream.data = r;
	stream.write_function = http_stream_write;
	stream.raw_write_function = http_stream_raw_write;

	if (!r || !r->requestInfo.uri) {
		return FALSE;
	}

	if ((command = strstr(r->requestInfo.uri, "/api/"))) {
		command += 5;
	} else if ((command = strstr(r->requestInfo.uri, "/webapi/"))) {
		command += 8;
		html++;
	} else {
		return FALSE;
	}

	if ((path_info = strchr(command, '/'))) {
		*path_info++ = '\0';
	}

	for (i = 0; i < r->response_headers.size; i++) {
		ti = &r->response_headers.item[i];
		if (!strcasecmp(ti->name, "freeswitch-user")) {
			fs_user = ti->value;
		} else if (!strcasecmp(ti->name, "freeswitch-domain")) {
			fs_domain = ti->value;
		} else if (!strcasecmp(ti->name, "http-allowed-api")) {
			int argc, x;
			char *argv[256] = { 0 };
			j++;

			if (!strcasecmp(ti->value, "any")) {
				auth++;
			}

			dup = strdup(ti->value);
			argc = switch_separate_string(dup, ',', argv, (sizeof(argv) / sizeof(argv[0])));

			for (x = 0; x < argc; x++) {
				if (!strcasecmp(command, argv[x])) {
					auth++;
				}
			}
		}
	}

	if (!fs_user || !strcmp(fs_user, globals.user)) {
		auth = 1;
	} else {
		if (!j) {
			auth = 0;
		}
	}

	if (auth) {
		goto auth;
	}
	//unauth:
	ResponseStatus(r, 403);
	ResponseError(r);
	switch_safe_free(dup);

	ret = TRUE;
	goto end;

  auth:

	if (switch_event_create(&stream.param_event, SWITCH_EVENT_API) == SWITCH_STATUS_SUCCESS) {
		const char *const content_length = RequestHeaderValue(r, "content-length");

		if (fs_user)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "FreeSWITCH-User", "%s", fs_user);
		if (fs_domain)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "FreeSWITCH-Domain", "%s", fs_domain);
		if (path_info)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-Path-Info", "%s", path_info);
		switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-URI", "%s", r->requestInfo.uri);
		if (r->requestInfo.query)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-QUERY", "%s", r->requestInfo.query);
		if (r->requestInfo.host)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-HOST", "%s", r->requestInfo.host);
		if (r->requestInfo.from)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-FROM", "%s", r->requestInfo.from);
		if (r->requestInfo.useragent)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-USER-AGENT", "%s", r->requestInfo.useragent);
		if (r->requestInfo.referer)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-REFERER", "%s", r->requestInfo.referer);
		if (r->requestInfo.requestline)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-REQUESTLINE", "%s", r->requestInfo.requestline);
		if (r->requestInfo.user)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-USER", "%s", r->requestInfo.user);
		if (r->requestInfo.port)
			switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-PORT", "%u", r->requestInfo.port);
		if (r->requestInfo.query || content_length) {
			char *q, *qd;
			char *next;
			char *query = (char *) r->requestInfo.query;
			char *name, *val;
			char qbuf[8192] = "";

			if (r->requestInfo.method == m_post && content_length) {
				int len = atoi(content_length);
				int qlen = 0;

				if (len > 0) {
					int succeeded;
					char *qp = qbuf;
					do {
						int blen = r->conn->buffersize - r->conn->bufferpos;

						if ((qlen + blen) > len) {
							blen = len - qlen;
						}

						qlen += blen;

						if (qlen > sizeof(qbuf)) {
							break;
						}

						memcpy(qp, r->conn->buffer + r->conn->bufferpos, blen);
						qp += blen;

						if (qlen >= len) {
							break;
						}
					} while ((succeeded = ConnRead(r->conn, 2000)));

					query = qbuf;
				}
			}
			if (query) {
				switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, "HTTP-QUERY", "%s", query);

				qd = strdup(query);
				switch_assert(qd != NULL);

				q = qd;
				next = q;

				do {
					char *p;

					if ((next = strchr(next, '&'))) {
						*next++ = '\0';
					}

					for (p = q; p && *p; p++) {
						if (*p == '+') {
							*p = ' ';
						}
					}

					switch_url_decode(q);

					name = q;
					if ((val = strchr(name, '='))) {
						*val++ = '\0';
						switch_event_add_header(stream.param_event, SWITCH_STACK_BOTTOM, name, "%s", val);
					}
					q = next;
				} while (q != NULL);

				free(qd);
			}
		}
	}
	//ResponseChunked(r);

	//ResponseContentType(r, mime);
	//ResponseWrite(r);

	HTTPWrite(r, buf, (uint32_t) strlen(buf));

	//HTTPWrite(r, "<pre>\n\n", 7);

	/* generation of the date field */
	{
		const char *dateValue;

		DateToString(r->date, &dateValue);

		if (dateValue) {
			ResponseAddField(r, "Date", dateValue);
		}
	}


	/* Generation of the server field */
	ResponseAddField(r, "Server", "FreeSWITCH-" SWITCH_VERSION_FULL "-mod_xml_rpc");

	if (html) {
		ResponseAddField(r, "Content-Type", "text/html");
	}

	for (i = 0; i < r->response_headers.size; i++) {
		ti = &r->response_headers.item[i];
		ConnWrite(r->conn, ti->name, (uint32_t) strlen(ti->name));
		ConnWrite(r->conn, ": ", 2);
		ConnWrite(r->conn, ti->value, (uint32_t) strlen(ti->value));
		ConnWrite(r->conn, CRLF, 2);
	}

	switch_snprintf(buf, sizeof(buf), "Connection: close\r\n");
	ConnWrite(r->conn, buf, (uint32_t) strlen(buf));

	if (html) {
		ConnWrite(r->conn, "\r\n", 2);
	}


	if (switch_api_execute(command, r->requestInfo.query, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		ResponseStatus(r, 200);
		r->responseStarted = TRUE;
		//r->done = TRUE;
	} else {
		ResponseStatus(r, 404);
		ResponseError(r);
	}

	//SocketClose(&(r->conn->socket));

	HTTPWriteEnd(r);
	//if(r->conn->channelP)
	//ConnKill(r->conn);
	//ChannelInterrupt(r->conn->channelP);
	//ConnClose(r->conn);
	//ChannelDestroy(r->conn->channelP);
	r->requestInfo.keepalive = 0;

  end:

	return ret;
}

static xmlrpc_value *freeswitch_api(xmlrpc_env * const envP, xmlrpc_value * const paramArrayP, void *const userData)
{
	char *command = NULL, *arg = NULL;
	switch_stream_handle_t stream = { 0 };
	xmlrpc_value *val = NULL;


	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ss)", &command, &arg);
	if (envP->fault_occurred) {
		goto done;
	}

	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute(command, arg, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		/* Return our result. */
		val = xmlrpc_build_value(envP, "s", stream.data);
		free(stream.data);
	} else {
		val = xmlrpc_build_value(envP, "s", "ERROR!");
	}

  done:
	/* xmlrpc-c requires us to free memory it malloced from xmlrpc_decompose_value */
	switch_safe_free(command);
	switch_safe_free(arg);
	return val;
}

static xmlrpc_value *freeswitch_man(xmlrpc_env * const envP, xmlrpc_value * const paramArrayP, void *const userData)
{
	char *oid = NULL, *relative_oid, *s_action = NULL, *data = NULL;
	char buf[SWITCH_MAX_MANAGEMENT_BUFFER_LEN] = "";
	switch_management_action_t action = SMA_NONE;
	xmlrpc_value *val = NULL;

	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(sss)", &oid, &s_action, &data);
	if (envP->fault_occurred) {
		goto done;
	}

	if (!strncasecmp(oid, FREESWITCH_OID_PREFIX, strlen(FREESWITCH_OID_PREFIX))) {
		relative_oid = oid + strlen(FREESWITCH_OID_PREFIX);
	} else {
		relative_oid = oid;
	}

	if (!switch_strlen_zero(data)) {
		switch_copy_string(buf, data, sizeof(buf));
	}

	if (!strcasecmp(s_action, "get")) {
		action = SMA_GET;
	} else if (!strcasecmp(s_action, "set")) {
		action = SMA_SET;
	}

	if (action) {
		if (switch_core_management_exec(relative_oid, action, buf, sizeof(buf)) == SWITCH_STATUS_SUCCESS) {
			if (action == SMA_SET) {
				if (*buf != '\0') {
					switch_snprintf(buf, sizeof(buf), "OK\n");
				}
			}
		} else {
			if (*buf != '\0') {
				switch_snprintf(buf, sizeof(buf), "ERROR\n");
			}
		}
	} else {
		switch_snprintf(buf, sizeof(buf), "Invalid Action %s\n", s_action);
	}

	/* Return our result. */
	val = xmlrpc_build_value(envP, "s", buf);

  done:
	/* xmlrpc-c requires us to free memory it malloced from xmlrpc_decompose_value */
	switch_safe_free(oid);
	switch_safe_free(s_action);
	switch_safe_free(data);
	return val;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_xml_rpc_runtime)
{
	xmlrpc_registry *registryP;
	xmlrpc_env env;
	char logfile[512];
	switch_hash_index_t *hi;
	const void *var;
	void *val;

	globals.running = 1;

	xmlrpc_env_init(&env);

	registryP = xmlrpc_registry_new(&env);

	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch.api", &freeswitch_api, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch_api", &freeswitch_api, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch.management", &freeswitch_man, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch_management", &freeswitch_man, NULL);

	MIMETypeInit();
	MIMETypeAdd("text/html", "html");
	for (hi = switch_core_mime_index(); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		if (var && val) {
			MIMETypeAdd((char *) val, (char *) var);
		}
	}

	switch_snprintf(logfile, sizeof(logfile), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, "freeswitch_http.log");
	ServerCreate(&globals.abyssServer, "XmlRpcServer", globals.port, SWITCH_GLOBAL_dirs.htdocs_dir, logfile);

	xmlrpc_server_abyss_set_handler(&env, &globals.abyssServer, "/RPC2", registryP);
	ServerInit(&globals.abyssServer);

#if 0
	if (ServerInit(&globals.abyssServer) != TRUE) {
		globals.running = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to start HTTP Port %d\n", globals.port);
		return SWITCH_STATUS_TERM;
	}
#endif

	ServerAddHandler(&globals.abyssServer, handler_hook);
	ServerAddHandler(&globals.abyssServer, auth_hook);
	ServerSetKeepaliveTimeout(&globals.abyssServer, 1);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting HTTP Port %d, DocRoot [%s]\n", globals.port, SWITCH_GLOBAL_dirs.htdocs_dir);
	ServerRun(&globals.abyssServer);
	globals.running = 0;

	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_rpc_shutdown)
{
	//globals.abyssServer.running = 0;
	//shutdown(globals.abyssServer.listensock, 2);
	ServerTerminate(&globals.abyssServer);
	while (globals.running) {
		switch_yield(100000);
	}

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
