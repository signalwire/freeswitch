/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2014, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * rayo_cpa_detector.h -- Rayo call progress analysis
 *
 */
#ifndef RAYO_CPA_DETECTOR_H
#define RAYO_CPA_DETECTOR_H

#include <switch.h>

#include "mod_rayo.h"

SWITCH_DECLARE(switch_status_t) rayo_cpa_detector_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
SWITCH_DECLARE(void) rayo_cpa_detector_shutdown(void);
SWITCH_DECLARE(int) rayo_cpa_detector_start(const char *call_uuid, const char *signal_ns, const char **error_detail);
SWITCH_DECLARE(void) rayo_cpa_detector_stop(const char *call_uuid, const char *signal_ns);

#endif


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
