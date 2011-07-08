#ifndef _MENU_H_
#define _MENU_H_

#include "config.h"

void mtvm_menu_purge(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_authenticate(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_main(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_record_name(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_set_password(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_select_greeting_slot(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_record_greeting_with_slot(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_preference(switch_core_session_t *session, vmivr_profile_t *profile);
void mtvm_menu_forward(switch_core_session_t *session, vmivr_profile_t *profile);

switch_status_t mtvm_menu_record(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_profile_t menu, const char *file_name);
char *mtvm_menu_get_input_set(switch_core_session_t *session, vmivr_profile_t *profile, vmivr_menu_profile_t menu, const char *input_mask, const char *terminate_key);


struct vmivr_menu_function {
        const char *name;
	void (*pt2Func)(switch_core_session_t *session, vmivr_profile_t *profile);

};
typedef struct vmivr_menu_function vmivr_menu_function_t;

extern vmivr_menu_function_t menu_list[];

void (*mtvm_get_menu_function(const char *menu_name))(switch_core_session_t *session, vmivr_profile_t *profile);

#endif /* _MENU_H_ */

