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
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_html.h -- HTML 5 interface
 *
 */

#ifndef MOD_VERTO_H
#define MOD_VERTO_H
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <poll.h>
#endif
#include <stdarg.h>
#ifndef WIN32
#include <netinet/tcp.h>
#include <sys/un.h>
#endif
#include <assert.h>
#include <errno.h>
#ifndef WIN32
#include <pwd.h>
#include <netdb.h>
#endif
#include <openssl/ssl.h>
#include "mcast.h"

#include "ks.h"

#define MAX_QUEUE_LEN 100000
#define MAX_MISSED 500

#define MAXPENDING 10000
#define STACK_SIZE 80 * 1024

#define VERTO_CHAT_PROTO "verto"

#define copy_string(x,y,z) strncpy(x, y, z - 1)
#define set_string(x,y) strncpy(x, y, sizeof(x)-1)

#define CODE_INVALID -32600
#define CODE_AUTH_REQUIRED -32000
#define CODE_AUTH_FAILED -32001
#define CODE_SESSION_ERROR -32002

#define MY_EVENT_CLIENT_CONNECT "verto::client_connect"
#define MY_EVENT_CLIENT_DISCONNECT "verto::client_disconnect"
#define MY_EVENT_LOGIN "verto::login"

typedef enum {
	PTYPE_CLIENT     = (1 << 0),
	PTYPE_CLIENT_SSL = (1 << 1)
} jsock_type_t;

typedef enum {
	JPFLAG_INIT = (1 << 0),
	JPFLAG_AUTHED = (1 << 1),
	JPFLAG_CHECK_ATTACH = (1 << 2),
	JPFLAG_EVENTS = (1 << 3),
	JPFLAG_AUTH_EVENTS = (1 << 4),
	JPFLAG_ALL_EVENTS_AUTHED = (1 << 5),
	JPFLAG_AUTH_EXPIRED = (1 << 6)
} jpflag_t;

struct verto_profile_s;

struct jsock_s {
	ks_socket_t client_socket;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	ks_pool_t *kpool;
	kws_t *ws;
	unsigned char buf[65535];
	char *name;
	jsock_type_t ptype;
	struct sockaddr_in remote_addr;
	struct sockaddr_in6 remote_addr6;
#ifndef WIN32
	struct passwd pw;
#endif
	uint32_t attach_timer;
	
	uint8_t drop;
	uint8_t nodelete;
	ks_socket_t local_sock;
	SSL *ssl;

	jpflag_t flags;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_event_t *allowed_methods;
	switch_event_t *allowed_jsapi;
	switch_event_t *allowed_fsapi;
	switch_event_t *allowed_event_channels;


	char *id;
	char *domain;
	char *uid;
	char *dialplan;
	char *context;


	char remote_host[256];
	int remote_port;
	int family;
	time_t exptime;
	time_t logintime;
	struct verto_profile_s *profile;
	switch_thread_rwlock_t *rwlock;

	switch_mutex_t *write_mutex;
	switch_mutex_t *filter_mutex;
	switch_mutex_t *flag_mutex;

	switch_event_t *params;
	switch_event_t *vars;

	switch_event_t *user_vars;

	switch_queue_t *event_queue;
	int lost_events;
	int ready;

	struct jsock_s *next;
};

typedef struct jsock_s jsock_t;

#define MAX_BIND 25
#define MAX_RTPIP 25

typedef struct ips {
	char local_ip[256];
	uint16_t local_port;
	int secure;
	int family;
} ips_t;

typedef enum {
	TFLAG_SENT_MEDIA = (1 << 0),
	TFLAG_ATTACH_REQ = (1 << 1),
	TFLAG_TRACKED = (1 << 2)
} tflag_t;

typedef struct verto_pvt_s {
	switch_memory_pool_t *pool;
	char *jsock_uuid;
	char *call_id;
	char *r_sdp;
	tflag_t flags;
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_media_handle_t *smh;
	switch_core_media_params_t *mparams;
	switch_call_cause_t remote_hangup_cause;
	time_t detach_time;
	struct verto_pvt_s *next;
	switch_byte_t text_read_frame_data[SWITCH_RTP_MAX_BUF_LEN];
	switch_frame_t text_read_frame;

	switch_thread_cond_t *text_cond;
	switch_mutex_t *text_cond_mutex;
	switch_mutex_t *text_read_mutex;
	switch_mutex_t *text_write_mutex;

	switch_buffer_t *text_read_buffer;
	switch_buffer_t *text_write_buffer;

} verto_pvt_t;

typedef struct verto_vhost_s {
	char *domain;
	char *alias;
	char *root;
	char *script_root;
	char *index;
	char *auth_realm;
	char *auth_user;
	char *auth_pass;
	switch_event_t *rewrites;
	switch_memory_pool_t *pool;
	struct verto_vhost_s *next;
} verto_vhost_t;

struct verto_profile_s {
	char *name;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *rwlock;

	struct ips ip[MAX_BIND];
	int i;

	const SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	char cert[512];
	char key[512];
	char chain[512];

	jsock_t *jsock_head;
	int jsock_count;
	ks_socket_t server_socket[MAX_BIND];
	int running;

	int ssl_ready;
	int ready;
	int debug;
	int chop_domain;
	
	int in_thread;
	int blind_reg;

	char *userauth;
	char *root_passwd;

	int send_passwd;
	
	char *context;
	char *dialplan;

	char *mcast_ip;
	switch_port_t mcast_port;

	mcast_handle_t mcast_sub;
	mcast_handle_t mcast_pub;

	char *extrtpip;

	char *rtpip[MAX_RTPIP];
	int rtpip_index;
	int rtpip_cur;

	char *rtpip6[MAX_RTPIP];
	int rtpip_index6;
	int rtpip_cur6;

	char *cand_acl[SWITCH_MAX_CAND_ACL];
	uint32_t cand_acl_count;

	char *conn_acl[SWITCH_MAX_CAND_ACL];
	uint32_t conn_acl_count;

	char *inbound_codec_string;
	char *outbound_codec_string;

	char *jb_msec;

	char *timer_name;
	char *local_network;

	verto_vhost_t *vhosts;

	char *register_domain;

	int enable_text;

	struct verto_profile_s *next;
};

typedef struct verto_profile_s verto_profile_t;

struct globals_s {
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;

	int profile_count;
	verto_profile_t *profile_head;
	int sig;
	int running;

	switch_hash_t *method_hash;
	switch_mutex_t *method_mutex;

	switch_hash_t *event_channel_hash;
	switch_thread_rwlock_t *event_channel_rwlock;

	int debug;
	int ready;
	int send_passwd;
	int profile_threads;
	int enable_presence;
	int enable_fs_events;
	switch_bool_t kslog_on;

	switch_hash_t *jsock_hash;
	switch_mutex_t *jsock_mutex;

	verto_pvt_t *tech_head;
	switch_thread_rwlock_t *tech_rwlock;

	switch_thread_cond_t *detach_cond;
	switch_mutex_t *detach_mutex;
	switch_mutex_t *detach2_mutex;

	uint32_t detached;
	uint32_t detach_timeout;

	switch_event_channel_id_t event_channel_id;

	switch_log_level_t debug_level;

};


typedef switch_bool_t (*jrpc_func_t)(const char *method, cJSON *params, jsock_t *jsock, cJSON **response);


void set_log_path(const char *path);


/** @} */
#endif
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
