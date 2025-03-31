/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2025-2025, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_uuidv7.c -- UUIDv7 generation functions
 *
 */

#include <switch.h>
#include "switch_uuidv7.h"

// #include <assert.h>
// #include <stdio.h>
// #include <string.h>
// #include <time.h>
// #include <unistd.h>
#ifdef __APPLE__
#include <sys/random.h> // for macOS getentropy()
#endif

SWITCH_DECLARE(int) uuidv7_new(uint8_t *uuid_out)
{
    int8_t status;
    // struct timespec tp;
    uint8_t uuid_prev[16] = {0};
    uint8_t rand_bytes[256] = {0};
    size_t n_rand_consumed = sizeof(rand_bytes);

    uint64_t unix_ts_ms ;
    // clock_gettime(CLOCK_REALTIME, &tp);
    // unix_ts_ms = (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
    unix_ts_ms = switch_time_now() / 1000;

    if (n_rand_consumed > sizeof(rand_bytes) - 10) {
        getentropy(rand_bytes, n_rand_consumed);
        n_rand_consumed = 0;
    }

    status = uuidv7_generate(uuid_prev, unix_ts_ms, &rand_bytes[n_rand_consumed], uuid_prev);
    n_rand_consumed += uuidv7_status_n_rand_consumed(status);
    memcpy(uuid_out, uuid_prev, 16);

    return status;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
