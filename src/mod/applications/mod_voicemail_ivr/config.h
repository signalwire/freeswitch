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
 * config.c -- VoiceMail IVR Config
 *
 */
#include "ivr.h"

#ifndef _CONFIG_H_
#define _CONFIG_H_

extern const char *global_cf;

#define VM_FOLDER_ROOT "inbox";
#define VM_MSG_NOT_READ "not-read"
#define VM_MSG_SAVED "save"
#define VM_MSG_NEW "new"

struct vmivr_profile {
	const char *name;

	const char *domain;
	const char *id;

	int current_msg;
	const char *current_msg_uuid;

	const char *folder_name;
	const char *folder_filter;

	const char *menu_check_auth;
	const char *menu_check_main;
	const char *menu_check_terminate;

	switch_bool_t authorized;

	const char *api_profile;
	const char *api_auth_login;
	const char *api_msg_delete;
	const char *api_msg_undelete;
	const char *api_msg_list;
	const char *api_msg_count;
	const char *api_msg_save;
	const char *api_msg_purge;
	const char *api_msg_get;
	const char *api_msg_forward;
	const char *api_pref_greeting_set;
	const char *api_pref_greeting_get;
	const char *api_pref_recname_set;
	const char *api_pref_password_set;

	switch_event_t *event_settings;
};
typedef struct vmivr_profile vmivr_profile_t;

struct vmivr_menu {
	const char *name;
	vmivr_profile_t *profile;

	switch_event_t *event_keys_action;
	switch_event_t *event_keys_dtmf;
	switch_event_t *event_keys_varname;
	switch_event_t *event_settings;
	switch_event_t *event_phrases;

	char *dtmfa[16];
	switch_event_t *phrase_params;
	ivre_data_t ivre_d;

	int ivr_maximum_attempts;
	int ivr_entry_timeout;
};
typedef struct vmivr_menu vmivr_menu_t;

vmivr_profile_t *get_profile(switch_core_session_t *session, const char *profile_name);
void free_profile(vmivr_profile_t *profile);

void menu_init(vmivr_profile_t *profile, vmivr_menu_t *menu);
void menu_instance_init(vmivr_menu_t *menu);
void menu_instance_free(vmivr_menu_t *menu);
void menu_free(vmivr_menu_t *menu);

#endif /* _CONFIG_H_ */
