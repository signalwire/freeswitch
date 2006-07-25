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


static const char modname[] = "mod_event_socket";
static char *MARKER = "1";

typedef enum {
	LFLAG_AUTHED = (1 << 0),
	LFLAG_RUNNING = (1 << 1),
	LFLAG_EVENTS = (1 << 2),
	LFLAG_LOG = (1 << 3)
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
	char *retbuf;
	uint8_t event_list[SWITCH_EVENT_ALL];
	switch_hash_t *event_hash;
	struct listener *next;
};

typedef struct listener listener_t;

static struct {
	switch_socket_t *sock;
	switch_mutex_t *mutex;
	listener_t *listeners;
	uint8_t ready;
} listen_list;

static struct {
	char *ip;
	uint16_t port;
	char *password;
} prefs;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip)
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_pass, prefs.password)

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

	assert(event != NULL);

	if (!listen_list.ready) {
		return;
	}
	
	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		uint8_t send = 0;

		if (!switch_test_flag(l, LFLAG_EVENTS)) {
			continue;
		}

		if (l->event_list[(uint8_t)SWITCH_EVENT_ALL]) {
			send = 1;
		} else if ((l->event_list[(uint8_t)event->event_id])) {
			if (event->event_id != SWITCH_EVENT_CUSTOM || (event->subclass && switch_core_hash_find(l->event_hash, event->subclass->name))) {
				send = 1;
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


static switch_loadable_module_interface_t event_socket_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL
};


static void close_socket(switch_socket_t **sock) {

	if (*sock) {
		apr_socket_shutdown(*sock, APR_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	listener_t *l;

	switch_mutex_lock(listen_list.mutex);
	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sock);
	}
	switch_mutex_unlock(listen_list.mutex);

	close_socket(&listen_list.sock);

	return SWITCH_STATUS_SUCCESS;
}



SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &event_socket_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

static void add_listener(listener_t *listener) 
{
	/* add me to the listeners so I get events */
	switch_mutex_lock(listen_list.mutex);
	listener->next = listen_list.listeners;
	listen_list.listeners = listener;
	switch_mutex_unlock(listen_list.mutex);
}

static void remove_listener(listener_t *listener)
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

static switch_status_t read_packet(listener_t *listener, switch_event_t **event, uint32_t timeout) 
{
	switch_size_t mlen;
	char mbuf[1024] = "";
	char buf[1024] = "";
	switch_size_t len;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int count = 0, bytes = 0;
	uint32_t elapsed = 0;
	time_t start = 0;
	void *pop;
	char *ptr;
	uint8_t crcount = 0;

	*event = NULL;
	start = time(NULL);
	ptr = mbuf;

	while(listener->sock) {
		uint8_t do_sleep = 1;
		mlen = 1;
		status = switch_socket_recv(listener->sock, ptr, &mlen);

		if (status != SWITCH_STATUS_BREAK && status != SWITCH_STATUS_SUCCESS) {
			return status;
		}

		if (status != SWITCH_STATUS_BREAK && mlen) {
			bytes++;

			if (*mbuf == '\r' || *mbuf == '\n') { /* bah */
				ptr = mbuf;
				continue;
			}

			if (*ptr == '\n') {
				crcount++;
			} else if (*ptr != '\r') {
				crcount = 0;
			}
			ptr++;
			if (crcount == 2) {
				char *next;
				char *cur = mbuf;

				while(cur) {
					if ((next = strchr(cur, '\r')) || (next = strchr(cur, '\n'))) {
						while (*next == '\r' || *next == '\n') {
							next++;
						}
					}
					count++;
					if (count == 1) {
						switch_event_create(event, SWITCH_EVENT_MESSAGE);
						switch_event_add_header(*event, SWITCH_STACK_BOTTOM, "Command", mbuf);
					} else {
						char *var, *val;
						var = mbuf;
						if ((val = strchr(var, ':'))) {
							*val++ = '\0';
							while(*val == ' ') {
								val++;
							}
						} 
						if (var && val) {
							switch_event_add_header(*event, SWITCH_STACK_BOTTOM, var, val);
						}
					}
					
					cur = next;
				}
				break;
			}
		}

		if (timeout) {
			elapsed = (uint32_t)(time(NULL) - start);
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
						snprintf(buf, sizeof(buf), "Content-Type: log/data\nContent-Length: %"APR_SSIZE_T_FMT"\n\n", strlen(data));
						len = strlen(buf) + 1;
						switch_socket_send(listener->sock, buf, &len);
						len = strlen(data) + 1;
						switch_socket_send(listener->sock, data, &len);
					
						free(data);
					}
					do_sleep = 0;
				}
			}

			if (switch_test_flag(listener, LFLAG_EVENTS)) {
				if (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
					char hbuf[512];
					switch_event_t *event = (switch_event_t *) pop;
					char *etype, *packet, *xmlstr = NULL;

					do_sleep = 0;
					if (listener->format == EVENT_FORMAT_PLAIN) {
						etype = "plain";
						switch_event_serialize(event, buf, sizeof(buf), NULL);
						packet = buf;
					} else {
						switch_xml_t xml;
						etype = "xml";

						if ((xml = switch_event_xmlize(event, NULL))) {
							xmlstr = switch_xml_toxml(xml);
							packet = xmlstr;
							switch_xml_free(xml);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML ERROR!\n");
							continue;
						}
					}
			
					len = strlen(packet) + 1;

					snprintf(hbuf, sizeof(hbuf), "Content-Length: %"APR_SSIZE_T_FMT"\n" 
							 "Content-Type: text/event-%s\n"
							 "\n", len, etype);

					len = strlen(hbuf) + 1;
					switch_socket_send(listener->sock, hbuf, &len);

					len = strlen(packet) + 1;
					switch_socket_send(listener->sock, packet, &len);

					if (xmlstr) {
						free(xmlstr);
					}
				}
			}
		}
		if (do_sleep) {
			switch_yield(1000);
		}
	}
	
	return status;

}

static switch_status_t parse_command(listener_t *listener, switch_event_t *event, char *reply, uint32_t reply_len)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cmd = switch_event_get_header(event, "command");

	*reply = '\0';

	if (!strncasecmp(cmd, "exit", 4)) {
		switch_clear_flag_locked(listener, LFLAG_RUNNING);
		snprintf(reply, reply_len, "+OK bye");
		goto done;
	}

	if (!switch_test_flag(listener, LFLAG_AUTHED)) {
		if (!strncasecmp(cmd, "auth ", 5)) {
			char *pass;
			strip_cr(cmd);

			pass = cmd + 5;

			if (!strcmp(prefs.password, pass)) {
				switch_set_flag_locked(listener, LFLAG_AUTHED);
				snprintf(reply, reply_len, "+OK accepted");
			} else {
				snprintf(reply, reply_len, "-ERR invalid");
				switch_clear_flag_locked(listener, LFLAG_RUNNING);
			}

			goto done;
		}

		goto done;
	}
	
	if (!strncasecmp(cmd, "api ", 4)) {
		char *api_cmd = cmd + 4;
		switch_stream_handle_t stream = {0};
		char *arg;

		if (!listener->retbuf) {
			listener->retbuf = switch_core_alloc(listener->pool, CMD_BUFLEN);
		}

		stream.data = listener->retbuf;
		stream.end = stream.data;
		stream.data_size = CMD_BUFLEN;
		stream.write_function = switch_console_stream_write;

		strip_cr(api_cmd);

		if ((arg = strchr(api_cmd, ' '))) {
			*arg++ = '\0';
		}
		
		if (switch_api_execute(api_cmd, arg, &stream) == SWITCH_STATUS_SUCCESS) {
			switch_size_t len;
			char buf[1024];

			len = strlen(listener->retbuf) + 1;			
			snprintf(buf, sizeof(buf), "Content-Type: api/response\nContent-Length: %"APR_SSIZE_T_FMT"\n\n", len);
			len = strlen(buf) + 1;
			switch_socket_send(listener->sock, buf, &len);
			len = strlen(listener->retbuf) + 1;
			switch_socket_send(listener->sock, listener->retbuf, &len);
			return SWITCH_STATUS_SUCCESS;
		} 
	} else if (!strncasecmp(cmd, "log", 3)) {

		char *level_s;
		strip_cr(cmd);
		
		level_s = cmd + 4;

		if (switch_strlen_zero(level_s)) {
			level_s = "debug";
		}

		if ((listener->level = switch_log_str2level(level_s))) {
			switch_set_flag(listener, LFLAG_LOG);
			snprintf(reply, reply_len, "+OK log level %s [%d]", level_s, listener->level);
		} else {
			snprintf(reply, reply_len, "-ERR invalid log level");
		}
	} else if (!strncasecmp(cmd, "nolog", 5)) {
		if (switch_test_flag(listener, LFLAG_LOG)) {
			switch_clear_flag_locked(listener, LFLAG_LOG);
			snprintf(reply, reply_len, "+OK no longer logging");
		} else {
			snprintf(reply, reply_len, "-ERR not loging");
		}
	} else if (!strncasecmp(cmd, "event", 5)) {
		char *next, *cur;
		uint32_t count = 0, key_count = 0;
		uint8_t custom = 0;
		
		strip_cr(cmd);
		cur = cmd + 5;

		if (cur && (cur = strchr(cur, ' '))) {
			for(cur++; cur; count++) {
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
					switch_core_hash_insert_dup(listener->event_hash, cur, MARKER);
				} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
					key_count++;
					listener->event_list[(uint8_t)type] = 1;
					if (type == SWITCH_EVENT_CUSTOM) {
						custom++;
					}
				}

			end:
				cur = next;
			}
		} 

		if (!key_count) {
			snprintf(reply, reply_len, "-ERR no keywords supplied");
			goto done;
		}

		if (!switch_test_flag(listener, LFLAG_EVENTS)) {
			switch_set_flag_locked(listener, LFLAG_EVENTS);
		}

		snprintf(reply, reply_len, "+OK event listener enabled %s", listener->format == EVENT_FORMAT_XML ? "xml" : "plain");
		
	} else if (!strncasecmp(cmd, "noevents", 8)) {
		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			uint8_t x = 0;
			switch_clear_flag_locked(listener, LFLAG_EVENTS);
			for (x = 0; x <= SWITCH_EVENT_ALL; x++) {
				listener->event_list[x] = 0;
			}
			/* wipe the hash */
			switch_core_hash_init(&listener->event_hash, listener->pool);
			snprintf(reply, reply_len, "+OK no longer listening for events");
		} else {
			snprintf(reply, reply_len, "-ERR not listening for events");
		}
	} 
	
 done:
	if (event) {
		switch_event_destroy(&event);
	}

	if (switch_strlen_zero(reply)) {
		snprintf(reply, reply_len, "-ERR command not found");
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

	assert(listener != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");

	switch_socket_opt_set(listener->sock, APR_SO_NONBLOCK, TRUE);
	switch_set_flag_locked(listener, LFLAG_RUNNING);
	add_listener(listener);

	snprintf(buf, sizeof(buf), "Content-Type: auth/request\n\n");
		
	len = strlen(buf) + 1;
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
		if (!switch_strlen_zero(reply)) {
			snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
			len = strlen(buf) + 1;
			switch_socket_send(listener->sock, buf, &len);
		}
		break;
	}
		

	while(switch_test_flag(listener, LFLAG_RUNNING) && listen_list.ready) {
		switch_event_t *event;

		len = sizeof(buf);
		memset(buf, 0, len);
		status = read_packet(listener, &event, 0);
		
		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (!event) {
			continue;
		}

		if (parse_command(listener, event, reply, sizeof(reply)) != SWITCH_STATUS_SUCCESS) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			break;
		}


		if (!switch_strlen_zero(reply)) {
			snprintf(buf, sizeof(buf), "Content-Type: command/reply\nReply-Text: %s\n\n", reply);
			len = strlen(buf) + 1;
			switch_socket_send(listener->sock, buf, &len);
		}
			
	}

 done:

	remove_listener(listener);
	close_socket(&listener->sock);

	if (switch_test_flag(listener, LFLAG_EVENTS)) {
		remove_listener(listener);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Closed\n");

	if (listener->pool) {
		switch_memory_pool_t *pool = listener->pool;
		switch_core_destroy_memory_pool(&pool);
	}

	return NULL;
}


/* Create a thread for the conference and launch it */
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcmp(var, "listen-ip")) {
					set_pref_ip(val);
				} else if (!strcmp(var, "listen-port")) {
					prefs.port = (uint16_t)atoi(val);
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


SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	switch_memory_pool_t *pool = NULL, *listener_pool = NULL;
    switch_status_t rv;
    switch_sockaddr_t *sa;
    switch_socket_t  *inbound_socket = NULL;
	listener_t *listener;

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&listen_list.mutex, SWITCH_MUTEX_NESTED, pool);

	
	for(;;) {
		rv = switch_sockaddr_info_get(&sa, prefs.ip, APR_INET, prefs.port, 0, pool);
		if (rv) goto fail;
		rv = switch_socket_create(&listen_list.sock, sa->family, SOCK_STREAM, APR_PROTO_TCP, pool);
		if (rv) goto sock_fail;
		rv = switch_socket_bind(listen_list.sock, sa);
		if (rv) goto sock_fail;
		rv = switch_socket_listen(listen_list.sock, 5);
		if (rv) goto sock_fail;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", prefs.ip, prefs.port);
		break;
	sock_fail:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
		switch_yield(1000000);
	}

	listen_list.ready = 1;

	if (switch_event_bind((char *) modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG);


	for (;;) {
		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}
		
		if ((rv = switch_socket_accept(&inbound_socket, listen_list.sock, listener_pool))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error\n");
			break;
		}
		
		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}
		
		switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);
		switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);

		listener->sock = inbound_socket;
		listener->pool = listener_pool;
		listener->format = EVENT_FORMAT_PLAIN;
		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener_pool);
		switch_core_hash_init(&listener->event_hash, listener_pool);

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



