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
 * mod_event_socket.c -- Socket Controlled Event Handler
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
	LFLAG_ASYNC = (1 << 7),
	LFLAG_STATEFUL = (1 << 8)
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
	switch_mutex_t *filter_mutex;
	uint32_t flags;
	switch_log_level_t level;
	char *ebuf;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	switch_thread_rwlock_t *rwlock;
	switch_core_session_t *session;
	int lost_events;
	int lost_logs;
	int hup;
	time_t last_flush;
	uint32_t timeout;
	uint32_t id;
	switch_sockaddr_t *sa;
	char remote_ip[50];
	switch_port_t remote_port;
	switch_event_t *filters;
	struct listener *next;
};

typedef struct listener listener_t;

static struct {
	switch_mutex_t *listener_mutex;	
	switch_event_node_t *node;
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
} prefs;


static void remove_listener(listener_t *listener);

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
			char *data = strdup(node->data);
			if (data) {
				if (switch_queue_trypush(l->log_queue, data) == SWITCH_STATUS_SUCCESS) {
					if (l->lost_logs) {
						int ll = l->lost_logs;
						switch_event_t *event;
						l->lost_logs = 0;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d log lines!\n", ll);
						if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "info", "lost %d log lines", ll);
							switch_event_fire(&event);
						}
					}
				} else {
					switch_safe_free(data);
					l->lost_logs++;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
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
			if (pop) free(pop);
		}
	}

	if (listener->event_queue) {
		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *pevent = (switch_event_t *) pop;
			if (!pop) continue;
			switch_event_destroy(&pevent);
		}
	}
}

static void expire_listener(listener_t **listener)
{
	
	flush_listener(*listener, SWITCH_TRUE, SWITCH_TRUE);
	switch_thread_rwlock_unlock((*listener)->rwlock);
	switch_core_hash_destroy(&(*listener)->event_hash);
	switch_core_destroy_memory_pool(&(*listener)->pool);
	switch_mutex_lock((*listener)->filter_mutex);
	if ((*listener)->filters) {
		switch_event_destroy(&(*listener)->filters);
	}
	switch_mutex_unlock((*listener)->filter_mutex);
	*listener = NULL;
}

static void event_handler(switch_event_t *event)
{
	switch_event_t *clone = NULL;
	listener_t *l, *lp;

	switch_assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}

	lp = listen_list.listeners;

	switch_mutex_lock(globals.listener_mutex);
	while(lp) {
		int send = 0;
		
		l = lp;
		lp = lp->next;
		
		if (!switch_test_flag(l, LFLAG_EVENTS)) {
			continue;
		}
		
		if (switch_test_flag(l, LFLAG_STATEFUL) && l->timeout && switch_timestamp(NULL) - l->last_flush > l->timeout) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Stateful Listener %u has expired\n", l->id);
			remove_listener(l);
			expire_listener(&l);
			continue;
		}
		
		
		if (l->event_list[SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || !event->subclass_name || (switch_core_hash_find(l->event_hash, event->subclass_name))) {
				send = 1;
			}
		}
		
		if (send && l->filters && l->filters->headers) {
			switch_event_header_t *hp;
			const char *hval;

			send = 0;
			switch_mutex_lock(l->filter_mutex);
			for (hp = l->filters->headers; hp;  hp = hp->next) {
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
			switch_mutex_unlock(l->filter_mutex);
		}
		
		if (send && switch_test_flag(l, LFLAG_MYEVENTS)) {
			char *uuid = switch_event_get_header(event, "unique-id");
			if (!uuid || strcmp(uuid, switch_core_session_get_uuid(l->session))) {
				send = 0;
			}
		}

		if (send) {
			if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
				if (switch_queue_trypush(l->event_queue, clone) == SWITCH_STATUS_SUCCESS) {
					if (l->lost_events) {
						int le = l->lost_events;
						l->lost_events = 0;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d events!\n", le);
						clone = NULL;
						if (switch_event_create(&clone, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
							switch_event_add_header(clone, SWITCH_STACK_BOTTOM, "info", "lost %d events", le);
							switch_event_fire(&clone);
						}
					}
				} else {
					l->lost_events++;
					switch_event_destroy(&clone);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
			}
		}

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

	if ((path = strchr((port_name ? port_name : host), '/'))) {
		*path++ = '\0';
		switch_channel_set_variable(channel, "socket_path", path);
	}

	switch_channel_set_variable(channel, "socket_host", host);

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
	switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

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
		return;
	} else {
		listener_run(NULL, (void *) listener);
	}

	if (switch_channel_get_state(channel) >= CS_HANGUP) {
		while (switch_test_flag(listener, LFLAG_SESSION)) {
			switch_yield(100000);
		}
	}

}


static void close_socket(switch_socket_t **sock)
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

	switch_log_unbind_logger(socket_logger);

	close_socket(&listen_list.sock);

	while (prefs.threads) {
		switch_yield(10000);
		if (++sanity == 1000) {
			break;
		}
	}
	switch_event_unbind(&globals.node);

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sock);
	}
	switch_mutex_unlock(globals.listener_mutex);



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


static listener_t *find_listener(uint32_t id)
{
	listener_t *l, *r = NULL;

	switch_mutex_lock(globals.listener_mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		if (l->id && l->id == id) {
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
	stream->write_function(stream, "  <format>%s</format>\n", listener->format == EVENT_FORMAT_XML ? "xml" : "plain");
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
		stream->write_function(stream, "This is a web application.!\n");
        return SWITCH_STATUS_SUCCESS;
	}
	stream->write_function(stream, "Content-Type: text/xml\n\n");
	
	stream->write_function(stream, "<?xml version=\"1.0\"?>\n");
	stream->write_function(stream, "<root>\n");
	
	if (!wcmd) {
		stream->write_function(stream, "<data><reply type=\"error\">Missing command parameter!</reply></data>\n");
		goto end;
	}
	
	if (!format) {
		format = "xml";
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

		if (switch_strlen_zero(action)) {
			stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
            goto end;
		}

		switch_mutex_lock(listener->filter_mutex);		
		if (!listener->filters) {
			switch_event_create(&listener->filters, SWITCH_EVENT_CHANNEL_DATA);
		}

		if (!strcasecmp(action, "delete")) {
			if (switch_strlen_zero(header_val)) {
				stream->write_function(stream, "<data><reply type=\"error\">Invalid Syntax</reply></data>\n");
				goto filter_end;
			}

			if (!strcasecmp(header_val, "all")) {
				switch_event_destroy(&listener->filters);
				switch_event_create(&listener->filters, SWITCH_EVENT_CHANNEL_DATA);
			} else {
				switch_event_del_header(listener->filters, header_val);
			}
			stream->write_function(stream, "<data>\n <reply type=\"success\">filter deleted.</reply>\n<api-command>\n");
		} else if (!strcasecmp(action, "add")) {
			if (switch_strlen_zero(header_name) || switch_strlen_zero(header_val)) {
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

	} else if (!strcasecmp(wcmd, "create-listener")) {
		char *events = switch_event_get_header(stream->param_event, "events");
		switch_memory_pool_t *pool;
		char *next, *cur;
		uint32_t count = 0, key_count = 0;
		uint8_t custom = 0;
		char *edup;
		
		if (switch_strlen_zero(events)) {
			stream->write_function(stream, "<data><reply type=\"error\">Missing parameter!</reply></data>\n");
			goto end;
		}

		switch_core_new_memory_pool(&pool);
		listener = switch_core_alloc(pool, sizeof(*listener));
		listener->pool = pool;
		listener->format = EVENT_FORMAT_PLAIN;
		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
		switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_core_hash_init(&listener->event_hash, listener->pool);
		switch_set_flag(listener, LFLAG_AUTHED);
		switch_set_flag(listener, LFLAG_STATEFUL);
		switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, listener->pool);
		switch_thread_rwlock_create(&listener->rwlock, listener->pool);
		listener->id = next_id();
		listener->timeout = 60;
		listener->last_flush = switch_timestamp(NULL);
		
		if (switch_stristr("xml", format)) {
			listener->format = EVENT_FORMAT_XML;
		} else {
			listener->format = EVENT_FORMAT_PLAIN;
		}

		edup = strdup(events);
		
		for (cur = edup; cur; count++) {
			switch_event_types_t type;

			if ((next = strchr(cur, ' '))) {
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
			stream->write_function(stream, "<data><reply type=\"error\">No keywords supplied</reply></data>\n");
			goto end;
		}


		switch_set_flag_locked(listener, LFLAG_EVENTS);
		add_listener(listener);
		stream->write_function(stream, "<data>\n");
		stream->write_function(stream, " <reply type=\"success\">Listener %u Created</reply>\n", listener->id);
		xmlize_listener(listener, stream);
		stream->write_function(stream, "</data>\n");

		goto end;
	} else if (!strcasecmp(wcmd, "destroy-listener")) {		
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;

		if (id) {
			idl = (uint32_t) atol(id);
		}

		if ((listener = find_listener(idl))) {
			remove_listener(listener);
			stream->write_function(stream, "<data>\n <reply type=\"success\">listener %u destroyed</reply>\n", listener->id);
			xmlize_listener(listener, stream);
			stream->write_function(stream, "</data>\n");
			expire_listener(&listener);
			goto end;
		} else {
			stream->write_function(stream, "<data><reply type=\"error\">Can't find listener</reply></data>\n");
			goto end;
		}

	} else if (!strcasecmp(wcmd, "check-listener")) {
		char *id = switch_event_get_header(stream->param_event, "listen-id");
		uint32_t idl = 0;
		void *pop;
		switch_event_t *pevent = NULL;
		
		if (id) {
			idl = (uint32_t) atol(id);
		}

		if (!(listener = find_listener(idl))) {
			stream->write_function(stream, "<data><reply type=\"error\">Can't find listener</reply></data>\n");
			goto end;
		}

		listener->last_flush = switch_timestamp(NULL);
		stream->write_function(stream, "<data>\n <reply type=\"success\">Current Events Follow</reply>\n");			
		xmlize_listener(listener, stream);
		stream->write_function(stream, "<events>\n");			

		while (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			char *etype;
			pevent = (switch_event_t *) pop;

			if (listener->format == EVENT_FORMAT_PLAIN) {
				etype = "plain";
				switch_event_serialize(pevent, &listener->ebuf, SWITCH_TRUE);
				stream->write_function(stream, "<event type=\"plain\">\n%s</event>", listener->ebuf);
			} else {
				switch_xml_t xml;
				etype = "xml";

				if ((xml = switch_event_xmlize(pevent, "%s", ""))) {
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

		stream->write_function(stream, " </events>\n</data>\n");

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
		switch_api_execute(api_command, api_args, NULL, stream);
		stream->param_event = oevent;
		stream->write_function(stream, " </api-command>\n</data>");
	} else {
		stream->write_function(stream, "<data><reply type=\"error\">INVALID COMMAND!</reply></data\n");
	}

 end:

	stream->write_function(stream, "</root>\n\n");
	
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_event_socket_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, pool);

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
	int clen = 0;
	
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

		if (switch_test_flag(listener, LFLAG_MYEVENTS) && !listener->hup && channel && !switch_channel_ready(channel)) {
			listener->hup = 1;
		}

		if (listener->hup == 2 || 
			((!switch_test_flag(listener, LFLAG_MYEVENTS) && !switch_test_flag(listener, LFLAG_EVENTS)) && 
			 channel && !switch_channel_ready(channel)) ) {
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
						switch_event_create(event, SWITCH_EVENT_COMMAND);
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
								switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, var, val);
								if (!strcasecmp(var, "content-length")) {
									clen = atoi(val);
									
									if (clen > 0) {
										char *body;
										char *p;
										
										switch_zmalloc(body, clen + 1);

										p = body;
										while(clen > 0) {
											mlen = clen;
											
											status = switch_socket_recv(listener->sock, p, &mlen);

											if (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS) {
												return SWITCH_STATUS_FALSE;
											}
				
											if (channel && !switch_channel_ready(channel)) {
												status = SWITCH_STATUS_FALSE;
												break;
											}

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

					if (!listener->ebuf) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No event data (allocation error?)\n");
						goto endloop;
					}

					len = strlen(listener->ebuf);

					switch_snprintf(hbuf, sizeof(hbuf), "Content-Length: %" SWITCH_SSIZE_T_FMT "\n" "Content-Type: text/event-%s\n" "\n", len, etype);

					len = strlen(hbuf);
					switch_socket_send(listener->sock, hbuf, &len);

					len = strlen(listener->ebuf);
					switch_socket_send(listener->sock, listener->ebuf, &len);

					switch_safe_free(listener->ebuf);

				  endloop:

					if (listener->hup == 1 && pevent->event_id == SWITCH_EVENT_CHANNEL_HANGUP) {
						char *uuid = switch_event_get_header(pevent, "unique-id");
						if (!strcmp(uuid, switch_core_session_get_uuid(listener->session))) {
							listener->hup = 2;
						}
					}
					
					switch_event_destroy(&pevent);
				}
			}
		}
		if (do_sleep) {
			switch_cond_next();
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

static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t *thread, void *obj)
{

	struct api_command_struct *acs = (struct api_command_struct *) obj;
	switch_stream_handle_t stream = { 0 };
	char *reply, *freply = NULL;
	switch_status_t status;

	if (!acs) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal error.\n");
		return NULL;
	}

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
		acs = NULL;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
	}
	return NULL;

}
static switch_status_t parse_command(listener_t *listener, switch_event_t **event, char *reply, uint32_t reply_len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cmd = switch_event_get_header(*event, "command");

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

	if (!strncasecmp(cmd, "filter ", 7)) {
		char *header_name = cmd + 7;
		char *header_val;

		strip_cr(header_name);

		while(header_name && *header_name && *header_name == ' ') header_name++;
		
		if (!(header_val = strchr(header_name, ' '))) {
			switch_snprintf(reply, reply_len, "-ERR invalid syntax");
			goto done;
		}

		*header_val++ = '\0';


		switch_mutex_lock(listener->filter_mutex);		
		if (!listener->filters) {
			switch_event_create(&listener->filters, SWITCH_EVENT_CHANNEL_DATA);
		}
		
		if (!strcasecmp(header_name, "delete")) {
			if (!strcasecmp(header_val, "all")) {
				switch_event_destroy(&listener->filters);
				switch_event_create(&listener->filters, SWITCH_EVENT_CHANNEL_DATA);
			} else {
				switch_event_del_header(listener->filters, header_val);
			}
			switch_snprintf(reply, reply_len, "+OK filter deleted. [%s]", header_val);
		} else {
			switch_event_add_header_string(listener->filters, SWITCH_STACK_BOTTOM, header_name, header_val);
			switch_snprintf(reply, reply_len, "+OK filter added. [%s]=[%s]", header_name, header_val);
		}
		switch_mutex_unlock(listener->filter_mutex);

		goto done;
	}


	if (listener->session || !strncasecmp(cmd, "myevents ", 9)) {
		switch_channel_t *channel = NULL;

		if (listener->session) {
			channel = switch_core_session_get_channel(listener->session);
		}

		if (!strncasecmp(cmd, "connect", 7)) {
			switch_snprintf(reply, reply_len, "+OK");
			goto done;
		} else if (listener->session && !strncasecmp(cmd, "sendmsg", 7)) {
			char *uuid = cmd + 8;
			switch_core_session_t *session = NULL;
			switch_bool_t uuid_supplied = SWITCH_FALSE;
			
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
				uuid = switch_event_get_header(*event, "session-id");
			}
			
			if (uuid) {
				uuid_supplied = SWITCH_TRUE;
			}
			
			if (uuid_supplied && (session = switch_core_session_locate(uuid))) {
				if ((status = switch_core_session_queue_private_event(session, event)) == SWITCH_STATUS_SUCCESS) {
					switch_snprintf(reply, reply_len, "+OK");
				} else {
					switch_snprintf(reply, reply_len, "-ERR memory error");
				}
				switch_core_session_rwunlock(session);
			} else {
				if (switch_test_flag(listener, LFLAG_ASYNC)) {
					if (!uuid_supplied) {
						switch_snprintf(reply, reply_len, "-ERR invalid session id [%s]", switch_str_nil(uuid));
					} else {
						if ((status = switch_core_session_queue_private_event(listener->session, event)) == SWITCH_STATUS_SUCCESS) {
							switch_snprintf(reply, reply_len, "+OK");
						} else {
							switch_snprintf(reply, reply_len, "-ERR memory error");
						}
					}
				} else {
					switch_ivr_parse_event(listener->session, *event);
					switch_snprintf(reply, reply_len, "+OK");
				}
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
			if (switch_test_flag(listener, LFLAG_MYEVENTS)) {
				switch_snprintf(reply, reply_len, "-ERR aready enabled.");
				goto done;
			}

			if (!listener->session) {
				char *uuid;
				
				if ((uuid = cmd + 9)) {
					strip_cr(uuid);
					
					if (!(listener->session = switch_core_session_locate(uuid))) {
						switch_snprintf(reply, reply_len, "-ERR invalid uuid");
						goto done;
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
				(*event)->event_id = etype;
			}
		}

		switch_event_fire(event);
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
			uuid = switch_event_get_header(*event, "session-id");
		}
		
		if (uuid && (session = switch_core_session_locate(uuid))) {
			if ((status = switch_core_session_queue_private_event(session, event)) == SWITCH_STATUS_SUCCESS) {
				switch_snprintf(reply, reply_len, "+OK");
			} else {
				switch_snprintf(reply, reply_len, "-ERR memory error");
			}
			switch_core_session_rwunlock(session);
		} else {
			switch_snprintf(reply, reply_len, "-ERR invalid session id [%s]", switch_str_nil(uuid));
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

		if ((uuid_str = switch_event_get_header(*event, "job-uuid"))) {
			switch_copy_string(acs->uuid_str, uuid_str, sizeof(acs->uuid_str));
		} else {
			switch_uuid_get(&uuid);
			switch_uuid_format(acs->uuid_str, &uuid);
		}
		switch_snprintf(reply, reply_len, "~Reply-Text: +OK Job-UUID: %s\nJob-UUID: %s\n\n", acs->uuid_str, acs->uuid_str);
		switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);

		status = SWITCH_STATUS_SUCCESS;
		goto done_noreply;
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
		flush_listener(listener, SWITCH_FALSE, SWITCH_TRUE);

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

	if (switch_strlen_zero(reply)) {
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

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);
	
	switch_assert(listener != NULL);
	
	if ((session = listener->session)) {
		channel = switch_core_session_get_channel(session);
		switch_core_session_read_lock(session);
	}

	if (prefs.acl_count && listener->sa && !switch_strlen_zero(listener->remote_ip)) {
		uint32_t x = 0;

		for (x = 0; x < prefs.acl_count; x++) {
			if (!switch_check_network_list_ip(listener->remote_ip, prefs.acl[x])) {
				const char message[] = "Access Denied, go away.\n";
				int mlen = strlen(message);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "IP %s Rejected by acl \"%s\"\n", listener->remote_ip, prefs.acl[x]);

				switch_snprintf(buf, sizeof(buf), "Content-Type: text/rude-rejection\nContent-Length %d\n\n", mlen);
				len = strlen(buf);
				switch_socket_send(listener->sock, buf, &len);
				len = mlen;
				switch_socket_send(listener->sock, message, &len);
				goto done;
			}
		}
	}
	
	if (switch_strlen_zero(listener->remote_ip)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open from %s:%d\n", listener->remote_ip, listener->remote_port);
	}

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

		if (switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}

		if (parse_command(listener, &ievent, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			goto done;
		}

		switch_caller_profile_event_set_data(switch_channel_get_caller_profile(channel), "Channel", call_event);
		switch_channel_event_set_data(channel, call_event);
		switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Content-Type", "command/reply");
		switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Reply-Text", "+OK\n");
		switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Socket-Mode", switch_test_flag(listener, LFLAG_ASYNC) ? "async" : "static");
		switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "Control", switch_test_flag(listener, LFLAG_FULL) ? "full" : "single-channel");

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

	while (switch_test_flag(listener, LFLAG_RUNNING) && listen_list.ready) {
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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");

	switch_thread_rwlock_wrlock(listener->rwlock);
	flush_listener(listener, SWITCH_TRUE, SWITCH_TRUE);
	switch_mutex_lock(listener->filter_mutex);
	if (listener->filters) {
		switch_event_destroy(&listener->filters);
	}
	switch_mutex_unlock(listener->filter_mutex);
	if (listener->sock) {
		char disco_buf[512] = "";
		const char message[] = "Disconnected, goodbye!\nSee you at ClueCon http://www.cluecon.com!\n";
		int mlen = strlen(message);
		
		switch_snprintf(disco_buf, sizeof(disco_buf), "Content-Type: text/disconnect-notice\nContent-Length %d\n\n", mlen);
		len = strlen(disco_buf);
		switch_socket_send(listener->sock, disco_buf, &len);
		len = mlen;
		switch_socket_send(listener->sock, message, &len);
		close_socket(&listener->sock);
	}

	switch_thread_rwlock_unlock(listener->rwlock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Closed\n");
	switch_core_hash_destroy(&listener->event_hash);

	if (listener->session) {
		switch_channel_clear_flag(switch_core_session_get_channel(listener->session), CF_CONTROLLED);
		switch_clear_flag_locked(listener, LFLAG_SESSION);
		switch_core_session_rwunlock(listener->session);
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
				} else if (!strcmp(var, "listen-port")) {
					prefs.port = (uint16_t) atoi(val);
				} else if (!strcmp(var, "password")) {
					set_pref_pass(val);
				} else if (!strcasecmp(var, "apply-inbound-acl")) {
					if (prefs.acl_count < MAX_ACL) {
						prefs.acl[prefs.acl_count++] = strdup(val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", MAX_ACL);
					}
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
	uint32_t x = 0;

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}


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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error! Could not listen on %s:%u\n", prefs.ip, prefs.port);
		switch_yield(100000);
	}

	listen_list.ready = 1;


	for (;;) {
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
		switch_mutex_init(&listener->filter_mutex, SWITCH_MUTEX_NESTED, listener->pool);

		switch_core_hash_init(&listener->event_hash, listener->pool);
		switch_socket_addr_get(&listener->sa, SWITCH_TRUE, listener->sock);
		switch_get_addr(listener->remote_ip, sizeof(listener->remote_ip), listener->sa);
		listener->remote_port = switch_sockaddr_get_port(listener->sa);
		launch_listener_thread(listener);

	}

	close_socket(&listen_list.sock);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
