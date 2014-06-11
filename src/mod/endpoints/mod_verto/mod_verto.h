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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h> 
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <stdarg.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include "mcast.h"

#define MAXPENDING 10000
#define STACK_SIZE 80 * 1024

#define VERTO_CHAT_PROTO "verto"

#define copy_string(x,y,z) strncpy(x, y, z - 1) 
#define set_string(x,y) strncpy(x, y, sizeof(x)-1) 

#define CODE_INVALID -32600
#define CODE_AUTH_REQUIRED -32000
#define CODE_AUTH_FAILED -32001
#define CODE_SESSION_ERROR -32002


typedef enum {
	PTYPE_CLIENT     = (1 << 0),
	PTYPE_CLIENT_SSL = (1 << 1)
} jsock_type_t;

typedef enum {
	JPFLAG_INIT = (1 << 0),
	JPFLAG_AUTHED = (1 << 1),
	JPFLAG_CHECK_ATTACH = (1 << 2)
} jpflag_t;

struct verto_profile_s;

struct jsock_s {
	int client_socket;
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	wsh_t ws;
	unsigned char buf[65535];
	char *name;
	jsock_type_t ptype;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	struct sockaddr_in send_addr;
	struct ucred credentials;
	struct passwd pw;

	int drop;
	int local_sock;
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
	
	struct verto_profile_s *profile;
	switch_thread_rwlock_t *rwlock;
	
	switch_mutex_t *write_mutex;

	switch_event_t *params;
	switch_event_t *vars;

	struct jsock_s *next;
};

typedef struct jsock_s jsock_t;

#define MAX_BIND 25
#define MAX_RTPIP 25

struct ips {
	char local_ip[256];
	in_addr_t local_ip_addr;
	int local_port;
	int secure;
};

typedef enum {
	TFLAG_SENT_MEDIA = (1 << 0),
	TFLAG_ATTACH_REQ = (1 << 1)
} tflag_t;

typedef struct verto_pvt_s {
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
} verto_pvt_t;

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
	int server_socket[MAX_BIND];
	int running;

	int ssl_ready;
	int ready;
	int debug;
	
	int in_thread;

	char *userauth;
	char *root_passwd;

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

	char *cand_acl[SWITCH_MAX_CAND_ACL];
	uint32_t cand_acl_count;

	char *inbound_codec_string;
	char *outbound_codec_string;

	char *timer_name;
	char *local_network;



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
	int profile_threads;
	int enable_presence;

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
};


extern struct globals_s globals;

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
