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
#include "mod_google_asr.h"

static size_t curl_io_write_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)user_data;
    size_t len = (size * nitems);

    if(len > 0 && asr_ctx->curl_recv_buffer_ref) {
        switch_buffer_write(asr_ctx->curl_recv_buffer_ref, buffer, len);
    }

    return len;
}

static size_t curl_io_read_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)user_data;
    size_t nmax = (size * nitems);
    size_t ncur = (asr_ctx->curl_send_buffer_len > nmax) ? nmax : asr_ctx->curl_send_buffer_len;

    if(ncur > 0) {
        memmove(buffer, asr_ctx->curl_send_buffer_ref, ncur);
        asr_ctx->curl_send_buffer_ref += ncur;
        asr_ctx->curl_send_buffer_len -= ncur;
    }

    return ncur;
}

switch_status_t curl_perform(asr_ctx_t *asr_ctx, globals_t *globals) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    CURL *curl_handle = NULL;
    switch_curl_slist_t *headers = NULL;
    char *epurl = NULL;
    switch_CURLcode curl_ret = 0;
    long http_resp = 0;

    if(asr_ctx->api_key) {
        epurl = switch_string_replace(globals->api_url, "${api-key}", asr_ctx->api_key);
    } else {
        epurl = strdup(globals->api_url);
    }

    curl_handle = switch_curl_easy_init();
    headers = switch_curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");

    switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_io_read_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *)asr_ctx);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_io_write_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)asr_ctx);

    if(globals->connect_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, globals->connect_timeout);
    }
    if(globals->request_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals->request_timeout);
    }
    if(globals->user_agent) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, globals->user_agent);
    }
    if(strncasecmp(epurl, "https", 5) == 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
    }
    if(globals->proxy) {
        if(globals->proxy_credentials != NULL) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, globals->proxy_credentials);
        }
        if(strncasecmp(globals->proxy, "https", 5) == 0) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY_SSL_VERIFYPEER, 0);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY, globals->proxy);
    }

    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, epurl);

    curl_ret = switch_curl_easy_perform(curl_handle);
    if(!curl_ret) {
        switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_resp);
        if(!http_resp) { switch_curl_easy_getinfo(curl_handle, CURLINFO_HTTP_CONNECTCODE, &http_resp); }
    } else {
        http_resp = curl_ret;
    }

    if(http_resp != 200) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "http-error=[%ld] (%s)\n", http_resp, globals->api_url);
        status = SWITCH_STATUS_FALSE;
    }

    if(asr_ctx->curl_recv_buffer_ref) {
        if(switch_buffer_inuse(asr_ctx->curl_recv_buffer_ref) > 0) {
            switch_buffer_write(asr_ctx->curl_recv_buffer_ref, "\0", 1);
        }
    }

    if(curl_handle) {
        switch_curl_easy_cleanup(curl_handle);
    }

    if(headers) {
        switch_curl_slist_free_all(headers);
    }

    switch_safe_free(epurl);
    return status;
}
