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
 *
 *
 * mod_erlang_event.h -- Erlang Event Handler derived from mod_event_socket
 *
 */

typedef enum {
	LFLAG_WAITING_FOR_PID = (1 << 0),	/* waiting for a node to return a pid */
	LFLAG_OUTBOUND_INIT = (1 << 1),	/* Erlang peer has been notified of this session */
	LFLAG_SESSION_COMPLETE = (1 << 2),
} session_flag_t;

typedef enum {
	NONE = 0,
	ERLANG_PID,
	ERLANG_REG_PROCESS
} process_type;

typedef enum {
	ERLANG_STRING = 0,
	ERLANG_BINARY
} erlang_encoding_t;

struct erlang_process {
	process_type type;
	char *reg_name;
	erlang_pid pid;
};

enum reply_state { reply_not_ready, reply_waiting, reply_found, reply_timeout };

struct fetch_reply_struct
{
	switch_thread_cond_t *ready_or_found;
	switch_mutex_t *mutex;
	enum reply_state state;
	ei_x_buff *reply;
	char winner[MAXNODELEN + 1];
};
typedef struct fetch_reply_struct fetch_reply_t;

struct spawn_reply_struct
{
	switch_thread_cond_t *ready_or_found;
	switch_mutex_t *mutex;
	erlang_pid *pid;
	char *hash;
};
typedef struct spawn_reply_struct spawn_reply_t;

struct session_elem {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_mutex_t *flag_mutex;
	uint32_t flags;
	struct erlang_process process;
	switch_queue_t *event_queue;
	switch_thread_rwlock_t *rwlock;
	switch_thread_rwlock_t *event_rwlock;
	switch_channel_state_t channel_state;
	switch_memory_pool_t *pool;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	spawn_reply_t *spawn_reply;
	//struct session_elem *next;
};

typedef struct session_elem session_elem_t;

typedef enum {
	LFLAG_RUNNING = (1 << 0),
	LFLAG_EVENTS = (1 << 1),
	LFLAG_LOG = (1 << 2),
	LFLAG_MYEVENTS = (1 << 3),
	LFLAG_STATEFUL = (1 << 4)
} event_flag_t;

/* There is one listener for each Erlang node we are attached to - either
   inbound or outbound. For example, if the erlang node node1@server connects
   to freeswitch then a listener is created and handles commands sent from
   that node. If 5 calls are directed to the outbound erlang application
   via the dialplan, and are also set to talk to node1@server, then those
   5 call sessions will be "attached" to the same listener.
 */
struct listener {
#ifdef WIN32
	SOCKET sockfd;
#else
	int sockfd;
#endif
	struct ei_cnode_s *ec;
	struct erlang_process log_process;
	struct erlang_process event_process;
	char *peer_nodename;
	switch_queue_t *event_queue;
	switch_queue_t *log_queue;
	switch_memory_pool_t *pool;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *sock_mutex;
	char *ebuf;
	uint32_t flags;
	switch_log_level_t level;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	switch_hash_t *event_hash;
	switch_thread_rwlock_t *rwlock;
	switch_thread_rwlock_t *event_rwlock;
	switch_thread_rwlock_t *session_rwlock;
	//session_elem_t *session_list;
	switch_hash_t *sessions;
	int lost_events;
	int lost_logs;
	uint32_t id;
	char remote_ip[50];
	/*switch_port_t remote_port; */
	struct listener *next;
};

typedef struct listener listener_t;

struct erlang_binding {
	switch_xml_section_t section;
	struct erlang_process process;
	listener_t *listener;
	struct erlang_binding *next;
};

struct api_command_struct {
	char *api_cmd;
	char *arg;
	listener_t *listener;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	uint8_t bg;
	erlang_pid pid;
	switch_memory_pool_t *pool;
};

struct globals_struct {
	switch_thread_rwlock_t *listener_rwlock;
	switch_thread_rwlock_t *bindings_rwlock;
	switch_event_node_t *node;
	switch_mutex_t *ref_mutex;
	switch_mutex_t *fetch_reply_mutex;
	switch_mutex_t *listener_count_mutex;
	switch_hash_t *fetch_reply_hash;
	unsigned int reference0;
	unsigned int reference1;
	unsigned int reference2;
	char TIMEOUT;				/* marker for a timed out request */
	char WAITING;				/* marker for a request waiting for a response */
	int debug;
};
typedef struct globals_struct globals_t;

struct listen_list_struct {
#ifdef WIN32
	SOCKET sockfd;
#else
	int sockfd;
#endif
	listener_t *listeners;
	uint8_t ready;
};
typedef struct listen_list_struct listen_list_t;

struct bindings_struct {
	struct erlang_binding *head;
	switch_xml_binding_t *search_binding;
};
typedef struct bindings_struct bindings_t;

#define MAX_ACL 100
struct prefs_struct {
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
	erlang_encoding_t encoding;
	int compat_rel;
};
typedef struct prefs_struct prefs_t;

/* shared globals */
#ifdef DEFINE_GLOBALS
globals_t globals;
listen_list_t listen_list;
bindings_t bindings;
prefs_t prefs;
#else
extern globals_t globals;
extern listen_list_t listen_list;
extern bindings_t bindings;
extern prefs_t prefs;
#endif

/* function prototypes */
/* handle_msg.c */
int handle_msg(listener_t *listener, erlang_msg * msg, ei_x_buff * buf, ei_x_buff * rbuf);

/* ei_helpers.c */
void ei_link(listener_t *listener, erlang_pid * from, erlang_pid * to);
void ei_encode_switch_event_headers(ei_x_buff * ebuf, switch_event_t *event);
void ei_encode_switch_event_tag(ei_x_buff * ebuf, switch_event_t *event, char *tag);
int ei_pid_from_rpc(struct ei_cnode_s *ec, int sockfd, erlang_ref * ref, char *module, char *function);
int ei_spawn(struct ei_cnode_s *ec, int sockfd, erlang_ref * ref, char *module, char *function, int argc, char **argv);
void ei_init_ref(struct ei_cnode_s *ec, erlang_ref * ref);
void ei_x_print_reg_msg(ei_x_buff * buf, char *dest, int send);
void ei_x_print_msg(ei_x_buff * buf, erlang_pid * pid, int send);
int ei_sendto(ei_cnode * ec, int fd, struct erlang_process *process, ei_x_buff * buf);
void ei_hash_ref(erlang_ref * ref, char *output);
int ei_compare_pids(erlang_pid * pid1, erlang_pid * pid2);
int ei_decode_string_or_binary(char *buf, int *index, int maxlen, char *dst);
switch_status_t initialise_ei(struct ei_cnode_s *ec);
#define ei_encode_switch_event(_b, _e) ei_encode_switch_event_tag(_b, _e, "event")

/* crazy macro for toggling encoding type */
#define _ei_x_encode_string(buf, string) switch (prefs.encoding) { \
	case ERLANG_BINARY: \
		ei_x_encode_binary(buf, string, strlen(string)); \
		break; \
	default: \
		ei_x_encode_string(buf, string); \
		break; \
}

#ifdef WIN32					/* MSDN suggested hack to fake errno for network operations */
/*#define errno WSAGetLastError()*/
#endif

/* mod_erlang_event.c */
session_elem_t *attach_call_to_registered_process(listener_t *listener, char *reg_name, switch_core_session_t *session);
session_elem_t *attach_call_to_pid(listener_t *listener, erlang_pid * pid, switch_core_session_t *session);
session_elem_t *attach_call_to_spawned_process(listener_t *listener, char *module, char *function, switch_core_session_t *session);
session_elem_t *find_session_elem_by_pid(listener_t *listener, erlang_pid *pid);
void put_reply_unlock(fetch_reply_t *p, char *uuid_str);

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
