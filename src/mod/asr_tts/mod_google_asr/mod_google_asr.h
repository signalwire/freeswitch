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
 *
 */
#ifndef MOD_GOOGLE_ASR_H
#define MOD_GOOGLE_ASR_H

#include <switch.h>
#include <switch_curl.h>
#include <switch_json.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define MOD_CONFIG_NAME         "google_asr.conf"
#define MOD_VERSION             "1.0.4"
#define QUEUE_SIZE              128
#define VAD_STORE_FRAMES        64
#define VAD_RECOVERY_FRAMES     20
#define BASE64_ENC_SZ(n)        (4*((n+2)/3))
#define BOOL2STR(v)             (v ? "true" : "false")

//#define MOD_GOOGLE_ASR_DEBUG

typedef struct {
    switch_mutex_t          *mutex;
    uint32_t                active_threads;
    uint32_t                speech_max_sec;
    uint32_t                speech_silence_sec;
    uint32_t                vad_silence_ms;
    uint32_t                vad_voice_ms;
    uint32_t                vad_threshold;
    uint32_t                request_timeout; // seconds
    uint32_t                connect_timeout; // seconds
    uint32_t                retries_on_error;
    uint8_t                 fl_vad_debug;
    uint8_t                 fl_shutdown;
    char                    *tmp_path;
    char                    *api_key;
    char                    *api_url;
    char                    *user_agent;
    char                    *default_lang;
    char                    *proxy;
    char                    *proxy_credentials;
    char                    *opt_encoding;
    char                    *opt_speech_model;
    char                    *opt_meta_microphone_distance;
    char                    *opt_meta_recording_device_type;
    char                    *opt_meta_interaction_type;
    uint32_t                opt_max_alternatives;
    uint32_t                opt_use_enhanced_model;
    uint32_t                opt_enable_word_time_offsets;
    uint32_t                opt_enable_word_confidence;
    uint32_t                opt_enable_profanity_filter;
    uint32_t                opt_enable_automatic_punctuation;
    uint32_t                opt_enable_spoken_punctuation;
    uint32_t                opt_enable_spoken_emojis;
} globals_t;


typedef struct {
    switch_memory_pool_t    *pool;
    switch_vad_t            *vad;
    switch_buffer_t         *vad_buffer;
    switch_mutex_t          *mutex;
    switch_queue_t          *q_audio;
    switch_queue_t          *q_text;
    switch_buffer_t         *curl_recv_buffer_ref;
    switch_byte_t           *curl_send_buffer_ref;
    char                    *api_key;
    char                    *lang;
    switch_vad_state_t      vad_state;
    uint32_t                retries_on_error;
    uint32_t                curl_send_buffer_len;
    uint32_t                transcription_results;
    uint32_t                vad_buffer_size;
    uint32_t                vad_stored_frames;
    uint32_t                chunk_buffer_size;
    uint32_t                refs;
    uint32_t                samplerate;
    uint32_t                channels;
    uint32_t                frame_len;
    uint32_t                silence_sec;
    uint8_t                 fl_start_timers;
    uint8_t                 fl_pause;
    uint8_t                 fl_vad_first_cycle;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_abort;
    //
    char                    *opt_speech_model;
    char                    *opt_meta_microphone_distance;
    char                    *opt_meta_recording_device_type;
    char                    *opt_meta_interaction_type;
    uint32_t                opt_max_alternatives;
    uint32_t                opt_use_enhanced_model;
    uint32_t                opt_enable_word_time_offsets;
    uint32_t                opt_enable_word_confidence;
    uint32_t                opt_enable_profanity_filter;
    uint32_t                opt_enable_automatic_punctuation;
    uint32_t                opt_enable_spoken_punctuation;
    uint32_t                opt_enable_spoken_emojis;
    uint32_t                opt_enable_speaker_diarization;
    uint32_t                opt_diarization_min_speaker_count;
    uint32_t                opt_diarization_max_speaker_count;
} asr_ctx_t;

typedef struct {
    uint32_t                len;
    switch_byte_t           *data;
} xdata_buffer_t;


/* curl.c */
switch_status_t curl_perform(asr_ctx_t *asr_ctx, globals_t *globals);

/* utils.c */
switch_status_t xdata_buffer_push(switch_queue_t *queue, switch_byte_t *data, uint32_t data_len);
switch_status_t xdata_buffer_alloc(xdata_buffer_t **out, switch_byte_t *data, uint32_t data_len);
void xdata_buffer_free(xdata_buffer_t **buf);
void xdata_buffer_queue_clean(switch_queue_t *queue);
void text_queue_clean(switch_queue_t *queue);
char *parse_response(char *data, switch_stream_handle_t *stream);

char *gcp_get_language(const char *val);
char *gcp_get_encoding(const char *val);
char *gcp_get_microphone_distance(const char *val);
char *gcp_get_recording_device(const char *val);
char *gcp_get_interaction(const char *val);


#endif
