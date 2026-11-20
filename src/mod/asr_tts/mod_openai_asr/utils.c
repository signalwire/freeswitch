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
#include "mod_openai_asr.h"

extern globals_t globals;

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

    if(!queue || !switch_queue_size(queue)) {
        return;
    }

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

char *chunk_write(switch_byte_t *buf, uint32_t buf_len, uint32_t channels, uint32_t samplerate, const char *file_ext) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_size_t len = (buf_len / sizeof(int16_t));
    switch_file_handle_t fh = { 0 };
    char *file_name = NULL;
    char name_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
    int flags = (SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT);

    switch_uuid_str((char *)name_uuid, sizeof(name_uuid));
    file_name = switch_mprintf("%s%s%s.%s", globals.tmp_path, SWITCH_PATH_SEPARATOR, name_uuid, (file_ext == NULL ? "wav" : file_ext) );

    if((status = switch_core_file_open(&fh, file_name, channels, samplerate, flags, NULL)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file (%s)\n", file_name);
        goto out;
    }

    if((status = switch_core_file_write(&fh, buf, &len)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to write (%s)\n", file_name);
        goto out;
    }

    switch_core_file_close(&fh);
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(file_name) {
            unlink(file_name);
            switch_safe_free(file_name);
        }
        return NULL;
    }

    return file_name;
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

    if(!(json = cJSON_Parse(data))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to parse json (%s)\n", data);
        if(stream) stream->write_function(stream, "-ERR: Unable to parse json (see log)\n");
    } else {
        cJSON *jres = cJSON_GetObjectItem(json, "error");
        if(jres) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Service returns error (%s)\n", data);
            if(stream) stream->write_function(stream, "-ERR: Service returns error (see log)\n");
        } else {
            cJSON *jres = cJSON_GetObjectItem(json, "text");
            if(jres) {
                result = strdup(jres->valuestring);
            }
        }
    }

    if(json) {
        cJSON_Delete(json);
    }

    return result;
}
