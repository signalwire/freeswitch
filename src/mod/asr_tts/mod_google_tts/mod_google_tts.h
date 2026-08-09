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
#ifndef MOD_GOOGLE_TTS_H
#define MOD_GOOGLE_TTS_H

#include <switch.h>
#include <switch_curl.h>

#define MOD_VERSION         "1.0.1_gcp_api_v1"
#define MOD_CONFIG_NAME     "google_tts.conf"
#define FILE_SIZE_MAX       (2*1024*1024)
#define BASE64_DEC_SZ(n)    ((n*3)/4)

//#define MOD_GTTS_DEBUG

typedef struct {
    switch_memory_pool_t    *pool;
    switch_file_handle_t    *fhnd;
    switch_buffer_t         *curl_recv_buffer;
    char                    *curl_send_buffer_ref;
    char                    *lang_code;
    char                    *gender;
    char                    *voice_name;
    char                    *dst_file;
    char                    *api_key;
    uint32_t                samplerate;
    uint32_t                channels;
    size_t                  curl_send_buffer_len;
    uint8_t                 fl_cache_enabled;
} tts_ctx_t;


/* utils.c */
char *lang2bcp47(const char *lng);
char *fmt_enct2fext(const char *fmt);
char *fmt_gender(const char *gender);
char *fmt_encode(const char *fmt);

char *strnstr(const char *s, const char *find, size_t slen);
char *escape_squotes(const char *string);

#endif
