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
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * mod_event_socket.c -- Socket Controlled Event Handler
 *
 */
#include <switch.h>
#define CMD_BUFLEN 1024 * 1000
#define MAX_QUEUE_LEN 100000
#define MAX_MISSED 500
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
	LFLAG_ASYNC = (1 << 7),
	LFLAG_STATEFUL = (1 << 8),
	LFLAG_OUTBOUND = (1 << 9),
	LFLAG_LINGER = (1 << 10),
	LFLAG_HANDLE_DISCO = (1 << 11),
	LFLAG_CONNECTED = (1 << 12),
	LFLAG_RESUME = (1 << 13),
	LFLAG_AUTH_EVENTS = (1 << 14),
	LFLAG_ALL_EVENTS_AUTHED = (1 << 15),
	LFLAG_ALLOW_LOG = (1 << 16)
} event_flag_t;

typedef enum {
	EVENT_FORMAT_PLAIN,
	EVENT_FORMAT_XML,
	EVENT_FORMAT_JSON
} event_format_t;

struct listener {
	switch_socket_t *sock;
	switch_queue_t *event_queue;
	switch_queue_t *log_queue;
	switch_memory_pool_t *pool;
	event_format_t format;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *filter_mutex;
	uint32_t flags;
	switch_log_level_t level;
	char *ebuf;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	uint8_t allowed_event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	switch_hash_t *allowed_event_hash;
	switch_hash_t *allowed_api_hash;
	switch_thread_rwlock_t *rwlock;
	switch_core_session_t *session;
	int lost_events;
	int lost_logs;
	time_t last_flush;
	time_t expire_time;
	uint32_t timeout;
	uint32_t id;
	switch_sockaddr_t *sa;
	char remote_ip[50];
	switch_port_t remote_port;
	switch_event_t *filters;
	time_t linger_timeout;
	struct listener *next;
	switch_pollfd_t *pollfd;
};

typedef struct listener listener_t;

static struct {
	switch_mutex_t *listener_mutex;
	switch_event_node_t *node;
	int debug;
} globals;

static struct {
	switch_socket_t *sock;
	switch_mutex_t *sock_mutex;
	listener_t *listeners;
	uint8_t ready;
} listen_list;

#define MAX_ACL 100

static struct {
	switch_mutex_t *mutex;
	char *ip;
	uint16_t port;
	char *password;
	int done;
	int threads;
	char *acl[MAX_ACL];
	uint32_t acl_count;
	uint32_t id;
	int nat_map;
	int stop_on_bind_error;
} prefs;


static const char *format2str(event_format_t format)
{
	switch (format) {
	case EVENT_FORMAT_PLAIN:
		return "plain";
	case EVENT_FORMAT_XML:
		return "xml";
	case EVENT_FORMAT_JSON:
		return "json";
	}

	return "invalid";
}

static void remove_listener(listener_t *listener);
static void kill_listener(listener_t *l, const char *message);
static void kill_all_listeners(void);

static uint32_t next_id(void)
{
	uint32_t id;
	switch_mutex_lock(globals.listener_mutex);
	id = ++prefs.id;
	switch_mutex_unlock(globals.listener_mutex);
	return id;
}

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_pass, prefs.password);

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj);
static void launch_listener_thread(listener_t *listener);

static switch_status_t socket_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	listener_t *l;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (switch_test_flag(l, LFLAG_LOG) && l->level >= node->level) {
			switch_log_node_t *dnode = switch_log_node_dup(node);

			if (switch_queue_trypush(l->log_queue, dnode) == SWITCH_STATUS_SUCCESS) {
				if (l->lost_logs) {
					int ll = l->lost_logs;
					l->lost_logs = 0;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d log lines!\n", ll);
				}
			} else {
				switch_log_node_free(&dnode);
				if (++l->lost_logs > MAX_MISSED) {
					kill_listener(l, NULL);
				}
			}
		}
	}
	switch_mutex_unlock(globals.listener_mutex);

	return SWITCH_STATUS_SUCCESS;
}

static void flush_listener(listener_t *listener, switch_bool_t flush_log, switch_bool_t flush_events)
{
	void *pop;

	if (listener->log_queue) {
		while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_log_node_t *dnode = (switch_log_node_t *) pop;
			if (dnode) {
				switch_log_node_free(&dnode);
			}
		}
	}

	if (listener->event_queue) {
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			if (!pop)
				continue;
			switch_event_destroy(&pevent);
		}
	}
}

static switch_status_t expire_listener(listener_t ** listener)
{
	listener_t *l;

	if (!listener || !*listener)
		return SWITCH_STATUS_FALSE;
	l = *listener;

	if (!l->expire_time) {
		l->expire_time = switch_epoch_time_now(NULL);
	}

	if (switch_thread_rwlock_trywrlock(l->rwlock) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(l->session), SWITCH_LOG_CRIT, "Stateful Listener %u has expired\n", l->id);

	flush_listener(*listener, SWITCH_TRUE, SWITCH_TRUE);
	switch_core_hash_destroy(&l->event_hash);

	if (l->allowed_event_hash) {
		switch_core_hash_destroy(&l->allowed_event_hash);
	}

	if (l->allowed_api_hash) {
		switch_core_hash_destroy(&l->allowed_api_hash);
	}


	switch_mutex_lock(l->filter_mutex);
	if (l->filters) {
		switch_event_destroy(&l->filters);
	}

	switch_mutex_unlock(l->filter_mutex);
	switch_thread_rwlock_unlock(l->rwlock);
	switch_core_destroy_memory_pool(&l->pool);

	*listener = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static void event_handler(switch_event_t *event)
{
	switch_event_t *clone = NULL;
	listener_t *l, *lp, *last = NULL;
	time_t now = switch_epoch_time_now(NULL);

	switch_assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}

	switch_mutex_lock(globals.listener_mutex);

	lp = listen_list.listeners;

	while (lp) {
		int send = 0;

		l = lp;
		lp = lp->next;

		if (switch_test_flag(l, LFLAG_STATEFUL) && (l->expire_time || (l->timeout && now - l->last_flush > l->timeout))) {
			if (expire_listener(&l) == SWITCH_STATUS_SUCCESS) {
				if (last) {
					last->next = lp;
				} else {
					listen_list.listeners = lp;
				}
				continue;
			}
		}

		if (l->expire_time || !switch_test_flag(l, LFLAG_EVENTS)) {
			last = l;
			continue;
		}

		if (l->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(l->event_hash, event->subclass_name))) {
				send = 1;
			}
		}

		if (send) {
			switch_mutex_lock(l->filter_mutex);

			if (l->filters && l->filters->headers) {
				switch_event_header_t *hp;
				const char *hval;

				send = 0;
				
				for (hp = l->filters->headers; hp; hp = hp->next) {
					if ((hval = switch_event_get_header(event, hp->name))) {
						const char *comp_to = hp->value;
						int pos = 1, cmp = 0;

						while (comp_to && *comp_to) {
							if (*comp_to == '+') {
								pos = 1;
							} else if (*comp_to == '-') {
								pos = 0;
							} else if (*comp_to != ' ') {
								break;
							}
							comp_to++;
						}

						if (send && pos) {
							continue;
						}

						if (!comp_to) {
							continue;
						}

						if (*hp->value == '/') {
							switch_regex_t *re = NULL;
							int ovector[30];
							cmp = !!switch_regex_perform(hval, comp_to, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
							switch_regex_safe_free(re);
						} else {
							cmp = !strcasecmp(hval, comp_to);
						}

						if (cmp) {
							if (pos) {
								send = 1;
							} else {
								send = 0;
								break;
							}
						}
					}
				}
			}

			switch_mutex_unlock(l->filter_mutex);
		}

		if (send && switch_test_flag(l, LFLAG_MYEVENTS)) {
			char *uuid = switch_event_get_header(event, "unique-id");
			if (!uuid || (l->session && strcmp(uuid, switch_core_session_get_uuid(l->session)))) {
				send = 0;
			}
		}

		if (send) {
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				if (switch_queue_trypush(l->event_queue, clone) == SWITCH_STATUS_SUCCESS) {
					if (l->lost_events) {
						int le = l->lost_events;
						l->lost_events = 0;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(l->session), SWITCH_LOG_CRIT, "Lost %d events!\n", le);
					}
				} else {
					if (++l->lost_events > MAX_MISSED) {
						kill_listener(l, NULL);
					}
					switch_event_destroy(&clone);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(l->session), SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}
		last = l;
	}
	switch_mutex_unlock(globals.listener_mutex);
}

SWITCH_STANDARD_APP(socket_function)
{
	char *host, *port_name, *path;
	switch_socket_t *new_sock;
	switch_sockaddr_t *sa;
	switch_port_t port = 8084;
	listener_t *listener;
	int argc = 0, x = 0;
	char *argv[80] = { 0 };
	char *mydata;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Parse Error!\n");
		return;
	}

	host = argv[0];

	if (zstr(host)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing Host!\n");
		return;
	}

	if ((port_name = strrchr(host, ':'))) {
		*port_name++ = '\0';
		port = (switch_port_t) atoi(port_name);
	}

	if ((path = strchr((port_name ? port_name : host), '/'))) {
		*path++ = '\0';
		switch_channel_set_variable(channel, "socket_path", path);
	}

	switch_channel_set_variable(channel, "socket_host", host);

	if (switch_sockaddr_info_get(&sa, host, SWITCH_UNSPEC, port, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}

	if (switch_socket_create(&new_sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, switch_core_session_get_pool(session))
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}

	switch_socket_opt_set(new_sock, SWITCH_SO_KEEPALIVE, 1);
	switch_socket_opt_set(new_sock, SWITCH_SO_TCP_NODELAY, 1);
	switch_socket_opt_set(new_sock, SWITCH_SO_TCP_KEEPIDLE, 30);
	switch_socket_opt_set(new_sock, SWITCH_SO_TCP_KEEPINTVL, 30);

	if (switch_socket_connect(new_sock, sa) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Socket Error!\n");
		return;
	}


	if (!(listener = switch_core_session_alloc(session, sizeof(*listener)))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Memory Error\n");
		return;
	}

	switch_thread_rwlock_create(&listener->rwlock, switch_core_session_get_pool(session));
	switch_queue_create(&listener->event_queue, MAX_QUEUE_LEN, switch_core_session_get_pool(session));
	switch_queue_create(&listener->log_queue, MAX_QUEUE_LEN, switch_core_session_get_pool(session));

	listener->sock = new_sock;
	listener->pool = switch_core_session_get_pool(session);
	listener->format = EVENT_FORMAT_PLAIN;
	listener->session = session;
	switch_set_flag(listener, LFLAG_ALLOW_LOG);

	switch_socket_create_pollset(&listener->pollfd, listener->sock, SWITCH_POLLIN | SWITCH_POLLERR, listener->pool);

	switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
	switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

	switch_core_hash_init(&listener->event_hash);
	switch_set_flag(listener, LFLAG_AUTHED);
	switch_set_flag(listener, LFLAG_OUTBOUND);
	for (x = 1; x < argc; x++) {
		if (argv[x] && !strcasecmp(argv[x], "full")) {
			switch_set_flag(listener, LFLAG_FULL);
		} else if (argv[x] && !strcasecmp(argv[x], "async")) {
			switch_set_flag(listener, LFLAG_ASYNC);
		}
	}

	if (switch_test_flag(listener, LFLAG_ASYNC)) {
		const char *var;

		launch_listener_thread(listener);

		while (switch_channel_ready(channel) && !switch_test_flag(listener, LFLAG_CONNECTED)) {
			switch_cond_next();
		}

		switch_ivr_park(session, NULL);

		switch_ivr_parse_all_events(session);

		if (switch_test_flag(listener, LFLAG_RESUME) || ((var = switch_channel_get_variable(channel, "socket_resume")) && switch_true(var))) {
			switch_channel_set_state(channel, CS_EXECUTE);
		}

		return;
	} else {
		listener_run(NULL, (void *) listener);
	}

	if (switch_channel_down(channel)) {
		while (switch_test_flag(listener, LFLAG_SESSION)) {
			switch_yield(100000);
		}
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
	int sanity = 0;

	prefs.done = 1;

	kill_all_listeners();
	switch_log_unbind_logger(socket_logger);

	close_socket(&listen_list.sock);

	while (prefs.threads) {
		switch_yield(100000);
		kill_all_listeners();
		if (++sanity >= 200) {
			break;
		}
	}

	switch_event_unbind(&globals.node);

	switch_safe_free(prefs.ip);
	switch_safe_free(prefs.password);

	return SWITCH_STATUS_SUCCESS;
}

static void add_listener(listener_t *listener)
{
	/* add me to the listeners so I get events */
	switch_mutex_lock(globals.listener_mutex);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_mutex_unlock(globals.listener_mutex);
}

static void remove_listener(listener_t *listener)
{
	listener_t *l, *last = NULL;

	switch_mutex_lock(globals.listener_mutex);
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
	switch_mutex_unlock(globals.listener_mutex);
}

static void send_disconnect(listener_t *listener, const char *message)
{
	
	char disco_buf[512] = "";
	switch_size_t len, mlen;

	if (zstr(message)) {
		message = "Disconnected.\n";
	}

	mlen = strlen(message);
	
	if (listener->session) {
		switch_snprintf(disco_buf, sizeof(disco_buf), "Content-Type: text/disconnect-notice\n"
						"Controlled-Session-UUID: %s\n"
						"Content-Disposition: disconnect\n" "Content-Length: %d\n\n", switch_core_session_get_uuid(listener->session), (int)mlen);
	} else {
		switch_snprintf(disco_buf, sizeof(disco_buf), "Content-Type: text/disconnect-notice\nContent-Length: %d\n\n", (int)mlen);
	}

	if (!listener->sock) return;

	len = strlen(disco_buf);
	switch_socket_send(listener->sock, disco_buf, &len);
	if (len > 0) {
		len = mlen;
		switch_socket_send(listener->sock, message, &len);
	}
}

static void kill_listener(listener_t *l, const char *message)
{

	if (message) {
		send_disconnect(l, message);
	}

	switch_clear_flag(l, LFLAG_RUNNING);
	if (l->sock) {
		switch_socket_shutdown(l->sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(l->sock);
	}

}

static void kill_all_listeners(void)
{
	listener_t *l;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		kill_listener(l, "The system is being shut down.\n");
	}
	switch_mutex_unlock(globals.listener_mutex);
}


static listener_t *find_listener(uint32_t id)
{
	listener_t *l, *r = NULL;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l->id && l->id == id && !l->expire_time) {
			if (switch_thread_rwlock_tryrdlock(l->rwlock) == SWITCH_STATUS_SUCCESS) {
				r = l;
			}
			break;
		}
	}
	switch_mutex_unlock(globals.listener_mutex);
	return r;
}

static void strip_cr(char *s)
{
	char *p;
	if ((p = strchr(s, '\r')) || (p = strchr(s, '\n'))) {
		*p = '\0';
	}
}


static void xmlize_listener(listener_t *listener, switch_stream_handle_t *stream)
{
	stream->write_function(stream, " <listener>\n");
	stream->write_function(stream, "  <listen-id>%u</listen-id>\n", listener->id);
	stream->write_function(stream, "  <format>%s</format>\n", format2str(listener->format));
	stream->write_function(stream, "  <timeout>%u</timeout>\n", listener->timeout);
	stream->write_function(stream, " </listener>\n");
}

SWITCH_STANDARD_API(event_sink_function)
{
	char *http = NULL;
	char *wcmd = NULL;
	char *format = NULL;
	listener_t *listener = NULL;

	if (stream->param_event) {
		http = switch_event_get_header(stream->param_event, "http-host");
		wcmd = switch_event_get_header(stream->param_event, "command");
		format = switch_event_get_header(stream->param_event, "format");
	}

	if (!http) {
		stream->write_function(stream, "This is a web application!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!format) {
		format = "xml";
	}

	if (switch_stristr("json", format)) {
		stream->write_function(stream, "Content-Type: application/json\n\n");
	} else {
		stream->write_function(stream, "Content-Type: text/xml\n\n");

		stream->write_function(stream, "<?xml version=\"1.0\"?>\n");
		stream->write_function(stream, "<root>\n");
	}

	if (!wcmd) {
		stream->write_function(stream, "<data><reply type=\"error\">Missing command parameter!</reply></data>\n");
		goto end;
	}

	if (!strcasecmp(wcmd, "filter")) {
		char *action = switch_event_get_header(stream->param_event, "action");
		char *header_name = switch_event_get_header(stream->param_event, "header-name");
		char *header_val = switch_event_get_header(stream->param_event, "header-val");
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if (!(listener = find_listener(idl))) {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Listen-ID</reply></data>\n");
			goto end;
		}

		if (zstr(action)) {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
			goto end;
		}

		switch_mutex_lock(listener->filter_mutex);
		if (!listener->filters) {
			switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
		}

		if (!strcasecmp(action, "delete")) {
			if (zstr(header_val)) {
				stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
				goto filter_end;
			}

			if (!strcasecmp(header_val, "all")) {
				switch_event_destroy(&listener->filters);
				switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
			} else {
				switch_event_del_header(listener->filters, header_val);
			}
			stream->write_function(stream, "<data>\n <reply type=\"success\">filter deleted.</reply>\n<api-command>\n");
		} else if (!strcasecmp(action, "add")) {
			if (zstr(header_name) || zstr(header_val)) {
				stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
				goto filter_end;
			}
			switch_event_add_header_string(listener->filters, SWITCH_STACK_BOTTOM, header_name, header_val);
			stream->write_function(stream, "<data>\n <reply type=\"success\">filter added.</reply>\n<api-command>\n");
		} else {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
		}

	  filter_end:

		switch_mutex_unlock(listener->filter_mutex);

	} else if (!strcasecmp(wcmd, "stop-logging")) {
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if (!(listener = find_listener(idl))) {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Listen-ID</reply></data>\n");
			goto end;
		}

		if (switch_test_flag(listener, LFLAG_LOG)) {
			switch_clear_flag_locked(listener, LFLAG_LOG);
			stream->write_function(stream, "<data><reply type=\"success\">Not Logging</reply></data>\n");
		} else {
			stream->write_function(stream, "<data><reply type=\"error\">Not Logging</reply></data>\n");
		}

		goto end;

	} else if (!strcasecmp(wcmd, "set-loglevel")) {
		char *loglevel = switch_event_get_header(stream->param_event, "loglevel");
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if (!(listener = find_listener(idl))) {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Listen-ID</reply></data>\n");
			goto end;
		}

		if (loglevel) {
			switch_log_level_t ltype = switch_log_str2level(loglevel);
			if (ltype != SWITCH_LOG_INVALID) {
				listener->level = ltype;
				switch_set_flag(listener, LFLAG_LOG);
				stream->write_function(stream, "<data><reply type=\"success\">Log Level %s</reply></data>\n", loglevel);
			} else {
				stream->write_function(stream, "<data><reply type=\"error\">Invalid Level</reply></data>\n");
			}
		} else {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
		}

		goto end;

	} else if (!strcasecmp(wcmd, "create-listener")) {
		char *events = switch_event_get_header(stream->param_event, "events");
		char *loglevel = switch_event_get_header(stream->param_event, "loglevel");
		switch_memory_pool_t *pool;
		char *next, *cur;
		uint32_t count = 0, key_count = 0;
		uint8_t custom = 0;
		char *edup;

		if (zstr(events) && zstr(loglevel)) {
			if (switch_stristr("json", format)) {
				stream->write_function(stream, "{\"reply\": \"error\", \"reply_text\":\"Missing parameter!\"}");
			} else {
				stream->write_function(stream, "<data><reply type=\"error\">Missing parameter!</reply></data>\n");
			}
			goto end;
		}

		switch_core_new_memory_pool(&pool);
		listener = switch_core_alloc(pool, sizeof(*listener));
		listener->pool = pool;
		listener->format = EVENT_FORMAT_PLAIN;
		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
		switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);


		switch_core_hash_init(&listener->event_hash);
		switch_set_flag(listener, LFLAG_AUTHED);
		switch_set_flag(listener, LFLAG_STATEFUL);
		switch_set_flag(listener, LFLAG_ALLOW_LOG);
		switch_queue_create(&listener->event_queue, MAX_QUEUE_LEN, listener->pool);
		switch_queue_create(&listener->log_queue, MAX_QUEUE_LEN, listener->pool);

		if (loglevel) {
			switch_log_level_t ltype = switch_log_str2level(loglevel);
			if (ltype != SWITCH_LOG_INVALID) {
				listener->level = ltype;
				switch_set_flag(listener, LFLAG_LOG);
			}
		}
		switch_thread_rwlock_create(&listener->rwlock, listener->pool);
		listener->id = next_id();
		listener->timeout = 60;
		listener->last_flush = switch_epoch_time_now(NULL);

		if (events) {
			char delim = ',';

			if (switch_stristr("xml", format)) {
				listener->format = EVENT_FORMAT_XML;
			} else if (switch_stristr("json", format)) {
				listener->format = EVENT_FORMAT_JSON;
			} else {
				listener->format = EVENT_FORMAT_PLAIN;
			}

			edup = strdup(events);

			if (strchr(edup, ' ')) {
				delim = ' ';
			}

			for (cur = edup; cur; count++) {
				switch_event_types_t type;

				if ((next = strchr(cur, delim))) {
					*next++ = '\0';
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

				cur = next;
			}


			switch_safe_free(edup);

			if (!key_count) {
				switch_core_hash_destroy(&listener->event_hash);
				switch_core_destroy_memory_pool(&listener->pool);
				if (listener->format == EVENT_FORMAT_JSON) {
					stream->write_function(stream, "{\"reply\": \"error\", \"reply_text\":\"No keywords supplied\"}");
				} else {
					stream->write_function(stream, "<data><reply type=\"error\">No keywords supplied</reply></data>\n");
				}
				goto end;
			}
		}

		switch_set_flag_locked(listener, LFLAG_EVENTS);
		add_listener(listener);
		if (listener->format == EVENT_FORMAT_JSON) {
			cJSON *cj, *cjlistener;
			char *p;

			cj = cJSON_CreateObject();
			cjlistener = cJSON_CreateObject();
			cJSON_AddNumberToObject(cjlistener, "listen-id", listener->id);
			cJSON_AddItemToObject(cjlistener, "format", cJSON_CreateString(format2str(listener->format)));
			cJSON_AddNumberToObject(cjlistener, "timeout", listener->timeout);
			cJSON_AddItemToObject(cj, "listener", cjlistener);
			p = cJSON_Print(cj);
			stream->write_function(stream, p);
			switch_safe_free(p);
			cJSON_Delete(cj);
		} else {
			stream->write_function(stream, "<data>\n");
			stream->write_function(stream, " <reply type=\"success\">Listener %u Created</reply>\n", listener->id);
			xmlize_listener(listener, stream);
			stream->write_function(stream, "</data>\n");
		}

		if (globals.debug > 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Creating event-sink listener [%u]\n", listener->id);
		}

		goto end;
	} else if (!strcasecmp(wcmd, "destroy-listener")) {
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if ((listener = find_listener(idl))) {
			if (globals.debug > 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Destroying event-sink listener [%u]\n", idl);
			}
			stream->write_function(stream, "<data>\n <reply type=\"success\">listener %u destroyed</reply>\n", listener->id);
			xmlize_listener(listener, stream);
			stream->write_function(stream, "</data>\n");
			listener->expire_time = switch_epoch_time_now(NULL);
			goto end;
		} else {
			if (globals.debug > 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Request to destroy unknown event-sink listener [%u]\n", idl);
			}
			stream->write_function(stream, "<data><reply type=\"error\">Can't find listener</reply></data>\n");
			goto end;
		}

	} else if (!strcasecmp(wcmd, "check-listener")) {
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;
		void *pop;
		switch_event_t *pevent = NULL;
		cJSON *cj = NULL, *cjevents = NULL;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if (!(listener = find_listener(idl))) {
			if (switch_stristr("json", format)) {
				stream->write_function(stream, "{\"reply\": \"error\", \"reply_text\":\"Can't find listener\"}");
			} else {
				stream->write_function(stream, "<data><reply type=\"error\">Can't find listener</reply></data>\n");
			}
			goto end;
		}

		listener->last_flush = switch_epoch_time_now(NULL);

		if (listener->format == EVENT_FORMAT_JSON) {
			cJSON *cjlistener;
			cj = cJSON_CreateObject();
			cjlistener = cJSON_CreateObject();
			cJSON_AddNumberToObject(cjlistener, "listen-id", listener->id);
			cJSON_AddItemToObject(cjlistener, "format", cJSON_CreateString(format2str(listener->format)));
			cJSON_AddNumberToObject(cjlistener, "timeout", listener->timeout);
			cJSON_AddItemToObject(cj, "listener", cjlistener);
		} else {
			stream->write_function(stream, "<data>\n <reply type=\"success\">Current Events Follow</reply>\n");
			xmlize_listener(listener, stream);
		}

		if (switch_test_flag(listener, LFLAG_LOG)) {
			stream->write_function(stream, "<log_data>\n");

			while (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				switch_log_node_t *dnode = (switch_log_node_t *) pop;
				size_t encode_len = (strlen(dnode->data) * 3) + 1;
				char *encode_buf = malloc(encode_len);

				switch_assert(encode_buf);

				memset(encode_buf, 0, encode_len);
				switch_url_encode((char *) dnode->data, encode_buf, encode_len);


				stream->write_function(stream,
									   "<log log-level=\"%d\" text-channel=\"%d\" log-file=\"%s\" log-func=\"%s\" log-line=\"%d\" user-data=\"%s\">%s</log>\n",
									   dnode->level, dnode->channel, dnode->file, dnode->func, dnode->line, switch_str_nil(dnode->userdata), encode_buf);
				free(encode_buf);
				switch_log_node_free(&dnode);
			}

			stream->write_function(stream, "</log_data>\n");
		}

		if (listener->format == EVENT_FORMAT_JSON) {
			cjevents = cJSON_CreateArray();
		} else {
			stream->write_function(stream, "<events>\n");
		}

		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			//char *etype;
			pevent = (switch_event_t *) pop;

			if (listener->format == EVENT_FORMAT_PLAIN) {
				//etype = "plain";
				switch_event_serialize(pevent, &listener->ebuf, SWITCH_TRUE);
				stream->write_function(stream, "<event type=\"plain\">\n%s</event>", listener->ebuf);
			} else if (listener->format == EVENT_FORMAT_JSON) {
				//etype = "json";
				cJSON *cjevent = NULL;

				switch_event_serialize_json_obj(pevent, &cjevent);
				cJSON_AddItemToArray(cjevents, cjevent);
			} else {
				switch_xml_t xml;
				//etype = "xml";

				if ((xml = switch_event_xmlize(pevent, SWITCH_VA_NONE))) {
					listener->ebuf = switch_xml_toxml(xml, SWITCH_FALSE);
					switch_xml_free(xml);
				} else {
					stream->write_function(stream, "<data><reply type=\"error\">XML Render Error</reply></data>\n");
					break;
				}

				stream->write_function(stream, "%s\n", listener->ebuf);
			}

			switch_safe_free(listener->ebuf);
			switch_event_destroy(&pevent);
		}

		if (listener->format == EVENT_FORMAT_JSON) {
			char *p = "{}";
			cJSON_AddItemToObject(cj, "events", cjevents);
			p = cJSON_Print(cj);
			if (cj && p) stream->write_function(stream, p);
			switch_safe_free(p);
			cJSON_Delete(cj);
			cj = NULL;
		} else {
			stream->write_function(stream, " </events>\n</data>\n");
		}

		if (pevent) {
			switch_event_destroy(&pevent);
		}

		switch_thread_rwlock_unlock(listener->rwlock);
	} else if (!strcasecmp(wcmd, "exec-fsapi")) {
		char *api_command = switch_event_get_header(stream->param_event, "fsapi-command");
		char *api_args = switch_event_get_header(stream->param_event, "fsapi-args");
		switch_event_t *event, *oevent;

		if (!(api_command)) {
			stream->write_function(stream, "<data><reply type=\"error\">INVALID API COMMAND!</reply></data>\n");
			goto end;
		}

		stream->write_function(stream, "<data>\n <reply type=\"success\">Execute API Command</reply>\n<api-command>\n");
		switch_event_create(&event, SWITCH_EVENT_REQUEST_PARAMS);
		oevent = stream->param_event;
		stream->param_event = event;

		if (!strcasecmp(api_command, "unload") && !strcasecmp(api_args, "mod_event_socket")) {
			api_command = "bgapi";
			api_args = "unload mod_event_socket";
		} else if (!strcasecmp(api_command, "reload") && !strcasecmp(api_args, "mod_event_socket")) {
			api_command = "bgapi";
			api_args = "reload mod_event_socket";
		}

		switch_api_execute(api_command, api_args, NULL, stream);
		stream->param_event = oevent;
		stream->write_function(stream, " </api-command>\n</data>");
	} else {
		stream->write_function(stream, "<data><reply type=\"error\">INVALID COMMAND!</reply></data\n");
	}

  end:

	if (switch_stristr("json", format)) {
	} else {
		stream->write_function(stream, "</root>\n\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_event_socket_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	memset(&globals, 0, sizeof(globals));

	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, pool);

	memset(&listen_list, 0, sizeof(listen_list));
	switch_mutex_init(&listen_list.sock_mutex, SWITCH_MUTEX_NESTED, pool);

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG, SWITCH_FALSE);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "socket", "Connect to a socket", "Connect to a socket", socket_function, "<ip>[:<port>]", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(api_interface, "event_sink", "event_sink", event_sink_function, "<web data>");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t read_packet(listener_t *listener, switch_event_t **event, uint32_t timeout)
{
	switch_size_t mlen, bytes = 0;
	char *mbuf = NULL;
	char buf[1024] = "";
	switch_size_t len;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int count = 0;
	uint32_t elapsed = 0;
	time_t start = 0;
	void *pop;
	char *ptr;
	uint8_t crcount = 0;
	uint32_t max_len = 10485760, block_len = 2048, buf_len = 0;
	switch_channel_t *channel = NULL;
	int clen = 0;

	*event = NULL;

	if (prefs.done) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_zmalloc(mbuf, block_len);
	switch_assert(mbuf);
	buf_len = block_len;

	start = switch_epoch_time_now(NULL);
	ptr = mbuf;

	if (listener->session) {
		channel = switch_core_session_get_channel(listener->session);
	}

	while (listener->sock && !prefs.done) {
		uint8_t do_sleep = 1;
		mlen = 1;

		if (bytes == buf_len - 1) {
			char *tmp;
			int pos;

			pos = (int)(ptr - mbuf);
			buf_len += block_len;
			tmp = realloc(mbuf, buf_len);
			switch_assert(tmp);
			mbuf = tmp;
			memset(mbuf + bytes, 0, buf_len - bytes);
			ptr = (mbuf + pos);

		}
		
		status = switch_socket_recv(listener->sock, ptr, &mlen);

		if (prefs.done || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
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
						switch_event_create(event, SWITCH_EVENT_CLONE);
						switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, "Command", mbuf);
					} else if (cur) {
						char *var, *val;
						var = cur;
						strip_cr(var);
						if (!zstr(var)) {
							if ((val = strchr(var, ':'))) {
								*val++ = '\0';
								while (*val == ' ') {
									val++;
								}
							}
							if (var && val) {
								switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, var, val);
								if (!strcasecmp(var, "content-length")) {
									clen = atoi(val);

									if (clen > 0) {
										char *body;
										char *p;

										switch_zmalloc(body, clen + 1);

										p = body;
										while (clen > 0) {
											mlen = clen;

											status = switch_socket_recv(listener->sock, p, &mlen);

											if (prefs.done || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
												free(body);												
												switch_goto_status(SWITCH_STATUS_FALSE, end);
											}

											/*
											   if (channel && !switch_channel_ready(channel)) {
											   status = SWITCH_STATUS_FALSE;
											   break;
											   }
											 */

											clen -= (int) mlen;
											p += mlen;
										}

										switch_event_add_body(*event, "%s", body);
										free(body);
									}
								}
							}
						}
					}

					cur = next;
				}
				break;
			}
		}

		if (timeout) {
			elapsed = (uint32_t) (switch_epoch_time_now(NULL) - start);
			if (elapsed >= timeout) {
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
				switch_goto_status(SWITCH_STATUS_FALSE, end);
			}
		}

		if (!*mbuf) {
			if (switch_test_flag(listener, LFLAG_LOG)) {
				if (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
					switch_log_node_t *dnode = (switch_log_node_t *) pop;

					if (dnode->data) {
						switch_snprintf(buf, sizeof(buf),
										"Content-Type: log/data\n"
										"Content-Length: %" SWITCH_SSIZE_T_FMT "\n"
										"Log-Level: %d\n"
										"Text-Channel: %d\n"
										"Log-File: %s\n"
										"Log-Func: %s\n"
										"Log-Line: %d\n"
										"User-Data: %s\n"
										"\n",
										strlen(dnode->data),
										dnode->level, dnode->channel, dnode->file, dnode->func, dnode->line, switch_str_nil(dnode->userdata)
							);
						len = strlen(buf);
						switch_socket_send(listener->sock, buf, &len);
						len = strlen(dnode->data);
						switch_socket_send(listener->sock, dnode->data, &len);
					}

					switch_log_node_free(&dnode);
					do_sleep = 0;
				}
			}


			if (listener->session) {
				switch_channel_t *chan = switch_core_session_get_channel(listener->session);
				if (switch_channel_get_state(chan) < CS_HANGUP && switch_channel_test_flag(chan, CF_DIVERT_EVENTS)) {
					switch_event_t *e = NULL;
					while (switch_core_session_dequeue_event(listener->session, &e, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
						if (switch_queue_trypush(listener->event_queue, e) != SWITCH_STATUS_SUCCESS) {
							switch_core_session_queue_event(listener->session, &e);
							break;
						}
					}
				}
			}

			if (switch_test_flag(listener, LFLAG_EVENTS)) {
				while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
					char hbuf[512];
					switch_event_t *pevent = (switch_event_t *) pop;
					char *etype;

					do_sleep = 0;
					if (listener->format == EVENT_FORMAT_PLAIN) {
						etype = "plain";
						switch_event_serialize(pevent, &listener->ebuf, SWITCH_TRUE);
					} else if (listener->format == EVENT_FORMAT_JSON) {
						etype = "json";
						switch_event_serialize_json(pevent, &listener->ebuf);
					} else {
						switch_xml_t xml;
						etype = "xml";

						if ((xml = switch_event_xmlize(pevent, SWITCH_VA_NONE))) {
							listener->ebuf = switch_xml_toxml(xml, SWITCH_FALSE);
							switch_xml_free(xml);
						} else {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session), SWITCH_LOG_ERROR, "XML ERROR!\n");
							goto endloop;
						}
					}

					switch_assert(listener->ebuf);

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

		if (switch_test_flag(listener, LFLAG_HANDLE_DISCO) && 
			listener->linger_timeout != (time_t) -1 && switch_epoch_time_now(NULL) > listener->linger_timeout) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session), SWITCH_LOG_DEBUG, "linger timeout, closing socket\n");
			status = SWITCH_STATUS_FALSE;
			break;
		}

		if (channel && switch_channel_down(channel) && !switch_test_flag(listener, LFLAG_HANDLE_DISCO)) {
			switch_set_flag_locked(listener, LFLAG_HANDLE_DISCO);
			if (switch_test_flag(listener, LFLAG_LINGER)) {
				char disco_buf[512] = "";
				
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(listener->session), SWITCH_LOG_DEBUG, "%s Socket Linger %d\n", 
								  switch_channel_get_name(channel), (int)listener->linger_timeout);
				
				switch_snprintf(disco_buf, sizeof(disco_buf), "Content-Type: text/disconnect-notice\n"
								"Controlled-Session-UUID: %s\n"
								"Content-Disposition: linger\n" 
								"Channel-Name: %s\n"
								"Linger-Time: %d\n"
								"Content-Length: 0\n\n", 
								switch_core_session_get_uuid(listener->session), switch_channel_get_name(channel), (int)listener->linger_timeout);


				if (listener->linger_timeout != (time_t) -1) {
					listener->linger_timeout += switch_epoch_time_now(NULL);
				}
				
				len = strlen(disco_buf);
				switch_socket_send(listener->sock, disco_buf, &len);
			} else {
				status = SWITCH_STATUS_FALSE;
				break;
			}
		}

		if (do_sleep) {
			int fdr = 0;
			switch_poll(listener->pollfd, 1, &fdr, 20000);
		} else {
			switch_os_yield();
		}
	}

 end:

	switch_safe_free(mbuf);
	return status;

}

struct api_command_struct {
	char *api_cmd;
	char *arg;
	listener_t *listener;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int bg;
	int ack;
	int console_execute;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t *thread, void *obj)
{

	struct api_command_struct *acs = (struct api_command_struct *) obj;
	switch_stream_handle_t stream = { 0 };
	char *reply, *freply = NULL;
	switch_status_t status;

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);


	if (!acs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal error.\n");
		goto cleanup;
	}

	if (!acs->listener || !switch_test_flag(acs->listener, LFLAG_RUNNING) ||
		!acs->listener->rwlock || switch_thread_rwlock_tryrdlock(acs->listener->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! cannot get read lock.\n");
		acs->ack = -1;
		goto done;
	}

	acs->ack = 1;

	SWITCH_STANDARD_STREAM(stream);

	if (acs->console_execute) {
		if ((status = switch_console_execute(acs->api_cmd, 0, &stream)) != SWITCH_STATUS_SUCCESS) {
			stream.write_function(&stream, "-ERR %s Command not found!\n", acs->api_cmd);
		}
	} else {
		status = switch_api_execute(acs->api_cmd, acs->arg, NULL, &stream);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		reply = stream.data;
	} else {
		freply = switch_mprintf("-ERR %s Command not found!\n", acs->api_cmd);
		reply = freply;
	}

	if (!reply) {
		reply = "Command returned no output!";
	}

	if (acs->bg) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", acs->uuid_str);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", acs->api_cmd);
			if (acs->arg) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", acs->arg);
			}
			switch_event_add_body(event, "%s", reply);
			switch_event_fire(&event);
		}
	} else {
		switch_size_t rlen, blen;
		char buf[1024] = "";

		if (!(rlen = strlen(reply))) {
			reply = "-ERR no reply\n";
			rlen = strlen(reply);
		}

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

	if (acs->bg) {
		switch_memory_pool_t *pool = acs->pool;
		if (acs->ack == -1) {
			int sanity = 2000;
			while (acs->ack == -1) {
				switch_cond_next();
				if (--sanity <= 0)
					break;
			}
		}

		acs = NULL;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;

	}

  cleanup:
	switch_mutex_lock(globals.listener_mutex);
	prefs.threads--;
	switch_mutex_unlock(globals.listener_mutex);

	return NULL;

}

static switch_bool_t auth_api_command(listener_t *listener, const char *api_cmd, const char *arg)
{
	const char *check_cmd = api_cmd;
	char *sneaky_commands[] = { "bgapi", "sched_api", "eval", "expand", "xml_wrap", NULL };
	int x = 0;
	char *dup_arg = NULL;
	char *next = NULL;
	switch_bool_t ok = SWITCH_TRUE;

  top:
	
	if (!switch_core_hash_find(listener->allowed_api_hash, check_cmd)) {
		ok = SWITCH_FALSE;
		goto end;
	}

	while (check_cmd) {
		for (x = 0; sneaky_commands[x]; x++) {
			if (!strcasecmp(sneaky_commands[x], check_cmd)) {
				if (check_cmd == api_cmd) {
					if (arg) {
						switch_safe_free(dup_arg);
						dup_arg = strdup(arg);
						check_cmd = dup_arg;
						if ((next = strchr(check_cmd, ' '))) {
							*next++ = '\0';
						}
					} else {
						break;
					}
				} else {
					if (next) {
						check_cmd = next;
					} else {
						check_cmd = dup_arg;
					}

					if ((next = strchr(check_cmd, ' '))) {
						*next++ = '\0';
					}
				}
				goto top;
			}
		}
		break;
	}

  end:

	switch_safe_free(dup_arg);
	return ok;

}

static switch_status_t parse_command(listener_t *listener, switch_event_t **event, char *reply, uint32_t reply_len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cmd = NULL;
	char unload_cheat[] = "api bgapi unload mod_event_socket";
	char reload_cheat[] = "api bgapi reload mod_event_socket";

	*reply = '\0';

	if (!event || !*event || !(cmd = switch_event_get_header(*event, "command"))) {
		switch_clear_flag_locked(listener, LFLAG_RUNNING);
		switch_snprintf(reply, reply_len, "-ERR command parse error.");
		goto done;
	}

	if (switch_stristr("unload", cmd) && switch_stristr("mod_event_socket", cmd)) {
		cmd = unload_cheat;
	} else if (switch_stristr("reload", cmd) && switch_stristr("mod_event_socket", cmd)) {
		cmd = reload_cheat;
	}

	if (!strncasecmp(cmd, "exit", 4) || !strncasecmp(cmd, "...", 3)) {
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

		if (!strncasecmp(cmd, "userauth ", 9)) {
			const char *passwd;
			const char *allowed_api;
			const char *allowed_events;
			switch_event_t *params;
			char *user = NULL, *domain_name = NULL, *pass = NULL;
			switch_xml_t x_domain = NULL, x_domain_root, x_user = NULL, x_params, x_param, x_group = NULL;
			int authed = 0;
			char *edup = NULL;
			char event_reply[512] = "Allowed-Events: all\n";
			char api_reply[512] = "Allowed-API: all\n";
			char log_reply[512] = "";
			int allowed_log = 1;
			char *tmp;

			switch_clear_flag(listener, LFLAG_ALLOW_LOG);

			strip_cr(cmd);

			user = cmd + 9;

			if (user && (domain_name = strchr(user, '@'))) {
				*domain_name++ = '\0';
			}

			if (domain_name && (pass = strchr(domain_name, ':'))) {
				*pass++ = '\0';
			}

			if ((tmp = strchr(user, ':'))) {
				*tmp++ = '\0';
				pass = tmp;
			}
			
			if (zstr(user) || zstr(domain_name)) {
				switch_snprintf(reply, reply_len, "-ERR invalid");
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
				goto done;
			}


			passwd = NULL;
			allowed_events = NULL;
			allowed_api = NULL;

			params = NULL;
			x_domain_root = NULL;


			switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
			switch_assert(params);
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "event_socket_auth");

			if (switch_xml_locate_user("id", user, domain_name, NULL, &x_domain_root, &x_domain, &x_user, &x_group, params) == SWITCH_STATUS_SUCCESS) {
				switch_xml_t list[3];
				int x = 0;

				list[0] = x_domain;
				list[1] = x_group;
				list[2] = x_user;

				for (x = 0; x < 3; x++) {
					if ((x_params = switch_xml_child(list[x], "params"))) {
						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr_soft(x_param, "name");
							const char *val = switch_xml_attr_soft(x_param, "value");

							if (!strcasecmp(var, "esl-password")) {
								passwd = val;
							} else if (!strcasecmp(var, "esl-allowed-log")) {
								allowed_log = switch_true(val);
							} else if (!strcasecmp(var, "esl-allowed-events")) {
								allowed_events = val;
							} else if (!strcasecmp(var, "esl-allowed-api")) {
								allowed_api = val;
							}
						}
					}
				}
			} else {
				authed = 0;
				goto bot;
			}

			if (!zstr(passwd) && !zstr(pass) && !strcmp(passwd, pass)) {
				authed = 1;

				if (allowed_events) {
					char delim = ',';
					char *cur, *next;
					int count = 0, custom = 0, key_count = 0;

					switch_set_flag(listener, LFLAG_AUTH_EVENTS);

					switch_snprintf(event_reply, sizeof(event_reply), "Allowed-Events: %s\n", allowed_events);

					switch_core_hash_init(&listener->allowed_event_hash);

					edup = strdup(allowed_events);
					cur = edup;

					if (strchr(edup, ' ')) {
						delim = ' ';
					}

					for (cur = edup; cur; count++) {
						switch_event_types_t type;

						if ((next = strchr(cur, delim))) {
							*next++ = '\0';
						}

						if (custom) {
							switch_core_hash_insert(listener->allowed_event_hash, cur, MARKER);
						} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
							key_count++;
							if (type == SWITCH_EVENT_ALL) {
								uint32_t x = 0;
								switch_set_flag(listener, LFLAG_ALL_EVENTS_AUTHED);
								for (x = 0; x < SWITCH_EVENT_ALL; x++) {
									listener->allowed_event_list[x] = 1;
								}
							}
							if (type <= SWITCH_EVENT_ALL) {
								listener->allowed_event_list[type] = 1;
							}

							if (type == SWITCH_EVENT_CUSTOM) {
								custom++;
							}
						}

						cur = next;
					}

					switch_safe_free(edup);
				}

				switch_snprintf(log_reply, sizeof(log_reply), "Allowed-LOG: %s\n", allowed_log ? "true" : "false");

				if (allowed_log) {
					switch_set_flag(listener, LFLAG_ALLOW_LOG);
				}

				if (allowed_api) {
					char delim = ',';
					char *cur, *next;
					int count = 0;

					switch_snprintf(api_reply, sizeof(api_reply), "Allowed-API: %s\n", allowed_api);

					switch_core_hash_init(&listener->allowed_api_hash);

					edup = strdup(allowed_api);
					cur = edup;

					if (strchr(edup, ' ')) {
						delim = ' ';
					}

					for (cur = edup; cur; count++) {
						if ((next = strchr(cur, delim))) {
							*next++ = '\0';
						}

						switch_core_hash_insert(listener->allowed_api_hash, cur, MARKER);

						cur = next;
					}

					switch_safe_free(edup);
				}

			}


		  bot:

			if (params) {
				switch_event_destroy(&params);
			}

			if (authed) {
				switch_set_flag_locked(listener, LFLAG_AUTHED);
				switch_snprintf(reply, reply_len, "~Reply-Text: +OK accepted\n%s%s%s\n", event_reply, api_reply, log_reply);
			} else {
				switch_snprintf(reply, reply_len, "-ERR invalid");
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
			}

			if (x_domain_root) {
				switch_xml_free(x_domain_root);
			}

		}

		goto done;
	}

	if (!strncasecmp(cmd, "filter ", 7)) {
		char *header_name = cmd + 7;
		char *header_val = NULL;

		strip_cr(header_name);

		while (header_name && *header_name && *header_name == ' ')
			header_name++;

		if ((header_val = strchr(header_name, ' '))) {
			*header_val++ = '\0';
		}

		switch_mutex_lock(listener->filter_mutex);
		if (!listener->filters) {
			switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
			switch_clear_flag(listener->filters, EF_UNIQ_HEADERS);
		}

		if (!strcasecmp(header_name, "delete") && header_val) {
			header_name = header_val;
			if ((header_val = strchr(header_name, ' '))) {
				*header_val++ = '\0';
			}
			if (!strcasecmp(header_name, "all")) {
				switch_event_destroy(&listener->filters);
				switch_event_create_plain(&listener->filters, SWITCH_EVENT_CLONE);
			} else {
				switch_event_del_header_val(listener->filters, header_name, header_val);
			}
			switch_snprintf(reply, reply_len, "+OK filter deleted. [%s][%s]", header_name, switch_str_nil(header_val));
		} else if (header_val) {
			if (!strcasecmp(header_name, "add")) {
				header_name = header_val;
				if ((header_val = strchr(header_name, ' '))) {
					*header_val++ = '\0';
				}
			}
			switch_event_add_header_string(listener->filters, SWITCH_STACK_BOTTOM, header_name, header_val);
			switch_snprintf(reply, reply_len, "+OK filter added. [%s]=[%s]", header_name, header_val);
		} else {
			switch_snprintf(reply, reply_len, "-ERR invalid syntax");
		}
		switch_mutex_unlock(listener->filter_mutex);

		goto done;
	}

	if (listener->session && !strncasecmp(cmd, "resume", 6)) {
		switch_set_flag_locked(listener, LFLAG_RESUME);
		switch_channel_set_variable(switch_core_session_get_channel(listener->session), "socket_resume", "true");
		switch_snprintf(reply, reply_len, "+OK");
		goto done;
	}

	if (listener->session || !strncasecmp(cmd, "myevents ", 9)) {
		switch_channel_t *channel = NULL;

		if (listener->session) {
			channel = switch_core_session_get_channel(listener->session);
		}

		if (!strncasecmp(cmd, "connect", 7)) {
			switch_event_t *call_event;
			char *event_str;
			switch_size_t len;

			switch_set_flag_locked(listener, LFLAG_CONNECTED);
			switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA);

			if (channel) {
				switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
				switch_channel_event_set_data(channel, call_event);
			}
			switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Content-Type", "command/reply");
			switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Reply-Text", "+OK\n");
			switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Socket-Mode", switch_test_flag(listener, LFLAG_ASYNC) ? "async" : "static");
			switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Control", switch_test_flag(listener, LFLAG_FULL) ? "full" : "single-channel");

			switch_event_serialize(call_event, &event_str, SWITCH_TRUE);
			switch_assert(event_str);
			len = strlen(event_str);
			switch_socket_send(listener->sock, event_str, &len);
			switch_safe_free(event_str);
			switch_event_destroy(&call_event);
			//switch_snprintf(reply, reply_len, "+OK");
			goto done_noreply;
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
			if (switch_test_flag(listener, LFLAG_MYEVENTS)) {
				switch_snprintf(reply, reply_len, "-ERR aready enabled.");
				goto done;
			}

			if (!listener->session) {
				char *uuid;

				if ((uuid = cmd + 9)) {
					char *fmt;
					strip_cr(uuid);
					
					if ((fmt = strchr(uuid, ' '))) {
						*fmt++ = '\0';
					}
						
					if (!(listener->session = switch_core_session_locate(uuid))) {
						if (fmt) {
							switch_snprintf(reply, reply_len, "-ERR invalid uuid");
							goto done;
						} else {
							fmt = uuid;
						}
					}

					if ((fmt = strchr(uuid, ' '))) {
						if (!strcasecmp(fmt, "xml")) {
							listener->format = EVENT_FORMAT_XML;
						} else if (!strcasecmp(fmt, "plain")) {
							listener->format = EVENT_FORMAT_PLAIN;
						} else if (!strcasecmp(fmt, "json")) {
							listener->format = EVENT_FORMAT_JSON;
						}						
					}

					switch_set_flag_locked(listener, LFLAG_SESSION);
					switch_set_flag_locked(listener, LFLAG_ASYNC);
				}


			}

			listener->event_list[SWITCH_EVENT_CHANNEL_ANSWER] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_APPLICATION] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_BRIDGE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_CREATE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_DATA] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_DESTROY] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_EXECUTE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_HANGUP] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_ORIGINATE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_UUID] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_OUTGOING] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_PARK] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_PROGRESS] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_STATE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_UNBRIDGE] = 1;
			listener->event_list[SWITCH_EVENT_CHANNEL_UNPARK] = 1;
			listener->event_list[SWITCH_EVENT_DETECTED_SPEECH] = 1;
			listener->event_list[SWITCH_EVENT_DTMF] = 1;
			listener->event_list[SWITCH_EVENT_NOTALK] = 1;
			listener->event_list[SWITCH_EVENT_TALK] = 1;
			switch_set_flag_locked(listener, LFLAG_MYEVENTS);
			switch_set_flag_locked(listener, LFLAG_EVENTS);
			if (strstr(cmd, "xml") || strstr(cmd, "XML")) {
				listener->format = EVENT_FORMAT_XML;
			}
			if (strstr(cmd, "json") || strstr(cmd, "JSON")) {
				listener->format = EVENT_FORMAT_JSON;
			}
			switch_snprintf(reply, reply_len, "+OK Events Enabled");
			goto done;
		}
	}


	if (!strncasecmp(cmd, "divert_events", 13)) {
		char *onoff = cmd + 13;
		switch_channel_t *channel;

		if (!listener->session) {
			switch_snprintf(reply, reply_len, "-ERR not controlling a session.");
			goto done;
		}

		channel = switch_core_session_get_channel(listener->session);

		if (onoff) {
			while (*onoff == ' ') {
				onoff++;
			}

			if (*onoff == '\r' || *onoff == '\n') {
				onoff = NULL;
			} else {
				strip_cr(onoff);
			}
		}

		if (zstr(onoff)) {
			switch_snprintf(reply, reply_len, "-ERR missing value.");
			goto done;
		}


		if (!strcasecmp(onoff, "on")) {
			switch_snprintf(reply, reply_len, "+OK events diverted");
			switch_channel_set_flag(channel, CF_DIVERT_EVENTS);
		} else {
			switch_snprintf(reply, reply_len, "+OK events not diverted");
			switch_channel_clear_flag(channel, CF_DIVERT_EVENTS);
		}

		goto done;

	}

	if (!strncasecmp(cmd, "sendmsg", 7)) {
		switch_core_session_t *session;
		char *uuid = cmd + 7;
		const char *async_var = switch_event_get_header(*event, "async");
		int async = switch_test_flag(listener, LFLAG_ASYNC);

		if (switch_true(async_var)) {
			async = 1;
		}

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

		if (zstr(uuid)) {
			uuid = switch_event_get_header(*event, "session-id");
		}

		if (uuid && listener->session && !strcmp(uuid, switch_core_session_get_uuid(listener->session))) {
			uuid = NULL;
		}

		if (zstr(uuid) && listener->session) {
			if (async) {
				if ((status = switch_core_session_queue_private_event(listener->session, event, SWITCH_FALSE)) == SWITCH_STATUS_SUCCESS) {
					switch_snprintf(reply, reply_len, "+OK");
				} else {
					switch_snprintf(reply, reply_len, "-ERR memory error");
				}
			} else {
				switch_ivr_parse_event(listener->session, *event);
				switch_snprintf(reply, reply_len, "+OK");
			}
		} else {
			if (!zstr(uuid) && (session = switch_core_session_locate(uuid))) {
				if ((status = switch_core_session_queue_private_event(session, event, SWITCH_FALSE)) == SWITCH_STATUS_SUCCESS) {
					switch_snprintf(reply, reply_len, "+OK");
				} else {
					switch_snprintf(reply, reply_len, "-ERR memory error");
				}
				switch_core_session_rwunlock(session);
			} else {
				switch_snprintf(reply, reply_len, "-ERR invalid session id [%s]", switch_str_nil(uuid));
			}
		}

		goto done;

	}

	if (switch_test_flag(listener, LFLAG_OUTBOUND) && !switch_test_flag(listener, LFLAG_FULL)) {
		goto done;
	}


	if (!strncasecmp(cmd, "sendevent", 9)) {
		char *ename;
		const char *uuid = NULL;
		char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		switch_uuid_str(uuid_str, sizeof(uuid_str));
		
		switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, "Event-UUID", uuid_str);

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
				const char *subclass_name = switch_event_get_header(*event, "Event-Subclass");
				(*event)->event_id = etype;

				if (etype == SWITCH_EVENT_CUSTOM && subclass_name) {
					switch_event_set_subclass_name(*event, subclass_name);
				}
			}
		}

		if ((uuid = switch_event_get_header(*event, "unique-id"))) {
			switch_core_session_t *dsession;

			if ((dsession = switch_core_session_locate(uuid))) {
				switch_core_session_queue_event(dsession, event);
				switch_core_session_rwunlock(dsession);
			}
		}

		if (*event) {
			switch_event_prep_for_delivery(*event);
			switch_event_fire(event);
		}
		switch_snprintf(reply, reply_len, "+OK %s", uuid_str);
		goto done;
	} else if (!strncasecmp(cmd, "api ", 4)) {
		struct api_command_struct acs = { 0 };
		char *console_execute = switch_event_get_header(*event, "console_execute");

		char *api_cmd = cmd + 4;
		char *arg = NULL;
		strip_cr(api_cmd);

		if (listener->allowed_api_hash) {
			char *api_copy = strdup(api_cmd);
			char *arg_copy = NULL;
			int ok = 0;

			if ((arg_copy = strchr(api_copy, ' '))) {
				*arg_copy++ = '\0';
			}
			
			ok = auth_api_command(listener, api_copy, arg_copy);
			free(api_copy);

			if (!ok) {
				switch_snprintf(reply, reply_len, "-ERR permission denied");
				status = SWITCH_STATUS_SUCCESS;
				goto done;
			}
		}
		
		if (!(acs.console_execute = switch_true(console_execute))) {
			if ((arg = strchr(api_cmd, ' '))) {
				*arg++ = '\0';
			}
		}
		
		acs.listener = listener;
		acs.api_cmd = api_cmd;
		acs.arg = arg;
		acs.bg = 0;


		api_exec(NULL, (void *) &acs);

		status = SWITCH_STATUS_SUCCESS;
		goto done_noreply;
	} else if (!strncasecmp(cmd, "bgapi ", 6)) {
		struct api_command_struct *acs = NULL;
		char *api_cmd = cmd + 6;
		char *arg = NULL;
		char *uuid_str = NULL;
		switch_memory_pool_t *pool;
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;
		switch_uuid_t uuid;
		int sanity;

		strip_cr(api_cmd);

		if ((arg = strchr(api_cmd, ' '))) {
			*arg++ = '\0';
		}

		if (listener->allowed_api_hash) {
			if (!auth_api_command(listener, api_cmd, arg)) {
				switch_snprintf(reply, reply_len, "-ERR permission denied");
				status = SWITCH_STATUS_SUCCESS;
				goto done;
			}
		}

		switch_core_new_memory_pool(&pool);
		acs = switch_core_alloc(pool, sizeof(*acs));
		switch_assert(acs);
		acs->pool = pool;
		acs->listener = listener;
		acs->console_execute = 0;

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

		if ((uuid_str = switch_event_get_header(*event, "job-uuid"))) {
			switch_copy_string(acs->uuid_str, uuid_str, sizeof(acs->uuid_str));
		} else {
			switch_uuid_get(&uuid);
			switch_uuid_format(acs->uuid_str, &uuid);
		}
		switch_snprintf(reply, reply_len, "~Reply-Text: +OK Job-UUID: %s\nJob-UUID: %s\n\n", acs->uuid_str, acs->uuid_str);
		switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);
		sanity = 2000;
		while (!acs->ack) {
			switch_cond_next();
			if (--sanity <= 0)
				break;
		}
		if (acs->ack == -1) {
			acs->ack--;
		}

		status = SWITCH_STATUS_SUCCESS;
		goto done_noreply;
	} else if (!strncasecmp(cmd, "log", 3)) {

		char *level_s;
		switch_log_level_t ltype = SWITCH_LOG_DEBUG;

		if (!switch_test_flag(listener, LFLAG_ALLOW_LOG)) {
			switch_snprintf(reply, reply_len, "-ERR permission denied");
			goto done;
		}
		//pull off the first newline/carriage return
		strip_cr(cmd);

		//move past the command
		level_s = cmd + 3;

		//see if we got an argument
		if (!zstr(level_s)) {
			//if so move to the argument
			level_s++;
		}
		//see if we lined up on an argument or not
		if (!zstr(level_s)) {
			ltype = switch_log_str2level(level_s);
		}

		if (ltype != SWITCH_LOG_INVALID) {
			listener->level = ltype;
			switch_set_flag(listener, LFLAG_LOG);
			switch_snprintf(reply, reply_len, "+OK log level %s [%d]", level_s, listener->level);
		} else {
			switch_snprintf(reply, reply_len, "-ERR invalid log level");
		}
	} else if (!strncasecmp(cmd, "linger", 6)) {
		if (listener->session) {
			time_t linger_time = 600; /* sounds reasonable? */
			if (*(cmd+6) == ' ' && *(cmd+7)) { /*how long do you want to linger?*/
				linger_time = (time_t) atoi(cmd+7);
			} else {
				linger_time = (time_t) -1;
			}

			listener->linger_timeout = linger_time;
			switch_set_flag_locked(listener, LFLAG_LINGER);
			if (listener->linger_timeout != (time_t) -1) {
				switch_snprintf(reply, reply_len, "+OK will linger %d seconds", (int)linger_time);
			} else {
				switch_snprintf(reply, reply_len, "+OK will linger");
			}
		} else {
			switch_snprintf(reply, reply_len, "-ERR not controlling a session");
		}
	} else if (!strncasecmp(cmd, "nolinger", 8)) {
		if (listener->session) {
			switch_clear_flag_locked(listener, LFLAG_LINGER);
			switch_snprintf(reply, reply_len, "+OK will not linger");
		} else {
			switch_snprintf(reply, reply_len, "-ERR not controlling a session");
		}
	} else if (!strncasecmp(cmd, "nolog", 5)) {
		flush_listener(listener, SWITCH_TRUE, SWITCH_FALSE);
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
					} else if (!strcasecmp(cur, "json")) {
						listener->format = EVENT_FORMAT_JSON;
						goto end;
					}
				}


				if (custom) {
					if (!listener->allowed_event_hash || switch_core_hash_find(listener->allowed_event_hash, cur)) {
						switch_core_hash_insert(listener->event_hash, cur, MARKER);
					} else {
						switch_snprintf(reply, reply_len, "-ERR permission denied");
						goto done;
					}
				} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
					if (switch_test_flag(listener, LFLAG_AUTH_EVENTS) && !listener->allowed_event_list[type] &&
						!switch_test_flag(listener, LFLAG_ALL_EVENTS_AUTHED)) {
						switch_snprintf(reply, reply_len, "-ERR permission denied");
						goto done;
					}

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

		switch_snprintf(reply, reply_len, "+OK event listener enabled %s", format2str(listener->format));

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
		flush_listener(listener, SWITCH_FALSE, SWITCH_TRUE);

		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}
			/* wipe the hash */
			switch_core_hash_destroy(&listener->event_hash);
			switch_core_hash_init(&listener->event_hash);
			switch_snprintf(reply, reply_len, "+OK no longer listening for events");
		} else {
			switch_snprintf(reply, reply_len, "-ERR not listening for events");
		}
	}

  done:

	if (zstr(reply)) {
		switch_snprintf(reply, reply_len, "-ERR command not found");
	}

  done_noreply:

	if (event) {
		switch_event_destroy(event);
	}

	return status;
}

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	char buf[1024];
	switch_size_t len;
	switch_status_t status;
	switch_event_t *event;
	char reply[512] = "";
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	switch_event_t *revent = NULL;
	const char *var;
	int locked = 1;

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);

	switch_assert(listener != NULL);

	if ((session = listener->session)) {
		if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
			locked = 0;
			goto done;
		}
	}

	switch_socket_opt_set(listener->sock, SWITCH_SO_TCP_NODELAY, TRUE);
	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);

	if (prefs.acl_count && listener->sa && !zstr(listener->remote_ip)) {
		uint32_t x = 0;

		for (x = 0; x < prefs.acl_count; x++) {
			if (!switch_check_network_list_ip(listener->remote_ip, prefs.acl[x])) {
				const char message[] = "Access Denied, go away.\n";
				int mlen = (int)strlen(message);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "IP %s Rejected by acl \"%s\"\n", listener->remote_ip,
								  prefs.acl[x]);

				switch_snprintf(buf, sizeof(buf), "Content-Type: text/rude-rejection\nContent-Length: %d\n\n", mlen);
				len = strlen(buf);
				switch_socket_send(listener->sock, buf, &len);
				len = mlen;
				switch_socket_send(listener->sock, message, &len);
				goto done;
			}
		}
	}

	if (globals.debug > 0) {
		if (zstr(listener->remote_ip)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Open\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Open from %s:%d\n", listener->remote_ip,
							  listener->remote_port);
		}
	}

	switch_socket_opt_set(listener->sock, SWITCH_SO_NONBLOCK, TRUE);
	switch_set_flag_locked(listener, LFLAG_RUNNING);
	add_listener(listener);

	if (session && switch_test_flag(listener, LFLAG_AUTHED)) {
		switch_event_t *ievent = NULL;

		switch_set_flag_locked(listener, LFLAG_SESSION);
		status = read_packet(listener, &ievent, 25);

		if (status != SWITCH_STATUS_SUCCESS || !ievent) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Socket Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}


		if (parse_command(listener, &ievent, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}


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

			if (parse_command(listener, &event, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
				goto done;
			}
			if (*reply != '\0') {
				if (*reply == '~') {
					switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\n%s", reply + 1);
				} else {
					switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
				}
				len = strlen(buf);
				switch_socket_send(listener->sock, buf, &len);
			}
			break;
		}
	}

	while (!prefs.done && switch_test_flag(listener, LFLAG_RUNNING) && listen_list.ready) {
		len = sizeof(buf);
		memset(buf, 0, len);
		status = read_packet(listener, &revent, 0);

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (!revent) {
			continue;
		}

		if (parse_command(listener, &revent, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}

		if (revent) {
			switch_event_destroy(&revent);
		}

		if (*reply != '\0') {
			if (*reply == '~') {
				switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\n%s", reply + 1);
			} else {
				switch_snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
			}
			len = strlen(buf);
			switch_socket_send(listener->sock, buf, &len);
		}

	}

  done:

	if (revent) {
		switch_event_destroy(&revent);
	}

	remove_listener(listener);

	if (globals.debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");
	}

	switch_thread_rwlock_wrlock(listener->rwlock);
	flush_listener(listener, SWITCH_TRUE, SWITCH_TRUE);
	switch_mutex_lock(listener->filter_mutex);
	if (listener->filters) {
		switch_event_destroy(&listener->filters);
	}
	switch_mutex_unlock(listener->filter_mutex);

	if (listener->session) {
		channel = switch_core_session_get_channel(listener->session);
	}

	if (channel && (switch_test_flag(listener, LFLAG_RESUME) || ((var = switch_channel_get_variable(channel, "socket_resume")) && switch_true(var)))) {
		switch_channel_set_state(channel, CS_RESET);
	}

	if (listener->sock) {
		send_disconnect(listener, "Disconnected, goodbye.\nSee you at ClueCon! http://www.cluecon.com/\n");
		close_socket(&listener->sock);
	}

	switch_thread_rwlock_unlock(listener->rwlock);

	if (globals.debug > 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Connection Closed\n");
	}

	switch_core_hash_destroy(&listener->event_hash);

	if (listener->allowed_event_hash) {
		switch_core_hash_destroy(&listener->allowed_event_hash);
	}

	if (listener->allowed_api_hash) {
		switch_core_hash_destroy(&listener->allowed_api_hash);
	}

	if (listener->session) {
		switch_channel_clear_flag(switch_core_session_get_channel(listener->session), CF_CONTROLLED);
		switch_clear_flag_locked(listener, LFLAG_SESSION);
		if (locked) {
			switch_core_session_rwunlock(listener->session);
		}
	} else if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads--;
	switch_mutex_unlock(globals.listener_mutex);

	return NULL;
}


/* Create a thread for the socket and launch it */
static void launch_listener_thread(listener_t *listener)
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "listen-ip")) {
					set_pref_ip(val);
				} else if (!strcmp(var, "debug")) {
					globals.debug = atoi(val);
				} else if (!strcmp(var, "nat-map")) {
					if (switch_true(val) && switch_nat_get_type()) {
						prefs.nat_map = 1;
					}
				} else if (!strcmp(var, "listen-port")) {
					prefs.port = (uint16_t) atoi(val);
				} else if (!strcmp(var, "password")) {
					set_pref_pass(val);
				} else if (!strcasecmp(var, "apply-inbound-acl") && ! zstr(val)) {
					if (prefs.acl_count < MAX_ACL) {
						prefs.acl[prefs.acl_count++] = strdup(val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", MAX_ACL);
					}
				} else if (!strcasecmp(var, "stop-on-bind-error")) {
					prefs.stop_on_bind_error = switch_true(val) ? 1 : 0;
				}
			}
		}
		switch_xml_free(xml);
	}

	if (zstr(prefs.ip)) {
		set_pref_ip("127.0.0.1");
	}

	if (zstr(prefs.password)) {
		set_pref_pass("ClueCon");
	}

	if (!prefs.nat_map) {
		prefs.nat_map = 0;
	}

	if (prefs.nat_map) {
		prefs.nat_map = 0;
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
	uint32_t x = 0;
	uint32_t errs = 0;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	config();

	while (!prefs.done) {
		rv = switch_sockaddr_info_get(&sa, prefs.ip, SWITCH_UNSPEC, prefs.port, 0, pool);
		if (rv)
			goto fail;
		rv = switch_socket_create(&listen_list.sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool);
		if (rv)
			goto sock_fail;
		rv = switch_socket_opt_set(listen_list.sock, SWITCH_SO_REUSEADDR, 1);
		if (rv)
			goto sock_fail;
#ifdef WIN32
		/* Enable dual-stack listening on Windows (if the listening address is IPv6), it's default on Linux */
		if (switch_sockaddr_get_family(sa) == AF_INET6) {
			rv = switch_socket_opt_set(listen_list.sock, 16384, 0);
			if (rv) goto sock_fail;
		}
#endif
		rv = switch_socket_bind(listen_list.sock, sa);
		if (rv)
			goto sock_fail;
		rv = switch_socket_listen(listen_list.sock, 5);
		if (rv)
			goto sock_fail;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", prefs.ip, prefs.port);

		if (prefs.nat_map) {
			switch_nat_add_mapping(prefs.port, SWITCH_NAT_TCP, NULL, SWITCH_FALSE);
		}

		break;
	  sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not listen on %s:%u\n", prefs.ip, prefs.port);
		if (prefs.stop_on_bind_error) {
			prefs.done = 1;
			goto fail;
		}
		switch_yield(100000);
	}

	listen_list.ready = 1;


	while (!prefs.done) {
		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}


		if ((rv = switch_socket_accept(&inbound_socket, listen_list.sock, listener_pool))) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
				goto end;
			} else {
				/* I wish we could use strerror_r here but its not defined everywhere =/ */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error [%s]\n", strerror(errno));
				if (++errs > 100) {
					goto end;
				}
			}
		} else {
			errs = 0;
		}


		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}

		switch_thread_rwlock_create(&listener->rwlock, listener_pool);
		switch_queue_create(&listener->event_queue, MAX_QUEUE_LEN, listener_pool);
		switch_queue_create(&listener->log_queue, MAX_QUEUE_LEN, listener_pool);

		listener->sock = inbound_socket;
		listener->pool = listener_pool;
		listener_pool = NULL;
		listener->format = EVENT_FORMAT_PLAIN;
		switch_set_flag(listener, LFLAG_FULL);
		switch_set_flag(listener, LFLAG_ALLOW_LOG);

		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
		switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_core_hash_init(&listener->event_hash);
		switch_socket_create_pollset(&listener->pollfd, listener->sock, SWITCH_POLLIN | SWITCH_POLLERR, listener->pool);



		if (switch_socket_addr_get(&listener->sa, SWITCH_TRUE, listener->sock) == SWITCH_STATUS_SUCCESS && listener->sa) {
			switch_get_addr(listener->remote_ip, sizeof(listener->remote_ip), listener->sa);
			if (listener->sa && (listener->remote_port = switch_sockaddr_get_port(listener->sa))) {
				launch_listener_thread(listener);
				continue;
			} 
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error initilizing connection\n");
		close_socket(&listener->sock);
		expire_listener(&listener);
	
	}

  end:

	close_socket(&listen_list.sock);

	if (prefs.nat_map && switch_nat_get_type()) {
		switch_nat_del_mapping(prefs.port, SWITCH_NAT_TCP);
	}

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}


	for (x = 0; x < prefs.acl_count; x++) {
		switch_safe_free(prefs.acl[x]);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
