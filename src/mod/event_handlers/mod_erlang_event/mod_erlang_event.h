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
 * Rob Charlton <rob.charlton@savageminds.com>
 *
 *
 * mod_erlang_event.h -- Erlang Event Handler derived from mod_event_socket
 *
 */


typedef enum {
	LFLAG_OUTBOUND_INIT = (1 << 0), /* Erlang peer has been notified of this session */
	LFLAG_SESSION_ALIVE
} session_flag_t;

typedef enum {
	ERLANG_PID = 0,
	ERLANG_REG_PROCESS
} process_type;

struct erlang_process {
	process_type type;
	char *reg_name;
	erlang_pid pid;
};

struct session_elem {
	switch_core_session_t *session;
	switch_mutex_t *flag_mutex;
	uint32_t flags;
	/* registered process name that will receive call notifications from this session */
	struct erlang_process process;
	switch_queue_t *event_queue;
	struct session_elem *next;
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
	int sockfd;
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
	switch_hash_t *fetch_reply_hash;
	switch_thread_rwlock_t *rwlock;
	switch_mutex_t *session_mutex;
	session_elem_t *session_list;
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
	switch_mutex_t *listener_mutex;
	switch_event_node_t *node;
};
typedef struct globals_struct globals_t;

struct listen_list_struct {
	int sockfd;
	switch_mutex_t *sock_mutex;
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
int handle_msg(listener_t *listener, erlang_msg *msg, ei_x_buff *buf, ei_x_buff *rbuf);

/* ei_helpers.c */
void ei_link(listener_t *listener, erlang_pid *from, erlang_pid *to);
void ei_encode_switch_event_headers(ei_x_buff *ebuf, switch_event_t *event);
void ei_encode_switch_event_tag(ei_x_buff *ebuf, switch_event_t *event, char *tag);
switch_status_t initialise_ei(struct ei_cnode_s *ec);
#define ei_encode_switch_event(_b, _e) ei_encode_switch_event_tag(_b, _e, "event")

/* mod_erlang_event.c */
session_elem_t* attach_call_to_listener(listener_t* listener, char* reg_name, switch_core_session_t *session);

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
