/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * John Wehle <john@feith.com>
 * Garmt Boekholt <garmt@cimico.com>
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_xml_rpc.c -- XML RPC
 *
 * embedded webserver for FS
 * exposes fs api to web (and ajax, javascript, ...)
 * supports GET/POST requests (as defined below)
 * and similar XMLRPC format /RPC2 freeswitch.api and management.api
 *
 * usage:
 * (1) http:/host:port/[txt|web|xml]api/fsapicommand[?arg[ arg]*][ &key=value[+&key=value]*]
 *     e.g.  http:/host:port/api/show?calls &refresh=5+&weather=nice
 * (2) http:/host:port/filepath - serves files from conf/htdocs
 *
 * NB:
 * ad (1) - key/value pairs are propagated as event headers
 *        - if &key=value is "&refresh=xx" - special feature: automatic refresh after xx sec (triggered by browser)
 *          note that refresh works only
 *                IF response content-type: text/html (i.e. "webapi" or "api")
 *                AND fs api command created an event header HTTP-REFRESH before the first write_stream
 *                NOTE if "api", fs api command has to overwrite content-type to be text/html i.s.o. text/plain (default)
 *
 *        - if api format is "api" mod_xml_rpc will automatically assume plain/text (so webunaware fs commands are rendered apropriately)
 *        - xmlapi doesn't seem to be used, however if a fs api command renders xml, you can set the format type to xml
 *          txtapi-text/plain or webapi-text/html surround xml with <pre> xml <pre/>
 *        - typically fs api command arguments are encoded with UrlPathEncode (spaces -> %20), and k/v pairs are urlencoded (space -> +)
 *
 * ad (2)   ms ie may show extra empty lines when serving large txt files as ie has problems rendering content-type "plain/text"
 *
 */
#include <switch.h>
#ifdef _MSC_VER
#pragma warning(disable:4142)
#endif

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>
#include <xmlrpc-c/base64_int.h>
#include <../lib/abyss/src/token.h>
#include <../lib/abyss/src/http.h>
#include <../lib/abyss/src/session.h>
#include "ws.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_rpc_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_rpc_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_xml_rpc_runtime);
SWITCH_MODULE_DEFINITION(mod_xml_rpc, mod_xml_rpc_load, mod_xml_rpc_shutdown, mod_xml_rpc_runtime);

static abyss_bool HTTPWrite(TSession * s, const char *buffer, const uint32_t len);

static struct {
	uint16_t port;
	uint8_t running;
	char *realm;
	char *user;
	char *pass;
	char *default_domain;
	switch_bool_t virtual_host;
	TServer abyssServer;
	xmlrpc_registry *registryP;
	switch_bool_t enable_websocket;
	char *commands_to_log;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_realm, globals.realm);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_user, globals.user);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_pass, globals.pass);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_default_domain, globals.default_domain);

static switch_status_t do_config(void)
{
	char *cf = "xml_rpc.conf";
	switch_xml_t cfg, xml, settings, param;
	char *realm, *user, *pass, *default_domain;

	default_domain = realm = user = pass = NULL;
	globals.commands_to_log = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	globals.virtual_host = SWITCH_TRUE;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!zstr(var) && !zstr(val)) {
				if (!strcasecmp(var, "auth-realm")) {
					realm = val;
				} else if (!strcasecmp(var, "auth-user")) {
					user = val;
				} else if (!strcasecmp(var, "auth-pass")) {
					pass = val;
				} else if (!strcasecmp(var, "http-port")) {
					globals.port = (uint16_t) atoi(val);
				} else if (!strcasecmp(var, "default-domain")) {
					default_domain = val;
				} else if (!strcasecmp(var, "virtual-host")) {
					globals.virtual_host = switch_true(val);
				} else if (!strcasecmp(var, "enable-websocket")) {
					globals.enable_websocket = switch_true(val);
				} else if (!strcasecmp(var, "commands-to-log")) {
					globals.commands_to_log = val;
				}
			}
		}
	}

	if (!globals.port) {
		globals.port = 8080;
	}
	if (realm) {
		set_global_realm(realm);
		if (user && pass) {
			set_global_user(user);
			set_global_pass(pass);
		}
	}
	if (default_domain) {
		set_global_default_domain(default_domain);
	}
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_rpc_load)
{

	if (switch_event_reserve_subclass("websocket::stophook") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", "websocket::stophook");
		return SWITCH_STATUS_TERM;
	}
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	do_config();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t http_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen)
{
	TSession *r = (TSession *) handle->data;

	return HTTPWrite(r, (char *) data, (uint32_t) datalen) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;

}

static switch_status_t http_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	TSession *r = (TSession *) handle->data;
	int ret = 1;
	char *data;
	switch_event_t *evnt = handle->param_event;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		/* Stream Content-Type (http header) to the xmlrpc (web) client, if fs api command did not do it yet.        */
		/* If (Content-Type in event) then the header was already replied.                                           */
		/* If fs api command is not "web aware", this will set the Content-Type to "text/plain".                     */
		const char *http_refresh = NULL;
		const char *ct = NULL;
		const char *refresh = NULL;
		if (evnt && !(ct = switch_event_get_header(evnt, "Content-Type"))){
			const char *val = switch_stristr("Content-Type", data);
			if (!val) {
				val = "Content-Type: text/plain\r\n\r\n";
				ret = HTTPWrite(r, val, (uint32_t) strlen(val));
			}
			/* flag to prevent running this more than once per http reply  */
			switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "Content-Type", strstr(val,":")+2);
			ct = switch_event_get_header(evnt, "Content-Type");
		}

		if (ret) {
			ret = HTTPWrite(r, data, (uint32_t) strlen(data));
		}
		switch_safe_free(data);

		/* e.g. "http://www.cluecon.fs/api/show?calls &refresh=5"  */
		/* fs api command can set event header "HTTP-REFRESH" so that the web page will automagically refresh, if    */
		/* "refresh=xxx" was part of the http query kv pairs */
		if (ret && ct && *ct && (http_refresh = switch_event_get_header(evnt, "HTTP-REFRESH"))
			                 && (refresh = switch_event_get_header(evnt, "refresh"))
			                 && !strstr("text/html", ct)
							 && (atoi(refresh) > 0 )) {
			const char *query = switch_event_get_header(evnt, "HTTP-QUERY");
			const char *uri = switch_event_get_header(evnt, "HTTP-URI");
			if (uri && query && *uri && *query) {
				char *buf = switch_mprintf("<META HTTP-EQUIV=REFRESH CONTENT=\"%s; URL=%s?%s\">\n", refresh, uri, query);
				ret = HTTPWrite(r, buf, (uint32_t) strlen(buf));
				switch_safe_free(buf);
			}
		}

		/* only one refresh meta header per reply */
		if (http_refresh) {
			switch_event_del_header(evnt, "HTTP-REFRESH");
		}
	}

	return ret ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

static abyss_bool user_attributes(const char *user, const char *domain_name,
								  const char **ppasswd, const char **pvm_passwd, const char **palias, const char **pallowed_commands)
{
	const char *passwd;
	const char *vm_passwd;
	const char *alias;
	const char *allowed_commands;
	switch_event_t *params;
	switch_xml_t x_user, x_params, x_param;

	passwd = NULL;
	vm_passwd = NULL;
	alias = NULL;
	allowed_commands = NULL;

	if (ppasswd) *ppasswd = NULL;
	if (pvm_passwd) *pvm_passwd = NULL;
	if (palias) *palias = NULL;
	if (pallowed_commands) *pallowed_commands = NULL;

	params = NULL;

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "number_alias", "check");


	if (switch_xml_locate_user_merged("id", user, domain_name, NULL, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&params);
		return FALSE;
	}

	switch_event_destroy(&params);
	alias = switch_xml_attr(x_user, "number-alias");

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr_soft(x_param, "value");

			if (!strcasecmp(var, "password")) {
				passwd = val;
			} else if (!strcasecmp(var, "vm-password")) {
				vm_passwd = val;
			} else if (!strcasecmp(var, "http-allowed-api")) {
				allowed_commands = val;
			}
		}
	}

	if (ppasswd && passwd) {
		*ppasswd = strdup(passwd);
	}

	if (pvm_passwd && vm_passwd) {
		*pvm_passwd = strdup(vm_passwd);
	}

	if (palias && alias) {
		*palias = strdup(alias);
	}

	if (pallowed_commands && allowed_commands) {
		*pallowed_commands = strdup(allowed_commands);
	}

	if (x_user) {
		switch_xml_free(x_user);
	}

	return TRUE;
}

static abyss_bool is_authorized(const TSession * r, const char *command)
{
	char *user = NULL, *domain_name = NULL;
	char *allowed_commands = NULL;
	char *dp;
	char *dup = NULL;
	char *argv[256] = { 0 };
	int argc = 0, i = 0, ok = 0;
	unsigned int err = 403;

	if (!r) {
		return FALSE;
	}

	if (zstr(globals.realm) && zstr(globals.user)) {
		return TRUE;
	}

	if (!r->requestInfo.user) {
		return FALSE;
	}

	user = strdup(r->requestInfo.user);

	if ((dp = strchr(user, '@'))) {
		*dp++ = '\0';
		domain_name = dp;
	}

	if (!zstr(globals.realm) && !zstr(globals.user) && !strcmp(user, globals.user)) {
		ok = 1;
		goto end;
	}

	if (zstr(user) || zstr(domain_name)) {
		goto end;
	}


	err = 686;

	if (!user_attributes(user, domain_name, NULL, NULL, NULL, &allowed_commands)) {
		goto end;
	}

	switch_safe_free(user);

	if (!allowed_commands) {
		goto end;
	}

	if ((dup = allowed_commands)) {
		argc = switch_separate_string(dup, ',', argv, (sizeof(argv) / sizeof(argv[0])));

		for (i = 0; i < argc && argv[i]; i++) {
			if (!strcasecmp(argv[i], command) || !strcasecmp(argv[i], "any")) {
				ok = 1;
				break;
			}
		}
	}

 end:

	switch_safe_free(user);
	switch_safe_free(dup);

	if (!ok) {
		ResponseStatus(r, (xmlrpc_uint16_t)err);
	}


	return ok ? TRUE : FALSE;
}

static abyss_bool http_directory_auth(TSession *r, char *domain_name)
{
	char *p = NULL;
	char *x = NULL;
	char z[256] = "", t[80] = "";
	char user[512] = "" ;
	char *pass = NULL;
	const char *mypass1 = NULL, *mypass2 = NULL;
	const char *box = NULL;
	int at = 0;
	char *dp = NULL;
	abyss_bool rval = FALSE;
	char *dup_domain = NULL;

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

				if ((dp = strchr(user, '@'))) {
					*dp++ = '\0';
					domain_name = dp;
					at++;
				}

				if (!domain_name) {
					if (globals.virtual_host) {
						if ((domain_name = (char *) r->requestInfo.host)) {
							if (!strncasecmp(domain_name, "www.", 3)) {
								domain_name += 4;
							}
						}
					}
					if (!domain_name) {
						if (globals.default_domain) {
							domain_name = globals.default_domain;
						} else {
							if ((dup_domain = switch_core_get_domain(SWITCH_TRUE))) {
								domain_name = dup_domain;
							}
						}
					}
				}

				if (zstr(user) || zstr(domain_name)) {
					goto fail;
				}

				if (!zstr(globals.realm) && !zstr(globals.user) && !zstr(globals.pass)) {
					if (at) {
						switch_snprintf(z, sizeof(z), "%s@%s:%s", globals.user, globals.realm, globals.pass);
					} else {
						switch_snprintf(z, sizeof(z), "%s:%s", globals.user, globals.pass);
					}
					xmlrpc_base64Encode(z, t);

					if (!strcmp(p, t)) {
						goto authed;
					}
				}

				if (!user_attributes(user, domain_name, &mypass1, &mypass2, &box, NULL)) {
					goto fail;
				}


				if (!zstr(mypass2) && !strcasecmp(mypass2, "user-choose")) {
					switch_safe_free(mypass2);
				}

				if (!mypass1) {
					goto authed;
				} else {
					if (at) {
						switch_snprintf(z, sizeof(z), "%s@%s:%s", user, domain_name, mypass1);
					} else {
						switch_snprintf(z, sizeof(z), "%s:%s", user, mypass1);
					}
					xmlrpc_base64Encode(z, t);

					if (!strcmp(p, t)) {
						goto authed;
					}

					if (mypass2) {
						if (at) {
							switch_snprintf(z, sizeof(z), "%s@%s:%s", user, domain_name, mypass2);
						} else {
							switch_snprintf(z, sizeof(z), "%s:%s", user, mypass2);
						}
						xmlrpc_base64Encode(z, t);

						if (!strcmp(p, t)) {
							goto authed;
						}
					}

					if (box) {
						if (at) {
							switch_snprintf(z, sizeof(z), "%s@%s:%s", box, domain_name, mypass1);
						} else {
							switch_snprintf(z, sizeof(z), "%s:%s", box, mypass1);
						}
						xmlrpc_base64Encode(z, t);

						if (!strcmp(p, t)) {
							goto authed;
						}

						if (mypass2) {
							if (at) {
								switch_snprintf(z, sizeof(z), "%s@%s:%s", box, domain_name, mypass2);
							} else {
								switch_snprintf(z, sizeof(z), "%s:%s", box, mypass2);
							}

							xmlrpc_base64Encode(z, t);

							if (!strcmp(p, t)) {
								goto authed;
							}
						}
					}
				}
				goto fail;

			  authed:

				switch_snprintf(z, sizeof(z), "%s@%s", (box ? box : user), domain_name);
				r->requestInfo.user = strdup(z);

				ResponseAddField(r, "freeswitch-user", (box ? box : user));
				ResponseAddField(r, "freeswitch-domain", domain_name);
				rval = TRUE;
				goto done;
			}
		}
	}

  fail:

	switch_snprintf(z, sizeof(z), "Basic realm=\"%s\"", domain_name ? domain_name : globals.realm);
	ResponseAddField(r, "WWW-Authenticate", z);
	ResponseStatus(r, 401);

  done:

	switch_safe_free(mypass1);
	switch_safe_free(mypass2);
	switch_safe_free(box);
	switch_safe_free(dup_domain);

	return rval;
}

void stop_hook_event_handler(switch_event_t *event) {
	wsh_t *wsh = (wsh_t *)event->bind_user_data;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "got websocket::stophook, closing\n");
	wsh->down++;
}

void event_handler(switch_event_t *event) {
	char *json;
	wsh_t *wsh = (wsh_t *)event->bind_user_data;
	switch_event_serialize_json(event, &json);
	ws_write_frame(wsh, WSOC_TEXT, json, strlen(json));
	free(json);
}

#define MAX_EVENT_BIND_SLOTS SWITCH_EVENT_ALL

abyss_bool websocket_hook(TSession *r)
{
	wsh_t *wsh;
	int ret;
	int i;
	ws_opcode_t opcode;
	uint8_t *data;
	switch_event_node_t *nodes[MAX_EVENT_BIND_SLOTS];
	int node_count = 0;
	char *p;
	char *key = NULL;
	char *version = NULL;
	char *proto = NULL;
	char *upgrade = NULL;

	for (i = 0; i < r->requestHeaderFields.size; i++) {
		TTableItem * const item = &r->requestHeaderFields.item[i];

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "headers %s: %s\n", item->name, item->value);
	}

	key = RequestHeaderValue(r, "sec-websocket-key");
	version = RequestHeaderValue(r, "sec-websocket-version");
	proto = RequestHeaderValue(r, "sec-websocket-protocol");
	upgrade = RequestHeaderValue(r, "upgrade");

	if (!key || !version || !proto || !upgrade) return FALSE;
	if (strncasecmp(upgrade, "websocket", 9) || strncasecmp(proto, "websocket", 9)) return FALSE;

	wsh = ws_init(r);
	if (!wsh) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "websocket memory error\n");
		return FALSE;
	}

	ret = ws_handshake_kvp(wsh, key, version, proto);
	if (ret < 0) wsh->down = 1;

	if (ret != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "handshake error %d\n", ret);
		return FALSE;
	}

	if (switch_event_bind_removable("websocket", SWITCH_EVENT_CUSTOM, "websocket::stophook", stop_hook_event_handler, wsh, &nodes[node_count++]) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't bind!\n");
		node_count--;
	}

	while (!wsh->down) {
		int bytes = ws_read_frame(wsh, &opcode, &data);

		if (bytes < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d %s\n", opcode, (char *)data);
			switch_yield(100000);
			continue;
		}

		switch (opcode) {
			case WSOC_CLOSE:
				ws_close(wsh, 1000);
				break;
			case WSOC_CONTINUATION:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "continue\n");
				continue;
			case WSOC_TEXT:
				p = data;
				if (!p) continue;
				if (!strncasecmp(data, "event ", 6)) {
					switch_event_types_t type;
					char *subclass;

					if (node_count == MAX_EVENT_BIND_SLOTS - 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot subscribe more than %d events\n", node_count);
						continue;
					}
					p += 6;
					if (p = strchr(p, ' ')) p++;
					if (!strncasecmp(p, "json ", 5)) {
						p += 5;
					} else if (!strncasecmp(p, "xml ", 4)) {
						p += 4;
					} else if (!strncasecmp(p, "plain ", 6)) {
						p += 6;
					}
					if (!*p) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing event type in [%s]\n", data);
						break;
					} else {
					}
					if (subclass = strchr(p, ' ')) {
						*subclass++ = '\0';
						if (!*subclass) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing subclass\n");
							continue;
						}
					} else {
						subclass = SWITCH_EVENT_SUBCLASS_ANY;
					}

					if (switch_name_event(p, &type) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown event %s\n", p);
						continue;
					}

					if (switch_event_bind_removable("websocket", type, subclass, event_handler, wsh, &nodes[node_count++]) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't bind!\n");
						node_count--;
						continue;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Bind %s\n", data);
					}

				}
				break;
			default:
				break;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "wsh->down = %d, node_count = %d\n", wsh->down, node_count);

	switch_yield(2000);
	while (--node_count >= 0) switch_event_unbind(&nodes[node_count]);

	switch_safe_free(wsh);

	return FALSE;
}

abyss_bool auth_hook(TSession * r)
{
	char *domain_name, *e;
	abyss_bool ret = FALSE;

	if (globals.enable_websocket && !strncmp(r->requestInfo.uri, "/socket", 7)) {
		// Chrome has no Authorization support yet
		// https://code.google.com/p/chromium/issues/detail?id=123862
		return websocket_hook(r);
	}

	if (!strncmp(r->requestInfo.uri, "/portal", 7) && strlen(r->requestInfo.uri) <= 8) {
		ResponseAddField(r, "Location", "/portal/index.html");
		ResponseStatus(r, 302);
		return TRUE;
	}

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


static abyss_bool HTTPWrite(TSession * s, const char *buffer, const uint32_t len)
{
	if (s->chunkedwrite && s->chunkedwritemode) {
		char t[16];

		if (ConnWrite(s->connP, t, sprintf(t, "%x" CRLF, len)))
			if (ConnWrite(s->connP, buffer, len))
				return ConnWrite(s->connP, CRLF, 2);

		return FALSE;
	}

	return ConnWrite(s->connP, buffer, len);
}

static abyss_bool HTTPWriteEnd(TSession * s)
{
	if (!s->chunkedwritemode)
		return TRUE;

	if (s->chunkedwrite) {
		/* May be one day trailer dumping will be added */
		s->chunkedwritemode = FALSE;
		return ConnWrite(s->connP, "0" CRLF CRLF, 5);
	}

	s->requestInfo.keepalive = FALSE;
	return TRUE;
}

abyss_bool handler_hook(TSession * r)
{
	switch_stream_handle_t stream = { 0 };
	char *command;
	char *full_command;
	int i;
	char *fs_user = NULL, *fs_domain = NULL;
	char *path_info = NULL;
	abyss_bool ret = TRUE;
	int html = 0, text = 0, xml = 0, api = 0;
	const char *api_str;
	const char *uri = 0;
	TRequestInfo *info = 0;
	switch_event_t *evnt = 0; /* shortcut to stream.param_event */
	char v[256] = "";

	if (!r || !(info = &r->requestInfo) || !(uri = info->uri)) {
		return FALSE;
	}

	stream.data = r;
	stream.write_function = http_stream_write;
	stream.raw_write_function = http_stream_raw_write;

	if ((command = strstr(uri, "/api/"))) {
		command += 5;
		api++;
	} else if ((command = strstr(uri, "/webapi/"))) {
		command += 8;
		html++;
	} else if ((command = strstr(uri, "/txtapi/"))) {
		command += 8;
		text++;
	} else if ((command = strstr(uri, "/xmlapi/"))) {
		command += 8;
		xml++;
	} else {
		return FALSE; /* 404 */
	}

	if ((path_info = strchr(command, '/'))) {
		*path_info++ = '\0';
	}

	for (i = 0; i < r->responseHeaderFields.size; i++) {
		TTableItem *ti = &r->responseHeaderFields.item[i];
		if (!strcasecmp(ti->name, "freeswitch-user")) {
			fs_user = ti->value;
		} else if (!strcasecmp(ti->name, "freeswitch-domain")) {
			fs_domain = ti->value;
		}
	}

	if (!is_authorized(r, command)) {
		ret = TRUE;
		goto end;
	}

/*  auth: */

	if (switch_event_create(&stream.param_event, SWITCH_EVENT_API) == SWITCH_STATUS_SUCCESS) {
		const char *const content_length = RequestHeaderValue(r, "content-length");
		evnt = stream.param_event;

		if (html) {
			switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "Content-Type", "text/html");
		} else if (text) {
			switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "Content-Type", "text/plain");
		} else if (xml) {
			switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "Content-Type", "text/xml");
		}
		if (api) {
			switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-API", "api");
		}
		if (fs_user)   switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "FreeSWITCH-User", fs_user);
		if (fs_domain) switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "FreeSWITCH-Domain", fs_domain);
		if (path_info) switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-Path-Info", path_info);

		if (info->host)        switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-HOST", info->host);
		if (info->from)        switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-FROM", info->from);
		if (info->useragent)   switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-USER-AGENT", info->useragent);
		if (info->referer)     switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-REFERER", info->referer);
		if (info->requestline) switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-REQUESTLINE", info->requestline);
		if (info->user)        switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-USER", info->user);
		if (info->port)        switch_event_add_header(evnt, SWITCH_STACK_BOTTOM, "HTTP-PORT", "%u", info->port);

		{
			char *q, *qd;
			char *next;
			char *query = (char *) info->query;
			char *name, *val;
			char qbuf[8192] = "";

			/* first finish reading from the socket if post method was used*/
			if (info->method == m_post && content_length) {
				int len = atoi(content_length);
				int qlen = 0;

				if (len > 0) {
					int succeeded = TRUE;
					char *qp = qbuf;
					char *readError;

					do {
						int blen = r->connP->buffersize - r->connP->bufferpos;

						if ((qlen + blen) > len) {
							blen = len - qlen;
						}

						qlen += blen;

						if (qlen > sizeof(qbuf)) {
							break;
						}

						memcpy(qp, r->connP->buffer.b + r->connP->bufferpos, blen);
						qp += blen;

						if (qlen >= len) {
							break;
						}

						ConnRead(r->connP, 2000, NULL, NULL, &readError);
		                if (readError) {
							succeeded = FALSE;
							free(readError);
						}

					} while (succeeded);

					query = qbuf;
				}
			}

			/* parse query and add kv-pairs as event headers  */
			/* a kv pair starts with '&', '+' or \0 mark the end */
			if (query) {
				switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-QUERY", query);
				qd = strdup(query);
			} else {
				qd = strdup(uri);
			}

			switch_assert(qd != NULL);

			q = qd;
			next = q;

			do {
				char *p;

				if (next = strchr(next, '&')) {
					if (!query) {
						/* pass kv pairs from uri to query       */
			            /* "?" is absent in url so parse uri     */
						*((char *)uri + (next - q - 1)) = '\0';
						query = next;
						switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-QUERY", next);
						/* and strip uri                                     */
						/* the start of first kv pair marks the end of uri   */
						/* to prevent kv-pairs confusing fs api commands     */
						/* that have arguments separated by space            */
					}
					*next++ = '\0';
				}

				for (p = q; p && *p; p++) {
					if (*p == '+') {
						*p = ' ';
					}
				}
				/* hmmm, get method requests are already decoded ... */
				switch_url_decode(q);

				name = q;
				if ((val = strchr(name, '='))) {
					*val++ = '\0';
					switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, name, val);
				}
				q = next;
			} while (q != NULL);

			free(qd);
		}
	}

	switch_event_add_header_string(evnt, SWITCH_STACK_BOTTOM, "HTTP-URI", uri);

	/* We made it this far, always OK */
	if (!HTTPWrite(r, "HTTP/1.1 200 OK\r\n", (uint32_t) strlen("HTTP/1.1 200 OK\r\n"))) {
		return TRUE;
	}

	ResponseAddField(r, "Connection", "close");

	/* generation of the date field */
	if (evnt)
	{
		ResponseAddField(r, "Date", switch_event_get_header(evnt, "Event-Date-GMT"));
	}
	else {
		const char *dateValue;

		DateToString(r->date, &dateValue);
		if (dateValue) {
			ResponseAddField(r, "Date", dateValue);
			free((void *)dateValue);
		}
	}

	/* Generation of the server field */
	switch_snprintf(v, sizeof(v), "FreeSWITCH-%s-mod_xml_rpc", switch_version_full());
	ResponseAddField(r, "Server", v);

	if (html) {
		ResponseAddField(r, "Content-Type", "text/html");
	} else if (text) {
		ResponseAddField(r, "Content-Type", "text/plain");
	} else if (xml) {
		ResponseAddField(r, "Content-Type", "text/xml");
	}

	for (i = 0; i < r->responseHeaderFields.size; i++) {
		TTableItem *ti = &r->responseHeaderFields.item[i];
		char *header = switch_mprintf("%s: %s\r\n", ti->name, ti->value);
		if (!ConnWrite(r->connP, header, (uint32_t) strlen(header))) {
			switch_safe_free(header);
			return TRUE;
		}
		switch_safe_free(header);
	}

	/* send end http header */
	if (html||text||xml) {
		if (!ConnWrite(r->connP, CRLF, 2)) {
			return TRUE;
		}
	}
	else {
		/* content-type and end of http header will be streamed by fs api or http_stream_write */
	}

	if (switch_stristr("unload", command) && switch_stristr("mod_xml_rpc", info->query)) {
		command = "bgapi";
		api_str = "unload mod_xml_rpc";
	} else if (switch_stristr("reload", command) && switch_stristr("mod_xml_rpc", info->query)) {
		command = "bgapi";
		api_str = "reload mod_xml_rpc";
	} else {
		api_str = info->query;
	}

	/* TODO (maybe): take "refresh=xxx" out of query as to not confuse fs api commands         */

	/* execute actual fs api command                                                            */
	/* fs api command will write to stream,  calling http_stream_write / http_stream_raw_write	*/
	/* switch_api_execute will stream INVALID COMMAND before it fails					        */
	switch_api_execute(command, api_str, NULL, &stream);
	
        if (globals.commands_to_log != NULL) {
                full_command = switch_mprintf("%s%s%s", command, (api_str==NULL ? "" : " "), api_str);

                if (switch_regex_match(full_command, globals.commands_to_log) == SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Executed HTTP request command: [%s].\n", full_command);
                }

                switch_safe_free(full_command);
        }

	r->responseStarted = TRUE;
	ResponseStatus(r, 200);     /* we don't want an assertion failure */
	r->requestInfo.keepalive = 0;

  end:

	return ret;
}

static xmlrpc_value *freeswitch_api(xmlrpc_env * const envP, xmlrpc_value * const paramArrayP, void *const userData, void *const callInfo)
{
	char *command = NULL, *arg = NULL;
	switch_stream_handle_t stream = { 0 };
	xmlrpc_value *val = NULL;
	switch_bool_t freed = 0;


	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ss)", &command, &arg);

	if (envP->fault_occurred) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Request!\n");
		return NULL;
	}

	if (!is_authorized((const TSession *) callInfo, command)) {
		val = xmlrpc_build_value(envP, "s", "UNAUTHORIZED!");
		goto end;
	}

	if (switch_stristr("unload", command) && switch_stristr("mod_xml_rpc", arg)) {
		switch_safe_free(command);
		switch_safe_free(arg);
		freed = 1;
		command = "bgapi";
		arg = "unload mod_xml_rpc";
	} else if (switch_stristr("reload", command) && switch_stristr("mod_xml_rpc", arg)) {
		switch_safe_free(command);
		switch_safe_free(arg);
		freed = 1;
		command = "bgapi";
		arg = "reload mod_xml_rpc";
	}

	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute(command, arg, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		/* Return our result. */
		val = xmlrpc_build_value(envP, "s", stream.data);
		free(stream.data);
	} else {
		val = xmlrpc_build_value(envP, "s", "ERROR!");
	}

  end:

	/* xmlrpc-c requires us to free memory it malloced from xmlrpc_decompose_value */
	if (!freed) {
		switch_safe_free(command);
		switch_safe_free(arg);
	}

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
		return NULL;
	}

	if (!strncasecmp(oid, FREESWITCH_OID_PREFIX, strlen(FREESWITCH_OID_PREFIX))) {
		relative_oid = oid + strlen(FREESWITCH_OID_PREFIX);
	} else {
		relative_oid = oid;
	}

	if (!zstr(data)) {
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

	/* xmlrpc-c requires us to free memory it malloced from xmlrpc_decompose_value */
	switch_safe_free(oid);
	switch_safe_free(s_action);
	switch_safe_free(data);
	return val;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_xml_rpc_runtime)
{
	xmlrpc_env env;
	char logfile[512];
	switch_hash_index_t *hi;
	const void *var;
	void *val;

	globals.running = 1;

	xmlrpc_env_init(&env);

	globals.registryP = xmlrpc_registry_new(&env);

	/* TODO why twice and why add_method for freeswitch.api and add_method2 for freeswitch.management ? */
    xmlrpc_registry_add_method2(&env, globals.registryP, "freeswitch.api", &freeswitch_api, NULL, NULL, NULL);
    xmlrpc_registry_add_method2(&env, globals.registryP, "freeswitch_api", &freeswitch_api, NULL, NULL, NULL);
    xmlrpc_registry_add_method(&env, globals.registryP, NULL, "freeswitch.management", &freeswitch_man, NULL);
    xmlrpc_registry_add_method(&env, globals.registryP, NULL, "freeswitch_management", &freeswitch_man, NULL);

	MIMETypeInit();

	for (hi = switch_core_mime_index(); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &var, NULL, &val);
		if (var && val) {
			MIMETypeAdd((char *) val, (char *) var);
		}
	}

	switch_snprintf(logfile, sizeof(logfile), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, "freeswitch_http.log");
	ServerCreate(&globals.abyssServer, "XmlRpcServer", globals.port, SWITCH_GLOBAL_dirs.htdocs_dir, logfile);

	xmlrpc_server_abyss_set_handler(&env, &globals.abyssServer, "/RPC2", globals.registryP);

	xmlrpc_env_clean(&env);

	if (ServerInit(&globals.abyssServer) != TRUE) {
		globals.running = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to start HTTP Port %d\n", globals.port);
		xmlrpc_registry_free(globals.registryP);
		MIMETypeTerm();

		return SWITCH_STATUS_TERM;
	}

	ServerAddHandler(&globals.abyssServer, handler_hook);
	ServerAddHandler(&globals.abyssServer, auth_hook);
	ServerSetKeepaliveTimeout(&globals.abyssServer, 5);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting HTTP Port %d, DocRoot [%s]%s\n",
		globals.port, SWITCH_GLOBAL_dirs.htdocs_dir, globals.enable_websocket ? " with websocket." : "");
	ServerRun(&globals.abyssServer);

	switch_yield(1000000);

	globals.running = 0;

	return SWITCH_STATUS_TERM;
}

void stop_all_websockets()
{
	switch_event_t *event;
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, "websocket::stophook") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Failed to create event!\n");
	}
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "stop", "now");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "stopping all websockets ...\n");
	if (switch_event_fire(&event) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Failed to fire the event!\n");
		switch_event_destroy(&event);
	}
}

/* upon module unload */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_rpc_shutdown)
{

	switch_event_free_subclass("websocket::stophook");
	
	/* Cann't find a way to stop the websockets, use this for a workaround before finding the real one that works */
	stop_all_websockets();

	/* this makes the worker thread (ServerRun) stop */
	ServerTerminate(&globals.abyssServer);

	do {
		switch_yield(100000);
	} while (globals.running);

	ServerFree(&globals.abyssServer);
	xmlrpc_registry_free(globals.registryP);
	MIMETypeTerm();

	switch_safe_free(globals.realm);
	switch_safe_free(globals.user);
	switch_safe_free(globals.pass);
	switch_safe_free(globals.default_domain);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
