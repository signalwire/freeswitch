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
 * OpenAI Speech-To-Text service for the Freeswitch.
 * https://platform.openai.com/docs/guides/speech-to-text
 *
 * Development respository:
 * https://github.com/akscf/mod_openai_asr
 *
 */
#include "mod_openai_asr.h"

globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_openai_asr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openai_asr_shutdown);
SWITCH_MODULE_DEFINITION(mod_openai_asr, mod_openai_asr_load, mod_openai_asr_shutdown, NULL);

static void *SWITCH_THREAD_FUNC transcribe_thread(switch_thread_t *thread, void *obj) {
    volatile asr_ctx_t *_ref = (asr_ctx_t *)obj;
    asr_ctx_t *asr_ctx = (asr_ctx_t *)_ref;
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_buffer_t *chunk_buffer = NULL;
    switch_buffer_t *curl_recv_buffer = NULL;
    switch_memory_pool_t *pool = NULL;
    cJSON *json = NULL;
    time_t sentence_timeout = 0;
    uint32_t schunks = 0;
    uint32_t chunk_buffer_size = 0;
    uint8_t fl_cbuff_overflow = SWITCH_FALSE;
    void *pop = NULL;

    switch_mutex_lock(asr_ctx->mutex);
    asr_ctx->refs++;
    switch_mutex_unlock(asr_ctx->mutex);

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto out;
    }
    if(switch_buffer_create_dynamic(&curl_recv_buffer, 1024, 2048, 8192) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_buffer_create_dynamic()\n");
        goto out;
    }

    while(SWITCH_TRUE) {
        if(globals.fl_shutdown || asr_ctx->fl_destroyed) {
            break;
        }
        if(chunk_buffer_size == 0) {
            switch_mutex_lock(asr_ctx->mutex);
            chunk_buffer_size = asr_ctx->chunk_buffer_size;
            switch_mutex_unlock(asr_ctx->mutex);

            if(chunk_buffer_size > 0) {
                if(switch_buffer_create(pool, &chunk_buffer, chunk_buffer_size) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_buffer_create()\n");
                    break;
                }
                switch_buffer_zero(chunk_buffer);
            }
            goto timer_next;
        }

        fl_cbuff_overflow = SWITCH_FALSE;
        while(switch_queue_trypop(asr_ctx->q_audio, &pop) == SWITCH_STATUS_SUCCESS) {
            xdata_buffer_t *audio_buffer = (xdata_buffer_t *)pop;
            if(globals.fl_shutdown || asr_ctx->fl_destroyed ) {
                xdata_buffer_free(&audio_buffer);
                break;
            }
            if(audio_buffer && audio_buffer->len) {
                if(switch_buffer_write(chunk_buffer, audio_buffer->data, audio_buffer->len) >= chunk_buffer_size) {
                    fl_cbuff_overflow = SWITCH_TRUE;
                    break;
                }
                schunks++;
            }
            xdata_buffer_free(&audio_buffer);
        }

        if(fl_cbuff_overflow) {
            sentence_timeout = 1;
        } else {
            if(schunks && asr_ctx->vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
                if(!sentence_timeout) {
                    sentence_timeout = asr_ctx->silence_sec + switch_epoch_time_now(NULL);
                }
            }
            if(sentence_timeout && (asr_ctx->vad_state == SWITCH_VAD_STATE_START_TALKING || asr_ctx->vad_state == SWITCH_VAD_STATE_TALKING)) {
                sentence_timeout = 0;
            }
        }

        if(sentence_timeout && sentence_timeout <= switch_epoch_time_now(NULL)) {
            const void *chunk_buffer_ptr = NULL;
            const void *http_response_ptr = NULL;
            uint32_t buf_len = 0, http_recv_len = 0, stt_failed = 0;
            char *chunk_fname = NULL;

            if((buf_len = switch_buffer_peek_zerocopy(chunk_buffer, &chunk_buffer_ptr)) > 0 && chunk_buffer_ptr) {
                chunk_fname = chunk_write((switch_byte_t *)chunk_buffer_ptr, buf_len, asr_ctx->channels, asr_ctx->samplerate, globals.opt_encoding);
            }
            if(chunk_fname) {
                for(uint32_t rqtry = 0; rqtry < asr_ctx->retries_on_error; rqtry++) {
                    switch_buffer_zero(curl_recv_buffer);
                    status = curl_perform(curl_recv_buffer, asr_ctx->opt_api_key, asr_ctx->opt_model, chunk_fname, &globals);
                    if(status == SWITCH_STATUS_SUCCESS || globals.fl_shutdown || asr_ctx->fl_destroyed) { break; }
                    switch_yield(1000);
                }

                http_recv_len = switch_buffer_peek_zerocopy(curl_recv_buffer, &http_response_ptr);
                if(status == SWITCH_STATUS_SUCCESS) {
                    if(http_response_ptr && http_recv_len) {
                        char *txt = parse_response((char *)http_response_ptr, NULL);
#ifdef MOD_OPENAI_ASR_DEBUG
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Service response [%s]\n", (char *)http_response_ptr);
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Text [%s]\n", txt ? txt : "null");
#endif
                        if(!txt) txt = strdup("");
                        if(switch_queue_trypush(asr_ctx->q_text, txt) == SWITCH_STATUS_SUCCESS) {
                            switch_mutex_lock(asr_ctx->mutex);
                            asr_ctx->transcription_results++;
                            switch_mutex_unlock(asr_ctx->mutex);
                        } else {
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Queue is full!\n");
                            switch_safe_free(txt);
                        }
                    } else {
                        stt_failed = 1;
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Empty service response!\n");
                    }
                } else {
                    stt_failed = 1;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to perform request!\n");
                }

                if(stt_failed) {
                    char *txt = strdup("[transcription failed]");
                    if(switch_queue_trypush(asr_ctx->q_text, txt) == SWITCH_STATUS_SUCCESS) {
                        switch_mutex_lock(asr_ctx->mutex);
                        asr_ctx->transcription_results++;
                        switch_mutex_unlock(asr_ctx->mutex);
                    } else {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Queue is full!\n");
                        switch_safe_free(txt);
                    }
                }

                schunks = 0;
                sentence_timeout = 0;
                unlink(chunk_fname);
                switch_safe_free(chunk_fname);
                switch_buffer_zero(chunk_buffer);
            }
        }

        timer_next:
        switch_yield(10000);
    }

out:
    if(json != NULL) {
        cJSON_Delete(json);
    }
    if(curl_recv_buffer) {
        switch_buffer_destroy(&curl_recv_buffer);
    }
    if(chunk_buffer) {
        switch_buffer_destroy(&chunk_buffer);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }

    switch_mutex_lock(asr_ctx->mutex);
    if(asr_ctx->refs > 0) asr_ctx->refs--;
    switch_mutex_unlock(asr_ctx->mutex);

    switch_mutex_lock(globals.mutex);
    if(globals.active_threads) globals.active_threads--;
    switch_mutex_unlock(globals.mutex);

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t asr_open(switch_asr_handle_t *ah, const char *codec, int samplerate, const char *dest, switch_asr_flag_t *flags) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;
    asr_ctx_t *asr_ctx = NULL;

    if(strcmp(codec, "L16") !=0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported encoding (%s)\n", codec);
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    if((asr_ctx = switch_core_alloc(ah->memory_pool, sizeof(asr_ctx_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_alloc()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    asr_ctx->channels = 1;
    asr_ctx->chunk_buffer_size = 0;
    asr_ctx->samplerate = samplerate;
    asr_ctx->silence_sec = globals.speech_silence_sec;
    asr_ctx->retries_on_error = globals.retries_on_error;

    asr_ctx->opt_model = globals.opt_model;
    asr_ctx->opt_api_key = globals.api_key;

   if((status = switch_mutex_init(&asr_ctx->mutex, SWITCH_MUTEX_NESTED, ah->memory_pool)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_mutex_init()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    switch_queue_create(&asr_ctx->q_audio, QUEUE_SIZE, ah->memory_pool);
    switch_queue_create(&asr_ctx->q_text, QUEUE_SIZE, ah->memory_pool);

    asr_ctx->vad_buffer = NULL;
    asr_ctx->frame_len = 0;
    asr_ctx->vad_buffer_size = 0;
    asr_ctx->vad_stored_frames = 0;
    asr_ctx->fl_vad_first_cycle = SWITCH_TRUE;

    if((asr_ctx->vad = switch_vad_init(asr_ctx->samplerate, asr_ctx->channels)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_vad_init()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }
    switch_vad_set_mode(asr_ctx->vad, -1);
    switch_vad_set_param(asr_ctx->vad, "debug", globals.fl_vad_debug);
    if(globals.vad_silence_ms > 0)  { switch_vad_set_param(asr_ctx->vad, "silence_ms", globals.vad_silence_ms); }
    if(globals.vad_voice_ms > 0)    { switch_vad_set_param(asr_ctx->vad, "voice_ms", globals.vad_voice_ms); }
    if(globals.vad_threshold > 0)   { switch_vad_set_param(asr_ctx->vad, "thresh", globals.vad_threshold); }

    ah->private_info = asr_ctx;

    switch_mutex_lock(globals.mutex);
    globals.active_threads++;
    switch_mutex_unlock(globals.mutex);

    switch_threadattr_create(&attr, ah->memory_pool);
    switch_threadattr_detach_set(attr, 1);
    switch_threadattr_stacksize_set(attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, attr, transcribe_thread, asr_ctx, ah->memory_pool);

out:
    return status;
}

static switch_status_t asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;
    uint8_t fl_wloop = SWITCH_TRUE;

    assert(asr_ctx != NULL);

    asr_ctx->fl_abort = SWITCH_TRUE;
    asr_ctx->fl_destroyed = SWITCH_TRUE;

    switch_mutex_lock(asr_ctx->mutex);
    fl_wloop = (asr_ctx->refs != 0);
    switch_mutex_unlock(asr_ctx->mutex);

    if(fl_wloop) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for unlock (refs=%d)...\n", asr_ctx->refs);
        while(fl_wloop) {
            switch_mutex_lock(asr_ctx->mutex);
            fl_wloop = (asr_ctx->refs != 0);
            switch_mutex_unlock(asr_ctx->mutex);
            switch_yield(100000);
        }
    }

    if(asr_ctx->q_audio) {
        xdata_buffer_queue_clean(asr_ctx->q_audio);
        switch_queue_term(asr_ctx->q_audio);
    }
    if(asr_ctx->q_text) {
        text_queue_clean(asr_ctx->q_text);
        switch_queue_term(asr_ctx->q_text);
    }
    if(asr_ctx->vad) {
        switch_vad_destroy(&asr_ctx->vad);
    }
    if(asr_ctx->vad_buffer) {
        switch_buffer_destroy(&asr_ctx->vad_buffer);
    }

    switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_feed(switch_asr_handle_t *ah, void *data, unsigned int data_len, switch_asr_flag_t *flags) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *) ah->private_info;
    switch_vad_state_t vad_state = 0;
    uint8_t fl_has_audio = SWITCH_FALSE;

    assert(asr_ctx != NULL);

    if(switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) {
        return SWITCH_STATUS_BREAK;
    }
    if(asr_ctx->fl_destroyed || asr_ctx->fl_abort) {
        return SWITCH_STATUS_BREAK;
    }
    if(asr_ctx->fl_pause) {
        return SWITCH_STATUS_SUCCESS;
    }
    if(!data || !data_len) {
        return SWITCH_STATUS_BREAK;
    }

    if(data_len > 0 && asr_ctx->frame_len == 0) {
        switch_mutex_lock(asr_ctx->mutex);
        asr_ctx->frame_len = data_len;
        asr_ctx->vad_buffer_size = asr_ctx->frame_len * VAD_STORE_FRAMES;
        asr_ctx->chunk_buffer_size = asr_ctx->samplerate * globals.speech_max_sec;
        switch_mutex_unlock(asr_ctx->mutex);

        if(switch_buffer_create(ah->memory_pool, &asr_ctx->vad_buffer, asr_ctx->vad_buffer_size) != SWITCH_STATUS_SUCCESS) {
            asr_ctx->vad_buffer_size = 0;
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_buffer_create()\n");
        }
    }

    if(asr_ctx->vad_buffer_size) {
        if(asr_ctx->vad_state == SWITCH_VAD_STATE_STOP_TALKING || (asr_ctx->vad_state == vad_state && vad_state == SWITCH_VAD_STATE_NONE)) {
            if(data_len <= asr_ctx->frame_len) {
                if(asr_ctx->vad_stored_frames >= VAD_STORE_FRAMES) {
                    switch_buffer_zero(asr_ctx->vad_buffer);
                    asr_ctx->vad_stored_frames = 0;
                    asr_ctx->fl_vad_first_cycle = SWITCH_FALSE;
                }
                switch_buffer_write(asr_ctx->vad_buffer, data, MIN(asr_ctx->frame_len, data_len));
                asr_ctx->vad_stored_frames++;
            }
        }

        vad_state = switch_vad_process(asr_ctx->vad, (int16_t *)data, (data_len / sizeof(int16_t)));
        if(vad_state == SWITCH_VAD_STATE_START_TALKING) {
            asr_ctx->vad_state = vad_state;
            fl_has_audio = SWITCH_TRUE;
        } else if (vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
            asr_ctx->vad_state = vad_state;
            fl_has_audio = SWITCH_FALSE;
            switch_vad_reset(asr_ctx->vad);
        } else if (vad_state == SWITCH_VAD_STATE_TALKING) {
            asr_ctx->vad_state = vad_state;
            fl_has_audio = SWITCH_TRUE;
        }
    } else {
        fl_has_audio = SWITCH_TRUE;
    }

    if(fl_has_audio) {
        if(vad_state == SWITCH_VAD_STATE_START_TALKING && asr_ctx->vad_stored_frames > 0) {
            xdata_buffer_t *tau_buf = NULL;
            const void *ptr = NULL;
            switch_size_t vblen = 0;
            uint32_t rframes = 0, rlen = 0;
            int ofs = 0;

            if((vblen = switch_buffer_peek_zerocopy(asr_ctx->vad_buffer, &ptr)) && ptr && vblen > 0) {
                rframes = (asr_ctx->vad_stored_frames >= VAD_RECOVERY_FRAMES ? VAD_RECOVERY_FRAMES : (asr_ctx->fl_vad_first_cycle ? asr_ctx->vad_stored_frames : VAD_RECOVERY_FRAMES));
                rlen = (rframes * asr_ctx->frame_len);
                ofs = (vblen - rlen);

                if(ofs < 0) {
                    uint32_t hdr_sz = -ofs;
                    uint32_t hdr_ofs = (asr_ctx->vad_buffer_size - hdr_sz);

                    switch_zmalloc(tau_buf, sizeof(xdata_buffer_t));

                    tau_buf->len = (hdr_sz + vblen + data_len);
                    switch_malloc(tau_buf->data, tau_buf->len);

                    memcpy(tau_buf->data, (void *)(ptr + hdr_ofs), hdr_sz);
                    memcpy(tau_buf->data + hdr_sz , (void *)(ptr + 0), vblen);
                    memcpy(tau_buf->data + rlen, data, data_len);

                    if(switch_queue_trypush(asr_ctx->q_audio, tau_buf) != SWITCH_STATUS_SUCCESS) {
                        xdata_buffer_free(&tau_buf);
                    }

                    switch_buffer_zero(asr_ctx->vad_buffer);
                    asr_ctx->vad_stored_frames = 0;
                } else {
                    switch_zmalloc(tau_buf, sizeof(xdata_buffer_t));

                    tau_buf->len = (rlen + data_len);
                    switch_malloc(tau_buf->data, tau_buf->len);

                    memcpy(tau_buf->data, (void *)(ptr + ofs), rlen);
                    memcpy(tau_buf->data + rlen, data, data_len);

                    if(switch_queue_trypush(asr_ctx->q_audio, tau_buf) != SWITCH_STATUS_SUCCESS) {
                        xdata_buffer_free(&tau_buf);
                    }

                    switch_buffer_zero(asr_ctx->vad_buffer);
                    asr_ctx->vad_stored_frames = 0;
                }
            }
        } else {
            xdata_buffer_push(asr_ctx->q_audio, data, data_len);
        }
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;

    assert(asr_ctx != NULL);

    if(asr_ctx->fl_pause) {
        return SWITCH_STATUS_FALSE;
    }

    return (asr_ctx->transcription_results > 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE);
}

static switch_status_t asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;
    switch_status_t status = SWITCH_STATUS_FALSE;
    void *pop = NULL;

    assert(asr_ctx != NULL);

    if(switch_queue_trypop(asr_ctx->q_text, &pop) == SWITCH_STATUS_SUCCESS) {
        if(pop) {
            *xmlstr = (char *)pop;
            status = SWITCH_STATUS_SUCCESS;

            switch_mutex_lock(asr_ctx->mutex);
            if(asr_ctx->transcription_results > 0) asr_ctx->transcription_results--;
            switch_mutex_unlock(asr_ctx->mutex);
        }
    }

    return status;
}

static switch_status_t asr_start_input_timers(switch_asr_handle_t *ah) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;

    assert(asr_ctx != NULL);

    asr_ctx->fl_start_timers = SWITCH_TRUE;

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_pause(switch_asr_handle_t *ah) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;

    assert(asr_ctx != NULL);

    asr_ctx->fl_pause = SWITCH_TRUE;

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_resume(switch_asr_handle_t *ah) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;

    assert(asr_ctx != NULL);

    asr_ctx->fl_pause = SWITCH_FALSE;

    return SWITCH_STATUS_SUCCESS;
}

static void asr_text_param(switch_asr_handle_t *ah, char *param, const char *val) {
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;

    assert(asr_ctx != NULL);

    if(strcasecmp(param, "lang") == 0) {
        if(val) asr_ctx->opt_lang = switch_core_strdup(ah->memory_pool, val);
    } else if(strcasecmp(param, "model") == 0) {
        if(val) asr_ctx->opt_model = switch_core_strdup(ah->memory_pool, val);
    } else if(strcasecmp(param, "key") == 0) {
        if(val) asr_ctx->opt_api_key = switch_core_strdup(ah->memory_pool, val);
    } else if(strcasecmp(param, "silence") == 0) {
        if(val) asr_ctx->silence_sec = atoi(val);
    }
}

static void asr_numeric_param(switch_asr_handle_t *ah, char *param, int val) {
}

static void asr_float_param(switch_asr_handle_t *ah, char *param, double val) {
}

static switch_status_t asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name) {
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t asr_unload_grammar(switch_asr_handle_t *ah, const char *name) {
    return SWITCH_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
#define CMD_SYNTAX "path_to/filename.(mp3|wav) [key=altkey model=altModel]\n"
SWITCH_STANDARD_API(openai_asr_cmd_handler) {
    switch_status_t status = 0;
    char *mycmd = NULL, *argv[10] = { 0 }; int argc = 0;
    switch_buffer_t *recv_buf = NULL;
    const void *response_ptr = NULL;
    char *opt_api_key = globals.api_key;
    char *opt_model = globals.opt_model;
    char *file_name = NULL, *file_ext = NULL;
    uint32_t recv_len = 0;

    if (!zstr(cmd)) {
        mycmd = strdup(cmd);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(argc == 0) {
        goto usage;
    }

    file_name = argv[0];
    if(switch_file_exists(file_name, NULL) != SWITCH_STATUS_SUCCESS) {
        stream->write_function(stream, "-ERR: file not found (%s)\n", file_name);
        goto out;
    }

    file_ext = strrchr(file_name, '.');
    if(!file_ext) {
        stream->write_function(stream, "-ERR: unsupported file encoding (null)\n");
        goto out;
    }

    file_ext++;
    if(strcasecmp("mp3", file_ext) && strcasecmp("wav", file_ext)) {
        stream->write_function(stream, "-ERR: unsupported file encoding (%s)\n", file_ext);
        goto out;
    }

    if(switch_buffer_create_dynamic(&recv_buf, 1024, 2048, 8192) != SWITCH_STATUS_SUCCESS) {
        stream->write_function(stream, "-ERR: switch_buffer_create_dynamic()\n");
        goto out;
    }

    if(argc > 1) {
        for(int i = 1; i < argc; i++) {
            char *kvp[2] = { 0 };
            if(switch_separate_string(argv[i], '=', kvp, 2) >= 2) {
                if(strcasecmp(kvp[0], "key") == 0) {
                    if(kvp[1]) opt_api_key = kvp[1];
                } else if(strcasecmp(kvp[0], "model") == 0) {
                    if(kvp[1]) opt_model = kvp[1];
                }
            }
        }
    }

    status = curl_perform(recv_buf, opt_api_key, opt_model, file_name, &globals);

    recv_len = switch_buffer_peek_zerocopy(recv_buf, &response_ptr);
    if(status == SWITCH_STATUS_SUCCESS && response_ptr && recv_len) {
        char *txt = parse_response((char *)response_ptr, stream);
        if(txt) {
            stream->write_function(stream, "+OK: %s\n", txt);
        }
        switch_safe_free(txt);
    } else {
        stream->write_function(stream, "-ERR: unable to perform request\n");
    }

    goto out;
usage:
    stream->write_function(stream, "-ERR:\nUsage: %s\n", CMD_SYNTAX);

out:
    if(recv_buf) {
        switch_buffer_destroy(&recv_buf);
    }

    switch_safe_free(mycmd);
    return SWITCH_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
SWITCH_MODULE_LOAD_FUNCTION(mod_openai_asr_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg, xml, settings, param;
    switch_api_interface_t *commands_interface;
    switch_asr_interface_t *asr_interface;

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

            if(!strcasecmp(var, "vad-silence-ms")) {
                if(val) globals.vad_silence_ms = atoi (val);
            } else if(!strcasecmp(var, "vad-voice-ms")) {
                if(val) globals.vad_voice_ms = atoi (val);
            } else if(!strcasecmp(var, "vad-threshold")) {
                if(val) globals.vad_threshold = atoi (val);
            } else if(!strcasecmp(var, "vad-debug")) {
                if(val) globals.fl_vad_debug = switch_true(val);
            } else if(!strcasecmp(var, "api-key")) {
                if(val) globals.api_key = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "api-url")) {
                if(val) globals.api_url = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "user-agent")) {
                if(val) globals.user_agent = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "proxy")) {
                if(val) globals.proxy = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "proxy-credentials")) {
                if(val) globals.proxy_credentials = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "encoding")) {
                if(val) globals.opt_encoding = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "model")) {
                if(val) globals.opt_model= switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "speech-max-sec")) {
                if(val) globals.speech_max_sec = atoi(val);
            } else if(!strcasecmp(var, "speech-silence-sec")) {
                if(val) globals.speech_silence_sec = atoi(val);
            } else if(!strcasecmp(var, "request-timeout")) {
                if(val) globals.request_timeout = atoi(val);
            } else if(!strcasecmp(var, "connect-timeout")) {
                if(val) globals.connect_timeout = atoi(val);
            } else if(!strcasecmp(var, "log-http-errors")) {
                if(val) globals.fl_log_http_errors = switch_true(val);
            } else if(!strcasecmp(var, "retries-on-error")) {
                if(val) globals.retries_on_error = atoi(val);
            }
        }
    }

    if(!globals.api_url) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required parameter: api-url\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    globals.opt_encoding = globals.opt_encoding ?  globals.opt_encoding : "wav";
    globals.speech_max_sec = !globals.speech_max_sec ? 35 : globals.speech_max_sec;
    globals.speech_silence_sec = !globals.speech_silence_sec ? 3 : globals.speech_silence_sec;
    globals.retries_on_error = !globals.retries_on_error ? 1 : globals.retries_on_error;

    globals.tmp_path = switch_core_sprintf(pool, "%s%sopenai-asr-cache", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR);
    if(switch_directory_exists(globals.tmp_path, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_dir_make(globals.tmp_path, SWITCH_FPROT_OS_DEFAULT, NULL);
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(commands_interface, "openai_asr_transcript", "OpenAI speech-to-text", openai_asr_cmd_handler, CMD_SYNTAX);

    asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
    asr_interface->interface_name = "openai";
    asr_interface->asr_open = asr_open;
    asr_interface->asr_close = asr_close;
    asr_interface->asr_feed = asr_feed;
    asr_interface->asr_pause = asr_pause;
    asr_interface->asr_resume = asr_resume;
    asr_interface->asr_check_results = asr_check_results;
    asr_interface->asr_get_results = asr_get_results;
    asr_interface->asr_start_input_timers = asr_start_input_timers;
    asr_interface->asr_text_param = asr_text_param;
    asr_interface->asr_numeric_param = asr_numeric_param;
    asr_interface->asr_float_param = asr_float_param;
    asr_interface->asr_load_grammar = asr_load_grammar;
    asr_interface->asr_unload_grammar = asr_unload_grammar;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "OpenAI-ASR (%s)\n", MOD_VERSION);
out:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_openai_asr_shutdown) {
    uint8_t fl_wloop = SWITCH_TRUE;

    globals.fl_shutdown = SWITCH_TRUE;

    switch_mutex_lock(globals.mutex);
    fl_wloop = (globals.active_threads > 0);
    switch_mutex_unlock(globals.mutex);

    if(fl_wloop) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for termination (%d) threads...\n", globals.active_threads);
        while(fl_wloop) {
            switch_mutex_lock(globals.mutex);
            fl_wloop = (globals.active_threads > 0);
            switch_mutex_unlock(globals.mutex);
            switch_yield(100000);
        }
    }

    return SWITCH_STATUS_SUCCESS;
}
