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
 * Google Speech-To-Text service for the Freeswitch.
 * https://cloud.google.com/speech-to-text/docs/reference/rest
 *
 * Development repository:
 * https://github.com/akscf/mod_google_asr
 *
 */
#include "mod_google_asr.h"

globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_google_asr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_asr_shutdown);
SWITCH_MODULE_DEFINITION(mod_google_asr, mod_google_asr_load, mod_google_asr_shutdown, NULL);


static void *SWITCH_THREAD_FUNC transcribe_thread(switch_thread_t *thread, void *obj) {
    volatile asr_ctx_t *_ref = (asr_ctx_t *)obj;
    asr_ctx_t *asr_ctx = (asr_ctx_t *)_ref;
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_byte_t *base64_buffer = NULL;
    switch_byte_t *curl_send_buffer = NULL;
    switch_buffer_t *chunk_buffer = NULL;
    switch_buffer_t *curl_recv_buffer = NULL;
    switch_memory_pool_t *pool = NULL;
    time_t speech_timeout = 0;
    uint32_t base64_buffer_size = 0, chunk_buffer_size = 0, recv_len = 0;
    uint32_t schunks = 0;
    uint8_t fl_cbuff_overflow = SWITCH_FALSE;
    const void *curl_recv_buffer_ptr = NULL;
    void *pop = NULL;

    switch_mutex_lock(asr_ctx->mutex);
    asr_ctx->refs++;
    switch_mutex_unlock(asr_ctx->mutex);

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto out;
    }
    if(switch_buffer_create_dynamic(&curl_recv_buffer, 1024, 4096, 32648) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_buffer_create_dynamic()\n");
        goto out;
    }

    while(SWITCH_TRUE) {
        if(globals.fl_shutdown || asr_ctx->fl_destroyed ) {
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
            speech_timeout = 1;
        } else {
            if(schunks && asr_ctx->vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
                if(!speech_timeout) {
                    speech_timeout = asr_ctx->silence_sec + switch_epoch_time_now(NULL);
                }
            }
            if(speech_timeout && (asr_ctx->vad_state == SWITCH_VAD_STATE_START_TALKING || asr_ctx->vad_state == SWITCH_VAD_STATE_TALKING)) {
                speech_timeout = 0;
            }
        }

        if(speech_timeout && speech_timeout <= switch_epoch_time_now(NULL)) {
            const void *chunk_buffer_ptr = NULL;
            uint32_t buf_len = switch_buffer_peek_zerocopy(chunk_buffer, &chunk_buffer_ptr);
            uint32_t b64_len = BASE64_ENC_SZ(buf_len) + 1;
            uint32_t stt_failed = 0;

            if(base64_buffer_size == 0 || base64_buffer_size < b64_len) {
                if(base64_buffer_size > 0) { switch_safe_free(base64_buffer); }
                switch_zmalloc(base64_buffer, b64_len);
                base64_buffer_size = b64_len;
            } else {
                memset(base64_buffer, 0x0, b64_len);
            }

            if(switch_b64_encode((uint8_t *)chunk_buffer_ptr, buf_len, base64_buffer, base64_buffer_size) == SWITCH_STATUS_SUCCESS) {
                curl_send_buffer = (switch_byte_t *)switch_mprintf( "{'config':{" \
                                            "'languageCode':'%s', 'encoding':'%s', 'sampleRateHertz':'%u', 'audioChannelCount':'%u', 'maxAlternatives':'%u', " \
                                            "'profanityFilter':'%s', 'enableWordTimeOffsets':'%s', 'enableWordConfidence':'%s', 'enableAutomaticPunctuation':'%s', " \
                                            "'enableSpokenPunctuation':'%s', 'enableSpokenEmojis':'%s', 'model':'%s', 'useEnhanced':'%s', " \
                                            " 'diarizationConfig':{'enableSpeakerDiarization': '%s', 'minSpeakerCount': '%u', 'maxSpeakerCount': '%u'}, " \
                                            "'metadata':{'interactionType':'%s', 'microphoneDistance':'%s', 'recordingDeviceType':'%s'}}, 'audio':{'content':'%s'}}",
                                            asr_ctx->lang,
                                            globals.opt_encoding,
                                            asr_ctx->samplerate,
                                            asr_ctx->channels,
                                            asr_ctx->opt_max_alternatives,
                                            BOOL2STR(asr_ctx->opt_enable_profanity_filter),
                                            BOOL2STR(asr_ctx->opt_enable_word_time_offsets),
                                            BOOL2STR(asr_ctx->opt_enable_word_confidence),
                                            BOOL2STR(asr_ctx->opt_enable_automatic_punctuation),
                                            BOOL2STR(asr_ctx->opt_enable_spoken_punctuation),
                                            BOOL2STR(asr_ctx->opt_enable_spoken_emojis),
                                            asr_ctx->opt_speech_model,
                                            BOOL2STR(asr_ctx->opt_use_enhanced_model),
                                            BOOL2STR(asr_ctx->opt_enable_speaker_diarization),
                                            asr_ctx->opt_diarization_min_speaker_count,
                                            asr_ctx->opt_diarization_max_speaker_count,
                                            asr_ctx->opt_meta_interaction_type,
                                            asr_ctx->opt_meta_microphone_distance,
                                            asr_ctx->opt_meta_recording_device_type,
                                            base64_buffer
                                        );

                asr_ctx->curl_send_buffer_ref = curl_send_buffer;
                asr_ctx->curl_send_buffer_len = strlen((const char *)curl_send_buffer);
                asr_ctx->curl_recv_buffer_ref = curl_recv_buffer;

                for(int rqtry = 0; rqtry < asr_ctx->retries_on_error; rqtry++) {
                    switch_buffer_zero(curl_recv_buffer);
                    status = curl_perform(asr_ctx, &globals);
                    if(status == SWITCH_STATUS_SUCCESS || globals.fl_shutdown || asr_ctx->fl_destroyed) { break; }
                    switch_yield(1000);
                }

                recv_len = switch_buffer_peek_zerocopy(curl_recv_buffer, &curl_recv_buffer_ptr);
                if(status == SWITCH_STATUS_SUCCESS) {
                    if(curl_recv_buffer_ptr && recv_len) {
                        char *txt = parse_response((char *)curl_recv_buffer_ptr, NULL);
#ifdef MOD_GOOGLE_ASR_DEBUG
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Service response [%s]\n", (char *)curl_recv_buffer_ptr);
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
                switch_safe_free(curl_send_buffer);
            } else {
                stt_failed = 1;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_b64_encode() failed\n");
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
            speech_timeout = 0;
            switch_buffer_zero(chunk_buffer);
        }

        timer_next:
        switch_yield(10000);
    }

out:
    switch_safe_free(base64_buffer);
    switch_safe_free(curl_send_buffer);

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
// asr interface
// ---------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t asr_open(switch_asr_handle_t *ah, const char *codec, int samplerate, const char *dest, switch_asr_flag_t *flags) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;
    asr_ctx_t *asr_ctx = NULL;

    if(strcmp(codec, "L16") !=0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported encoding: %s\n", codec);
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
    asr_ctx->lang = (char *)globals.default_lang;
    asr_ctx->api_key = globals.api_key;
    asr_ctx->retries_on_error = globals.retries_on_error;

    asr_ctx->opt_max_alternatives = globals.opt_max_alternatives;
    asr_ctx->opt_enable_profanity_filter = globals.opt_enable_profanity_filter;
    asr_ctx->opt_enable_word_time_offsets = globals.opt_enable_word_time_offsets;
    asr_ctx->opt_enable_word_confidence = globals.opt_enable_word_confidence;
    asr_ctx->opt_enable_automatic_punctuation = globals.opt_enable_automatic_punctuation;
    asr_ctx->opt_enable_spoken_punctuation = globals.opt_enable_spoken_punctuation;
    asr_ctx->opt_enable_spoken_emojis = globals.opt_enable_spoken_emojis;
    asr_ctx->opt_meta_interaction_type = globals.opt_meta_interaction_type;
    asr_ctx->opt_meta_microphone_distance = globals.opt_meta_microphone_distance;
    asr_ctx->opt_meta_recording_device_type = globals.opt_meta_recording_device_type;
    asr_ctx->opt_speech_model = globals.opt_speech_model;
    asr_ctx->opt_use_enhanced_model = globals.opt_use_enhanced_model;
    asr_ctx->opt_enable_speaker_diarization = SWITCH_FALSE;
    asr_ctx->opt_diarization_min_speaker_count = 1;
    asr_ctx->opt_diarization_max_speaker_count = 1;

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
    if(globals.vad_silence_ms > 0) { switch_vad_set_param(asr_ctx->vad, "silence_ms", globals.vad_silence_ms); }
    if(globals.vad_voice_ms > 0)   { switch_vad_set_param(asr_ctx->vad, "voice_ms", globals.vad_voice_ms); }
    if(globals.vad_threshold > 0)  { switch_vad_set_param(asr_ctx->vad, "thresh", globals.vad_threshold); }

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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for unlock (refs=%u)...\n", asr_ctx->refs);
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
    asr_ctx_t *asr_ctx = (asr_ctx_t *)ah->private_info;
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
        if(val) asr_ctx->lang = switch_core_strdup(ah->memory_pool, gcp_get_language(val));
    } else if(strcasecmp(param, "silence") == 0) {
        if(val) asr_ctx->silence_sec = atoi(val);
    } else if(strcasecmp(param, "key") == 0) {
        if(val) asr_ctx->api_key = switch_core_strdup(ah->memory_pool, val);
    } else if(!strcasecmp(param, "speech-model")) {
        if(val) asr_ctx->opt_speech_model = switch_core_strdup(ah->memory_pool, val);
    } else if(!strcasecmp(param, "use-enhanced-model")) {
        if(val) asr_ctx->opt_use_enhanced_model = switch_true(val);
    } else if(!strcasecmp(param, "max-alternatives")) {
        if(val) asr_ctx->opt_max_alternatives = atoi(val);
    } else if(!strcasecmp(param, "enable-word-time-offsets")) {
        if(val) asr_ctx->opt_enable_word_time_offsets = switch_true(val);
    } else if(!strcasecmp(param, "enable-enable-word-confidence;")) {
        if(val) asr_ctx->opt_enable_word_confidence = switch_true(val);
    } else if(!strcasecmp(param, "enable-profanity-filter")) {
        if(val) asr_ctx->opt_enable_profanity_filter = switch_true(val);
    } else if(!strcasecmp(param, "enable-automatic-punctuation")) {
        if(val) asr_ctx->opt_enable_automatic_punctuation = switch_true(val);
    } else if(!strcasecmp(param, "enable-spoken-punctuation")) {
        if(val) asr_ctx->opt_enable_spoken_punctuation = switch_true(val);
    } else if(!strcasecmp(param, "enable-spoken-emojis")) {
        if(val) asr_ctx->opt_enable_spoken_emojis = switch_true(val);
    } else if(!strcasecmp(param, "microphone-distance")) {
        if(val) asr_ctx->opt_meta_microphone_distance = switch_core_strdup(ah->memory_pool, gcp_get_microphone_distance(val));
    } else if(!strcasecmp(param, "recording-device-type")) {
        if(val) asr_ctx->opt_meta_recording_device_type = switch_core_strdup(ah->memory_pool, gcp_get_recording_device(val));
    } else if(!strcasecmp(param, "interaction-type")) {
        if(val) asr_ctx->opt_meta_interaction_type = switch_core_strdup(ah->memory_pool, gcp_get_interaction(val));
    } else if(!strcasecmp(param, "enable-speaker-diarizatio")) {
        if(val) asr_ctx->opt_enable_speaker_diarization = switch_true(val);
    } else if(!strcasecmp(param, "diarization-min-speakers")) {
        if(val) asr_ctx->opt_diarization_min_speaker_count = atoi(val);
    } else if(!strcasecmp(param, "diarization-max-speakers")) {
        if(val) asr_ctx->opt_diarization_max_speaker_count = atoi(val);
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

#define CMD_SYNTAX "path_to/filename.(mp3|wav) []\n"
SWITCH_STANDARD_API(google_asr_cmd_handler) {
    //switch_status_t status = 0;
    char *mycmd = NULL, *argv[10] = { 0 }; int argc = 0;

    if (!zstr(cmd)) {
        mycmd = strdup(cmd);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(argc == 0) {
        goto usage;
    }

    //
    // todo
    //

    stream->write_function(stream, "-ERR: not yet implemented\n");
    goto out;
usage:
    stream->write_function(stream, "-ERR:\nUsage: %s\n", CMD_SYNTAX);

out:

    switch_safe_free(mycmd);
    return SWITCH_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
SWITCH_MODULE_LOAD_FUNCTION(mod_google_asr_load) {
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
            } else if(!strcasecmp(var, "default-language")) {
                if(val) globals.default_lang = switch_core_strdup(pool, gcp_get_language(val));
            } else if(!strcasecmp(var, "encoding")) {
                if(val) globals.opt_encoding = switch_core_strdup(pool, gcp_get_encoding(val));
            } else if(!strcasecmp(var, "speech-max-sec")) {
                if(val) globals.speech_max_sec = atoi(val);
            } else if(!strcasecmp(var, "speech-silence-sec")) {
                if(val) globals.speech_silence_sec = atoi(val);
            } else if(!strcasecmp(var, "request-timeout")) {
                if(val) globals.request_timeout = atoi(val);
            } else if(!strcasecmp(var, "connect-timeout")) {
                if(val) globals.connect_timeout = atoi(val);
            } else if(!strcasecmp(var, "retries-on-error")) {
                if(val) globals.retries_on_error = atoi(val);
            } else if(!strcasecmp(var, "speech-model")) {
                if(val) globals.opt_speech_model = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "use-enhanced-model")) {
                if(val) globals.opt_use_enhanced_model = switch_true(val);
            } else if(!strcasecmp(var, "max-alternatives")) {
                if(val) globals.opt_max_alternatives = atoi(val);
            } else if(!strcasecmp(var, "enable-word-time-offsets")) {
                if(val) globals.opt_enable_word_time_offsets = switch_true(val);
            } else if(!strcasecmp(var, "enable-word-confidence")) {
                if(val) globals.opt_enable_word_confidence = switch_true(val);
            } else if(!strcasecmp(var, "enable-profanity-filter")) {
                if(val) globals.opt_enable_profanity_filter = switch_true(val);
            } else if(!strcasecmp(var, "enable-automatic-punctuation")) {
                if(val) globals.opt_enable_automatic_punctuation = switch_true(val);
            } else if(!strcasecmp(var, "enable-spoken-punctuation")) {
                if(val) globals.opt_enable_spoken_punctuation = switch_true(val);
            } else if(!strcasecmp(var, "enable-spoken-emojis")) {
                if(val) globals.opt_enable_spoken_emojis = switch_true(val);
            } else if(!strcasecmp(var, "microphone-distance")) {
                if(val) globals.opt_meta_microphone_distance = switch_core_strdup(pool, gcp_get_microphone_distance(val));
            } else if(!strcasecmp(var, "recording-device-type")) {
                if(val) globals.opt_meta_recording_device_type = switch_core_strdup(pool, gcp_get_recording_device(val));
            } else if(!strcasecmp(var, "interaction-type")) {
                if(val) globals.opt_meta_interaction_type = switch_core_strdup(pool, gcp_get_interaction(val));
            }
        }
    }

    if(!globals.api_url) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing required parameter: api-url\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    globals.speech_max_sec = !globals.speech_max_sec ? 35 : globals.speech_max_sec;
    globals.speech_silence_sec = !globals.speech_silence_sec ? 3 : globals.speech_silence_sec;
    globals.opt_encoding = globals.opt_encoding ? globals.opt_encoding : gcp_get_encoding("l16");
    globals.opt_speech_model = globals.opt_speech_model ?  globals.opt_speech_model : "phone_call";
    globals.opt_max_alternatives = globals.opt_max_alternatives > 0 ? globals.opt_max_alternatives : 1;
    globals.opt_meta_microphone_distance = globals.opt_meta_microphone_distance ? globals.opt_meta_microphone_distance : gcp_get_microphone_distance("unspecified");
    globals.opt_meta_recording_device_type = globals.opt_meta_recording_device_type ? globals.opt_meta_recording_device_type : gcp_get_recording_device("unspecified");
    globals.opt_meta_interaction_type = globals.opt_meta_interaction_type ? globals.opt_meta_interaction_type : gcp_get_interaction("unspecified");
    globals.retries_on_error = !globals.retries_on_error ? 1 : globals.retries_on_error;

    globals.tmp_path = switch_core_sprintf(pool, "%s%sgoogle-asr-cache", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR);
    if(switch_directory_exists(globals.tmp_path, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_dir_make(globals.tmp_path, SWITCH_FPROT_OS_DEFAULT, NULL);
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(commands_interface, "google_asr_transcript", "Google speech-to-text", google_asr_cmd_handler, CMD_SYNTAX);

    asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
    asr_interface->interface_name = "google";
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

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Google-ASR (%s)\n", MOD_VERSION);
out:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_google_asr_shutdown) {
    uint8_t fl_wloop = SWITCH_TRUE;

    globals.fl_shutdown = SWITCH_TRUE;

    switch_mutex_lock(globals.mutex);
    fl_wloop = (globals.active_threads > 0);
    switch_mutex_unlock(globals.mutex);

    if(fl_wloop) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Waiting for termination (%d) threads...\n", globals.active_threads);
        while(fl_wloop) {
            switch_mutex_lock(globals.mutex);
            fl_wloop = (globals.active_threads > 0);
            switch_mutex_unlock(globals.mutex);
            switch_yield(100000);
        }
    }

    return SWITCH_STATUS_SUCCESS;
}
