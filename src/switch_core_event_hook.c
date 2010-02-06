/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_core_event_hook.c Core Event Hooks
 *
 */
#include "switch.h"
#include "private/switch_core_pvt.h"

NEW_HOOK_DECL(outgoing_channel)
	NEW_HOOK_DECL(receive_message)
	NEW_HOOK_DECL(receive_event)
	NEW_HOOK_DECL(state_change)
	NEW_HOOK_DECL(read_frame)
	NEW_HOOK_DECL(write_frame)
	NEW_HOOK_DECL(video_read_frame)
	NEW_HOOK_DECL(video_write_frame)
	NEW_HOOK_DECL(kill_channel)
	NEW_HOOK_DECL(send_dtmf)
	NEW_HOOK_DECL(recv_dtmf)
	NEW_HOOK_DECL(resurrect_session)

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
