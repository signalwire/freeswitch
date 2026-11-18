/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/F
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Michael Jerris <mike@jerris.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Michael Jerris <mike@jerris.com>
 * Andrey Volk <andrey@signalwrie.com>
 *
 * switch_spandsp.c -- spandsp wrappers and extensions
 *
 */

#include <switch.h>

#define SPANDSP_NO_TIFF 1
#include "spandsp.h"

SWITCH_DECLARE(switch_plc_state_t *) switch_plc_init(switch_plc_state_t *s) {
	return (switch_plc_state_t *)plc_init((plc_state_t *)s);
}

SWITCH_DECLARE(int) switch_plc_free(switch_plc_state_t *s) {
	return plc_free((plc_state_t *)s);
}

SWITCH_DECLARE(int) switch_plc_fillin(switch_plc_state_t *s, int16_t amp[], int len) {
	return plc_fillin((plc_state_t *)s, amp, len);
}

SWITCH_DECLARE(int) switch_plc_rx(switch_plc_state_t* s, int16_t amp[], int len)
{
	return plc_rx((plc_state_t*)s, amp, len);
}


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
