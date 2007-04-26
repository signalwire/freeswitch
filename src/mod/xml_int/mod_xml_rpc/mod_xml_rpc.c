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
#ifdef _MSC_VER
#pragma warning(disable:4142)
#endif

#include <xmlrpc-c/base.h>
#ifdef ABYSS_WIN32
#undef strcasecmp
#endif
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

static const char modname[] = "mod_xml_rpc";

static switch_loadable_module_interface_t xml_rpc_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

static struct {
	uint16_t port;
	uint8_t running;
	char *realm;
	char *user;
	char *pass;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_realm, globals.realm)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_user, globals.user)
	SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_pass, globals.pass)

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


SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &xml_rpc_module_interface;

	memset(&globals, 0, sizeof(globals));

	do_config();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
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


abyss_bool HandleHook(TSession * r)
{
	char *m = "text/html";
	switch_stream_handle_t stream = { 0 };
	char *command;

	stream.data = r;
	stream.write_function = http_stream_write;

	if (globals.realm) {
		if (!RequestAuth(r, globals.realm, globals.user, globals.pass)) {
			return TRUE;
		}
	}



	if (strncmp(r->uri, "/api/", 5)) {
		return FALSE;
	}

	if (switch_event_create(&stream.event, SWITCH_EVENT_API) == SWITCH_STATUS_SUCCESS) {
		if (r->uri)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-URI", "%s", r->uri);
		if (r->query)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-QUERY", "%s", r->query);
		if (r->host)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-HOST", "%s", r->host);
		if (r->from)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-FROM", "%s", r->from);
		if (r->useragent)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-USER-AGENT", "%s", r->useragent);
		if (r->referer)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-REFERER", "%s", r->referer);
		if (r->requestline)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-REQUESTLINE", "%s", r->requestline);
		if (r->user)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-USER", "%s", r->user);
		if (r->port)
			switch_event_add_header(stream.event, SWITCH_STACK_BOTTOM, "HTTP-PORT", "%u", r->port);
	}

	command = r->uri + 5;

	ResponseChunked(r);
	ResponseStatus(r, 200);
	ResponseContentType(r, m);
	ResponseWrite(r);
	switch_api_execute(command, r->query, NULL, &stream);
	HTTPWriteEnd(r);
	return TRUE;
}


static xmlrpc_value *freeswitch_api(xmlrpc_env * const envP, xmlrpc_value * const paramArrayP, void *const userData)
{
	char *command, *arg;
	switch_stream_handle_t stream = { 0 };
	xmlrpc_value *val = NULL;


	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ss)", &command, &arg);
	if (envP->fault_occurred) {
		return NULL;
	}

	SWITCH_STANDARD_STREAM(stream);
	if (switch_api_execute(command, arg, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
		/* Return our result. */
		val = xmlrpc_build_value(envP, "s", stream.data);
		free(stream.data);
	} else {
		val = xmlrpc_build_value(envP, "s", "ERROR!");
	}

	return val;
}

static xmlrpc_value *freeswitch_man(xmlrpc_env * const envP, xmlrpc_value * const paramArrayP, void *const userData)
{
	char *oid, *relative_oid, *s_action, *data;
	char buf[SWITCH_MAX_MANAGEMENT_BUFFER_LEN] = "";
	switch_management_action_t action = SMA_NONE;
	xmlrpc_value *val;

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
				if (switch_strlen_zero(buf)) {
					snprintf(buf, sizeof(buf), "OK\n");
				}
			}
		} else {
			if (switch_strlen_zero(buf)) {
				snprintf(buf, sizeof(buf), "ERROR\n");
			}
		}
	} else {
		snprintf(buf, sizeof(buf), "Invalid Action %s\n", s_action);
	}

	/* Return our result. */
	val = xmlrpc_build_value(envP, "s", buf);


	return val;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	TServer abyssServer;
	xmlrpc_registry *registryP;
	xmlrpc_env env;
	char logfile[512];

	globals.running = 1;

	xmlrpc_env_init(&env);

	registryP = xmlrpc_registry_new(&env);

	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch.api", &freeswitch_api, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch_api", &freeswitch_api, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch.management", &freeswitch_man, NULL);
	xmlrpc_registry_add_method(&env, registryP, NULL, "freeswitch_management", &freeswitch_man, NULL);

	MIMETypeInit();
	MIMETypeAdd("text/html", "html");

	snprintf(logfile, sizeof(logfile), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, "freeswitch_http.log");
	ServerCreate(&abyssServer, "XmlRpcServer", globals.port, SWITCH_GLOBAL_dirs.htdocs_dir, logfile);

	xmlrpc_server_abyss_set_handler(&env, &abyssServer, "/RPC2", registryP);

	if (ServerInit(&abyssServer) != TRUE) {
		globals.running = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to start HTTP Port %d\n", globals.port);
		return SWITCH_STATUS_TERM;
	}

	ServerAddHandler(&abyssServer, HandleHook);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Starting HTTP Port %d, DocRoot [%s]\n", globals.port, SWITCH_GLOBAL_dirs.htdocs_dir);
	while (globals.running) {
		ServerRunOnce2(&abyssServer, ABYSS_FOREGROUND);
	}


	return SWITCH_STATUS_SUCCESS;
}



SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	globals.running = 0;
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
