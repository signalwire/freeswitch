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
#include "mod_openai_asr.h"

static size_t curl_io_write_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    switch_buffer_t *recv_buffer = (switch_buffer_t *)user_data;
    size_t len = (size * nitems);

    if(len > 0 && recv_buffer) {
        switch_buffer_write(recv_buffer, buffer, len);
    }

    return len;
}

switch_status_t curl_perform(switch_buffer_t *recv_buffer, char *api_key, char *model_name, char *filename, globals_t *globals) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    CURL *curl_handle = NULL;
    curl_mime *form = NULL;
    curl_mimepart *field1=NULL, *field2=NULL;
    switch_curl_slist_t *headers = NULL;
    switch_CURLcode curl_ret = 0;
    long http_resp = 0;

    curl_handle = switch_curl_easy_init();
    headers = switch_curl_slist_append(headers, "Content-Type: multipart/form-data");

    switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_io_write_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) recv_buffer);

    if(globals->connect_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, globals->connect_timeout);
    }
    if(globals->request_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals->request_timeout);
    }
    if(globals->user_agent) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, globals->user_agent);
    }
    if(strncasecmp(globals->api_url, "https", 5) == 0) {
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

    if(api_key) {
        curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, api_key);
        curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    }

    if((form = curl_mime_init(curl_handle))) {
        if((field1 = curl_mime_addpart(form))) {
            curl_mime_name(field1, "model");
            curl_mime_data(field1, model_name, CURL_ZERO_TERMINATED);
        }
        if((field2 = curl_mime_addpart(form))) {
            curl_mime_name(field2, "file");
            curl_mime_filedata(field2, filename);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, form);
    }

    headers = switch_curl_slist_append(headers, "Expect:");
    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, globals->api_url);

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

    if(recv_buffer) {
        if(switch_buffer_inuse(recv_buffer) > 0) {
            switch_buffer_write(recv_buffer, "\0", 1);
        }
    }

    if(curl_handle) {
        switch_curl_easy_cleanup(curl_handle);
    }
    if(form) {
        curl_mime_free(form);
    }
    if(headers) {
        switch_curl_slist_free_all(headers);
    }

    return status;
}
