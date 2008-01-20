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
 * mod_event_socket.c -- Socket Controled Event Handler
 *
 */
#include <switch.h>
#define CMD_BUFLEN 1024 * 1000

SWITCH_MODULE_LOAD_FUNCTION(mod_event_socket_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_socket_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_socket_runtime);
SWITCH_MODULE_DEFINITION(mod_event_socket, mod_event_socket_load, mod_event_socket_shutdown, mod_event_socket_runtime);

static char *MARKER = "1";

typedef enum {
	LFLAG_AUTHED = (1 << 0),
	LFLAG_RUNNING = (1 << 1),
	LFLAG_EVENTS = (1 << 2),
	LFLAG_LOG = (1 << 3),
	LFLAG_FULL = (1 << 4),
	LFLAG_MYEVENTS = (1 << 5),
	LFLAG_SESSION = (1 << 6),
	LFLAG_ASYNC = (1 << 7)
} event_flag_t;

typedef enum {
	EVENT_FORMAT_PLAIN,
	EVENT_FORMAT_XML
} event_format_t;

struct listener {
	switch_socket_t *sock;
	switch_queue_t *event_queue;
	switch_queue_t *log_queue;
	switch_memory_pool_t *pool;
	event_format_t format;
	switch_mutex_t *flag_mutex;
	uint32_t flags;
	switch_log_level_t level;
	char *ebuf;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	switch_thread_rwlock_t *rwlock;
	switch_core_session_t *session;
	struct listener *next;
};

typedef struct listener listener_t;

static struct {
	switch_socket_t *sock;
	switch_mutex_t *mutex;
	switch_mutex_t *sock_mutex;
	listener_t *listeners;
	uint8_t ready;
} listen_list;

static struct {
	switch_mutex_t *mutex;
	char *ip;
	uint16_t port;
	char *password;
	int done;
	int threads;
} prefs;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_pass, prefs.password);

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t * thread, void *obj);
static void launch_listener_thread(listener_t * listener);

static switch_status_t socket_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	listener_t *l;

	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (switch_test_flag(l, LFLAG_LOG) && l->level >= node->level) {
			char *data = strdup(node->data);
			if (data) {
				switch_queue_push(l->log_queue, data);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}
	}
	switch_mutex_unlock(listen_list.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event)
{
	switch_event_t *clone = NULL;
	listener_t *l;

	switch_assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}

	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		uint8_t send = 0;

		if (!switch_test_flag(l, LFLAG_EVENTS)) {
			continue;
		}
		
		if (l->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass || (switch_core_hash_find(l->event_hash, event->subclass->name))) {
				send = 1;
			}
		}

		if (send && switch_test_flag(l, LFLAG_MYEVENTS)) {
			char *uuid = switch_event_get_header(event, "unique-id");
			if (!uuid || strcmp(uuid, switch_core_session_get_uuid(l->session))) {
				send = 0;
			}
		}

		if (send) {
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				switch_queue_push(l->event_queue, clone);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}

	}
	switch_mutex_unlock(listen_list.mutex);
}

SWITCH_STANDARD_APP(socket_function)
{
	char *host, *port_name;
	switch_socket_t *new_sock;
	switch_sockaddr_t *sa;
	switch_port_t port = 8084;
	listener_t *listener;
	int argc = 0, x = 0;
	char *argv[80] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return;
	}

	host = argv[0];

	if (switch_strlen_zero(host)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing Host!\n");
		return;
	}

	if ((port_name = strchr(host, ':'))) {
		*port_name++ = '\0';
		port = (switch_port_t) atoi(port_name);
	}

	if (switch_sockaddr_info_get(&sa, host, AF_INET, port, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}

	if (switch_socket_create(&new_sock, AF_INET, SOCK_STREAM, SWITCH_PROTO_TCP, switch_core_session_get_pool(session))
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}

	switch_socket_opt_set(new_sock, SWITCH_SO_KEEPALIVE, 1);

	if (switch_socket_connect(new_sock, sa) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}


	if (!(listener = switch_core_session_alloc(session, sizeof(*listener)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
		return;
	}

	switch_thread_rwlock_create(&listener->rwlock, switch_core_session_get_pool(session));
	switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, switch_core_session_get_pool(session));
	switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, switch_core_session_get_pool(session));

	listener->sock = new_sock;
	listener->pool = switch_core_session_get_pool(session);
	listener->format = EVENT_FORMAT_PLAIN;
	listener->session = session;

	switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_core_hash_init(&listener->event_hash, listener->pool);
	switch_set_flag(listener, LFLAG_AUTHED);
	for (x = 1; x < argc; x++) {
		if (argv[x] && !strcasecmp(argv[x], "full")) {
			switch_set_flag(listener, LFLAG_FULL);
		} else if (argv[x] && !strcasecmp(argv[x], "async")) {
			switch_set_flag(listener, LFLAG_ASYNC);
		}
	}

	if (switch_test_flag(listener, LFLAG_ASYNC)) {
		launch_listener_thread(listener);
		switch_ivr_park(session, NULL);
	} else {
		listener_run(NULL, (void *) listener);
	}

	while (switch_test_flag(listener, LFLAG_SESSION)) {
		switch_yield(100000);
	}

}


static void close_socket(switch_socket_t ** sock)
{
	switch_mutex_lock(listen_list.sock_mutex);
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
	switch_mutex_unlock(listen_list.sock_mutex);
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_socket_shutdown)
{
	listener_t *l;
	int sanity = 0;

	prefs.done = 1;

	close_socket(&listen_list.sock);

	while(prefs.threads) {
		switch_yield(10000);
		if (++sanity == 1000) {
			break;
		}
	}

	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sock);
	}
	switch_mutex_unlock(listen_list.mutex);



	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_event_socket_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "socket", "Connect to a socket", "Connect to a socket", socket_function, "<ip>[:<port>]", SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static void add_listener(listener_t * listener)
{
	/* add me to the listeners so I get events */
	switch_mutex_lock(listen_list.mutex);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_mutex_unlock(listen_list.mutex);
}

static void remove_listener(listener_t * listener)
{
	listener_t *l, *last = NULL;

	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l == listener) {
			if (last) {
				last->next = l->next;
			} else {
				listen_list.listeners = l->next;
			}
		}
		last = l;
	}
	switch_mutex_unlock(listen_list.mutex);
}

static void strip_cr(char *s)
{
	char *p;
	if ((p = strchr(s, '\r')) || (p = strchr(s, '\n'))) {
		*p = '\0';
	}
}

static switch_status_t read_packet(listener_t * listener, switch_event_t **event, uint32_t timeout)
{
	switch_size_t mlen, bytes = 0;
	char mbuf[2048] = "";
	char buf[1024] = "";
	switch_size_t len;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int count = 0;
	uint32_t elapsed = 0;
	time_t start = 0;
	void *pop;
	char *ptr;
	uint8_t crcount = 0;
	uint32_t max_len = sizeof(mbuf);
	switch_channel_t *channel = NULL;
	*event = NULL;
	start = switch_timestamp(NULL);
	ptr = mbuf;


	if (listener->session) {
		channel = switch_core_session_get_channel(listener->session);
	}

	while (listener->sock && !prefs.done) {
		uint8_t do_sleep = 1;
		mlen = 1;
		status = switch_socket_recv(listener->sock, ptr, &mlen);

		if (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}

		if (channel && !switch_channel_ready(channel)) {
			status = SWITCH_STATUS_FALSE;
			break;
		}

		if (mlen) {
			bytes += mlen;
			do_sleep = 0;

			if (*mbuf == '\r' || *mbuf == '\n') {	/* bah */
				ptr = mbuf;
				mbuf[0] = '\0';
				bytes = 0;
				continue;
			}

			if (*ptr == '\n') {
				crcount++;
			} else if (*ptr != '\r') {
				crcount = 0;
			}
			ptr++;

			if (bytes >= max_len) {
				crcount = 2;
			}

			if (crcount == 2) {
				char *next;
				char *cur = mbuf;
				bytes = 0;
				while (cur) {
					if ((next = strchr(cur, '\r')) || (next = strchr(cur, '\n'))) {
						while (*next == '\r' || *next == '\n') {
							next++;
						}
					}
					count++;
					if (count == 1) {
						switch_event_create(event, SWITCH_EVENT_MESSAGE);
						switch_event_add_header(*event, SWITCH_STACK_BOTTOM, "Command", "%s", mbuf);
					} else if (cur) {
						char *var, *val;
						var = cur;
						strip_cr(var);
						if (!switch_strlen_zero(var)) {
							if ((val = strchr(var, ':'))) {
								*val++ = '\0';
								while (*val == ' ') {
									val++;
								}
							}
							if (var && val) {
								switch_event_add_header(*event, SWITCH_STACK_BOTTOM, var, "%s", val);
							}
						}
					}

					cur = next;
				}
				break;
			}
		}

		if (timeout) {
			elapsed = (uint32_t) (switch_timestamp(NULL) - start);
			if (elapsed >= timeout) {
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
				return SWITCH_STATUS_FALSE;
			}
		}

		if (!*mbuf) {
			if (switch_test_flag(listener, LFLAG_LOG)) {
				if (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
					char *data = (char *) pop;


					if (data) {
						switch_snprintf(buf, sizeof(buf), "Content-Type: log/data\nContent-Length: %" SWITCH_SSIZE_T_FMT "\n\n", strlen(data));
						len = strlen(buf);
						switch_socket_send(listener->sock, buf, &len);
						len = strlen(data);
						switch_socket_send(listener->sock, data, &len);

						free(data);
					}
					do_sleep = 0;
				}
			}

			if (switch_test_flag(listener, LFLAG_EVENTS)) {
				if (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
					char hbuf[512];
					switch_event_t *pevent = (switch_event_t *) pop;
					char *etype;

					do_sleep = 0;
					if (listener->format == EVENT_FORMAT_PLAIN) {
						etype = "plain";
						switch_event_serialize(pevent, &listener->ebuf, SWITCH_TRUE);
					} else {
						switch_xml_t xml;
						etype = "xml";

						if ((xml = switch_event_xmlize(pevent, "%s", ""))) {
							listener->ebuf = switch_xml_toxml(xml, SWITCH_FALSE);
							switch_xml_free(xml);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML ERROR!\n");
							goto endloop;
						}
					}

					len = strlen(listener->ebuf);

					switch_snprintf(hbuf, sizeof(hbuf), "Content-Length: %" SWITCH_SSIZE_T_FMT "\n" "Content-Type: text/event-%s\n" "\n", len, etype);

					len = strlen(hbuf);
					switch_socket_send(listener->sock, hbuf, &len);

					len = strlen(listener->ebuf);
					switch_socket_send(listener->sock, listener->ebuf, &len);

					switch_safe_free(listener->ebuf);
					
				endloop:
					switch_event_destroy(&pevent);
				}
			}
		}
		if (do_sleep) {
			switch_yield(1000);
		}
	}



	return status;

}

struct api_command_struct {
	char *api_cmd;
	char *arg;
	listener_t *listener;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint8_t bg;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t * thread, void *obj)
{

	struct api_command_struct *acs = (struct api_command_struct *) obj;
	switch_stream_handle_t stream = { 0 };
	char *reply, *freply = NULL;
	switch_status_t status;

	if (!acs->listener || !acs->listener->rwlock || switch_thread_rwlock_tryrdlock(acs->listener->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! cannot get read lock.\n");
		goto done;
	}


	SWITCH_STANDARD_STREAM(stream);

	if ((status = switch_api_execute(acs->api_cmd, acs->arg, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
		reply = stream.data;
	} else {
		freply = switch_mprintf("%s: Command not found!\n", acs->api_cmd);
		reply = freply;
	}

	if (!reply) {
		reply = "Command returned no output!";
	}

	if (acs->bg) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Job-UUID", "%s", acs->uuid_str);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Job-Command", "%s", acs->api_cmd);
			if (acs->arg) {
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", "%s", acs->arg);
			}
			switch_event_add_body(event, "%s", reply);
			switch_event_fire(&event);
		}
	} else {
		switch_size_t rlen, blen;
		char buf[1024] = "";
		rlen = strlen(reply);
		switch_snprintf(buf, sizeof(buf), "Content-Type: api/response\nContent-Length: %" SWITCH_SSIZE_T_FMT "\n\n", rlen);
		blen = strlen(buf);
		switch_socket_send(acs->listener->sock, buf, &blen);
		switch_socket_send(acs->listener->sock, reply, &rlen);
	}

	switch_safe_free(stream.data);
	switch_safe_free(freply);

	if (acs->listener->rwlock) {
		switch_thread_rwlock_unlock(acs->listener->rwlock);
	}

 done:
	if (acs && acs->bg) {
		switch_memory_pool_t *pool = acs->pool;
		acs = NULL;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
	}
	return NULL;

}
static switch_status_t parse_command(listener_t * listener, switch_event_t *event, char *reply, uint32_t reply_len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cmd = switch_event_get_header(event, "command");

	*reply = '\0';

	if (!strncasecmp(cmd, "exit", 4)) {
		switch_clear_flag_locked(listener, LFLAG_RUNNING);
		switch_snprintf(reply, reply_len, "+OK bye");
		goto done;
	}

	if (!switch_test_flag(listener, LFLAG_AUTHED)) {
		if (!strncasecmp(cmd, "auth ", 5)) {
			char *pass;
			strip_cr(cmd);

			pass = cmd + 5;

			if (!strcmp(prefs.password, pass)) {
				switch_set_flag_locked(listener, LFLAG_AUTHED);
				switch_snprintf(reply, reply_len, "+OK accepted");
			} else {
				switch_snprintf(reply, reply_len, "-ERR invalid");
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
			}

			goto done;
		}

		goto done;
	}


	if (listener->session) {
		switch_channel_t *channel = switch_core_session_get_channel(listener->session);
		switch_assert(channel != NULL);

		if (!strncasecmp(cmd, "connect", 7)) {
			switch_snprintf(reply, reply_len, "+OK");
			goto done;
		} else if (!strncasecmp(cmd, "sendmsg", 7)) {
			if (switch_test_flag(listener, LFLAG_ASYNC)) {
				if ((status = switch_core_session_queue_private_event(listener->session, &event)) == SWITCH_STATUS_SUCCESS) {
					switch_snprintf(reply, reply_len, "+OK");
				} else {
					switch_snprintf(reply, reply_len, "-ERR memory error");
				}
			} else {
				switch_ivr_parse_event(listener->session, event);
				switch_snprintf(reply, reply_len, "+OK");
			}
			goto done;
		} else if (!strncasecmp(cmd, "getvar", 6)) {
			char *arg;
			const char *val = "";

			strip_cr(cmd);

			if ((arg = strchr(cmd, ' '))) {
				*arg++ = '\0';
				if (!(val = switch_channel_get_variable(channel, arg))) {
					val = "";
				}

			}
			switch_snprintf(reply, reply_len, "%s", val);
			goto done;
		} else if (!strncasecmp(cmd, "myevents", 8)) {
			listener->event_list[SWITCH_EVENT_CHANNEL_CREATE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_DESTROY] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_STATE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_ANSWER] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_HANGUP] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_EXECUTE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_BRIDGE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_UNBRIDGE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_PROGRESS] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_OUTGOING] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_PARK] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_UNPARK] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_APPLICATION] = 1;
			listener->event_list[SWITCH_EVENT_TALK] = 1;
			listener->event_list[SWITCH_EVENT_DTMF] = 1;
			listener->event_list[SWITCH_EVENT_NOTALK] = 1;
			listener->event_list[SWITCH_EVENT_DETECTED_SPEECH] = 1;
			switch_set_flag_locked(listener, LFLAG_MYEVENTS);
			switch_set_flag_locked(listener, LFLAG_EVENTS);
			if (strstr(cmd, "xml") || strstr(cmd, "XML")) {
				listener->format = EVENT_FORMAT_XML;
			}
			switch_snprintf(reply, reply_len, "+OK Events Enabled");
			goto done;
		}

		if (!switch_test_flag(listener, LFLAG_FULL)) {
			goto done;
		}
	}

	if (!strncasecmp(cmd, "sendevent", 9)) {
		char *ename;
		strip_cr(cmd);

		ename = cmd + 9;

		while (ename && (*ename == '\t' || *ename == ' ')) {
			++ename;
		}

		if (ename && (*ename == '\r' || *ename == '\n')) {
			ename = NULL;
		}

		if (ename) {
			switch_event_types_t etype;
			if (switch_name_event(ename, &etype) == SWITCH_STATUS_SUCCESS) {
				event->event_id = etype;
			}
		}

		switch_event_fire(&event);
		switch_snprintf(reply, reply_len, "+OK");
		goto done;
	} else if (!strncasecmp(cmd, "sendmsg", 7)) {
		switch_core_session_t *session;
		char *uuid = cmd + 8;

		if (uuid) {
			while (*uuid == ' ') {
				uuid++;
			}

			if (*uuid == '\r' || *uuid == '\n') {
				uuid = NULL;
			} else {
				strip_cr(uuid);
			}
		}

		if (!uuid) {
			uuid = switch_event_get_header(event, "session-id");
		}

		if ((session = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			if (!switch_channel_test_flag(channel, CF_CONTROLLED)) {
				switch_core_session_rwunlock(session);
				session = NULL;
			}
		}

		if (session) {
			if ((status = switch_core_session_queue_private_event(session, &event)) == SWITCH_STATUS_SUCCESS) {
				switch_snprintf(reply, reply_len, "+OK");
			} else {
				switch_snprintf(reply, reply_len, "-ERR memory error");
			}
			switch_core_session_rwunlock(session);
		} else {
			switch_snprintf(reply, reply_len, "-ERR invalid session id [%s]", uuid);
		}

		goto done;

	} else if (!strncasecmp(cmd, "api ", 4)) {
		struct api_command_struct acs = { 0 };
		char *api_cmd = cmd + 4;
		char *arg = NULL;
		strip_cr(api_cmd);

		if ((arg = strchr(api_cmd, ' '))) {
			*arg++ = '\0';
		}

		acs.listener = listener;
		acs.api_cmd = api_cmd;
		acs.arg = arg;
		acs.bg = 0;

		api_exec(NULL, (void *) &acs);
		//switch_snprintf(reply, reply_len, "+OK");

		return SWITCH_STATUS_SUCCESS;
	} else if (!strncasecmp(cmd, "bgapi ", 6)) {
		struct api_command_struct *acs = NULL;
		char *api_cmd = cmd + 6;
		char *arg = NULL;
		char *uuid_str = NULL;
		switch_memory_pool_t *pool;
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;
		switch_uuid_t uuid;

		strip_cr(api_cmd);

		if ((arg = strchr(api_cmd, ' '))) {
			*arg++ = '\0';
		}

		switch_core_new_memory_pool(&pool);
		acs = switch_core_alloc(pool, sizeof(*acs));
		switch_assert(acs);
		acs->pool = pool;
		acs->listener = listener;
		if (api_cmd) {
			acs->api_cmd = switch_core_strdup(acs->pool, api_cmd);
		}
		if (arg) {
			acs->arg = switch_core_strdup(acs->pool, arg);
		}
		acs->bg = 1;
		switch_threadattr_create(&thd_attr, acs->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);
		if ((uuid_str = switch_event_get_header(event, "job-uuid"))) {
			switch_copy_string(acs->uuid_str, uuid_str, sizeof(acs->uuid_str));
		} else {
			switch_uuid_get(&uuid);
			switch_uuid_format(acs->uuid_str, &uuid);
		}
		switch_snprintf(reply, reply_len, "+OK Job-UUID: %s", acs->uuid_str);
		
		return SWITCH_STATUS_SUCCESS;
	} else if (!strncasecmp(cmd, "log", 3)) {

		char *level_s;
		switch_log_level_t ltype = SWITCH_LOG_DEBUG;

		//pull off the first newline/carriage return
		strip_cr(cmd);

		//move past the command
		level_s = cmd + 3;

		//see if we got an argument
		if (!switch_strlen_zero(level_s)) {
			//if so move to the argument
			level_s++;
		}
		//see if we lined up on an argument or not
		if (!switch_strlen_zero(level_s)) {
			ltype = switch_log_str2level(level_s);
		}

		if (ltype && ltype != SWITCH_LOG_INVALID) {
			listener->level = ltype;
			switch_set_flag(listener, LFLAG_LOG);
			switch_snprintf(reply, reply_len, "+OK log level %s [%d]", level_s, listener->level);
		} else {
			switch_snprintf(reply, reply_len, "-ERR invalid log level");
		}
	} else if (!strncasecmp(cmd, "nolog", 5)) {
		void *pop;
		while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS);

		if (switch_test_flag(listener, LFLAG_LOG)) {
			switch_clear_flag_locked(listener, LFLAG_LOG);
			switch_snprintf(reply, reply_len, "+OK no longer logging");
		} else {
			switch_snprintf(reply, reply_len, "-ERR not loging");
		}
	} else if (!strncasecmp(cmd, "event", 5)) {
		char *next, *cur;
		uint32_t count = 0, key_count = 0;
		uint8_t custom = 0;

		strip_cr(cmd);
		cur = cmd + 5;

		if (cur && (cur = strchr(cur, ' '))) {
			for (cur++; cur; count++) {
				switch_event_types_t type;

				if ((next = strchr(cur, ' '))) {
					*next++ = '\0';
				}

				if (!count) {
					if (!strcasecmp(cur, "xml")) {
						listener->format = EVENT_FORMAT_XML;
						goto end;
					} else if (!strcasecmp(cur, "plain")) {
						listener->format = EVENT_FORMAT_PLAIN;
						goto end;
					}
				}

				if (custom) {
					switch_core_hash_insert(listener->event_hash, cur, MARKER);
				} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
					key_count++;
					if (type == SWITCH_EVENT_ALL) {
						uint32_t x = 0;
						for (x = 0; x < SWITCH_EVENT_ALL; x++) {
							listener->event_list[x] = 1;
						}
					}
					if (type <= SWITCH_EVENT_ALL) {
						listener->event_list[type] = 1;
					}
					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					}
				}

			end:
				cur = next;
			}
		}

		if (!key_count) {
			switch_snprintf(reply, reply_len, "-ERR no keywords supplied");
			goto done;
		}

		if (!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}

		switch_snprintf(reply, reply_len, "+OK event listener enabled %s", listener->format == EVENT_FORMAT_XML ? "xml" : "plain");

	} else if (!strncasecmp(cmd, "nixevent", 8)) {
		char *next, *cur;
		uint32_t count = 0, key_count = 0;
		uint8_t custom = 0;

		strip_cr(cmd);
		cur = cmd + 5;

		if (cur && (cur = strchr(cur, ' '))) {
			for (cur++; cur; count++) {
				switch_event_types_t type;

				if ((next = strchr(cur, ' '))) {
					*next++ = '\0';
				}

				if (custom) {
					switch_core_hash_delete(listener->event_hash, cur);
				} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
					uint32_t x = 0;
					key_count++;

					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					} else if (type == SWITCH_EVENT_ALL) {
						for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
							listener->event_list[x] = 0;
						}
					} else {
						if (listener->event_list[SWITCH_EVENT_ALL]) {
							listener->event_list[SWITCH_EVENT_ALL] = 0;
							for (x = 0; x < SWITCH_EVENT_ALL; x++) {
								listener->event_list[x] = 1;
							}
						}
						listener->event_list[type] = 0;
					}
				}

				cur = next;
			}
		}

		if (!key_count) {
			switch_snprintf(reply, reply_len, "-ERR no keywords supplied");
			goto done;
		}

		if (!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}

		switch_snprintf(reply, reply_len, "+OK events nixed");

	} else if (!strncasecmp(cmd, "noevents", 8)) {
		void *pop;
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS);

		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}
			/* wipe the hash */
			switch_core_hash_destroy(&listener->event_hash);
			switch_core_hash_init(&listener->event_hash, listener->pool);
			switch_snprintf(reply, reply_len, "+OK no longer listening for events");
		} else {
			switch_snprintf(reply, reply_len, "-ERR not listening for events");
		}
	}

 done:
	if (event) {
		switch_event_destroy(&event);
	}

	if (switch_strlen_zero(reply)) {
		switch_snprintf(reply, reply_len, "-ERR command not found");
	}

	return status;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t * thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	char buf[1024];
	switch_size_t len;
	switch_status_t status;
	switch_event_t *event;
	char reply[512] = "";
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	switch_mutex_lock(listen_list.mutex);
	prefs.threads++;
	switch_mutex_unlock(listen_list.mutex);

	switch_assert(listener != NULL);

	if ((session = listener->session)) {
		channel = switch_core_session_get_channel(session);
		switch_core_session_read_lock(session);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");

	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);
	switch_set_flag_locked(listener, LFLAG_RUNNING);
	add_listener(listener);

	if (session && switch_test_flag(listener, LFLAG_AUTHED)) {
		switch_event_t *ievent = NULL, *call_event;
		char *event_str;


		switch_set_flag_locked(listener, LFLAG_SESSION);
		status = read_packet(listener, &ievent, 25);

		if (status != SWITCH_STATUS_SUCCESS || !ievent) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Socket Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}

		if (switch_event_create(&call_event, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}

		if (parse_command(listener, ievent, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}

		switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
		switch_channel_event_set_data(channel, call_event);
		switch_event_add_header(call_event, SWITCH_STACK_BOTTOM, "Content-Type", "command/reply");

		switch_event_add_header(call_event, SWITCH_STACK_BOTTOM, "Socket-Mode", switch_test_flag(listener, LFLAG_ASYNC) ? "async" : "static");
		switch_event_add_header(call_event, SWITCH_STACK_BOTTOM, "Control", switch_test_flag(listener, LFLAG_FULL) ? "full" : "single-channel");

		switch_event_serialize(call_event, &event_str, SWITCH_TRUE);
		if (!event_str) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}
		len = strlen(event_str);
		switch_socket_send(listener->sock, event_str, &len);

		switch_safe_free(event_str);
	} else {
		switch_snprintf(buf, sizeof(buf), "Content-Type: auth/request\n\n");

		len = strlen(buf);
		switch_socket_send(listener->sock, buf, &len);

		while (!switch_test_flag(listener, LFLAG_AUTHED)) {
			status = read_packet(listener, &event, 25);
			if (status != SWITCH_STATUS_SUCCESS) {
				goto done;
			}
			if (!event) {
				continue;
			}

			if (parse_command(listener, event, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
				goto done;
			}
			if (*reply != '\0') {
				switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
				len = strlen(buf);
				switch_socket_send(listener->sock, buf, &len);
			}
			break;
		}
	}

	while (switch_test_flag(listener, LFLAG_RUNNING) && listen_list.ready) {
		switch_event_t *revent;

		len = sizeof(buf);
		memset(buf, 0, len);
		status = read_packet(listener, &revent, 0);

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (!revent) {
			continue;
		}

		if (parse_command(listener, revent, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}

		if (*reply != '\0') {
			switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
			len = strlen(buf);
			switch_socket_send(listener->sock, buf, &len);
		}

	}

 done:

	remove_listener(listener);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");

	switch_thread_rwlock_wrlock(listener->rwlock);
	switch_thread_rwlock_unlock(listener->rwlock);
	close_socket(&listener->sock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Closed\n");
	switch_core_hash_destroy(&listener->event_hash);

	if (session) {
		switch_channel_clear_flag(switch_core_session_get_channel(session), CF_CONTROLLED);
		switch_clear_flag_locked(listener, LFLAG_SESSION);
		switch_core_session_rwunlock(session);
	} else if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(listen_list.mutex);
	prefs.threads--;
	switch_mutex_unlock(listen_list.mutex);

	return NULL;
}


/* Create a thread for the socket and launch it */
static void launch_listener_thread(listener_t * listener)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, listener->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, listener_run, listener, listener->pool);
}

static int config(void)
{
	char *cf = "event_socket.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&prefs, 0, sizeof(prefs));

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "listen-ip")) {
					set_pref_ip(val);
				} else if (!strcmp(var, "listen-port")) {
					prefs.port = (uint16_t) atoi(val);
				} else if (!strcmp(var, "password")) {
					set_pref_pass(val);
				}
			}
		}
		switch_xml_free(xml);
	}

	if (switch_strlen_zero(prefs.ip)) {
		set_pref_ip("127.0.0.1");
	}

	if (switch_strlen_zero(prefs.password)) {
		set_pref_pass("ClueCon");
	}

	if (!prefs.port) {
		prefs.port = 8021;
	}

	return 0;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_socket_runtime)
{
	switch_memory_pool_t *pool = NULL, *listener_pool = NULL;
	switch_status_t rv;
	switch_sockaddr_t *sa;
	switch_socket_t *inbound_socket = NULL;
	listener_t *listener;

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&listen_list.mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&listen_list.sock_mutex, SWITCH_MUTEX_NESTED, pool);


	for (;;) {
		rv = switch_sockaddr_info_get(&sa, prefs.ip, SWITCH_INET, prefs.port, 0, pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&listen_list.sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(listen_list.sock, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
		rv = switch_socket_bind(listen_list.sock, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(listen_list.sock, 5);
		if (rv)
			goto sock_fail;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", prefs.ip, prefs.port);
		break;
	sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
		switch_yield(100000);
	}

	listen_list.ready = 1;

	if (switch_event_bind((char *) modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG);


	for(;;) {
		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}

		if ((rv = switch_socket_accept(&inbound_socket, listen_list.sock, listener_pool))) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error\n");
			}
			break;
		}

		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}

		switch_thread_rwlock_create(&listener->rwlock, listener_pool);
		switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);
		switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);

		listener->sock = inbound_socket;
		listener->pool = listener_pool;
		listener_pool = NULL;
		listener->format = EVENT_FORMAT_PLAIN;
		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
		switch_core_hash_init(&listener->event_hash, listener->pool);
		launch_listener_thread(listener);

	}

	close_socket(&listen_list.sock);

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}

 fail:
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
