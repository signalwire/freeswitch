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

#define API_COMMAND_DISCONNECT 0
#define API_COMMAND_REMOTE_IP 1
#define API_COMMAND_STREAMS 2
#define API_COMMAND_BINDINGS 3
#define API_COMMAND_OPTION 4

#define API_NODE_OPTION_FRAMING 0
#define API_NODE_OPTION_LEGACY 1
#define API_NODE_OPTION_MAX 99

static const char *node_runtime_options[] = {
		"event-stream-framing",
		"enable-legacy",
		NULL
};

static int api_find_node_option(char *option) {
	int i;
	for(i = 0; node_runtime_options[i] != NULL; i++) {
		if(!strcasecmp(option, node_runtime_options[i])) {
			return i;
		}
	}
	return API_NODE_OPTION_MAX;
}

static switch_status_t api_get_node_option(ei_node_t *ei_node, switch_stream_handle_t *stream, char *arg) {
	int option = api_find_node_option(arg);
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	switch (option) {
	case API_NODE_OPTION_FRAMING:
		stream->write_function(stream, "+OK %i", ei_node->event_stream_framing);
		break;

	case API_NODE_OPTION_LEGACY:
		stream->write_function(stream, "+OK %s", ei_node->legacy ? "true" : "false");
		break;

	default:
		stream->write_function(stream, "-ERR invalid option %s", arg);
		ret = SWITCH_STATUS_NOTFOUND;
		break;
	}

	return ret;
}

static switch_status_t api_set_node_option(ei_node_t *ei_node, switch_stream_handle_t *stream, char *name, char *value) {
	int option = api_find_node_option(name);
	short val;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	switch (option) {
	case API_NODE_OPTION_FRAMING:
		val = atoi(value);
		if (val != 1 && val != 2 && val != 4) {
			stream->write_function(stream, "-ERR Invalid event stream framing value (%i)", val);
			ret = SWITCH_STATUS_GENERR;
		} else {
			stream->write_function(stream, "+OK %i", val);
			ei_node->event_stream_framing = val;
		}
		break;

	case API_NODE_OPTION_LEGACY:
		ei_node->legacy = switch_true(value);
		stream->write_function(stream, "+OK %s", ei_node->legacy ? "true" : "false");
		break;

	default:
		stream->write_function(stream, "-ERR invalid option %s", name);
		ret = SWITCH_STATUS_NOTFOUND;
		break;
	}

	return ret;
}

static switch_status_t api_erlang_status(switch_stream_handle_t *stream) {
	switch_sockaddr_t *sa;
	uint16_t port;
	char ipbuf[48];
	const char *ip_addr;
	ei_node_t *ei_node;

	switch_socket_addr_get(&sa, SWITCH_FALSE, kazoo_globals.acceptor);

	port = switch_sockaddr_get_port(sa);
	ip_addr = switch_get_addr(ipbuf, sizeof (ipbuf), sa);

	stream->write_function(stream, "Running %s\n", VERSION);
	stream->write_function(stream, "Listening for new Erlang connections on %s:%u with cookie %s\n", ip_addr, port, kazoo_globals.ei_cookie);
	stream->write_function(stream, "Registered as Erlang node %s, visible as %s\n", kazoo_globals.ei_cnode.thisnodename, kazoo_globals.ei_cnode.thisalivename);

	if (kazoo_globals.ei_compat_rel) {
		stream->write_function(stream, "Using Erlang compatibility mode: %d\n", kazoo_globals.ei_compat_rel);
	}

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
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
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_event_filter(switch_stream_handle_t *stream) {
	switch_hash_index_t *hi = NULL;
	int column = 0;
	int idx = 0;

	for (hi = (switch_hash_index_t *)switch_core_hash_first_iter(kazoo_globals.event_filter, hi); hi; hi = switch_core_hash_next(&hi)) {
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

	while(kazoo_globals.kazoo_var_prefixes[idx] != NULL) {
		char var[100];
		char *prefix = kazoo_globals.kazoo_var_prefixes[idx];
		sprintf(var, "%s*", prefix);
		stream->write_function(stream, "%-50s", var);
		idx++;
	}


	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_nodes_list(switch_stream_handle_t *stream) {
	ei_node_t *ei_node;

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		stream->write_function(stream, "%s (%s)\n", ei_node->peer_nodename, ei_node->remote_ip);
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_erlang_nodes_count(switch_stream_handle_t *stream) {
	ei_node_t *ei_node;
	int count = 0;

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		count++;
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	stream->write_function(stream, "%d\n", count);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t api_complete_erlang_node(const char *line, const char *cursor, switch_console_callback_match_t **matches) {
	switch_console_callback_match_t *my_matches = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	ei_node_t *ei_node;

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		switch_console_push_match(&my_matches, ei_node->peer_nodename);
		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

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
		char ipbuf[48] = {0};
		const char *ip_addr;

		switch_socket_addr_get(&sa, SWITCH_TRUE, event_stream->acceptor);
		port = switch_sockaddr_get_port(sa);
		ip_addr = switch_get_addr(ipbuf, sizeof (ipbuf), sa);

		if (zstr(ip_addr)) {
			ip_addr = kazoo_globals.ip;
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

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		int length = strlen(ei_node->peer_nodename);

		if (!strncmp(ei_node->peer_nodename, nodename, length)) {
			handle_node_api_command(ei_node, stream, command);
			switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);
			return SWITCH_STATUS_SUCCESS;
		}

		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return SWITCH_STATUS_NOTFOUND;
}

static switch_status_t handle_node_api_command_arg(ei_node_t *ei_node, switch_stream_handle_t *stream, uint32_t command, char *arg) {

	switch (command) {
	case API_COMMAND_OPTION:
		return api_get_node_option(ei_node, stream, arg);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_NOTFOUND;
}

static switch_status_t handle_node_api_command_args(ei_node_t *ei_node, switch_stream_handle_t *stream, uint32_t command, int argc, char *argv[]) {

	switch (command) {
	case API_COMMAND_OPTION:
		return api_set_node_option(ei_node, stream, argv[0], argv[1]);
		break;
	default:
		break;
	}

	return SWITCH_STATUS_NOTFOUND;
}

static switch_status_t api_erlang_node_command_arg(switch_stream_handle_t *stream, const char *nodename, uint32_t command, char *arg) {
	ei_node_t *ei_node;
	switch_status_t ret = SWITCH_STATUS_NOTFOUND;

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		int length = strlen(ei_node->peer_nodename);

		if (!strncmp(ei_node->peer_nodename, nodename, length)) {
			ret = handle_node_api_command_arg(ei_node, stream, command, arg);
			switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);
			return ret ;
		}

		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return ret;
}

static switch_status_t api_erlang_node_command_args(switch_stream_handle_t *stream, const char *nodename, uint32_t command, int argc, char *argv[]) {
	ei_node_t *ei_node;
	switch_status_t ret = SWITCH_STATUS_NOTFOUND;

	switch_thread_rwlock_rdlock(kazoo_globals.ei_nodes_lock);
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		int length = strlen(ei_node->peer_nodename);

		if (!strncmp(ei_node->peer_nodename, nodename, length)) {
			ret = handle_node_api_command_args(ei_node, stream, command, argc, argv);
			switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);
			return ret;
		}

		ei_node = ei_node->next;
	}
	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return ret;
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

	if (!strncmp(argv[0], "status", 7)) {
		unknown_command = 0;
		api_erlang_status(stream);
	} else if (!strncmp(argv[0], "event_filter", 13)) {
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
		if (!strncmp(argv[2], "disconnect", 11)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_DISCONNECT);
		} else if (!strncmp(argv[2], "connection", 11)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_REMOTE_IP);
		} else if (!strncmp(argv[2], "event_streams", 14)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_STREAMS);
		} else if (!strncmp(argv[2], "fetch_bindings", 15)) {
			unknown_command = 0;
			api_erlang_node_command(stream, argv[1], API_COMMAND_BINDINGS);
		} else if (!strncmp(argv[2], "option", 7) && !zstr(argv[3])) {
			unknown_command = 0;
			if(argc > 4 && !zstr(argv[4]))
				api_erlang_node_command_args(stream, argv[1], API_COMMAND_OPTION, argc - 3, &argv[3]);
			else
				api_erlang_node_command_arg(stream, argv[1], API_COMMAND_OPTION, argv[3]);
		}
	}

	if (unknown_command) {
		stream->write_function(stream, "%s", usage_string);
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

void add_cli_api(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface)
{
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

}

void remove_cli_api()
{
	switch_console_set_complete("del erlang");
	switch_console_del_complete_func("::erlang::node");

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
