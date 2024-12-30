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
 * Provides the ability to interact with TTS services over HTTP
 *
 * Development repository:
 * https://github.com/akscf/mod_curl_tts
 *
 */
#include "mod_curl_tts.h"

globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_curl_tts_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_curl_tts_shutdown);
SWITCH_MODULE_DEFINITION(mod_curl_tts, mod_curl_tts_load, mod_curl_tts_shutdown, NULL);

// ---------------------------------------------------------------------------------------------------------------------------------------------
// speech api
// ---------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t speech_open(switch_speech_handle_t *sh, const char *voice, int samplerate, int channels, switch_speech_flag_t *flags) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    tts_ctx_t *tts_ctx = NULL;

    tts_ctx = switch_core_alloc(sh->memory_pool, sizeof(tts_ctx_t));
    tts_ctx->pool = sh->memory_pool;
    tts_ctx->fhnd = switch_core_alloc(tts_ctx->pool, sizeof(switch_file_handle_t));
    tts_ctx->language = (globals.fl_voice_name_as_language && voice) ? switch_core_strdup(sh->memory_pool, voice) : NULL;
    tts_ctx->samplerate = samplerate;
    tts_ctx->channels = channels;
    tts_ctx->api_url = globals.api_url;
    tts_ctx->api_key = globals.api_key;

    sh->private_info = tts_ctx;

    if((status = switch_buffer_create_dynamic(&tts_ctx->curl_recv_buffer, 1024, 8192, globals.file_size_max)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_buffer_create_dynamic() fail\n");
        goto out;
    }

    if((status = switch_core_hash_init(&tts_ctx->curl_params)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_hash_init()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

out:
    return status;
}

static switch_status_t speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *) sh->private_info;
    assert(tts_ctx != NULL);

    if(switch_test_flag(tts_ctx->fhnd, SWITCH_FILE_OPEN)) {
        switch_core_file_close(tts_ctx->fhnd);
    }

    if(tts_ctx->curl_recv_buffer) {
        switch_buffer_destroy(&tts_ctx->curl_recv_buffer);
    }

    if(!globals.fl_cache_enabled) {
        if(tts_ctx->mp3_name) unlink(tts_ctx->mp3_name);
        if(tts_ctx->wav_name) unlink(tts_ctx->wav_name);
    }

    if(tts_ctx->curl_params) {
        switch_core_hash_destroy(&tts_ctx->curl_params);
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char digest[SWITCH_MD5_DIGEST_STRING_SIZE + 1] = { 0 };
    const void *ptr = NULL;
    uint32_t recv_len = 0;

    assert(tts_ctx != NULL);

    switch_md5_string(digest, (void *)text, strlen(text));
    if(!tts_ctx->mp3_name) {
        tts_ctx->mp3_name = switch_core_sprintf(sh->memory_pool, "%s%s%s.mp3", globals.cache_path, SWITCH_PATH_SEPARATOR, digest);
    }
    if(!tts_ctx->wav_name) {
        tts_ctx->wav_name = switch_core_sprintf(sh->memory_pool, "%s%s%s.wav", globals.cache_path, SWITCH_PATH_SEPARATOR, digest);
    }

    if(switch_file_exists(tts_ctx->mp3_name, tts_ctx->pool) == SWITCH_STATUS_SUCCESS) {
        if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->mp3_name, tts_ctx->channels, tts_ctx->samplerate,
                                           (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file: %s\n", tts_ctx->mp3_name);
            status = SWITCH_STATUS_FALSE;
            goto out;
        }
    } else if(switch_file_exists(tts_ctx->wav_name, tts_ctx->pool) == SWITCH_STATUS_SUCCESS) {
        if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->wav_name, tts_ctx->channels, tts_ctx->samplerate,
                                           (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file: %s\n", tts_ctx->wav_name);
            status = SWITCH_STATUS_FALSE;
            goto out;
        }
    } else {
        switch_buffer_zero(tts_ctx->curl_recv_buffer);
        status = curl_perform(tts_ctx , text);
        recv_len = switch_buffer_peek_zerocopy(tts_ctx->curl_recv_buffer, &ptr);

        if(status == SWITCH_STATUS_SUCCESS) {
            char *dst_name = NULL;

            if(strcasecmp(tts_ctx->media_ctype, "audio/mpeg") == 0) {
                dst_name = tts_ctx->mp3_name;
            } else if(strcasecmp(tts_ctx->media_ctype, "audio/wav") == 0) {
                dst_name = tts_ctx->wav_name;
            } else {
                status = SWITCH_STATUS_FALSE;
            }

            if(status == SWITCH_STATUS_SUCCESS) {
                if((status = write_file(dst_name, (switch_byte_t *)ptr, recv_len)) == SWITCH_STATUS_SUCCESS) {
                    if((status = switch_core_file_open(tts_ctx->fhnd, dst_name, tts_ctx->channels, tts_ctx->samplerate,
                                                       (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file: %s\n", dst_name);
                        goto out;
                    }
                }
            } else {
                if(!dst_name) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported media-type (%s)\n", tts_ctx->media_ctype);
                } else if(globals.fl_log_http_error) {
                    if(recv_len > 0) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Services response: %s\n", (char *)ptr);
                    }
                }
            }
        }
    }
out:
    return status;
}

static switch_status_t speech_read_tts(switch_speech_handle_t *sh, void *data, size_t *data_len, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;
    size_t len = (*data_len / sizeof(int16_t));

    assert(tts_ctx != NULL);

    if(tts_ctx->fhnd->file_interface == NULL) {
        return SWITCH_STATUS_FALSE;
    }

    if(switch_core_file_read(tts_ctx->fhnd, data, &len) != SWITCH_STATUS_SUCCESS) {
        switch_core_file_close(tts_ctx->fhnd);
        return SWITCH_STATUS_FALSE;
    }

    *data_len = (len * sizeof(int16_t));
    if(!data_len) {
        switch_core_file_close(tts_ctx->fhnd);
        return SWITCH_STATUS_BREAK;
    }

    return SWITCH_STATUS_SUCCESS;
}

static void speech_flush_tts(switch_speech_handle_t *sh) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;

    assert(tts_ctx != NULL);

    if(tts_ctx->fhnd != NULL && tts_ctx->fhnd->file_interface != NULL) {
        switch_core_file_close(tts_ctx->fhnd);
    }
}

static void speech_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;

    assert(tts_ctx != NULL);

    if(strcasecmp(param, "url") == 0) {
        if(val) tts_ctx->api_url = switch_core_strdup(sh->memory_pool, val);
    } else if(strcasecmp(param, "key") == 0) {
        if(val) tts_ctx->api_key = switch_core_strdup(sh->memory_pool, val);
    } else if(strcasecmp(param, "language") == 0) {
        if(val) tts_ctx->api_key = switch_core_strdup(sh->memory_pool, val);
    } else if(strcasecmp(param, "text") == 0) {
        // reserved (ignore)
    } else {
        if(tts_ctx->curl_params && val) {
            switch_core_hash_insert(tts_ctx->curl_params, param, switch_core_strdup(sh->memory_pool, val));
        }
    }
}

static void speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val) {
}

static void speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val) {
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
SWITCH_MODULE_LOAD_FUNCTION(mod_curl_tts_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg, xml, settings, param;
    switch_speech_interface_t *speech_interface;

    memset(&globals, 0, sizeof(globals));
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);

    if((xml = switch_xml_open_cfg(MOD_CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open configuration: %s\n", MOD_CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if((settings = switch_xml_child(cfg, "settings"))) {
        for (param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            if(!strcasecmp(var, "api-url")) {
                if(val) globals.api_url = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "api-key")) {
                if(val) globals.api_key = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "cache-path")) {
                if(val) globals.cache_path = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "user-agent")) {
                if(val) globals.user_agent = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "request-timeout")) {
                if(val) globals.request_timeout = atoi(val);
            } else if(!strcasecmp(var, "connect-timeout")) {
                if(val) globals.connect_timeout = atoi(val);
            } else if(!strcasecmp(var, "voice-name-as-language")) {
                if(val) globals.fl_voice_name_as_language = switch_true(val);
            } else if(!strcasecmp(var, "log-http-errors")) {
                if(val) globals.fl_log_http_error = switch_true(val);
            } else if(!strcasecmp(var, "cache-enable")) {
                if(val) globals.fl_cache_enabled = switch_true(val);
            } else if(!strcasecmp(var, "file-size-max")) {
                if(val) globals.file_size_max = atoi(val);
            } else if(!strcasecmp(var, "proxy")) {
                if(val) globals.proxy = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "proxy-credentials")) {
                if(val) globals.proxy_credentials = switch_core_strdup(pool, val);
            }
        }
    }

    globals.tmp_path = SWITCH_GLOBAL_dirs.temp_dir;
    globals.cache_path = (globals.cache_path == NULL ? "/tmp/curl-tts-cache" : globals.cache_path);
    globals.file_size_max = globals.file_size_max > 0 ? globals.file_size_max : FILE_SIZE_MAX;

    if(switch_directory_exists(globals.cache_path, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_dir_make(globals.cache_path, SWITCH_FPROT_OS_DEFAULT, NULL);
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
    speech_interface->interface_name = "curl";

    speech_interface->speech_open = speech_open;
    speech_interface->speech_close = speech_close;
    speech_interface->speech_feed_tts = speech_feed_tts;
    speech_interface->speech_read_tts = speech_read_tts;
    speech_interface->speech_flush_tts = speech_flush_tts;

    speech_interface->speech_text_param_tts = speech_text_param_tts;
    speech_interface->speech_numeric_param_tts = speech_numeric_param_tts;
    speech_interface->speech_float_param_tts = speech_float_param_tts;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "CURL-TTS (%s)\n", MOD_VERSION);
out:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_curl_tts_shutdown) {

    return SWITCH_STATUS_SUCCESS;
}
