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

#ifndef _BLADE_WEB_H_
#define _BLADE_WEB_H_
#include <blade.h>

KS_BEGIN_EXTERN_C
KS_DECLARE(ks_status_t) blade_webrequest_create(blade_webrequest_t **bwreqP, const char *action, const char *path);
KS_DECLARE(ks_status_t) blade_webrequest_load(blade_webrequest_t **bwreqP, struct mg_connection *conn);
KS_DECLARE(ks_status_t) blade_webrequest_destroy(blade_webrequest_t **bwreqP);
KS_DECLARE(const char *) blade_webrequest_action_get(blade_webrequest_t *bwreq);
KS_DECLARE(const char *) blade_webrequest_path_get(blade_webrequest_t *bwreq);
KS_DECLARE(ks_status_t) blade_webrequest_query_add(blade_webrequest_t *bwreq, const char *name, const char *value);
KS_DECLARE(const char *) blade_webrequest_query_get(blade_webrequest_t *bwreq, const char *name);
KS_DECLARE(ks_status_t) blade_webrequest_header_add(blade_webrequest_t *bwreq, const char *header, const char *value);
KS_DECLARE(ks_status_t) blade_webrequest_header_printf(blade_webrequest_t *bwreq, const char *header, const char *fmt, ...);
KS_DECLARE(const char *) blade_webrequest_header_get(blade_webrequest_t *bwreq, const char *header);
KS_DECLARE(ks_status_t) blade_webrequest_content_json_append(blade_webrequest_t *bwreq, cJSON *json);
KS_DECLARE(ks_status_t) blade_webrequest_content_string_append(blade_webrequest_t *bwreq, const char *str);
KS_DECLARE(ks_status_t) blade_webrequest_send(blade_webrequest_t *bwreq, ks_bool_t secure, const char *host, ks_port_t port, blade_webresponse_t **bwresP);

KS_DECLARE(ks_status_t) blade_webrequest_oauth2_token_by_credentials_send(ks_bool_t secure, const char *host, ks_port_t port, const char *path, const char *client_id, const char *client_secret, const char **token);
KS_DECLARE(ks_status_t) blade_webrequest_oauth2_token_by_code_send(ks_bool_t secure, const char *host, ks_port_t port, const char *path, const char *client_id, const char *client_secret, const char *code, const char **token);


KS_DECLARE(ks_status_t) blade_webresponse_create(blade_webresponse_t **bwresP, const char *status);
KS_DECLARE(ks_status_t) blade_webresponse_load(blade_webresponse_t **bwresP, struct mg_connection *conn);
KS_DECLARE(ks_status_t) blade_webresponse_destroy(blade_webresponse_t **bwresP);
KS_DECLARE(ks_status_t) blade_webresponse_header_add(blade_webresponse_t *bwres, const char *header, const char *value);
KS_DECLARE(const char *) blade_webresponse_header_get(blade_webresponse_t *bwres, const char *header);
KS_DECLARE(ks_status_t) blade_webresponse_content_json_append(blade_webresponse_t *bwres, cJSON *json);
KS_DECLARE(ks_status_t) blade_webresponse_content_string_append(blade_webresponse_t *bwres, const char *str);
KS_DECLARE(ks_status_t) blade_webresponse_content_json_get(blade_webresponse_t *bwres, cJSON **json);
KS_DECLARE(ks_status_t) blade_webresponse_send(blade_webresponse_t *bwres, struct mg_connection *conn);

KS_END_EXTERN_C

#endif

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
