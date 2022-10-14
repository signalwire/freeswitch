/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2017, Seven Du <dujinfang@gmail.com>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 *
 * msrp.c -- MSRP lib
 *
 */

#include <switch.h>
#include <switch_ssl.h>
#include <switch_msrp.h>
#include <switch_stun.h>

#define MSRP_BUFF_SIZE (SWITCH_RTP_MAX_BUF_LEN - 32)
#define DEBUG_MSRP 0

struct msrp_socket_s {
	switch_port_t port;
	switch_socket_t *sock;
	switch_thread_t *thread;
	int secure;
};

struct msrp_client_socket_s {
	switch_socket_t *sock;
	SSL *ssl;
	int secure;
	int client_mode;
	struct switch_msrp_session_s *msrp_session;
};

static struct {
	int running;
	int debug;
	switch_memory_pool_t *pool;
	// switch_mutex_t *mutex;
	char *ip;
	int message_buffer_size;

	char *cert;
	char *key;
	const SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	int ssl_ready;
	const SSL_METHOD *ssl_client_method;
	SSL_CTX *ssl_client_ctx;

	switch_msrp_socket_t msock;
	switch_msrp_socket_t msock_ssl;
} globals;

typedef struct worker_helper{
	int debug;
	switch_memory_pool_t *pool;
	switch_msrp_client_socket_t csock;
	switch_msrp_session_t *msrp_session;
} worker_helper_t;

SWITCH_DECLARE(void) switch_msrp_msg_set_payload(switch_msrp_msg_t *msrp_msg, const char *buf, switch_size_t payload_bytes)
{
	if (!msrp_msg->payload) {
		switch_malloc(msrp_msg->payload, payload_bytes + 1);
	} else if (msrp_msg->payload_bytes < payload_bytes + 1) {
		msrp_msg->payload = realloc(msrp_msg->payload, payload_bytes + 1);
	}

	switch_assert(msrp_msg->payload);
	memcpy(msrp_msg->payload, buf, payload_bytes);
	*(msrp_msg->payload + payload_bytes) = '\0';
	msrp_msg->payload_bytes = payload_bytes;
}

static switch_bool_t msrp_check_success_report(switch_msrp_msg_t *msrp_msg)
{
	const char *msrp_h_success_report = switch_msrp_msg_get_header(msrp_msg, MSRP_H_SUCCESS_REPORT);
	return (msrp_h_success_report && !strcmp(msrp_h_success_report, "yes"));
}

static void msrp_deinit_ssl()
{
	globals.ssl_ready = 0;
	if (globals.ssl_ctx) {
		SSL_CTX_free(globals.ssl_ctx);
		globals.ssl_ctx = NULL;
	}
	if (globals.ssl_client_ctx) {
		SSL_CTX_free(globals.ssl_client_ctx);
		globals.ssl_client_ctx = NULL;
	}
}

static void msrp_init_ssl()
{
	const char *err = "";

	globals.ssl_client_method = SSLv23_client_method();
	globals.ssl_client_ctx = SSL_CTX_new(globals.ssl_client_method);
	assert(globals.ssl_client_ctx);
	SSL_CTX_set_options(globals.ssl_client_ctx, SSL_OP_NO_SSLv2);

	globals.ssl_method = SSLv23_server_method();   /* create server instance */
	globals.ssl_ctx = SSL_CTX_new(globals.ssl_method);    /* create context */
	assert(globals.ssl_ctx);
	globals.ssl_ready = 1;

	/* Disable SSLv2 */
	SSL_CTX_set_options(globals.ssl_ctx, SSL_OP_NO_SSLv2);
	/* Disable SSLv3 */
	SSL_CTX_set_options(globals.ssl_ctx, SSL_OP_NO_SSLv3);
	/* Disable TLSv1 */
	SSL_CTX_set_options(globals.ssl_ctx, SSL_OP_NO_TLSv1);
	/* Disable Compression CRIME (Compression Ratio Info-leak Made Easy) */
	SSL_CTX_set_options(globals.ssl_ctx, SSL_OP_NO_COMPRESSION);

	// /* set the local certificate from CertFile */
	// if (!zstr(profile->chain)) {
	// 	if (switch_file_exists(profile->chain, NULL) != SWITCH_STATUS_SUCCESS) {
	// 		err = "SUPPLIED CHAIN FILE NOT FOUND\n";
	// 		goto fail;
	// 	}

	// 	if (!SSL_CTX_use_certificate_chain_file(profile->ssl_ctx, profile->chain)) {
	// 		err = "CERT CHAIN FILE ERROR";
	// 		goto fail;
	// 	}
	// }

	if (switch_file_exists(globals.cert, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "SUPPLIED CERT FILE NOT FOUND\n";
		goto fail;
	}

	if (!SSL_CTX_use_certificate_file(globals.ssl_ctx, globals.cert, SSL_FILETYPE_PEM)) {
		err = "CERT FILE ERROR";
		goto fail;
	}

	/* set the private key from KeyFile */

	if (switch_file_exists(globals.key, NULL) != SWITCH_STATUS_SUCCESS) {
		err = "SUPPLIED KEY FILE NOT FOUND\n";
		goto fail;
	}

	if (!SSL_CTX_use_PrivateKey_file(globals.ssl_ctx, globals.key, SSL_FILETYPE_PEM)) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	/* verify private key */
	if ( !SSL_CTX_check_private_key(globals.ssl_ctx) ) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	SSL_CTX_set_cipher_list(globals.ssl_ctx, "HIGH:!DSS:!aNULL@STRENGTH");

	return;

 fail:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SSL ERR: %s\n", err);
	msrp_deinit_ssl();
}

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_ip, globals.ip);

static switch_status_t load_config()
{
	char *cf = "msrp.conf";
	switch_xml_t cfg, xml = NULL, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	globals.cert = switch_core_sprintf(globals.pool, "%s%swss.pem", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR);
	globals.key = globals.cert;

	if ( switch_file_exists(globals.key, globals.pool) != SWITCH_STATUS_SUCCESS ) {
		switch_core_gen_certs(globals.key);
	}

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		return status;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "listen-ip")) {
				set_global_ip(val);
			} else if (!strcasecmp(var, "listen-port")) {
				globals.msock.port = atoi(val);
			} else if (!strcasecmp(var, "listen-ssl-port")) {
				globals.msock_ssl.port = atoi(val);
			} else if (!strcasecmp(var, "debug")) {
				globals.debug = switch_true(val);
			} else if (!strcasecmp(var, "secure-cert")) {
				globals.cert = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "secure-key")) {
				globals.key = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "message-buffer-size") && val) {
				globals.message_buffer_size = atoi(val);
				if (globals.message_buffer_size == 0) globals.message_buffer_size = 50;
			}
		}
	}

	switch_xml_free(xml);

	return status;
}

static void *SWITCH_THREAD_FUNC msrp_listener(switch_thread_t *thread, void *obj);

static void close_socket(switch_socket_t ** sock)
{
	// switch_mutex_lock(globals.sock_mutex);
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
	// switch_mutex_unlock(globals.sock_mutex);
}

static switch_status_t msock_init(char *ip, switch_port_t port, switch_socket_t **sock, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *sa;
	switch_status_t rv;

	rv = switch_sockaddr_info_get(&sa, ip, SWITCH_UNSPEC, port, 0, pool);
	if (rv) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot get information about MSRP listen IP address %s\n", ip);
		goto sock_fail;
	}

	rv = switch_socket_create(sock, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool);
	if (rv) goto sock_fail;

	rv = switch_socket_opt_set(*sock, SWITCH_SO_REUSEADDR, 1);
	if (rv) goto sock_fail;

#ifdef WIN32
	/* Enable dual-stack listening on Windows */
	if (switch_sockaddr_get_family(sa) == AF_INET6) {
		rv = switch_socket_opt_set(*sock, SWITCH_SO_IPV6_V6ONLY, 0);
		if (rv) goto sock_fail;
	}
#endif

	rv = switch_socket_bind(*sock, sa);
	if (rv) goto sock_fail;

	rv = switch_socket_listen(*sock, 5);
	if (rv) goto sock_fail;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Socket up listening on %s:%u\n", ip, port);

	return SWITCH_STATUS_SUCCESS;

sock_fail:
	return rv;
}

SWITCH_DECLARE(const char *) switch_msrp_listen_ip()
{
	return globals.ip;
}

SWITCH_DECLARE(switch_status_t) switch_msrp_init()
{
	switch_memory_pool_t *pool;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_status_t status;

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return SWITCH_STATUS_FALSE;
	}

	memset(&globals, 0, sizeof(globals));
	set_global_ip("0.0.0.0");
	globals.pool = pool;
	globals.msock.port = (switch_port_t)0;
	globals.msock_ssl.port = (switch_port_t)0;
	globals.msock_ssl.secure = 1;
	globals.message_buffer_size = 50;
	globals.debug = DEBUG_MSRP;

	load_config();

	if (globals.msock.port) {
		globals.running = 1;

		status = msock_init(globals.ip, globals.msock.port, &globals.msock.sock, pool);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_threadattr_create(&thd_attr, pool);
			// switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, msrp_listener, &globals.msock, pool);
			globals.msock.thread = thread;
		}
	}

	if (globals.msock_ssl.port) {
		globals.running = 1;

		msrp_init_ssl();
		status = msock_init(globals.ip, globals.msock_ssl.port, &globals.msock_ssl.sock, pool);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_threadattr_create(&thd_attr, pool);
			// switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, msrp_listener, &globals.msock_ssl, pool);
			globals.msock_ssl.thread = thread;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_msrp_destroy()
{
	switch_status_t st = SWITCH_STATUS_SUCCESS;
	switch_socket_t *sock;

	globals.running = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "destroying thread\n");

	sock = globals.msock.sock;
	close_socket(&sock);

	sock = globals.msock_ssl.sock;
	close_socket(&sock);

	if (globals.msock.thread) switch_thread_join(&st, globals.msock.thread);
	if (globals.msock_ssl.thread) switch_thread_join(&st, globals.msock_ssl.thread);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "destroy thread done\n");

	globals.msock.thread = NULL;
	globals.msock_ssl.thread = NULL;

	msrp_deinit_ssl();

	switch_safe_free(globals.ip);

	return st;
}

SWITCH_DECLARE(switch_msrp_session_t *)switch_msrp_session_new(switch_memory_pool_t *pool, const char *call_id, switch_bool_t secure)
{
	switch_msrp_session_t *ms;
	ms = switch_core_alloc(pool, sizeof(switch_msrp_session_t));
	switch_assert(ms);
	ms->pool = pool;
	ms->secure = secure;
	ms->local_port = secure ? globals.msock_ssl.port : globals.msock.port;
	ms->msrp_msg_buffer_size = globals.message_buffer_size;
	ms->call_id = switch_core_strdup(pool, call_id);
	switch_mutex_init(&ms->mutex, SWITCH_MUTEX_NESTED, pool);
	return ms;
}

SWITCH_DECLARE(switch_status_t) switch_msrp_session_destroy(switch_msrp_session_t **ms)
{
	int sanity = 500;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying MSRP session %s\n", (*ms)->call_id);

	switch_mutex_lock((*ms)->mutex);

	if ((*ms)->csock && (*ms)->csock->sock) {
		close_socket(&(*ms)->csock->sock);
	}

	switch_mutex_unlock((*ms)->mutex);

	switch_yield(20000);

	while(sanity-- > 0 && (*ms)->running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "waiting MSRP worker %s\n", (*ms)->call_id);
		switch_yield(20000);
	}

	if ((*ms)->send_queue) {
		switch_msrp_msg_t *msg = NULL;

		while (switch_queue_trypop((*ms)->send_queue, (void **)&msg) == SWITCH_STATUS_SUCCESS) {
			switch_msrp_msg_destroy(&msg);
		}
	}

	switch_mutex_destroy((*ms)->mutex);
	ms = NULL;
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t switch_msrp_session_push_msg(switch_msrp_session_t *ms, switch_msrp_msg_t *msg)
{
	switch_mutex_lock(ms->mutex);

	if (ms->last_msg == NULL) {
		ms->last_msg = msg;
		ms->msrp_msg = msg;
	} else {
		ms->last_msg->next = msg;
		ms->last_msg = msg;
	}

	ms->msrp_msg_count++;

	switch_mutex_unlock(ms->mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_msrp_msg_t *)switch_msrp_session_pop_msg(switch_msrp_session_t *ms)
{
	switch_msrp_msg_t *m = NULL;

	switch_mutex_lock(ms->mutex);

	m = ms->msrp_msg;

	if (m == NULL) {
		switch_mutex_unlock(ms->mutex);
		switch_yield(20000);
		switch_mutex_lock(ms->mutex);
	}

	m = ms->msrp_msg;

	if (m == NULL) {
		switch_mutex_unlock(ms->mutex);
		return NULL;
	}

	ms->msrp_msg = ms->msrp_msg->next;
	ms->msrp_msg_count--;

	if (ms->msrp_msg == NULL) ms->last_msg = NULL;

	switch_mutex_unlock(ms->mutex);

	return m;
}

char *msrp_msg_serialize(switch_msrp_msg_t *msrp_msg)
{
	char *code_number_str = switch_mprintf("%d", msrp_msg->code_number);
	const char *content_type = switch_msrp_msg_get_header(msrp_msg, MSRP_H_CONTENT_TYPE);
	char method[10];
	char *result = NULL;

	switch(msrp_msg->method) {
		case MSRP_METHOD_SEND: sprintf(method, "SEND"); break;
		case MSRP_METHOD_AUTH: sprintf(method, "AUTH"); break;
		case MSRP_METHOD_REPLY: sprintf(method, "REPLY"); break;
		case MSRP_METHOD_REPORT: sprintf(method, "REPORT"); break;
		default: sprintf(method, "??%d", msrp_msg->method); break;
	}

	result = switch_mprintf("\n=================================\n"
		"MSRP %s %s%s\r\nFrom: %s\r\nTo: %s\r\nMessage-ID: %s\r\n"
		"Byte-Range: %" SWITCH_SIZE_T_FMT "-%" SWITCH_SIZE_T_FMT"/%" SWITCH_SIZE_T_FMT "\r\n"
		"%s%s%s" /* Content-Type */
		"%s%s%s%s$\r\n"
		"=================================\n",
		msrp_msg->transaction_id ? msrp_msg->transaction_id : code_number_str,
		msrp_msg->transaction_id ? "" : " ",
		msrp_msg->transaction_id ? method : msrp_msg->code_description,
		switch_msrp_msg_get_header(msrp_msg, MSRP_H_FROM_PATH),
		switch_msrp_msg_get_header(msrp_msg, MSRP_H_TO_PATH),
		switch_msrp_msg_get_header(msrp_msg, MSRP_H_MESSAGE_ID),
		msrp_msg->byte_start,
		msrp_msg->byte_end,
		msrp_msg->bytes,
		content_type ? "Content-Type: " : "",
		content_type ? content_type : "",
		content_type ? "\r\n" : "",
		msrp_msg->payload ? "\r\n" : "",
		msrp_msg->payload ? msrp_msg->payload : "",
		msrp_msg->payload ? "\r\n" : "",
		msrp_msg->delimiter);

	switch_safe_free(code_number_str)

	return result;
}

static switch_status_t msrp_socket_recv(switch_msrp_client_socket_t *csock, char *buf, switch_size_t *len)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (csock->secure) {
		switch_ssize_t r;
		r = SSL_read(csock->ssl, buf, *len);
		if (r < 0) {
			int error = SSL_get_error(csock->ssl, r);
			if (!(SSL_ERROR_SYSCALL == error && errno == 9)) {// socket closed
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "TLS read error: ret=%" SWITCH_SSIZE_T_FMT " error=%d errno=%d\n", r, error, errno);
			}
			*len = 0;
		} else {
			*len = r;
			status = SWITCH_STATUS_SUCCESS;
		}
	} else {
		status = switch_socket_recv(csock->sock, buf, len);
	}

	if (globals.debug && status != SWITCH_STATUS_SUCCESS) {
		char err[1024] = { 0 };
		switch_strerror(status, err, sizeof(err) - 1);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "msrp socket recv status = %d, %s\n", status, err);
	}

	return status;
}

static switch_status_t msrp_socket_send(switch_msrp_client_socket_t *csock, char *buf, switch_size_t *len)
{
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "send: %s\n", buf);

	if (csock->secure) {
		*len = SSL_write(csock->ssl, buf, *len);
		return *len ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
	} else {
		return switch_socket_send(csock->sock, buf, len);
	}
}

void dump_buffer(const char *buf, switch_size_t len, int line, int is_send)
{
	int i, j, k = 0;
	char buff[MSRP_BUFF_SIZE * 2];
	// return;
	for(i=0,j=0; i<len; i++) {
		if (buf[i] == '\0') {
			buff[j++] = '\\';
			buff[j++] = '0';
		} else if(buf[i] == '\r') {
			buff[j++] = '\\';
			buff[j++] = 'r';
		} else if(buf[i] == '\n') {
			buff[j++] = '\\';
			buff[j++] = 'n';
			buff[j++] = '\n';
			k = 0;
		}
		 else {
			buff[j++] = buf[i];
		}
		if ((++k) %80 == 0) buff[j++] = '\n';
		if (j >= MSRP_BUFF_SIZE * 2) break;
	}

	buff[j] = '\0';
	switch_log_printf(SWITCH_CHANNEL_LOG, is_send ? SWITCH_LOG_NOTICE: SWITCH_LOG_INFO,
		"%d: %s [%" SWITCH_SIZE_T_FMT "] bytes [\n%s]\n", line, is_send? "SEND" : "RECV", len, buff);
}

char *find_delim(char *buf, int len, const char *delim)
{
	char *p, *q, *s = NULL;
	char *end;

	p = buf;
	q = (char *)delim;

	if (p == NULL) return NULL;
	if (q == NULL) return NULL;

	end = buf + len - strlen(delim);

	while(p < end && *q) {
			if (*p == *q) {
					if (s == NULL) s = p;
					p++;
					q++;
			} else {
					s = NULL;
					p++;
					q = (char *)delim;
			}
			if (*q == '\0') return s;
	}
	return NULL;
}

/*
MSRP d4c667b2351e958f SEND
To-Path: msrp://192.168.0.56:2856/73671a97c9dec690d303;tcp
From-Path: msrp://192.168.0.56:2855/2fb5dfec96f3609f7b48;tcp
Message-ID: 7b7c9965ffa8533c
Byte-Range: 1-0/0
-------d4c667b2351e958f$
*/

char* HEADER_NAMES[] = {
	"MSRP_H_FROM_PATH",
	"MSRP_H_TO_PATH",
	"MSRP_H_MESSAGE_ID",
	"MSRP_H_CONTENT_TYPE",
	"MSRP_H_SUCCESS_REPORT",
	"MSRP_H_FAILURE_REPORT",
	"MSRP_H_STATUS",
	"MSRP_H_KEEPALIVE",

	"MSRP_H_TRASACTION_ID",
	"MSRP_H_DELIMITER",
	"MSRP_H_CODE_DESCRIPTION",

	"MSRP_H_UNKNOWN"
};

SWITCH_DECLARE(char*) switch_msrp_msg_header_name(switch_msrp_header_type_t htype) {
	if (htype > MSRP_H_UNKNOWN) htype = MSRP_H_UNKNOWN;

	return HEADER_NAMES[htype];
}

SWITCH_DECLARE(switch_status_t) switch_msrp_msg_add_header(switch_msrp_msg_t *msrp_msg, switch_msrp_header_type_t htype, char *fmt, ...)
{
	switch_status_t status;

	int ret = 0;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return SWITCH_STATUS_MEMERR;
	}

	status = switch_event_add_header_string(msrp_msg->headers, SWITCH_STACK_BOTTOM, switch_msrp_msg_header_name(htype), data);

	switch (htype) {
	case MSRP_H_TRASACTION_ID:
		msrp_msg->transaction_id = switch_msrp_msg_get_header(msrp_msg, MSRP_H_TRASACTION_ID);
		break;
	case MSRP_H_DELIMITER:
		msrp_msg->delimiter = switch_msrp_msg_get_header(msrp_msg, MSRP_H_DELIMITER);
		break;
	case MSRP_H_CODE_DESCRIPTION:
		msrp_msg->code_description = switch_msrp_msg_get_header(msrp_msg, MSRP_H_CODE_DESCRIPTION);
		break;
	default: break;
	}

	return status;
}

SWITCH_DECLARE(const char *) switch_msrp_msg_get_header(switch_msrp_msg_t *msrp_msg, switch_msrp_header_type_t htype) {
	char *v = switch_event_get_header(msrp_msg->headers, switch_msrp_msg_header_name(htype));
	return v;
}

static char *msrp_parse_header(char *start, int skip, const char *end, switch_msrp_msg_t *msrp_msg, switch_msrp_header_type_t htype, switch_memory_pool_t *pool)
{
	char *p = start + skip;
	char *q;
	if (*p && *p == ' ') p++;
	q = p;
	while(*q != '\n' && q < end) q++;
	if (q > p) {
		if (*(q-1) == '\r') *(q-1) = '\0';
		*q = '\0';
		switch_msrp_msg_add_header(msrp_msg, htype, p);
		return q + 1;
	}
	return start;
}

static switch_msrp_msg_t *msrp_parse_headers(char *start, int len, switch_msrp_msg_t *msrp_msg, switch_memory_pool_t *pool)
{
	char *p = start;
	char *q;
	const char *end = start + len;

	while(p < end) {
		if (!strncasecmp(p, "MSRP ", 5)) {
			p += 5;
			q = p;

			while(*q && q < end && *q != ' ') q++;

			if (q > p) {
				*q = '\0';
				switch_msrp_msg_add_header(msrp_msg, MSRP_H_TRASACTION_ID, p);
				switch_msrp_msg_add_header(msrp_msg, MSRP_H_DELIMITER, "-------%s", p);
				msrp_msg->state = MSRP_ST_PARSE_HEADER;
			}

			p = q;

			if (++p >= end) goto done;

			if (!strncasecmp(p, "SEND", 4)) {
				msrp_msg->method = MSRP_METHOD_SEND;
				p += 6; /*skip \r\n*/
			} else if (!strncasecmp(p, "REPORT", 6)) {
				msrp_msg->method = MSRP_METHOD_REPORT;
				p += 8;
			} else if (!strncasecmp(p, "AUTH", 4)) {
				msrp_msg->method = MSRP_METHOD_AUTH;
				p += 6;
			} else {/* MSRP transaction_id coden_number codede_scription */
				msrp_msg->method = MSRP_METHOD_REPLY;
				q = p;
				while(*q && q < end && *q != ' ') q++;
				if (q > p) {
					*q = '\0';
					msrp_msg->code_number = atoi(p);
					p = ++q;
					while(*q && q < end && *q != '\n') q++;
					if (q > p) {
						if (*(q-1) == '\r') *(q-1) = '\0';
						*q = '\0';
						switch_msrp_msg_add_header(msrp_msg, MSRP_H_CODE_DESCRIPTION, p);
						p = ++q;
					}
				}
			}
		} else if (!strncasecmp(p, "From-Path:", 10)) {
			q = msrp_parse_header(p, 10, end, msrp_msg, MSRP_H_FROM_PATH, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "To-Path:", 8)) {
			q = msrp_parse_header(p, 8, end, msrp_msg, MSRP_H_TO_PATH, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Status:", 7)) {
			q = msrp_parse_header(p, 7, end, msrp_msg, MSRP_H_STATUS, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Keep-Alive:", 11)) {
			q = msrp_parse_header(p, 11, end, msrp_msg, MSRP_H_KEEPALIVE, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Message-ID:", 11)) {
			q = msrp_parse_header(p, 11, end, msrp_msg, MSRP_H_MESSAGE_ID, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Content-Type:", 13)) {
			q = msrp_parse_header(p, 13, end, msrp_msg, MSRP_H_CONTENT_TYPE, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Success-Report:", 15)) {
			q = msrp_parse_header(p, 15, end, msrp_msg, MSRP_H_SUCCESS_REPORT, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Failure-Report:", 15)) {
			q = msrp_parse_header(p, 15, end, msrp_msg, MSRP_H_FAILURE_REPORT, pool);
			if (q == p) break; /* incomplete header*/
			p = q;
		} else if (!strncasecmp(p, "Byte-Range:", 11)) {
			p += 11;
			if (*p && *p == ' ') p++;
			q = p;
			while(*q && q < end && *q != '-') q++;
			if (q > p) {
				*q = '\0';
				msrp_msg->byte_start = atoi(p);
				switch_assert(msrp_msg->byte_start > 0);
				p = ++q;
				if (*p && *p == '*') {
					msrp_msg->range_star = 1;
				}
				while(*q && q < end && *q != '/') q++;
				if (q > p) {
					*q = '\0';
					msrp_msg->byte_end = msrp_msg->range_star ? 0 : atoi(p);
					p = ++q;
					while(*q && q < end && *q != '\n') q++;
					if (q > p) {
						if (*(q-1) == '\r') *(q-1) = '\0';
						*q = '\0';
						msrp_msg->bytes = atoi(p);

						if (!msrp_msg->range_star) {
							msrp_msg->payload_bytes = msrp_msg->byte_end + 1 - msrp_msg->byte_start;
						}

						if (globals.debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%" SWITCH_SIZE_T_FMT " payload bytes\n", msrp_msg->payload_bytes);

						/*Fixme sanity check to avoid large byte-range attack*/
						if (!msrp_msg->range_star && msrp_msg->payload_bytes > msrp_msg->bytes) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "payload size does't match %" SWITCH_SIZE_T_FMT " != %" SWITCH_SIZE_T_FMT "\n", msrp_msg->payload_bytes, msrp_msg->bytes);
							msrp_msg->state = MSRP_ST_ERROR;
							p = ++q;
							break;
						}
						p = ++q;
					}
				}
			}
		} else if (*p == '\r' && *(p+1) == '\n') {
			msrp_msg->state = MSRP_ST_WAIT_BODY;
			p += 2;
			break;
		} else if (msrp_msg->delimiter &&
			!strncasecmp(p, msrp_msg->delimiter, strlen(msrp_msg->delimiter))) {
				char *x = p + strlen(msrp_msg->delimiter);
				if (x < end) {
					if (*x == '$') {
						p = x + 1;
						msrp_msg->state = MSRP_ST_DONE;
						if (*p == '\r') p++;
						if (*p == '\n') p++;
						break;
					} else if(*x == '+') {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported %c\n", *x);
						if (*p == '\r') p++;
						if (*p == '\n') p++;
						break;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported %c\n", *x);
						msrp_msg->state = MSRP_ST_ERROR; //TODO support # etc.
						break;
					}
				}
				break;
		} else {/* unsupported header */
			q = p;
			while(*q && q < end && *q != ':') q++;
			if (q > p) {
				char *last_p = p;
				*q = '\0';
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported header [%s]\n", p);
				p = q + 1;
				q = msrp_parse_header(p, 0, end, msrp_msg, MSRP_H_UNKNOWN, pool);
				if (q == p) {
					p = last_p;
					break; /* incomplete header */
				}
				p = q;
			}
		}
	}

done:
	msrp_msg->last_p = p;
	return msrp_msg;
}

static switch_msrp_msg_t *msrp_parse_buffer(char *buf, int len, switch_msrp_msg_t *msrp_msg, switch_memory_pool_t *pool)
{
	char *start;

	if (!msrp_msg) {
		msrp_msg = switch_msrp_msg_create();
		msrp_msg->state = MSRP_ST_WAIT_HEADER;
	}

	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "parse state: %d\n", msrp_msg->state);
		dump_buffer(buf, len, __LINE__, 0);
	}

	if (msrp_msg->state == MSRP_ST_WAIT_HEADER) {
		if ((start = (char *)switch_stristr("MSRP ", buf)) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not an MSRP packet, Skip!\n");
			return msrp_msg;
		}

		msrp_msg = msrp_parse_headers(start, len - (start - buf), msrp_msg, pool);

		if (msrp_msg->state == MSRP_ST_ERROR) return msrp_msg;
		if (msrp_msg->state == MSRP_ST_DONE) return msrp_msg;

		if (msrp_msg->last_p && msrp_msg->last_p < buf + len) {
			msrp_msg = msrp_parse_buffer(msrp_msg->last_p, len - (msrp_msg->last_p - buf), msrp_msg, pool);
		}
	} else if (msrp_msg->state == MSRP_ST_WAIT_BODY) {
		if(!msrp_msg->range_star && msrp_msg->byte_end == 0) {
			msrp_msg->state = MSRP_ST_DONE;
			return msrp_msg;
		}

		if (msrp_msg->range_star) { /* the * case */
			/*hope we can find the delimiter at the end*/
			int dlen;
			char *delim_pos = NULL;
			switch_size_t payload_bytes;

			switch_assert(msrp_msg->delimiter);
			dlen = strlen(msrp_msg->delimiter);

			if (!strncmp(buf + len - dlen - 3, msrp_msg->delimiter, dlen)) { /*bingo*/
				payload_bytes = len - dlen - 5;
				switch_msrp_msg_set_payload(msrp_msg, buf, payload_bytes);
				msrp_msg->byte_end = msrp_msg->byte_start + payload_bytes - 1;
				msrp_msg->state = MSRP_ST_DONE;
				msrp_msg->last_p = buf + len;
				if (msrp_msg->accumulated_bytes) {
					msrp_msg->accumulated_bytes += payload_bytes;
				}
				return msrp_msg;
			} else if ((delim_pos = find_delim(buf, len, msrp_msg->delimiter))) {
				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "=======================================delimiter: %s\n", delim_pos);
				}
				switch_assert(delim_pos - buf >= 2);
				payload_bytes = delim_pos - buf - 2;
				switch_msrp_msg_set_payload(msrp_msg, buf, payload_bytes);
				msrp_msg->byte_end = msrp_msg->byte_start + msrp_msg->payload_bytes - 1;
				msrp_msg->state = MSRP_ST_DONE;
				msrp_msg->last_p = delim_pos + dlen + 3;
				if (msrp_msg->accumulated_bytes) {
					msrp_msg->accumulated_bytes += payload_bytes;
				}
				return msrp_msg;
			} else {/* keep waiting*/
				msrp_msg->last_p = buf;
				return msrp_msg;
			}
		} else if (msrp_msg->payload_bytes == 0) {
			int dlen = strlen(msrp_msg->delimiter);

			if (strncasecmp(buf, msrp_msg->delimiter, dlen)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error find delimiter\n");
				msrp_msg->state = MSRP_ST_ERROR;
				return msrp_msg;
			}

			msrp_msg->payload = NULL;
			msrp_msg->state = MSRP_ST_DONE;
			msrp_msg->last_p = buf + dlen + 3; /*Fixme: assuming end with $\r\n*/

			return msrp_msg;
		} else {
			int dlen = strlen(msrp_msg->delimiter);

			if (msrp_msg->payload_bytes > len) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "payload too large ... %d > %d\n", (int)msrp_msg->payload_bytes, (int)len);
				msrp_msg->state = MSRP_ST_ERROR; // not supported yet
				return msrp_msg;
			}

			switch_msrp_msg_set_payload(msrp_msg, buf, msrp_msg->payload_bytes);
			msrp_msg->last_p = buf + msrp_msg->payload_bytes;
			msrp_msg->state = MSRP_ST_DONE;
			msrp_msg->last_p = buf + msrp_msg->payload_bytes;

			if (msrp_msg->payload_bytes <= len - dlen - 5) {
				msrp_msg->last_p = buf + msrp_msg->payload_bytes + dlen + 5;

				if (globals.debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "payload bytes: %" SWITCH_SIZE_T_FMT " len: %d dlen: %d delimiter: %s\n", msrp_msg->payload_bytes, len, dlen, msrp_msg->delimiter);

				return msrp_msg; /*Fixme: assuming \r\ndelimiter$\r\n present*/
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%" SWITCH_SIZE_T_FMT " %d %d\n", msrp_msg->payload_bytes, len, dlen);
			msrp_msg->state = MSRP_ST_ERROR;

			return msrp_msg;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error code: %d\n", msrp_msg->state);
	}

	return msrp_msg;
}

switch_status_t msrp_reply(switch_msrp_client_socket_t *csock, switch_msrp_msg_t *msrp_msg)
{
	char buf[2048];
	switch_size_t len;
	sprintf(buf, "MSRP %s 200 OK\r\nTo-Path: %s\r\nFrom-Path: %s\r\n"
		"%s$\r\n",
		msrp_msg->transaction_id,
		switch_str_nil(switch_msrp_msg_get_header(msrp_msg, MSRP_H_FROM_PATH)),
		switch_str_nil(switch_msrp_msg_get_header(msrp_msg, MSRP_H_TO_PATH)),
		msrp_msg->delimiter);
	len = strlen(buf);

	return msrp_socket_send(csock, buf, &len);
}

switch_status_t msrp_report(switch_msrp_client_socket_t *csock, switch_msrp_msg_t *msrp_msg, char *status_code)
{
	char buf[2048];
	switch_size_t len;
	sprintf(buf, "MSRP %s REPORT\r\nTo-Path: %s\r\nFrom-Path: %s\r\nMessage-ID: %s\r\n"
		"Status: 000 %s\r\nByte-Range: 1-%" SWITCH_SIZE_T_FMT "/%" SWITCH_SIZE_T_FMT
		"\r\n%s$\r\n",
		msrp_msg->transaction_id,
		switch_str_nil(switch_msrp_msg_get_header(msrp_msg, MSRP_H_FROM_PATH)),
		switch_str_nil(switch_msrp_msg_get_header(msrp_msg, MSRP_H_TO_PATH)),
		switch_str_nil(switch_msrp_msg_get_header(msrp_msg, MSRP_H_MESSAGE_ID)),
		switch_str_nil(status_code),
		msrp_msg->accumulated_bytes ? msrp_msg->accumulated_bytes : msrp_msg->byte_end,
		msrp_msg->bytes,
		msrp_msg->delimiter);
	len = strlen(buf);

	if (globals.debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "report: %" SWITCH_SIZE_T_FMT " bytes [\n%s]\n", len, buf);

	return msrp_socket_send(csock, buf, &len);
}

static switch_bool_t msrp_find_uuid(char *uuid, const char *to_path)
{
	int len = strlen(to_path);
	int i;
	int slash_count = 0;
	switch_assert(to_path);
	for(i=0; i<len; i++){
		if (*(to_path + i) == '/') {
			if (++slash_count == 3) break;
		}
	}
	if (slash_count < 3) return SWITCH_FALSE;
	if (len - i++ < 36) return SWITCH_FALSE;
	switch_snprintf(uuid, 37, to_path + i);
	return SWITCH_TRUE;
}

static void *SWITCH_THREAD_FUNC msrp_worker(switch_thread_t *thread, void *obj)
{
	worker_helper_t *helper = (worker_helper_t *) obj;
	switch_msrp_client_socket_t *csock = &helper->csock;
	switch_memory_pool_t *pool = helper->pool;
	char buf[MSRP_BUFF_SIZE];
	char *p;
	char *last_p;
	switch_size_t len = MSRP_BUFF_SIZE;
	switch_status_t status;
	switch_msrp_msg_t *msrp_msg = NULL;
	char uuid[128] = { 0 };
	switch_msrp_session_t *msrp_session = NULL;
	int sanity = 10;
	SSL *ssl = NULL;
	int client_mode = helper->csock.client_mode;

	if (client_mode) {
		switch_sockaddr_t *sa = NULL;
		switch_msrp_msg_t *setup_msg = switch_msrp_msg_create();
		const char *remote_ip = NULL;
		switch_port_t remote_port = 0;
		char *dup = NULL;
		char *p = NULL;

		switch_assert(setup_msg);
		switch_assert(helper->msrp_session);
		msrp_session = helper->msrp_session;
		msrp_session->running = 1;

		switch_assert(msrp_session->remote_path);
		dup = switch_core_strdup(pool, msrp_session->remote_path);
		switch_assert(dup);

		p = (char *)switch_stristr("msrp://", dup);

		if (p) {
			p += 7;
		} else {
			p = (char *)switch_stristr("msrps://", dup);

			if (p) p+= 8;
		}

		if (p) {
			remote_ip = p;

			p = (char *)switch_stristr(":", p);

			if (p) {
				*p++ = '\0';
				remote_port = atoi(p);
			}
		}

		if (!remote_ip || remote_port <= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error get remote MSRP ip:port from path: [%s]\n", msrp_session->remote_path);
		}

		if (switch_sockaddr_info_get(&sa, remote_ip, SWITCH_UNSPEC, remote_port, 0, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
			goto end;
		}

		if (switch_socket_create(&csock->sock, switch_sockaddr_get_family(sa),
			SOCK_STREAM, SWITCH_PROTO_TCP, pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error!\n");
			goto end;
		}

		switch_socket_opt_set(csock->sock, SWITCH_SO_KEEPALIVE, 1);
		switch_socket_opt_set(csock->sock, SWITCH_SO_TCP_NODELAY, 1);
		// switch_socket_opt_set(csock->sock, SWITCH_SO_NONBLOCK, TRUE);
		switch_socket_opt_set(csock->sock, SWITCH_SO_TCP_KEEPIDLE, 30);
		switch_socket_opt_set(csock->sock, SWITCH_SO_TCP_KEEPINTVL, 30);
		switch_socket_timeout_set(csock->sock, 3000000); // abort connection 3 seconds than forever

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MSRP %s Connecting to %s\n", msrp_session->call_id, msrp_session->remote_path);

		if ((switch_socket_connect(csock->sock, sa)) != SWITCH_STATUS_SUCCESS) {
			char errbuf[512] = {0};
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error: %s\n", switch_strerror(errno, errbuf, sizeof(errbuf)));
			goto end;
		}

		switch_socket_timeout_set(csock->sock, -1);

		if (msrp_session->secure) {
			X509 *cert = NULL;
			switch_os_socket_t sockdes = SWITCH_SOCK_INVALID;
			int ret;

			switch_os_sock_get(&sockdes, csock->sock);
			switch_assert(sockdes != SWITCH_SOCK_INVALID);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MSRP setup TLS %s\n", msrp_session->call_id);

			ssl = SSL_new(globals.ssl_client_ctx);
			assert(ssl);
			csock->ssl = ssl;
			SSL_set_fd(ssl, sockdes);

			if ((ret = SSL_connect(ssl)) != 1 ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: Could not build a SSL session to: %s error=%d\n", msrp_session->remote_path, SSL_get_error(ssl, ret));
				goto end;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Successfully enabled SSL/TLS session to: %s\n", msrp_session->remote_path);

			cert = SSL_get_peer_certificate(ssl);

			if (cert == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: Could not get a certificate from: %s\n", msrp_session->remote_path);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Got SSL Cert from %s\n", msrp_session->remote_path);
#if 0
				certname = X509_NAME_new();
				certname = X509_get_subject_name(cert);

				X509_NAME_print_ex(outbio, certname, 0, 0);
				BIO_printf(outbio, "\n");
#endif
			}
		}

		helper->msrp_session->csock = csock;

		switch_msrp_msg_add_header(setup_msg, MSRP_H_CONTENT_TYPE, "text/plain");

		if (SWITCH_STATUS_SUCCESS != switch_msrp_send(msrp_session, setup_msg)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MSRP initial setup send error!\n");
			switch_msrp_msg_destroy(&setup_msg);
			goto end;
		}

		switch_msrp_msg_destroy(&setup_msg);
	} else { // server mode
		switch_socket_opt_set(csock->sock, SWITCH_SO_TCP_NODELAY, TRUE);
		// switch_socket_opt_set(csock->sock, SWITCH_SO_NONBLOCK, TRUE);

		if (csock->secure) { // tls?
			int secure_established = 0;
			int sanity = 10;
			switch_os_socket_t sockdes = SWITCH_SOCK_INVALID;
			int code = 0;

			if (globals.ssl_ready != 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SSL not ready\n");
				goto end;
			}

			switch_os_sock_get(&sockdes, csock->sock);
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "socket: %d\n", sockdes);
			switch_assert(sockdes != SWITCH_SOCK_INVALID);

			ssl = SSL_new(globals.ssl_ctx);
			assert(ssl);
			csock->ssl = ssl;

			SSL_set_fd(ssl, sockdes);

			do {
				code = SSL_accept(ssl);

				if (code == 1) {
					secure_established = 1;
					goto done;
				} else if (code == 0) {
					goto err;
				} else if (code < 0) {
					if (code == -1 && SSL_get_error(ssl, code) != SSL_ERROR_WANT_READ) {
						goto err;
					}
				}
			} while(sanity--);

			err:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SSL ERR code=%d error=%d\n", code, SSL_get_error(ssl, code));
				goto end;

			done:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SSL established = %d\n", secure_established);
		}

		len = MSRP_BUFF_SIZE;
		status = msrp_socket_recv(csock, buf, &len);

		if (helper->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "status:%d, len:%" SWITCH_SIZE_T_FMT "\n", status, len);
		}

		if (status == SWITCH_STATUS_SUCCESS) {
			msrp_msg = msrp_parse_buffer(buf, len, NULL, pool);
			switch_assert(msrp_msg);
		} else {
			goto end;
		}

		if (helper->debug) {
			char *data = msrp_msg_serialize(msrp_msg);

			if (data) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", data);
				free(data);
			}
		}

		if (msrp_msg->state == MSRP_ST_DONE && msrp_msg->method == MSRP_METHOD_SEND) {
			msrp_reply(csock, msrp_msg);
			if (msrp_check_success_report(msrp_msg)) {
				msrp_report(csock, msrp_msg, "200 OK");
			}
		} else if (msrp_msg->state == MSRP_ST_DONE && msrp_msg->method == MSRP_METHOD_AUTH) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MSRP_METHOD_AUTH\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse initial message error!\n");
			goto end;
		}

		if (msrp_find_uuid(uuid, switch_msrp_msg_get_header(msrp_msg, MSRP_H_TO_PATH)) != SWITCH_TRUE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid MSRP to-path!\n");
		}

		{
			switch_core_session_t *session = NULL;

			while (sanity-- && !(session = switch_core_session_locate(uuid))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "waiting for session\n");
				switch_yield(1000000);
			}

			if(!session) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No such session %s\n", uuid);
				goto end;
			}

			msrp_session = switch_core_media_get_msrp_session(session);
			switch_assert(msrp_session);
			msrp_session->csock = csock;
			msrp_session->running = 1;

			switch_core_session_rwunlock(session);
		}
	}

	len = MSRP_BUFF_SIZE;
	p = buf;
	last_p = buf;
	if (msrp_msg) switch_msrp_msg_destroy(&msrp_msg);

	while (msrp_socket_recv(csock, p, &len) == SWITCH_STATUS_SUCCESS) {
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "read bytes: %" SWITCH_SIZE_T_FMT "\n", len);

		if (len == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "read bytes: %" SWITCH_SIZE_T_FMT "\n", len);
			continue;
		}

		if (helper->debug) dump_buffer(buf, (p - buf) + len, __LINE__, 0);

	again:
		msrp_msg = msrp_parse_buffer(last_p, p - last_p + len, msrp_msg, pool);

		switch_assert(msrp_msg);

		if (msrp_msg->state == MSRP_ST_ERROR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "msrp parse error!\n");
			goto end;
		}

		if (helper->debug) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "state:%d, len:%" SWITCH_SIZE_T_FMT " payload_bytes:%" SWITCH_SIZE_T_FMT "\n", msrp_msg->state, len, msrp_msg->payload_bytes);
		}

		if (msrp_msg->state == MSRP_ST_DONE && msrp_msg->method == MSRP_METHOD_SEND) {
			msrp_reply(csock, msrp_msg);

			if (msrp_check_success_report(msrp_msg)) {
				msrp_report(csock, msrp_msg, "200 OK");
			}

			last_p = msrp_msg->last_p;
			switch_msrp_session_push_msg(msrp_session, msrp_msg);
			msrp_msg = NULL;
		} else if (msrp_msg->state == MSRP_ST_DONE) { /* throw away */
			last_p = msrp_msg->last_p;
			switch_msrp_msg_destroy(&msrp_msg);
		} else {
			last_p = msrp_msg->last_p;
		}

		while (msrp_session && msrp_session->running && msrp_session->msrp_msg_count > msrp_session->msrp_msg_buffer_size) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s reading too fast, relax...\n", msrp_session->call_id);
			switch_yield(100000);
		}

		if (p + len > last_p) { // unparsed msg in buffer
			p += len;
			len = MSRP_BUFF_SIZE - (p - buf);

			if (!msrp_msg) {
				int rest_len = p - last_p;

				memmove(buf, last_p, rest_len);
				p = buf + rest_len;
				len = MSRP_BUFF_SIZE - rest_len;
				last_p = buf;

				if (rest_len > 10) { // might have a complete msg in buffer, try parse again
					len = 0;
					goto again;
				}

				continue;
			}

			if (p >= buf + MSRP_BUFF_SIZE) {
				switch_msrp_msg_t *new_msg;

				if (msrp_msg->state != MSRP_ST_WAIT_BODY || !msrp_msg->range_star) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "buffer overflow\n");
					/*todo, do a strstr in the whole buffer ?*/
					break;
				}

				switch_assert(p == buf + MSRP_BUFF_SIZE);

				/* buffer full*/
				msrp_msg->payload_bytes = 0;
				new_msg = switch_msrp_msg_dup(msrp_msg);
				switch_msrp_msg_set_payload(new_msg, last_p, p - last_p);
				new_msg->state = MSRP_ST_DONE;
				switch_msrp_session_push_msg(msrp_session, new_msg);
				new_msg = NULL;

				msrp_msg->accumulated_bytes += (p - last_p);
				p = buf;
				len = MSRP_BUFF_SIZE;
				last_p = buf;
				msrp_msg->last_p = buf;
				msrp_msg->byte_start = msrp_msg->byte_end = 0;
				msrp_msg->payload_bytes = 0;

				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "acc: %" SWITCH_SIZE_T_FMT "\n", msrp_msg->accumulated_bytes);
				}
			}
		} else { /* all buffer parsed */
			p = buf;
			len = MSRP_BUFF_SIZE;
			last_p = buf;
		}
		if (!msrp_session->running) break;
	}

end:

	if (msrp_session) {
		switch_mutex_lock(msrp_session->mutex);
		close_socket(&csock->sock);
		switch_mutex_unlock(msrp_session->mutex);
	}

	if (!client_mode) switch_core_destroy_memory_pool(&pool);

	if (ssl) SSL_free(ssl);

	if (msrp_session) msrp_session->running = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP worker down %s\n", msrp_session ? msrp_session->call_id : "!");

	return NULL;
}

static void *SWITCH_THREAD_FUNC msrp_listener(switch_thread_t *thread, void *obj)
{
	switch_msrp_socket_t *msock = (switch_msrp_socket_t *)obj;
	switch_memory_pool_t *pool = NULL;
	switch_threadattr_t *thd_attr = NULL;
	switch_socket_t *sock = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP listener start%s\n", msock->secure ? " ssl" : "");

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
		return NULL;
	}

	switch_socket_opt_set(msock->sock, SWITCH_SO_TCP_NODELAY, TRUE);
	// switch_socket_opt_set(msock->sock, SWITCH_SO_NONBLOCK, TRUE);

	while (globals.running && switch_socket_accept(&sock, msock->sock, pool) == SWITCH_STATUS_SUCCESS) {
		switch_memory_pool_t *worker_pool;
		worker_helper_t *helper;

		if (globals.debug > 0) {
			switch_sockaddr_t *addr = NULL;
			char remote_ip[128];

			/* Get the remote address/port info */
			switch_socket_addr_get(&addr, SWITCH_TRUE, sock);

			if (addr) {
				switch_get_addr(remote_ip, sizeof(remote_ip), addr);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Connection Open%s from %s:%d\n", msock->secure ? " SSL" : "", remote_ip, switch_sockaddr_get_port(addr));
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error get remote addr!\n");
			}
		}

		if (switch_core_new_memory_pool(&worker_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OH OH no pool\n");
			return NULL;
		}

		helper = switch_core_alloc(worker_pool, sizeof(worker_helper_t));

		switch_assert(helper != NULL);
		helper->pool = worker_pool;
		helper->debug = globals.debug;
		helper->csock.sock = sock;
		helper->csock.secure = msock->secure;

		switch_threadattr_create(&thd_attr, pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&thread, thd_attr, msrp_worker, helper, worker_pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP worker new thread spawned!\n");
	}

	if (pool) switch_core_destroy_memory_pool(&pool);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP listener down\n");

	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_msrp_start_client(switch_msrp_session_t *msrp_session)
{
	worker_helper_t *helper;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	helper = switch_core_alloc(msrp_session->pool, sizeof(worker_helper_t));

	switch_assert(helper != NULL);
	helper->pool = msrp_session->pool;
	helper->debug = globals.debug;
	helper->csock.sock = NULL; // client mode
	helper->csock.secure = msrp_session->secure;
	helper->csock.client_mode = 1;
	helper->msrp_session = msrp_session;

	switch_threadattr_create(&thd_attr, helper->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, msrp_worker, helper, helper->pool);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "MSRP new worker client started! %s\n", msrp_session->call_id);

	return SWITCH_STATUS_SUCCESS;
}

void random_string(char *buf, uint16_t size)
{
	switch_stun_random_string(buf, size, NULL);
}

#define MSRP_TRANS_ID_LEN 16
static switch_status_t switch_msrp_do_send(switch_msrp_session_t *ms, switch_msrp_msg_t *msrp_msg, const char *file, const char *func, int line)
{
	char transaction_id[MSRP_TRANS_ID_LEN + 1] = { 0 };
	char buf[MSRP_BUFF_SIZE];
	char message_id[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
	switch_size_t len;
	const char *msrp_h_to_path = switch_msrp_msg_get_header(msrp_msg, MSRP_H_TO_PATH);
	const char *msrp_h_from_path = switch_msrp_msg_get_header(msrp_msg, MSRP_H_FROM_PATH);
	const char *to_path = msrp_h_to_path ? msrp_h_to_path : ms->remote_path;
	const char *from_path = msrp_h_from_path ? msrp_h_from_path: ms->local_path;
	const char *content_type = switch_msrp_msg_get_header(msrp_msg, MSRP_H_CONTENT_TYPE);

	if (msrp_msg->payload_bytes == 2 && msrp_msg->payload && !strncmp(msrp_msg->payload, "\r\n", 2)) {
		// discard \r\n appended in uuid_send_text
		return SWITCH_STATUS_SUCCESS;
	}

	if (!from_path) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, ms->call_id, SWITCH_LOG_WARNING, "NO FROM PATH\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (zstr(content_type)) {
		content_type = "text/plain";
	}

	random_string(transaction_id, MSRP_TRANS_ID_LEN);
	switch_uuid_str(message_id, sizeof(message_id));

	sprintf(buf, "MSRP %s SEND\r\nTo-Path: %s\r\nFrom-Path: %s\r\n"
		"Message-ID: %s\r\n"
		"Byte-Range: 1-%" SWITCH_SIZE_T_FMT "/%" SWITCH_SIZE_T_FMT "\r\n"
		"%s%s%s",
		transaction_id,
		to_path,
		from_path,
		message_id,
		msrp_msg->payload ? msrp_msg->payload_bytes : 0,
		msrp_msg->payload ? msrp_msg->payload_bytes : 0,
		msrp_msg->payload ? "Content-Type: " : "",
		msrp_msg->payload ? content_type : "",
		msrp_msg->payload ? "\r\n\r\n" : "");

	len = strlen(buf);

	if (msrp_msg->payload) {
		if (len + msrp_msg->payload_bytes >= MSRP_BUFF_SIZE) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, ms->call_id, SWITCH_LOG_ERROR, "payload too large! %" SWITCH_SIZE_T_FMT "\n", len + msrp_msg->payload_bytes);
			return SWITCH_STATUS_FALSE;
		}
		memcpy(buf + len, msrp_msg->payload, msrp_msg->payload_bytes);
		len += msrp_msg->payload_bytes;
		sprintf(buf + len, "\r\n");
		len += 2;
	}

	sprintf(buf + len, "-------%s$\r\n", transaction_id);
	len += (10 + strlen(transaction_id));

	if (globals.debug) dump_buffer(buf, len, __LINE__, 1);

	return ms->csock ? msrp_socket_send(ms->csock, buf, &len) : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE (switch_status_t) switch_msrp_perform_send(switch_msrp_session_t *ms, switch_msrp_msg_t *msrp_msg, const char *file, const char *func, int line)
{
	switch_msrp_msg_t *msg = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!ms->running) {
		if (!ms->send_queue) {
			switch_queue_create(&ms->send_queue, 100, ms->pool);
		}

		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, ms->call_id, SWITCH_LOG_WARNING, "MSRP not ready! Buffering one message %" SWITCH_SIZE_T_FMT " bytes\n", msrp_msg->payload_bytes);

		if (globals.debug && msrp_msg->payload) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, ms->call_id, SWITCH_LOG_WARNING, "MSRP not ready! Buffered one message [%s]\n", msrp_msg->payload);
		}

		msg = switch_msrp_msg_dup(msrp_msg);

		status = switch_queue_trypush(ms->send_queue, msg);

		if (status != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, ms->call_id, SWITCH_LOG_ERROR, "MSRP queue FULL! Discard one message %" SWITCH_SIZE_T_FMT " bytes\n", msg->payload_bytes);
			switch_msrp_msg_destroy(&msg);
		}

		return status;
	}

	if (ms->send_queue) {
		while (status == SWITCH_STATUS_SUCCESS && switch_queue_trypop(ms->send_queue, (void **)&msg) == SWITCH_STATUS_SUCCESS) {
			status = switch_msrp_do_send(ms, msg, file, func, line);
		}

		switch_queue_term(ms->send_queue);
		ms->send_queue = NULL;
	}

	status = switch_msrp_do_send(ms, msrp_msg, file, func, line);

	return status;
}

SWITCH_DECLARE(switch_msrp_msg_t *) switch_msrp_msg_create()
{
	switch_msrp_msg_t *msg = malloc(sizeof(switch_msrp_msg_t));
	switch_assert(msg);

	memset(msg, 0, sizeof(switch_msrp_msg_t));
	switch_event_create(&msg->headers, SWITCH_EVENT_GENERAL);
	switch_assert(msg->headers);

	return msg;
}

SWITCH_DECLARE(switch_msrp_msg_t *) switch_msrp_msg_dup(switch_msrp_msg_t *msg)
{
	switch_msrp_msg_t *new_msg = malloc(sizeof(switch_msrp_msg_t));
	switch_assert(new_msg);
	memset(new_msg, 0, sizeof(switch_msrp_msg_t));
	switch_event_dup(&new_msg->headers, msg->headers);
	switch_assert(new_msg->headers);

	new_msg->transaction_id = switch_msrp_msg_get_header(new_msg, MSRP_H_TRASACTION_ID);
	new_msg->delimiter = switch_msrp_msg_get_header(new_msg, MSRP_H_DELIMITER);
	new_msg->code_description = switch_msrp_msg_get_header(new_msg, MSRP_H_CODE_DESCRIPTION);
	new_msg->state = msg->state;
	new_msg->method = msg->method;
	new_msg->code_number = msg->code_number;
	new_msg->payload_bytes = msg->payload_bytes;

	if (msg->payload_bytes > 0 && msg->payload) {
		new_msg->payload = malloc(msg->payload_bytes + 1);
		switch_assert(new_msg->payload);
		memcpy(new_msg->payload, msg->payload, msg->payload_bytes);
		*(new_msg->payload + msg->payload_bytes) = '\0';
	}

	return new_msg;
}

SWITCH_DECLARE(void) switch_msrp_msg_destroy(switch_msrp_msg_t **msg)
{
	switch_msrp_msg_t *msrp_msg = *msg;
	if (msrp_msg->headers) {
		switch_event_destroy(&msrp_msg->headers);
	}

	switch_safe_free(msrp_msg->payload);
	*msg = NULL;
}

/* Experimental */

SWITCH_STANDARD_APP(msrp_recv_file_function)
{
	switch_msrp_session_t *msrp_session = NULL;
	switch_msrp_msg_t *msrp_msg = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_file_t *fd;
	const char *filename = data;

	switch_channel_set_flag(channel, CF_TEXT_PASSIVE);
	switch_channel_answer(channel);

	if (zstr(data)) {
		filename = switch_channel_get_variable(channel, "sip_msrp_file_name");

		if (zstr(filename)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No file specified.\n");
			return;
		}

		filename = switch_core_session_sprintf(session, "%s%s%s", SWITCH_GLOBAL_dirs.base_dir, SWITCH_PATH_SEPARATOR, filename);
	}

	if (!(msrp_session = switch_core_media_get_msrp_session(session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not a MSRP session!\n");
		return;
	}

	if (switch_file_open(&fd, filename, SWITCH_FOPEN_WRITE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Open File %s\n", filename);
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "File [%s] Opened\n", filename);

	while (1) {
		if ((msrp_msg = switch_msrp_session_pop_msg(msrp_session)) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "MSRP message queue size: %d\n", (int)msrp_session->msrp_msg_count);
			if (!switch_channel_ready(channel)) break;
			continue;
		}

		if (msrp_msg->method == MSRP_METHOD_SEND) {
			switch_size_t bytes = msrp_msg->payload_bytes;
			const char *msg = switch_msrp_msg_get_header(msrp_msg, MSRP_H_MESSAGE_ID);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s %" SWITCH_SIZE_T_FMT " bytes writing\n", msg, bytes);
			switch_file_write(fd, msrp_msg->payload, &bytes);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%" SWITCH_SIZE_T_FMT " bytes written\n", bytes);
			if (bytes != msrp_msg->payload_bytes) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "write failed, bytes lost!\n");
			}
		}

		switch_safe_free(msrp_msg);
	}

	switch_file_close(fd);
	switch_channel_clear_flag(channel, CF_TEXT_PASSIVE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "File closed!\n");
}

/* Experimental, if it doesn't work, it doesn't */

SWITCH_STANDARD_APP(msrp_send_file_function)
{
	switch_msrp_session_t *msrp_session = NULL;
	switch_msrp_msg_t *msrp_msg = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_memory_pool_t *pool = switch_core_session_get_pool(session);
	switch_file_t *fd;
	const char *filename = data;
	switch_size_t len = 1024;
	char buf[1024];
	int sanity = 10;

	if (!(msrp_session = switch_core_media_get_msrp_session(session))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not a msrp session!\n");
		return;
	}

	if (switch_file_open(&fd, filename, SWITCH_FOPEN_READ, SWITCH_FPROT_OS_DEFAULT, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Open File %s\n", filename);
		return;
	}

	msrp_msg = switch_msrp_msg_create();

	switch_msrp_msg_add_header(msrp_msg, MSRP_H_CONTENT_TYPE, "text/plain");

	msrp_msg->payload_bytes = switch_file_get_size(fd);
	msrp_msg->byte_start = 1;

	while(sanity-- && !msrp_session->running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Waiting MSRP socket ...\n");
		switch_yield(1000000);
	}

	if (!msrp_session->running) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Waiting for MSRP socket timedout, exiting...\n");
		goto end;
	}

	while (switch_file_read(fd, buf, &len) == SWITCH_STATUS_SUCCESS &&
		switch_channel_ready(channel) && len > 0) {

		msrp_msg->byte_end = msrp_msg->byte_start + len + 1;
		switch_msrp_msg_set_payload(msrp_msg, buf, len);

		/*TODO: send in chunk should ending in + but not $ after delimiter*/
		switch_msrp_send(msrp_session, msrp_msg);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%" SWITCH_SIZE_T_FMT " bytes sent\n", len);

		msrp_msg->byte_start += len;
	}

	sanity = 10;

	while(sanity-- && switch_channel_ready(channel)) {
		switch_yield(1000000);
	}

end:
	switch_file_close(fd);
	switch_msrp_msg_destroy(&msrp_msg);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "File [%s] sent, closed!\n", filename);
}

SWITCH_STANDARD_API(uuid_msrp_send_function)
{
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc;
	switch_core_session_t *msession = NULL;
	switch_msrp_session_t *msrp_session = NULL;
	switch_msrp_msg_t *msrp_msg = NULL;

	if (zstr(cmd) || !(mycmd = strdup(cmd))) {
		goto error;
	}

	argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2 || !argv[0]) {
		goto error;
	}

	if (!(msession = switch_core_session_locate(argv[0]))) {
		stream->write_function(stream, "-ERR Usage: cannot locate session.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(msrp_session = switch_core_media_get_msrp_session(msession))) {
		stream->write_function(stream, "-ERR No msrp_session.\n");
		switch_core_session_rwunlock(msession);
		return SWITCH_STATUS_SUCCESS;
	}

	msrp_msg = switch_msrp_msg_create();
	switch_msrp_msg_add_header(msrp_msg, MSRP_H_CONTENT_TYPE, "text/plain");
	switch_msrp_msg_set_payload(msrp_msg, argv[1], strlen(argv[1]));
	switch_msrp_send(msrp_session, msrp_msg);
	switch_msrp_msg_destroy(&msrp_msg);
	stream->write_function(stream, "+OK message sent\n");
	switch_core_session_rwunlock(msession);
	return SWITCH_STATUS_SUCCESS;

error:
	stream->write_function(stream, "-ERR Usage: uuid_msrp_send <uuid> msg\n");
	return SWITCH_STATUS_SUCCESS;
}

#define MSRP_SYNTAX "debug <on|off>|restart"
SWITCH_STANDARD_API(msrp_api_function)
{
	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR usage: " MSRP_SYNTAX "\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!strcmp(cmd, "debug on")) {
		globals.debug = 1;
		stream->write_function(stream, "+OK debug on\n");
	} else if(!strcmp(cmd, "debug off")) {
		globals.debug = 0;
		stream->write_function(stream, "+OK debug off\n");
	} else if(!strcmp(cmd, "restart")) {
		switch_msrp_destroy();
		switch_msrp_init();
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_msrp_load_apis_and_applications(switch_loadable_module_interface_t **module_interface)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	SWITCH_ADD_API(api_interface, "msrp", "MSRP Functions", msrp_api_function, MSRP_SYNTAX);

	SWITCH_ADD_API(api_interface, "uuid_msrp_send", "send msrp text", uuid_msrp_send_function, "<msg>");
	SWITCH_ADD_APP(app_interface, "msrp_recv_file", "Recv msrp message to file", "Recv msrp message", msrp_recv_file_function, "<filename>", SAF_SUPPORT_TEXT_ONLY | SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "msrp_send_file", "Send file via msrp", "Send file via msrp", msrp_send_file_function, "<filename>", SAF_SUPPORT_TEXT_ONLY | SAF_SUPPORT_NOMEDIA);

	switch_console_set_complete("add msrp debug on");
	switch_console_set_complete("add msrp debug off");
	switch_console_set_complete("restart");
	switch_console_set_complete("add uuid_msrp_send ::console::list_uuid");
}


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
