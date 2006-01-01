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
 * switch_channel.h -- Media Channel Interface
 *
 */
/*! \file switch_channel.h
    \brief Media Channel Interface
*/

#ifndef SWITCH_CHANNEL_H
#define SWITCH_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

SWITCH_DECLARE(switch_channel_state) switch_channel_get_state(switch_channel *channel);
SWITCH_DECLARE(switch_channel_state) switch_channel_set_state(switch_channel *channel, switch_channel_state state);


SWITCH_DECLARE(switch_status) switch_channel_alloc(switch_channel **channel, switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_channel_init(switch_channel *channel,
								switch_core_session *session,
								switch_channel_state state,
								switch_channel_flag flags);


SWITCH_DECLARE(void) switch_channel_set_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_caller_profile(switch_channel *channel);
SWITCH_DECLARE(void) switch_channel_set_originator_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originator_caller_profile(switch_channel *channel);
SWITCH_DECLARE(void) switch_channel_set_originatee_caller_profile(switch_channel *channel, switch_caller_profile *caller_profile);
SWITCH_DECLARE(switch_caller_profile *) switch_channel_get_originatee_caller_profile(switch_channel *channel);
SWITCH_DECLARE(char *) switch_channel_get_variable(switch_channel *channel, char *varname);
SWITCH_DECLARE(switch_status) switch_channel_set_variable(switch_channel *channel, char *varname, char *value);
SWITCH_DECLARE(void) switch_channel_set_caller_extension(switch_channel *channel, switch_caller_extension *caller_extension);
SWITCH_DECLARE(switch_caller_extension *) switch_channel_get_caller_extension(switch_channel *channel);

SWITCH_DECLARE(switch_status) switch_channel_test_flag(switch_channel *channel, int flags);
SWITCH_DECLARE(switch_status) switch_channel_set_flag(switch_channel *channel, int flags);
SWITCH_DECLARE(switch_status) switch_channel_clear_flag(switch_channel *channel, int flags);
SWITCH_DECLARE(switch_status) switch_channel_answer(switch_channel *channel);
SWITCH_DECLARE(void) switch_channel_set_event_handlers(switch_channel *channel, const struct switch_event_handler_table *event_handlers);
SWITCH_DECLARE(const struct switch_event_handler_table *) switch_channel_get_event_handlers(switch_channel *channel);
SWITCH_DECLARE(switch_status) switch_channel_set_private(switch_channel *channel, void *private);
SWITCH_DECLARE(void *) switch_channel_get_private(switch_channel *channel);
SWITCH_DECLARE(switch_status) switch_channel_set_name(switch_channel *channel, char *name);
SWITCH_DECLARE(char *) switch_channel_get_name(switch_channel *channel);
SWITCH_DECLARE(switch_status) switch_channel_hangup(switch_channel *channel);
SWITCH_DECLARE(int) switch_channel_has_dtmf(switch_channel *channel);
SWITCH_DECLARE(switch_status) switch_channel_queue_dtmf(switch_channel *channel, char *dtmf);
SWITCH_DECLARE(int) switch_channel_dequeue_dtmf(switch_channel *channel, char *dtmf, size_t len);
SWITCH_DECLARE(switch_status) switch_channel_set_raw_mode (switch_channel *channel, int freq, int bits, int channels, int ms, int kbps);
SWITCH_DECLARE(switch_status) switch_channel_get_raw_mode (switch_channel *channel, int *freq, int *bits, int *channels, int *ms, int *kbps);
SWITCH_DECLARE(const char *) switch_channel_state_name(switch_channel_state state);
SWITCH_DECLARE(void) switch_channel_event_set_data(switch_channel *channel, switch_event *event);
#ifdef __cplusplus
}
#endif

#endif
