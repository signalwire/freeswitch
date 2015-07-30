#include <switch.h>
#include <ei.h>

#define MAX_ACL 100
#define CMD_BUFLEN 1024 * 1000
#define MAX_QUEUE_LEN 25000
#define MAX_MISSED 500
#define MAX_PID_CHARS 255
#define VERSION "mod_kazoo v1.2.10-14"

#define API_COMMAND_DISCONNECT 0
#define API_COMMAND_REMOTE_IP 1
#define API_COMMAND_STREAMS 2
#define API_COMMAND_BINDINGS 3

typedef enum {
	LFLAG_RUNNING = (1 << 0)
} event_flag_t;

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

struct ei_event_binding_s {
	char id[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_event_node_t *node;
	switch_event_types_t type;
	const char *subclass_name;
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
	struct ei_event_stream_s *next;
};
typedef struct ei_event_stream_s ei_event_stream_t;

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
	char remote_ip[25];
	uint16_t remote_port;
	char local_ip[25];
	uint16_t local_port;
	uint32_t flags;
	struct ei_node_s *next;
};
typedef struct ei_node_s ei_node_t;

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
	switch_xml_binding_t *chatplan_fetch_binding;
	switch_xml_binding_t *channels_fetch_binding;
	switch_hash_t *event_filter;
	int epmdfd;
	int num_worker_threads;
	switch_bool_t nat_map;
	switch_bool_t ei_shortname;
	int ei_compat_rel;
	char *ip;
	char *ei_cookie;
	char *ei_nodename;
	char *kazoo_var_prefix;
	int var_prefix_length;
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
};
typedef struct globals_s globals_t;
extern globals_t globals;

/* kazoo_node.c */
switch_status_t new_kazoo_node(int nodefd, ErlConnect *conn);

/* kazoo_event_stream.c */
ei_event_stream_t *find_event_stream(ei_event_stream_t *event_streams, const erlang_pid *from);
ei_event_stream_t *new_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from);
switch_status_t remove_event_stream(ei_event_stream_t **event_streams, const erlang_pid *from);
switch_status_t remove_event_streams(ei_event_stream_t **event_streams);
unsigned long get_stream_port(const ei_event_stream_t *event_stream);
switch_status_t add_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name);
switch_status_t remove_event_binding(ei_event_stream_t *event_stream, const switch_event_types_t event_type, const char *subclass_name);
switch_status_t remove_event_bindings(ei_event_stream_t *event_stream);

/* kazoo_fetch_agent.c */
switch_status_t bind_fetch_agents();
switch_status_t unbind_fetch_agents();
switch_status_t remove_xml_clients(ei_node_t *ei_node);
switch_status_t add_fetch_handler(ei_node_t *ei_node, erlang_pid *from, switch_xml_binding_t *binding);
switch_status_t remove_fetch_handlers(ei_node_t *ei_node, erlang_pid *from);
switch_status_t fetch_reply(char *uuid_str, char *xml_str, switch_xml_binding_t *binding);
switch_status_t handle_api_command_streams(ei_node_t *ei_node, switch_stream_handle_t *stream);

/* kazoo_utils.c */
void close_socket(switch_socket_t **sock);
void close_socketfd(int *sockfd);
switch_socket_t *create_socket_with_port(switch_memory_pool_t *pool, switch_port_t port);
switch_socket_t *create_socket(switch_memory_pool_t *pool);
switch_status_t create_ei_cnode(const char *ip_addr, const char *name, struct ei_cnode_s *ei_cnode);
switch_status_t ei_compare_pids(const erlang_pid *pid1, const erlang_pid *pid2);
void ei_encode_switch_event_headers(ei_x_buff *ebuf, switch_event_t *event);
void ei_link(ei_node_t *ei_node, erlang_pid * from, erlang_pid * to);
void ei_encode_switch_event(ei_x_buff * ebuf, switch_event_t *event);
int ei_helper_send(ei_node_t *ei_node, erlang_pid* to, ei_x_buff *buf);
int ei_decode_atom_safe(char *buf, int *index, char *dst);
int ei_decode_string_or_binary_limited(char *buf, int *index, int maxsize, char *dst);
int ei_decode_string_or_binary(char *buf, int *index, char **dst);
switch_hash_t *create_default_filter();

/* kazoo_commands.c */
void add_kz_commands(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface);

/* kazoo_dptools.c */
void add_kz_dptools(switch_loadable_module_interface_t **module_interface, switch_application_interface_t *app_interface);

#define _ei_x_encode_string(buf, string) { ei_x_encode_binary(buf, string, strlen(string)); }

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
