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

#define RAYO_CPA_BASE RAYO_BASE "cpa:"
#define RAYO_CPA_NS RAYO_CPA_BASE RAYO_VERSION

#define RAYO_CPA_BEEP_NS RAYO_CPA_BASE "beep:" RAYO_VERSION
#define RAYO_CPA_DTMF_NS RAYO_CPA_BASE "dtmf:" RAYO_VERSION
#define RAYO_CPA_SPEECH_NS RAYO_CPA_BASE "speech:" RAYO_VERSION
#define RAYO_CPA_FAX_CED_NS RAYO_CPA_BASE "fax-ced:" RAYO_VERSION
#define RAYO_CPA_FAX_CNG_NS RAYO_CPA_BASE "fax-cng:" RAYO_VERSION
#define RAYO_CPA_RING_NS RAYO_CPA_BASE "ring:" RAYO_VERSION
#define RAYO_CPA_BUSY_NS RAYO_CPA_BASE "busy:" RAYO_VERSION
#define RAYO_CPA_CONGESTION_NS RAYO_CPA_BASE "congestion:" RAYO_VERSION
#define RAYO_CPA_SIT_NS RAYO_CPA_BASE "sit:" RAYO_VERSION
#define RAYO_CPA_MODEM_NS RAYO_CPA_BASE "modem:" RAYO_VERSION
#define RAYO_CPA_OFFHOOK_NS RAYO_CPA_BASE "offhook:" RAYO_VERSION

extern switch_status_t rayo_cpa_detector_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool, const char *config_file);
extern void rayo_cpa_detector_shutdown(void);
extern int rayo_cpa_detector_start(const char *call_uuid, const char *signal_ns, const char **error_detail);
extern void rayo_cpa_detector_stop(const char *call_uuid, const char *signal_ns);

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
