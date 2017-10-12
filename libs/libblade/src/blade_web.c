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

struct blade_webrequest_s {
	const char *action;
	const char *path;

	ks_hash_t *query;
	ks_hash_t *headers;

	ks_sb_t *content;
};

struct blade_webresponse_s {
	const char *status_code;
	const char *status_message;

	ks_hash_t *headers;

	ks_sb_t *content;
};

static void blade_webrequest_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_webrequest_t *bwreq = (blade_webrequest_t *)ptr;

	ks_assert(bwreq);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

static void blade_webresponse_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_webresponse_t *bwres = (blade_webresponse_t *)ptr;

	ks_assert(bwres);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

KS_DECLARE(ks_status_t) blade_webrequest_create(blade_webrequest_t **bwreqP, const char *action, const char *path)
{
	ks_pool_t *pool = NULL;
	blade_webrequest_t *bwreq = NULL;

	ks_assert(bwreqP);
	ks_assert(action);
	ks_assert(path);

	ks_pool_open(&pool);
	ks_assert(pool);

	bwreq = ks_pool_alloc(pool, sizeof(blade_webrequest_t));

	bwreq->action = ks_pstrdup(pool, action);
	bwreq->path = ks_pstrdup(pool, path);

	ks_hash_create(&bwreq->query, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwreq->query);

	ks_hash_create(&bwreq->headers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwreq->headers);

	ks_sb_create(&bwreq->content, pool, 0);
	ks_assert(bwreq->content);

	ks_pool_set_cleanup(bwreq, NULL, blade_webrequest_cleanup);

	*bwreqP = bwreq;

	blade_webrequest_header_add(bwreq, "Content-Type", "application/x-www-form-urlencoded");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webrequest_load(blade_webrequest_t **bwreqP, struct mg_connection *conn)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	blade_webrequest_t *bwreq = NULL;
	const struct mg_request_info *info = NULL;
	char buf[1024];
	int bytes = 0;

	ks_assert(bwreqP);
	ks_assert(conn);

	info = mg_get_request_info(conn);

	ks_pool_open(&pool);
	ks_assert(pool);

	bwreq = ks_pool_alloc(pool, sizeof(blade_webrequest_t));

	bwreq->action = ks_pstrdup(pool, info->request_method);
	bwreq->path = ks_pstrdup(pool, info->request_uri);

	ks_hash_create(&bwreq->query, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwreq->query);

	ks_hash_create(&bwreq->headers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwreq->headers);

	ks_sb_create(&bwreq->content, pool, 0);
	ks_assert(bwreq->content);

	ks_pool_set_cleanup(bwreq, NULL, blade_webrequest_cleanup);

	if (info->query_string && info->query_string[0]) {
		char *query = ks_pstrdup(pool, info->query_string);
		char *start = query;
		char *end = NULL;

		do {
			char *key = start;
			char *value = NULL;

			end = strchr(start, '&');
			if (end) *end = '\0';

			value = strchr(start, '=');
			if (value) {
				*value = '\0';
				value++;

				if (*key && *value) {
					ks_hash_insert(bwreq->query, (void *)ks_pstrdup(pool, key), (void *)ks_pstrdup(pool, value));
				}
			}

			if (end) start = ++end;
			else start = NULL;
		} while (start);

		ks_pool_free(&query);
	}

	for (int index = 0; index < info->num_headers; ++index) {
		const struct mg_header *header = &info->http_headers[index];
		ks_hash_insert(bwreq->headers, (void *)ks_pstrdup(pool, header->name), (void *)ks_pstrdup(pool, header->value));
	}

	while ((bytes = mg_read(conn, buf, sizeof(buf))) > 0) ks_sb_append_ex(bwreq->content, buf, bytes);
	if (bytes < 0) {
		blade_webrequest_destroy(&bwreq);
		ret = KS_STATUS_FAIL;
	}
	else *bwreqP = bwreq;

	return ret;
}

KS_DECLARE(ks_status_t) blade_webrequest_destroy(blade_webrequest_t **bwreqP)
{
	blade_webrequest_t *bwreq = NULL;
	ks_pool_t *pool;

	ks_assert(bwreqP);
	ks_assert(*bwreqP);

	bwreq = *bwreqP;
	*bwreqP = NULL;

	pool = ks_pool_get(bwreq);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_webrequest_action_get(blade_webrequest_t *bwreq)
{
	ks_assert(bwreq);
	return bwreq->action;
}

KS_DECLARE(const char *) blade_webrequest_path_get(blade_webrequest_t *bwreq)
{
	ks_assert(bwreq);
	return bwreq->path;
}

KS_DECLARE(ks_status_t) blade_webrequest_query_add(blade_webrequest_t *bwreq, const char *name, const char *value)
{
	ks_assert(bwreq);
	ks_assert(name);
	ks_assert(value);

	ks_hash_insert(bwreq->query, (void *)ks_pstrdup(ks_pool_get(bwreq), name), (void *)ks_pstrdup(ks_pool_get(bwreq), value));

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_webrequest_query_get(blade_webrequest_t *bwreq, const char *name)
{
	ks_assert(bwreq);
	ks_assert(name);

	return (const char *)ks_hash_search(bwreq->query, (void *)name, KS_UNLOCKED);
}

KS_DECLARE(ks_status_t) blade_webrequest_header_add(blade_webrequest_t *bwreq, const char *header, const char *value)
{
	ks_assert(bwreq);
	ks_assert(header);
	ks_assert(value);

	ks_hash_insert(bwreq->headers, (void *)ks_pstrdup(ks_pool_get(bwreq), header), (void *)ks_pstrdup(ks_pool_get(bwreq), value));

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webrequest_header_printf(blade_webrequest_t *bwreq, const char *header, const char *fmt, ...)
{
	va_list ap;
	char *result = NULL;

	ks_assert(bwreq);
	ks_assert(header);
	ks_assert(fmt);

	va_start(ap, fmt);
	result = ks_vpprintf(ks_pool_get(bwreq), fmt, ap);
	va_end(ap);

	ks_hash_insert(bwreq->headers, (void *)ks_pstrdup(ks_pool_get(bwreq), header), (void *)result);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_webrequest_header_get(blade_webrequest_t *bwreq, const char *header)
{
	ks_assert(bwreq);
	ks_assert(header);

	return (const char *)ks_hash_search(bwreq->headers, (void *)header, KS_UNLOCKED);
}

KS_DECLARE(ks_status_t) blade_webrequest_content_json_append(blade_webrequest_t *bwreq, cJSON *json)
{
	ks_assert(bwreq);
	ks_assert(json);

	blade_webrequest_header_add(bwreq, "Content-Type", "application/json");

	ks_sb_json(bwreq->content, json);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webrequest_content_string_append(blade_webrequest_t *bwreq, const char *str)
{
	ks_assert(bwreq);
	ks_assert(str);

	ks_sb_append(bwreq->content, str);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webrequest_send(blade_webrequest_t *bwreq, ks_bool_t secure, const char *host, ks_port_t port, blade_webresponse_t **bwresP)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	char buf[1024];
	struct mg_connection *conn = NULL;
	const char *path = NULL;
	ks_sb_t *pathAndQuery = NULL;

	ks_assert(bwreq);
	ks_assert(host);
	ks_assert(bwresP);

	if (port == 0) port = secure ? 443 : 80;

	conn = mg_connect_client(host, port, secure, buf, sizeof(buf));
	if (!conn) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	path = bwreq->path;
	if (ks_hash_count(bwreq->query) > 0) {
		ks_bool_t firstQuery = KS_TRUE;

		ks_sb_create(&pathAndQuery, NULL, 0);
		ks_sb_append(pathAndQuery, bwreq->path);
		for (ks_hash_iterator_t *it = ks_hash_first(bwreq->query, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
			const char *key;
			const char *value;

			ks_hash_this(it, (const void **)&key, NULL, (void **)&value);

			// @todo make sure key and value are URL encoded
			mg_url_encode(key, buf, sizeof(buf));
			ks_sb_printf(pathAndQuery, "%c%s=", firstQuery ? '?' : '&', buf);

			mg_url_encode(value, buf, sizeof(buf));
			ks_sb_append(pathAndQuery, buf);

			firstQuery = KS_FALSE;
		}

		path = ks_sb_cstr(pathAndQuery);
	}

	mg_printf(conn,
		"%s %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Length: %lu\r\n",
		bwreq->action,
		path,
		host,
		ks_sb_length(bwreq->content));

	if (pathAndQuery) ks_sb_destroy(&pathAndQuery);

	for (ks_hash_iterator_t *it = ks_hash_first(bwreq->headers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key;
		const char *value;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
		mg_printf(conn, "%s: %s\r\n", key, value);
	}

	mg_write(conn, "\r\n", 2);

	mg_write(conn, ks_sb_cstr(bwreq->content), ks_sb_length(bwreq->content));

	if (mg_get_response(conn, buf, sizeof(buf), 1000) <= 0) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ret = blade_webresponse_load(bwresP, conn);

done:
	if (conn) mg_close_connection(conn);
	return ret;
}

KS_DECLARE(ks_status_t) blade_webrequest_oauth2_token_by_credentials_send(ks_bool_t secure, const char *host, ks_port_t port, const char *path, const char *client_id, const char *client_secret, const char **token)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_webrequest_t *bwreq = NULL;
	blade_webresponse_t *bwres = NULL;
	cJSON *json = NULL;
	char *auth = NULL;
	char encoded[1024];
	ks_pool_t *pool = NULL;
	const char *tok = NULL;

	ks_assert(host);
	ks_assert(path);
	ks_assert(client_id);
	ks_assert(client_secret);
	ks_assert(token);

	blade_webrequest_create(&bwreq, "POST", path);

	auth = ks_psprintf(ks_pool_get(bwreq), "%s:%s", client_id, client_secret);
	ks_b64_encode((unsigned char *)auth, strlen(auth), (unsigned char *)encoded, sizeof(encoded));
	ks_pool_free(&auth);

	blade_webrequest_header_printf(bwreq, "Authorization", "Basic %s", encoded);

	json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "grant_type", "client_credentials");
	blade_webrequest_content_json_append(bwreq, json);
	cJSON_Delete(json);

	if ((ret = blade_webrequest_send(bwreq, secure, host, port, &bwres)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = blade_webresponse_content_json_get(bwres, &json)) != KS_STATUS_SUCCESS) goto done;

	if ((tok = cJSON_GetObjectCstr(json, "access_token")) == NULL) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_pool_open(&pool);
	*token = ks_pstrdup(pool, tok);

done:
	if (json) cJSON_Delete(json);
	blade_webrequest_destroy(&bwreq);
	if (bwres) blade_webresponse_destroy(&bwres);

	return ret;
}

KS_DECLARE(ks_status_t) blade_webrequest_oauth2_token_by_code_send(ks_bool_t secure, const char *host, ks_port_t port, const char *path, const char *client_id, const char *client_secret, const char *code, const char **token)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	blade_webrequest_t *bwreq = NULL;
	blade_webresponse_t *bwres = NULL;
	cJSON *json = NULL;
	char *auth = NULL;
	char encoded[1024];
	ks_pool_t *pool = NULL;
	const char *tok = NULL;

	ks_assert(host);
	ks_assert(path);
	ks_assert(client_id);
	ks_assert(client_secret);
	ks_assert(code);
	ks_assert(token);

	blade_webrequest_create(&bwreq, "POST", path);

	auth = ks_psprintf(ks_pool_get(bwreq), "%s:%s", client_id, client_secret);
	ks_b64_encode((unsigned char *)auth, strlen(auth), (unsigned char *)encoded, sizeof(encoded));
	ks_pool_free(&auth);

	blade_webrequest_header_printf(bwreq, "Authorization", "Basic %s", encoded);

	json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "grant_type", "authorization_code");
	cJSON_AddStringToObject(json, "code", code);
	blade_webrequest_content_json_append(bwreq, json);
	cJSON_Delete(json);

	if ((ret = blade_webrequest_send(bwreq, secure, host, port, &bwres)) != KS_STATUS_SUCCESS) goto done;

	if ((ret = blade_webresponse_content_json_get(bwres, &json)) != KS_STATUS_SUCCESS) goto done;

	if ((tok = cJSON_GetObjectCstr(json, "access_token")) == NULL) {
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_pool_open(&pool);
	*token = ks_pstrdup(pool, tok);

done:
	if (json) cJSON_Delete(json);
	blade_webrequest_destroy(&bwreq);
	if (bwres) blade_webresponse_destroy(&bwres);

	return ret;
}


KS_DECLARE(ks_status_t) blade_webresponse_create(blade_webresponse_t **bwresP, const char *status)
{
	ks_pool_t *pool = NULL;
	blade_webresponse_t *bwres = NULL;

	ks_assert(bwresP);
	ks_assert(status);

	ks_pool_open(&pool);
	ks_assert(pool);

	bwres = ks_pool_alloc(pool, sizeof(blade_webresponse_t));

	bwres->status_code = ks_pstrdup(pool, status);
	bwres->status_message = ks_pstrdup(pool, mg_get_response_code_text(NULL, atoi(status)));

	ks_hash_create(&bwres->headers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwres->headers);

	ks_sb_create(&bwres->content, pool, 0);
	ks_assert(bwres->content);

	ks_pool_set_cleanup(bwres, NULL, blade_webresponse_cleanup);

	*bwresP = bwres;

	blade_webresponse_header_add(bwres, "Content-Type", "application/x-www-form-urlencoded");

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webresponse_load(blade_webresponse_t **bwresP, struct mg_connection *conn)
{
	ks_status_t ret = KS_STATUS_SUCCESS;
	ks_pool_t *pool = NULL;
	blade_webresponse_t *bwres = NULL;
	const struct mg_request_info *info = NULL;
	char buf[1024];
	int bytes = 0;

	ks_assert(bwresP);
	ks_assert(conn);

	info = mg_get_request_info(conn);

	ks_pool_open(&pool);
	ks_assert(pool);

	bwres = ks_pool_alloc(pool, sizeof(blade_webrequest_t));

	bwres->status_code = ks_pstrdup(pool, info->request_uri);
	bwres->status_message = ks_pstrdup(pool, info->http_version);

	ks_hash_create(&bwres->headers, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_NOLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY | KS_HASH_FLAG_FREE_VALUE, pool);
	ks_assert(bwres->headers);

	ks_sb_create(&bwres->content, pool, 0);
	ks_assert(bwres->content);

	ks_pool_set_cleanup(bwres, NULL, blade_webresponse_cleanup);

	for (int index = 0; index < info->num_headers; ++index) {
		const struct mg_header *header = &info->http_headers[index];
		ks_hash_insert(bwres->headers, (void *)ks_pstrdup(pool, header->name), (void *)ks_pstrdup(pool, header->value));
	}

	while ((bytes = mg_read(conn, buf, sizeof(buf))) > 0) ks_sb_append_ex(bwres->content, buf, bytes);
	if (bytes < 0) {
		blade_webresponse_destroy(&bwres);
		ret = KS_STATUS_FAIL;
	}
	else *bwresP = bwres;

	return ret;
}

KS_DECLARE(ks_status_t) blade_webresponse_destroy(blade_webresponse_t **bwresP)
{
	blade_webresponse_t *bwres = NULL;
	ks_pool_t *pool;

	ks_assert(bwresP);
	ks_assert(*bwresP);

	bwres = *bwresP;
	*bwresP = NULL;

	pool = ks_pool_get(bwres);

	ks_pool_close(&pool);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webresponse_header_add(blade_webresponse_t *bwres, const char *header, const char *value)
{
	ks_assert(bwres);
	ks_assert(header);
	ks_assert(value);

	ks_hash_insert(bwres->headers, (void *)ks_pstrdup(ks_pool_get(bwres), header), (void *)ks_pstrdup(ks_pool_get(bwres), value));

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(const char *) blade_webresponse_header_get(blade_webresponse_t *bwres, const char *header)
{
	ks_assert(bwres);
	ks_assert(header);

	return (const char *)ks_hash_search(bwres->headers, (void *)header, KS_UNLOCKED);
}

KS_DECLARE(ks_status_t) blade_webresponse_content_json_append(blade_webresponse_t *bwres, cJSON *json)
{
	ks_assert(bwres);
	ks_assert(json);

	blade_webresponse_header_add(bwres, "Content-Type", "application/json");

	ks_sb_json(bwres->content, json);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webresponse_content_string_append(blade_webresponse_t *bwres, const char *str)
{
	ks_assert(bwres);
	ks_assert(str);

	ks_sb_append(bwres->content, str);

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) blade_webresponse_content_json_get(blade_webresponse_t *bwres, cJSON **json)
{
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(bwres);
	ks_assert(json);

	if (!(*json = cJSON_Parse(ks_sb_cstr(bwres->content)))) ret = KS_STATUS_FAIL;

	return ret;
}

KS_DECLARE(ks_status_t) blade_webresponse_send(blade_webresponse_t *bwres, struct mg_connection *conn)
{
	ks_assert(bwres);
	ks_assert(conn);

	mg_printf(conn,
		"HTTP/1.1 %s %s\r\n"
		"Content-Length: %lu\r\n"
		"Connection: close\r\n",
		bwres->status_code,
		bwres->status_message,
		ks_sb_length(bwres->content));

	for (ks_hash_iterator_t *it = ks_hash_first(bwres->headers, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		const char *key;
		const char *value;

		ks_hash_this(it, (const void **)&key, NULL, (void **)&value);
		mg_printf(conn, "%s: %s\r\n", key, value);
	}

	mg_write(conn, "\r\n", 2);

	mg_write(conn, ks_sb_cstr(bwres->content), ks_sb_length(bwres->content));

	return KS_STATUS_SUCCESS;
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
