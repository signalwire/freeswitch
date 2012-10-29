/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * mod_xml_scgi.c -- SCGI XML Gateway
 *
 */
#include <switch.h>
#include <scgi.h>


SWITCH_MODULE_LOAD_FUNCTION(mod_xml_scgi_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_scgi_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_scgi, mod_xml_scgi_load, mod_xml_scgi_shutdown, NULL);


struct xml_binding {
	char *host;
	switch_port_t port;
	char *uri;
	char *url;

	int timeout;
	switch_hash_t *vars_map;
	char *bindings;
	
	char *server;
	switch_thread_t *thread;
	struct xml_binding *next;
};

static int GLOBAL_DEBUG = 0;

typedef struct xml_binding xml_binding_t;

#define XML_SCGI_MAX_BYTES 1024 * 1024

typedef struct hash_node {
	switch_hash_t *hash;
	struct hash_node *next;
} hash_node_t;

static struct {
	switch_memory_pool_t *pool;
	hash_node_t *hash_root;
	hash_node_t *hash_tail;
	int running;
	xml_binding_t *bindings;
} globals;

#define XML_SCGI_SYNTAX "[debug_on|debug_off]"
SWITCH_STANDARD_API(xml_scgi_function)
{
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		goto usage;
	}

	if (!strcasecmp(cmd, "debug_on")) {
		GLOBAL_DEBUG = 1;
	} else if (!strcasecmp(cmd, "debug_off")) {
		GLOBAL_DEBUG = 0;
	} else {
		goto usage;
	}

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;

  usage:
	stream->write_function(stream, "USAGE: %s\n", XML_SCGI_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

void *SWITCH_THREAD_FUNC monitor_thread_run(switch_thread_t *thread, void *obj)
{
	xml_binding_t *binding = (xml_binding_t *) obj;
	time_t st;
	int diff;

	while(globals.running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Running server command: %s\n", binding->server);
		st = switch_epoch_time_now(NULL);
		switch_system(binding->server, SWITCH_TRUE);
		diff = (int) switch_epoch_time_now(NULL) - st;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Server command complete: %s\n", binding->server);

		if (globals.running && diff < 5) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Server command had short run duration, sleeping: %s\n", binding->server);
			switch_yield(10000000);
		}
	}
	
	return NULL;
}

static void launch_monitor_thread(xml_binding_t *binding)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_IMPORTANT);
	switch_thread_create(&binding->thread, thd_attr, monitor_thread_run, binding, globals.pool);
}


static switch_xml_t xml_url_fetch(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
								  void *user_data)
{
	switch_xml_t xml = NULL;
	char *data = NULL;
	xml_binding_t *binding = (xml_binding_t *) user_data;
	char hostname[256] = "";
	char basic_data[512];
	unsigned char buf[16336] = "";
	ssize_t len = -1, bytes = 0;
	scgi_handle_t handle = { 0 };
	switch_stream_handle_t stream = { 0 };
	char *txt = NULL;

	strncpy(hostname, switch_core_get_switchname(), sizeof(hostname));

	if (!binding) {
		return NULL;
	}

	switch_snprintf(basic_data, sizeof(basic_data), "hostname=%s&section=%s&tag_name=%s&key_name=%s&key_value=%s",
					hostname, section, switch_str_nil(tag_name), switch_str_nil(key_name), switch_str_nil(key_value));

	data = switch_event_build_param_string(params, basic_data, binding->vars_map);
	switch_assert(data);

	scgi_add_param(&handle, "REQUEST_METHOD", "POST");
	scgi_add_param(&handle, "REQUEST_URI", binding->uri);
	scgi_add_body(&handle, data);

	if (scgi_connect(&handle, binding->host, binding->port, binding->timeout * 1000) == SCGI_SUCCESS) {
		scgi_send_request(&handle);

		SWITCH_STANDARD_STREAM(stream);
		txt = (char *) stream.data;

		while((len = scgi_recv(&handle, buf, sizeof(buf))) > 0) {
			char *expanded = switch_event_expand_headers(params, (char *)buf);
			
			bytes += len;

			if (bytes > XML_SCGI_MAX_BYTES) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Data too big!\n");
				len = -1;
				break;
			}

			stream.write_function(&stream, "%s", expanded);
			txt = (char *) stream.data;

			if (expanded != (char *)buf) {
				free(expanded);
			}
			
			memset(buf, 0, sizeof(buf));
		}

		scgi_disconnect(&handle);

		if (len < 0 && (!txt || !strlen(txt))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DEBUG:\nURL: %s Connection Read Failed: [%s]\n", binding->url, handle.err);
			goto end;
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DEBUG:\nURL: %s Connection Failed: [%s]\n", binding->url, handle.err);
		goto end;
	}

	

	if (GLOBAL_DEBUG) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DEBUG:\nURL: %s\nPOST_DATA:\n%s\n\nRESPONSE:\n-----\n%s\n-----\n", 
						  binding->url, data, switch_str_nil(txt));
	}

	

	if (bytes && txt) {
		if (!(xml = switch_xml_parse_str_dynamic(txt, FALSE))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing Result! [%s]\ndata: [%s] RESPONSE[%s]\n", 
							  binding->url, data, switch_str_nil(txt));
		}
		txt = NULL;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received error trying to fetch %s\ndata: [%s] RESPONSE [%s]\n", 
						  binding->url, data, switch_str_nil(txt));
	}


 end:
	
	switch_safe_free(data);
	switch_safe_free(txt);
	
	return xml;
}

#define ENABLE_PARAM_VALUE "enabled"
static switch_status_t do_config(void)
{
	char *cf = "xml_scgi.conf";
	switch_xml_t cfg, xml, bindings_tag, binding_tag, param;
	xml_binding_t *binding = NULL;
	int x = 0;
	int need_vars_map = 0;
	switch_hash_t *vars_map = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (!(bindings_tag = switch_xml_child(cfg, "bindings"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <bindings> tag!\n");
		goto done;
	}

	for (binding_tag = switch_xml_child(bindings_tag, "binding"); binding_tag; binding_tag = binding_tag->next) {
		char *bname = (char *) switch_xml_attr_soft(binding_tag, "name");
		char *host = "127.0.0.1";
		char *port = "8080";
		char *bind_mask = NULL;
		int timeout = 0;
		char *server = NULL;

		hash_node_t *hash_node;
		need_vars_map = 0;
		vars_map = NULL;

		for (param = switch_xml_child(binding_tag, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "host")) {
				bind_mask = (char *) switch_xml_attr_soft(param, "bindings");
				if (val) {
					host = val;
				}
			} else if (!strcasecmp(var, "port")) {
				port = val;
			} else if (!strcasecmp(var, "timeout")) {
				int tmp = atoi(val);
				if (tmp >= 0) {
					timeout = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't set a negative timeout!\n");
				}
			} else if (!strcasecmp(var, "enable-post-var")) {
				if (!vars_map && need_vars_map == 0) {
					if (switch_core_hash_init(&vars_map, globals.pool) != SWITCH_STATUS_SUCCESS) {
						need_vars_map = -1;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't init params hash!\n");
						continue;
					}
					need_vars_map = 1;
				}

				if (vars_map && val) {
					if (switch_core_hash_insert(vars_map, val, ENABLE_PARAM_VALUE) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't add %s to params hash!\n", val);
					}
				}
			} else if (!strcasecmp(var, "server")) {
				server = val;
			}
		}

		if (!host) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Binding has no host!\n");
			if (vars_map) {
				switch_core_hash_destroy(&vars_map);
			}
			continue;
		}

		if (!(binding = switch_core_alloc(globals.pool, sizeof(*binding)))) {
			if (vars_map) {
				switch_core_hash_destroy(&vars_map);
			}
			goto done;
		}
		memset(binding, 0, sizeof(*binding));

		binding->timeout = timeout;
		binding->host = switch_core_strdup(globals.pool, host);
		binding->port = atoi(port);
		binding->vars_map = vars_map;
		binding->uri = switch_mprintf("/%s", bname);
		binding->url = switch_mprintf("scgi://%s:%s/%s", host, port, bname);

		if (server) {
			binding->server = switch_core_strdup(globals.pool, server);
		}

        if (bind_mask) {
			binding->bindings = switch_core_strdup(globals.pool, bind_mask);
		}                                         

		if (vars_map) {
			switch_zmalloc(hash_node, sizeof(hash_node_t));
			hash_node->hash = vars_map;
			hash_node->next = NULL;

			if (!globals.hash_root) {
				globals.hash_root = hash_node;
				globals.hash_tail = globals.hash_root;
			}

			else {
				globals.hash_tail->next = hash_node;
				globals.hash_tail = globals.hash_tail->next;
			}

		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Binding [%s] XML Fetch Function [%s] [%s]\n",
						  zstr(bname) ? "N/A" : bname, binding->url, binding->bindings ? binding->bindings : "all");
		switch_xml_bind_search_function(xml_url_fetch, switch_xml_parse_section_string(binding->bindings), binding);
		
		if (binding->server) {
			launch_monitor_thread(binding);
		}

		binding->next = globals.bindings;
		globals.bindings = binding;
		

		x++;
		binding = NULL;
	}

  done:
	switch_xml_free(xml);

	return x ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_scgi_load)
{
	switch_api_interface_t *xml_scgi_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	globals.running = 1;
	globals.pool = pool;
	globals.hash_root = NULL;
	globals.hash_tail = NULL;

	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_ADD_API(xml_scgi_api_interface, "xml_scgi", "XML SCGI", xml_scgi_function, XML_SCGI_SYNTAX);
	switch_console_set_complete("add xml_scgi debug_on");
	switch_console_set_complete("add xml_scgi debug_off");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_scgi_shutdown)
{
	hash_node_t *ptr = NULL;
	xml_binding_t *bp;

	globals.running = 0;

	for(bp = globals.bindings; bp; bp = bp->next) {
		if (bp->thread) {
			switch_status_t st;
			scgi_handle_t handle = { 0 };
			unsigned char buf[16336] = "";
			int x = 3;

			scgi_add_param(&handle, "REQUEST_METHOD", "POST");
			scgi_add_param(&handle, "REQUEST_URI", bp->uri);
			scgi_add_body(&handle, "SHUTDOWN");

			while(x--) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sending shutdown message to server for %s\n", bp->url);

				if (scgi_connect(&handle, bp->host, bp->port, bp->timeout * 1000) == SCGI_SUCCESS) {
					while(0 && scgi_recv(&handle, buf, sizeof(buf)) > 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s\n", (char *) buf);
						memset(buf, 0, sizeof(buf));
					}
					break;
				}
				
				switch_yield(5000000);
			}

			scgi_disconnect(&handle);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Waiting for server to stop.\n");
			switch_thread_join(&st, bp->thread);
		}
	}


	while (globals.hash_root) {
		ptr = globals.hash_root;
		switch_core_hash_destroy(&ptr->hash);
		globals.hash_root = ptr->next;
		switch_safe_free(ptr);
	}

	switch_xml_unbind_search_function_ptr(xml_url_fetch);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
