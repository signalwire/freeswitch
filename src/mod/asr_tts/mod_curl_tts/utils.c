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
#include "mod_curl_tts.h"

char *escape_dquotes(const char *string) {
    size_t string_len = strlen(string);
    size_t i;
    size_t n = 0;
    size_t dest_len = 0;
    char *dest;

    dest_len = strlen(string) + 1;
    for (i = 0; i < string_len; i++) {
        switch (string[i]) {
            case '\"': dest_len += 1; break;
        }
    }

    dest = (char *) malloc(sizeof(char) * dest_len);
    switch_assert(dest);

    for (i = 0; i < string_len; i++) {
        switch (string[i]) {
            case '\"':
                dest[n++] = '\\';
                dest[n++] = '\"';
            break;
            default:
                dest[n++] = string[i];
        }
    }
    dest[n++] = '\0';

    switch_assert(n == dest_len);
    return dest;
}

switch_status_t write_file(char *file_name, switch_byte_t *buf, uint32_t buf_len) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_memory_pool_t *pool = NULL;
    switch_size_t len = buf_len;
    switch_file_t *fd = NULL;

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_new_memory_pool() fail\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }
    if((status = switch_file_open(&fd, file_name, (SWITCH_FOPEN_WRITE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_CREATE), SWITCH_FPROT_OS_DEFAULT, pool)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open fail: %s\n", file_name);
        goto out;
    }
    if((status = switch_file_write(fd, buf, &len)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write fail (%s)\n", file_name);
    }
    switch_file_close(fd);
out:
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    return status;
}
