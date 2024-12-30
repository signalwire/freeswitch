/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * Module Contributor(s):
 *  Konstantin Alexandrin <akscfx@gmail.com>
 *
 *
 */
#include "mod_curl_tts.h"

extern globals_t globals;

static size_t curl_io_write_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)user_data;
    size_t len = (size * nitems);

    if(len > 0 && tts_ctx->curl_recv_buffer) {
        switch_buffer_write(tts_ctx->curl_recv_buffer, buffer, len);
    }

    return len;
}

static size_t curl_io_read_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)user_data;
    size_t nmax = (size * nitems);
    size_t ncur = (tts_ctx->curl_send_buffer_len > nmax) ? nmax : tts_ctx->curl_send_buffer_len;

    memmove(buffer, tts_ctx->curl_send_buffer_ref, ncur);
    tts_ctx->curl_send_buffer_ref += ncur;
    tts_ctx->curl_send_buffer_len -= ncur;

    return ncur;
}


switch_status_t curl_perform(tts_ctx_t *tts_ctx, char *text) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    CURL *curl_handle = NULL;
    switch_curl_slist_t *headers = NULL;
    switch_CURLcode curl_ret = 0;
    long http_resp = 0;
    char *pdata = NULL;
    char *qtext = NULL;

    if(!tts_ctx->api_url) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "api_url not determined\n");
        return SWITCH_STATUS_FALSE;
    }

    if(text) {
        qtext = escape_dquotes(text);
    }

    if(tts_ctx->curl_params && !switch_core_hash_empty(tts_ctx->curl_params)) {
        const void *hkey = NULL; void *hval = NULL;
        switch_hash_index_t *hidx = NULL;
        cJSON *jopts = NULL;

        jopts = cJSON_CreateObject();
        for(hidx = switch_core_hash_first_iter(tts_ctx->curl_params, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
            switch_core_hash_this(hidx, &hkey, NULL, &hval);
            if(hkey && hval) {
                cJSON_AddItemToObject(jopts, (char *)hkey, cJSON_CreateString((char *)hval));
            }
        }

        cJSON_AddItemToObject(jopts, "language", cJSON_CreateString((char *)tts_ctx->language));
        cJSON_AddItemToObject(jopts, "text", cJSON_CreateString((char *)qtext));

        pdata = cJSON_PrintUnformatted(jopts);
        cJSON_Delete(jopts);
    } else {
        cJSON *jopts = NULL;

        jopts = cJSON_CreateObject();

        cJSON_AddItemToObject(jopts, "language", cJSON_CreateString((char *)tts_ctx->language));
        cJSON_AddItemToObject(jopts, "text", cJSON_CreateString((char *)qtext));

        pdata = cJSON_PrintUnformatted(jopts);
        cJSON_Delete(jopts);
    }

    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "POST: [%s]\n", pdata);

    tts_ctx->media_ctype = NULL;
    tts_ctx->curl_send_buffer_len = strlen(pdata);
    tts_ctx->curl_send_buffer_ref = pdata;

    curl_handle = switch_curl_easy_init();

    headers = switch_curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = switch_curl_slist_append(headers, "Expect:");

    switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

    switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, tts_ctx->curl_send_buffer_len);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *) pdata);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_io_read_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *) tts_ctx);

    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_io_write_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) tts_ctx);

    if(globals.connect_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, globals.connect_timeout);
    }
    if(globals.request_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals.request_timeout);
    }
    if(globals.user_agent) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, globals.user_agent);
    }

    if(strncasecmp(tts_ctx->api_url, "https", 5) == 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
    }
    if(globals.proxy) {
        if(globals.proxy_credentials != NULL) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, globals.proxy_credentials);
        }
        if(strncasecmp(globals.proxy, "https", 5) == 0) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY_SSL_VERIFYPEER, 0);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY, globals.proxy);
    }

    if(tts_ctx->api_key) {
        curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, tts_ctx->api_key);
        curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    }

    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, tts_ctx->api_url);

    curl_ret = switch_curl_easy_perform(curl_handle);
    if(!curl_ret) {
        switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_resp);
        if(!http_resp) { switch_curl_easy_getinfo(curl_handle, CURLINFO_HTTP_CONNECTCODE, &http_resp); }
    } else {
        http_resp = curl_ret;
    }

    if(http_resp != 200) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "http-error=[%ld] (%s)\n", http_resp, tts_ctx->api_url);
        status = SWITCH_STATUS_FALSE;
    } else {
        char *ct = NULL;
        if(!curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct) && ct) {
            tts_ctx->media_ctype = switch_core_strdup(tts_ctx->pool, ct);
        }
    }

    if(tts_ctx->curl_recv_buffer) {
        if(switch_buffer_inuse(tts_ctx->curl_recv_buffer) > 0) {
            switch_buffer_write(tts_ctx->curl_recv_buffer, "\0", 1);
        }
    }

    if(curl_handle) { switch_curl_easy_cleanup(curl_handle); }
    if(headers) { switch_curl_slist_free_all(headers); }

    switch_safe_free(pdata);
    switch_safe_free(qtext);

    return status;
}

