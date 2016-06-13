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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * mod_kazoo.c -- Socket Controlled Event Handler
 *
 */
#include "mod_kazoo.h"

#define KAZOO_DESC "kazoo information"
#define KAZOO_SYNTAX "<command> [<args>]"

globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_kazoo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_kazoo_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_kazoo_runtime);
SWITCH_MODULE_DEFINITION(mod_kazoo, mod_kazoo_load, mod_kazoo_shutdown, mod_kazoo_runtime);

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, globals.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ei_cookie, globals.ei_cookie);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ei_nodename, globals.ei_nodename);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_kazoo_var_prefix, globals.kazoo_var_prefix);

static switch_status_t api_erlang_status(switch_stream_handle_t *stream) {
	switch_sockaddr_t *sa;
	uint16_t port;
	char ipbuf[25];
	const char *ip_addr;
	ei_node_t *ei_node;

	switch_socket_addr_get(&sa, SWITCH_FALSE, globals.acceptor);

	port = switch_sockaddr_get_port(sa);
	ip_addr = switch_get_addr(ipbuf, sizeof (ipbuf), sa);

	stream->write_function(stream, "Running %s\n", VERSION);
	stream->write_function(stream, "Listening for new Erlang connections on %s:%u with cookie %s\n", ip_addr, port, globals.ei_cookie);
	stream->write_function(stream, "Registered as Erlang node %s, visible as %s\n", globals.ei_cnode.thisnodename, globals.ei_cnode.thisalivename);

	if (globals.ei_compat_rel) {
		stream->write_function(stream, "Using Erlang compatibility mode: %d\n", globals.ei_compat_rel);
	}

	switch_thread_rwlock_rdlock(globals.ei_nodes_lock);
	ei_node = globals.ei_nodes;
	if (!ei_node) {
		stream->write_function(stream, "No erlang nodes connected\n");
	} else {
		stream->write_function(stream, "Connected to:\n");
		while(ei_node != NULL) {
			unsigned int year, day, hour, min, sec, delta;

			delta = (switch_micro_time_now() - ei_node->created_time) / 1000000;
			sec = delta % 60;
			min = delta / 60 % 60;
			hour = delta / 3600 % 24;
			day = delta / 86400 % 7;
			year = delta / 31556926 % 12;
			stream->write_function(stream, "  %s (%s:%d) up %d years, %d days, %d hours, %d minutes, %d seconds\n"
								   ,ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port, year, day, hour, min, sec);
			ei_node = ei_node->next;
		}
	}
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_event_filter(switch_stream_handle_t *stream) {
	switch_hash_index_t *hi = NULL;
	int column = 0;

	for (hi = (switch_hash_index_t *)switch_core_hash_first_iter(globals.event_filter, hi); hi; hi = switch_core_hash_next(&hi)) {
		const void *key;
		void *val;
		switch_core_hash_this(hi, &key, NULL, &val);
		stream->write_function(stream, "%-50s", (char *)key);
		if (++column > 2) {
			stream->write_function(stream, "\n");
			column = 0;
		}
	}

	if (++column > 2) {
		stream->write_function(stream, "\n");
		column = 0;
	}

	stream->write_function(stream, "%-50s", globals.kazoo_var_prefix);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_nodes_list(switch_stream_handle_t *stream) {
	ei_node_t *ei_node;

	switch_thread_rwlock_rdlock(globals.ei_nodes_lock);
	ei_node = globals.ei_nodes;
	while(ei_node != NULL) {
		stream->write_function(stream, "%s (%s)\n", ei_node->peer_nodename, ei_node->remote_ip);
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_nodes_count(switch_stream_handle_t *stream) {
	ei_node_t *ei_node;
	int count = 0;

	switch_thread_rwlock_rdlock(globals.ei_nodes_lock);
	ei_node = globals.ei_nodes;
	while(ei_node != NULL) {
		count++;
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);

	stream->write_function(stream, "%d\n", count);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_complete_erlang_node(const char *line, const char *cursor, switch_console_callback_match_t **matches) {
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	ei_node_t *ei_node;

	switch_thread_rwlock_rdlock(globals.ei_nodes_lock);
	ei_node = globals.ei_nodes;
	while(ei_node != NULL) {
		switch_console_push_match(&my_matches, ei_node->peer_nodename);
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);

	if (my_matches) {
		*matches = my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

static switch_status_t handle_node_api_event_stream(ei_event_stream_t *event_stream, switch_stream_handle_t *stream) {
	ei_event_binding_t *binding;
	int column = 0;

	switch_mutex_lock(event_stream->socket_mutex);
	if (event_stream->connected == SWITCH_FALSE) {
		switch_sockaddr_t *sa;
		uint16_t port;
		char ipbuf[25] = {0};
		const char *ip_addr;

		switch_socket_addr_get(&sa, SWITCH_TRUE, event_stream->acceptor);
		port = switch_sockaddr_get_port(sa);
		ip_addr = switch_get_addr(ipbuf, sizeof (ipbuf), sa);

		if (zstr(ip_addr)) {
			ip_addr = globals.ip;
		}

		stream->write_function(stream, "%s:%d -> disconnected\n"
							   ,ip_addr, port);
	} else {
		stream->write_function(stream, "%s:%d -> %s:%d\n"
							   ,event_stream->local_ip, event_stream->local_port
							   ,event_stream->remote_ip, event_stream->remote_port);
	}

	binding = event_stream->bindings;
	while(binding != NULL) {
		if (binding->type == SWITCH_EVENT_CUSTOM) {
			stream->write_function(stream, "CUSTOM %-43s", binding->subclass_name);
		} else {
			stream->write_function(stream, "%-50s", switch_event_name(binding->type));
		}

		if (++column > 2) {
			stream->write_function(stream, "\n");
			column = 0;
		}

		binding = binding->next;
	}
	switch_mutex_unlock(event_stream->socket_mutex);

	if (!column) {
		stream->write_function(stream, "\n");
	} else {
		stream->write_function(stream, "\n\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_node_api_event_streams(ei_node_t *ei_node, switch_stream_handle_t *stream) {
	ei_event_stream_t *event_stream;

	switch_mutex_lock(ei_node->event_streams_mutex);
	event_stream = ei_node->event_streams;
	while(event_stream != NULL) {
		handle_node_api_event_stream(event_stream, stream);
		event_stream = event_stream->next;
	}
	switch_mutex_unlock(ei_node->event_streams_mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_node_api_command(ei_node_t *ei_node, switch_stream_handle_t *stream, uint32_t command) {
	unsigned int year, day, hour, min, sec, delta;

	switch (command) {
	case API_COMMAND_DISCONNECT:
		stream->write_function(stream, "Disconnecting erlang node %s at managers request\n", ei_node->peer_nodename);
		switch_clear_flag(ei_node, LFLAG_RUNNING);
		break;
	case API_COMMAND_REMOTE_IP:
		delta = (switch_micro_time_now() - ei_node->created_time) / 1000000;
		sec = delta % 60;
		min = delta / 60 % 60;
		hour = delta / 3600 % 24;
		day = delta / 86400 % 7;
		year = delta / 31556926 % 12;

		stream->write_function(stream, "Uptime           %d years, %d days, %d hours, %d minutes, %d seconds\n", year, day, hour, min, sec);
		stream->write_function(stream, "Local Address    %s:%d\n", ei_node->local_ip, ei_node->local_port);
		stream->write_function(stream, "Remote Address   %s:%d\n", ei_node->remote_ip, ei_node->remote_port);
		break;
	case API_COMMAND_STREAMS:
		handle_node_api_event_streams(ei_node, stream);
		break;
	case API_COMMAND_BINDINGS:
		handle_api_command_streams(ei_node, stream);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_node_command(switch_stream_handle_t *stream, const char *nodename, uint32_t command) {
	ei_node_t *ei_node;

	switch_thread_rwlock_rdlock(globals.ei_nodes_lock);
	ei_node = globals.ei_nodes;
	while(ei_node != NULL) {
		int length = strlen(ei_node->peer_nodename);

		if (!strncmp(ei_node->peer_nodename, nodename, length)) {
			handle_node_api_command(ei_node, stream, command);
			switch_thread_rwlock_unlock(globals.ei_nodes_lock);
			return SWITCH_STATUS_SUCCESS;
		}

		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);

	return SWITCH_STATUS_NOTFOUND;
}

static int read_cookie_from_file(char *filename) {
	int fd;
	char cookie[MAXATOMLEN + 1];
	char *end;
	struct stat buf;
	ssize_t res;

	if (!stat(filename, &buf)) {
		if ((buf.st_mode & S_IRWXG) || (buf.st_mode & S_IRWXO)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s must only be accessible by owner only.\n", filename);
			return 2;
		}
		if (buf.st_size > MAXATOMLEN) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s contains a cookie larger than the maximum atom size of %d.\n", filename, MAXATOMLEN);
			return 2;
		}
		fd = open(filename, O_RDONLY);
		if (fd < 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open cookie file %s : %d.\n", filename, errno);
			return 2;
		}

		if ((res = read(fd, cookie, MAXATOMLEN)) < 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read cookie file %s : %d.\n", filename, errno);
		}

		cookie[MAXATOMLEN] = '\0';

		/* replace any end of line characters with a null */
		if ((end = strchr(cookie, '\n'))) {
			*end = '\0';
		}

		if ((end = strchr(cookie, '\r'))) {
			*end = '\0';
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set cookie from file %s: %s\n", filename, cookie);

		set_pref_ei_cookie(cookie);
		return 0;
	} else {
		/* don't error here, because we might be blindly trying to read $HOME/.erlang.cookie, and that can fail silently */
		return 1;
	}
}

static switch_status_t config(void) {
	char *cf = "kazoo.conf";
	switch_xml_t cfg, xml, child, param;
	globals.send_all_headers = 0;
	globals.send_all_private_headers = 1;
	globals.connection_timeout = 500;
	globals.receive_timeout = 200;
	globals.receive_msg_preallocate = 2000;
	globals.event_stream_preallocate = 4000;
	globals.send_msg_batch = 10;
	globals.event_stream_framing = 2;
	globals.port = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open configuration file %s\n", cf);
		return SWITCH_STATUS_FALSE;
	} else {
		if ((child = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(child, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "listen-ip")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set bind ip address: %s\n", val);
					set_pref_ip(val);
				} else if (!strcmp(var, "listen-port")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set bind port: %s\n", val);
					globals.port = atoi(val);
				} else if (!strcmp(var, "cookie")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set cookie: %s\n", val);
					set_pref_ei_cookie(val);
				} else if (!strcmp(var, "cookie-file")) {
					if (read_cookie_from_file(val) == 1) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to read cookie from %s\n", val);
					}
				} else if (!strcmp(var, "nodename")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set node name: %s\n", val);
					set_pref_ei_nodename(val);
				} else if (!strcmp(var, "shortname")) {
					globals.ei_shortname = switch_true(val);
				} else if (!strcmp(var, "kazoo-var-prefix")) {
					set_pref_kazoo_var_prefix(val);
				} else if (!strcmp(var, "compat-rel")) {
					if (atoi(val) >= 7)
						globals.ei_compat_rel = atoi(val);
					else
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid compatibility release '%s' specified\n", val);
				} else if (!strcmp(var, "nat-map")) {
					globals.nat_map = switch_true(val);
				} else if (!strcmp(var, "send-all-headers")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-all-headers: %s\n", val);
					globals.send_all_headers = switch_true(val);
				} else if (!strcmp(var, "send-all-private-headers")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-all-private-headers: %s\n", val);
					globals.send_all_private_headers = switch_true(val);
				} else if (!strcmp(var, "connection-timeout")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set connection-timeout: %s\n", val);
					globals.connection_timeout = atoi(val);
				} else if (!strcmp(var, "receive-timeout")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set receive-timeout: %s\n", val);
					globals.receive_timeout = atoi(val);
				} else if (!strcmp(var, "receive-msg-preallocate")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set receive-msg-preallocate: %s\n", val);
					globals.receive_msg_preallocate = atoi(val);
				} else if (!strcmp(var, "event-stream-preallocate")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set event-stream-preallocate: %s\n", val);
					globals.event_stream_preallocate = atoi(val);
				} else if (!strcmp(var, "send-msg-batch-size")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set send-msg-batch-size: %s\n", val);
					globals.send_msg_batch = atoi(val);
				} else if (!strcmp(var, "event-stream-framing")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Set event-stream-framing: %s\n", val);
					globals.event_stream_framing = atoi(val);
				}
			}
		}

		if ((child = switch_xml_child(cfg, "event-filter"))) {
			switch_hash_t *filter;

			switch_core_hash_init(&filter);
			for (param = switch_xml_child(child, "header"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				switch_core_hash_insert(filter, var, "1");
			}

			globals.event_filter = filter;
		}

		switch_xml_free(xml);
	}

	if (globals.receive_msg_preallocate < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid receive message preallocate value, disabled\n");
		globals.receive_msg_preallocate = 0;
	}

	if (globals.event_stream_preallocate < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid event stream preallocate value, disabled\n");
		globals.event_stream_preallocate = 0;
	}

	if (globals.send_msg_batch < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid send message batch size, reverting to default\n");
		globals.send_msg_batch = 10;
	}

	if (!globals.event_filter) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Event filter not found in configuration, using default\n");
		globals.event_filter = create_default_filter();
	}

	if (globals.event_stream_framing < 1 || globals.event_stream_framing > 4) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid event stream framing value, using default\n");
		globals.event_stream_framing = 2;
	}

	if (zstr(globals.kazoo_var_prefix)) {
		set_pref_kazoo_var_prefix("variable_ecallmgr*");
		globals.var_prefix_length = 17; //ignore the *
	} else {
		/* we could use the global pool but then we would have to conditionally
		 * free the pointer if it was not drawn from the XML */
		char *buf;
		int size = switch_snprintf(NULL, 0, "variable_%s*", globals.kazoo_var_prefix) + 1;

		switch_malloc(buf, size);
		switch_snprintf(buf, size, "variable_%s*", globals.kazoo_var_prefix);
		switch_safe_free(globals.kazoo_var_prefix);
		globals.kazoo_var_prefix = buf;
		globals.var_prefix_length = size - 2; //ignore the *
	}

	if (!globals.num_worker_threads) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Number of worker threads not found in configuration, using default\n");
		globals.num_worker_threads = 10;
	}

	if (zstr(globals.ip)) {
		set_pref_ip("0.0.0.0");
	}

	if (zstr(globals.ei_cookie)) {
		int res;
		char *home_dir = getenv("HOME");
		char path_buf[1024];

		if (!zstr(home_dir)) {
			/* $HOME/.erlang.cookie */
			switch_snprintf(path_buf, sizeof (path_buf), "%s%s%s", home_dir, SWITCH_PATH_SEPARATOR, ".erlang.cookie");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Checking for cookie at path: %s\n", path_buf);

			res = read_cookie_from_file(path_buf);
			if (res) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No cookie or valid cookie file specified, using default cookie\n");
				set_pref_ei_cookie("ClueCon");
			}
		}
	}

	if (!globals.ei_nodename) {
		set_pref_ei_nodename("freeswitch");
	}

	if (!globals.nat_map) {
		globals.nat_map = 0;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t create_acceptor() {
	switch_sockaddr_t *sa;
	uint16_t port;
    char ipbuf[25];
    const char *ip_addr;

	/* if the config has specified an erlang release compatibility then pass that along to the erlang interface */
	if (globals.ei_compat_rel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Compatability with OTP R%d requested\n", globals.ei_compat_rel);
		ei_set_compat_rel(globals.ei_compat_rel);
	}

	if (!(globals.acceptor = create_socket_with_port(globals.pool, globals.port))) {
		return SWITCH_STATUS_SOCKERR;
	}

	switch_socket_addr_get(&sa, SWITCH_FALSE, globals.acceptor);

	port = switch_sockaddr_get_port(sa);
	ip_addr = switch_get_addr(ipbuf, sizeof (ipbuf), sa);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Erlang connection acceptor listening on %s:%u\n", ip_addr, port);

	/* try to initialize the erlang interface */
	if (create_ei_cnode(ip_addr, globals.ei_nodename, &globals.ei_cnode) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_SOCKERR;
	}

	/* tell the erlang port manager where we can be reached.  this returns a file descriptor pointing to epmd or -1 */
	if ((globals.epmdfd = ei_publish(&globals.ei_cnode, port)) == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						  "Failed to publish port to epmd. Try starting it yourself or run an erl shell with the -sname or -name option.\n");
		return SWITCH_STATUS_SOCKERR;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected to epmd and published erlang cnode name %s at port %d\n", globals.ei_cnode.thisnodename, port);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(exec_api_cmd)
{
	char *argv[1024] = { 0 };
	int unknown_command = 1, argc = 0;
	char *mycmd = NULL;

	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------------------------------------------\n"
		"erlang status                            - provides an overview of the current status\n"
		"erlang event_filter                      - lists the event headers that will be sent to Erlang nodes\n"
		"erlang nodes list                        - lists connected Erlang nodes (usefull for monitoring tools)\n"
		"erlang nodes count                       - provides a count of connected Erlang nodes (usefull for monitoring tools)\n"
		"erlang node <node_name> disconnect       - disconnects an Erlang node\n"
		"erlang node <node_name> connection       - Shows the connection info\n"
		"erlang node <node_name> event_streams    - lists the event streams for an Erlang node\n"
		"erlang node <node_name> fetch_bindings   - lists the XML fetch bindings for an Erlang node\n"
		"---------------------------------------------------------------------------------------------------------------------\n";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(mycmd = strdup(cmd))) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		stream->write_function(stream, "%s", usage_string);
		switch_safe_free(mycmd);
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(argv[0])) {
		stream->write_function(stream, "%s", usage_string);
		switch_safe_free(mycmd);
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strncmp(argv[0], "status", 6)) {
		unknown_command = 0;
		api_erlang_status(stream);
	} else if (!strncmp(argv[0], "event_filter", 6)) {
		unknown_command = 0;
		api_erlang_event_filter(stream);
	} else if (!strncmp(argv[0], "nodes", 6) && !zstr(argv[1])) {
		if (!strncmp(argv[1], "list", 6)) {
			unknown_command = 0;
			api_erlang_nodes_list(stream);
		} else if (!strncmp(argv[1], "count", 6)) {
			unknown_command = 0;
			api_erlang_nodes_count(stream);
		}
	} else if (!strncmp(argv[0], "node", 6) && !zstr(argv[1]) && !zstr(argv[2])) {
		if (!strncmp(argv[2], "disconnect", 6)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_DISCONNECT);
		} else if (!strncmp(argv[2], "connection", 2)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_REMOTE_IP);
		} else if (!strncmp(argv[2], "event_streams", 6)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_STREAMS);
		} else if (!strncmp(argv[2], "fetch_bindings", 6)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_BINDINGS);
		}
	}

	if (unknown_command) {
		stream->write_function(stream, "%s", usage_string);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_kazoo_load) {
	switch_api_interface_t *api_interface = NULL;
	switch_application_interface_t *app_interface = NULL;

	memset(&globals, 0, sizeof(globals));

	globals.pool = pool;
	globals.ei_nodes = NULL;

	if(config() != SWITCH_STATUS_SUCCESS) {
		// TODO: what would we need to clean up here?
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Improper configuration!\n");
		return SWITCH_STATUS_TERM;
	}

	if(create_acceptor() != SWITCH_STATUS_SUCCESS) {
		// TODO: what would we need to clean up here
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create erlang connection acceptor!\n");
		close_socket(&globals.acceptor);
		return SWITCH_STATUS_TERM;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* create an api for cli debug commands */
	SWITCH_ADD_API(api_interface, "erlang", KAZOO_DESC, exec_api_cmd, KAZOO_SYNTAX);
	switch_console_set_complete("add erlang status");
	switch_console_set_complete("add erlang event_filter");
	switch_console_set_complete("add erlang nodes list");
	switch_console_set_complete("add erlang nodes count");
	switch_console_set_complete("add erlang node ::erlang::node disconnect");
	switch_console_set_complete("add erlang node ::erlang::node connection");
	switch_console_set_complete("add erlang node ::erlang::node event_streams");
	switch_console_set_complete("add erlang node ::erlang::node fetch_bindings");
	switch_console_add_complete_func("::erlang::node", api_complete_erlang_node);

	switch_thread_rwlock_create(&globals.ei_nodes_lock, pool);

	switch_set_flag(&globals, LFLAG_RUNNING);

	/* create all XML fetch agents */
	bind_fetch_agents();

	/* add our modified commands */
	add_kz_commands(module_interface, api_interface);

	/* add our modified dptools */
	add_kz_dptools(module_interface, app_interface);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_kazoo_shutdown) {
	int sanity = 0;

	switch_console_set_complete("del erlang");
	switch_console_del_complete_func("::erlang::node");

	/* stop taking new requests and start shuting down the threads */
	switch_clear_flag(&globals, LFLAG_RUNNING);

	/* give everyone time to cleanly shutdown */
	while (switch_atomic_read(&globals.threads)) {
		switch_yield(100000);
		if (++sanity >= 200) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to kill all threads, continuing. This probably wont end well.....good luck!\n");
			break;
		}
	}

	if (globals.event_filter) {
		switch_core_hash_destroy(&globals.event_filter);
	}

	switch_thread_rwlock_wrlock(globals.ei_nodes_lock);
	switch_thread_rwlock_unlock(globals.ei_nodes_lock);
	switch_thread_rwlock_destroy(globals.ei_nodes_lock);

	/* close the connection to epmd and the acceptor */
	close_socketfd(&globals.epmdfd);
	close_socket(&globals.acceptor);

	/* remove all XML fetch agents */
	unbind_fetch_agents();

	/* Close the port we reserved for uPnP/Switch behind firewall, if necessary */
	//	if (globals.nat_map && switch_nat_get_type()) {
	//		switch_nat_del_mapping(globals.port, SWITCH_NAT_TCP);
	//	}

	/* clean up our allocated preferences */
	switch_safe_free(globals.ip);
	switch_safe_free(globals.ei_cookie);
	switch_safe_free(globals.ei_nodename);
	switch_safe_free(globals.kazoo_var_prefix);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_kazoo_runtime) {
	switch_os_socket_t os_socket;

	switch_atomic_inc(&globals.threads);

	switch_os_sock_get(&os_socket, globals.acceptor);

	while (switch_test_flag(&globals, LFLAG_RUNNING)) {
		int nodefd;
		ErlConnect conn;

		/* zero out errno because ei_accept doesn't differentiate between a */
		/* failed authentication or a socket failure, or a client version */
		/* mismatch or a godzilla attack (and a godzilla attack is highly likely) */
		errno = 0;

		/* wait here for an erlang node to connect, timming out to check if our module is still running every now-and-again */
		if ((nodefd = ei_accept_tmo(&globals.ei_cnode, (int) os_socket, &conn, globals.connection_timeout)) == ERL_ERROR) {
			if (erl_errno == ETIMEDOUT) {
				continue;
			} else if (errno) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Erlang connection acceptor socket error %d %d\n", erl_errno, errno);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								  "Erlang node connection failed - ensure your cookie matches '%s' and you are using a good nodename\n", globals.ei_cookie);
			}
			continue;
		}

		if (!switch_test_flag(&globals, LFLAG_RUNNING)) {
			break;
		}

		/* NEW ERLANG NODE CONNECTION! Hello friend! */
		new_kazoo_node(nodefd, &conn);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Erlang connection acceptor shut down\n");

	switch_atomic_dec(&globals.threads);

	return SWITCH_STATUS_TERM;
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
