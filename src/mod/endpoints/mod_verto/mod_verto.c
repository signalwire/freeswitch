/*
 * Freeswitch Modular Media Switching Software Library / Soft-Switch Application
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
 * mod_verto.c -- HTML5 Verto interface
 *
 */
#include <switch.h>
#include <switch_json.h>
#include <switch_stun.h>


/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_verto_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_verto_load);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_verto_runtime);

SWITCH_MODULE_DEFINITION(mod_verto, mod_verto_load, mod_verto_shutdown, mod_verto_runtime);

#define EP_NAME "verto.rtc"
//#define WSS_STANDALONE 1
#include "ws.h"

//////////////////////////
#include <mod_verto.h>
#ifndef WIN32
#include <sys/param.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <sys/file.h>
#endif
#include <ctype.h>
#include <sys/stat.h>

#ifdef WIN32
#define strerror_r(errno, buf, len) strerror_s(buf, len, errno)
#endif

#define log_and_exit(severity, ...) switch_log_printf(SWITCH_CHANNEL_LOG, (severity), __VA_ARGS__); goto error
#define die(...) log_and_exit(SWITCH_LOG_WARNING, __VA_ARGS__)
#define die_errno(fmt) do { char errbuf[BUFSIZ] = {0}; strerror_r(errno, (char *)&errbuf, sizeof(errbuf)); die(fmt ", errno=%d, %s\n", errno, (char *)&errbuf); } while(0)
#define die_errnof(fmt, ...) do { char errbuf[BUFSIZ] = {0}; strerror_r(errno, (char *)&errbuf, sizeof(errbuf)); die(fmt ", errno=%d, %s\n", __VA_ARGS__, errno, (char *)&errbuf); } while(0)

static struct globals_s verto_globals;


static struct {
	switch_mutex_t *store_mutex;
	switch_hash_t *store_hash;
} json_GLOBALS;


const char json_sql[] =
	"create table json_store (\n"
	" name varchar(255) not null,\n"
	" data text\n"
	");\n";


typedef enum {
	CMD_ADD,
	CMD_DEL,
	CMD_DUMP,
	CMD_COMMIT,
	CMD_RETRIEVE
} store_cmd_t;

typedef struct {
	switch_mutex_t *mutex;
	cJSON *JSON_STORE;
} json_store_t;

static void json_cleanup(void)
{
	switch_hash_index_t *hi = NULL;
	void *val;
	const void *var;
	cJSON *json;

	if (!json_GLOBALS.store_hash) {
		return;
	}

	switch_mutex_lock(json_GLOBALS.store_mutex);
 top:

	for (hi = switch_core_hash_first_iter(json_GLOBALS.store_hash, hi); hi;) {
		switch_core_hash_this(hi, &var, NULL, &val);
		json = (cJSON *) val;
		cJSON_Delete(json);
		switch_core_hash_delete(json_GLOBALS.store_hash, var);
		goto top;
	}
	switch_safe_free(hi);

	switch_mutex_unlock(json_GLOBALS.store_mutex);

}

static switch_bool_t check_name(const char *name)
{
	const char *p;

	for(p = name; p && *p; p++) {
		if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_') continue;
		return SWITCH_FALSE;
	}

	return SWITCH_TRUE;
}


static switch_status_t verto_read_text_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id);
static switch_status_t verto_write_text_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id);
static void set_text_funcs(switch_core_session_t *session);

static verto_profile_t *find_profile(const char *name);
static jsock_t *get_jsock(const char *uuid);

static void verto_deinit_ssl(verto_profile_t *profile)
{
	if (profile->ssl_ctx) {
		SSL_CTX_free(profile->ssl_ctx);
		profile->ssl_ctx = NULL;
	}
}

static void close_file(ws_socket_t *sock)
{
	if (*sock != ws_sock_invalid) {
#ifndef WIN32
		close(*sock);
#else
		closesocket(*sock);
#endif
		*sock = ws_sock_invalid;
	}
}

static void close_socket(ws_socket_t *sock)
{
	if (*sock != ws_sock_invalid) {
		shutdown(*sock, 2);
		close_file(sock);
	}
}

void verto_broadcast(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id, void *user_data);

static int verto_init_ssl(verto_profile_t *profile)
{
	const char *err = "";
	int i = 0;

	profile->ssl_method = SSLv23_server_method();   /* create server instance */
	profile->ssl_ctx = SSL_CTX_new(profile->ssl_method);         /* create context */
	profile->ssl_ready = 1;
	assert(profile->ssl_ctx);

	/* Disable SSLv2 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_SSLv2);
	/* Disable SSLv3 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_SSLv3);
	/* Disable TLSv1 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_TLSv1);
	/* Disable Compression CRIME (Compression Ratio Info-leak Made Easy) */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_COMPRESSION);

	/* set the local certificate from CertFile */
	if (!zstr(profile->chain)) {
		if (switch_file_exists(profile->chain, NULL) != SWITCH_STATUS_SUCCESS) {
			err = "SUPPLIED CHAIN FILE NOT FOUND\n";
			goto fail;
		}

		if (!SSL_CTX_use_certificate_chain_file(profile->ssl_ctx, profile->chain)) {
			err = "CERT CHAIN FILE ERROR";
			goto fail;
		}
	}

	if (switch_file_exists(profile->cert, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "SUPPLIED CERT FILE NOT FOUND\n";
		goto fail;
	}

	if (!SSL_CTX_use_certificate_file(profile->ssl_ctx, profile->cert, SSL_FILETYPE_PEM)) {
		err = "CERT FILE ERROR";
		goto fail;
	}

	/* set the private key from KeyFile */

	if (switch_file_exists(profile->key, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "SUPPLIED KEY FILE NOT FOUND\n";
		goto fail;
	}

	if (!SSL_CTX_use_PrivateKey_file(profile->ssl_ctx, profile->key, SSL_FILETYPE_PEM)) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	/* verify private key */
	if ( !SSL_CTX_check_private_key(profile->ssl_ctx) ) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	SSL_CTX_set_cipher_list(profile->ssl_ctx, "HIGH:!DSS:!aNULL@STRENGTH");

	return 1;

 fail:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SSL ERR: %s\n", err);

	profile->ssl_ready = 0;
	verto_deinit_ssl(profile);

	for (i = 0; i < profile->i; i++) {
		if (profile->ip[i].secure) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SSL NOT ENABLED FOR LISTENER %s:%d. REVERTING TO WS\n",
							  profile->ip[i].local_ip, profile->ip[i].local_port);
			profile->ip[i].secure = 0;
		}
	}

	return 0;

}


struct jsock_sub_node_head_s;

typedef struct jsock_sub_node_s {
	jsock_t *jsock;
	uint32_t serno;
	struct jsock_sub_node_head_s *head;
	struct jsock_sub_node_s *next;
} jsock_sub_node_t;

typedef struct jsock_sub_node_head_s {
	jsock_sub_node_t *node;
	jsock_sub_node_t *tail;
	char *event_channel;
} jsock_sub_node_head_t;

static uint32_t jsock_unsub_head(jsock_t *jsock, jsock_sub_node_head_t *head)
{
	uint32_t x = 0;

	jsock_sub_node_t *thisnp = NULL, *np, *last = NULL;

	np = head->tail = head->node;

	while (np) {

		thisnp = np;
		np = np->next;

		if (!jsock || thisnp->jsock == jsock) {
			x++;

			if (last) {
				last->next = np;
			} else {
				head->node = np;
			}

			if (thisnp->jsock->profile->debug || verto_globals.debug) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "UNSUBBING %s [%s]\n", thisnp->jsock->name, thisnp->head->event_channel);
			}

			thisnp->jsock = NULL;
			free(thisnp);
		} else {
			last = thisnp;
			head->tail = last;
		}
	}

	return x;
}

static void detach_calls(jsock_t *jsock);

static void unsub_all_jsock(void)
{
	switch_hash_index_t *hi;
	void *val;
	jsock_sub_node_head_t *head;

	switch_thread_rwlock_wrlock(verto_globals.event_channel_rwlock);
 top:
	head = NULL;

	for (hi = switch_core_hash_first(verto_globals.event_channel_hash); hi;) {
		switch_core_hash_this(hi, NULL, NULL, &val);
		head = (jsock_sub_node_head_t *) val;
		jsock_unsub_head(NULL, head);
		switch_core_hash_delete(verto_globals.event_channel_hash, head->event_channel);
		free(head->event_channel);
		free(head);
		switch_safe_free(hi);
		goto top;
	}

	switch_thread_rwlock_unlock(verto_globals.event_channel_rwlock);
}

static uint32_t jsock_unsub_channel(jsock_t *jsock, const char *event_channel)
{
	jsock_sub_node_head_t *head;
	uint32_t x = 0;

	switch_thread_rwlock_wrlock(verto_globals.event_channel_rwlock);

	if (!event_channel) {
		switch_hash_index_t *hi;
		void *val;

		for (hi = switch_core_hash_first(verto_globals.event_channel_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);

			if (val) {
				head = (jsock_sub_node_head_t *) val;
				x += jsock_unsub_head(jsock, head);
			}
		}

	} else {
		if ((head = switch_core_hash_find(verto_globals.event_channel_hash, event_channel))) {
			x += jsock_unsub_head(jsock, head);
		}
	}

	switch_thread_rwlock_unlock(verto_globals.event_channel_rwlock);

	return x;
}

static void presence_ping(const char *event_channel)
{
	switch_console_callback_match_t *matches;
	const char *val = event_channel;

	if (val) {
		if (!strcasecmp(val, "presence")) {
			val = NULL;
		} else {
			char *p;
			if ((p = strchr(val, '.'))) {
				val = (p+1);
			}
		}
	}

	if ((matches = switch_core_session_findall_matching_var("presence_id", val))) {
		switch_console_callback_match_node_t *m;
		switch_core_session_t *session;

		for (m = matches->head; m; m = m->next) {
			if ((session = switch_core_session_locate(m->val))) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				switch_event_t *event;

				if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_CALLSTATE) == SWITCH_STATUS_SUCCESS) {
					switch_channel_callstate_t callstate = switch_channel_get_callstate(channel);

					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Original-Channel-Call-State", switch_channel_callstate2str(callstate));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Channel-Call-State-Number", "%d", callstate);
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}

				switch_core_session_rwunlock(session);
			}
		}

		switch_console_free_matches(&matches);
	}
}

static switch_status_t jsock_sub_channel(jsock_t *jsock, const char *event_channel)
{
	jsock_sub_node_t *node, *np;
	jsock_sub_node_head_t *head;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_thread_rwlock_wrlock(verto_globals.event_channel_rwlock);

	if (!(head = switch_core_hash_find(verto_globals.event_channel_hash, event_channel))) {
		switch_zmalloc(head, sizeof(*head));
		head->event_channel = strdup(event_channel);
		switch_core_hash_insert(verto_globals.event_channel_hash, event_channel, head);

		switch_zmalloc(node, sizeof(*node));
		node->jsock = jsock;
		node->head = head;
		head->node = node;
		head->tail = node;
		status = SWITCH_STATUS_SUCCESS;
	} else {
		int exist = 0;

		for (np = head->node; np; np = np->next) {
			if (np->jsock == jsock) {
				exist = 1;
				break;
			}
		}

		if (!exist) {
			switch_zmalloc(node, sizeof(*node));
			node->jsock = jsock;
			node->head = head;

			if (!head->node) {
				head->node = node;
				head->tail = node;
			} else {
				head->tail->next = node;
				head->tail = head->tail->next;
			}
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	switch_thread_rwlock_unlock(verto_globals.event_channel_rwlock);

	if (status == SWITCH_STATUS_SUCCESS && !strncasecmp(event_channel, "presence", 8)) {
		presence_ping(event_channel);
	}

	return status;
}

static uint32_t ID = 1;

static void del_jsock(jsock_t *jsock)
{
	jsock_t *p, *last = NULL;

	jsock_unsub_channel(jsock, NULL);
	switch_event_channel_permission_clear(jsock->uuid_str);

	switch_mutex_lock(jsock->profile->mutex);
	for(p = jsock->profile->jsock_head; p; p = p->next) {
		if (p == jsock) {
			if (last) {
				last->next = p->next;
			} else {
				jsock->profile->jsock_head = p->next;
			}
			jsock->profile->jsock_count--;
			break;
		}

		last = p;
	}
	switch_mutex_unlock(jsock->profile->mutex);

}

static void add_jsock(jsock_t *jsock)
{

	switch_mutex_lock(jsock->profile->mutex);
	jsock->next = jsock->profile->jsock_head;
	jsock->profile->jsock_head = jsock;
	jsock->profile->jsock_count++;
	switch_mutex_unlock(jsock->profile->mutex);

}

static uint32_t next_id(void)
{
	uint32_t id;

	switch_mutex_lock(verto_globals.mutex);
	id = ID++;
	switch_mutex_unlock(verto_globals.mutex);

	return id;
}

static cJSON *jrpc_new(uint32_t id)
{
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddItemToObject(obj, "jsonrpc", cJSON_CreateString("2.0"));

	if (id) {
		cJSON_AddItemToObject(obj, "id", cJSON_CreateNumber(id));
	}

	return obj;
}

static cJSON *jrpc_new_req(const char *method, const char *call_id, cJSON **paramsP)
{
	cJSON *msg, *params = NULL;
	uint32_t id = next_id();

	msg = jrpc_new(id);

	if (paramsP && *paramsP) {
		params = *paramsP;
	}

	if (!params) {
		params = cJSON_CreateObject();
	}

	cJSON_AddItemToObject(msg, "method", cJSON_CreateString(method));
	cJSON_AddItemToObject(msg, "params", params);

	if (call_id) {
		cJSON_AddItemToObject(params, "callID", cJSON_CreateString(call_id));
	}

	if (paramsP) {
		*paramsP = params;
	}

	return msg;
}

static void jrpc_add_id(cJSON *obj, cJSON *jid, const char *idstr, int id)
{
	if (jid) {
		cJSON_AddItemToObject(obj, "id", cJSON_Duplicate(jid, 1));
	} else if (idstr) {
		cJSON_AddItemToObject(obj, "id", zstr(idstr) ? cJSON_CreateNull() : cJSON_CreateString(idstr));
	} else {
		cJSON_AddItemToObject(obj, "id", cJSON_CreateNumber(id));
	}
}

static void jrpc_add_error(cJSON *obj, int code, const char *message, cJSON *jid)
{
	cJSON *error = cJSON_CreateObject();

	cJSON_AddItemToObject(obj, "error", error);
	cJSON_AddItemToObject(error, "code", cJSON_CreateNumber(code));
	cJSON_AddItemToObject(error, "message", cJSON_CreateString(message));
	if (!cJSON_GetObjectItem(obj, "id")) {
		jrpc_add_id(obj, jid, "", 0);
	}
}

static void jrpc_add_result(cJSON *obj, cJSON *result)
{
	if (result) {
		cJSON_AddItemToObject(obj, "result", result);
	}
}

static switch_ssize_t ws_write_json(jsock_t *jsock, cJSON **json, switch_bool_t destroy)
{
	char *json_text;
	switch_ssize_t r = -1;

	switch_assert(json);

	if (!*json) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "WRITE NULL JS ERROR %" SWITCH_SIZE_T_FMT "\n", r);
		return r;
	}

	if (!zstr(jsock->uuid_str)) {
		cJSON *result = cJSON_GetObjectItem(*json, "result");

		if (result) {
			cJSON_AddItemToObject(result, "sessid", cJSON_CreateString(jsock->uuid_str));
		}
	}

	if ((json_text = cJSON_PrintUnformatted(*json))) {
		if (jsock->profile->debug || verto_globals.debug) {
			char *log_text = cJSON_Print(*json);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WRITE %s [%s]\n", jsock->name, log_text);
			free(log_text);
		}
		switch_mutex_lock(jsock->write_mutex);
		r = ws_write_frame(&jsock->ws, WSOC_TEXT, json_text, strlen(json_text));
		switch_mutex_unlock(jsock->write_mutex);
		switch_safe_free(json_text);
	}

	if (destroy) {
		cJSON_Delete(*json);
		*json = NULL;
	}

	if (r <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ALERT, "WRITE RETURNED ERROR %" SWITCH_SIZE_T_FMT " \n", r);
		jsock->drop = 1;
		jsock->ready = 0;
	}

	return r;
}

static switch_status_t jsock_queue_event(jsock_t *jsock, cJSON **json, switch_bool_t destroy)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	cJSON *jp;

	if (destroy) {
		jp = *json;
	} else {
		jp = cJSON_Duplicate(*json, 1);
	}

	if (switch_queue_trypush(jsock->event_queue, jp) == SWITCH_STATUS_SUCCESS) {
		status = SWITCH_STATUS_SUCCESS;

		if (jsock->lost_events) {
			int le = jsock->lost_events;
			jsock->lost_events = 0;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Lost %d json events!\n", le);
		}
	} else {
		if (++jsock->lost_events > MAX_MISSED) {
			jsock->drop++;
		}

		if (!destroy) {
			cJSON_Delete(jp);
			jp = NULL;
		}
	}

	if (destroy) {
		*json = NULL;
	}

	return status;
}

static void write_event(const char *event_channel, jsock_t *use_jsock, cJSON *event)
{
	jsock_sub_node_head_t *head;

	if ((head = switch_core_hash_find(verto_globals.event_channel_hash, event_channel))) {
		jsock_sub_node_t *np;

		for(np = head->node; np; np = np->next) {
			cJSON *msg = NULL, *params;

			if (!use_jsock || use_jsock == np->jsock) {
				params = cJSON_Duplicate(event, 1);
				cJSON_AddItemToObject(params, "eventSerno", cJSON_CreateNumber(np->serno++));
				msg = jrpc_new_req("verto.event", NULL, &params);
				jsock_queue_event(np->jsock, &msg, SWITCH_TRUE);
			}
		}
	}
}

static void jsock_send_event(cJSON *event)
{

	const char *event_channel, *session_uuid = NULL;
	jsock_t *use_jsock = NULL;
	switch_core_session_t *session = NULL;

	if (!(event_channel = cJSON_GetObjectCstr(event, "eventChannel"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NO EVENT CHANNEL SPECIFIED\n");
		return;
	}


	if ((session = switch_core_session_locate(event_channel))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *jsock_uuid_str = switch_channel_get_variable(channel, "jsock_uuid_str");
		if (jsock_uuid_str) {
			use_jsock = get_jsock(jsock_uuid_str);
		}
		switch_core_session_rwunlock(session);
	}

	if (use_jsock || (use_jsock = get_jsock(event_channel))) { /* implicit subscription to channel identical to the connection uuid or session uuid */
		cJSON *msg = NULL, *params;
		params = cJSON_Duplicate(event, 1);
		msg = jrpc_new_req("verto.event", NULL, &params);
		jsock_queue_event(use_jsock, &msg, SWITCH_TRUE);
		switch_thread_rwlock_unlock(use_jsock->rwlock);
		use_jsock = NULL;
		return;
	}


	if ((session_uuid = cJSON_GetObjectCstr(event, "sessid"))) {
		if (!(use_jsock = get_jsock(session_uuid))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Socket %s not connected\n", session_uuid);
			return;
		}
	}

	switch_thread_rwlock_rdlock(verto_globals.event_channel_rwlock);
	write_event(event_channel, use_jsock, event);
	if (strchr(event_channel, '.')) {
		char *main_channel = strdup(event_channel);
		char *p;
		switch_assert(main_channel);
		p = strchr(main_channel, '.');
		if (p) *p = '\0';
		write_event(main_channel, use_jsock, event);
		free(main_channel);
	}
	switch_thread_rwlock_unlock(verto_globals.event_channel_rwlock);

	if (use_jsock) {
		switch_thread_rwlock_unlock(use_jsock->rwlock);
		use_jsock = NULL;
	}
}

static jrpc_func_t jrpc_get_func(jsock_t *jsock, const char *method)
{
	jrpc_func_t func = NULL;
	char *main_method = NULL;

	switch_assert(method);

	if (jsock->allowed_methods) {
		if (strchr(method, '.')) {
			char *p;
			main_method = strdup(method);
			switch_assert(main_method);
			if ((p = strchr(main_method, '.'))) {
				*p = '\0';
			}
		}

		if (!(switch_event_get_header(jsock->allowed_methods, method) || (main_method && switch_event_get_header(jsock->allowed_methods, main_method)))) {
			goto end;
		}
	}

	switch_mutex_lock(verto_globals.method_mutex);
	func = (jrpc_func_t) (intptr_t) switch_core_hash_find(verto_globals.method_hash, method);
	switch_mutex_unlock(verto_globals.method_mutex);

 end:

	switch_safe_free(main_method);

	return func;
}


static void jrpc_add_func(const char *method, jrpc_func_t func)
{
	switch_assert(method);
	switch_assert(func);

	switch_mutex_lock(verto_globals.method_mutex);
	switch_core_hash_insert(verto_globals.method_hash, method, (void *) (intptr_t) func);
	switch_mutex_unlock(verto_globals.method_mutex);
}

static char *MARKER = "X";

static void set_perm(const char *str, switch_event_t **event)
{
	char delim = ',';
	char *cur, *next;
	int count = 0;
	char *edup;

	if (!zstr(str)) {
		if (!strcasecmp(str, "__ANY__")) {
			return;
		}
	}

	switch_event_create(event, SWITCH_EVENT_REQUEST_PARAMS);

	if (!zstr(str)) {
		edup = strdup(str);
		switch_assert(edup);

		if (strchr(edup, ' ')) {
			delim = ' ';
		}

		for (cur = edup; cur; count++) {
			if ((next = strchr(cur, delim))) {
				*next++ = '\0';
			}

			switch_event_add_header_string(*event, SWITCH_STACK_BOTTOM, cur, MARKER);

			cur = next;
		}

		switch_safe_free(edup);

	}
}

static void check_permissions(jsock_t *jsock, switch_xml_t x_user, cJSON *params)
{
	switch_xml_t x_param, x_params;
	const char *allowed_methods = NULL, *allowed_jsapi = NULL, *allowed_fsapi = NULL, *allowed_event_channels = NULL;

	if ((x_params = switch_xml_child(x_user, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (zstr(val) || zstr(var)) {
				continue;
			}

			if (!strcasecmp(var, "jsonrpc-allowed-methods")) {
				allowed_methods = val;
			}

			if (!strcasecmp(var, "jsonrpc-allowed-jsapi")) {
				allowed_jsapi = val;
			}

			if (!strcasecmp(var, "jsonrpc-allowed-fsapi")) {
				allowed_fsapi = val;
			}

			if (!strcasecmp(var, "jsonrpc-allowed-event-channels")) {
				allowed_event_channels = val;
			}
		}
	}


	set_perm(allowed_methods, &jsock->allowed_methods);
	set_perm(allowed_jsapi, &jsock->allowed_jsapi);
	set_perm(allowed_fsapi, &jsock->allowed_fsapi);
	set_perm(allowed_event_channels, &jsock->allowed_event_channels);

	switch_event_add_header_string(jsock->allowed_methods, SWITCH_STACK_BOTTOM, "login", MARKER);

}

static void login_fire_custom_event(jsock_t *jsock, cJSON *params, int success, const char *result_txt)
{
	switch_event_t *s_event;

	if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_LOGIN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_profile_name", jsock->profile->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_client_address", jsock->name);
		if (params) {
			switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_login", cJSON_GetObjectCstr(params, "login"));
			if (success) {
				switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_sessid", cJSON_GetObjectCstr(params, "sessid"));
			}
		}
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "verto_success", "%d", success);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_result_txt", result_txt);
		switch_event_fire(&s_event);
	}
}

static switch_bool_t check_auth(jsock_t *jsock, cJSON *params, int *code, char *message, switch_size_t mlen)
{
	switch_bool_t r = SWITCH_FALSE;
	const char *passwd = NULL;
	const char *login = NULL;
	cJSON *json_ptr = NULL;
	char *input = NULL;
	char *a1_hash = NULL;
	char a1_hash_buff[33] = "";

	if (!params) {
		*code = CODE_AUTH_FAILED;
		switch_snprintf(message, mlen, "Missing params");
		goto end;
	}

	login = cJSON_GetObjectCstr(params, "login");
	passwd = cJSON_GetObjectCstr(params, "passwd");

	if (zstr(login)) {
		goto end;
	}

	if (zstr(passwd)) {
		*code = CODE_AUTH_FAILED;
		switch_snprintf(message, mlen, "Missing passwd");
		login_fire_custom_event(jsock, params, 0, "Missing passwd");
		goto end;
	}


	if (!strcmp(login, "root") && jsock->profile->root_passwd) {
		if (!(r = !strcmp(passwd, jsock->profile->root_passwd))) {
			*code = CODE_AUTH_FAILED;
			switch_snprintf(message, mlen, "Authentication Failure");
			login_fire_custom_event(jsock, params, 0, "Authentication Failure");
		}

	} else if (!zstr(jsock->profile->userauth)) {
		switch_xml_t x_user = NULL;
		char *id = NULL, *domain = NULL;
		switch_event_t *req_params;

		if (*jsock->profile->userauth == '@') {
			domain = jsock->profile->userauth + 1;
			id = (char *) login;
		} else if (switch_true(jsock->profile->userauth)) {
			id = switch_core_strdup(jsock->pool, login);

			if ((domain = strchr(id, '@'))) {
				*domain++ = '\0';
			}

		}

		if (jsock->profile->register_domain) {
			domain = jsock->profile->register_domain;
		}

		if (!(id && domain)) {
			*code = CODE_AUTH_FAILED;
			switch_snprintf(message, mlen, "Missing or improper credentials");
			goto end;
		}

		switch_event_create(&req_params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(req_params);

		if ((json_ptr = cJSON_GetObjectItem(params, "loginParams"))) {
			cJSON * i;

			for(i = json_ptr->child; i; i = i->next) {
				if (i->type == cJSON_True) {
					switch_event_add_header_string(req_params, SWITCH_STACK_BOTTOM, i->string, "true");
				} else if (i->type == cJSON_False) {
					switch_event_add_header_string(req_params, SWITCH_STACK_BOTTOM, i->string, "false");
				} else if (!zstr(i->string) && !zstr(i->valuestring)) {
					switch_event_add_header_string(req_params, SWITCH_STACK_BOTTOM, i->string, i->valuestring);
				}
			}
		}


		if ((json_ptr = cJSON_GetObjectItem(params, "userVariables"))) {
			cJSON * i;

			for(i = json_ptr->child; i; i = i->next) {
				if (i->type == cJSON_True) {
					switch_event_add_header_string(jsock->user_vars, SWITCH_STACK_BOTTOM, i->string, "true");
				} else if (i->type == cJSON_False) {
					switch_event_add_header_string(jsock->user_vars, SWITCH_STACK_BOTTOM, i->string, "false");
				} else if (!zstr(i->string) && !zstr(i->valuestring)) {
					switch_event_add_header_string(jsock->user_vars, SWITCH_STACK_BOTTOM, i->string, i->valuestring);
				}
			}
		}

		switch_event_add_header_string(req_params, SWITCH_STACK_BOTTOM, "action", "jsonrpc-authenticate");

		if (switch_xml_locate_user_merged("id", id, domain, NULL, &x_user, req_params) != SWITCH_STATUS_SUCCESS && !jsock->profile->blind_reg) {
			*code = CODE_AUTH_FAILED;
			switch_snprintf(message, mlen, "Login Incorrect");
			login_fire_custom_event(jsock, params, 0, "Login Incorrect");
		} else {
			switch_xml_t x_param, x_params;
			const char *use_passwd = NULL, *verto_context = NULL, *verto_dialplan = NULL;

			jsock->id = switch_core_strdup(jsock->pool, id);
			jsock->domain = switch_core_strdup(jsock->pool, domain);
			jsock->uid = switch_core_sprintf(jsock->pool, "%s@%s", id, domain);
			jsock->ready = 1;

			if (!x_user) {
				switch_event_destroy(&req_params);
				r = SWITCH_TRUE;
				goto end;
			}

			if ((x_params = switch_xml_child(x_user, "params"))) {
				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!use_passwd && !strcasecmp(var, "password")) {
						use_passwd = val;
					} else if (!strcasecmp(var, "jsonrpc-password")) {
						use_passwd = val;
					} else if (!strcasecmp(var, "a1-hash")) {
						use_passwd = val;
						input = switch_mprintf("%s:%s:%s", id, domain, passwd);
						switch_md5_string(a1_hash_buff, (void *) input, strlen(input));
						a1_hash = a1_hash_buff;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"a1-hash-plain = '%s' a1-hash-md5 = '%s'\n", input, a1_hash);
						switch_safe_free(input);
					} else if (!strcasecmp(var, "verto-context")) {
						verto_context = val;
					} else if (!strcasecmp(var, "verto-dialplan")) {
						verto_dialplan = val;
					}

					switch_event_add_header_string(jsock->params, SWITCH_STACK_BOTTOM, var, val);
				}
			}

			if ((x_params = switch_xml_child(x_user, "variables"))) {
				for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					switch_event_add_header_string(jsock->vars, SWITCH_STACK_BOTTOM, var, val);
				}
			}

			if (!zstr(verto_dialplan)) {
				jsock->dialplan = switch_core_strdup(jsock->pool, verto_dialplan);
			}

			if (!zstr(verto_context)) {
				jsock->context = switch_core_strdup(jsock->pool, verto_context);
			}


			if (zstr(use_passwd) || strcmp(a1_hash ? a1_hash : passwd, use_passwd)) {
				r = SWITCH_FALSE;
				*code = CODE_AUTH_FAILED;
				switch_snprintf(message, mlen, "Authentication Failure");
				jsock->uid = NULL;
				login_fire_custom_event(jsock, params, 0, "Authentication Failure");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"auth using %s\n",a1_hash ? "a1-hash" : "username & password");
				r = SWITCH_TRUE;
				check_permissions(jsock, x_user, params);
			}



			switch_xml_free(x_user);
		}

		switch_event_destroy(&req_params);
	}

 end:

	return r;

}

static void set_call_params(cJSON *params, verto_pvt_t *tech_pvt) {
	const char *caller_id_name = NULL;
	const char *caller_id_number = NULL;
	const char *callee_id_name = NULL;
	const char *callee_id_number = NULL;
	const char *prefix = "verto_h_";
	switch_event_header_t *var = NULL;

	caller_id_name = switch_channel_get_variable(tech_pvt->channel, "caller_id_name");
	caller_id_number = switch_channel_get_variable(tech_pvt->channel, "caller_id_number");
	callee_id_name = switch_channel_get_variable(tech_pvt->channel, "callee_id_name");
	callee_id_number = switch_channel_get_variable(tech_pvt->channel, "callee_id_number");

	if (caller_id_name) cJSON_AddItemToObject(params, "caller_id_name", cJSON_CreateString(caller_id_name));
	if (caller_id_number) cJSON_AddItemToObject(params, "caller_id_number", cJSON_CreateString(caller_id_number));

	if (callee_id_name) cJSON_AddItemToObject(params, "callee_id_name", cJSON_CreateString(callee_id_name));
	if (callee_id_number) cJSON_AddItemToObject(params, "callee_id_number", cJSON_CreateString(callee_id_number));

	cJSON_AddItemToObject(params, "display_direction",
						  cJSON_CreateString(switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound"));

	for (var = switch_channel_variable_first(tech_pvt->channel); var; var = var->next) {
		const char *name = (char *) var->name;
		char *value = (char *) var->value;
		if (!strncasecmp(name, prefix, strlen(prefix))) {
			cJSON_AddItemToObject(params, name, cJSON_CreateString(value));
		}
	}
	switch_channel_variable_last(tech_pvt->channel);

}

static jsock_t *get_jsock(const char *uuid)
{
	jsock_t *jsock = NULL;

	switch_mutex_lock(verto_globals.jsock_mutex);
	if ((jsock = switch_core_hash_find(verto_globals.jsock_hash, uuid))) {
		if (switch_thread_rwlock_tryrdlock(jsock->rwlock) != SWITCH_STATUS_SUCCESS) {
			jsock = NULL;
		}
	}
	switch_mutex_unlock(verto_globals.jsock_mutex);

	return jsock;
}

static void attach_jsock(jsock_t *jsock)
{
	jsock_t *jp;
	int proceed = 1;

	switch_mutex_lock(verto_globals.jsock_mutex);

	switch_assert(jsock);

	if ((jp = switch_core_hash_find(verto_globals.jsock_hash, jsock->uuid_str))) {
		if (jp == jsock) {
			proceed = 0;
		} else {
			cJSON *params = NULL;
			cJSON *msg = NULL;
			msg = jrpc_new_req("verto.punt", NULL, &params);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "New connection for session %s dropping previous connection.\n", jsock->uuid_str);
			switch_core_hash_delete(verto_globals.jsock_hash, jsock->uuid_str);
			ws_write_json(jp, &msg, SWITCH_TRUE);
			cJSON_Delete(msg);
			jp->nodelete = 1;
			jp->drop = 1;
		}
	}

	if (proceed) {
		switch_core_hash_insert(verto_globals.jsock_hash, jsock->uuid_str, jsock);
	}

	switch_mutex_unlock(verto_globals.jsock_mutex);
}

static void detach_jsock(jsock_t *jsock)
{
	if (jsock->nodelete) {
		return;
	}

	switch_mutex_lock(verto_globals.jsock_mutex);
	switch_core_hash_delete(verto_globals.jsock_hash, jsock->uuid_str);
	switch_mutex_unlock(verto_globals.jsock_mutex);
}

static int attach_wake(void)
{
	switch_status_t status;
	int tries = 0;

 top:

	status = switch_mutex_trylock(verto_globals.detach_mutex);

	if (status == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(verto_globals.detach_cond);
		switch_mutex_unlock(verto_globals.detach_mutex);
		return 1;
	} else {
		if (switch_mutex_trylock(verto_globals.detach2_mutex) == SWITCH_STATUS_SUCCESS) {
			switch_mutex_unlock(verto_globals.detach2_mutex);
		} else {
			if (++tries < 10) {
				switch_cond_next();
				goto top;
			}
		}
	}

	return 0;
}

static void tech_reattach(verto_pvt_t *tech_pvt, jsock_t *jsock)
{
	cJSON *params = NULL;
	cJSON *msg = NULL;

	tech_pvt->detach_time = 0;
	verto_globals.detached--;
	attach_wake();
	switch_set_flag(tech_pvt, TFLAG_ATTACH_REQ);
	msg = jrpc_new_req("verto.attach", tech_pvt->call_id, &params);

	switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);
	switch_channel_set_flag(tech_pvt->channel, CF_RECOVERING);
	switch_core_media_gen_local_sdp(tech_pvt->session, SDP_TYPE_REQUEST, NULL, 0, NULL, 0);
	switch_channel_clear_flag(tech_pvt->channel, CF_REINVITE);
	switch_channel_clear_flag(tech_pvt->channel, CF_RECOVERING);
	switch_core_session_request_video_refresh(tech_pvt->session);

	cJSON_AddItemToObject(params, "sdp", cJSON_CreateString(tech_pvt->mparams->local_sdp_str));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_DEBUG, "Local attach SDP %s:\n%s\n",
					  switch_channel_get_name(tech_pvt->channel),
					  tech_pvt->mparams->local_sdp_str);
	set_call_params(params, tech_pvt);
 	jsock_queue_event(jsock, &msg, SWITCH_TRUE);
}

static void drop_detached(void)
{
	verto_pvt_t *tech_pvt;
	switch_time_t now = switch_epoch_time_now(NULL);

	switch_thread_rwlock_rdlock(verto_globals.tech_rwlock);
	for(tech_pvt = verto_globals.tech_head; tech_pvt; tech_pvt = tech_pvt->next) {
		if (!switch_channel_up_nosig(tech_pvt->channel)) {
			continue;
		}

		if (tech_pvt->detach_time && (now - tech_pvt->detach_time) > verto_globals.detach_timeout) {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE);
		}
	}
	switch_thread_rwlock_unlock(verto_globals.tech_rwlock);
}

static void attach_calls(jsock_t *jsock)
{
	verto_pvt_t *tech_pvt;
	cJSON *msg = NULL;
	cJSON *params = NULL;
	cJSON *reattached_sessions = NULL;

	reattached_sessions = cJSON_CreateArray();

	switch_thread_rwlock_rdlock(verto_globals.tech_rwlock);
	for(tech_pvt = verto_globals.tech_head; tech_pvt; tech_pvt = tech_pvt->next) {
		if (tech_pvt->detach_time && !strcmp(tech_pvt->jsock_uuid, jsock->uuid_str)) {
			if (!switch_channel_up_nosig(tech_pvt->channel)) {
				continue;
			}

			tech_reattach(tech_pvt, jsock);
			cJSON_AddItemToArray(reattached_sessions, cJSON_CreateString(jsock->uuid_str));
		}
	}
	switch_thread_rwlock_unlock(verto_globals.tech_rwlock);

	msg = jrpc_new_req("verto.clientReady", NULL, &params);
	cJSON_AddItemToObject(params, "reattached_sessions", reattached_sessions);
	jsock_queue_event(jsock, &msg, SWITCH_TRUE);
}

static void detach_calls(jsock_t *jsock)
{
	verto_pvt_t *tech_pvt;
	int wake = 0;

	switch_thread_rwlock_rdlock(verto_globals.tech_rwlock);
	for(tech_pvt = verto_globals.tech_head; tech_pvt; tech_pvt = tech_pvt->next) {
		if (!strcmp(tech_pvt->jsock_uuid, jsock->uuid_str)) {
			if (!switch_channel_up_nosig(tech_pvt->channel)) {
				continue;
			}

			if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				continue;
			}

			if (switch_channel_test_flag(tech_pvt->channel, CF_VIDEO_ONLY)) {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_NORMAL_CLEARING);
				continue;
			}

			switch_core_session_stop_media(tech_pvt->session);
			tech_pvt->detach_time = switch_epoch_time_now(NULL);
			verto_globals.detached++;
			wake = 1;
		}
	}
	switch_thread_rwlock_unlock(verto_globals.tech_rwlock);

	if (wake) attach_wake();
}

static void process_jrpc_response(jsock_t *jsock, cJSON *json)
{
}

static void set_session_id(jsock_t *jsock, const char *uuid)
{
	//cJSON *params, *msg = jrpc_new(0);

	if (!zstr(uuid)) {
		switch_set_string(jsock->uuid_str, uuid);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s re-connecting session %s\n", jsock->name, jsock->uuid_str);
	} else {
		switch_uuid_str(jsock->uuid_str, sizeof(jsock->uuid_str));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s new RPC session %s\n", jsock->name, jsock->uuid_str);
	}

	attach_jsock(jsock);

}

static cJSON *process_jrpc(jsock_t *jsock, cJSON *json)
{
	cJSON *reply = NULL, *echo = NULL, *id = NULL, *params = NULL, *response = NULL, *result;
	const char *method = NULL, *version = NULL, *sessid = NULL;
	jrpc_func_t func = NULL;

	switch_assert(json);

	method = cJSON_GetObjectCstr(json, "method");
	result = cJSON_GetObjectItem(json, "result");
	version = cJSON_GetObjectCstr(json, "jsonrpc");
	id = cJSON_GetObjectItem(json, "id");

	if ((params = cJSON_GetObjectItem(json, "params"))) {
		sessid = cJSON_GetObjectCstr(params, "sessid");
	}

	if (!switch_test_flag(jsock, JPFLAG_INIT)) {
		set_session_id(jsock, sessid);
		switch_set_flag(jsock, JPFLAG_INIT);
	}

	if (zstr(version) || strcmp(version, "2.0")) {
		reply = jrpc_new(0);
		jrpc_add_error(reply, CODE_INVALID, "Invalid message", id);
		goto end;
	}

	if (result) {
		process_jrpc_response(jsock, json);
		return NULL;
	}

	reply = jrpc_new(0);

	jrpc_add_id(reply, id, "", 0);

	if (!switch_test_flag(jsock, JPFLAG_AUTHED) && (jsock->profile->userauth || jsock->profile->root_passwd)) {
		int code = CODE_AUTH_REQUIRED;
		char message[128] = "Authentication Required";

		if (!check_auth(jsock, params, &code, message, sizeof(message))) {
			jrpc_add_error(reply, code, message, id);
			goto end;
		}
		switch_set_flag(jsock, JPFLAG_AUTHED);
	}

	if (!method || !(func = jrpc_get_func(jsock, method))) {
		jrpc_add_error(reply, -32601, "Invalid Method, Missing Method or Permission Denied", id);
	} else {
		if (func(method, params, jsock, &response) == SWITCH_TRUE) {

			if (params) {
				echo = cJSON_GetObjectItem(params, "echoParams");
			}
			if (echo) {
				if ((echo->type == cJSON_True || (echo->type == cJSON_String && switch_true(echo->valuestring)))) {
					cJSON_AddItemToObject(response, "requestParams", cJSON_Duplicate(params, 1));
				} else {
					cJSON_AddItemToObject(response, "requestParams", cJSON_Duplicate(echo, 1));
				}
			}

			jrpc_add_result(reply, response);
		} else {
			if (response) {
				cJSON_AddItemToObject(reply, "error", response);
			} else {
				jrpc_add_error(reply, -32602, "Permission Denied", id);
			}
		}
	}

 end:

	return reply;
}

static switch_status_t process_input(jsock_t *jsock, uint8_t *data, switch_ssize_t bytes)
{
	cJSON *json = NULL, *reply = NULL;
	char *ascii = (char *) data;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (ascii) {
		json = cJSON_Parse(ascii);
	}

	if (json) {

		if (jsock->profile->debug || verto_globals.debug) {
			char *log_text = cJSON_Print(json);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "READ %s [%s]\n", jsock->name, log_text);
			free(log_text);
		}

		if (json->type == cJSON_Array) { /* batch mode */
			int i, len = cJSON_GetArraySize(json);

			reply = cJSON_CreateArray();

			for(i = 0; i < len; i++) {
				cJSON *obj, *item = cJSON_GetArrayItem(json, i);

				if ((obj = process_jrpc(jsock, item))) {
					cJSON_AddItemToArray(reply, obj);
				}
			}
		} else {
			reply = process_jrpc(jsock, json);
		}
	} else {
		reply = jrpc_new(0);
		jrpc_add_error(reply, -32600, "Invalid Request", NULL);
	}

	if (reply) {
		ws_write_json(jsock, &reply, SWITCH_TRUE);
	}

	if (json) {
		cJSON_Delete(json);
	}

	return status;
}

static void jsock_check_event_queue(jsock_t *jsock)
{
	void *pop;
	int this_pass = switch_queue_size(jsock->event_queue);

	switch_mutex_lock(jsock->write_mutex);
	while(this_pass-- > 0 && switch_queue_trypop(jsock->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		cJSON *json = (cJSON *) pop;
		ws_write_json(jsock, &json, SWITCH_TRUE);
	}
	switch_mutex_unlock(jsock->write_mutex);
}

/* DO NOT use this unless you know what you are doing, you are WARNNED!!! */
static uint8_t *http_stream_read(switch_stream_handle_t *handle, int *len)
{
	switch_http_request_t *r = (switch_http_request_t *) handle->data;
	jsock_t *jsock = r->user_data;
	wsh_t *wsh = &jsock->ws;

	if (!jsock->profile->running) {
		*len = 0;
		return NULL;
	}

	*len = (int)(r->bytes_buffered - r->bytes_read);

	if (*len > 0) { // we already read part of the body
		uint8_t *data = (uint8_t *)wsh->buffer + r->bytes_read;
		r->bytes_read = r->bytes_buffered;
		return data;
	}

	if (r->content_length && (r->bytes_read - r->bytes_header) >= r->content_length) {
		*len = 0;
		return NULL;
	}

	*len = (int)(r->content_length - (r->bytes_read - r->bytes_header));
	*len = *len > sizeof(wsh->buffer) ? wsh->buflen : *len;

	if ((*len = (int)ws_raw_read(wsh, wsh->buffer, *len, wsh->block)) < 0) {
		*len = 0;
		return NULL;
	}

	r->bytes_read += *len;

	return (uint8_t *)wsh->buffer;
}

static switch_status_t http_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen)
{
	switch_http_request_t *r = (switch_http_request_t *) handle->data;
	jsock_t *jsock = r->user_data;

	return ws_raw_write(&jsock->ws, data, (uint32_t)datalen) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t http_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	switch_http_request_t *r = (switch_http_request_t *) handle->data;
	jsock_t *jsock = r->user_data;
	int ret = 1;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (data) {
		if (ret) {
			ret =(int) ws_raw_write(&jsock->ws, data, (uint32_t)strlen(data));
		}
		switch_safe_free(data);
	}

	return ret ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static void http_static_handler(switch_http_request_t *request, verto_vhost_t *vhost)
{
	jsock_t *jsock = request->user_data;
	char path[512];
	switch_file_t *fd;
	char *ext;
	uint8_t chunk[4096];
	const char *mime_type = "text/html", *new_type;

	if (strncmp(request->method, "GET", 3) && strncmp(request->method, "HEAD", 4)) {
		char *data = "HTTP/1.1 415 Method Not Allowed\r\n"
			"Content-Length: 0\r\n\r\n";
		ws_raw_write(&jsock->ws, data, strlen(data));
		return;
	}

	switch_snprintf(path, sizeof(path), "%s%s", vhost->root, request->uri);

	if (switch_directory_exists(path, NULL) == SWITCH_STATUS_SUCCESS) {
		switch_snprintf(path, sizeof(path), "%s%s%s%s",
			vhost->root, request->uri, end_of(path) == '/' ? "" : SWITCH_PATH_SEPARATOR, vhost->index);
		// printf("local path: %s\n", path);
	}

	if ((ext = strrchr(path, '.'))) {
		ext++;
		if ((new_type = switch_core_mime_ext2type(ext))) {
			mime_type = new_type;
		}
	}

	if (switch_file_exists(path, NULL) == SWITCH_STATUS_SUCCESS &&
		switch_file_open(&fd, path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, jsock->pool) == SWITCH_STATUS_SUCCESS) {

		switch_size_t flen = switch_file_get_size(fd);

		switch_snprintf((char *)chunk, sizeof(chunk),
			"HTTP/1.1 200 OK\r\n"
			"Date: %s\r\n"
			"Server: FreeSWITCH-%s-mod_verto\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %" SWITCH_SIZE_T_FMT "\r\n\r\n",
			switch_event_get_header(request->headers, "Event-Date-GMT"),
			switch_version_full(),
			mime_type,
			flen);

		ws_raw_write(&jsock->ws, chunk, strlen((char *)chunk));

		for (;;) {
			switch_status_t status;

			flen = sizeof(chunk);
			status = switch_file_read(fd, chunk, &flen);

			if (status != SWITCH_STATUS_SUCCESS || flen == 0) {
				break;
			}

			ws_raw_write(&jsock->ws, chunk, flen);
		}
		switch_file_close(fd);
	} else {
		char *data = "HTTP/1.1 404 Not Found\r\n"
			"Content-Length: 0\r\n\r\n";
		ws_raw_write(&jsock->ws, data, strlen(data));
	}
}

static void http_run(jsock_t *jsock)
{
	switch_http_request_t request = { 0 };
	switch_stream_handle_t stream = { 0 };
	char *err = NULL;
	char *ext;
	verto_vhost_t *vhost;
	switch_bool_t keepalive;

new_req:

	request.user_data = jsock;

	if (switch_event_create(&stream.param_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		goto err;
	}

	request.headers = stream.param_event;
	if (switch_http_parse_header(jsock->ws.buffer, (uint32_t)jsock->ws.datalen, &request) != SWITCH_STATUS_SUCCESS) {
		switch_event_destroy(&stream.param_event);
		goto err;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s [%4" SWITCH_SIZE_T_FMT "] %s\n", jsock->name, jsock->ws.datalen, request.uri);

	if (!strncmp(request.method, "OPTIONS", 7)) {
		char data[512];
		switch_snprintf(data, sizeof(data),
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 0\r\n"
			"Date: %s\r\n"
			"Allow: HEAD,GET,POST,PUT,DELETE,PATCH,OPTIONS\r\n"
			"Server: FreeSWITCH-%s-mod_verto\r\n\r\n",
			switch_event_get_header(request.headers, "Event-Date-GMT"),
			switch_version_full());

		ws_raw_write(&jsock->ws, data, strlen(data));
		goto done;
	}

	if (!strncmp(request.method, "POST", 4) && request.content_length && request.content_type &&
		!strncmp(request.content_type, "application/x-www-form-urlencoded", 33)) {

		char *buffer = NULL;
		switch_ssize_t len = 0, bytes = 0;

		if (request.content_length > 2 * 1024 * 1024 - 1) {
			char *data = "HTTP/1.1 413 Request Entity Too Large\r\n"
				"Content-Length: 0\r\n\r\n";
			ws_raw_write(&jsock->ws, data, strlen(data));
			goto done;
		}

		if (!(buffer = malloc(2 * 1024 * 1024))) {
			goto request_err;
		}

		if ((bytes = request.bytes_buffered - request.bytes_read) > 0) {
			memcpy(buffer, jsock->ws.buffer + request.bytes_read, bytes);
		}

		while(bytes < (switch_ssize_t)request.content_length) {
			len = request.content_length - bytes;

			if ((len = ws_raw_read(&jsock->ws, buffer + bytes, len, jsock->ws.block)) < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read error %" SWITCH_SSIZE_T_FMT"\n", len);
				goto done;
			}

			bytes += len;
		}

		*(buffer + bytes) = '\0';

		switch_http_parse_qs(&request, buffer);
		free(buffer);
	}

	// switch_http_dump_request(&request);

	stream.data = &request;
	stream.read_function = http_stream_read;
	stream.write_function = http_stream_write;
	stream.raw_write_function = http_stream_raw_write;

	switch_event_add_header_string(request.headers, SWITCH_STACK_BOTTOM, "Request-Method", request.method);
	switch_event_add_header_string(request.headers, SWITCH_STACK_BOTTOM, "HTTP-Request-URI", request.uri);

	if (!jsock->profile->vhosts) goto err;

	/* only one vhost supported for now */
	vhost = jsock->profile->vhosts;

	if (!switch_test_flag(jsock, JPFLAG_AUTHED) && vhost->auth_realm) {
		int code = CODE_AUTH_REQUIRED;
		char message[128] = "Authentication Required";
		cJSON *params = NULL;
		char *www_auth;
		char auth_buffer[512];
		char *auth_user = NULL, *auth_pass = NULL;

		www_auth = switch_event_get_header(request.headers, "Authorization");

		if (zstr(www_auth)) {
			switch_snprintf(auth_buffer, sizeof(auth_buffer),
				"HTTP/1.1 401 Authentication Required\r\n"
				"WWW-Authenticate: Basic realm=\"%s\"\r\n"
				"Content-Length: 0\r\n\r\n",
				vhost->auth_realm);
			ws_raw_write(&jsock->ws, auth_buffer, strlen(auth_buffer));
			goto done;
		}

		if (strncasecmp(www_auth, "Basic ", 6)) goto err;

		www_auth += 6;

		switch_b64_decode(www_auth, auth_buffer, sizeof(auth_buffer));

		auth_user = auth_buffer;

		if ((auth_pass = strchr(auth_user, ':'))) {
			*auth_pass++ = '\0';
		}

		if (vhost->auth_user && vhost->auth_pass && auth_pass &&
			!strcmp(vhost->auth_user, auth_user) &&
			!strcmp(vhost->auth_pass, auth_pass)) {
			goto authed;
		}

		if (!(params = cJSON_CreateObject())) {
			goto request_err;
		}

		cJSON_AddItemToObject(params, "login", cJSON_CreateString(auth_user));
		cJSON_AddItemToObject(params, "passwd", cJSON_CreateString(auth_pass));

		if (!check_auth(jsock, params, &code, message, sizeof(message))) {
			switch_snprintf(auth_buffer, sizeof(auth_buffer),
				"HTTP/1.1 401 Authentication Required\r\n"
				"WWW-Authenticate: Basic realm=\"%s\"\r\n"
				"Content-Length: 0\r\n\r\n",
				vhost->auth_realm);
			ws_raw_write(&jsock->ws, auth_buffer, strlen(auth_buffer));
			cJSON_Delete(params);
			goto done;
		} else {
			cJSON_Delete(params);
		}

authed:
		switch_set_flag(jsock, JPFLAG_AUTHED);
		switch_event_add_header_string(request.headers, SWITCH_STACK_BOTTOM, "HTTP-USER", auth_user);
	}

	if (vhost->rewrites) {
		switch_event_header_t *rule = vhost->rewrites->headers;
		switch_regex_t *re = NULL;
		int ovector[30];
		int proceed;

		while(rule) {
			char *expression = rule->name;

			if ((proceed = switch_regex_perform(request.uri, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "%d request [%s] matched expr [%s]\n", proceed, request.uri, expression);
				request.uri = rule->value;
				break;
			}

			rule = rule->next;
		}
	}

	switch_event_add_header_string(request.headers, SWITCH_STACK_BOTTOM, "HTTP-URI", request.uri);

	if ((ext = strrchr(request.uri, '.'))) {
		char path[1024];

		if (!strncmp(ext, ".lua", 4)) {
			switch_snprintf(path, sizeof(path), "%s%s", vhost->script_root, request.uri);
			switch_api_execute("lua", path, NULL, &stream);
		} else {
			http_static_handler(&request, vhost);
		}

	} else {
		http_static_handler(&request, vhost);
	}

done:

	keepalive = request.keepalive;
	switch_http_free_request(&request);

	if (keepalive) {
		wsh_t *wsh = &jsock->ws;

		memset(&request, 0, sizeof(request));
		wsh->datalen = 0;
		*wsh->buffer = '\0';

		while(jsock->profile->running) {
			int pflags;

			if (wsh->ssl && SSL_pending(wsh->ssl) > 0) {
				pflags = SWITCH_POLL_READ;
			} else {
				pflags = switch_wait_sock(jsock->client_socket, 3000, SWITCH_POLL_READ | SWITCH_POLL_ERROR | SWITCH_POLL_HUP);
			}

			if (jsock->drop) { die("%s Dropping Connection\n", jsock->name); }
			if (pflags < 0 && (errno != EINTR)) { die_errnof("%s POLL FAILED with %d", jsock->name, pflags); }
			if (pflags == 0) { /* keepalive socket poll timeout */ break; }
			if (pflags > 0 && (pflags & SWITCH_POLL_HUP)) { log_and_exit(SWITCH_LOG_INFO, "%s POLL HANGUP DETECTED (peer closed its end of socket)\n", jsock->name); }
			if (pflags > 0 && (pflags & SWITCH_POLL_ERROR)) { die("%s POLL ERROR\n", jsock->name); }
			if (pflags > 0 && (pflags & SWITCH_POLL_INVALID)) { die("%s POLL INVALID SOCKET (not opened or already closed)\n", jsock->name); }
			if (pflags > 0 && (pflags & SWITCH_POLL_READ)) {
				ssize_t bytes;

				bytes = ws_raw_read(wsh, wsh->buffer + wsh->datalen, wsh->buflen - wsh->datalen - 1, wsh->block);

				if (bytes < 0) {
					die("%s BAD READ %" SWITCH_SIZE_T_FMT "\n", jsock->name, bytes);
					break;
				}

				if (bytes == 0) {
					bytes = ws_raw_read(wsh, wsh->buffer + wsh->datalen, wsh->buflen - wsh->datalen - 1, wsh->block);

					if (bytes < 0) {
						die("%s BAD READ %" SWITCH_SIZE_T_FMT "\n", jsock->name, bytes);
						break;
					}

					if (bytes == 0) { // socket broken ?
						break;
					}
				}

				wsh->datalen += bytes;
				*(wsh->buffer + wsh->datalen) = '\0';

				if (strstr(wsh->buffer, "\r\n\r\n") || strstr(wsh->buffer, "\n\n")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "socket %s is going to handle a new request\n", jsock->name);
					goto new_req;
				}
			}
		}
	}

	return;

request_err:
	switch_http_free_request(&request);

err:
	err = "HTTP/1.1 500 Internal Server Error\r\n"
		"Content-Length: 0\r\n\r\n";
	ws_raw_write(&jsock->ws, err, strlen(err));

error:
	return;
}

static void client_run(jsock_t *jsock)
{
	if (ws_init(&jsock->ws, jsock->client_socket, (jsock->ptype & PTYPE_CLIENT_SSL) ? jsock->profile->ssl_ctx : NULL, 0, 1, !!jsock->profile->vhosts) < 0) {
		if (jsock->profile->vhosts) {
			http_run(jsock);
			ws_close(&jsock->ws, WS_NONE);
			goto error;
		} else {
			log_and_exit(SWITCH_LOG_NOTICE, "%s WS SETUP FAILED\n", jsock->name);
		}
	}

	while(jsock->profile->running) {
		int pflags;

		if (jsock->ws.ssl && SSL_pending(jsock->ws.ssl) > 0) {
			pflags = SWITCH_POLL_READ;
		} else {
			pflags = switch_wait_sock(jsock->client_socket, 50, SWITCH_POLL_READ | SWITCH_POLL_ERROR | SWITCH_POLL_HUP);
		}

		if (jsock->drop) { die("%s Dropping Connection\n", jsock->name); }
		if (pflags < 0 && (errno != EINTR)) { die_errnof("%s POLL FAILED with %d", jsock->name, pflags); }
		if (pflags == 0) {/* socket poll timeout */ jsock_check_event_queue(jsock); }
		if (pflags > 0 && (pflags & SWITCH_POLL_HUP)) { log_and_exit(SWITCH_LOG_INFO, "%s POLL HANGUP DETECTED (peer closed its end of socket)\n", jsock->name); }
		if (pflags > 0 && (pflags & SWITCH_POLL_ERROR)) { die("%s POLL ERROR\n", jsock->name); }
		if (pflags > 0 && (pflags & SWITCH_POLL_INVALID)) { die("%s POLL INVALID SOCKET (not opened or already closed)\n", jsock->name); }
		if (pflags > 0 && (pflags & SWITCH_POLL_READ)) {
			switch_ssize_t bytes;
			ws_opcode_t oc;
			uint8_t *data;

			bytes = ws_read_frame(&jsock->ws, &oc, &data);

			if (bytes < 0) {
				if (bytes == -WS_RECV_CLOSE) {
					log_and_exit(SWITCH_LOG_INFO, "%s Client sent close request\n", jsock->name);
				} else {
					die("%s BAD READ %" SWITCH_SSIZE_T_FMT "\n", jsock->name, bytes);
				}
			}

			if (bytes) {
				char *s = (char *) data;

				if (*s == '#') {
					char repl[2048] = "";
					switch_time_t a, b;

					if (s[1] == 'S' && s[2] == 'P') {

						if (s[3] == 'U') {
							int i, size = 0;
							char *p = s+4;
							int loops = 0;
							int rem = 0;
							int dur = 0, j = 0;

							if ((size = atoi(p)) <= 0) {
								continue;
							}

							a = switch_time_now();
							do {
								bytes = ws_read_frame(&jsock->ws, &oc, &data);
								s = (char *) data;
							} while (bytes && data && s[0] == '#' && s[3] == 'B');
							b = switch_time_now();

							if (!bytes || !data) continue;

							if (s[0] != '#') goto nm;

							switch_snprintf(repl, sizeof(repl), "#SPU %ld", (long)((b - a) / 1000));
							ws_write_frame(&jsock->ws, WSOC_TEXT, repl, strlen(repl));
							loops = size / 1024;
							rem = size % 1024;
							switch_snprintf(repl, sizeof(repl), "#SPB ");
							memset(repl+4, '.', 1024);

							for (j = 0; j < 10 ; j++) {
								int ddur = 0;
								a = switch_time_now();
								for (i = 0; i < loops; i++) {
									ws_write_frame(&jsock->ws, WSOC_TEXT, repl, 1024);
								}
								if (rem) {
									ws_write_frame(&jsock->ws, WSOC_TEXT, repl, rem);
								}
								b = switch_time_now();
								ddur += (int)((b - a) / 1000);
								dur += ddur;

							}

							dur /= j+1;

							switch_snprintf(repl, sizeof(repl), "#SPD %d", dur);
							ws_write_frame(&jsock->ws, WSOC_TEXT, repl, strlen(repl));
						}
					}

					continue;
				}

			nm:

				if (process_input(jsock, data, bytes) != SWITCH_STATUS_SUCCESS) {
					die("%s Input Error\n", jsock->name);
				}

				if (!switch_test_flag(jsock, JPFLAG_CHECK_ATTACH) && switch_test_flag(jsock, JPFLAG_AUTHED)) {
					attach_calls(jsock);
					switch_set_flag(jsock, JPFLAG_CHECK_ATTACH);
				}
			}
		}
	}

 error:

	detach_jsock(jsock);
	ws_destroy(&jsock->ws);

	return;
}

static void jsock_flush(jsock_t *jsock)
{
	void *pop;

	switch_mutex_lock(jsock->write_mutex);
	while(switch_queue_trypop(jsock->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		cJSON *json = (cJSON *) pop;
		cJSON_Delete(json);
	}
	switch_mutex_unlock(jsock->write_mutex);
}

static void *SWITCH_THREAD_FUNC client_thread(switch_thread_t *thread, void *obj)
{
	switch_event_t *s_event;

	jsock_t *jsock = (jsock_t *) obj;

	switch_event_create(&jsock->params, SWITCH_EVENT_CHANNEL_DATA);
	switch_event_create(&jsock->vars, SWITCH_EVENT_CHANNEL_DATA);
	switch_event_create(&jsock->user_vars, SWITCH_EVENT_CHANNEL_DATA);


	add_jsock(jsock);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Starting client thread.\n", jsock->name);

	if ((jsock->ptype & PTYPE_CLIENT) || (jsock->ptype & PTYPE_CLIENT_SSL)) {
		client_run(jsock);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s Ending client thread.\n", jsock->name);
	}

	detach_calls(jsock);

	del_jsock(jsock);

	switch_event_destroy(&jsock->params);
	switch_event_destroy(&jsock->vars);
	switch_event_destroy(&jsock->user_vars);

	if (jsock->client_socket != ws_sock_invalid) {
		close_socket(&jsock->client_socket);
	}

	switch_event_destroy(&jsock->allowed_methods);
	switch_event_destroy(&jsock->allowed_fsapi);
	switch_event_destroy(&jsock->allowed_jsapi);
	switch_event_destroy(&jsock->allowed_event_channels);

	jsock_flush(jsock);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Ending client thread.\n", jsock->name);
	if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_CLIENT_DISCONNECT) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_profile_name", jsock->profile->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_client_address", jsock->name);
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_login", switch_str_nil(jsock->uid));
		switch_event_fire(&s_event);
	}
	switch_thread_rwlock_wrlock(jsock->rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Thread ended\n", jsock->name);
	switch_thread_rwlock_unlock(jsock->rwlock);

	return NULL;
}


static switch_bool_t auth_api_command(jsock_t *jsock, const char *api_cmd, const char *arg)
{
	const char *check_cmd = api_cmd;
	char *sneaky_commands[] = { "bgapi", "sched_api", "eval", "expand", "xml_wrap", NULL };
	int x = 0;
	char *dup_arg = NULL;
	char *next = NULL;
	switch_bool_t ok = SWITCH_TRUE;

  top:

	if (!jsock->allowed_fsapi) {
		ok = SWITCH_FALSE;
		goto end;
	}

	if (!switch_event_get_header(jsock->allowed_fsapi, check_cmd)) {
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
						switch_assert(dup_arg);
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

//// VERTO

static void track_pvt(verto_pvt_t *tech_pvt)
{
	switch_thread_rwlock_wrlock(verto_globals.tech_rwlock);
	tech_pvt->next = verto_globals.tech_head;
	verto_globals.tech_head = tech_pvt;
	switch_set_flag(tech_pvt, TFLAG_TRACKED);
	switch_thread_rwlock_unlock(verto_globals.tech_rwlock);
}

static void untrack_pvt(verto_pvt_t *tech_pvt)
{
	verto_pvt_t *p, *last = NULL;
	int wake = 0;
	
	switch_thread_rwlock_wrlock(verto_globals.tech_rwlock);

	if (tech_pvt->detach_time) {
		verto_globals.detached--;
		tech_pvt->detach_time = 0;
		wake = 1;
	}

	if (switch_test_flag(tech_pvt, TFLAG_TRACKED)) {
		switch_clear_flag(tech_pvt, TFLAG_TRACKED);
		for(p = verto_globals.tech_head; p; p = p->next) {
			if (p == tech_pvt) {
				if (last) {
					last->next = p->next;
				} else {
					verto_globals.tech_head = p->next;
				}
				break;
			}

			last = p;
		}
	}
		
	switch_thread_rwlock_unlock(verto_globals.tech_rwlock);

	if (wake) attach_wake();
}

switch_endpoint_interface_t *verto_endpoint_interface = NULL;

static switch_status_t verto_on_destroy(switch_core_session_t *session)
{
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	switch_buffer_destroy(&tech_pvt->text_read_buffer);
	switch_buffer_destroy(&tech_pvt->text_write_buffer);

	UNPROTECT_INTERFACE(verto_endpoint_interface);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t verto_on_hangup(switch_core_session_t *session)
{
	jsock_t *jsock = NULL;
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	untrack_pvt(tech_pvt);

	// get the jsock and send hangup notice
	if (!tech_pvt->remote_hangup_cause && (jsock = get_jsock(tech_pvt->jsock_uuid))) {
		cJSON *params = NULL;
		cJSON *msg = jrpc_new_req("verto.bye", tech_pvt->call_id, &params);
		switch_call_cause_t cause = switch_channel_get_cause(tech_pvt->channel);
		switch_channel_set_variable(tech_pvt->channel, "verto_hangup_disposition", "send_bye");

		cJSON_AddItemToObject(params, "causeCode", cJSON_CreateNumber(cause));
		cJSON_AddItemToObject(params, "cause", cJSON_CreateString(switch_channel_cause2str(cause)));
		jsock_queue_event(jsock, &msg, SWITCH_TRUE);

		switch_thread_rwlock_unlock(jsock->rwlock);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t verto_set_media_options(verto_pvt_t *tech_pvt, verto_profile_t *profile);

static switch_status_t verto_connect(switch_core_session_t *session, const char *method)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    jsock_t *jsock = NULL;
    verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

    if (!(jsock = get_jsock(tech_pvt->jsock_uuid))) {
        status = SWITCH_STATUS_BREAK;
    } else {
        cJSON *params = NULL;
        cJSON *msg = NULL;
		const char *var = NULL;
		switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(tech_pvt->channel);
		switch_event_header_t *hp;

		//DUMP_EVENT(jsock->params);

		switch_channel_set_variable(tech_pvt->channel, "verto_user", jsock->uid);
		switch_channel_set_variable(tech_pvt->channel, "presence_id", jsock->uid);
		switch_channel_set_variable(tech_pvt->channel, "verto_client_address", jsock->name);
		switch_channel_set_variable(tech_pvt->channel, "chat_proto", VERTO_CHAT_PROTO);
		switch_channel_set_variable(tech_pvt->channel, "verto_host", jsock->domain);

		for (hp = jsock->user_vars->headers; hp; hp = hp->next) {
			switch_channel_set_variable(tech_pvt->channel, hp->name, hp->value);
		}

		if ((var = switch_event_get_header(jsock->params, "caller-id-name"))) {
			caller_profile->callee_id_name = switch_core_strdup(caller_profile->pool, var);
		}

		if ((var = switch_event_get_header(jsock->params, "caller-id-number"))) {
			caller_profile->callee_id_number = switch_core_strdup(caller_profile->pool, var);
		}

		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
			switch_core_media_absorb_sdp(session);
		} else {
			switch_channel_set_variable(tech_pvt->channel, "media_webrtc", "true");
			switch_core_session_set_ice(tech_pvt->session);

			if (verto_set_media_options(tech_pvt, jsock->profile) != SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_FALSE;
				switch_thread_rwlock_unlock(jsock->rwlock);
				return status;
			}


			switch_channel_set_variable(tech_pvt->channel, "verto_profile_name", jsock->profile->name);

			if (!switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
				switch_channel_set_variable(tech_pvt->channel, "codec_string", NULL);
				switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);

				if ((status = switch_core_media_choose_ports(tech_pvt->session, SWITCH_TRUE, SWITCH_TRUE)) != SWITCH_STATUS_SUCCESS) {
					//if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(jsock->rwlock);
					return status;
				}
			}

			switch_core_media_gen_local_sdp(session, SDP_TYPE_REQUEST, NULL, 0, NULL, 0);
		}

        msg = jrpc_new_req(method, tech_pvt->call_id, &params);

        if (tech_pvt->mparams->local_sdp_str) {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Local %s SDP %s:\n%s\n",
							  method,
							  switch_channel_get_name(tech_pvt->channel),
                              tech_pvt->mparams->local_sdp_str);

            cJSON_AddItemToObject(params, "sdp", cJSON_CreateString(tech_pvt->mparams->local_sdp_str));
			set_call_params(params, tech_pvt);

            jsock_queue_event(jsock, &msg, SWITCH_TRUE);
        } else {
            status = SWITCH_STATUS_FALSE;
        }

        switch_thread_rwlock_unlock(jsock->rwlock);
    }

    return status;
}

switch_status_t verto_tech_media(verto_pvt_t *tech_pvt, const char *r_sdp, switch_sdp_type_t sdp_type)
{
	uint8_t match = 0, p = 0;

	switch_assert(tech_pvt != NULL);
	switch_assert(r_sdp != NULL);

	if (zstr(r_sdp)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((match = switch_core_media_negotiate_sdp(tech_pvt->session, r_sdp, &p, sdp_type))) {
		if (switch_core_media_choose_ports(tech_pvt->session, SWITCH_TRUE, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		//if (switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}

		if (switch_core_media_activate_rtp(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
		//if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
		//	switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "EARLY MEDIA");
		//		switch_channel_mark_pre_answered(tech_pvt->channel);
		//}
		return SWITCH_STATUS_SUCCESS;
	}


	return SWITCH_STATUS_FALSE;
}

static switch_status_t verto_on_init(switch_core_session_t *session)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	if (switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING_BRIDGE) || switch_channel_test_flag(tech_pvt->channel, CF_RECOVERING)) {
		int tries = 120;

		switch_core_session_clear_crypto(session);

		while(--tries > 0) {

			status = verto_connect(session, "verto.attach");

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_set_flag(tech_pvt, TFLAG_ATTACH_REQ);
				break;
			} else if (status == SWITCH_STATUS_BREAK) {
				switch_yield(1000000);
				continue;
			} else {
				tries = 0;
				break;
			}
		}

		if (!tries) {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			status = SWITCH_STATUS_FALSE;
		}

		switch_channel_set_flag(tech_pvt->channel, CF_VIDEO_BREAK);
        switch_core_session_kill_channel(tech_pvt->session, SWITCH_SIG_BREAK);

		tries = 500;
		while(--tries > 0 && switch_test_flag(tech_pvt, TFLAG_ATTACH_REQ)) {
			switch_yield(10000);
		}

		switch_core_session_request_video_refresh(session);
		switch_channel_set_flag(tech_pvt->channel, CF_VIDEO_BREAK);
        switch_core_session_kill_channel(tech_pvt->session, SWITCH_SIG_BREAK);

		goto end;
	}

	if (switch_channel_direction(tech_pvt->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		if ((status = verto_connect(tech_pvt->session, "verto.invite")) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		} else {
			switch_channel_mark_ring_ready(tech_pvt->channel);
		}
	}

 end:

	if (status == SWITCH_STATUS_SUCCESS) {
		track_pvt(tech_pvt);
	}
	
	return status;
}


static switch_state_handler_table_t verto_state_handlers = {
	/*.on_init */ verto_on_init,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ verto_on_hangup,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ NULL,
    /*.on_destroy */ verto_on_destroy,
    SSH_FLAG_STICKY
};




static switch_status_t verto_set_media_options(verto_pvt_t *tech_pvt, verto_profile_t *profile)
{
	uint32_t i;


	switch_mutex_lock(profile->mutex);
	if (!zstr(profile->rtpip[profile->rtpip_cur])) {
		tech_pvt->mparams->rtpip4 = switch_core_session_strdup(tech_pvt->session, profile->rtpip[profile->rtpip_cur++]);
		tech_pvt->mparams->rtpip = tech_pvt->mparams->rtpip4;
		if (profile->rtpip_cur == profile->rtpip_index) {
			profile->rtpip_cur = 0;
		}
	}

	if (!zstr(profile->rtpip6[profile->rtpip_cur6])) {
		tech_pvt->mparams->rtpip6 = switch_core_session_strdup(tech_pvt->session, profile->rtpip6[profile->rtpip_cur6++]);

		if (zstr(tech_pvt->mparams->rtpip)) {
			tech_pvt->mparams->rtpip = tech_pvt->mparams->rtpip6;
		}

		if (profile->rtpip_cur6 == profile->rtpip_index6) {
			profile->rtpip_cur6 = 0;
		}
	}
	switch_mutex_unlock(profile->mutex);

	if (zstr(tech_pvt->mparams->rtpip)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(tech_pvt->session), SWITCH_LOG_ERROR, "%s has no media ip, check your configuration\n",
						  switch_channel_get_name(tech_pvt->channel));
		//switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL);
		return SWITCH_STATUS_FALSE;
	}

	tech_pvt->mparams->extrtpip = tech_pvt->mparams->extsipip = profile->extrtpip;

	//tech_pvt->mparams->dtmf_type = tech_pvt->profile->dtmf_type;
	switch_channel_set_flag(tech_pvt->channel, CF_TRACKABLE);
	switch_channel_set_variable(tech_pvt->channel, "secondary_recovery_module", modname);

	switch_core_media_check_dtmf_type(tech_pvt->session);

	//switch_channel_set_cap(tech_pvt->channel, CC_MEDIA_ACK);
	switch_channel_set_cap(tech_pvt->channel, CC_BYPASS_MEDIA);
	//switch_channel_set_cap(tech_pvt->channel, CC_PROXY_MEDIA);
	switch_channel_set_cap(tech_pvt->channel, CC_JITTERBUFFER);
	switch_channel_set_cap(tech_pvt->channel, CC_FS_RTP);

	//switch_channel_set_cap(tech_pvt->channel, CC_QUEUEABLE_DTMF_DELAY);
	//tech_pvt->mparams->ndlb = tech_pvt->profile->mndlb;

	tech_pvt->mparams->inbound_codec_string = switch_core_session_strdup(tech_pvt->session, profile->inbound_codec_string);
	tech_pvt->mparams->outbound_codec_string = switch_core_session_strdup(tech_pvt->session, profile->outbound_codec_string);

	tech_pvt->mparams->jb_msec = profile->jb_msec;
	switch_media_handle_set_media_flag(tech_pvt->smh, SCMF_SUPPRESS_CNG);

	//tech_pvt->mparams->auto_rtp_bugs = profile->auto_rtp_bugs;
	tech_pvt->mparams->timer_name =  profile->timer_name;
	//tech_pvt->mparams->vflags = profile->vflags;
	//tech_pvt->mparams->manual_rtp_bugs = profile->manual_rtp_bugs;
	//tech_pvt->mparams->manual_video_rtp_bugs = profile->manual_video_rtp_bugs;

	tech_pvt->mparams->local_network = switch_core_session_strdup(tech_pvt->session, profile->local_network);


	//tech_pvt->mparams->rtcp_audio_interval_msec = profile->rtpp_audio_interval_msec;
	//tech_pvt->mparams->rtcp_video_interval_msec = profile->rtpp_video_interval_msec;
	//tech_pvt->mparams->sdp_username = profile->sdp_username;
	//tech_pvt->mparams->cng_pt = tech_pvt->cng_pt;
	//tech_pvt->mparams->rtc_timeout_sec = profile->rtp_timeout_sec;
	//tech_pvt->mparams->rtc_hold_timeout_sec = profile->rtp_hold_timeout_sec;
	//switch_media_handle_set_media_flags(tech_pvt->media_handle, tech_pvt->profile->media_flags);


	for(i = 0; i < profile->cand_acl_count; i++) {
		switch_core_media_add_ice_acl(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, profile->cand_acl[i]);
		switch_core_media_add_ice_acl(tech_pvt->session, SWITCH_MEDIA_TYPE_VIDEO, profile->cand_acl[i]);
	}

	if (profile->enable_text && !tech_pvt->text_read_buffer) {
		set_text_funcs(tech_pvt->session);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t verto_media(switch_core_session_t *session)
{
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_core_media_prepare_codecs(tech_pvt->session, SWITCH_TRUE);

	if (tech_pvt->r_sdp) {
		if (verto_tech_media(tech_pvt, tech_pvt->r_sdp, SDP_TYPE_REQUEST) != SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
			return SWITCH_STATUS_FALSE;
		}
	}

	if ((status = switch_core_media_choose_ports(tech_pvt->session, SWITCH_TRUE, SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
		//if ((status = switch_core_media_choose_port(tech_pvt->session, SWITCH_MEDIA_TYPE_AUDIO, 0)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return status;
	}

	switch_core_media_gen_local_sdp(session, SDP_TYPE_RESPONSE, NULL, 0, NULL, 0);

	if (switch_core_media_activate_rtp(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

	if (tech_pvt->mparams->local_sdp_str) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Local SDP %s:\n%s\n", switch_channel_get_name(tech_pvt->channel),
						  tech_pvt->mparams->local_sdp_str);
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}


static switch_status_t verto_send_media_indication(switch_core_session_t *session, const char *method)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
	const char *proxy_sdp = NULL;

	if (switch_test_flag(tech_pvt, TFLAG_SENT_MEDIA)) {
		status = SWITCH_STATUS_SUCCESS;
	}

	if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
		if ((proxy_sdp = switch_channel_get_variable(tech_pvt->channel, SWITCH_B_SDP_VARIABLE))) {
			status = SWITCH_STATUS_SUCCESS;
			switch_core_media_set_local_sdp(session, proxy_sdp, SWITCH_TRUE);
		}
	}


	if (status == SWITCH_STATUS_SUCCESS || (status = verto_media(session)) == SWITCH_STATUS_SUCCESS) {
		jsock_t *jsock = NULL;

		if (!(jsock = get_jsock(tech_pvt->jsock_uuid))) {
			status = SWITCH_STATUS_FALSE;
		} else {
			cJSON *params = NULL;
			cJSON *msg = jrpc_new_req(method, tech_pvt->call_id, &params);
			if (!switch_test_flag(tech_pvt, TFLAG_SENT_MEDIA)) {
				cJSON_AddItemToObject(params, "sdp", cJSON_CreateString(tech_pvt->mparams->local_sdp_str));
			}

			switch_set_flag(tech_pvt, TFLAG_SENT_MEDIA);

			if (jsock_queue_event(jsock, &msg, SWITCH_TRUE) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

			switch_thread_rwlock_unlock(jsock->rwlock);
		}
	}

	return status;
}

static switch_status_t messagehook (switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_status_t r = SWITCH_STATUS_SUCCESS;
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	switch(msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		{
			const char *name, *number;
			cJSON *jmsg = NULL, *params = NULL;
			jsock_t *jsock = NULL;

			if ((jsock = get_jsock(tech_pvt->jsock_uuid))) {
				name = msg->string_array_arg[0];
				number = msg->string_array_arg[1];

				if (name || number) {
					jmsg = jrpc_new_req("verto.display", tech_pvt->call_id, &params);
					switch_ivr_eavesdrop_update_display(session, name, number);
					switch_channel_set_variable(tech_pvt->channel, "last_sent_display_name", name);
					switch_channel_set_variable(tech_pvt->channel, "last_sent_display_number", number);
					cJSON_AddItemToObject(params, "display_name", cJSON_CreateString(name));
					cJSON_AddItemToObject(params, "display_number", cJSON_CreateString(number));
					set_call_params(params, tech_pvt);
					jsock_queue_event(jsock, &jmsg, SWITCH_TRUE);
				}

				switch_thread_rwlock_unlock(jsock->rwlock);
			}

		}
		break;
	case SWITCH_MESSAGE_INDICATE_MEDIA_RENEG:
		{
			jsock_t *jsock = NULL;

			if ((jsock = get_jsock(tech_pvt->jsock_uuid))) {
				switch_core_session_stop_media(session);
				detach_calls(jsock);
				tech_reattach(tech_pvt, jsock);
				switch_thread_rwlock_unlock(jsock->rwlock);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_ANSWER:
		r = verto_send_media_indication(session, "verto.answer");
		break;
	case SWITCH_MESSAGE_INDICATE_PROGRESS:
		r = verto_send_media_indication(session, "verto.media");
		break;
	default:
		break;
	}

	return r;
}



static int verto_recover_callback(switch_core_session_t *session)
{
	int r = 0;
	char name[512];
	verto_pvt_t *tech_pvt = NULL;
	verto_profile_t *profile = NULL;
	const char *profile_name = NULL, *jsock_uuid_str = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_channel_test_flag(channel, CF_VIDEO_ONLY)) {
		return 0;
	}

	PROTECT_INTERFACE(verto_endpoint_interface);

	profile_name = switch_channel_get_variable(channel, "verto_profile_name");
	jsock_uuid_str = switch_channel_get_variable(channel, "jsock_uuid_str");

	if (!(profile_name && jsock_uuid_str && (profile = find_profile(profile_name)))) {
		UNPROTECT_INTERFACE(verto_endpoint_interface);
		return 0;
	}

	tech_pvt = switch_core_session_alloc(session, sizeof(*tech_pvt));
	tech_pvt->pool = switch_core_session_get_pool(session);
	tech_pvt->session = session;
	tech_pvt->channel = channel;
	tech_pvt->jsock_uuid = (char *) jsock_uuid_str;
	switch_core_session_set_private_class(session, tech_pvt, SWITCH_PVT_SECONDARY);


	tech_pvt->call_id = switch_core_session_strdup(session, switch_core_session_get_uuid(session));

	switch_snprintf(name, sizeof(name), "verto.rtc/%s", tech_pvt->jsock_uuid);
	switch_channel_set_name(channel, name);

	if ((tech_pvt->smh = switch_core_session_get_media_handle(session))) {
		tech_pvt->mparams = switch_core_media_get_mparams(tech_pvt->smh);
		if (verto_set_media_options(tech_pvt, profile) != SWITCH_STATUS_SUCCESS) {
			UNPROTECT_INTERFACE(verto_endpoint_interface);
			return 0;
		}
	}

	switch_channel_add_state_handler(channel, &verto_state_handlers);
	switch_core_event_hook_add_receive_message(session, messagehook);

	//track_pvt(tech_pvt);

	//switch_channel_clear_flag(tech_pvt->channel, CF_ANSWERED);
	//switch_channel_clear_flag(tech_pvt->channel, CF_EARLY_MEDIA);

	switch_thread_rwlock_unlock(profile->rwlock);

	r++;

	return r;
}


static void pass_sdp(verto_pvt_t *tech_pvt)
{
	switch_core_session_t *other_session = NULL;
	switch_channel_t *other_channel = NULL;

	if (switch_core_session_get_partner(tech_pvt->session, &other_session) == SWITCH_STATUS_SUCCESS) {
		other_channel = switch_core_session_get_channel(other_session);
		switch_channel_pass_sdp(tech_pvt->channel, other_channel, tech_pvt->r_sdp);

		switch_channel_set_flag(other_channel, CF_PROXY_MODE);
		switch_core_session_queue_indication(other_session, SWITCH_MESSAGE_INDICATE_ANSWER);
		switch_core_session_rwunlock(other_session);
	}
}


//// METHODS

#define switch_either(_A, _B) zstr(_A) ? _B : _A

static switch_bool_t verto__answer_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *obj = cJSON_CreateObject();
	switch_core_session_t *session;
	cJSON *dialog = NULL;
	const char *call_id = NULL, *sdp = NULL;
	int err = 0;
	const char *callee_id_name = NULL, *callee_id_number = NULL;

	*response = obj;

	if (!params) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if (!(dialog = cJSON_GetObjectItem(params, "dialogParams"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Dialog data missing"));
		err = 1; goto cleanup;
	}

	if (!(call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CallID missing"));
		err = 1; goto cleanup;
	}

	if (!(sdp = cJSON_GetObjectCstr(params, "sdp"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("SDP missing"));
		err = 1; goto cleanup;
	}

	callee_id_name = cJSON_GetObjectCstr(dialog, "callee_id_name");
	callee_id_number = cJSON_GetObjectCstr(dialog, "callee_id_number");


	if ((session = switch_core_session_locate(call_id))) {
		verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
		switch_core_session_t *other_session = NULL;

		tech_pvt->r_sdp = switch_core_session_strdup(session, sdp);
		switch_channel_set_variable(tech_pvt->channel, SWITCH_R_SDP_VARIABLE, sdp);
		switch_channel_set_variable(tech_pvt->channel, "verto_client_address", jsock->name);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote SDP %s:\n%s\n", switch_channel_get_name(tech_pvt->channel), sdp);
		switch_core_media_set_sdp_codec_string(session, sdp, SDP_TYPE_RESPONSE);

		switch_ivr_set_user(session, jsock->uid);

		if (switch_core_session_get_partner(tech_pvt->session, &other_session) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
			switch_channel_set_variable(other_channel, SWITCH_B_SDP_VARIABLE, sdp);
			switch_core_session_rwunlock(other_session);
		}

		if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
			pass_sdp(tech_pvt);
		} else {
			if (verto_tech_media(tech_pvt, tech_pvt->r_sdp, SDP_TYPE_RESPONSE) != SWITCH_STATUS_SUCCESS) {
				switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CODEC ERROR"));
				err = 1;
			}

			if (!err && switch_core_media_activate_rtp(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("MEDIA ERROR"));
				err = 1;
			}
		}

		if (!err) {
			if (callee_id_name) {
				switch_channel_set_profile_var(tech_pvt->channel, "callee_id_name", callee_id_name);
			}
			if (callee_id_number) {
				switch_channel_set_profile_var(tech_pvt->channel, "callee_id_number", callee_id_number);
			}
			switch_channel_mark_answered(tech_pvt->channel);
		}

		switch_core_session_rwunlock(session);
	} else {
		err = 1;
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL DOES NOT EXIST"));
	}

 cleanup:


	if (!err) return SWITCH_TRUE;


	cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));


	return SWITCH_FALSE;

}

static switch_bool_t verto__bye_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *obj = cJSON_CreateObject(), *causeObj = NULL;
	switch_core_session_t *session;
	cJSON *dialog = NULL;
	const char *call_id = NULL, *cause_str = NULL;
	int err = 0, got_cause = 0;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	*response = obj;

	if (!params) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if (!(dialog = cJSON_GetObjectItem(params, "dialogParams"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Dialog data missing"));
		err = 1; goto cleanup;
	}

	if (!(call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CallID missing"));
		err = 1; goto cleanup;
	}

	if ((cause_str = cJSON_GetObjectCstr(params, "cause"))) {
		switch_call_cause_t check = switch_channel_str2cause(cause_str);

		if (check != SWITCH_CAUSE_NONE) {
			cause = check;
			got_cause = 1;
		}
	} 

	if (!got_cause && (causeObj = cJSON_GetObjectItem(params, "causeCode"))) {
		int check = 0;
		const char *cause_str = NULL;

		if (!zstr(causeObj->valuestring)) {
			check = atoi(causeObj->valuestring);
		} else if (causeObj->valueint) {
			check = causeObj->valueint;
		}

		cause_str = switch_channel_cause2str((switch_call_cause_t)check);

		if (!zstr(cause_str) && strcasecmp(cause_str, "unknown")) {
			cause = (switch_call_cause_t) check;
		}
	}

	cJSON_AddItemToObject(obj, "callID", cJSON_CreateString(call_id));

	if ((session = switch_core_session_locate(call_id))) {
		verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
		tech_pvt->remote_hangup_cause = cause;
		switch_channel_set_variable(tech_pvt->channel, "verto_hangup_disposition", "recv_bye");
		switch_channel_hangup(tech_pvt->channel, cause);

		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL ENDED"));
		cJSON_AddItemToObject(obj, "causeCode", cJSON_CreateNumber(cause));
		cJSON_AddItemToObject(obj, "cause", cJSON_CreateString(switch_channel_cause2str(cause)));
		switch_core_session_rwunlock(session);
	} else {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL DOES NOT EXIST"));
		err = 1;
	}

 cleanup:


	if (!err) return SWITCH_TRUE;


	cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));


	return SWITCH_FALSE;
}

static switch_status_t xfer_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state == CS_HANGUP) {
		switch_core_session_t *ksession;
		const char *uuid = switch_channel_get_variable(channel, "att_xfer_kill_uuid");

		if (uuid && (ksession = switch_core_session_force_locate(uuid))) {
			switch_channel_t *kchannel = switch_core_session_get_channel(ksession);

			switch_channel_clear_flag(kchannel, CF_XFER_ZOMBIE);
			switch_channel_clear_flag(kchannel, CF_TRANSFER);
			if (switch_channel_up(kchannel)) {
				switch_channel_hangup(kchannel, SWITCH_CAUSE_NORMAL_CLEARING);
			}

			switch_core_session_rwunlock(ksession);
		}

		switch_core_event_hook_remove_state_change(session, xfer_hanguphook);

	}

	return SWITCH_STATUS_SUCCESS;
}

static void mark_transfer_record(switch_core_session_t *session, const char *br_a, const char *br_b)
{
	switch_core_session_t *br_b_session, *br_a_session;
	switch_channel_t *channel;
	const char *uvar1, *dvar1, *uvar2, *dvar2;

	channel = switch_core_session_get_channel(session);

	uvar1 = "verto_user";
	dvar1 = "verto_host";

	if ((br_b_session = switch_core_session_locate(br_b)) ) {
		switch_channel_t *br_b_channel = switch_core_session_get_channel(br_b_session);
		switch_caller_profile_t *cp = switch_channel_get_caller_profile(br_b_channel);

		if (switch_channel_direction(br_b_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			uvar2 = "sip_from_user";
			dvar2 = "sip_from_host";
		} else {
			uvar2 = "sip_to_user";
			dvar2 = "sip_to_host";
		}

		cp->transfer_source = switch_core_sprintf(cp->pool,
												  "%ld:%s:att_xfer:%s@%s/%s@%s",
												  (long) switch_epoch_time_now(NULL),
												  cp->uuid_str,
												  switch_channel_get_variable(channel, uvar1),
												  switch_channel_get_variable(channel, dvar1),
												  switch_channel_get_variable(br_b_channel, uvar2),
												  switch_channel_get_variable(br_b_channel, dvar2));

		switch_channel_add_variable_var_check(br_b_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
		switch_channel_set_variable(br_b_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, cp->transfer_source);

		switch_core_session_rwunlock(br_b_session);
	}



	if ((br_a_session = switch_core_session_locate(br_a)) ) {
		switch_channel_t *br_a_channel = switch_core_session_get_channel(br_a_session);
		switch_caller_profile_t *cp = switch_channel_get_caller_profile(br_a_channel);

		if (switch_channel_direction(br_a_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			uvar2 = "sip_from_user";
			dvar2 = "sip_from_host";
		} else {
			uvar2 = "sip_to_user";
			dvar2 = "sip_to_host";
		}

		cp->transfer_source = switch_core_sprintf(cp->pool,
												  "%ld:%s:att_xfer:%s@%s/%s@%s",
												  (long) switch_epoch_time_now(NULL),
												  cp->uuid_str,
												  switch_channel_get_variable(channel, uvar1),
												  switch_channel_get_variable(channel, dvar1),
												  switch_channel_get_variable(br_a_channel, uvar2),
												  switch_channel_get_variable(br_a_channel, dvar2));

		switch_channel_add_variable_var_check(br_a_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
		switch_channel_set_variable(br_a_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, cp->transfer_source);

		switch_core_session_rwunlock(br_a_session);
	}


}

static switch_bool_t attended_transfer(switch_core_session_t *session, switch_core_session_t *b_session) {
	verto_pvt_t *tech_pvt = NULL, *b_tech_pvt = NULL;
	switch_bool_t result = SWITCH_FALSE;
	const char *br_a = NULL, *br_b = NULL;

	tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
	b_tech_pvt = switch_core_session_get_private_class(b_session, SWITCH_PVT_SECONDARY);

	if (tech_pvt && b_tech_pvt) {
		switch_channel_set_variable(tech_pvt->channel, "refer_uuid", switch_core_session_get_uuid(b_tech_pvt->session));
		switch_channel_set_variable(tech_pvt->channel, "transfer_disposition", "recv_replace");
		switch_channel_set_variable(b_tech_pvt->channel, "transfer_disposition", "replaced");

		br_a = switch_channel_get_partner_uuid(tech_pvt->channel);
		br_b = switch_channel_get_partner_uuid(b_tech_pvt->channel);

		if (!switch_ivr_uuid_exists(br_a)) {
			br_a = NULL;
		}

		if (!switch_ivr_uuid_exists(br_b)) {
			br_b = NULL;
		}
	}

	if (tech_pvt && b_tech_pvt && switch_channel_test_flag(b_tech_pvt->channel, CF_ORIGINATOR)) {
		switch_core_session_t *a_session;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE,
						  "Attended Transfer on originating session %s\n", switch_core_session_get_uuid(b_session));



		switch_channel_set_variable_printf(b_tech_pvt->channel, "transfer_to", "satt:%s", br_a);

		switch_channel_set_variable(b_tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");


		switch_channel_clear_flag(b_tech_pvt->channel, CF_LEG_HOLDING);
		switch_channel_set_variable(b_tech_pvt->channel, SWITCH_HOLDING_UUID_VARIABLE, br_a);
		switch_channel_set_flag(b_tech_pvt->channel, CF_XFER_ZOMBIE);
		switch_channel_set_flag(b_tech_pvt->channel, CF_TRANSFER);


		if ((a_session = switch_core_session_locate(br_a))) {
			const char *moh = "local_stream://moh";
			switch_channel_t *a_channel = switch_core_session_get_channel(a_session);
			switch_caller_profile_t *prof = switch_channel_get_caller_profile(b_tech_pvt->channel);
			const char *tmp;

			switch_core_event_hook_add_state_change(a_session, xfer_hanguphook);
			switch_channel_set_variable(a_channel, "att_xfer_kill_uuid", switch_core_session_get_uuid(b_session));
			switch_channel_set_variable(a_channel, "att_xfer_destination_number", prof->destination_number);
			switch_channel_set_variable(a_channel, "att_xfer_callee_id_name", prof->callee_id_name);
			switch_channel_set_variable(a_channel, "att_xfer_callee_id_number", prof->callee_id_number);

			if ((tmp = switch_channel_get_hold_music(a_channel))) {
				moh = tmp;
			}

			if (!zstr(moh) && !strcasecmp(moh, "silence")) {
				moh = NULL;
			}

			if (moh) {
				char *xdest;
				xdest = switch_core_session_sprintf(a_session, "m:\":endless_playback:%s\"park", moh);
				switch_ivr_session_transfer(a_session, xdest, "inline", NULL);
			} else {
				switch_ivr_session_transfer(a_session, "park", "inline", NULL);
			}

			switch_core_session_rwunlock(a_session);

			result = SWITCH_TRUE;

			switch_channel_hangup(b_tech_pvt->channel, SWITCH_CAUSE_NORMAL_CLEARING);
		} else {
			result = SWITCH_FALSE;
		}

	} else if (br_a && br_b) {
		switch_core_session_t *tmp = NULL;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Attended Transfer [%s][%s]\n",
						  switch_str_nil(br_a), switch_str_nil(br_b));

		if ((tmp = switch_core_session_locate(br_b))) {
			switch_channel_t *tchannel = switch_core_session_get_channel(tmp);

			switch_channel_set_variable(tchannel, "transfer_disposition", "bridge");

			switch_channel_set_flag(tchannel, CF_ATTENDED_TRANSFER);
			switch_core_session_rwunlock(tmp);
		}

		if (switch_true(switch_channel_get_variable(tech_pvt->channel, "recording_follow_transfer")) &&
			(tmp = switch_core_session_locate(br_a))) {
			switch_channel_set_variable(switch_core_session_get_channel(tmp), "transfer_disposition", "bridge");
			switch_ivr_transfer_recordings(session, tmp);
			switch_core_session_rwunlock(tmp);
		}


		if (switch_true(switch_channel_get_variable(b_tech_pvt->channel, "recording_follow_transfer")) &&
			(tmp = switch_core_session_locate(br_b))) {
			switch_ivr_transfer_recordings(b_session, tmp);
			switch_core_session_rwunlock(tmp);
		}

		switch_channel_set_variable_printf(tech_pvt->channel, "transfer_to", "att:%s", br_b);

		mark_transfer_record(session, br_a, br_b);

		switch_ivr_uuid_bridge(br_a, br_b);
		switch_channel_set_variable(b_tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "ATTENDED_TRANSFER");

		result = SWITCH_TRUE;

		switch_channel_clear_flag(b_tech_pvt->channel, CF_LEG_HOLDING);
		switch_channel_set_variable(b_tech_pvt->channel, "park_timeout", "2:attended_transfer");
		switch_channel_set_state(b_tech_pvt->channel, CS_PARK);

	} else {
		if (!br_a && !br_b) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
							  "Cannot transfer channels that are not in a bridge.\n");
			result = SWITCH_FALSE;
		} else {
			switch_core_session_t *t_session, *hup_session;
			switch_channel_t *hup_channel;
			const char *ext;

			if (br_a && !br_b) {
				t_session = switch_core_session_locate(br_a);
				hup_channel = b_tech_pvt->channel;
				hup_session = b_session;
			} else {
				verto_pvt_t *h_tech_pvt = (verto_pvt_t *) switch_core_session_get_private_class(b_session, SWITCH_PVT_SECONDARY);
				t_session = switch_core_session_locate(br_b);
				hup_channel = tech_pvt->channel;
				hup_session = session;
				switch_channel_clear_flag(h_tech_pvt->channel, CF_LEG_HOLDING);
				switch_channel_hangup(b_tech_pvt->channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
			}

			if (t_session) {
				//switch_channel_t *t_channel = switch_core_session_get_channel(t_session);
				const char *idest = switch_channel_get_variable(hup_channel, "inline_destination");
				ext = switch_channel_get_variable(hup_channel, "destination_number");

				if (switch_true(switch_channel_get_variable(hup_channel, "recording_follow_transfer"))) {
					switch_ivr_transfer_recordings(hup_session, t_session);
				}

				if (idest) {
					switch_ivr_session_transfer(t_session, idest, "inline", NULL);
				} else {
					switch_ivr_session_transfer(t_session, ext, NULL, NULL);
				}

				result = SWITCH_TRUE;
				switch_channel_hangup(hup_channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
				switch_core_session_rwunlock(t_session);
			} else {
				result = SWITCH_FALSE;
			}
		}
	}

	return result;
}


static switch_bool_t verto__modify_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *obj = cJSON_CreateObject();
	switch_core_session_t *session;
	cJSON *dialog = NULL;
	const char *call_id = NULL, *destination = NULL, *action = NULL;
	int err = 0;

	*response = obj;

	if (!params) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if (!(dialog = cJSON_GetObjectItem(params, "dialogParams"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Dialog data missing"));
		err = 1; goto cleanup;
	}

	if (!(call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CallID missing"));
		err = 1; goto cleanup;
	}

	if (!(action = cJSON_GetObjectCstr(params, "action"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("action missing"));
		err = 1; goto cleanup;
	}

	cJSON_AddItemToObject(obj, "callID", cJSON_CreateString(call_id));
	cJSON_AddItemToObject(obj, "action", cJSON_CreateString(action));


	if ((session = switch_core_session_locate(call_id))) {
		verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

		if (!strcasecmp(action, "transfer")) {
			switch_core_session_t *other_session = NULL;

			if (!(destination = cJSON_GetObjectCstr(params, "destination"))) {
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("destination missing"));
				err = 1; goto rwunlock;
			}

			if (switch_core_session_get_partner(tech_pvt->session, &other_session) == SWITCH_STATUS_SUCCESS) {
				switch_ivr_session_transfer(other_session, destination, NULL, NULL);
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL TRANSFERRED"));
				switch_channel_set_variable(tech_pvt->channel, "transfer_disposition", "recv_replace");
				switch_core_session_rwunlock(other_session);
			} else {
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("call is not bridged"));
				err = 1; goto rwunlock;
			}

		} else if (!strcasecmp(action, "replace")) {
			const char *replace_call_id;
			switch_core_session_t *b_session = NULL;

			if (!(replace_call_id = cJSON_GetObjectCstr(params, "replaceCallID"))) {
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("replaceCallID missing"));
				err = 1; goto rwunlock;
			}

			if ((b_session = switch_core_session_locate(replace_call_id))) {
				err = (int) !attended_transfer(session, b_session);
				if (err) {
					cJSON_AddItemToObject(obj, "message", cJSON_CreateString("transfer failed"));
				}
				switch_core_session_rwunlock(b_session);
			} else {
				cJSON_AddItemToObject(obj, "message", cJSON_CreateString("invalid transfer leg"));
				err = 1; goto rwunlock;
			}
		} else if (!strcasecmp(action, "hold")) {
			switch_core_media_toggle_hold(session, 1);
		} else if (!strcasecmp(action, "unhold")) {
			switch_core_media_toggle_hold(session, 0);
		} else if (!strcasecmp(action, "toggleHold")) {
			switch_core_media_toggle_hold(session, !!!switch_channel_test_flag(tech_pvt->channel, CF_PROTO_HOLD));
		}

		cJSON_AddItemToObject(obj, "holdState", cJSON_CreateString(switch_channel_test_flag(tech_pvt->channel, CF_PROTO_HOLD) ? "held" : "active"));


	rwunlock:

		switch_core_session_rwunlock(session);
	} else {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL DOES NOT EXIST"));
		err = 1;
	}

 cleanup:


	if (!err) return SWITCH_TRUE;


	cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));


	return SWITCH_FALSE;
}

static switch_bool_t verto__attach_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *obj = cJSON_CreateObject();
	switch_core_session_t *session = NULL;
	int err = 0;
	cJSON *dialog;
	verto_pvt_t *tech_pvt = NULL;
	const char *call_id = NULL, *sdp = NULL;
	uint8_t match = 0, p = 0;

	*response = obj;

	if (!params) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if (!(dialog = cJSON_GetObjectItem(params, "dialogParams"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Dialog data missing"));
		err = 1; goto cleanup;
	}

	if (!(sdp = cJSON_GetObjectCstr(params, "sdp"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("SDP missing"));
		err = 1; goto cleanup;
	}

	if (!(call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CallID missing"));
		err = 1; goto cleanup;
	}

	if (!(session = switch_core_session_locate(call_id))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL DOES NOT EXIST"));
		err = 1; goto cleanup;
	}

	tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
	tech_pvt->r_sdp = switch_core_session_strdup(session, sdp);


	if (!switch_channel_test_flag(tech_pvt->channel, CF_ANSWERED)) {
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Cannot attach to a call that has not been answered."));
		err = 1; goto cleanup;
	}


	switch_channel_set_variable(tech_pvt->channel, SWITCH_R_SDP_VARIABLE, tech_pvt->r_sdp);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote SDP %s:\n%s\n",
					  switch_channel_get_name(tech_pvt->channel), tech_pvt->r_sdp);

	switch_core_media_clear_ice(tech_pvt->session);
	switch_channel_set_flag(tech_pvt->channel, CF_REINVITE);
	switch_channel_set_flag(tech_pvt->channel, CF_RECOVERING);

	//switch_channel_audio_sync(tech_pvt->channel);
	//switch_channel_set_flag(tech_pvt->channel, CF_VIDEO_BREAK);
	//switch_core_session_kill_channel(tech_pvt->session, SWITCH_SIG_BREAK);

	if ((match = switch_core_media_negotiate_sdp(tech_pvt->session, tech_pvt->r_sdp, &p, SDP_TYPE_RESPONSE))) {
		if (switch_core_media_activate_rtp(tech_pvt->session) != SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "MEDIA ERROR");
			cJSON_AddItemToObject(obj, "message", cJSON_CreateString("MEDIA ERROR"));
			err = 1; goto cleanup;
		}
	} else {
		switch_channel_set_variable(tech_pvt->channel, SWITCH_ENDPOINT_DISPOSITION_VARIABLE, "CODEC NEGOTIATION ERROR");
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CODEC NEGOTIATION ERROR"));
		err = 1; goto cleanup;
	}

 cleanup:

	if (tech_pvt) {
		switch_channel_clear_flag(tech_pvt->channel, CF_REINVITE);
		switch_channel_clear_flag(tech_pvt->channel, CF_RECOVERING);
		switch_clear_flag(tech_pvt, TFLAG_ATTACH_REQ);
		if (switch_channel_test_flag(tech_pvt->channel, CF_CONFERENCE)) {
			switch_channel_set_flag(tech_pvt->channel, CF_CONFERENCE_ADV);
		}
	}

	if (session) {
		switch_core_session_rwunlock(session);
	}

	if (!err) {
		return SWITCH_TRUE;
	}

	if (tech_pvt && tech_pvt->channel) {
		switch_channel_hangup(tech_pvt->channel, SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL);
	}


	cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));

	return SWITCH_FALSE;
}

static void parse_user_vars(cJSON *obj, switch_core_session_t *session)
{
	cJSON *json_ptr;

	switch_assert(obj);
	switch_assert(session);

	if ((json_ptr = cJSON_GetObjectItem(obj, "userVariables"))) {
		cJSON * i;
		switch_channel_t *channel = switch_core_session_get_channel(session);

		for(i = json_ptr->child; i; i = i->next) {
			char *varname = switch_core_session_sprintf(session, "verto_dvar_%s", i->string);

			if (i->type == cJSON_True) {
				switch_channel_set_variable(channel, varname, "true");
			} else if (i->type == cJSON_False) {
				switch_channel_set_variable(channel, varname, "false");
			} else if (!zstr(i->string) && !zstr(i->valuestring)) {
				switch_channel_set_variable(channel, varname, i->valuestring);
			}
		}
	}
}

static switch_bool_t verto__info_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *msg = NULL, *dialog = NULL, *txt = NULL;
	const char *call_id = NULL, *dtmf = NULL;
	switch_bool_t r = SWITCH_TRUE;
	char *proto = VERTO_CHAT_PROTO;
	char *pproto = NULL;
	int err = 0;

	*response = cJSON_CreateObject();

	if (!params) {
		cJSON_AddItemToObject(*response, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if ((dialog = cJSON_GetObjectItem(params, "dialogParams")) && (call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		switch_core_session_t *session = NULL;

		if ((session = switch_core_session_locate(call_id))) {

			parse_user_vars(dialog, session);

			if ((dtmf = cJSON_GetObjectCstr(params, "dtmf"))) {
				verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
				char *send;

				if (!tech_pvt) {
					cJSON_AddItemToObject(*response, "message", cJSON_CreateString("Invalid channel"));
					err = 1; goto cleanup;
				}

				send = switch_mprintf("~%s", dtmf);

				if (switch_channel_test_flag(tech_pvt->channel, CF_PROXY_MODE)) {
					switch_core_session_t *other_session = NULL;

					if (switch_core_session_get_partner(tech_pvt->session, &other_session) == SWITCH_STATUS_SUCCESS) {
						switch_core_session_send_dtmf_string(other_session, send);
						switch_core_session_rwunlock(other_session);
					}
				} else {
					switch_channel_queue_dtmf_string(tech_pvt->channel, send);
				}
				free(send);
				cJSON_AddItemToObject(*response, "message", cJSON_CreateString("SENT"));
			}

			switch_core_session_rwunlock(session);
		}
	}

	if ((txt = cJSON_GetObjectItem(params, "txt"))) {
		switch_core_session_t *session;

		if ((session = switch_core_session_locate(call_id))) {
			verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
			char charbuf[2] = "";
			char *chardata = NULL;
			cJSON *data;

			if (tech_pvt->text_read_buffer) {
				if ((data = cJSON_GetObjectItem(txt, "code"))) {
					charbuf[0] = data->valueint;
					chardata = charbuf;
				} else if ((data = cJSON_GetObjectItem(txt, "chars"))) {
					if (data->valuestring) {
						chardata = data->valuestring;
					} else if (data->valueint) {
						charbuf[0] = data->valueint;
						chardata = charbuf;
					}
				}


				if (chardata) {
					switch_mutex_lock(tech_pvt->text_read_mutex);
					switch_buffer_write(tech_pvt->text_read_buffer, chardata, strlen(chardata));
					switch_mutex_unlock(tech_pvt->text_read_mutex);

					if ((switch_mutex_trylock(tech_pvt->text_cond_mutex) == SWITCH_STATUS_SUCCESS)) {
						switch_thread_cond_signal(tech_pvt->text_cond);
						switch_mutex_unlock(tech_pvt->text_cond_mutex);
					}
				}

			}

			switch_core_session_rwunlock(session);
		}
	}

	if ((msg = cJSON_GetObjectItem(params, "msg"))) {
		switch_event_t *event;
		char *to = (char *) cJSON_GetObjectCstr(msg, "to");
		//char *from = (char *) cJSON_GetObjectCstr(msg, "from");
		cJSON *i, *indialog =  cJSON_GetObjectItem(msg, "inDialog");
		const char *body = cJSON_GetObjectCstr(msg, "body");
		switch_bool_t is_dialog = indialog && (indialog->type == cJSON_True || (indialog->type == cJSON_String && switch_true(indialog->valuestring)));

		if (!zstr(to)) {
			if (strchr(to, '+')) {
				pproto = strdup(to);
				switch_assert(pproto);
				if ((to = strchr(pproto, '+'))) {
					*to++ = '\0';
				}
				proto = pproto;
			}
		}

		if (!zstr(to) && !zstr(body) && switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", VERTO_CHAT_PROTO);

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", jsock->uid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_user", jsock->id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_host", jsock->domain);


			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "to", to);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "text/plain");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from_full", jsock->id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "verto_profile", jsock->profile->name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "verto_jsock_uuid", jsock->uuid_str);

			for(i = msg->child; i; i = i->next) {
				if (!zstr(i->string) && !zstr(i->valuestring) && (!strncasecmp(i->string, "from_", 5) || !strncasecmp(i->string, "to_", 3))) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, i->string, i->valuestring);
				}
			}

			if (is_dialog) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call_id", call_id);
			}

			switch_event_add_body(event, "%s", body);

			if (strcasecmp(proto, VERTO_CHAT_PROTO)) {
				switch_core_chat_send(proto, event);
			}

			if (is_dialog) {
				if ((dialog = cJSON_GetObjectItem(params, "dialogParams")) && (call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
					switch_core_session_t *session = NULL;

					if ((session = switch_core_session_locate(call_id))) {
						switch_core_session_queue_event(session, &event);
						switch_core_session_rwunlock(session);
					}
				}

			} else {
				switch_core_chat_send("GLOBAL", event);
			}

			if (event) {
				switch_event_destroy(&event);
			}

			cJSON_AddItemToObject(*response, "message", cJSON_CreateString("SENT"));
			r = SWITCH_TRUE;

		} else {
			r = SWITCH_FALSE;
			cJSON_AddItemToObject(*response, "message", cJSON_CreateString("INVALID MESSAGE to and body params required"));
		}


		switch_safe_free(pproto);
	}

 cleanup:

	if (!err) return r;

	cJSON_AddItemToObject(*response, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));

	return SWITCH_FALSE;
}



static switch_bool_t verto__invite_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *obj = cJSON_CreateObject(), *screenShare = NULL, *dedEnc = NULL, *mirrorInput, *bandwidth = NULL, *canvas = NULL;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel;
	switch_event_t *var_event;
	switch_call_cause_t reason = SWITCH_CAUSE_INVALID_MSG_UNSPECIFIED, cancel_cause = 0;
	switch_caller_profile_t *caller_profile;
	int err = 0;
	cJSON *dialog;
	verto_pvt_t *tech_pvt;
	char name[512];
	const char *var, *destination_number, *call_id = NULL, *sdp = NULL,
		*caller_id_name = NULL, *caller_id_number = NULL, *remote_caller_id_name = NULL, *remote_caller_id_number = NULL,*context = NULL;
	switch_event_header_t *hp;

	*response = obj;

	PROTECT_INTERFACE(verto_endpoint_interface);

	if (!params) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Params data missing"));
		err = 1; goto cleanup;
	}

	if (switch_event_create_plain(&var_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
		err=1; goto cleanup;
	}

	if (!(dialog = cJSON_GetObjectItem(params, "dialogParams"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Dialog data missing"));
		err = 1; goto cleanup;
	}

	if (!(call_id = cJSON_GetObjectCstr(dialog, "callID"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CallID missing"));
		err = 1; goto cleanup;
	}

	if (!(sdp = cJSON_GetObjectCstr(params, "sdp"))) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("SDP missing"));
		err = 1; goto cleanup;
	}

	switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_uuid", call_id);

	if ((reason = switch_core_session_outgoing_channel(NULL, var_event, "rtc",
													   NULL, &session, NULL, SOF_NONE, &cancel_cause)) != SWITCH_CAUSE_SUCCESS) {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Cannot create channel"));
		err = 1; goto cleanup;
	}

	channel = switch_core_session_get_channel(session);
	switch_channel_set_direction(channel, SWITCH_CALL_DIRECTION_INBOUND);

	tech_pvt = switch_core_session_alloc(session, sizeof(*tech_pvt));
	tech_pvt->session = session;
	tech_pvt->pool = switch_core_session_get_pool(session);
	tech_pvt->channel = channel;
	tech_pvt->jsock_uuid = switch_core_session_strdup(session, jsock->uuid_str);
	tech_pvt->r_sdp = switch_core_session_strdup(session, sdp);
	switch_core_media_set_sdp_codec_string(session, sdp, SDP_TYPE_REQUEST);
	switch_core_session_set_private_class(session, tech_pvt, SWITCH_PVT_SECONDARY);

	tech_pvt->call_id = switch_core_session_strdup(session, call_id);

	if (!(destination_number = cJSON_GetObjectCstr(dialog, "destination_number"))) {
		destination_number = "service";
	}

	switch_snprintf(name, sizeof(name), "verto.rtc/%s", destination_number);
	switch_channel_set_name(channel, name);

	if ((tech_pvt->smh = switch_core_session_get_media_handle(session))) {
		tech_pvt->mparams = switch_core_media_get_mparams(tech_pvt->smh);
		if (verto_set_media_options(tech_pvt, jsock->profile) != SWITCH_STATUS_SUCCESS) {
			cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Cannot set media options"));
			err = 1; goto cleanup;
		}
	} else {
		cJSON_AddItemToObject(obj, "message", cJSON_CreateString("Cannot create media handle"));
		err = 1; goto cleanup;
	}

	if ((screenShare = cJSON_GetObjectItem(dialog, "screenShare")) && screenShare->type == cJSON_True) {
		switch_channel_set_variable(channel, "video_screen_share", "true");
		switch_channel_set_flag(channel, CF_VIDEO_ONLY);
	}

	if ((dedEnc = cJSON_GetObjectItem(dialog, "dedEnc")) && dedEnc->type == cJSON_True) {
		switch_channel_set_variable(channel, "video_use_dedicated_encoder", "true");
	}

	if ((mirrorInput = cJSON_GetObjectItem(dialog, "mirrorInput")) && mirrorInput->type == cJSON_True) {
		switch_channel_set_variable(channel, "video_mirror_input", "true");
		switch_channel_set_flag(channel, CF_VIDEO_MIRROR_INPUT);
	}

	if ((canvas = cJSON_GetObjectItem(dialog, "conferenceCanvasID"))) {
		int canvas_id = 0;

		if (!zstr(canvas->valuestring)) {
			canvas_id = atoi(canvas->valuestring);
		} else if (canvas->valueint) {
			canvas_id = canvas->valueint;
		}

		if (canvas_id >= 0) {
			switch_channel_set_variable_printf(channel, "video_initial_watching_canvas", "%d", canvas_id);
			switch_channel_set_variable(channel, "video_second_screen", "true");
		}
	}

	if ((bandwidth = cJSON_GetObjectItem(dialog, "outgoingBandwidth"))) {
		int core_bw = 0, bwval = 0;
		const char *val;

		if ((val = switch_channel_get_variable_dup(channel, "rtp_video_max_bandwidth_in", SWITCH_FALSE, -1))) {
			core_bw = switch_parse_bandwidth_string(val);
		}

		if (!zstr(bandwidth->valuestring) && strcasecmp(bandwidth->valuestring, "default")) {
			bwval = atoi(bandwidth->valuestring);
		} else if (bandwidth->valueint) {
			bwval = bandwidth->valueint;
		}

		if (bwval < 0) bwval = 0;

		if (core_bw && bwval && bwval < core_bw) {
			switch_channel_set_variable_printf(channel, "rtp_video_max_bandwidth_in", "%d", bwval);
		}
	}

	if ((bandwidth = cJSON_GetObjectItem(dialog, "incomingBandwidth"))) {
		int core_bw = 0, bwval = 0;
		const char *val;

		if ((val = switch_channel_get_variable_dup(channel, "rtp_video_max_bandwidth_out", SWITCH_FALSE, -1))) {
			core_bw = switch_parse_bandwidth_string(val);
		}

		if (!zstr(bandwidth->valuestring) && strcasecmp(bandwidth->valuestring, "default")) {
			bwval = atoi(bandwidth->valuestring);
		} else if (bandwidth->valueint) {
			bwval = bandwidth->valueint;
		}

		if (bwval < 0) bwval = 0;

		if (core_bw && bwval && bwval < core_bw) {
			switch_channel_set_variable_printf(channel, "rtp_video_max_bandwidth_out", "%d", bwval);
		}
	}

	parse_user_vars(dialog, session);


	switch_channel_set_variable(channel, "jsock_uuid_str", jsock->uuid_str);
	switch_channel_set_variable(channel, "verto_user", jsock->uid);
	switch_channel_set_variable(channel, "presence_id", jsock->uid);
	switch_channel_set_variable(channel, "verto_client_address", jsock->name);
	switch_channel_set_variable(channel, "chat_proto", VERTO_CHAT_PROTO);
	switch_channel_set_variable(channel, "verto_host", jsock->domain);
	switch_channel_set_variable(channel, "event_channel_cookie", tech_pvt->jsock_uuid);
	switch_channel_set_variable(channel, "verto_profile_name", jsock->profile->name);

	caller_id_name = cJSON_GetObjectCstr(dialog, "caller_id_name");
	caller_id_number = cJSON_GetObjectCstr(dialog, "caller_id_number");

	remote_caller_id_name = cJSON_GetObjectCstr(dialog, "remote_caller_id_name");
	remote_caller_id_number = cJSON_GetObjectCstr(dialog, "remote_caller_id_number");

	if (zstr(caller_id_name)) {
		if ((var = switch_event_get_header(jsock->params, "caller-id-name"))) {
			caller_id_name = var;
		}
	} else if (caller_id_name) {
		switch_event_add_header_string(jsock->params, SWITCH_STACK_BOTTOM, "caller-id-name", caller_id_name);
	}

	if (zstr(caller_id_number)) {
		if ((var = switch_event_get_header(jsock->params, "caller-id-number"))) {
			caller_id_number = var;
		}
	}

	if (!(context = switch_event_get_header(jsock->vars, "user_context"))) {
		context = switch_either(jsock->context, jsock->profile->context);
	}

	if ((caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
													jsock->uid,
													switch_either(jsock->dialplan, jsock->profile->dialplan),
													caller_id_name,
													caller_id_number,
													jsock->remote_host,
													cJSON_GetObjectCstr(dialog, "ani"),
													cJSON_GetObjectCstr(dialog, "aniii"),
													cJSON_GetObjectCstr(dialog, "rdnis"),
													modname,
													context,
													destination_number))) {

		switch_channel_set_caller_profile(channel, caller_profile);

	}

	switch_ivr_set_user(session, jsock->uid);

	for (hp = jsock->user_vars->headers; hp; hp = hp->next) {
		switch_channel_set_variable(channel, hp->name, hp->value);
	}


	switch_channel_set_profile_var(channel, "callee_id_name", remote_caller_id_name);
	switch_channel_set_profile_var(channel, "callee_id_number", remote_caller_id_number);


	switch_channel_set_variable(channel, "verto_remote_caller_id_name", remote_caller_id_name);
	switch_channel_set_variable(channel, "verto_remote_caller_id_number", remote_caller_id_number);



	switch_channel_set_variable(channel, SWITCH_R_SDP_VARIABLE, sdp);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Remote SDP %s:\n%s\n", switch_channel_get_name(tech_pvt->channel), sdp);

	cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL CREATED"));
	cJSON_AddItemToObject(obj, "callID", cJSON_CreateString(tech_pvt->call_id));

	switch_channel_add_state_handler(channel, &verto_state_handlers);
	switch_core_event_hook_add_receive_message(session, messagehook);
	switch_channel_set_state(channel, CS_INIT);
	//track_pvt(tech_pvt);
	switch_core_session_thread_launch(session);

 cleanup:

	switch_event_destroy(&var_event);

	if (!err) {
		return SWITCH_TRUE;
	}

	UNPROTECT_INTERFACE(verto_endpoint_interface);

	if (session) {
		switch_core_session_destroy(&session);
	}

	cJSON_AddItemToObject(obj, "causeCode", cJSON_CreateNumber(reason));
	cJSON_AddItemToObject(obj, "cause", cJSON_CreateString(switch_channel_cause2str(reason)));
	cJSON_AddItemToObject(obj, "message", cJSON_CreateString("CALL ERROR"));
	cJSON_AddItemToObject(obj, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));

	return SWITCH_FALSE;

}

static switch_bool_t event_channel_check_auth(jsock_t *jsock, const char *event_channel)
{

	char *main_event_channel = NULL;
	switch_bool_t ok = SWITCH_TRUE, pre_ok = SWITCH_FALSE;
	switch_core_session_t *session = NULL;

	switch_assert(event_channel);

	pre_ok = switch_event_channel_permission_verify(jsock->uuid_str, event_channel);

	if (!pre_ok && (session = switch_core_session_locate(event_channel))) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		const char *jsock_uuid_str = switch_channel_get_variable(channel, "jsock_uuid_str");

		if (jsock_uuid_str && !strcmp(jsock_uuid_str, jsock->uuid_str)) {
			pre_ok = SWITCH_TRUE;
		}

		switch_core_session_rwunlock(session);
	}

	if (pre_ok) {
		return pre_ok;
	}

	if (jsock->allowed_event_channels) {
		if (strchr(event_channel, '.')) {
			char *p;
			main_event_channel = strdup(event_channel);
			switch_assert(main_event_channel);
			if ((p = strchr(main_event_channel, '.'))) {
				*p = '\0';
			}
		}

		if ((!verto_globals.enable_fs_events && (!strcasecmp(event_channel, "FSevent") || (main_event_channel && !strcasecmp(main_event_channel, "FSevent")))) || 
			!(switch_event_get_header(jsock->allowed_event_channels, event_channel) || 
			  (main_event_channel && switch_event_get_header(jsock->allowed_event_channels, main_event_channel)))) {
			ok = SWITCH_FALSE;
		}
	}

	switch_safe_free(main_event_channel);
	return ok;

}

static switch_bool_t parse_subs(jsock_t *jsock, const char *event_channel, cJSON **sub_list, cJSON **err_list, cJSON **exist_list)
{
	switch_bool_t r = SWITCH_FALSE;

	if (event_channel_check_auth(jsock, event_channel)) {
		if (!*sub_list) {
			*sub_list = cJSON_CreateArray();
		}

		if (jsock_sub_channel(jsock, event_channel) == SWITCH_STATUS_SUCCESS) {
			cJSON_AddItemToArray(*sub_list, cJSON_CreateString(event_channel));
		} else {
			if (!*exist_list) {
				*exist_list = cJSON_CreateArray();
			}
			cJSON_AddItemToArray(*exist_list, cJSON_CreateString(event_channel));
		}

		r = SWITCH_TRUE;
	} else {
		if (!*err_list) {
			*err_list = cJSON_CreateArray();
		}
		cJSON_AddItemToArray(*err_list, cJSON_CreateString(event_channel));
	}

	return r;
}

static switch_bool_t verto__subscribe_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	switch_bool_t r = SWITCH_TRUE;
	cJSON *subs = NULL, *errs = NULL, *exist = NULL;

	*response = cJSON_CreateObject();

	if (params) {
		cJSON *jchannel = cJSON_GetObjectItem(params, "eventChannel");

		if (jchannel) {
			if (jchannel->type == cJSON_String) {
				parse_subs(jsock, jchannel->valuestring, &subs, &errs, &exist);
			} else if (jchannel->type == cJSON_Array) {
				int i, len = cJSON_GetArraySize(jchannel);

				for(i = 0; i < len; i++) {
					cJSON *str = cJSON_GetArrayItem(jchannel, i);
					if (str->type == cJSON_String) {
						parse_subs(jsock, str->valuestring, &subs, &errs, &exist);
					}
				}
			}
		}
	}

	if (subs) {
		cJSON_AddItemToObject(*response, "subscribedChannels", subs);
	}

	if (errs) {
		cJSON_AddItemToObject(*response, "unauthorizedChannels", errs);
	}

	if (exist) {
		cJSON_AddItemToObject(*response, "alreadySubscribedChannels", exist);
	}

	if (!subs) {
		r = SWITCH_FALSE;
	}

	return r;
}

static void do_unsub(jsock_t *jsock, const char *event_channel, cJSON **subs, cJSON **errs)
{
	if (jsock_unsub_channel(jsock, event_channel)) {
		if (!*subs) {
			*subs = cJSON_CreateArray();
		}
		cJSON_AddItemToArray(*subs, cJSON_CreateString(event_channel));
	} else {
		if (!*errs) {
			*errs = cJSON_CreateArray();
		}
		cJSON_AddItemToArray(*errs, cJSON_CreateString(event_channel));
	}
}

static switch_bool_t verto__unsubscribe_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	switch_bool_t r = SWITCH_TRUE;
	cJSON *subs = NULL, *errs = NULL;

	*response = cJSON_CreateObject();

	if (params) {
		cJSON *jchannel = cJSON_GetObjectItem(params, "eventChannel");

		if (jchannel) {
			if (jchannel->type == cJSON_String) {
				do_unsub(jsock, jchannel->valuestring, &subs, &errs);
			} else if (jchannel->type == cJSON_Array) {
				int i, len = cJSON_GetArraySize(jchannel);

				for(i = 0; i < len; i++) {
					cJSON *str = cJSON_GetArrayItem(jchannel, i);
					if (str->type == cJSON_String) {
						do_unsub(jsock, str->valuestring, &subs, &errs);
					}
				}
			}
		}
	}

	if (subs) {
		cJSON_AddItemToObject(*response, "unsubscribedChannels", subs);
	}

	if (errs) {
		cJSON_AddItemToObject(*response, "notSubscribedChannels", errs);
	}

	if (errs && !subs) {
		r = SWITCH_FALSE;
	}

	return r;
}

static switch_bool_t verto__broadcast_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	char *json_text = NULL;
	const char *event_channel = cJSON_GetObjectCstr(params, "eventChannel");
	cJSON *jevent, *broadcast;
	const char *display = NULL;

	*response = cJSON_CreateObject();


	if (!event_channel) {
		cJSON_AddItemToObject(*response, "message", cJSON_CreateString("eventChannel not specified."));
		cJSON_AddItemToObject(*response, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));
		goto end;
	}

	if (!event_channel_check_auth(jsock, event_channel)) {
		cJSON_AddItemToObject(*response, "message", cJSON_CreateString("Permission Denied."));
		cJSON_AddItemToObject(*response, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));
		goto end;
	}


	cJSON_AddItemToObject(params, "userid", cJSON_CreateString(jsock->uid));

	display = switch_event_get_header(jsock->params, "caller-id-name");
	if (display) {
		cJSON_AddItemToObject(params, "fromDisplay", cJSON_CreateString(display));
	}

	jevent = cJSON_Duplicate(params, 1);

	broadcast = cJSON_GetObjectItem(params, "localBroadcast");

	if (broadcast && broadcast->type == cJSON_True) {
		write_event(event_channel, NULL, jevent);
	} else {
		switch_event_channel_broadcast(event_channel, &jevent, modname, verto_globals.event_channel_id);
	}

	if (jsock->profile->mcast_pub.sock != ws_sock_invalid) {
		if ((json_text = cJSON_PrintUnformatted(params))) {

			if (mcast_socket_send(&jsock->profile->mcast_pub, json_text, strlen(json_text) + 1) <= 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "multicast socket send error! %s\n", strerror(errno));
				//r = SWITCH_FALSE;
				//cJSON_AddItemToObject(*response, "message", cJSON_CreateString("MCAST Data Send failure!"));
			} else {
				//r = SWITCH_TRUE;
				//cJSON_AddItemToObject(*response, "message", cJSON_CreateString("MCAST Data Sent"));
				if (verto_globals.debug > 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MCAST Data Sent: %s\n",json_text);
				}
			}
			free(json_text);
			json_text = NULL;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "JSON ERROR!\n");
		}
	}

 end:

	return SWITCH_TRUE;
}

static switch_bool_t login_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	*response = cJSON_CreateObject();
	cJSON_AddItemToObject(*response, "message", cJSON_CreateString("logged in"));

	login_fire_custom_event(jsock, params, 1, "Logged in");

	return SWITCH_TRUE;
}

static switch_bool_t echo_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	if (params) {
		*response = cJSON_Duplicate(params, 1);
		return SWITCH_TRUE;
	}

	*response = cJSON_CreateObject();

	cJSON_AddItemToObject(*response, "message", cJSON_CreateString("Params data missing"));
	cJSON_AddItemToObject(*response, "code", cJSON_CreateNumber(CODE_SESSION_ERROR));

	return SWITCH_FALSE;
}

static switch_bool_t jsapi_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	if (jsock->allowed_jsapi) {
		const char *function;

		if (params) {
			if ((function = cJSON_GetObjectCstr(params, "command"))) {
				if (!switch_event_get_header(jsock->allowed_jsapi, function)) {
					return SWITCH_FALSE;
				}

				if (jsock->allowed_fsapi && !strcmp(function, "fsapi")) {
					cJSON *data = cJSON_GetObjectItem(params, "data");
					if (data) {
						cJSON *cmd = cJSON_GetObjectItem(data, "cmd");
						cJSON *arg = cJSON_GetObjectItem(data, "arg");

						if (cmd  && cmd->type == cJSON_String && cmd->valuestring &&
							!auth_api_command(jsock, cmd->valuestring, arg ? arg->valuestring : NULL)) {
							return SWITCH_FALSE;
						}
					}
				}
			}
		}
	}

	switch_json_api_execute(params, NULL, response);

	return *response ? SWITCH_TRUE : SWITCH_FALSE;
}

static switch_bool_t fsapi_func(const char *method, cJSON *params, jsock_t *jsock, cJSON **response)
{
	cJSON *cmd = NULL, *arg = NULL, *reply;
	switch_stream_handle_t stream = { 0 };
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (params) {
		cmd = cJSON_GetObjectItem(params, "cmd");
		arg = cJSON_GetObjectItem(params, "arg");
	}

	if (cmd && jsock->allowed_fsapi) {
		if (cmd->type == cJSON_String && cmd->valuestring && !auth_api_command(jsock, cmd->valuestring, arg ? arg->valuestring : NULL)) {
			return SWITCH_FALSE;
		}
	}

	if (cmd && !cmd->valuestring) {
		cmd = NULL;
	}

	if (arg && !arg->valuestring) {
		arg = NULL;
	}

	reply = cJSON_CreateObject();

	SWITCH_STANDARD_STREAM(stream);

	if (cmd && (status = switch_api_execute(cmd->valuestring, arg ? arg->valuestring : NULL, NULL, &stream)) == SWITCH_STATUS_SUCCESS) {
		cJSON_AddItemToObject(reply, "message", cJSON_CreateString((char *) stream.data));
	} else {
		cJSON_AddItemToObject(reply, "message", cJSON_CreateString("INVALID CALL"));
	}

	switch_safe_free(stream.data);

	if (reply) {
		*response = reply;
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}

////

static void jrpc_init(void)
{
	jrpc_add_func("echo", echo_func);
	jrpc_add_func("jsapi", jsapi_func);
	jrpc_add_func("fsapi", fsapi_func);
	jrpc_add_func("login", login_func);

	jrpc_add_func("verto.invite", verto__invite_func);
	jrpc_add_func("verto.info", verto__info_func);
	jrpc_add_func("verto.attach", verto__attach_func);
	jrpc_add_func("verto.bye", verto__bye_func);
	jrpc_add_func("verto.answer", verto__answer_func);
	jrpc_add_func("verto.subscribe", verto__subscribe_func);
	jrpc_add_func("verto.unsubscribe", verto__unsubscribe_func);
	jrpc_add_func("verto.broadcast", verto__broadcast_func);
	jrpc_add_func("verto.modify", verto__modify_func);

}




static int start_jsock(verto_profile_t *profile, ws_socket_t sock, int family)
{
	jsock_t *jsock = NULL;
	int flag = 1;
	int i;
#ifndef WIN32
    unsigned int len;
#else
    int len;
#endif
	jsock_type_t ptype = PTYPE_CLIENT;
	switch_thread_data_t *td;
	switch_memory_pool_t *pool;
	switch_event_t *s_event;

	switch_core_new_memory_pool(&pool);


	jsock = (jsock_t *) switch_core_alloc(pool, sizeof(*jsock));
	jsock->pool = pool;
	jsock->family = family;

	if (family == PF_INET) {
		len = sizeof(jsock->remote_addr);

		if ((jsock->client_socket = accept(sock, (struct sockaddr *) &jsock->remote_addr, &len)) < 0) {
			die_errno("ACCEPT FAILED");
		}
	} else {
		len = sizeof(jsock->remote_addr6);

		if ((jsock->client_socket = accept(sock, (struct sockaddr *) &jsock->remote_addr6, &len)) < 0) {
			die_errno("ACCEPT FAILED");
		}
	}

	for (i = 0; i < profile->i; i++) {
		if ( profile->server_socket[i] == sock ) {
			if (profile->ip[i].secure) {
				ptype = PTYPE_CLIENT_SSL;
			}
			break;
		}
	}

	jsock->local_sock = sock;
	jsock->profile = profile;

	if (zstr(jsock->name)) {
		if (family == PF_INET) {
			jsock->remote_port = ntohs(jsock->remote_addr.sin_port);
			inet_ntop(AF_INET, &jsock->remote_addr.sin_addr, jsock->remote_host, sizeof(jsock->remote_host));
			jsock->name = switch_core_sprintf(pool, "%s:%d", jsock->remote_host, jsock->remote_port);
		} else {
			jsock->remote_port = ntohs(jsock->remote_addr6.sin6_port);
			inet_ntop(AF_INET6, &jsock->remote_addr6.sin6_addr, jsock->remote_host, sizeof(jsock->remote_host));
			jsock->name = switch_core_sprintf(pool, "[%s]:%d", jsock->remote_host, jsock->remote_port);
		}
	}

	jsock->ptype = ptype;

	for(i = 0; i < profile->conn_acl_count; i++) {
		if (!switch_check_network_list_ip(jsock->remote_host, profile->conn_acl[i])) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Client Connect from %s:%d refused by ACL %s\n",
							  jsock->name, jsock->remote_host, jsock->remote_port, profile->conn_acl[i]);
			goto error;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Client Connect from %s:%d accepted\n",
					  jsock->name, jsock->remote_host, jsock->remote_port);

	if (switch_event_create_subclass(&s_event, SWITCH_EVENT_CUSTOM, MY_EVENT_CLIENT_CONNECT) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(s_event, SWITCH_STACK_BOTTOM, "verto_profile_name", profile->name);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "verto_client_address", "%s", jsock->name);
		switch_event_fire(&s_event);
	}

	/* no nagle please */
	setsockopt(jsock->client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));


#if defined(SO_KEEPALIVE)
	setsockopt(jsock->client_socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&flag, sizeof(flag));
#endif
	flag = 30;
#if defined(TCP_KEEPIDLE)
	setsockopt(jsock->client_socket, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&flag, sizeof(flag));
#endif
#if defined(TCP_KEEPINTVL)
	setsockopt(jsock->client_socket, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&flag, sizeof(flag));
#endif

	td = switch_core_alloc(jsock->pool, sizeof(*td));

	td->alloc = 0;
	td->func = client_thread;
	td->obj = jsock;
	td->pool = pool;

	switch_mutex_init(&jsock->write_mutex, SWITCH_MUTEX_NESTED, jsock->pool);
	switch_mutex_init(&jsock->filter_mutex, SWITCH_MUTEX_NESTED, jsock->pool);
	switch_queue_create(&jsock->event_queue, MAX_QUEUE_LEN, jsock->pool);
	switch_thread_rwlock_create(&jsock->rwlock, jsock->pool);
	switch_thread_pool_launch_thread(&td);

	return 0;

 error:

	if (jsock) {
		if (jsock->client_socket != ws_sock_invalid) {
			close_socket(&jsock->client_socket);
		}

		switch_core_destroy_memory_pool(&pool);
	}

	return -1;
}

static ws_socket_t prepare_socket(ips_t *ips)
{
	ws_socket_t sock = ws_sock_invalid;
#ifndef WIN32
	int reuse_addr = 1;
#else
	char reuse_addr = 1;
#endif
	int family;
	struct sockaddr_in addr;
	struct sockaddr_in6 addr6;

	if (strchr(ips->local_ip, ':')) {
		family = PF_INET6;
	} else {
		family = PF_INET;
	}

	if ((sock = socket(family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		die_errno("Socket Error!");
	}

	if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0 ) {
		die_errno("Socket setsockopt Error!");
	}

	if (family == PF_INET) {
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ips->local_ip);
		addr.sin_port = htons(ips->local_port);
		if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			die_errno("Bind Error!");
		}
	} else {
		memset(&addr6, 0, sizeof(addr6));
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(ips->local_port);
		inet_pton(AF_INET6, ips->local_ip, &(addr6.sin6_addr));
		if (bind(sock, (struct sockaddr *) &addr6, sizeof(addr6)) < 0) {
			die_errno("Bind Error!");
		}
	}

    if (listen(sock, MAXPENDING) < 0) {
		die_errno("Listen error");
	}

	ips->family = family;

	return sock;

 error:

	close_file(&sock);

	return ws_sock_invalid;
}

static void handle_mcast_sub(verto_profile_t *profile)
{
	int bytes;

	if (profile->mcast_sub.sock == ws_sock_invalid) {
		return;
	}

	bytes = (int)mcast_socket_recv(&profile->mcast_sub, NULL, 0, 0);

	if (bytes > 0) {
		cJSON *json;

		profile->mcast_sub.buffer[bytes] = '\0';

		if ((json = cJSON_Parse((char *)profile->mcast_sub.buffer))) {
			jsock_send_event(json);
			cJSON_Delete(json);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s MCAST JSON PARSE ERR: %s\n", profile->name, (char *)profile->mcast_sub.buffer);
		}

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s MCAST INVALID READ %d\n", profile->name, bytes);
	}

}

static int profile_one_loop(verto_profile_t *profile)
{
	switch_waitlist_t pfds[MAX_BIND+4];
	int res, x = 0;
	int i = 0;
	int max = 2;

	memset(&pfds[0], 0, sizeof(pfds[0]) * MAX_BIND+2);

	for (i = 0; i < profile->i; i++)  {
		pfds[i].sock = profile->server_socket[i];
		pfds[i].events = SWITCH_POLL_READ|SWITCH_POLL_ERROR;
	}

	if (profile->mcast_ip) {
		pfds[i].sock = profile->mcast_sub.sock;
		pfds[i++].events = SWITCH_POLL_READ|SWITCH_POLL_ERROR;
	}

	max = i;

	if ((res = switch_wait_socklist(pfds, max, 100)) < 0) {
		if (errno != EINTR) {
			die_errnof("%s POLL FAILED with %d", profile->name, res);
		}
		return 0;
	}

	if (res == 0) {
		return 0;
	}

	for (x = 0; x < max; x++) {
		if (pfds[x].revents & SWITCH_POLL_HUP) { log_and_exit(SWITCH_LOG_INFO, "%s POLL HANGUP DETECTED (peer closed its end of socket)\n", profile->name); }
		if (pfds[x].revents & SWITCH_POLL_ERROR) { die("%s POLL ERROR\n", profile->name); }
		if (pfds[x].revents & SWITCH_POLL_INVALID) { die("%s POLL INVALID SOCKET (not opened or already closed)\n", profile->name); }
		if (pfds[x].revents & SWITCH_POLL_READ) {
			if (profile->mcast_ip && pfds[x].sock == (switch_os_socket_t)profile->mcast_sub.sock) {
				handle_mcast_sub(profile);
			} else {
				start_jsock(profile, pfds[x].sock, profile->ip[x].family);
			}
		}
	}

	return res;

 error:
	return -1;
}


static void kill_profile(verto_profile_t *profile)
{
	jsock_t *p;
	verto_vhost_t *h;
	int i;

	profile->running = 0;

	//if (switch_thread_rwlock_tryrdlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
	//	return;
	//}

	switch_mutex_lock(profile->mutex);
	for (i = 0; i < profile->i; i++) {
		close_socket(&profile->server_socket[i]);
	}

	for(p = profile->jsock_head; p; p = p->next) {
		close_socket(&p->client_socket);
	}

	h = profile->vhosts;
	while(h) {
		if (h->rewrites) {
			switch_event_destroy(&h->rewrites);
		}

		h = h->next;
	}

	switch_mutex_unlock(profile->mutex);



	//switch_thread_rwlock_unlock(profile->rwlock);
}

static void kill_profiles(void)
{
	verto_profile_t *pp;
	int sanity = 50;

	switch_mutex_lock(verto_globals.mutex);
	for(pp = verto_globals.profile_head; pp; pp = pp->next) {
		kill_profile(pp);
	}
	switch_mutex_unlock(verto_globals.mutex);


	while(--sanity > 0 && verto_globals.profile_threads > 0) {
		switch_yield(100000);
	}
}


static void runtime(verto_profile_t *profile)
{
	int i;
	int listeners = 0;

	for (i = 0; i < profile->i; i++) {
		//if ((profile->server_socket[i] = prepare_socket(profile->ip[i].local_ip_addr, profile->ip[i].local_port)) < 0) {
		if ((profile->server_socket[i] = prepare_socket(&profile->ip[i])) != ws_sock_invalid) {
			listeners++;
		}
	}

	if (!listeners) {
		die("%s Client Socket Error! No Listeners!\n", profile->name);
	}

	if (profile->mcast_ip) {
		int ok = 1;

		if (mcast_socket_create(profile->mcast_ip, profile->mcast_port, &profile->mcast_sub, MCAST_RECV | MCAST_TTL_HOST) < 0) {
			ok++;
		}

		if (mcast_socket_create(profile->mcast_ip, profile->mcast_port + 1, &profile->mcast_pub, MCAST_SEND | MCAST_TTL_HOST) > 0) {
			mcast_socket_close(&profile->mcast_sub);
			ok = 0;
		}

		if (ok) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s MCAST Bound to %s:%d/%d\n", profile->name, profile->mcast_ip, profile->mcast_port, profile->mcast_port + 1);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s MCAST Disabled\n", profile->name);
		}
	}


	while(profile->running) {
		if (profile_one_loop(profile) < 0) {
			goto error;
		}
	}

 error:

	if (profile->mcast_sub.sock != ws_sock_invalid) {
		mcast_socket_close(&profile->mcast_sub);
	}

	if (profile->mcast_pub.sock != ws_sock_invalid) {
		mcast_socket_close(&profile->mcast_pub);
	}

}

static void do_shutdown(void)
{
	verto_globals.running = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Shutting down (SIG %d)\n", verto_globals.sig);

	kill_profiles();

	unsub_all_jsock();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Done\n");
}


static void parse_ip(char *host, switch_size_t host_len, uint16_t *port, char *input)
{
	char *p;
	//struct hostent *hent;

	if ((p = strchr(input, '['))) {
		char *end = switch_find_end_paren(p, '[', ']');
		if (end) {
			p++;
			strncpy(host, p, end - p);
			if (*(end+1) == ':' && end + 2 < end_of_p(input)) {
				end += 2;
				if (*end) {
					*port = (uint16_t)atoi(end);
				}
			}
		} else {
			strncpy(host, "::", host_len);
		}
	} else {
		strncpy(host, input, host_len);
		if ((p = strrchr(host, ':')) != NULL) {
			*p++  = '\0';
			*port = (uint16_t)atoi(p);
		}
	}

#if 0
	if ( host[0] < '0' || host[0] > '9' ) {
		// Non-numeric host (at least it doesn't start with one).  Convert it to ip addr first
		if ((hent = gethostbyname(host)) != NULL) {
			if (hent->h_addrtype == AF_INET) {
				memcpy(addr, hent->h_addr_list[0], 4);
			}
		}

	} else {
		*addr = inet_addr(host);
	}
#endif
}


static verto_profile_t *find_profile(const char *name)
{
	verto_profile_t *p, *r = NULL;
	switch_mutex_lock(verto_globals.mutex);
	for(p = verto_globals.profile_head; p; p = p->next) {
		if (!strcmp(name, p->name)) {
			r = p;
			break;
		}
	}

	if (r && (!r->in_thread || !r->running)) {
		r = NULL;
	}

	if (r && switch_thread_rwlock_tryrdlock(r->rwlock) != SWITCH_STATUS_SUCCESS) {
		r = NULL;
	}
	switch_mutex_unlock(verto_globals.mutex);

	return r;
}

static switch_bool_t profile_exists(const char *name)
{
	switch_bool_t r = SWITCH_FALSE;
	verto_profile_t *p;

	switch_mutex_lock(verto_globals.mutex);
	for(p = verto_globals.profile_head; p; p = p->next) {
		if (!strcmp(p->name, name)) {
			r = SWITCH_TRUE;
			break;
		}
	}
	switch_mutex_unlock(verto_globals.mutex);

	return r;
}

static void del_profile(verto_profile_t *profile)
{
	verto_profile_t *p, *last = NULL;

	switch_mutex_lock(verto_globals.mutex);
	for(p = verto_globals.profile_head; p; p = p->next) {
		if (p == profile) {
			if (last) {
				last->next = p->next;
			} else {
				verto_globals.profile_head = p->next;
			}
			verto_globals.profile_count--;
			break;
		}

		last = p;
	}
	switch_mutex_unlock(verto_globals.mutex);
}

static switch_status_t add_profile(verto_profile_t *profile)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(verto_globals.mutex);

	if (!profile_exists(profile->name)) {
		status = SWITCH_STATUS_SUCCESS;
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		profile->next = verto_globals.profile_head;
		verto_globals.profile_head = profile;
		verto_globals.profile_count++;
	}

	switch_mutex_unlock(verto_globals.mutex);

	return status;
}

static switch_status_t parse_config(const char *cf)
{

	switch_xml_t cfg, xml, settings, param, xprofile, xprofiles;
	switch_xml_t xvhosts, xvhost, rewrites, rule;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((xprofiles = switch_xml_child(cfg, "profiles"))) {
		for (xprofile = switch_xml_child(xprofiles, "profile"); xprofile; xprofile = xprofile->next) {
			verto_profile_t *profile;
			switch_memory_pool_t *pool;
			const char *name = switch_xml_attr(xprofile, "name");

			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Required field name missing\n");
				continue;
			}

			if (profile_exists(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile %s already exists\n", name);
				continue;
			}


			switch_core_new_memory_pool(&pool);
			profile = switch_core_alloc(pool, sizeof(*profile));
			profile->pool = pool;
			profile->name = switch_core_strdup(profile->pool, name);
			switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, profile->pool);
			switch_thread_rwlock_create(&profile->rwlock, profile->pool);
			add_profile(profile);

			profile->local_network = "localnet.auto";

			profile->mcast_sub.sock = ws_sock_invalid;
			profile->mcast_pub.sock = ws_sock_invalid;


			for (param = switch_xml_child(xprofile, "param"); param; param = param->next) {
				char *var = NULL;
				char *val = NULL;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "bind-local")) {
					const char *secure = switch_xml_attr_soft(param, "secure");
					if (profile->i < MAX_BIND) {
						parse_ip(profile->ip[profile->i].local_ip, sizeof(profile->ip[profile->i].local_ip), &profile->ip[profile->i].local_port, val);
						if (switch_true(secure)) {
							profile->ip[profile->i].secure = 1;
						}
						profile->i++;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max Bindings Reached!\n");
					}
				} else if (!strcasecmp(var, "enable-text")) {
					profile->enable_text = 1;
				} else if (!strcasecmp(var, "secure-combined")) {
					set_string(profile->cert, val);
					set_string(profile->key, val);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Secure key and cert specified\n");
				} else if (!strcasecmp(var, "secure-cert")) {
					set_string(profile->cert, val);
				} else if (!strcasecmp(var, "secure-key")) {
					set_string(profile->key, val);
				} else if (!strcasecmp(var, "secure-chain")) {
					set_string(profile->chain, val);
				} else if (!strcasecmp(var, "inbound-codec-string") && !zstr(val)) {
					profile->inbound_codec_string = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "outbound-codec-string") && !zstr(val)) {
					profile->outbound_codec_string = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "auto-jitterbuffer-msec") && !zstr(val)) {
					profile->jb_msec = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "blind-reg") && !zstr(val)) {
					profile->blind_reg = switch_true(val);
				} else if (!strcasecmp(var, "userauth") && !zstr(val)) {
					profile->userauth = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "root-password") && !zstr(val)) {
					profile->root_passwd = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "context") && !zstr(val)) {
					profile->context = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "dialplan") && !zstr(val)) {
					profile->dialplan = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "mcast-ip") && val) {
					profile->mcast_ip = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "mcast-port") && val) {
					profile->mcast_port = (switch_port_t) atoi(val);
				} else if (!strcasecmp(var, "timer-name") && !zstr(var)) {
					profile->timer_name = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "force-register-domain") && !zstr(val)) {
					profile->register_domain = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "local-network") && !zstr(val)) {
					profile->local_network = switch_core_strdup(profile->pool, val);
				} else if (!strcasecmp(var, "apply-candidate-acl")) {
					if (profile->cand_acl_count < SWITCH_MAX_CAND_ACL) {
						profile->cand_acl[profile->cand_acl_count++] = switch_core_strdup(profile->pool, val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SWITCH_MAX_CAND_ACL);
					}
				} else if (!strcasecmp(var, "apply-connection-acl")) {
					if (profile->conn_acl_count < SWITCH_MAX_CAND_ACL) {
						profile->conn_acl[profile->conn_acl_count++] = switch_core_strdup(profile->pool, val);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Max acl records of %d reached\n", SWITCH_MAX_CAND_ACL);
					}
				} else if (!strcasecmp(var, "rtp-ip")) {
					if (zstr(val)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid RTP IP.\n");
					} else {
						if (strchr(val, ':')) {
							if (profile->rtpip_index6 < MAX_RTPIP -1) {
								profile->rtpip6[profile->rtpip_index6++] = switch_core_strdup(profile->pool, val);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Too many RTP IP.\n");
							}
						} else {
							if (profile->rtpip_index < MAX_RTPIP -1) {
								profile->rtpip[profile->rtpip_index++] = switch_core_strdup(profile->pool, val);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Too many RTP IP.\n");
							}
						}
					}
				} else if (!strcasecmp(var, "ext-rtp-ip")) {
					if (zstr(val)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid External RTP IP.\n");
					} else {
						switch_stun_ip_lookup(&profile->extrtpip, val, profile->pool);
					}
				} else if (!strcasecmp(var, "debug")) {
					if (val) {
						profile->debug = atoi(val);
					}
				}
			}

			if (zstr(profile->outbound_codec_string)) {
				profile->outbound_codec_string = "opus,vp8";
			}

			if (zstr(profile->inbound_codec_string)) {
				profile->inbound_codec_string = profile->outbound_codec_string;
			}

			if (zstr(profile->jb_msec)) {
				profile->jb_msec = "1p:50p";
			}

			if (zstr(profile->timer_name)) {
				profile->timer_name = "soft";
			}

			if (zstr(profile->dialplan)) {
				profile->dialplan = "XML";
			}

			if (zstr(profile->context)) {
				profile->context = "default";
			}

			if (zstr(profile->ip[0].local_ip) ) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: local_ip bad\n", profile->name);
			if (profile->ip[0].local_port <= 0  ) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: local_port bad\n", profile->name);

			if (zstr(profile->ip[0].local_ip) || profile->ip[0].local_port <= 0) {
				del_profile(profile);
				switch_core_destroy_memory_pool(&pool);
			} else {
				int i;

				for (i = 0; i < profile->i; i++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  strchr(profile->ip[i].local_ip, ':') ? "%s Bound to [%s]:%d\n" : "%s Bound to %s:%d\n",
									  profile->name, profile->ip[i].local_ip, profile->ip[i].local_port);
				}
			}

			/* parse vhosts */
			/* WARNNING: Experimental feature, DO NOT use until we remove this warnning!! */
			if ((xvhosts = switch_xml_child(xprofile, "vhosts"))) {
				verto_vhost_t *vhost_tail = NULL;

				for (xvhost = switch_xml_child(xvhosts, "vhost"); xvhost; xvhost = xvhost->next) {
					verto_vhost_t *vhost;
					const char *domain = switch_xml_attr(xvhost, "domain");

					if (zstr(domain)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Required field domain missing\n");
						continue;
					}

					vhost = switch_core_alloc(profile->pool, sizeof(*vhost));
					memset(vhost, 0, sizeof(*vhost));
					vhost->pool = profile->pool;
					vhost->domain = switch_core_strdup(profile->pool, domain);

					if (!vhost_tail) {
						profile->vhosts = vhost;
					} else {
						vhost_tail->next = vhost;
					}

					vhost_tail = vhost;

					for (param = switch_xml_child(xvhost, "param"); param; param = param->next) {
						char *var = NULL;
						char *val = NULL;

						var = (char *) switch_xml_attr_soft(param, "name");
						val = (char *) switch_xml_attr_soft(param, "value");

						if (!strcasecmp(var, "alias")) {
							vhost->alias = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "root")) {
							vhost->root = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "script-root")) {
							vhost->script_root = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "index")) {
							vhost->index = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "auth-realm")) {
							vhost->auth_realm = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "auth-user")) {
							vhost->auth_user = switch_core_strdup(vhost->pool, val);
						} else if (!strcasecmp(var, "auth-pass")) {
							vhost->auth_pass = switch_core_strdup(vhost->pool, val);
						}
					}

					if (zstr(vhost->root)) {
						vhost->root = SWITCH_GLOBAL_dirs.htdocs_dir;
					}

					if (zstr(vhost->script_root)) {
						vhost->script_root = SWITCH_GLOBAL_dirs.script_dir;
					}

					if (zstr(vhost->index)) {
						vhost->index = "index.html";
					}

					if ((rewrites = switch_xml_child(xvhost, "rewrites"))) {
						if (switch_event_create(&vhost->rewrites, SWITCH_EVENT_CLONE) == SWITCH_STATUS_SUCCESS) {
							for (rule = switch_xml_child(rewrites, "rule"); rule; rule = rule->next) {
								char *expr = NULL;
								char *val = NULL;

								expr = (char *) switch_xml_attr_soft(rule, "expression");
								val =  (char *) switch_xml_attr_soft(rule, "value");

								if (zstr(expr)) continue;

								switch_event_add_header_string(vhost->rewrites, SWITCH_STACK_BOTTOM, expr, val);
							}
						}
					} // rewrites
				} // xvhost
			} // xvhosts
		} // xprofile
	} // xprofiles

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;

			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");


			if (!strcasecmp(var, "debug")) {
				if (val) {
					verto_globals.debug = atoi(val);
				}
			} else if (!strcasecmp(var, "enable-presence") && val) {
				verto_globals.enable_presence = switch_true(val);
			} else if (!strcasecmp(var, "enable-fs-events") && val) {
				verto_globals.enable_fs_events = switch_true(val);
			} else if (!strcasecmp(var, "detach-timeout-sec") && val) {
				int tmp = atoi(val);
				if (tmp > 0) {
					verto_globals.detach_timeout = tmp;
				}
			}
		}
	}

	switch_xml_free(xml);

	return status;
}

static int init(void)
{
	verto_profile_t *p;

	parse_config("verto.conf");

	switch_mutex_lock(verto_globals.mutex);
	for(p = verto_globals.profile_head; p; p = p->next) {
		verto_init_ssl(p);
	}
	switch_mutex_unlock(verto_globals.mutex);

	verto_globals.running = 1;

	return 0;
}


#if 0
static void print_status(verto_profile_t *profile, switch_stream_handle_t *stream)
{
	jsock_t *p;

	stream->write_function(stream, "REMOTE\t\t\tLOCAL\n");

	for(p = profile->jsock_head; p; p = p->next) {
		if (p->ptype & PTYPE_CLIENT) {
			int i;

			for (i = 0; i < profile->i; i++) {
				if (profile->server_socket[i] == p->local_sock) {
					stream->write_function(stream, "%s\t%s:%d\n", p->name, profile->ip[i].local_ip, profile->ip[i].local_port);
				}
			}
		}
	}
}
#endif
typedef switch_status_t (*verto_command_t) (char **argv, int argc, switch_stream_handle_t *stream);
static switch_status_t cmd_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	verto_profile_t *profile = NULL;
	jsock_t *jsock;
	verto_vhost_t *vhost;
	int cp = 0;
	int cc = 0;
	const char *line = "=================================================================================================";
	int i;

	stream->write_function(stream, "%25s\t%s\t  %40s\t%s\n", "Name", "   Type", "Data", "State");
	stream->write_function(stream, "%s\n", line);

	switch_mutex_lock(verto_globals.mutex);
	for(profile = verto_globals.profile_head; profile; profile = profile->next) {
		for (i = 0; i < profile->i; i++) {
			char *tmpurl = switch_mprintf(strchr(profile->ip[i].local_ip, ':') ? "%s:[%s]:%d" : "%s:%s:%d",
										  (profile->ip[i].secure == 1) ? "wss" : "ws", profile->ip[i].local_ip, profile->ip[i].local_port);
			stream->write_function(stream, "%25s\t%s\t  %40s\t%s\n", profile->name, "profile", tmpurl, (profile->server_socket[i] != ws_sock_invalid) ? "RUNNING" : "DOWN");
			switch_safe_free(tmpurl);
		}
		cp++;

		switch_mutex_lock(profile->mutex);
		for (vhost = profile->vhosts; vhost; vhost = vhost->next) {
			char *tmpname = switch_mprintf("%s::%s", profile->name, vhost->domain);
			stream->write_function(stream, "%25s\t%s\t  %40s\t%s (%s)\n", tmpname, "vhost", vhost->root, vhost->auth_user ? "AUTH" : "NOAUTH", vhost->auth_user ? vhost->auth_user : "");
			switch_safe_free(tmpname);
		}

		for (jsock = profile->jsock_head; jsock; jsock = jsock->next) {
			char *tmpname = switch_mprintf("%s::%s@%s", profile->name, jsock->id, jsock->domain);
			stream->write_function(stream, "%25s\t%s\t  %40s\t%s (%s)\n", tmpname, "client", jsock->name, (!zstr(jsock->uid)) ? "CONN_REG" : "CONN_NO_REG", (jsock->ptype & PTYPE_CLIENT_SSL) ? "WSS": "WS");
			cc++;
			switch_safe_free(tmpname);
		}
		switch_mutex_unlock(profile->mutex);
	}
	switch_mutex_unlock(verto_globals.mutex);

	stream->write_function(stream, "%s\n", line);
	stream->write_function(stream, "%d profile%s , %d client%s\n", cp, cp == 1 ? "" : "s", cc, cc == 1 ? "" : "s");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t cmd_xml_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	verto_profile_t *profile = NULL;
	jsock_t *jsock;
	int cp = 0;
	int cc = 0;
	const char *header = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	int i;

	stream->write_function(stream, "%s\n", header);
	stream->write_function(stream, "<profiles>\n");
	switch_mutex_lock(verto_globals.mutex);
	for(profile = verto_globals.profile_head; profile; profile = profile->next) {
		for (i = 0; i < profile->i; i++) { 
			char *tmpurl = switch_mprintf(strchr(profile->ip[i].local_ip, ':') ? "%s:[%s]:%d" : "%s:%s:%d",
										  (profile->ip[i].secure == 1) ? "wss" : "ws", profile->ip[i].local_ip, profile->ip[i].local_port);
			stream->write_function(stream, "<profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s</state>\n</profile>\n", profile->name, "profile", tmpurl, (profile->running) ? "RUNNING" : "DOWN");
			switch_safe_free(tmpurl);
		}
		cp++;

		switch_mutex_lock(profile->mutex);
		for(jsock = profile->jsock_head; jsock; jsock = jsock->next) {
			char *tmpname = switch_mprintf("%s@%s", jsock->id, jsock->domain);
			stream->write_function(stream, "<client>\n<profile>%s</profile>\n<name>%s</name>\n<type>%s</type>\n<data>%s</data>\n<state>%s (%s)</state>\n</client>\n", profile->name, tmpname, "client", jsock->name,
									 (!zstr(jsock->uid)) ? "CONN_REG" : "CONN_NO_REG",  (jsock->ptype & PTYPE_CLIENT_SSL) ? "WSS": "WS");
			cc++;
			switch_safe_free(tmpname);
		}
		switch_mutex_unlock(profile->mutex);
	}
	switch_mutex_unlock(verto_globals.mutex);
	stream->write_function(stream, "</profiles>\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(verto_function)
{
	char *argv[1024] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	verto_command_t func = NULL;
	int lead = 1;
	static const char usage_string[] = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"verto [status|xmlstatus]\n"
		"verto help\n"
		"--------------------------------------------------------------------------------\n";

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strcasecmp(argv[0], "help")) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	} else if (!strcasecmp(argv[0], "status")) {
		func = cmd_status;
	} else if (!strcasecmp(argv[0], "xmlstatus")) {
		func = cmd_xml_status;
	}

	if (func) {
		status = func(&argv[lead], argc - lead, stream);
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

  done:
	switch_safe_free(mycmd);
	return status;

}



static void *SWITCH_THREAD_FUNC profile_thread(switch_thread_t *thread, void *obj)
{
	verto_profile_t *profile = (verto_profile_t *) obj;
	int sanity = 50;

	switch_mutex_lock(verto_globals.mutex);
	verto_globals.profile_threads++;
	switch_mutex_unlock(verto_globals.mutex);

	profile->in_thread = 1;
	profile->running = 1;


	runtime(profile);
	profile->running = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "profile %s shutdown, Waiting for %d threads\n", profile->name, profile->jsock_count);

	while(--sanity > 0 && profile->jsock_count > 0) {
		switch_yield(100000);
	}

	verto_deinit_ssl(profile);

	del_profile(profile);

	switch_thread_rwlock_wrlock(profile->rwlock);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Thread ending\n", profile->name);
	switch_thread_rwlock_unlock(profile->rwlock);
	profile->in_thread = 0;

	switch_mutex_lock(verto_globals.mutex);
	verto_globals.profile_threads--;
	switch_mutex_unlock(verto_globals.mutex);

	return NULL;

}

static void run_profile_thread(verto_profile_t *profile) {
	switch_thread_data_t *td;

	td = switch_core_alloc(profile->pool, sizeof(*td));

	td->alloc = 0;
	td->func = profile_thread;
	td->obj = profile;
	td->pool = profile->pool;

	switch_thread_pool_launch_thread(&td);
}

static void run_profiles(void)
{
	verto_profile_t *p;

	switch_mutex_lock(verto_globals.mutex);
	for(p = verto_globals.profile_head; p; p = p->next) {
		if (!p->in_thread) {
			run_profile_thread(p);
		}
	}
	switch_mutex_unlock(verto_globals.mutex);

}


//// ENDPOINT


static switch_call_cause_t verto_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session,
												  switch_memory_pool_t **pool,
												  switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause);
switch_io_routines_t verto_io_routines = {
	/*.outgoing_channel */ verto_outgoing_channel
};


switch_io_routines_t verto_io_override = {
	/*.outgoing_channel */ NULL,
	/*.read_frame */ NULL,
	/*.write_frame */ NULL,
	/*.kill_channel */ NULL,
	/*.send_dtmf */ NULL,
	/*.receive_message */ NULL,
	/*.receive_event */ NULL,
	/*.state_change */ NULL,
	/*.read_video_frame */ NULL,
	/*.write_video_frame */ NULL,
	/*.read_text_frame */ verto_read_text_frame,
	/*.write_text_frame */ verto_write_text_frame,
	/*.state_run*/ NULL,
	/*.get_jb*/ NULL
};


static switch_status_t verto_read_text_frame(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);
	switch_status_t status;

	if (!tech_pvt->text_read_buffer) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(tech_pvt->text_cond_mutex);

	switch_thread_cond_timedwait(tech_pvt->text_cond, tech_pvt->text_cond_mutex, 100000);
	switch_mutex_unlock(tech_pvt->text_cond_mutex);

	*frame = &tech_pvt->text_read_frame;
	(*frame)->flags = 0;

	switch_mutex_lock(tech_pvt->text_read_mutex);
	if (switch_buffer_inuse(tech_pvt->text_read_buffer)) {
		status = SWITCH_STATUS_SUCCESS;
		tech_pvt->text_read_frame.datalen = switch_buffer_read(tech_pvt->text_read_buffer, tech_pvt->text_read_frame.data, 100);
	} else {
		(*frame)->flags |= SFF_CNG;
		tech_pvt->text_read_frame.datalen = 2;
		status = SWITCH_STATUS_BREAK;
	}
	switch_mutex_unlock(tech_pvt->text_read_mutex);



	return status;
}

static switch_status_t verto_write_text_frame(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	switch_mutex_lock(tech_pvt->text_write_mutex);


	if (frame) {
		switch_buffer_write(tech_pvt->text_write_buffer, frame->data, frame->datalen);
	}

	if (switch_buffer_inuse(tech_pvt->text_write_buffer)) {
		uint32_t datalen;
		switch_byte_t data[SWITCH_RTP_MAX_BUF_LEN] = "";

		if ((datalen = switch_buffer_read(tech_pvt->text_write_buffer, data, 100))) {
			cJSON *obj = NULL, *txt = NULL, *params = NULL;
			jsock_t *jsock;

			obj = jrpc_new_req("verto.info", tech_pvt->call_id, &params);
			txt = json_add_child_obj(params, "txt", NULL);
			cJSON_AddItemToObject(txt, "chars", cJSON_CreateString((char *)data));

			if ((jsock = get_jsock(tech_pvt->jsock_uuid))) {
				jsock_queue_event(jsock, &obj, SWITCH_TRUE);
				switch_thread_rwlock_unlock(jsock->rwlock);
			} else {
				cJSON_Delete(obj);
			}
		}
	}


	switch_mutex_unlock(tech_pvt->text_write_mutex);

	return SWITCH_STATUS_SUCCESS;
}



static void set_text_funcs(switch_core_session_t *session)
{
	verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

	if (!tech_pvt || tech_pvt->text_read_buffer) {
		return;
	}

	if ((switch_core_session_override_io_routines(session, &verto_io_override) == SWITCH_STATUS_SUCCESS)) {
		tech_pvt->text_read_frame.data = tech_pvt->text_read_frame_data;

		switch_mutex_init(&tech_pvt->text_read_mutex, SWITCH_MUTEX_NESTED, tech_pvt->pool);
		switch_mutex_init(&tech_pvt->text_write_mutex, SWITCH_MUTEX_NESTED, tech_pvt->pool);
		switch_mutex_init(&tech_pvt->text_cond_mutex, SWITCH_MUTEX_NESTED, tech_pvt->pool);
		switch_thread_cond_create(&tech_pvt->text_cond, tech_pvt->pool);

		switch_buffer_create_dynamic(&tech_pvt->text_read_buffer, 512, 1024, 0);
		switch_buffer_create_dynamic(&tech_pvt->text_write_buffer, 512, 1024, 0);

		switch_channel_set_flag(switch_core_session_get_channel(session), CF_HAS_TEXT);
		switch_core_session_start_text_thread(session);
	}
}


static char *verto_get_dial_string(const char *uid, switch_stream_handle_t *rstream)
{
	jsock_t *jsock;
	verto_profile_t *profile;
	switch_stream_handle_t *use_stream = NULL, stream = { 0 };
	char *gen_uid = NULL;
	int hits = 0;

	if (!strchr(uid, '@')) {
		gen_uid = switch_mprintf("%s@%s", uid, switch_core_get_domain(SWITCH_FALSE));
		uid = gen_uid;
	}

	if (rstream) {
		use_stream = rstream;
	} else {
		SWITCH_STANDARD_STREAM(stream);
		use_stream = &stream;
	}

	switch_mutex_lock(verto_globals.mutex);
	for(profile = verto_globals.profile_head; profile; profile = profile->next) {

		switch_mutex_lock(profile->mutex);

		for(jsock = profile->jsock_head; jsock; jsock = jsock->next) {
			if (jsock->ready && !zstr(jsock->uid) && !zstr(uid) && !strcmp(uid, jsock->uid)) {
				use_stream->write_function(use_stream, "%s/u:%s,", EP_NAME, jsock->uuid_str);
				hits++;
			}
		}

		switch_mutex_unlock(profile->mutex);
	}
	switch_mutex_unlock(verto_globals.mutex);

	switch_safe_free(gen_uid);

	if (!hits) {
		use_stream->write_function(use_stream, "error/user_not_registered");
	}

	if (use_stream->data) {
		char *p = use_stream->data;
		if (end_of(p) == ',') {
			end_of(p) = '\0';
		}
	}

	return use_stream->data;
}

SWITCH_STANDARD_API(verto_contact_function)
{
	char *uid = (char *) cmd;

	if (!zstr(uid)) {
		verto_get_dial_string(uid, stream);
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_call_cause_t verto_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session,
												  switch_memory_pool_t **pool,
												  switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = SWITCH_CAUSE_CHANNEL_UNACCEPTABLE;
	char *dest = NULL;

	PROTECT_INTERFACE(verto_endpoint_interface);

	if (!zstr(outbound_profile->destination_number)) {
		dest = strdup(outbound_profile->destination_number);
	}

	if (zstr(dest)) {
		goto end;
	}

	if (!switch_stristr("u:", dest)) {
		char *dial_str = verto_get_dial_string(dest, NULL);

		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "verto_orig_dest", dest);
		if (zstr(switch_event_get_header(var_event, "origination_callee_id_number"))) {
			char *p;
			char *trimmed_dest = strdup(dest);
			switch_assert(trimmed_dest);
			p = strchr(trimmed_dest, '@');
			if (p) *p = '\0';
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_callee_id_number", trimmed_dest);
			free(trimmed_dest);
		}

		cause = SWITCH_CAUSE_USER_NOT_REGISTERED;

		if (dial_str) {
			switch_originate_flag_t myflags = SOF_NONE;

			if ((flags & SOF_NO_LIMITS)) {
				myflags |= SOF_NO_LIMITS;
			}

			if ((flags & SOF_FORKED_DIAL)) {
				myflags |= SOF_NOBLOCK;
			}

			if (switch_ivr_originate(session, new_session, &cause, dial_str, 0, NULL,
									 NULL, NULL, outbound_profile, var_event, myflags, cancel_cause, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_rwunlock(*new_session);
			}

			free(dial_str);
		}

		return cause;
	} else {
		const char *dialed_user = switch_event_get_header(var_event, "dialed_user");
		const char *dialed_domain = switch_event_get_header(var_event, "dialed_domain");

		if (dialed_user) {
			if (dialed_domain) {
				switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, "verto_orig_dest", "%s@%s", dialed_user, dialed_domain);
			} else {
				switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "verto_orig_dest", dialed_user);
			}
			if (zstr(switch_event_get_header(var_event, "origination_callee_id_number"))) {
				switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_callee_id_number", dialed_user);
				outbound_profile->callee_id_number = switch_sanitize_number(switch_core_strdup(outbound_profile->pool, dialed_user));
			}
		}
	}

	if ((cause = switch_core_session_outgoing_channel(session, var_event, "rtc",
												  outbound_profile, new_session, NULL, SOF_NONE, cancel_cause)) == SWITCH_CAUSE_SUCCESS) {
		switch_channel_t *channel = switch_core_session_get_channel(*new_session);
		char *jsock_uuid_str = outbound_profile->destination_number + 2;
		switch_caller_profile_t *caller_profile;
		verto_pvt_t *tech_pvt = NULL;
		char name[512];

		tech_pvt = switch_core_session_alloc(*new_session, sizeof(*tech_pvt));
		tech_pvt->pool = switch_core_session_get_pool(*new_session);
		tech_pvt->session = *new_session;
		tech_pvt->channel = channel;
		tech_pvt->jsock_uuid = switch_core_session_strdup(*new_session, jsock_uuid_str);

		switch_core_session_set_private_class(*new_session, tech_pvt, SWITCH_PVT_SECONDARY);

		if (session) {
			switch_channel_t *ochannel = switch_core_session_get_channel(session);

			if (switch_true(switch_channel_get_variable(ochannel, SWITCH_BYPASS_MEDIA_VARIABLE))) {
				switch_channel_set_flag(channel, CF_PROXY_MODE);
				switch_channel_set_flag(ochannel, CF_PROXY_MODE);
				switch_channel_set_cap(channel, CC_BYPASS_MEDIA);
			}
		}

		tech_pvt->call_id = switch_core_session_strdup(*new_session, switch_core_session_get_uuid(*new_session));
		if ((tech_pvt->smh = switch_core_session_get_media_handle(*new_session))) {
			tech_pvt->mparams = switch_core_media_get_mparams(tech_pvt->smh);
		}

		switch_snprintf(name, sizeof(name), "verto.rtc/%s", tech_pvt->jsock_uuid);
		switch_channel_set_name(channel, name);
		switch_channel_set_variable(channel, "jsock_uuid_str", tech_pvt->jsock_uuid);
		switch_channel_set_variable(channel, "event_channel_cookie", tech_pvt->jsock_uuid);


		if ((caller_profile = switch_caller_profile_dup(switch_core_session_get_pool(*new_session), outbound_profile))) {
			switch_channel_set_caller_profile(channel, caller_profile);
		}

		switch_channel_add_state_handler(channel, &verto_state_handlers);
		switch_core_event_hook_add_receive_message(*new_session, messagehook);
		switch_channel_set_state(channel, CS_INIT);
		//track_pvt(tech_pvt);
	}

 end:

	if (cause != SWITCH_CAUSE_SUCCESS) {
		UNPROTECT_INTERFACE(verto_endpoint_interface);
	}

	switch_safe_free(dest);

	return cause;
}

void verto_broadcast(const char *event_channel, cJSON *json, const char *key, switch_event_channel_id_t id, void *user_data)
{
	if (verto_globals.debug > 9) {
		char *json_text;
		if ((json_text = cJSON_Print(json))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "EVENT BROADCAST %s %s\n", event_channel, json_text);
			free(json_text);
		}
	}

	jsock_send_event(json);
}


static int verto_send_chat(const char *uid, const char *call_id, cJSON *msg)
{
	jsock_t *jsock;
	verto_profile_t *profile;
	int hits = 0;
	int done = 0;

	if (!strchr(uid, '@')) {
		return 0;
	}

	if (call_id) {
		switch_core_session_t *session;
		if ((session = switch_core_session_locate(call_id))) {
			verto_pvt_t *tech_pvt = switch_core_session_get_private_class(session, SWITCH_PVT_SECONDARY);

			if ((jsock = get_jsock(tech_pvt->jsock_uuid))) {
				jsock_queue_event(jsock, &msg, SWITCH_FALSE);
				//if (ws_write_json(jsock, &msg, SWITCH_FALSE) <= 0) {
				//	switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				//}
				switch_thread_rwlock_unlock(jsock->rwlock);
				done = 1;
			}

			switch_core_session_rwunlock(session);
		}
	}

	if (done) {
		return 1;
	}

	switch_mutex_lock(verto_globals.mutex);
	for(profile = verto_globals.profile_head; profile; profile = profile->next) {
	
		switch_mutex_lock(profile->mutex);

		for(jsock = profile->jsock_head; jsock; jsock = jsock->next) {
			if (jsock->ready && !zstr(jsock->uid) && !strcmp(uid, jsock->uid)) {
				jsock_queue_event(jsock, &msg, SWITCH_FALSE);
				hits++;
			}
		}

		switch_mutex_unlock(profile->mutex);
	}
	switch_mutex_unlock(verto_globals.mutex);

	return hits;
}

static switch_status_t chat_send(switch_event_t *message_event)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *to = switch_event_get_header(message_event, "to");
	const char *from = switch_event_get_header(message_event, "from");
	const char *body = switch_event_get_body(message_event);
	const char *call_id = switch_event_get_header(message_event, "call_id");

	//DUMP_EVENT(message_event);


	if (!zstr(to) && !zstr(body) && !zstr(from)) {
		cJSON *obj = NULL, *msg = NULL, *params = NULL;
		switch_event_header_t *eh;

		obj = jrpc_new_req("verto.info", call_id, &params);
		msg = json_add_child_obj(params, "msg", NULL);

		cJSON_AddItemToObject(msg, "from", cJSON_CreateString(from));
		cJSON_AddItemToObject(msg, "to", cJSON_CreateString(to));
		cJSON_AddItemToObject(msg, "body", cJSON_CreateString(body));

		for (eh = message_event->headers; eh; eh = eh->next) {
			if (!strncasecmp(eh->name, "from_", 5) || !strncasecmp(eh->name, "to_", 3)) {
				cJSON_AddItemToObject(msg, eh->name, cJSON_CreateString(eh->value));
			}
		}

		verto_send_chat(to, call_id, obj);
		cJSON_Delete(obj);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "INVALID EVENT\n");
		DUMP_EVENT(message_event);
		status = SWITCH_STATUS_FALSE;
	}


	return status;
}



static switch_cache_db_handle_t *json_get_db_handle(void)
{

	switch_cache_db_handle_t *dbh = NULL;
	const char *dsn;


	if (!(dsn = switch_core_get_variable("json_db_handle"))) {
		dsn = "json";
	}


	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}

	return dbh;
}


static int jcallback(void *pArg, int argc, char **argv, char **columnNames)
{
	char **data = (char **) pArg;

	if (argv[0] && !*data) {
		*data = strdup(argv[0]);
	}

	return 0;
}

static cJSON *json_retrieve(const char *name, switch_mutex_t *mutex)
{
	char *sql, *errmsg;
	switch_cache_db_handle_t *dbh;
	char *ascii = NULL;
	cJSON *json = NULL;

	if (!check_name(name)) {
		return NULL;
	}

	sql = switch_mprintf("select data from json_store where name='%q'", name);

	dbh = json_get_db_handle();

	if (mutex) switch_mutex_lock(mutex);
	switch_cache_db_execute_sql_callback(dbh, sql, jcallback, &ascii, &errmsg);

	switch_cache_db_release_db_handle(&dbh);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	} else {
		if (ascii) {
			json = cJSON_Parse(ascii);
		}
	}

	if (mutex) switch_mutex_unlock(mutex);


	switch_safe_free(ascii);

	return json;

}

static switch_bool_t json_commit(cJSON *json, const char *name, switch_mutex_t *mutex)
{
	char *ascii;
	char *sql;
	char del_sql[128] = "";
	switch_cache_db_handle_t *dbh;
	char *err;

	if (!check_name(name)) {
		return SWITCH_FALSE;
	}

	if (!(ascii = cJSON_PrintUnformatted(json))) {
		return SWITCH_FALSE;
	}


	sql = switch_mprintf("insert into json_store (name,data) values('%q','%q')", name, ascii);
	switch_snprintfv(del_sql, sizeof(del_sql), "delete from json_store where name='%q'", name);

	dbh = json_get_db_handle();


	if (mutex) switch_mutex_lock(mutex);
	switch_cache_db_execute_sql(dbh, del_sql, &err);

	if (err) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sql err [%s]\n", err);
		free(err);
	} else {
		switch_cache_db_execute_sql(dbh, sql, &err);

		if (err) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sql err [%s]\n", err);
			free(err);
		}
	}

	if (mutex) switch_mutex_unlock(mutex);

	switch_safe_free(sql);
	switch_safe_free(ascii);

	switch_cache_db_release_db_handle(&dbh);

	return SWITCH_TRUE;
}

static switch_status_t json_hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	json_store_t *session_store = NULL;
	char *ascii = NULL;

	if (state == CS_HANGUP) {
		if ((session_store = (json_store_t *) switch_channel_get_private(channel, "_json_store_"))) {
			if ((ascii = cJSON_PrintUnformatted(session_store->JSON_STORE))) {
				switch_channel_set_variable(channel, "json_store_data", ascii);
				free(ascii);
			}
			cJSON_Delete(session_store->JSON_STORE);
			session_store->JSON_STORE = NULL;
			switch_channel_set_private(channel, "_json_store_", NULL);
		}
		switch_core_event_hook_remove_state_change(session, json_hanguphook);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_JSON_API(json_store_function)
{
	cJSON *JSON_STORE = NULL, *reply = NULL, *data = cJSON_GetObjectItem(json, "data");
	switch_status_t status = SWITCH_STATUS_FALSE;
	const char *cmd_attr = cJSON_GetObjectCstr(data, "cmd");
	const char *uuid = cJSON_GetObjectCstr(data, "uuid");
	const char *error = NULL, *message = NULL;
	store_cmd_t cmd;
	const char *key = cJSON_GetObjectCstr(data, "key");
	const char *verbose = cJSON_GetObjectCstr(data, "verbose");
	const char *commit = cJSON_GetObjectCstr(data, "commit");
	const char *file = cJSON_GetObjectCstr(data, "file");
	const char *storename = cJSON_GetObjectCstr(data, "storeName");
	cJSON *obj, **use_store = NULL;
	switch_core_session_t *tsession = NULL;
	switch_channel_t *tchannel = NULL;
	json_store_t *session_store = NULL;

	reply = cJSON_CreateObject();

	if (uuid) {
		if ((tsession = switch_core_session_locate(uuid))) {
			tchannel = switch_core_session_get_channel(tsession);
		} else {
			error = "Invalid INPUT, Missing UUID";
			goto end;
		}
	} else {
		if (zstr(storename)) {
			storename = "global";
		}
	}


	if (zstr(cmd_attr)) {
		error = "INVALID INPUT, Command not supplied";
		goto end;
	}


	if (!strcasecmp(cmd_attr, "add")) {
		cmd = CMD_ADD;
	} else if (!strcasecmp(cmd_attr, "del")) {
		cmd = CMD_DEL;
	} else if (!strcasecmp(cmd_attr, "dump")) {
		cmd = CMD_DUMP;
	} else if (!strcasecmp(cmd_attr, "commit")) {
		cmd = CMD_COMMIT;
	} else if (!strcasecmp(cmd_attr, "retrieve")) {
		cmd = CMD_RETRIEVE;
	} else {
		error = "INVALID INPUT, Unknown Command";
		goto end;
	}


	if (cmd == CMD_ADD) {
		if (zstr(key)) {
			error = "INVALID INPUT, No key supplied";
			goto end;
		}
	}


	if (cmd == CMD_RETRIEVE || cmd == CMD_COMMIT) {
		if (zstr(file)) {
			error = "INVALID INPUT, No file specified";
			goto end;
		}
	}

	switch_mutex_lock(json_GLOBALS.store_mutex);
	if (tsession) {
		if (!(session_store = (json_store_t *) switch_channel_get_private(tchannel, "_json_store_"))) {
			session_store = switch_core_session_alloc(tsession, sizeof(*session_store));
			switch_mutex_init(&session_store->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(tsession));
			session_store->JSON_STORE = cJSON_CreateObject();
			switch_channel_set_private(tchannel, "_json_store_", session_store);
			switch_core_event_hook_add_state_change(tsession, json_hanguphook);
		}

		use_store = &session_store->JSON_STORE;
		switch_mutex_lock(session_store->mutex);
		switch_mutex_unlock(json_GLOBALS.store_mutex);
	} else {
		JSON_STORE = switch_core_hash_find(json_GLOBALS.store_hash, storename);

		if (!JSON_STORE) {
			JSON_STORE = cJSON_CreateObject();
			switch_core_hash_insert(json_GLOBALS.store_hash, storename, JSON_STORE);
		}
		use_store = &JSON_STORE;
	}

	switch(cmd) {
	case CMD_RETRIEVE:
		obj = json_retrieve(file, NULL);

		if (!obj) {
			error = "CANNOT LOAD DATA";

			if (session_store) {
				switch_mutex_unlock(session_store->mutex);
			} else {
				switch_mutex_unlock(json_GLOBALS.store_mutex);
			}

			goto end;
		}

		cJSON_Delete(*use_store);
		*use_store = obj;
		message = "Store Loaded";

		break;
	case CMD_ADD:

		if (!(obj = cJSON_GetObjectItem(data, key))) {
			error = "INVALID INPUT";

			if (session_store) {
				switch_mutex_unlock(session_store->mutex);
			} else {
				switch_mutex_unlock(json_GLOBALS.store_mutex);
			}

			goto end;
		}

		cJSON_DeleteItemFromObject(*use_store, key);
		obj = cJSON_Duplicate(obj, 1);
		cJSON_AddItemToObject(*use_store, key, obj);
		message = "Item Added";
		break;

	case CMD_DEL:

		if (!key) {
			cJSON_Delete(*use_store);
			*use_store = cJSON_CreateObject();
			message = "Store Deleted";
		} else {
			cJSON_DeleteItemFromObject(*use_store, key);
			message = "Item Deleted";
		}
		break;

	default:
		break;
	}


	if (switch_true(verbose) || cmd == CMD_DUMP) {
		cJSON *dump;

		if (key) {
			dump = cJSON_GetObjectItem(*use_store, key);
		} else {
			dump = *use_store;
		}

		if (dump) {
			dump = cJSON_Duplicate(dump, 1);
			cJSON_AddItemToObject(reply, "data", dump);
			message = "Data Dumped";
		} else {
			error = "Key not found";
		}
	}

	if (session_store) {
		switch_mutex_unlock(session_store->mutex);
	} else {
		switch_mutex_unlock(json_GLOBALS.store_mutex);
	}

	if (cmd == CMD_COMMIT || commit) {
		switch_bool_t ok;

		if (commit && zstr(file)) {
			file = commit;
		}

		if (session_store) {
			ok = json_commit(session_store->JSON_STORE, file, session_store->mutex);
		} else {
			ok = json_commit(JSON_STORE, file, json_GLOBALS.store_mutex);
		}

		cJSON_AddItemToObject(reply, "commitStatus", cJSON_CreateString(ok ? "success" : "fail"));
		if (!message) {
			message = "Message Comitted";
		}
		status = SWITCH_STATUS_SUCCESS;
	}


 end:

	if (!zstr(error)) {
		cJSON_AddItemToObject(reply, "errorMessage", cJSON_CreateString(error));
	}

	if (!zstr(message)) {
		cJSON_AddItemToObject(reply, "message", cJSON_CreateString(message));
		status = SWITCH_STATUS_SUCCESS;
	}

	*json_reply = reply;

	if (tsession) {
		switch_core_session_rwunlock(tsession);
	}

	return status;
}

#define add_it(_name, _ename) if ((tmp = switch_event_get_header(event, _ename))) { cJSON_AddItemToObject(data, _name, cJSON_CreateString(tmp));}

static void presence_event_handler(switch_event_t *event)
{
	cJSON *msg = NULL, *data = NULL;
	const char *tmp;
	switch_event_header_t *hp;
	char *event_channel;
	const char *presence_id = switch_event_get_header(event, "channel-presence-id");

	if (!verto_globals.running) {
		return;
	}

	if (!verto_globals.enable_presence || zstr(presence_id)) {
		return;
	}

	msg = cJSON_CreateObject();
	data = json_add_child_obj(msg, "data", NULL);

	event_channel = switch_mprintf("presence.%s", presence_id);

	cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
	add_it("channelCallState", "channel-call-state");
	add_it("originalChannelCallState", "original-channel-call-state");
	add_it("channelState", "channel-state");

	add_it("callerUserName", "caller-username");
	add_it("callerIDName", "caller-caller-id-name");
	add_it("callerIDNumber", "caller-caller-id-number");
	add_it("calleeIDName", "caller-callee-id-name");
	add_it("calleeIDNumber", "caller-callee-id-number");
	add_it("channelUUID", "unique-id");

	add_it("presenceCallDirection", "presence-call-direction");
	add_it("channelPresenceID", "channel-presence-id");
	add_it("channelPresenceData", "channel-presence-data");

	for(hp = event->headers; hp; hp = hp->next) {
		if (!strncasecmp(hp->name, "PD-", 3)) {
			add_it(hp->name, hp->name);
		}
	}

	switch_event_channel_broadcast(event_channel, &msg, __FILE__, NO_EVENT_CHANNEL_ID);

	free(event_channel);

}

static void event_handler(switch_event_t *event)
{
	cJSON *msg = NULL, *data = NULL;
	char *event_channel;

	if (!verto_globals.enable_fs_events) {
		return;
	}

	switch_event_serialize_json_obj(event, &data);

	msg = cJSON_CreateObject();

	if (event->event_id == SWITCH_EVENT_CUSTOM) {
		const char *subclass = switch_event_get_header(event, "Event-Subclass");
		event_channel = switch_mprintf("FSevent.%s::%s", switch_event_name(event->event_id), subclass);
		switch_tolower_max(event_channel + 8);
	} else {
		event_channel = switch_mprintf("FSevent.%s", switch_event_name(event->event_id));
		switch_tolower_max(event_channel + 8);
	}

	cJSON_AddItemToObject(msg, "eventChannel", cJSON_CreateString(event_channel));
	cJSON_AddItemToObject(msg, "data", data);

	/* comment broadcasting globally and change to only within the module cos FS events are heavy */
	//switch_event_channel_broadcast(event_channel, &msg, __FILE__, NO_EVENT_CHANNEL_ID);
	verto_broadcast(event_channel, msg, __FILE__, NO_EVENT_CHANNEL_ID, NULL);cJSON_Delete(msg);

	free(event_channel);

}

/* Macro expands to: switch_status_t mod_verto_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_verto_load)
{
	switch_api_interface_t *api_interface = NULL;
	switch_chat_interface_t *chat_interface = NULL;
	switch_json_api_interface_t *json_api_interface = NULL;
	int r;
	switch_cache_db_handle_t *dbh;
	//switch_application_interface_t *app_interface = NULL;


	if (switch_event_reserve_subclass(MY_EVENT_LOGIN) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MY_EVENT_LOGIN);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(MY_EVENT_CLIENT_DISCONNECT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MY_EVENT_CLIENT_DISCONNECT);
		return SWITCH_STATUS_TERM;
	}

	if (switch_event_reserve_subclass(MY_EVENT_CLIENT_CONNECT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MY_EVENT_CLIENT_CONNECT);
		return SWITCH_STATUS_TERM;
	}

	memset(&verto_globals, 0, sizeof(verto_globals));
	verto_globals.pool = pool;
#ifndef WIN32
	verto_globals.ready = SIGUSR1;
#endif
	verto_globals.enable_presence = SWITCH_TRUE;
	verto_globals.enable_fs_events = SWITCH_FALSE;

	switch_mutex_init(&verto_globals.mutex, SWITCH_MUTEX_NESTED, verto_globals.pool);

	switch_mutex_init(&verto_globals.method_mutex, SWITCH_MUTEX_NESTED, verto_globals.pool);
	switch_core_hash_init(&verto_globals.method_hash);

	switch_thread_rwlock_create(&verto_globals.event_channel_rwlock, verto_globals.pool);
	switch_core_hash_init(&verto_globals.event_channel_hash);

	switch_mutex_init(&verto_globals.jsock_mutex, SWITCH_MUTEX_NESTED, verto_globals.pool);
	switch_core_hash_init(&verto_globals.jsock_hash);

	switch_thread_rwlock_create(&verto_globals.tech_rwlock, verto_globals.pool);

	switch_mutex_init(&verto_globals.detach_mutex, SWITCH_MUTEX_NESTED, verto_globals.pool);
	switch_mutex_init(&verto_globals.detach2_mutex, SWITCH_MUTEX_NESTED, verto_globals.pool);
	switch_thread_cond_create(&verto_globals.detach_cond, verto_globals.pool);
	verto_globals.detach_timeout = 120;




	memset(&json_GLOBALS, 0, sizeof(json_GLOBALS));
	switch_mutex_init(&json_GLOBALS.store_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&json_GLOBALS.store_hash);


	dbh = json_get_db_handle();
	switch_cache_db_test_reactive(dbh, "select name from json_store where name=''", "drop table json_store", json_sql);
	switch_cache_db_release_db_handle(&dbh);



	switch_event_channel_bind(SWITCH_EVENT_CHANNEL_GLOBAL, verto_broadcast, &verto_globals.event_channel_id, NULL);


	r = init();

	if (r) return SWITCH_STATUS_TERM;

	jrpc_init();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "verto", "Verto API", verto_function, "syntax");
	SWITCH_ADD_API(api_interface, "verto_contact", "Generate a verto endpoint dialstring", verto_contact_function, "user@domain");
	switch_console_set_complete("add verto help");
	switch_console_set_complete("add verto status");
	switch_console_set_complete("add verto xmlstatus");

	SWITCH_ADD_JSON_API(json_api_interface, "store", "JSON store", json_store_function, "");

	verto_endpoint_interface = (switch_endpoint_interface_t *) switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	verto_endpoint_interface->interface_name = EP_NAME;
	verto_endpoint_interface->io_routines = &verto_io_routines;

	SWITCH_ADD_CHAT(chat_interface, VERTO_CHAT_PROTO, chat_send);

	switch_core_register_secondary_recover_callback(modname, verto_recover_callback);

	if (verto_globals.enable_presence) {
		switch_event_bind(modname, SWITCH_EVENT_CHANNEL_CALLSTATE, SWITCH_EVENT_SUBCLASS_ANY, presence_event_handler, NULL);
	}

	if (verto_globals.enable_fs_events) {
		if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
			return SWITCH_STATUS_GENERR;
		}
	}

	run_profiles();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_verto_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_verto_shutdown)
{

	switch_event_free_subclass(MY_EVENT_LOGIN);
	switch_event_free_subclass(MY_EVENT_CLIENT_DISCONNECT);
	switch_event_free_subclass(MY_EVENT_CLIENT_CONNECT);

	json_cleanup();
	switch_core_hash_destroy(&json_GLOBALS.store_hash);

	switch_event_channel_unbind(NULL, verto_broadcast, NULL);
	switch_event_unbind_callback(presence_event_handler);
	switch_event_unbind_callback(event_handler);

	switch_core_unregister_secondary_recover_callback(modname);
	do_shutdown();
	attach_wake();
	attach_wake();

	switch_core_hash_destroy(&verto_globals.method_hash);
	switch_core_hash_destroy(&verto_globals.event_channel_hash);
	switch_core_hash_destroy(&verto_globals.jsock_hash);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_verto_runtime)
{
	switch_mutex_lock(verto_globals.detach_mutex);

	while(verto_globals.running) {
		if (verto_globals.detached) {
			drop_detached();
			switch_yield(1000000);
		} else {
			switch_mutex_lock(verto_globals.detach2_mutex);
			if (verto_globals.running) {
				switch_thread_cond_wait(verto_globals.detach_cond, verto_globals.detach_mutex);
			}
			switch_mutex_unlock(verto_globals.detach2_mutex);
		}
	}

	switch_mutex_unlock(verto_globals.detach_mutex);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
