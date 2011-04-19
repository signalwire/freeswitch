/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH mod_spandsp.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * Antonio Gallo <agx@linux.it>
 * mod_spandsp.h -- applications provided by SpanDSP
 *
 */

#include <switch.h>

#ifdef WIN32
#define FAX_INVALID_SOCKET INVALID_HANDLE_VALUE
typedef HANDLE zap_socket_t;
#else
#define FAX_INVALID_SOCKET -1
typedef int zap_socket_t;
#endif

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>


typedef enum {
	FUNCTION_TX,
	FUNCTION_RX,
    FUNCTION_GW
} mod_spandsp_fax_application_mode_t;

void mod_spandsp_fax_load(switch_memory_pool_t *pool);
switch_status_t mod_spandsp_codecs_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool);
switch_status_t mod_spandsp_dsp_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool);

void mod_spandsp_fax_shutdown(void);
void mod_spandsp_dsp_shutdown(void);

void mod_spandsp_fax_event_handler(switch_event_t *event);
void mod_spandsp_fax_process_fax(switch_core_session_t *session, const char *data, mod_spandsp_fax_application_mode_t app_mode);
switch_bool_t t38_gateway_start(switch_core_session_t *session, const char *app, const char *data);

switch_status_t spandsp_stop_inband_dtmf_session(switch_core_session_t *session);
switch_status_t spandsp_inband_dtmf_session(switch_core_session_t *session);

switch_status_t callprogress_detector_start(switch_core_session_t *session, const char *name);
switch_status_t callprogress_detector_stop(switch_core_session_t *session);

switch_status_t spandsp_fax_detect_session(switch_core_session_t *session,
														   const char *flags, time_t timeout,
														   int hits, const char *app, const char *data, switch_tone_detect_callback_t callback);

switch_status_t spandsp_fax_stop_detect_session(switch_core_session_t *session);
