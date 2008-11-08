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
 * Andrew Thompson <andrew@hijacked.us>
 *
 *
 * mod_erlang_event.c -- Erlang Event Handler derived from mod_event_socket
 *
 */
#include <switch.h>

#include <ei.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_erlang_event_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_erlang_event_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_erlang_event_runtime);
SWITCH_MODULE_DEFINITION(mod_erlang_event, mod_erlang_event_load, mod_erlang_event_shutdown, mod_erlang_event_runtime);

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


/* TODO - support multiple event handlers per erlang connection each with their own event filters? */
struct event_handler {
	erlang_pid pid;
	switch_hash_t *event_hash;
	struct event_handler *next;
};

struct listener {
	int sockfd;
	const struct ei_cnode_s *ec;
	erlang_pid log_pid;
	erlang_pid event_pid;
	switch_queue_t *event_queue;
	switch_queue_t *log_queue;
	switch_memory_pool_t *pool;
	switch_mutex_t *flag_mutex;
	char *ebuf;
	uint32_t flags;
	switch_log_level_t level;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	switch_thread_rwlock_t *rwlock;
	switch_core_session_t *session;
	int lost_events;
	int lost_logs;
	time_t last_flush;
	uint32_t timeout;
	uint32_t id;
	char remote_ip[50];
	/*switch_port_t remote_port;*/
	struct listener *next;
};

typedef struct listener listener_t;

static struct {
	int sockfd;
	switch_mutex_t *sock_mutex;
	listener_t *listeners;
	uint8_t ready;
} listen_list;

#define MAX_ACL 100

static struct {
	switch_mutex_t *mutex;
	char *ip;
	char *nodename;
	switch_bool_t shortname;
	uint16_t port;
	char *cookie;
	int done;
	int threads;
	char *acl[MAX_ACL];
	uint32_t acl_count;
	uint32_t id;
} prefs;


static void remove_listener(listener_t *listener);

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_ip, prefs.ip);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_cookie, prefs.cookie);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_pref_nodename, prefs.nodename);

static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj);
static void launch_listener_thread(listener_t *listener);

static struct {
	switch_mutex_t *listener_mutex;
	switch_event_node_t *node;
} globals;


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


static void expire_listener(listener_t **listener)
{
	void *pop;
	
	switch_thread_rwlock_unlock((*listener)->rwlock);
	switch_core_hash_destroy(&(*listener)->event_hash);
	switch_core_destroy_memory_pool(&(*listener)->pool);
	
	while (switch_queue_trypop((*listener)->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *pevent = (switch_event_t *) pop;
		switch_event_destroy(&pevent);
	}

	*listener = NULL;
}


static void ei_encode_switch_event(ei_x_buff *ebuf, switch_event_t *event)
{
	int i;
	switch_event_header_t *hp;
	for (i = 0, hp = event->headers; hp; hp = hp->next, i++) {
	}

	if (event->body)
		i++;

	ei_x_encode_tuple_header(ebuf, 2);
	ei_x_encode_atom(ebuf, "event");
	ei_x_encode_list_header(ebuf, i);

	for (hp = event->headers; hp; hp = hp->next) {
		ei_x_encode_tuple_header(ebuf, 2);
		ei_x_encode_string(ebuf, hp->name);
		ei_x_encode_string(ebuf, hp->value);
	}

	if (event->body) {
		ei_x_encode_tuple_header(ebuf, 2);
		ei_x_encode_string(ebuf, "body");
		ei_x_encode_string(ebuf, event->body);
	}

	ei_x_encode_empty_list(ebuf);
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
		uint8_t send = 0;
		
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


static void close_socket(int *sock)
{
	switch_mutex_lock(listen_list.sock_mutex);
	if (*sock) {
		shutdown(*sock, SHUT_RDWR);
		close(*sock);
		sock = NULL;
	}
	switch_mutex_unlock(listen_list.sock_mutex);
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_erlang_event_shutdown)
{
	listener_t *l;
	int sanity = 0;

	prefs.done = 1;

	switch_log_unbind_logger(socket_logger);

	/*close_socket(&listen_list.sockfd);*/
	
	while (prefs.threads || prefs.done == 1) {
		switch_yield(10000);
		if (++sanity == 1000) {
			break;
		}
	}

	switch_event_unbind(&globals.node);

	switch_mutex_lock(globals.listener_mutex);

	for (l = listen_list.listeners; l; l = l->next) {
		close_socket(&l->sockfd);
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

SWITCH_MODULE_LOAD_FUNCTION(mod_erlang_event_load)
{
	switch_mutex_init(&globals.listener_mutex, SWITCH_MUTEX_NESTED, pool);

	if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	switch_log_bind_logger(socket_logger, SWITCH_LOG_DEBUG, SWITCH_FALSE);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


struct api_command_struct {
	char *api_cmd;
	char *arg;
	listener_t *listener;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint8_t bg;
	erlang_pid pid;
	switch_memory_pool_t *pool;
};


static void *SWITCH_THREAD_FUNC api_exec(switch_thread_t *thread, void *obj)
{
	switch_bool_t r = SWITCH_TRUE;
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
		r = SWITCH_FALSE;
	}

	if (!reply) {
		reply = "Command returned no output!";
		r = SWITCH_FALSE;
	}

	if (*reply == '-')
		r = SWITCH_FALSE;

	if (acs->bg) {
		switch_event_t *event;

		if (switch_event_create(&event, SWITCH_EVENT_BACKGROUND_JOB) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-UUID", acs->uuid_str);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command", acs->api_cmd);

			ei_x_buff ebuf;
			ei_x_new_with_version(&ebuf);

			if (acs->arg) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Command-Arg", acs->arg);
			}

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Job-Successful", r ? "true" : "false");
			switch_event_add_body(event, "%s", reply);

			switch_event_fire(&event);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending bgapi reply to %s\n", acs->pid.node);

			ei_x_encode_tuple_header(&ebuf, 3);

			if (r)
				ei_x_encode_atom(&ebuf, "bgok");
			else
				ei_x_encode_atom(&ebuf, "bgerror");

			ei_x_encode_string(&ebuf, acs->uuid_str);
			ei_x_encode_string(&ebuf, reply);

			ei_send(acs->listener->sockfd, &acs->pid, ebuf.buff, ebuf.index);

			ei_x_free(&ebuf);
		}
	} else {
		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);
		ei_x_encode_tuple_header(&rbuf, 2);

		if (!strlen(reply)) {
			reply = "Command returned no output!";
			r = SWITCH_FALSE;
		}

		if (r) {
			ei_x_encode_atom(&rbuf, "ok");
		} else {
			ei_x_encode_atom(&rbuf, "error");
		}
		
		ei_x_encode_string(&rbuf, reply);

		ei_send(acs->listener->sockfd, &acs->pid, rbuf.buff, rbuf.index);
		ei_x_free(&rbuf);
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


static int handle_msg(listener_t *listener, erlang_msg *msg, ei_x_buff *buf, ei_x_buff *rbuf)
{
	int type, size, version, arity;
	char tupletag[MAXATOMLEN];
	char atom[MAXATOMLEN];

	buf->index = 0;
	ei_decode_version(buf->buff, &buf->index, &version);
	ei_get_type(buf->buff, &buf->index, &type, &size);

	switch(type) {
	case ERL_SMALL_TUPLE_EXT :
	case ERL_LARGE_TUPLE_EXT :
		ei_decode_tuple_header(buf->buff, &buf->index, &arity);
		if (ei_decode_atom(buf->buff, &buf->index, tupletag)) {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "badarg");
			break;
		}

		if (!strncmp(tupletag, "set_log_level", MAXATOMLEN)) {
			if (arity == 2) {
				switch_log_level_t ltype = SWITCH_LOG_DEBUG;
				char loglevelstr[MAXATOMLEN];
				if (ei_decode_atom(buf->buff, &buf->index, loglevelstr)) {
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "badarg");
					break;
				}
				ltype = switch_log_str2level(loglevelstr);

				if (ltype && ltype != SWITCH_LOG_INVALID) {
					listener->level = ltype;
				} else {
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "badarg");
					break;
				}
			} else {
				/* tuple too long */
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}
		} else if (!strncmp(tupletag, "event", MAXATOMLEN)) {
			if (arity == 1) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			int custom = 0;
			switch_event_types_t type;

			if (!switch_test_flag(listener, LFLAG_EVENTS)) {
				switch_set_flag_locked(listener, LFLAG_EVENTS);
			}

			for (int i = 1; i < arity; i++) {
				if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

					if (custom) {
						switch_core_hash_insert(listener->event_hash, atom, MARKER);
					} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
						if (type == SWITCH_EVENT_ALL) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "ALL events enabled\n");
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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "enable event %s\n", atom);
				}
			}
		} else if (!strncmp(tupletag, "nixevent", MAXATOMLEN)) {
			if (arity == 1) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			int custom = 0;
			switch_event_types_t type;

			for (int i = 1; i < arity; i++) {
				if (!ei_decode_atom(buf->buff, &buf->index, atom)) {

					if (custom) {
						switch_core_hash_delete(listener->event_hash, atom);
					} else if (switch_name_event(atom, &type) == SWITCH_STATUS_SUCCESS) {
						uint32_t x = 0;

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
				}
			}
		} else if (!strncmp(tupletag, "api", MAXATOMLEN)) {
			if (arity < 3) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			char api_cmd[MAXATOMLEN];
			char arg[1024];
	
			if (ei_decode_atom(buf->buff, &buf->index, api_cmd)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			if (ei_decode_string(buf->buff, &buf->index, arg)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}
			struct api_command_struct acs = { 0 };
			acs.listener = listener;
			acs.api_cmd = api_cmd;
			acs.arg = arg;
			acs.bg = 0;
			acs.pid = msg->from;
			api_exec(NULL, (void *) &acs);
			goto noreply;

		} else if (!strncmp(tupletag, "bgapi", MAXATOMLEN)) {
			if (arity < 3) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			char api_cmd[MAXATOMLEN];
			char arg[1024];

			if (ei_decode_atom(buf->buff, &buf->index, api_cmd)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			if (ei_decode_string(buf->buff, &buf->index, arg)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			struct api_command_struct *acs = NULL;
			switch_memory_pool_t *pool;
			switch_thread_t *thread;
			switch_threadattr_t *thd_attr = NULL;
			switch_uuid_t uuid;

			switch_core_new_memory_pool(&pool);
			acs = switch_core_alloc(pool, sizeof(*acs));
			switch_assert(acs);
			acs->pool = pool;
			acs->listener = listener;
			acs->api_cmd = switch_core_strdup(acs->pool, api_cmd);
			acs->arg = switch_core_strdup(acs->pool, arg);
			acs->bg = 1;
			acs->pid = msg->from;

			switch_threadattr_create(&thd_attr, acs->pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

			switch_uuid_get(&uuid);
			switch_uuid_format(acs->uuid_str, &uuid);
			switch_thread_create(&thread, thd_attr, api_exec, acs, acs->pool);

			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "ok");
			ei_x_encode_string(rbuf, acs->uuid_str);

			break;
		} else if (!strncmp(tupletag, "sendevent", MAXATOMLEN)) {
			char ename[MAXATOMLEN];

			if (ei_decode_atom(buf->buff, &buf->index, ename)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			int headerlength;

			if (ei_decode_list_header(buf->buff, &buf->index, &headerlength)) {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "badarg");
				break;
			}

			switch_event_types_t etype;
			if (switch_name_event(ename, &etype) == SWITCH_STATUS_SUCCESS) {
				switch_event_t *event;

				if (switch_event_create(&event, etype) == SWITCH_STATUS_SUCCESS) {

					char key[1024];
					char value[1024];
					int i = 0;
					while(!ei_decode_tuple_header(buf->buff, &buf->index, &arity) && arity == 2) {
						i++;
						if (ei_decode_string(buf->buff, &buf->index, key))
							goto sendmsg_fail;
						if (ei_decode_string(buf->buff, &buf->index, value))
							goto sendmsg_fail;

						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, key, value);
					}

					if (headerlength != i)
						goto sendmsg_fail;
					

					switch_event_fire(&event);
					ei_x_encode_atom(rbuf, "ok");
					break;

sendmsg_fail:
					ei_x_encode_tuple_header(rbuf, 2);
					ei_x_encode_atom(rbuf, "error");
					ei_x_encode_atom(rbuf, "badarg");
					break;
				}
			}

		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "undef");
			break;
		}

		ei_x_encode_atom(rbuf, "ok");
		break;
	case ERL_ATOM_EXT :
		if(ei_decode_atom(buf->buff, &buf->index, atom)) {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "badarg");
			break;
		}

		if (!strncmp(atom, "nolog", MAXATOMLEN)) {
			if (switch_test_flag(listener, LFLAG_LOG)) {
				switch_clear_flag_locked(listener, LFLAG_LOG);
			}
		} else if (!strncmp(atom, "register_log_handler", MAXATOMLEN)) {
			listener->log_pid = msg->from;
			listener->level = SWITCH_LOG_DEBUG;
			switch_set_flag(listener, LFLAG_LOG);
		} else if (!strncmp(atom, "register_event_handler", MAXATOMLEN)) {
			listener->event_pid = msg->from;
			if (!switch_test_flag(listener, LFLAG_EVENTS)) {
				switch_set_flag_locked(listener, LFLAG_EVENTS);
			}
		} else if (!strncmp(atom, "noevents", MAXATOMLEN)) {
			void *pop;
			/*purge the event queue */
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
			} else {
				ei_x_encode_tuple_header(rbuf, 2);
				ei_x_encode_atom(rbuf, "error");
				ei_x_encode_atom(rbuf, "notlistening");
				break;
			}
		} else if (!strncmp(atom, "exit", MAXATOMLEN)) {
			switch_clear_flag_locked(listener, LFLAG_RUNNING);
			ei_x_encode_atom(rbuf, "ok");
			goto event_done;
		} else {
			ei_x_encode_tuple_header(rbuf, 2);
			ei_x_encode_atom(rbuf, "error");
			ei_x_encode_atom(rbuf, "undef");
			break;
		}

		ei_x_encode_atom(rbuf, "ok");
		break;
	default :
		/* some other kind of erlang term */
		ei_x_encode_tuple_header(rbuf, 2);
		ei_x_encode_atom(rbuf, "error");
		ei_x_encode_atom(rbuf, "undef");
		break;
	}

	ei_send(listener->sockfd, &msg->from, rbuf->buff, rbuf->index);
noreply:
	return 0;

event_done:
	ei_send(listener->sockfd, &msg->from, rbuf->buff, rbuf->index);
	return 1;
}


static void *SWITCH_THREAD_FUNC listener_run(switch_thread_t *thread, void *obj)
{
	listener_t *listener = (listener_t *) obj;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	int status = 1;
	void *pop;

	switch_mutex_lock(globals.listener_mutex);
	prefs.threads++;
	switch_mutex_unlock(globals.listener_mutex);

	switch_assert(listener != NULL);
	
	if (prefs.acl_count && !switch_strlen_zero(listener->remote_ip)) {
		uint32_t x = 0;
		for (x = 0; x < prefs.acl_count; x++) {
			if (!switch_check_network_list_ip(listener->remote_ip, prefs.acl[x])) {
				erlang_msg msg;

				ei_x_buff buf;
				ei_x_new(&buf);

				status = ei_xreceive_msg(listener->sockfd, &msg, &buf);
				/* get data off the socket, just so we can get the pid on the other end */
				if (status == ERL_MSG) {
					/* if we got a message, return an ACL error. */
					ei_x_buff rbuf;
					ei_x_new_with_version(&rbuf);

					ei_x_encode_tuple_header(&rbuf, 2);
					ei_x_encode_atom(&rbuf, "error");
					ei_x_encode_atom(&rbuf, "acldeny");

					ei_send(listener->sockfd, &msg.from, rbuf.buff, rbuf.index);

					ei_x_free(&rbuf);
				}

				ei_x_free(&buf);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection from %s denied by acl %s\n", listener->remote_ip, prefs.acl[x]);
				goto done;
			}
		}
	}

	if ((session = listener->session)) {
		channel = switch_core_session_get_channel(session);
		switch_core_session_read_lock(session);
	}

	if (switch_strlen_zero(listener->remote_ip)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connection Open from %s\n", listener->remote_ip);/*, listener->remote_port);*/
	}

	switch_set_flag_locked(listener, LFLAG_RUNNING);
	add_listener(listener);

	while ((status >= 0 || erl_errno == ETIMEDOUT || erl_errno == EAGAIN) && !prefs.done) {
		erlang_msg msg;

		ei_x_buff buf;
		ei_x_new(&buf);

		ei_x_buff rbuf;
		ei_x_new_with_version(&rbuf);

		status = ei_xreceive_msg_tmo(listener->sockfd, &msg, &buf, 100);

		switch(status) {
			case ERL_TICK :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Tick\n");
				break;
			case ERL_MSG :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Message\n");
				switch(msg.msgtype) {
					case ERL_SEND :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_send\n");
						if (handle_msg(listener, &msg, &buf, &rbuf)) {
							goto done;
						}
						break;
					case ERL_REG_SEND :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_reg_send\n");
						if (handle_msg(listener, &msg, &buf, &rbuf)) {
							goto done;
						}
						break;
					case ERL_LINK :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_link\n");
						break;
					case ERL_UNLINK :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_unlink\n");
						break;
					case ERL_EXIT :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_exit\n");
						break;
					default :
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected msg type %d\n", (int)(msg.msgtype));
						break;
				}
				break;
			case ERL_ERROR :
				if (erl_errno != ETIMEDOUT && erl_errno != EAGAIN) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "erl_error\n");
				}
				break;
			default :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unexpected status %d \n", status);
				break;
		}

		ei_x_free(&buf);
		ei_x_free(&rbuf);

		/* send out any pending crap in the log queue */
		if (switch_test_flag(listener, LFLAG_LOG)) {
			if (switch_queue_trypop(listener->log_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				char *data = (char *) pop;

				if (data) {
					ei_x_buff lbuf;
					ei_x_new_with_version(&lbuf);
					ei_x_encode_tuple_header(&lbuf, 2);
					ei_x_encode_atom(&lbuf, "log");
					ei_x_encode_string(&lbuf, data);
					ei_send(listener->sockfd, &listener->log_pid, lbuf.buff, lbuf.index);
					ei_x_free(&lbuf);
				}
			}
		}

		/* ditto with the event queue */
		if (switch_test_flag(listener, LFLAG_EVENTS)) {
			if (switch_queue_trypop(listener->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {

				switch_event_t *pevent = (switch_event_t *) pop;

				ei_x_buff ebuf;
				ei_x_new_with_version(&ebuf);

				ei_encode_switch_event(&ebuf, pevent);

				ei_send(listener->sockfd, &listener->event_pid, ebuf.buff, ebuf.index);

				ei_x_free(&ebuf);
				switch_event_destroy(&pevent);
			}
		}
	}

done:
	remove_listener(listener);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Session complete, waiting for children\n");

	switch_thread_rwlock_wrlock(listener->rwlock);
	
	if (listener->sockfd) {
		close_socket(&listener->sockfd);
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
	char *cf = "erlang_event.conf";
	switch_xml_t cfg, xml, settings, param;

	memset(&prefs, 0, sizeof(prefs));

	prefs.shortname = SWITCH_TRUE;

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
				} else if (!strcmp(var, "cookie")) {
					set_pref_cookie(val);
				} else if (!strcmp(var, "nodename")) {
					set_pref_nodename(val);
				} else if (!strcmp(var, "shortname")) {
						prefs.shortname = switch_true(val);
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

	if (switch_strlen_zero(prefs.cookie)) {
		set_pref_cookie("ClueCon");
	}

	if (!prefs.port) {
		prefs.port = 8031;
	}

	if (!prefs.nodename) {
		prefs.nodename = "freeswitch";
	}

	return 0;
}


SWITCH_MODULE_RUNTIME_FUNCTION(mod_erlang_event_runtime)
{
	switch_memory_pool_t *pool = NULL, *listener_pool = NULL;
	switch_status_t rv;
	listener_t *listener;
	uint32_t x = 0;
	struct ei_cnode_s ec;
	ErlConnect conn;
	struct sockaddr_in server_addr;
	int on = 1;
	int clientfd;
	int epmdfd;

	memset(&listen_list, 0, sizeof(listen_list));
	config();

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&listen_list.sock_mutex, SWITCH_MUTEX_NESTED, pool);

	/* zero out the struct before we use it */
	memset(&server_addr, 0, sizeof(server_addr));

	/* convert the configured IP to network byte order, handing errors */
	rv = inet_pton(AF_INET, prefs.ip, &server_addr.sin_addr.s_addr);
	if (rv == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not parse invalid ip address: %s\n", prefs.ip);
		return SWITCH_STATUS_GENERR;
	} else if (rv == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error when parsing ip address %s : %s\n", prefs.ip, strerror(errno));
		return SWITCH_STATUS_GENERR;
	}

	/* set the address family and port */
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(prefs.port);

	/* do the socket setup ei is too lazy to do for us */
	for (;;) {

		if ((listen_list.sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate socket on %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}

		if (setsockopt(listen_list.sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to enable SO_REUSEADDR for socket on %s:%u : %s\n", prefs.ip, prefs.port, strerror(errno));
			goto sock_fail;
		}
		
		if (bind(listen_list.sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to bind to %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}
		
		if (listen(listen_list.sockfd, 5) < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to listen on %s:%u\n", prefs.ip, prefs.port);
			goto sock_fail;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Socket up listening on %s:%u\n", prefs.ip, prefs.port);
		break;
	  sock_fail:
		switch_yield(100000);
	}

	struct hostent *nodehost = gethostbyaddr(&server_addr.sin_addr.s_addr, sizeof(server_addr.sin_addr.s_addr), AF_INET);

	char *thishostname = nodehost->h_name;
	char thisnodename[MAXNODELEN+1];

	if (!strcmp(thishostname, "localhost"))
		gethostname(thishostname, EI_MAXHOSTNAMELEN);

	if (prefs.shortname) {
		char *off;
		if ((off = strchr(thishostname, '.'))) {
			*off = '\0';
		}
	}

	snprintf(thisnodename, MAXNODELEN+1, "%s@%s", prefs.nodename, thishostname);

	/* init the ei stuff */
	if (ei_connect_xinit(&ec, thishostname, prefs.nodename, thisnodename, (Erl_IpAddr)(&server_addr.sin_addr.s_addr), prefs.cookie, 0) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to init ei connection\n");
		close_socket(&listen_list.sockfd);
		return SWITCH_STATUS_GENERR;
	}

	/* return value is -1 for error, a descriptor pointing to epmd otherwise */
	if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to publish port to empd, trying to start empd manually\n");
		if (system("epmd -daemon")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to start empd manually\n");
			close_socket(&listen_list.sockfd);
			return SWITCH_STATUS_GENERR;
		}
		if ((epmdfd = ei_publish(&ec, prefs.port)) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to publish port to empd AGAIN\n");
			close_socket(&listen_list.sockfd);
			return SWITCH_STATUS_GENERR;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Connected and published erlang cnode at %s port %u\n", thisnodename, prefs.port);

	listen_list.ready = 1;

	for (;;) {
		/* zero out errno because ei_accept doesn't differentiate between a
		 * failed authentication or a socket failure, or a client version
		 * mismatch or a godzilla attack */
		errno = 0;
		if ((clientfd = ei_accept_tmo(&ec, listen_list.sockfd, &conn, 100)) == ERL_ERROR) {
			if (prefs.done) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Shutting Down\n");
			} else if (erl_errno == ETIMEDOUT) {
				continue;
			} else if (errno) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error %d %d\n", erl_errno, errno);
			} else {
				/* if errno didn't get set, assume nothing *too* horrible occured */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
						"Ignorable error in ei_accept - probable bad client version, bad cookie or bad nodename\n");
				continue;
			}
			break;
		}

		if (switch_core_new_memory_pool(&listener_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			goto fail;
		}

		if (!(listener = switch_core_alloc(listener_pool, sizeof(*listener)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error\n");
			break;
		}

		switch_thread_rwlock_create(&listener->rwlock, listener_pool);
		switch_queue_create(&listener->event_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);
		switch_queue_create(&listener->log_queue, SWITCH_CORE_QUEUE_LEN, listener_pool);

		inet_ntop(AF_INET, conn.ipadr, listener->remote_ip, sizeof(listener->remote_ip));

		listener->ec = &ec;
		listener->sockfd = clientfd;
		listener->pool = listener_pool;
		listener_pool = NULL;
		listener->level = SWITCH_LOG_DEBUG;
		switch_mutex_init(&listener->flag_mutex, SWITCH_MUTEX_NESTED, listener->pool);
		switch_core_hash_init(&listener->event_hash, listener->pool);

		launch_listener_thread(listener);

	}

	/* cleanup epmd registration */
	ei_unpublish(&ec);
	close(epmdfd);

	close_socket(&listen_list.sockfd);

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	if (listener_pool) {
		switch_core_destroy_memory_pool(&listener_pool);
	}


	for (x = 0; x < prefs.acl_count; x++) {
		switch_safe_free(prefs.acl[x]);
	}

	prefs.done = 2;
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
