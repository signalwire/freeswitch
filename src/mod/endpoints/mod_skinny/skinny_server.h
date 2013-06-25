/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
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
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * skinny_server.h -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */
#ifndef _SKINNY_SERVER_H
#define _SKINNY_SERVER_H

#include <switch.h>

/* SESSION FUNCTIONS */
switch_status_t skinny_create_ingoing_session(listener_t *listener, uint32_t *line_instance, switch_core_session_t **session);
skinny_action_t skinny_session_dest_match_pattern(switch_core_session_t *session, char **data);
switch_status_t skinny_session_process_dest(switch_core_session_t *session, listener_t *listener, uint32_t line_instance, char *dest, char append_dest, uint32_t backspace);
switch_status_t skinny_session_send_call_info(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_send_call_info_all(switch_core_session_t *session);
switch_status_t skinny_session_set_variables(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_call_cause_t skinny_ring_lines(private_t *tech_pvt, switch_core_session_t *remote_session);
switch_status_t skinny_session_ring_out(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_answer(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_start_media(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_hold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_unhold_line(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_session_stop_media(switch_core_session_t *session, listener_t *listener, uint32_t line_instance);
switch_status_t skinny_hold_active_calls(listener_t *listener);

/* SKINNY MESSAGE HANDLERS */

#endif /* _SKINNY_SERVER_H */

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

