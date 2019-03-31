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
 * Andrew Thompson <andrew@hijacked.us>
 * Rob Charlton <rob.charlton@savageminds.com>
 * Darren Schreiber <d@d-man.org>
 * Mike Jerris <mike@jerris.com>
 * Tamas Cseke <tamas.cseke@virtual-call-center.eu>
 *
 *
 * handle_msg.c -- handle messages received from erlang nodes
 *
 */
#include "mod_kazoo.h"

struct api_command_struct_s {
	char *cmd;
	char *arg;
	ei_node_t *ei_node;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	erlang_pid pid;
	switch_memory_pool_t *pool;
};
typedef struct api_command_struct_s api_command_struct_t;

static char *REQUEST_ATOMS[] = {
	"noevents",
	"exit",
	"link",
	"nixevent",
	"sendevent",
	"sendmsg",
	"commands",
	"command",
	"bind",
	"getpid",
	"version",
	"bgapi",
	"api",
	"event",
	"fetch_reply",
	"config",
	"bgapi4",
	"api4"
};

typedef enum {
	REQUEST_NOEVENTS,
	REQUEST_EXIT,
	REQUEST_LINK,
	REQUEST_NIXEVENT,
	REQUEST_SENDEVENT,
	REQUEST_SENDMSG,
	REQUEST_COMMANDS,
	REQUEST_COMMAND,
	REQUEST_BIND,
	REQUEST_GETPID,
	REQUEST_VERSION,
	REQUEST_BGAPI,
	REQUEST_API,
	REQUEST_EVENT,
	REQUEST_FETCH_REPLY,
	REQUEST_CONFIG,
	REQUEST_BGAPI4,
	REQUEST_API4,
	REQUEST_MAX
} request_atoms_t;

static switch_status_t find_request(char *atom, int *request) {
	int i;
	for (i = 0; i < REQUEST_MAX; i++) {
		if(!strncmp(atom, REQUEST_ATOMS[i], MAXATOMLEN)) {
			*request = i;
			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

static void destroy_node_handler(ei_node_t *ei_node) {
	int pending = 0;
	void *pop;

	switch_clear_flag(ei_node, LFLAG_RUNNING);

	/* wait for pending bgapi requests to complete */
	while ((pending = switch_atomic_read(&ei_node->pending_bgapi))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for %d pending bgapi requests to complete\n", pending);
		switch_yield(500000);
	}

	/* wait for receive handlers to complete */
	while ((pending = switch_atomic_read(&ei_node->receive_handlers))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for %d receive handlers to complete\n", pending);
		switch_yield(500000);
	}

	switch_mutex_lock(ei_node->event_streams_mutex);
	remove_event_streams(&ei_node->event_streams);
	switch_mutex_unlock(ei_node->event_streams_mutex);

	remove_xml_clients(ei_node);

	while (switch_queue_trypop(ei_node->received_msgs, &pop) == SWITCH_STATUS_SUCCESS) {
		ei_received_msg_t *received_msg = (ei_received_msg_t *) pop;

		ei_x_free(&received_msg->buf);
		switch_safe_free(received_msg);
	}

	while (switch_queue_trypop(ei_node->send_msgs, &pop) == SWITCH_STATUS_SUCCESS) {
		ei_send_msg_t *send_msg = (ei_send_msg_t *) pop;

		ei_x_free(&send_msg->buf);
		switch_safe_free(send_msg);
	}

	close_socketfd(&ei_node->nodefd);

	switch_mutex_destroy(ei_node->event_streams_mutex);

	switch_core_destroy_memory_pool(&ei_node->pool);
}

static switch_status_t add_to_ei_nodes(ei_node_t *this_ei_node) {
	switch_thread_rwlock_wrlock(kazoo_globals.ei_nodes_lock);

	if (!kazoo_globals.ei_nodes) {
		kazoo_globals.ei_nodes = this_ei_node;
	} else {
		this_ei_node->next = kazoo_globals.ei_nodes;
		kazoo_globals.ei_nodes = this_ei_node;
	}

	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t remove_from_ei_nodes(ei_node_t *this_ei_node) {
	ei_node_t *ei_node, *prev = NULL;
	int found = 0;

	switch_thread_rwlock_wrlock(kazoo_globals.ei_nodes_lock);

	/* try to find the event bindings list for the requestor */
	ei_node = kazoo_globals.ei_nodes;
	while(ei_node != NULL) {
		if (ei_node == this_ei_node) {
			found = 1;
			break;
		}

		prev = ei_node;
		ei_node = ei_node->next;
	}

	if (found) {
		if (!prev) {
			kazoo_globals.ei_nodes = this_ei_node->next;
		} else {
			prev->next = ei_node->next;
		}
	}

	switch_thread_rwlock_unlock(kazoo_globals.ei_nodes_lock);

	return SWITCH_STATUS_SUCCESS;
}

void kazoo_event_add_unique_header_string(switch_event_t *event, switch_stack_t stack, const char *header_name, const char *data) {
	if(!switch_event_get_header_ptr(event, header_name))
		switch_event_add_header_string(event, stack, header_name, data);
}


SWITCH_DECLARE(switch_status_t) kazoo_api_execute(const char *cmd, const char *arg, switch_core_session_t *session, switch_stream_handle_t *stream, char** reply)
{
	switch_api_interface_t *api;
	switch_status_t status;
	char *arg_used;
	char *cmd_used;
	int  fire_event = 0;
	char *arg_expanded;
	switch_event_t* evt;

	switch_assert(stream != NULL);
	switch_assert(stream->data != NULL);
	switch_assert(stream->write_function != NULL);

	arg_expanded = (char *) arg;

	switch_event_create(&evt, SWITCH_EVENT_GENERAL);
	arg_expanded = switch_event_expand_headers(evt, arg);
	switch_event_destroy(&evt);

	cmd_used = (char *) cmd;
	arg_used = arg_expanded;

	if (!stream->param_event) {
		switch_event_create(&stream->param_event, SWITCH_EVENT_API);
		fire_event = 1;
	}

	if (stream->param_event) {
		if (cmd_used && *cmd_used) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command", cmd_used);
		}
		if (arg_used && *arg_used) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command-Argument", arg_used);
		}
		if (arg_expanded && *arg_expanded) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command-Argument-Expanded", arg_expanded);
		}
	}


	if (cmd_used && (api = switch_loadable_module_get_api_interface(cmd_used)) != 0) {
		if ((status = api->function(arg_used, session, stream)) != SWITCH_STATUS_SUCCESS) {
			kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Result", "error");
			kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Error", stream->data);
		} else {
			kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Result", "success");
			kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Output", stream->data);
		}
		UNPROTECT_INTERFACE(api);
	} else {
		status = SWITCH_STATUS_FALSE;
		kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Result", "error");
		kazoo_event_add_unique_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Error", "invalid command");
	}

	if (stream->param_event && fire_event) {
		switch_event_fire(&stream->param_event);
	}

	if (cmd_used != cmd) {
		switch_safe_free(cmd_used);
	}

	if (arg_used != arg_expanded) {
		switch_safe_free(arg_used);
	}

	if (arg_expanded != arg) {
		switch_safe_free(arg_expanded);
	}

	return status;
}

static switch_status_t api_exec_stream(char *cmd, char *arg, switch_stream_handle_t *stream, char **reply) {
        switch_status_t status = SWITCH_STATUS_FALSE;

        if (kazoo_api_execute(cmd, arg, NULL, stream, reply) != SWITCH_STATUS_SUCCESS) {
                if(stream->data && strlen(stream->data)) {
                         *reply = strdup(stream->data);
                         status = SWITCH_STATUS_FALSE;
                } else {
                        *reply = switch_mprintf("%s: Command not found", cmd);
                        status = SWITCH_STATUS_NOTFOUND;
                }
        } else if (!stream->data || !strlen(stream->data)) {
                *reply = switch_mprintf("%s: Command returned no output", cmd);
                status = SWITCH_STATUS_SUCCESS;
        } else {
                *reply = strdup(stream->data);
                status = SWITCH_STATUS_SUCCESS;
        }

        return status;
}

static switch_status_t api_exec(char *cmd, char *arg, char **reply) {
	switch_stream_handle_t stream = { 0 };
	switch_status_t status = SWITCH_STATUS_FALSE;

	SWITCH_STANDARD_STREAM(stream);

	status = api_exec_stream(cmd, arg, &stream, reply);

	switch_safe_free(stream.data);

	return status;
}

static void *SWITCH_THREAD_FUNC bgapi3_exec(switch_thread_t *thread, void *obj) {
	api_command_struct_t *acs = (api_command_struct_t *) obj;
	switch_memory_pool_t *pool = acs->pool;
	char *reply = NULL;
	char *cmd = acs->cmd;
	char *arg = acs->arg;
	ei_node_t *ei_node = acs->ei_node;
	ei_send_msg_t *send_msg;

	switch_malloc(send_msg, sizeof(*send_msg));
	memcpy(&send_msg->pid, &acs->pid, sizeof(erlang_pid));

	if(!switch_test_flag(ei_node, LFLAG_RUNNING) || !switch_test_flag(&kazoo_globals, LFLAG_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring command while shuting down\n");
		switch_atomic_dec(&ei_node->pending_bgapi);
		return NULL;
	}

	ei_x_new_with_version(&send_msg->buf);

	ei_x_encode_tuple_header(&send_msg->buf, 3);

	if (api_exec(cmd, arg, &reply) == SWITCH_STATUS_SUCCESS) {
		ei_x_encode_atom(&send_msg->buf, "bgok");
	} else {
		ei_x_encode_atom(&send_msg->buf, "bgerror");
	}

	_ei_x_encode_string(&send_msg->buf, acs->uuid_str);
	_ei_x_encode_string(&send_msg->buf, reply);

	if (switch_queue_trypush(ei_node->send_msgs, send_msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send bgapi response %s to %s <%d.%d.%d>\n"
						  ,acs->uuid_str
						  ,acs->pid.node
						  ,acs->pid.creation
						  ,acs->pid.num
						  ,acs->pid.serial);
		ei_x_free(&send_msg->buf);
		switch_safe_free(send_msg);
	}

	switch_atomic_dec(&ei_node->pending_bgapi);

	switch_safe_free(reply);
	switch_safe_free(acs->arg);
	switch_core_destroy_memory_pool(&pool);

	return NULL;
}


static void *SWITCH_THREAD_FUNC bgapi4_exec(switch_thread_t *thread, void *obj) {
	api_command_struct_t *acs = (api_command_struct_t *) obj;
	switch_memory_pool_t *pool = acs->pool;
	char *reply = NULL;
	char *cmd = acs->cmd;
	char *arg = acs->arg;
	ei_node_t *ei_node = acs->ei_node;
	ei_send_msg_t *send_msg;
	switch_stream_handle_t stream = { 0 };

	if(!switch_test_flag(ei_node, LFLAG_RUNNING) || !switch_test_flag(&kazoo_globals, LFLAG_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring command while shuting down\n");
		switch_atomic_dec(&ei_node->pending_bgapi);
		return NULL;
	}

	SWITCH_STANDARD_STREAM(stream);
	switch_event_create(&stream.param_event, SWITCH_EVENT_API);

	switch_malloc(send_msg, sizeof(*send_msg));
	memcpy(&send_msg->pid, &acs->pid, sizeof(erlang_pid));

	ei_x_new_with_version(&send_msg->buf);
	ei_x_encode_tuple_header(&send_msg->buf, (stream.param_event ? 4 : 3));

	if (api_exec_stream(cmd, arg, &stream, &reply) == SWITCH_STATUS_SUCCESS) {
		ei_x_encode_atom(&send_msg->buf, "bgok");
	} else {
		ei_x_encode_atom(&send_msg->buf, "bgerror");
	}

	_ei_x_encode_string(&send_msg->buf, acs->uuid_str);
	_ei_x_encode_string(&send_msg->buf, reply);

	if (stream.param_event) {
		ei_encode_switch_event_headers_2(&send_msg->buf, stream.param_event, 0);
	}

	if (switch_queue_trypush(ei_node->send_msgs, send_msg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to send bgapi response %s to %s <%d.%d.%d>\n"
						  ,acs->uuid_str
						  ,acs->pid.node
						  ,acs->pid.creation
						  ,acs->pid.num
						  ,acs->pid.serial);
		ei_x_free(&send_msg->buf);
		switch_safe_free(send_msg);
	}

	switch_atomic_dec(&ei_node->pending_bgapi);

	if (stream.param_event) {
		switch_event_fire(&stream.param_event);
	}

	switch_safe_free(reply);
	switch_safe_free(acs->arg);
	switch_core_destroy_memory_pool(&pool);
	switch_safe_free(stream.data);

	return NULL;
}

static void log_sendmsg_request(char *uuid, switch_event_t *event)
{
	char *cmd = switch_event_get_header(event, "call-command");
	unsigned long cmd_hash;
	switch_ssize_t hlen = -1;
	unsigned long CMD_EXECUTE = switch_hashfunc_default("execute", &hlen);
	unsigned long CMD_XFEREXT = switch_hashfunc_default("xferext", &hlen);

	if (zstr(cmd)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "log|%s|invalid \n", uuid);
		DUMP_EVENT(event);
		return;
	}

	cmd_hash = switch_hashfunc_default(cmd, &hlen);

	if (cmd_hash == CMD_EXECUTE) {
		char *app_name = switch_event_get_header(event, "execute-app-name");
		char *app_arg = switch_event_get_header(event, "execute-app-arg");

		if(app_name) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "log|%s|executing %s %s \n", uuid, app_name, switch_str_nil(app_arg));
		}
	} else if (cmd_hash == CMD_XFEREXT) {
		switch_event_header_t *hp;

		for (hp = event->headers; hp; hp = hp->next) {
			char *app_name;
			char *app_arg;

			if (!strcasecmp(hp->name, "application")) {
				app_name = strdup(hp->value);
				app_arg = strchr(app_name, ' ');

				if (app_arg) {
					*app_arg++ = '\0';
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "log|%s|building xferext extension: %s %s\n", uuid, app_name, app_arg);
			}
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "log|%s|transfered call to xferext extension\n", uuid);
	}
}

static switch_status_t build_event(switch_event_t *event, ei_x_buff * buf) {
	int propslist_length, arity;
	int n=0;

	if(!event) {
		return SWITCH_STATUS_FALSE;
	}

	if (ei_decode_list_header(buf->buff, &buf->index, &propslist_length)) {
		return SWITCH_STATUS_FALSE;
	}

	while (!ei_decode_tuple_header(buf->buff, &buf->index, &arity) && n < propslist_length) {
		char key[1024];
		char *value;

		if (arity != 2) {
			return SWITCH_STATUS_FALSE;
		}

		if (ei_decode_string_or_binary_limited(buf->buff, &buf->index, sizeof(key), key)) {
			return SWITCH_STATUS_FALSE;
		}

		if (ei_decode_string_or_binary(buf->buff, &buf->index, &value)) {
			return SWITCH_STATUS_FALSE;
		}

		if (!strcmp(key, "body")) {
			switch_safe_free(event->body);
			event->body = value;
		} else	{
			if(!strcasecmp(key, "Call-ID")) {
				switch_core_session_t *session = NULL;
				if(!zstr(value)) {
					if ((session = switch_core_session_force_locate(value)) != NULL) {
						switch_channel_t *channel = switch_core_session_get_channel(session);
						switch_channel_event_set_data(channel, event);
						switch_core_session_rwunlock(session);
					}
				}
			}
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM | SWITCH_STACK_NODUP, key, value);
		}
		n++;
	}
	ei_skip_term(buf->buff, &buf->index);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t erlang_response_badarg(ei_x_buff * rbuf) {
	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "badarg");
	}

	return SWITCH_STATUS_GENERR;
}

static switch_status_t erlang_response_baduuid(ei_x_buff * rbuf) {
	if (rbuf) {
		ei_x_format_wo_ver(rbuf, "{~a,~a}", "error", "baduuid");
	}

	return SWITCH_STATUS_NOTFOUND;
}

static switch_status_t erlang_response_notimplemented(ei_x_buff * rbuf) {
	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "not_implemented");
	}

	return SWITCH_STATUS_NOTFOUND;
}

static switch_status_t erlang_response_ok(ei_x_buff *rbuf) {
	if (rbuf) {
		ei_x_encode_atom(rbuf, "ok");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t erlang_response_ok_uuid(ei_x_buff *rbuf, const char * uuid) {
	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		ei_x_encode_binary(rbuf, uuid, strlen(uuid));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_noevents(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	ei_event_stream_t *event_stream;

	switch_mutex_lock(ei_node->event_streams_mutex);
	if ((event_stream = find_event_stream(ei_node->event_streams, pid))) {
		remove_event_bindings(event_stream);
	}
	switch_mutex_unlock(ei_node->event_streams_mutex);

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_exit(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	switch_clear_flag(ei_node, LFLAG_RUNNING);

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_link(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	ei_link(ei_node, ei_self(&kazoo_globals.ei_cnode), pid);

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_nixevent(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char event_name[MAXATOMLEN + 1];
	switch_event_types_t event_type;
	ei_event_stream_t *event_stream;
	int custom = 0, length = 0;
	int i;

	if (ei_decode_list_header(buf->buff, &buf->index, &length)
		|| length == 0) {
		return erlang_response_badarg(rbuf);
	}

	switch_mutex_lock(ei_node->event_streams_mutex);
	if (!(event_stream = find_event_stream(ei_node->event_streams, pid))) {
		switch_mutex_unlock(ei_node->event_streams_mutex);
		return erlang_response_ok(rbuf);
	}

	for (i = 1; i <= length; i++) {
		if (ei_decode_atom_safe(buf->buff, &buf->index, event_name)) {
			switch_mutex_unlock(ei_node->event_streams_mutex);
			return erlang_response_badarg(rbuf);
		}

		if (custom) {
			remove_event_binding(event_stream, SWITCH_EVENT_CUSTOM, event_name);
		} else if (switch_name_event(event_name, &event_type) == SWITCH_STATUS_SUCCESS) {
			switch (event_type) {

			case SWITCH_EVENT_CUSTOM:
				custom++;
				break;

			case SWITCH_EVENT_ALL:
			{
				switch_event_types_t type;
				for (type = 0; type < SWITCH_EVENT_ALL; type++) {
					if(type != SWITCH_EVENT_CUSTOM) {
						remove_event_binding(event_stream, type, NULL);
					}
				}
				break;
			}

			default:
				remove_event_binding(event_stream, event_type, NULL);
			}
		} else {
			switch_mutex_unlock(ei_node->event_streams_mutex);
			return erlang_response_badarg(rbuf);
		}
	}
	switch_mutex_unlock(ei_node->event_streams_mutex);

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_sendevent(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char event_name[MAXATOMLEN + 1];
	char subclass_name[MAXATOMLEN + 1];
	switch_event_types_t event_type;
	switch_event_t *event = NULL;

	if (ei_decode_atom_safe(buf->buff, &buf->index, event_name)
		|| switch_name_event(event_name, &event_type) != SWITCH_STATUS_SUCCESS)
        {
			return erlang_response_badarg(rbuf);
        }

	if (!strncasecmp(event_name, "CUSTOM", MAXATOMLEN)) {
		if(ei_decode_atom(buf->buff, &buf->index, subclass_name)) {
			return erlang_response_badarg(rbuf);
		}
		switch_event_create_subclass(&event, event_type, subclass_name);
	} else {
		switch_event_create(&event, event_type);
	}

	if (build_event(event, buf) == SWITCH_STATUS_SUCCESS) {
		switch_event_fire(&event);
		return erlang_response_ok(rbuf);
	}

	if(event) {
		switch_event_destroy(&event);
	}

	return erlang_response_badarg(rbuf);
}

static switch_status_t handle_request_command(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	switch_core_session_t *session;
	switch_event_t *event = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_uuid_t cmd_uuid;
	char cmd_uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	if (ei_decode_string_or_binary_limited(buf->buff, &buf->index, sizeof(uuid_str), uuid_str)) {
		return erlang_response_badarg(rbuf);
	}

	switch_uuid_get(&cmd_uuid);
	switch_uuid_format(cmd_uuid_str, &cmd_uuid);

	switch_event_create(&event, SWITCH_EVENT_COMMAND);
	if (build_event(event, buf) != SWITCH_STATUS_SUCCESS) {
		return erlang_response_badarg(rbuf);
	}

	log_sendmsg_request(uuid_str, event);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event-uuid", cmd_uuid_str);

	if (zstr_buf(uuid_str) || !(session = switch_core_session_locate(uuid_str))) {
		return erlang_response_baduuid(rbuf);
	}
	switch_core_session_queue_private_event(session, &event, SWITCH_FALSE);
	switch_core_session_rwunlock(session);

	return erlang_response_ok_uuid(rbuf, cmd_uuid_str);
}

static switch_status_t handle_request_commands(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	switch_core_session_t *session;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int propslist_length, n;
	switch_uuid_t cmd_uuid;
	char cmd_uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	if (ei_decode_string_or_binary_limited(buf->buff, &buf->index, sizeof(uuid_str), uuid_str)) {
		return erlang_response_badarg(rbuf);
	}

	if (zstr_buf(uuid_str) || !(session = switch_core_session_locate(uuid_str))) {
		return erlang_response_baduuid(rbuf);
	}

	switch_uuid_get(&cmd_uuid);
	switch_uuid_format(cmd_uuid_str, &cmd_uuid);

	if (ei_decode_list_header(buf->buff, &buf->index, &propslist_length)) {
		switch_core_session_rwunlock(session);
		return SWITCH_STATUS_FALSE;
	}

	for(n = 0; n < propslist_length; n++) {
		switch_event_t *event = NULL;
		switch_event_create(&event, SWITCH_EVENT_COMMAND);
		if (build_event(event, buf) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_rwunlock(session);
			return erlang_response_badarg(rbuf);
		}
		log_sendmsg_request(uuid_str, event);
		if(n == (propslist_length - 1)) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event-uuid", cmd_uuid_str);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event-uuid", "null");
//			switch_event_del_header_val(event, "event-uuid-name", NULL);
		}
		switch_core_session_queue_private_event(session, &event, SWITCH_FALSE);
	}

	switch_core_session_rwunlock(session);

	return erlang_response_ok_uuid(rbuf, cmd_uuid_str);

}

static switch_status_t handle_request_sendmsg(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	switch_core_session_t *session;
	switch_event_t *event = NULL;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

	if (ei_decode_string_or_binary_limited(buf->buff, &buf->index, sizeof(uuid_str), uuid_str)) {
		return erlang_response_badarg(rbuf);
	}

	switch_event_create(&event, SWITCH_EVENT_SEND_MESSAGE);
	if (build_event(event, buf) != SWITCH_STATUS_SUCCESS) {
		return erlang_response_badarg(rbuf);
	}

	log_sendmsg_request(uuid_str, event);

	if (zstr_buf(uuid_str) || !(session = switch_core_session_locate(uuid_str))) {
		return erlang_response_baduuid(rbuf);
	}
	switch_core_session_queue_private_event(session, &event, SWITCH_FALSE);
	switch_core_session_rwunlock(session);

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_config(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {

	fetch_config();
	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_bind(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char section_str[MAXATOMLEN + 1];
	switch_xml_section_t section;

	if (ei_decode_atom_safe(buf->buff, &buf->index, section_str)
		|| !(section = switch_xml_parse_section_string(section_str))) {
		return erlang_response_badarg(rbuf);
	}

	switch(section) {
	case SWITCH_XML_SECTION_CONFIG:
		add_fetch_handler(ei_node, pid, kazoo_globals.config_fetch_binding);
		if(!kazoo_globals.config_fetched) {
			kazoo_globals.config_fetched = 1;
			fetch_config();
		}
		break;
	case SWITCH_XML_SECTION_DIRECTORY:
		add_fetch_handler(ei_node, pid, kazoo_globals.directory_fetch_binding);
		break;
	case SWITCH_XML_SECTION_DIALPLAN:
		add_fetch_handler(ei_node, pid, kazoo_globals.dialplan_fetch_binding);
		break;
	case SWITCH_XML_SECTION_CHANNELS:
		add_fetch_handler(ei_node, pid, kazoo_globals.channels_fetch_binding);
		break;
	case SWITCH_XML_SECTION_LANGUAGES:
		add_fetch_handler(ei_node, pid, kazoo_globals.languages_fetch_binding);
		break;
	case SWITCH_XML_SECTION_CHATPLAN:
		add_fetch_handler(ei_node, pid, kazoo_globals.chatplan_fetch_binding);
		break;
	default:
		return erlang_response_badarg(rbuf);
	}

	return erlang_response_ok(rbuf);
}

static switch_status_t handle_request_getpid(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		ei_x_encode_pid(rbuf, ei_self(&kazoo_globals.ei_cnode));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_version(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		_ei_x_encode_string(rbuf, VERSION);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_bgapi(switch_thread_start_t func, ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	api_command_struct_t *acs = NULL;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_uuid_t uuid;
	char cmd[MAXATOMLEN + 1];

	if (ei_decode_atom_safe(buf->buff, &buf->index, cmd)) {
		return erlang_response_badarg(rbuf);
	}

	switch_core_new_memory_pool(&pool);
	acs = switch_core_alloc(pool, sizeof(*acs));

	if (ei_decode_string_or_binary(buf->buff, &buf->index, &acs->arg)) {
		switch_core_destroy_memory_pool(&pool);
		return erlang_response_badarg(rbuf);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "bgexec: %s(%s)\n", cmd, acs->arg);

	acs->pool = pool;
	acs->ei_node = ei_node;
	acs->cmd = switch_core_strdup(pool, cmd);
	memcpy(&acs->pid, pid, sizeof(erlang_pid));

	switch_threadattr_create(&thd_attr, acs->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	switch_uuid_get(&uuid);
	switch_uuid_format(acs->uuid_str, &uuid);
	switch_thread_create(&thread, thd_attr, func, acs, acs->pool);

	switch_atomic_inc(&ei_node->pending_bgapi);

	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");
		_ei_x_encode_string(rbuf, acs->uuid_str);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_bgapi3(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
        return handle_request_bgapi(bgapi3_exec, ei_node, pid, buf, rbuf);
}

static switch_status_t handle_request_bgapi4(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
        return handle_request_bgapi(bgapi4_exec, ei_node, pid, buf, rbuf);
}

static switch_status_t handle_request_api4(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
        char cmd[MAXATOMLEN + 1];
        char *arg;
	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);
	switch_event_create(&stream.param_event, SWITCH_EVENT_API);

        if (ei_decode_atom_safe(buf->buff, &buf->index, cmd)) {
                return erlang_response_badarg(rbuf);
        }

        if (ei_decode_string_or_binary(buf->buff, &buf->index, &arg)) {
                return erlang_response_badarg(rbuf);
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "exec: %s(%s)\n", cmd, arg);

        if (rbuf) {
                char *reply;
		switch_status_t status;

		status = api_exec_stream(cmd, arg, &stream, &reply);

		if (status == SWITCH_STATUS_SUCCESS) {
			ei_x_encode_tuple_header(buf, 2);
			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_tuple_header(buf, (stream.param_event ? 3 : 2));
			ei_x_encode_atom(rbuf, "error");
		}

		_ei_x_encode_string(rbuf, reply);

		if (stream.param_event && status != SWITCH_STATUS_SUCCESS) {
			ei_encode_switch_event_headers(rbuf, stream.param_event);
		}

                switch_safe_free(reply);
        }

	if (stream.param_event) {
		switch_event_fire(&stream.param_event);
	}

        switch_safe_free(arg);
	switch_safe_free(stream.data);

        return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_api(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char cmd[MAXATOMLEN + 1];
	char *arg;

	if (ei_decode_atom_safe(buf->buff, &buf->index, cmd)) {
		return erlang_response_badarg(rbuf);
	}

	if (ei_decode_string_or_binary(buf->buff, &buf->index, &arg)) {
		return erlang_response_badarg(rbuf);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "exec: %s(%s)\n", cmd, arg);

	if (rbuf) {
		char *reply;

		ei_x_encode_tuple_header(rbuf, 2);

		if (api_exec(cmd, arg, &reply) == SWITCH_STATUS_SUCCESS) {
			ei_x_encode_atom(rbuf, "ok");
		} else {
			ei_x_encode_atom(rbuf, "error");
		}

		_ei_x_encode_string(rbuf, reply);
		switch_safe_free(reply);
	}

	switch_safe_free(arg);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_event(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char event_name[MAXATOMLEN + 1];
	ei_event_stream_t *event_stream;
	int length = 0;
	int i;

	if (ei_decode_list_header(buf->buff, &buf->index, &length) || !length) {
		return erlang_response_badarg(rbuf);
	}

	switch_mutex_lock(ei_node->event_streams_mutex);
	if (!(event_stream = find_event_stream(ei_node->event_streams, pid))) {
		event_stream = new_event_stream(ei_node, pid);
		/* ensure we are notified if the requesting processes dies so we can clean up */
		ei_link(ei_node, ei_self(&kazoo_globals.ei_cnode), pid);
	}

	for (i = 1; i <= length; i++) {

		if (ei_decode_atom_safe(buf->buff, &buf->index, event_name)) {
			switch_mutex_unlock(ei_node->event_streams_mutex);
			return erlang_response_badarg(rbuf);
		}
		add_event_binding(event_stream, event_name);
	}
	switch_mutex_unlock(ei_node->event_streams_mutex);

	if (rbuf) {
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "ok");

		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_string(rbuf, ei_node->local_ip);
		ei_x_encode_ulong(rbuf, get_stream_port(event_stream));
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_request_fetch_reply(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char section_str[MAXATOMLEN + 1];
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *xml_str;
	switch_xml_section_t section;
	switch_status_t result;

	if (ei_decode_atom_safe(buf->buff, &buf->index, section_str)
		|| !(section = switch_xml_parse_section_string(section_str))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring a fetch reply without a configuration section\n");
		return erlang_response_badarg(rbuf);
	}

	if (ei_decode_string_or_binary_limited(buf->buff, &buf->index, sizeof(uuid_str), uuid_str)
		|| zstr_buf(uuid_str)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring a fetch reply without request UUID\n");
		return erlang_response_badarg(rbuf);
	}

	if (ei_decode_string_or_binary(buf->buff, &buf->index, &xml_str)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring a fetch reply without XML : %s \n", uuid_str);
		return erlang_response_badarg(rbuf);
	}

	if (zstr(xml_str)) {
		switch_safe_free(xml_str);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Ignoring an empty fetch reply : %s\n", uuid_str);
		return erlang_response_badarg(rbuf);
	}

	switch(section) {
	case SWITCH_XML_SECTION_CONFIG:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.config_fetch_binding);
		break;
	case SWITCH_XML_SECTION_DIRECTORY:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.directory_fetch_binding);
		break;
	case SWITCH_XML_SECTION_DIALPLAN:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.dialplan_fetch_binding);
		break;
	case SWITCH_XML_SECTION_CHANNELS:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.channels_fetch_binding);
		break;
	case SWITCH_XML_SECTION_LANGUAGES:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.languages_fetch_binding);
		break;
	case SWITCH_XML_SECTION_CHATPLAN:
		result = fetch_reply(uuid_str, xml_str, kazoo_globals.chatplan_fetch_binding);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received fetch reply %s for an unknown configuration section: %s : %s\n", uuid_str, section_str, xml_str);
		return erlang_response_badarg(rbuf);
	}

	if (result == SWITCH_STATUS_SUCCESS) {
		return erlang_response_ok(rbuf);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received fetch reply %s is unknown or has expired : %s\n", uuid_str, xml_str);
		return erlang_response_baduuid(rbuf);
	}
}

static switch_status_t handle_kazoo_request(ei_node_t *ei_node, erlang_pid *pid, ei_x_buff *buf, ei_x_buff *rbuf) {
	char atom[MAXATOMLEN + 1];
	int type, size, arity = 0, request;

	/* ...{_, _}} | ...atom()} = Buf */
	ei_get_type(buf->buff, &buf->index, &type, &size);

	/* is_tuple(Type) */
	if (type == ERL_SMALL_TUPLE_EXT) {
		/* ..._, _} = Buf */
		ei_decode_tuple_header(buf->buff, &buf->index, &arity);
	}

	if (ei_decode_atom_safe(buf->buff, &buf->index, atom)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Recieved mod_kazoo message that did not contain a command (ensure you are using Kazoo v2.14+).\n");
		return erlang_response_badarg(rbuf);
	}

	if (find_request(atom, &request) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Recieved mod_kazoo message for unimplemented feature (ensure you are using Kazoo v2.14+): %s\n", atom);
		return erlang_response_badarg(rbuf);
	}

	switch(request) {
	case REQUEST_NOEVENTS:
		return handle_request_noevents(ei_node, pid, buf, rbuf);
	case REQUEST_EXIT:
		return handle_request_exit(ei_node, pid, buf, rbuf);
	case REQUEST_LINK:
		return handle_request_link(ei_node, pid, buf, rbuf);
	case REQUEST_NIXEVENT:
		return handle_request_nixevent(ei_node, pid, buf, rbuf);
	case REQUEST_SENDEVENT:
		return handle_request_sendevent(ei_node, pid, buf, rbuf);
	case REQUEST_SENDMSG:
		return handle_request_sendmsg(ei_node, pid, buf, rbuf);
	case REQUEST_COMMAND:
		return handle_request_command(ei_node, pid, buf, rbuf);
	case REQUEST_COMMANDS:
		return handle_request_commands(ei_node, pid, buf, rbuf);
	case REQUEST_BIND:
		return handle_request_bind(ei_node, pid, buf, rbuf);
	case REQUEST_GETPID:
		return handle_request_getpid(ei_node, pid, buf, rbuf);
	case REQUEST_VERSION:
		return handle_request_version(ei_node, pid, buf, rbuf);
	case REQUEST_BGAPI:
		return handle_request_bgapi3(ei_node, pid, buf, rbuf);
	case REQUEST_API:
		return handle_request_api(ei_node, pid, buf, rbuf);
	case REQUEST_EVENT:
		return handle_request_event(ei_node, pid, buf, rbuf);
	case REQUEST_FETCH_REPLY:
		return handle_request_fetch_reply(ei_node, pid, buf, rbuf);
	case REQUEST_CONFIG:
		return handle_request_config(ei_node, pid, buf, rbuf);
	case REQUEST_BGAPI4:
		return handle_request_bgapi4(ei_node, pid, buf, rbuf);
	case REQUEST_API4:
		return handle_request_api4(ei_node, pid, buf, rbuf);
	default:
		return erlang_response_notimplemented(rbuf);
	}
}

static switch_status_t handle_mod_kazoo_request(ei_node_t *ei_node, erlang_msg *msg, ei_x_buff *buf) {
	char atom[MAXATOMLEN + 1];
	int version, type, size, arity;

	buf->index = 0;
	ei_decode_version(buf->buff, &buf->index, &version);
	ei_get_type(buf->buff, &buf->index, &type, &size);

	/* is_tuple(Type) */
	if (type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received erlang message of an unexpected type (ensure you are using Kazoo v2.14+).\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	if (ei_decode_atom_safe(buf->buff, &buf->index, atom)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Recieved erlang message tuple that did not start with an atom (ensure you are using Kazoo v2.14+).\n");
		return SWITCH_STATUS_GENERR;
	}

	/* {'$gen_cast', {_, _}} = Buf */
	if (arity == 2 && !strncmp(atom, "$gen_cast", 9)) {
		return handle_kazoo_request(ei_node, &msg->from, buf, NULL);
        /* {'$gen_call', {_, _}, {_, _}} = Buf */
	} else if (arity == 3 && !strncmp(atom, "$gen_call", 9)) {
		switch_status_t status;
		ei_send_msg_t *send_msg;
		erlang_ref ref;

		switch_malloc(send_msg, sizeof(*send_msg));

		ei_x_new(&send_msg->buf);

		ei_x_new_with_version(&send_msg->buf);

		/* ...{_, _}, {_, _}} = Buf */
		ei_get_type(buf->buff, &buf->index, &type, &size);

		/* is_tuple(Type) */
		if (type != ERL_SMALL_TUPLE_EXT) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received erlang call message of an unexpected type (ensure you are using Kazoo v2.14+).\n");
			return SWITCH_STATUS_GENERR;
		}

		/* ..._, _}, {_, _}} = Buf */
		ei_decode_tuple_header(buf->buff, &buf->index, &arity);

		/* ...pid(), _}, {_, _}} = Buf */
		if (ei_decode_pid(buf->buff, &buf->index, &send_msg->pid)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received erlang call without a reply pid (ensure you are using Kazoo v2.14+).\n");
			return SWITCH_STATUS_GENERR;
		}

		/* ...ref()}, {_, _}} = Buf */
		if (ei_decode_ref(buf->buff, &buf->index, &ref)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received erlang call without a reply tag (ensure you are using Kazoo v2.14+).\n");
			return SWITCH_STATUS_GENERR;
		}

		/* send_msg->buf = {ref(), ... */
		ei_x_encode_tuple_header(&send_msg->buf, 2);
		ei_x_encode_ref(&send_msg->buf, &ref);

		status = handle_kazoo_request(ei_node, &msg->from, buf, &send_msg->buf);

		if (switch_queue_trypush(ei_node->send_msgs, send_msg) != SWITCH_STATUS_SUCCESS) {
			ei_x_free(&send_msg->buf);
			switch_safe_free(send_msg);
		}

		return status;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Recieved inappropriate erlang message (ensure you are using Kazoo v2.14+)\n");
		return SWITCH_STATUS_GENERR;
	}
}

/* fake enough of the net_kernel module to be able to respond to net_adm:ping */
static switch_status_t handle_net_kernel_request(ei_node_t *ei_node, erlang_msg *msg, ei_x_buff *buf) {
	int version, size, type, arity;
	char atom[MAXATOMLEN + 1];
	ei_send_msg_t *send_msg;
	erlang_ref ref;

	switch_malloc(send_msg, sizeof(*send_msg));

	ei_x_new(&send_msg->buf);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received net_kernel message, attempting to reply\n");

	buf->index = 0;
	ei_decode_version(buf->buff, &buf->index, &version);
	ei_get_type(buf->buff, &buf->index, &type, &size);

	/* is_tuple(Buff) */
	if (type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received net_kernel message of an unexpected type\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	/* {_, _, _} = Buf */
	if (arity != 3) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received net_kernel tuple has an unexpected arity\n");
		return SWITCH_STATUS_GENERR;
	}

	/* {'$gen_call', _, _} = Buf */
	if (ei_decode_atom_safe(buf->buff, &buf->index, atom) || strncmp(atom, "$gen_call", 9)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received net_kernel message tuple does not begin with the atom '$gen_call'\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);

	/* {_, Sender, _}=Buff, is_tuple(Sender) */
	if (type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Second element of the net_kernel tuple is an unexpected type\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	/* {_, _}=Sender */
	if (arity != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Second element of the net_kernel message has an unexpected arity\n");
		return SWITCH_STATUS_GENERR;
	}

	/* {Pid, Ref}=Sender */
	if (ei_decode_pid(buf->buff, &buf->index, &send_msg->pid) || ei_decode_ref(buf->buff, &buf->index, &ref)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to decode erlang pid or ref of the net_kernel tuple second element\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_get_type(buf->buff, &buf->index, &type, &size);

	/* {_, _, Request}=Buff, is_tuple(Request) */
	if (type != ERL_SMALL_TUPLE_EXT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Third element of the net_kernel message is an unexpected type\n");
		return SWITCH_STATUS_GENERR;
	}

	ei_decode_tuple_header(buf->buff, &buf->index, &arity);

	/* {_, _}=Request */
	if (arity != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Third element of the net_kernel message has an unexpected arity\n");
		return SWITCH_STATUS_GENERR;
	}

	/* {is_auth, _}=Request */
	if (ei_decode_atom_safe(buf->buff, &buf->index, atom) || strncmp(atom, "is_auth", MAXATOMLEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "The net_kernel message third element does not begin with the atom 'is_auth'\n");
		return SWITCH_STATUS_GENERR;
	}

	/* To ! {Tag, Reply} */
	ei_x_new_with_version(&send_msg->buf);
	ei_x_encode_tuple_header(&send_msg->buf, 2);
	ei_x_encode_ref(&send_msg->buf, &ref);
	ei_x_encode_atom(&send_msg->buf, "yes");

	if (switch_queue_trypush(ei_node->send_msgs, send_msg) != SWITCH_STATUS_SUCCESS) {
		ei_x_free(&send_msg->buf);
		switch_safe_free(send_msg);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t handle_erl_send(ei_node_t *ei_node, erlang_msg *msg, ei_x_buff *buf) {
	if (!strncmp(msg->toname, "net_kernel", MAXATOMLEN)) {
		return handle_net_kernel_request(ei_node, msg, buf);
	} else if (!strncmp(msg->toname, "mod_kazoo", MAXATOMLEN)) {
		return handle_mod_kazoo_request(ei_node, msg, buf);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Received erlang message to unknown process \"%s\" (ensure you are using Kazoo v2.14+).\n", msg->toname);
		return SWITCH_STATUS_GENERR;
	}
}

static switch_status_t handle_erl_msg(ei_node_t *ei_node, erlang_msg *msg, ei_x_buff *buf) {
	switch (msg->msgtype) {
	case ERL_SEND:
	case ERL_REG_SEND:
		return handle_erl_send(ei_node, msg, buf);
	case ERL_LINK:
		/* we received an erlang link request?	Should we be linking or are they linking to us and this just informs us? */
		return SWITCH_STATUS_SUCCESS;
	case ERL_UNLINK:
		/* we received an erlang unlink request?  Same question as the ERL_LINK, are we expected to do something? */
		return SWITCH_STATUS_SUCCESS;
	case ERL_EXIT:
		/* we received a notice that a process we were linked to has exited, clean up any bindings */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received erlang exit notice for %s <%d.%d.%d>\n", msg->from.node, msg->from.creation, msg->from.num, msg->from.serial);

		switch_mutex_lock(ei_node->event_streams_mutex);
		remove_event_stream(&ei_node->event_streams, &msg->from);
		switch_mutex_unlock(ei_node->event_streams_mutex);

		remove_fetch_handlers(ei_node, &msg->from);
		return SWITCH_STATUS_SUCCESS;
	case ERL_EXIT2:
		/* erlang nodes appear to send both the old and new style exit notices so just ignore these */
		return SWITCH_STATUS_FALSE;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Received unexpected erlang message type %d\n", (int) (msg->msgtype));
		return SWITCH_STATUS_FALSE;
	}
}

static void *SWITCH_THREAD_FUNC receive_handler(switch_thread_t *thread, void *obj) {
	ei_node_t *ei_node = (ei_node_t *) obj;

	switch_atomic_inc(&kazoo_globals.threads);
	switch_atomic_inc(&ei_node->receive_handlers);

	switch_assert(ei_node != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting erlang receive handler %p: %s (%s:%d)\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port);

	while (switch_test_flag(ei_node, LFLAG_RUNNING) && switch_test_flag(&kazoo_globals, LFLAG_RUNNING)) {
		void *pop;

		if (switch_queue_pop_timeout(ei_node->received_msgs, &pop, 100000) == SWITCH_STATUS_SUCCESS) {
			ei_received_msg_t *received_msg = (ei_received_msg_t *) pop;
			handle_erl_msg(ei_node, &received_msg->msg, &received_msg->buf);
			ei_x_free(&received_msg->buf);
			switch_safe_free(received_msg);
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutdown erlang receive handler %p: %s (%s:%d)\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port);

	switch_atomic_dec(&ei_node->receive_handlers);
	switch_atomic_dec(&kazoo_globals.threads);

	return NULL;
}

static void *SWITCH_THREAD_FUNC handle_node(switch_thread_t *thread, void *obj) {
	ei_node_t *ei_node = (ei_node_t *) obj;
	ei_received_msg_t *received_msg = NULL;
	int fault_count = 0;

	switch_atomic_inc(&kazoo_globals.threads);

	switch_assert(ei_node != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting node request handler %p: %s (%s:%d)\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port);

	add_to_ei_nodes(ei_node);

	while (switch_test_flag(ei_node, LFLAG_RUNNING) && switch_test_flag(&kazoo_globals, LFLAG_RUNNING)) {
		int status;
		int send_msg_count = 0;
		void *pop;

		if (!received_msg) {
			switch_malloc(received_msg, sizeof(*received_msg));
			/* create a new buf for the erlang message and a rbuf for the reply */
			if(kazoo_globals.receive_msg_preallocate > 0) {
				received_msg->buf.buff = malloc(kazoo_globals.receive_msg_preallocate);
				received_msg->buf.buffsz = kazoo_globals.receive_msg_preallocate;
				received_msg->buf.index = 0;
				if(received_msg->buf.buff == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not pre-allocate memory for mod_kazoo message\n");
					goto exit;
				}
			} else {
				ei_x_new(&received_msg->buf);
			}
		}

		while (++send_msg_count <= kazoo_globals.send_msg_batch
			   && switch_queue_pop_timeout(ei_node->send_msgs, &pop, 20000) == SWITCH_STATUS_SUCCESS) {
			ei_send_msg_t *send_msg = (ei_send_msg_t *) pop;
			ei_helper_send(ei_node, &send_msg->pid, &send_msg->buf);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sent erlang message to %s <%d.%d.%d>\n"
							  ,send_msg->pid.node
							  ,send_msg->pid.creation
							  ,send_msg->pid.num
							  ,send_msg->pid.serial);
			ei_x_free(&send_msg->buf);
			switch_safe_free(send_msg);
		}

		/* wait for a erlang message, or timeout to check if the module is still running */
		status = ei_xreceive_msg_tmo(ei_node->nodefd, &received_msg->msg, &received_msg->buf, kazoo_globals.receive_timeout);

		switch (status) {
		case ERL_TICK:
			/* erlang nodes send ticks to eachother to validate they are still reachable, we dont have to do anything here */
			break;
		case ERL_MSG:
			fault_count = 0;

			if (switch_queue_trypush(ei_node->received_msgs, received_msg) != SWITCH_STATUS_SUCCESS) {
				ei_x_free(&received_msg->buf);
				switch_safe_free(received_msg);
			}

			if (kazoo_globals.receive_msg_preallocate > 0 && received_msg->buf.buffsz > kazoo_globals.receive_msg_preallocate) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "increased received message buffer size to %d\n", received_msg->buf.buffsz);
			}

			received_msg = NULL;
			break;
		case ERL_ERROR:
			switch (erl_errno) {
			case ETIMEDOUT:
			case EAGAIN:
				/* if ei_xreceive_msg_tmo just timed out, ignore it and let the while loop check if we are still running */
				/* the erlang lib just wants us to try to receive again, so we will! */
				break;
			case EMSGSIZE:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Erlang communication fault with node %p %s (%s:%d): my spoon is too big\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port);
				switch_clear_flag(ei_node, LFLAG_RUNNING);
				break;
			case EIO:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Erlang communication fault with node %p %s (%s:%d): socket closed or I/O error [fault count %d]\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port, ++fault_count);

				if (fault_count >= kazoo_globals.io_fault_tolerance) {
					switch_clear_flag(ei_node, LFLAG_RUNNING);
				}

				break;
			default:
				/* OH NOS! something has gone horribly wrong, shutdown the connection if status set by ei_xreceive_msg_tmo is less than or equal to 0 */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Erlang communication fault with node %p %s (%s:%d): erl_errno=%d errno=%d\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port, erl_errno, errno);
				if (status < 0) {
					switch_clear_flag(ei_node, LFLAG_RUNNING);
				}
				break;
			}
			break;
		default:
			/* HUH? didnt plan for this, whatevs shutdown the connection if status set by ei_xreceive_msg_tmo is less than or equal to 0 */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unexpected erlang receive status %p %s (%s:%d): %d\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port, status);
			if (status < 0) {
				switch_clear_flag(ei_node, LFLAG_RUNNING);
			}
			break;
		}
	}

 exit:

    if (received_msg) {
		ei_x_free(&received_msg->buf);
		switch_safe_free(received_msg);
	}

	remove_from_ei_nodes(ei_node);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutdown erlang node handler %p: %s (%s:%d)\n", (void *)ei_node, ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port);

	destroy_node_handler(ei_node);

	switch_atomic_dec(&kazoo_globals.threads);
	return NULL;
}

/* Create a thread to wait for messages from an erlang node and process them */
switch_status_t new_kazoo_node(int nodefd, ErlConnect *conn) {
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool = NULL;
	switch_sockaddr_t *sa;
	ei_node_t *ei_node;
	int i = 0;

	/* create memory pool for this erlang node */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: Too bad drinking scotch isn't a paying job or Kenny's dad would be a millionare!\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* from the erlang node's memory pool, allocate some memory for the structure */
	if (!(ei_node = switch_core_alloc(pool, sizeof (*ei_node)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory: Stan, don't you know the first law of physics? Anything that's fun costs at least eight dollars.\n");
		return SWITCH_STATUS_MEMERR;
	}

	memset(ei_node, 0, sizeof(*ei_node));

	/* store the location of our pool */
	ei_node->pool = pool;

	/* save the file descriptor that the erlang interface lib uses to communicate with the new node */
	ei_node->nodefd = nodefd;
	ei_node->peer_nodename = switch_core_strdup(ei_node->pool, conn->nodename);
	ei_node->created_time = switch_micro_time_now();
	ei_node->legacy = kazoo_globals.enable_legacy;
	ei_node->event_stream_framing = kazoo_globals.event_stream_framing;

	/* store the IP and node name we are talking with */
	switch_os_sock_put(&ei_node->socket, (switch_os_socket_t *)&nodefd, pool);

	switch_socket_addr_get(&sa, SWITCH_TRUE, ei_node->socket);
	ei_node->remote_port = switch_sockaddr_get_port(sa);
	switch_get_addr(ei_node->remote_ip, sizeof (ei_node->remote_ip), sa);

	switch_socket_addr_get(&sa, SWITCH_FALSE, ei_node->socket);
	ei_node->local_port = switch_sockaddr_get_port(sa);
	switch_get_addr(ei_node->local_ip, sizeof (ei_node->local_ip), sa);

	switch_queue_create(&ei_node->send_msgs, MAX_QUEUE_LEN, pool);
	switch_queue_create(&ei_node->received_msgs, MAX_QUEUE_LEN, pool);

	switch_mutex_init(&ei_node->event_streams_mutex, SWITCH_MUTEX_DEFAULT, pool);

	/* when we start we are running */
	switch_set_flag(ei_node, LFLAG_RUNNING);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "New erlang connection from node %s (%s:%d) -> (%s:%d)\n", ei_node->peer_nodename, ei_node->remote_ip, ei_node->remote_port, ei_node->local_ip, ei_node->local_port);

	for(i = 0; i < kazoo_globals.num_worker_threads; i++) {
		switch_threadattr_create(&thd_attr, ei_node->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, receive_handler, ei_node, ei_node->pool);
	}

	switch_threadattr_create(&thd_attr, ei_node->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, handle_node, ei_node, ei_node->pool);

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
