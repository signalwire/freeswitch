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
#ifndef MOD_OPENAI_TTS_H
#define MOD_OPENAI_TTS_H

#include <switch.h>
#include <switch_curl.h>

#define MOD_VERSION         "1.0.1_api_v1"
#define MOD_CONFIG_NAME     "openai_tts.conf"
#define FILE_SIZE_MAX       (2*1024*1024)

//#define MOD_OAI_TTS_DEBUG

typedef struct {
    char        *lang;
    char        *voice;
    char        *model;
} tts_model_info_t;

typedef struct {
    switch_memory_pool_t    *pool;
    switch_file_handle_t    *fhnd;
    switch_buffer_t         *curl_recv_buffer;
    char                    *curl_send_buffer_ref;
    char                    *language;
    char                    *api_key;
    char                    *voice;
    char                    *model;
    char                    *dst_file;
    uint32_t                samplerate;
    uint32_t                channels;
    size_t                  curl_send_buffer_len;
    uint8_t                 fl_cache_enabled;
} tts_ctx_t;

char *enc2ext(const char *fmt);
char *escape_dquotes(const char *string);

switch_status_t write_file(char *file_name, switch_byte_t *buf, uint32_t buf_len);

#endif
