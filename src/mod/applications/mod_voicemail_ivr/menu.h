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
 * menu.h -- VoiceMail IVR Menu Include
 *
 */
#ifndef _MENU_H_
#define _MENU_H_

#include "config.h"

void vmivr_menu_purge(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_authenticate(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_main(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_navigator(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_record_name(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_set_password(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_select_greeting_slot(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_record_greeting_with_slot(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_preference(switch_core_session_t *session, vmivr_profile_t *profile);
void vmivr_menu_forward(switch_core_session_t *session, vmivr_profile_t *profile);

switch_status_t vmivr_menu_record(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_t *menu, const char *file_name);
char *vmivr_menu_get_input_set(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_t *menu, const char *input_mask);


struct vmivr_menu_function {
	const char *name;
	void (*pt2Func)(switch_core_session_t *session, vmivr_profile_t *profile);

};
typedef struct vmivr_menu_function vmivr_menu_function_t;

extern vmivr_menu_function_t menu_list[];

void (*vmivr_get_menu_function(const char *menu_name))(switch_core_session_t *session, vmivr_profile_t *profile);

#endif /* _MENU_H_ */

