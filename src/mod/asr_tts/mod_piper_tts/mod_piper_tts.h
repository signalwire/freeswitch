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
#ifndef MOD_PIPER_TTS_H
#define MOD_PIPER_TTS_H

#include <switch.h>

#define MOD_VERSION         "v1.0.2"
#define MOD_CONFIG_NAME     "piper_tts.conf"
#define PIPER_FILE_ENCODING "wav"

//#define MOD_PIPER_TTS_DEBUG


typedef struct {
    switch_mutex_t          *mutex;
    switch_hash_t           *models;
    const char              *tmp_path;
    const char              *cache_path;
    const char              *piper_bin;
    const char              *piper_opts;
    uint8_t                 fl_cache_enabled;
    uint8_t                 fl_voice_as_language;
} piper_globals_t;

typedef struct {
    char                    *lang;
    char                    *path;
} tts_model_info_t;

typedef struct {
    switch_memory_pool_t    *pool;
    switch_file_handle_t    *fhnd;
    char                    *language;
    char                    *model;
    char                    *dst_fname;
    uint32_t                samplerate;
    uint32_t                channels;
    uint8_t                 fl_cache_enabled;
} tts_ctx_t;


#endif
