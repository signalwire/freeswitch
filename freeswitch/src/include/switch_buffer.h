/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_buffer.h -- Data Buffering Code
 *
 */
#ifndef SWITCH_BUFFER_H
#define SWITCH_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct switch_buffer;

SWITCH_DECLARE(switch_status) switch_buffer_create(switch_memory_pool *pool, switch_buffer **buffer, size_t max_len);
SWITCH_DECLARE(int) switch_buffer_len(switch_buffer *buffer);
SWITCH_DECLARE(int) switch_buffer_freespace(switch_buffer *buffer);
SWITCH_DECLARE(int) switch_buffer_inuse(switch_buffer *buffer);
SWITCH_DECLARE(int) switch_buffer_read(switch_buffer *buffer, void *data, size_t datalen);
SWITCH_DECLARE(int) switch_buffer_write(switch_buffer *buffer, void *data, size_t datalen);
SWITCH_DECLARE(int) switch_buffer_toss(switch_buffer *buffer, size_t datalen);


#ifdef __cplusplus
}
#endif


#endif
