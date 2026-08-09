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
 * Google Text-To-Speech service for the Freeswitch
 * https://cloud.google.com/text-to-speech/docs/reference/rest
 *
 * Development repository:
 * https://github.com/akscf/mod_google_tts
 *
 */
#include "mod_google_tts.h"

static struct {
    char                    *file_ext;
    char                    *cache_path;
    char                    *tmp_path;
    char                    *opt_gender;
    char                    *opt_encoding;
    char                    *user_agent;
    char                    *api_url;
    char                    *api_key;
    char                    *proxy;
    char                    *proxy_credentials;
    uint32_t                file_size_max;
    uint32_t                request_timeout;        // seconds
    uint32_t                connect_timeout;        // seconds
    uint8_t                 fl_voice_name_as_lang;
    uint8_t                 fl_log_http_error;
    uint8_t                 fl_cache_enabled;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_google_tts_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_tts_shutdown);
SWITCH_MODULE_DEFINITION(mod_google_tts, mod_google_tts_load, mod_google_tts_shutdown, NULL);


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

static switch_status_t curl_perform(tts_ctx_t *tts_ctx, char *text) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    CURL *curl_handle = NULL;
    switch_curl_slist_t *headers = NULL;
    switch_CURLcode curl_ret = 0;
    long http_resp = 0;
    const char *xgender = (tts_ctx->gender ? tts_ctx->gender : globals.opt_gender);
    const char *ygender = (!globals.fl_voice_name_as_lang && tts_ctx->voice_name) ? tts_ctx->voice_name : NULL;
    char *pdata = NULL;
    char *qtext = NULL;
    char *epurl = NULL;

    if(text) {
        qtext = escape_squotes(text);
    }

    if(tts_ctx->api_key) {
        epurl = switch_string_replace(globals.api_url, "${api-key}", tts_ctx->api_key);
    } else {
        epurl = strdup(globals.api_url);
    }

    pdata = switch_mprintf( "{'input':{'text':'%s'},'voice':{'ssmlGender':'%s', 'languageCode':'%s'},'audioConfig':{'audioEncoding':'%s', 'sampleRateHertz':'%d'}}\n\n",
                qtext ? qtext : "",
                ygender ? ygender : xgender,
                tts_ctx->lang_code,
                globals.opt_encoding,
                tts_ctx->samplerate
            );

#ifdef MOD_GTTS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CURL: URL=[%s], PDATA=[%s]\n", epurl, pdata);
#endif

    tts_ctx->curl_send_buffer_len = strlen(pdata);
    tts_ctx->curl_send_buffer_ref = pdata;

    curl_handle = switch_curl_easy_init();

    headers = switch_curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = switch_curl_slist_append(headers, "Expect:");

    switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

    switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, tts_ctx->curl_send_buffer_len);
    switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *)pdata);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_io_read_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *)tts_ctx);

    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_io_write_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)tts_ctx);

    if(globals.connect_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, globals.connect_timeout);
    }
    if(globals.request_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, globals.request_timeout);
    }
    if(globals.user_agent) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, globals.user_agent);
    }
    if(strncasecmp(epurl, "https", 5) == 0) {
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

    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, epurl);

    curl_ret = switch_curl_easy_perform(curl_handle);
    if(!curl_ret) {
        switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_resp);
        if(!http_resp) { switch_curl_easy_getinfo(curl_handle, CURLINFO_HTTP_CONNECTCODE, &http_resp); }
    } else {
        http_resp = curl_ret;
    }

    if(http_resp != 200) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "http-error=[%ld] (%s)\n", http_resp, globals.api_url);
        status = SWITCH_STATUS_FALSE;
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
    switch_safe_free(epurl);
    return status;
}

static switch_status_t extract_audio(tts_ctx_t *tts_ctx, char *buf_in, uint32_t buf_len) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_memory_pool_t *pool = tts_ctx->pool;
    switch_file_t *fd = NULL;
    char *buf_out = NULL, *ptr = NULL;
    size_t len = buf_len, dec_len = 0;
    uint32_t ofs1 = 0, ofs2 = 0;

    if((ptr = strnstr(buf_in, "\"audioContent\"", len)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed media content\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    for(ofs1 = ((ptr - buf_in) + 14); ofs1 < len; ofs1++) {
        if(buf_in[ofs1] == '"') { ofs1++; break; }
    }
    if(ofs1 >= len) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed media content\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    for(ofs2 = len; ofs2 > ofs1; ofs2--) {
        if(buf_in[ofs2] == '"') { buf_in[ofs2]='\0'; ofs2--; break; }
    }
    if(ofs2 <= ofs1) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed media content\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    ptr = (void *)(buf_in + ofs1);
    len = (ofs2 - ofs1);
    dec_len = BASE64_DEC_SZ(len);

    if(dec_len < 4 ) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Malformed media content\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    if((buf_out = switch_core_alloc(pool, dec_len)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_alloc() failed\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    len = switch_b64_decode(ptr, buf_out, dec_len);
    if(len != dec_len) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "switch_b64_decode: (len != dec_len)\n");
        dec_len = len;
    }

    status = switch_file_open(&fd, tts_ctx->dst_file,
                              (SWITCH_FOPEN_WRITE | SWITCH_FOPEN_CREATE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_BINARY),
                              (SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE), pool);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to create output file (%s)\n", tts_ctx->dst_file);
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    status = switch_file_write(fd, buf_out, &len);
    if(status != SWITCH_STATUS_SUCCESS || len != dec_len) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to write into file (%s)\n", tts_ctx->dst_file);
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

out:
    if(fd) {
        switch_file_close(fd);
    }
    return status;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// speech api
// ---------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t speech_open(switch_speech_handle_t *sh, const char *voice, int samplerate, int channels, switch_speech_flag_t *flags) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    tts_ctx_t *tts_ctx = NULL;

    tts_ctx = switch_core_alloc(sh->memory_pool, sizeof(tts_ctx_t));

    tts_ctx->pool = sh->memory_pool;
    tts_ctx->fhnd = switch_core_alloc(tts_ctx->pool, sizeof(switch_file_handle_t));
    tts_ctx->voice_name = switch_core_strdup(tts_ctx->pool, voice);
    tts_ctx->lang_code = (globals.fl_voice_name_as_lang && voice) ? switch_core_strdup(sh->memory_pool, lang2bcp47(voice)) : "en-gb";
    tts_ctx->api_key = globals.api_key;
    tts_ctx->channels = channels;
    tts_ctx->samplerate = samplerate;
    tts_ctx->fl_cache_enabled = globals.fl_cache_enabled;

    sh->private_info = tts_ctx;

    if((status = switch_buffer_create_dynamic(&tts_ctx->curl_recv_buffer, 1024, 8192, globals.file_size_max)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_buffer_create_dynamic()\n");
        goto out;
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

    if(!tts_ctx->fl_cache_enabled) {
        if(tts_ctx->dst_file) unlink(tts_ctx->dst_file);
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char digest[SWITCH_MD5_DIGEST_STRING_SIZE + 1] = { 0 };
    char uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
    const void *ptr = NULL;
    uint32_t recv_len = 0;

    assert(tts_ctx != NULL);

    if(tts_ctx->fl_cache_enabled) {
        switch_md5_string(digest, (void *)text, strlen(text));
        tts_ctx->dst_file = switch_core_sprintf(sh->memory_pool, "%s%s%s.%s", globals.cache_path, SWITCH_PATH_SEPARATOR, digest, globals.file_ext);
    } else {
        switch_uuid_str((char *)uuid, sizeof(uuid));
        tts_ctx->dst_file = switch_core_sprintf(sh->memory_pool, "%s%s%s.%s", globals.tmp_path, SWITCH_PATH_SEPARATOR, uuid, globals.file_ext);
    }

    if(switch_file_exists(tts_ctx->dst_file, tts_ctx->pool) == SWITCH_STATUS_SUCCESS) {
        if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->dst_file, 0, tts_ctx->samplerate, (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) == SWITCH_STATUS_SUCCESS) {
            goto out;
        }
    }

#ifdef MOD_GTTS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "language=[%s]\n", tts_ctx->lang_code);
#endif

    switch_buffer_zero(tts_ctx->curl_recv_buffer);
    status = curl_perform(tts_ctx , text);
    recv_len = switch_buffer_peek_zerocopy(tts_ctx->curl_recv_buffer, &ptr);

    if(status == SWITCH_STATUS_SUCCESS) {
        if((status = extract_audio(tts_ctx, (char *)ptr, recv_len)) == SWITCH_STATUS_SUCCESS) {
            if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->dst_file, 0, tts_ctx->samplerate, (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file (%s)\n", tts_ctx->dst_file);
                switch_goto_status(SWITCH_STATUS_FALSE, out);
            }
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to extract media\n");
            status = SWITCH_STATUS_FALSE;
        }
    } else {
        if(globals.fl_log_http_error && recv_len > 0) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Service response: %s\n", (char *)ptr);
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

    if(strcasecmp(param, "key") == 0) {
        if(val) {  tts_ctx->api_key = switch_core_strdup(sh->memory_pool, val); }
    } else if(strcasecmp(param, "lang") == 0) {
        if(val) tts_ctx->lang_code = switch_core_strdup(sh->memory_pool, lang2bcp47(val));
    } else if(strcasecmp(param, "gender") == 0) {
        if(val) tts_ctx->gender = switch_core_strdup(sh->memory_pool, fmt_gender(val));
    } else if(strcasecmp(param, "cache") == 0) {
        if(val) tts_ctx->fl_cache_enabled = switch_true(val);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported parameter [%s]\n", param);
    }
}

static void speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val) {
}

static void speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val) {
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
SWITCH_MODULE_LOAD_FUNCTION(mod_google_tts_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg, xml, settings, param;
    switch_speech_interface_t *speech_interface;

    memset(&globals, 0, sizeof(globals));

    if((xml = switch_xml_open_cfg(MOD_CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open configuration: %s\n", MOD_CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if((settings = switch_xml_child(cfg, "settings"))) {
        for (param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *)switch_xml_attr_soft(param, "name");
            char *val = (char *)switch_xml_attr_soft(param, "value");

            if(!strcasecmp(var, "api-url")) {
                if(val) globals.api_url = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "api-key")) {
                if(val) globals.api_key = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "cache-path")) {
                if(val) globals.cache_path = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "gender")) {
                if(val) globals.opt_gender = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "encoding")) {
                if(val) globals.opt_encoding = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "user-agent")) {
                if(val) globals.user_agent = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "request-timeout")) {
                if(val) globals.request_timeout = atoi(val);
            } else if(!strcasecmp(var, "connect-timeout")) {
                if(val) globals.connect_timeout = atoi(val);
            } else if(!strcasecmp(var, "voice-name-as-language")) {
                if(val) globals.fl_voice_name_as_lang = switch_true(val);
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

    if(!globals.api_url) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required parameter: api-url\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    globals.tmp_path = SWITCH_GLOBAL_dirs.temp_dir;
    globals.cache_path = (globals.cache_path == NULL ? "/tmp/google-tts-cache" : globals.cache_path);
    globals.opt_gender = fmt_gender(globals.opt_gender == NULL ? "female" : globals.opt_gender);
    globals.opt_encoding = fmt_encode(globals.opt_encoding == NULL ? "mp3" : globals.opt_encoding);
    globals.file_size_max = globals.file_size_max > 0 ? globals.file_size_max : FILE_SIZE_MAX;
    globals.file_ext = fmt_enct2fext(globals.opt_encoding);

    if(switch_directory_exists(globals.cache_path, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_dir_make(globals.cache_path, SWITCH_FPROT_OS_DEFAULT, NULL);
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
    speech_interface->interface_name = "google";

    speech_interface->speech_open = speech_open;
    speech_interface->speech_close = speech_close;
    speech_interface->speech_feed_tts = speech_feed_tts;
    speech_interface->speech_read_tts = speech_read_tts;
    speech_interface->speech_flush_tts = speech_flush_tts;

    speech_interface->speech_text_param_tts = speech_text_param_tts;
    speech_interface->speech_numeric_param_tts = speech_numeric_param_tts;
    speech_interface->speech_float_param_tts = speech_float_param_tts;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "GoogleTTS (%s)\n", MOD_VERSION);
out:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_tts_shutdown) {

    return SWITCH_STATUS_SUCCESS;
}
