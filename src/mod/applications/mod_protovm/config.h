#ifndef _CONFIG_H_
#define _CONFIG_H_

extern const char *global_cf;

struct vmivr_profile {
	const char *name;

	const char *domain;
	const char *id;

	int current_msg;
	const char *current_msg_uuid;

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
	const char *api_pref_recname_set;
	const char *api_pref_password_set;

};
typedef struct vmivr_profile vmivr_profile_t;

struct vmivr_menu_profile {
	const char *name;

        switch_event_t *event_keys_action;
	switch_event_t *event_keys_dtmf;
	switch_event_t *event_keys_varname;
        switch_event_t *event_phrases;
};
typedef struct vmivr_menu_profile vmivr_menu_profile_t;

vmivr_profile_t *get_profile(switch_core_session_t *session, const char *profile_name);

void free_profile_menu_event(vmivr_menu_profile_t *menu);
void populate_profile_menu_event(vmivr_profile_t *profile, vmivr_menu_profile_t *menu);

#endif /* _CONFIG_H_ */
