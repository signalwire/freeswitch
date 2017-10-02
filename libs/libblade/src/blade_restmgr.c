/*
 * Copyright (c) 2017, Shane Bryldt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "blade.h"

typedef struct blade_restmgr_config_s {
	ks_bool_t enabled;

	ks_hash_t *options;
} blade_restmgr_config_t;

struct blade_restmgr_s {
	blade_handle_t *handle;

	blade_restmgr_config_t config;

	void *data;

	struct mg_context *context;
};

int blade_restmgr_handle_begin_request(struct mg_connection *conn);
void blade_restmgr_handle_end_request(const struct mg_connection *conn, int reply_status_code);
int blade_restmgr_handle_log_message(const struct mg_connection *conn, const char *message);
int blade_restmgr_handle_log_access(const struct mg_connection *conn, const char *message);
int blade_restmgr_handle_init_ssl(void *ssl_context, void *user_data);
void blade_restmgr_handle_connection_close(const struct mg_connection *conn);
const char *blade_restmgr_handle_open_file(const struct mg_connection *conn, const char *path, size_t *data_len);
void blade_restmgr_handle_init_lua(const struct mg_connection *conn, void *lua_context);
int blade_restmgr_handle_http_error(struct mg_connection *conn, int status);
void blade_restmgr_handle_init_context(const struct mg_context *ctx);
void blade_restmgr_handle_init_thread(const struct mg_context *ctx, int thread_type);
void blade_restmgr_handle_exit_context(const struct mg_context *ctx);


static void blade_restmgr_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_restmgr_t *brestmgr = (blade_restmgr_t *)ptr;

	ks_assert(brestmgr);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_restmgr_create(blade_restmgr_t **brestmgrP, blade_handle_t *bh)
{
	ks_pool_t *pool = NULL;
	blade_restmgr_t *brestmgr = NULL;

	ks_assert(brestmgrP);

	ks_pool_open(&pool);
	ks_assert(pool);

	brestmgr = ks_pool_alloc(pool, sizeof(blade_restmgr_t));
	brestmgr->handle = bh;

	ks_hash_create(&brestmgr->config.options, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK, pool);
	ks_assert(brestmgr->config.options);

	ks_pool_set_cleanup(brestmgr, NULL, blade_restmgr_cleanup);

	*brestmgrP = brestmgr;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_restmgr_destroy(blade_restmgr_t **brestmgrP)
{
	blade_restmgr_t *brestmgr = NULL;
	ks_pool_t *pool;

	ks_assert(brestmgrP);
	ks_assert(*brestmgrP);

	brestmgr = *brestmgrP;
	*brestmgrP = NULL;

	pool = ks_pool_get(brestmgr);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

#define CONFIG_LOADSTR(k) \
tmp = config_lookup_from(rest, k); \
if (tmp && config_setting_type(tmp) != CONFIG_TYPE_STRING) return KS_STATUS_FAIL; \
if (tmp) ks_hash_insert(brestmgr->config.options, (void *)k, (void *)ks_pstrdup(pool, config_setting_get_string(tmp)));

ks_status_t blade_restmgr_config(blade_restmgr_t *brestmgr, config_setting_t *config)
{
	ks_pool_t *pool = NULL;
	config_setting_t *rest = NULL;
	config_setting_t *tmp = NULL;

	ks_assert(brestmgr);

	pool = ks_pool_get(brestmgr);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	rest = config_setting_get_member(config, "rest");
	if (rest) {
		tmp = config_lookup_from(rest, "enabled");
		if (!tmp) return KS_STATUS_FAIL;

		if (config_setting_type(tmp) != CONFIG_TYPE_BOOL) return KS_STATUS_FAIL;
		brestmgr->config.enabled = config_setting_get_bool(tmp);
		if (!brestmgr->config.enabled) return KS_STATUS_SUCCESS;

		CONFIG_LOADSTR("cgi_pattern");
		CONFIG_LOADSTR("cgi_environment");
		CONFIG_LOADSTR("put_delete_auth_file");
		CONFIG_LOADSTR("cgi_interpreter");
		CONFIG_LOADSTR("protect_uri");
		CONFIG_LOADSTR("authentication_domain");
		CONFIG_LOADSTR("enable_auth_domain_check");
		CONFIG_LOADSTR("ssi_pattern");
		CONFIG_LOADSTR("throttle");
		CONFIG_LOADSTR("access_log_file");
		CONFIG_LOADSTR("enable_directory_listing");
		CONFIG_LOADSTR("error_log_file");
		CONFIG_LOADSTR("global_auth_file");
		CONFIG_LOADSTR("index_files");
		CONFIG_LOADSTR("enable_keep_alive");
		CONFIG_LOADSTR("access_control_list");
		CONFIG_LOADSTR("extra_mime_types");
		CONFIG_LOADSTR("listening_ports");
		CONFIG_LOADSTR("document_root");
		CONFIG_LOADSTR("ssl_certificate");
		CONFIG_LOADSTR("ssl_certificate_chain");
		CONFIG_LOADSTR("num_threads");
		CONFIG_LOADSTR("run_as_user");
		CONFIG_LOADSTR("url_rewrite_patterns");
		CONFIG_LOADSTR("hide_files_patterns");
		CONFIG_LOADSTR("request_timeout_ms");
		CONFIG_LOADSTR("keep_alive_timeout_ms");
		CONFIG_LOADSTR("linger_timeout_ms");
		CONFIG_LOADSTR("ssl_verify_peer");
		CONFIG_LOADSTR("ssl_ca_path");
		CONFIG_LOADSTR("ssl_ca_file");
		CONFIG_LOADSTR("ssl_verify_depth");
		CONFIG_LOADSTR("ssl_default_verify_paths");
		CONFIG_LOADSTR("ssl_cipher_list");
		CONFIG_LOADSTR("ssl_protocol_version");
		CONFIG_LOADSTR("ssl_short_trust");
		CONFIG_LOADSTR("websocket_timeout_ms");
		CONFIG_LOADSTR("decode_url");
		CONFIG_LOADSTR("lua_preload_file");
		CONFIG_LOADSTR("lua_script_pattern");
		CONFIG_LOADSTR("lua_server_page_pattern");
		CONFIG_LOADSTR("duktape_script_pattern");
		CONFIG_LOADSTR("websocket_root");
		CONFIG_LOADSTR("lua_websocket_pattern");
		CONFIG_LOADSTR("access_control_allow_origin");
		CONFIG_LOADSTR("access_control_allow_methods");
		CONFIG_LOADSTR("access_control_allow_headers");
		CONFIG_LOADSTR("error_pages");
		CONFIG_LOADSTR("tcp_nodelay");
		CONFIG_LOADSTR("static_file_max_age");
		CONFIG_LOADSTR("strict_transport_security_max_age");
		CONFIG_LOADSTR("allow_sendfile_call");
		CONFIG_LOADSTR("case_sensitive");
		CONFIG_LOADSTR("lua_background_script");
		CONFIG_LOADSTR("lua_background_script_params");
		CONFIG_LOADSTR("additional_header");
		CONFIG_LOADSTR("max_request_size");
		CONFIG_LOADSTR("allow_index_script_resource");
	}

	if (brestmgr->config.enabled) {
		ks_log(KS_LOG_DEBUG, "Configured\n");
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_restmgr_startup(blade_restmgr_t *brestmgr, config_setting_t *config)
{
	ks_assert(brestmgr);

	if (blade_restmgr_config(brestmgr, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_restmgr_config failed\n");
		return KS_STATUS_FAIL;
	}

	if (brestmgr->config.enabled) {
		struct mg_callbacks callbacks;
		const char **options = (const char **)ks_pool_calloc(ks_pool_get(brestmgr), (ks_hash_count(brestmgr->config.options) * 2) + 1, sizeof(char *));
		ks_size_t index = 0;

		memset(&callbacks, 0, sizeof(struct mg_callbacks));
		callbacks.begin_request = blade_restmgr_handle_begin_request;
		callbacks.end_request = blade_restmgr_handle_end_request;
		callbacks.log_message = blade_restmgr_handle_log_message;
		callbacks.log_access = blade_restmgr_handle_log_access;
		//callbacks.init_ssl = blade_restmgr_handle_init_ssl;
		callbacks.connection_close = blade_restmgr_handle_connection_close;
		callbacks.open_file = blade_restmgr_handle_open_file;
		callbacks.init_lua = blade_restmgr_handle_init_lua;
		callbacks.http_error = blade_restmgr_handle_http_error;
		callbacks.init_context = blade_restmgr_handle_init_context;
		callbacks.init_thread = blade_restmgr_handle_init_thread;
		callbacks.exit_context = blade_restmgr_handle_exit_context;

		for (ks_hash_iterator_t *it = ks_hash_first(brestmgr->config.options, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const char *key = NULL;
			const char *value = NULL;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
			options[index++] = key;
			options[index++] = value;
		}
		options[index++] = NULL;

		brestmgr->context = mg_start(&callbacks, brestmgr->data, options);

		ks_pool_free(&options);

		if (!brestmgr->context) return KS_STATUS_FAIL;
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_restmgr_shutdown(blade_restmgr_t *brestmgr)
{
	ks_assert(brestmgr);
	if (brestmgr->context) {
		mg_stop(brestmgr->context);
		brestmgr->context = NULL;
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(blade_handle_t *) blade_restmgr_handle_get(blade_restmgr_t *brestmgr)
{
	ks_assert(brestmgr);

	return brestmgr->handle;
}

KS_DECLARE(ks_status_t) blade_restmgr_data_set(blade_restmgr_t *brestmgr, void *data)
{
	ks_assert(brestmgr);

	brestmgr->data = data;

	return KS_STATUS_SUCCESS;
}


int blade_restmgr_handle_begin_request(struct mg_connection *conn)
{
	const struct mg_request_info *info = mg_get_request_info(conn);

	ks_log(KS_LOG_DEBUG, "Request for: %s on %s\n", info->request_uri, info->request_method);

	return 0;
}

void blade_restmgr_handle_end_request(const struct mg_connection *conn, int reply_status_code)
{

}

int blade_restmgr_handle_log_message(const struct mg_connection *conn, const char *message)
{
	return 0;
}

int blade_restmgr_handle_log_access(const struct mg_connection *conn, const char *message)
{
	return 0;
}

int blade_restmgr_handle_init_ssl(void *ssl_context, void *user_data)
{
	return 0;
}

void blade_restmgr_handle_connection_close(const struct mg_connection *conn)
{

}

const char *blade_restmgr_handle_open_file(const struct mg_connection *conn, const char *path, size_t *data_len)
{
	return NULL;
}

void blade_restmgr_handle_init_lua(const struct mg_connection *conn, void *lua_context)
{

}

int blade_restmgr_handle_http_error(struct mg_connection *conn, int status)
{
	return 1;
}

void blade_restmgr_handle_init_context(const struct mg_context *ctx)
{

}

void blade_restmgr_handle_init_thread(const struct mg_context *ctx, int thread_type)
{

}

void blade_restmgr_handle_exit_context(const struct mg_context *ctx)
{

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
