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
#ifndef MOD_CURL_TTS_H
#define MOD_CURL_TTS_H

#include <switch.h>
#include <switch_curl.h>

#define MOD_VERSION         "1.0.0"
#define MOD_CONFIG_NAME     "curl_tts.conf"
#define FILE_SIZE_MAX       (2*1024*1024)

typedef struct {
    switch_mutex_t          *mutex;
    char                    *cache_path;
    char                    *tmp_path;
    char                    *user_agent;
    char                    *api_url;
    char                    *api_key;
    char                    *proxy;
    char                    *proxy_credentials;
    uint32_t                file_size_max;
    uint32_t                request_timeout;            // seconds
    uint32_t                connect_timeout;            // seconds
    uint8_t                 fl_voice_name_as_language;
    uint8_t                 fl_log_http_error;
    uint8_t                 fl_cache_enabled;
} globals_t;

typedef struct {
    switch_memory_pool_t    *pool;
    switch_file_handle_t    *fhnd;
    switch_buffer_t         *curl_recv_buffer;
    switch_hash_t           *curl_params;
    char                    *curl_send_buffer_ref;
    char                    *api_url;
    char                    *api_key;
    char                    *language;
    char                    *mp3_name;
    char                    *wav_name;
    char                    *media_ctype;
    uint32_t                samplerate;
    uint32_t                channels;
    size_t                  curl_send_buffer_len;
} tts_ctx_t;


char *escape_dquotes(const char *string);
switch_status_t write_file(char *file_name, switch_byte_t *buf, uint32_t buf_len);
switch_status_t curl_perform(tts_ctx_t *tts_ctx, char *text);


#endif
