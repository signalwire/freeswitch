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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * utils.c -- VoiceMail IVR / Different utility that might need to go into the core (after cleanup)
 *
 */
#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"

void append_event_message(switch_core_session_t *session, vmivr_profile_t *profile, switch_event_t *phrase_params, switch_event_t *msg_list_event, size_t current_msg);
char *generate_random_file_name(switch_core_session_t *session, const char *mod_name, const char *file_extension);
switch_event_t *jsonapi2event(switch_core_session_t *session, const char *api, const char *data);
void jsonapi_populate_event(switch_core_session_t *session, switch_event_t *apply_event, const char *api, const char *data);
switch_status_t vmivr_merge_media_files(const char** inputs, const char *output, int rate);
switch_status_t vmivr_api_execute(switch_core_session_t *session, const char *apiname, const char *arguments);
#endif /* _UTIL_H_ */

