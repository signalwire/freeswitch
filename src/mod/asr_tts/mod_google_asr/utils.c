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
#include "mod_google_asr.h"

extern globals_t globals;

/**
 ** https://cloud.google.com/speech-to-text/docs/reference/rest/v1/RecognitionConfig
 ** https://cloud.google.com/speech-to-text/docs/speech-to-text-supported-languages
 **
 **/
char *gcp_get_language(const char *val) {
    if(strcasecmp(val, "en") == 0) { return "en-US"; }
    if(strcasecmp(val, "de") == 0) { return "de-DE"; }
    if(strcasecmp(val, "es") == 0) { return "es-US"; }
    if(strcasecmp(val, "it") == 0) { return "it-IT"; }
    if(strcasecmp(val, "ru") == 0) { return "ru-RU"; }
    return (char *)val;
}

char *gcp_get_encoding(const char *val) {
    if(strcasecmp(val, "unspecified") == 0)  { return "ENCODING_UNSPECIFIED"; }
    if(strcasecmp(val, "l16") == 0)  { return "LINEAR16"; }
    if(strcasecmp(val, "flac") == 0) { return "FLAC"; }
    if(strcasecmp(val, "ulaw") == 0) { return "MULAW"; }
    if(strcasecmp(val, "amr") == 0)  { return "AMR"; }
    return (char *)val;
}

char *gcp_get_microphone_distance(const char *val) {
    if(strcasecmp(val, "unspecified") == 0)  { return "MICROPHONE_DISTANCE_UNSPECIFIED"; }
    if(strcasecmp(val, "nearfield") == 0)  { return "NEARFIELD"; }
    if(strcasecmp(val, "midfield") == 0)  { return "MIDFIELD"; }
    if(strcasecmp(val, "farfield") == 0)  { return "FARFIELD"; }
    return (char *)val;
}

char *gcp_get_recording_device(const char *val) {
    if(strcasecmp(val, "unspecified") == 0)  { return "RECORDING_DEVICE_TYPE_UNSPECIFIED"; }
    if(strcasecmp(val, "smartphone") == 0)  { return "SMARTPHONE"; }
    if(strcasecmp(val, "pc") == 0)          { return "PC"; }
    if(strcasecmp(val, "phone_line") == 0)  { return "PHONE_LINE"; }
    if(strcasecmp(val, "vehicle") == 0)     { return "VEHICLE"; }
    if(strcasecmp(val, "other_outdoor_device") == 0) { return "OTHER_OUTDOOR_DEVICE"; }
    if(strcasecmp(val, "other_indoor_device") == 0)  { return "OTHER_INDOOR_DEVICE"; }
    return (char *)val;
}

char *gcp_get_interaction(const char *val) {
    if(strcasecmp(val, "unspecified") == 0)  { return "INTERACTION_TYPE_UNSPECIFIED"; }
    if(strcasecmp(val, "discussion") == 0)  { return "DISCUSSION"; }
    if(strcasecmp(val, "presentation") == 0)  { return "PRESENTATION"; }
    if(strcasecmp(val, "phone_call") == 0)  { return "PHONE_CALL"; }
    if(strcasecmp(val, "voicemal") == 0)  { return "VOICEMAIL"; }
    if(strcasecmp(val, "professionally_produced") == 0)  { return "PROFESSIONALLY_PRODUCED"; }
    if(strcasecmp(val, "voice_search") == 0)  { return "VOICE_SEARCH"; }
    if(strcasecmp(val, "voice_command") == 0)  { return "VOICE_COMMAND"; }
    if(strcasecmp(val, "dictation") == 0)  { return "DICTATION"; }
    return (char *)val;
}

switch_status_t xdata_buffer_alloc(xdata_buffer_t **out, switch_byte_t *data, uint32_t data_len) {
    xdata_buffer_t *buf = NULL;

    switch_zmalloc(buf, sizeof(xdata_buffer_t));

    if(data_len) {
        switch_malloc(buf->data, data_len);
        switch_assert(buf->data);

        buf->len = data_len;
        memcpy(buf->data, data, data_len);
    }

    *out = buf;
    return SWITCH_STATUS_SUCCESS;
}

void xdata_buffer_free(xdata_buffer_t **buf) {
    if(buf && *buf) {
        switch_safe_free((*buf)->data);
        free(*buf);
    }
}

void xdata_buffer_queue_clean(switch_queue_t *queue) {
    xdata_buffer_t *data = NULL;

    if(!queue || !switch_queue_size(queue)) { return; }

    while(switch_queue_trypop(queue, (void *) &data) == SWITCH_STATUS_SUCCESS) {
        if(data) { xdata_buffer_free(&data); }
    }
}

switch_status_t xdata_buffer_push(switch_queue_t *queue, switch_byte_t *data, uint32_t data_len) {
    xdata_buffer_t *buff = NULL;

    if(xdata_buffer_alloc(&buff, data, data_len) == SWITCH_STATUS_SUCCESS) {
        if(switch_queue_trypush(queue, buff) == SWITCH_STATUS_SUCCESS) {
            return SWITCH_STATUS_SUCCESS;
        }
        xdata_buffer_free(&buff);
    }
    return SWITCH_STATUS_FALSE;
}

void text_queue_clean(switch_queue_t *queue) {
    void *data = NULL;

    if(!queue || !switch_queue_size(queue)) {
        return;
    }

    while(switch_queue_trypop(queue, (void *)&data) == SWITCH_STATUS_SUCCESS) {
        switch_safe_free(data);
    }
}

char *parse_response(char *data, switch_stream_handle_t *stream) {
    char *result = NULL;
    cJSON *json = NULL;

    if(!data) {
        return NULL;
    }

    if((json = cJSON_Parse(data)) != NULL) {
        cJSON *jres = cJSON_GetObjectItem(json, "results");
        if(jres && cJSON_GetArraySize(jres) > 0) {
            cJSON *jelem = cJSON_GetArrayItem(jres, 0);
            if(jelem) {
                jres = cJSON_GetObjectItem(jelem, "alternatives");
                if(jres && cJSON_GetArraySize(jres) > 0) {
                    jelem = cJSON_GetArrayItem(jres, 0);
                    if(jelem) {
                        cJSON *jt = cJSON_GetObjectItem(jelem, "transcript");
                        if(jt && jt->valuestring) {
                            result = strdup(jt->valuestring);
                        }
                    }
                }
            }
        }
    }

    if(json) {
        cJSON_Delete(json);
    }

    return result;
}
