#ifndef KAZOO_EI_H
#define KAZOO_EI_H

#include <switch.h>
#include <ei.h>

#define MODNAME "mod_kazoo"
#define BUNDLE "community"
#define RELEASE "v1.5.0-1"
#define VERSION "mod_kazoo v1.5.0-1 community"

#define KZ_MAX_SEPARATE_STRINGS 10

typedef enum {KAZOO_FETCH_PROFILE, KAZOO_EVENT_PROFILE} kazoo_profile_type;

typedef enum {ERLANG_TUPLE, ERLANG_MAP} kazoo_json_term;

typedef struct ei_xml_agent_s ei_xml_agent_t;
typedef ei_xml_agent_t *ei_xml_agent_ptr;

typedef struct kazoo_event kazoo_event_t;
typedef kazoo_event_t *kazoo_event_ptr;

typedef struct kazoo_event_profile kazoo_event_profile_t;
typedef kazoo_event_profile_t *kazoo_event_profile_ptr;

typedef struct kazoo_fetch_profile kazoo_fetch_profile_t;
typedef kazoo_fetch_profile_t *kazoo_fetch_profile_ptr;

typedef struct kazoo_config_t kazoo_config;
typedef kazoo_config *kazoo_config_ptr;

#include "kazoo_fields.h"
#include "kazoo_config.h"

struct ei_send_msg_s {
	ei_x_buff buf;
	erlang_pid pid;
};
typedef struct ei_send_msg_s ei_send_msg_t;

struct ei_received_msg_s {
	ei_x_buff buf;
	erlang_msg msg;
};
typedef struct ei_received_msg_s ei_received_msg_t;


typedef struct ei_event_stream_s ei_event_stream_t;
typedef struct ei_node_s ei_node_t;

struct ei_event_binding_s {
	char id[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_event_node_t *node;
	switch_event_types_t type;
	const char *subclass_name;
	ei_event_stream_t* stream;
	kazoo_event_ptr event;

	struct ei_event_binding_s *next;
};
typedef struct ei_event_binding_s ei_event_binding_t;

struct ei_event_stream_s {
	switch_memory_pool_t *pool;
	ei_event_binding_t *bindings;
	switch_queue_t *queue;
	switch_socket_t *acceptor;
	switch_pollset_t *pollset;
	switch_pollfd_t *pollfd;
	switch_socket_t *socket;
	switch_mutex_t *socket_mutex;
	switch_bool_t connected;
	char remote_ip[48];
	uint16_t remote_port;
	char local_ip[48];
	uint16_t local_port;
	erlang_pid pid;
	uint32_t flags;
	ei_node_t *node;
	short event_stream_framing;
	struct ei_event_stream_s *next;
};

struct ei_node_s {
	int nodefd;
	switch_atomic_t pending_bgapi;
	switch_atomic_t receive_handlers;
	switch_memory_pool_t *pool;
	ei_event_stream_t *event_streams;
	switch_mutex_t *event_streams_mutex;
	switch_queue_t *send_msgs;
	switch_queue_t *received_msgs;
	char *peer_nodename;
	switch_time_t created_time;
	switch_socket_t *socket;
	char remote_ip[48];
	uint16_t remote_port;
	char local_ip[48];
	uint16_t local_port;
	uint32_t flags;
	int legacy;
	short event_stream_framing;
	struct ei_node_s *next;
};


struct xml_fetch_reply_s {
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *xml_str;
	struct xml_fetch_reply_s *next;
};
typedef struct xml_fetch_reply_s xml_fetch_reply_t;

struct fetch_handler_s {
	erlang_pid pid;
	struct fetch_handler_s *next;
};
typedef struct fetch_handler_s fetch_handler_t;

struct ei_xml_client_s {
	ei_node_t *ei_node;
	fetch_handler_t *fetch_handlers;
	struct ei_xml_client_s *next;
};
typedef struct ei_xml_client_s ei_xml_client_t;

struct ei_xml_agent_s {
	switch_memory_pool_t *pool;
	switch_xml_section_t section;
	switch_thread_rwlock_t *lock;
	ei_xml_client_t *clients;
	switch_mutex_t *current_client_mutex;
	ei_xml_client_t *current_client;
	switch_mutex_t *replies_mutex;
	switch_thread_cond_t *new_reply;
	xml_fetch_reply_t *replies;
	kazoo_fetch_profile_ptr profile;

};

typedef enum {
	KZ_TWEAK_INTERACTION_ID,
	KZ_TWEAK_EXPORT_VARS,
	KZ_TWEAK_SWITCH_URI,
	KZ_TWEAK_REPLACES_CALL_ID,
	KZ_TWEAK_LOOPBACK_VARS,
	KZ_TWEAK_CALLER_ID,
	KZ_TWEAK_TRANSFERS,
	KZ_TWEAK_BRIDGE,
	KZ_TWEAK_BRIDGE_REPLACES_ALEG,
	KZ_TWEAK_BRIDGE_REPLACES_CALL_ID,
	KZ_TWEAK_BRIDGE_VARIABLES,
	KZ_TWEAK_RESTORE_CALLER_ID_ON_BLIND_XFER,

	/* No new flags below this line */
	KZ_TWEAK_MAX
} kz_tweak_t;

struct globals_s {
	switch_memory_pool_t *pool;
	switch_atomic_t threads;
	switch_socket_t *acceptor;
	struct ei_cnode_s ei_cnode;
	switch_thread_rwlock_t *ei_nodes_lock;
	ei_node_t *ei_nodes;

	switch_xml_binding_t *config_fetch_binding;
	switch_xml_binding_t *directory_fetch_binding;
	switch_xml_binding_t *dialplan_fetch_binding;
	switch_xml_binding_t *channels_fetch_binding;
	switch_xml_binding_t *languages_fetch_binding;
	switch_xml_binding_t *chatplan_fetch_binding;

	switch_hash_t *event_filter;
	int epmdfd;
	int num_worker_threads;
	switch_bool_t nat_map;
	switch_bool_t ei_shortname;
	int ei_compat_rel;
	char *ip;
	char *hostname;
	char *ei_cookie;
	char *ei_nodename;
	uint32_t flags;
	int send_all_headers;
	int send_all_private_headers;
	int connection_timeout;
	int receive_timeout;
	int receive_msg_preallocate;
	int event_stream_preallocate;
	int send_msg_batch;
	short event_stream_framing;
	switch_port_t port;
	int config_fetched;
	int io_fault_tolerance;
	kazoo_event_profile_ptr events;
	kazoo_config_ptr definitions;
	kazoo_config_ptr event_handlers;
	kazoo_config_ptr fetch_handlers;
	kazoo_json_term  json_encoding;

	char **profile_vars_prefixes;
	char **kazoo_var_prefixes;

	int legacy_events;
	uint8_t tweaks[KZ_TWEAK_MAX];


};
typedef struct globals_s globals_t;
extern globals_t kazoo_globals;

/* kazoo_event_stream.c */
ei_event_stream_t *find_event_stream(ei_event_stream_t *event_streams, const erlang_pid *from);

//ei_event_stream_t *new_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from);
ei_event_stream_t *new_event_stream(ei_node_t *ei_node, const erlang_pid *from);


switch_status_t remove_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from);
switch_status_t remove_event_streams(ei_event_stream_t **event_streams);
unsigned long get_stream_port(const ei_event_stream_t *event_stream);
switch_status_t add_event_binding(ei_event_stream_t *event_stream, const char *event_name);
//switch_status_t add_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name);
switch_status_t remove_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name);
switch_status_t remove_event_bindings(ei_event_stream_t *event_stream);

/* kazoo_node.c */
switch_status_t new_kazoo_node(int nodefd, ErlConnect *conn);

/* kazoo_ei_utils.c */
void close_socket(switch_socket_t **sock);
void close_socketfd(int *sockfd);
switch_socket_t *create_socket_with_port(switch_memory_pool_t *pool, switch_port_t port);
switch_socket_t *create_socket(switch_memory_pool_t *pool);
switch_status_t create_ei_cnode(const char *ip_addr, const char *name, struct ei_cnode_s *ei_cnode);
switch_status_t ei_compare_pids(const erlang_pid *pid1, const erlang_pid *pid2);
void ei_encode_switch_event_headers(ei_x_buff *ebuf, switch_event_t *event);
void ei_encode_switch_event_headers_2(ei_x_buff *ebuf, switch_event_t *event, int decode);
void ei_encode_json(ei_x_buff *ebuf, cJSON *JObj);
void ei_link(ei_node_t *ei_node, erlang_pid * from, erlang_pid * to);
void ei_encode_switch_event(ei_x_buff * ebuf, switch_event_t *event);
int ei_helper_send(ei_node_t *ei_node, erlang_pid* to, ei_x_buff *buf);
int ei_decode_atom_safe(char *buf, int *index, char *dst);
int ei_decode_string_or_binary_limited(char *buf, int *index, int maxsize, char *dst);
int ei_decode_string_or_binary(char *buf, int *index, char **dst);
switch_status_t create_acceptor();
switch_hash_t *create_default_filter();

void fetch_config();

switch_status_t kazoo_load_config();
void kazoo_destroy_config();


#define _ei_x_encode_string(buf, string) { ei_x_encode_binary(buf, string, strlen(string)); }

/* kazoo_fetch_agent.c */
switch_status_t bind_fetch_agents();
switch_status_t unbind_fetch_agents();
switch_status_t remove_xml_clients(ei_node_t *ei_node);
switch_status_t add_fetch_handler(ei_node_t *ei_node, erlang_pid *from, switch_xml_binding_t *binding);
switch_status_t remove_fetch_handlers(ei_node_t *ei_node, erlang_pid *from);
switch_status_t fetch_reply(char *uuid_str, char *xml_str, switch_xml_binding_t *binding);
switch_status_t handle_api_command_streams(ei_node_t *ei_node, switch_stream_handle_t *stream);

void bind_event_profiles(kazoo_event_ptr event);
void rebind_fetch_profiles(kazoo_config_ptr fetch_handlers);
switch_status_t kazoo_config_handlers(switch_xml_t cfg);

/* runtime */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_kazoo_runtime);



#define kz_test_tweak(flag) (kazoo_globals.tweaks[flag] ? 1 : 0)
#define kz_set_tweak(flag) kazoo_globals.tweaks[flag] = 1
#define kz_clear_tweak(flag) kazoo_globals.tweaks[flag] = 0

#endif /* KAZOO_EI_H */

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
