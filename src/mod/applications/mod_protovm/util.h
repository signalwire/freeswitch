#ifndef _UTIL_H_
#define _UTIL_H_

#include "config.h"

switch_status_t mt_merge_files(const char** inputs, const char *output);

void append_event_message(switch_core_session_t *session, vmivr_profile_t *profile, switch_event_t *phrase_params, switch_event_t *msg_list_event, size_t current_msg);
void append_event_profile(switch_event_t *phrase_params, vmivr_profile_t *profile, vmivr_menu_profile_t menu);
char *generate_random_file_name(switch_core_session_t *session, const char *mod_name, char *file_extension);
switch_event_t *jsonapi2event(switch_core_session_t *session, switch_event_t *apply_event, const char *api, const char *data);
switch_status_t mt_merge_media_files(const char** inputs, const char *output);
switch_status_t mt_api_execute(switch_core_session_t *session, const char *apiname, const char *arguments);
void populate_dtmfa_from_event(switch_event_t *phrase_params, vmivr_profile_t *profile, vmivr_menu_profile_t menu, char **dtmfa);
#endif /* _UTIL_H_ */

