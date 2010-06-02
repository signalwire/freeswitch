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
 * The Original Code is FreeSWITCH mod_spandsp.
 *
 * The Initial Developer of the Original Code is
 * Michael Jerris <mike@jerris.com
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Massimo Cetra <devel@navynet.it>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Brian West <brian@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * Antonio Gallo <agx@linux.it>
 * Christopher M. Rienzo <chris@rienzo.net>
 * mod_spandsp.c -- Module implementing spandsp fax, dsp, and codec functionality
 *
 */

#include "mod_spandsp.h"
#include <spandsp/version.h>

/* **************************************************************************
   FREESWITCH MODULE DEFINITIONS
   ************************************************************************* */

#define SPANFAX_RX_USAGE "<filename>"
#define SPANFAX_TX_USAGE "<filename>"

SWITCH_MODULE_LOAD_FUNCTION(mod_spandsp_init);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spandsp_shutdown);
SWITCH_MODULE_DEFINITION(mod_spandsp, mod_spandsp_init, mod_spandsp_shutdown, NULL);

static switch_event_node_t *NODE = NULL;

SWITCH_STANDARD_APP(spanfax_tx_function)
{
	mod_spandsp_fax_process_fax(session, data, FUNCTION_TX);
}

SWITCH_STANDARD_APP(spanfax_rx_function)
{
	mod_spandsp_fax_process_fax(session, data, FUNCTION_RX);
}

SWITCH_STANDARD_APP(dtmf_session_function)
{
	spandsp_inband_dtmf_session(session);
}

SWITCH_STANDARD_APP(stop_dtmf_session_function)
{
	spandsp_stop_inband_dtmf_session(session);
}

static void event_handler(switch_event_t *event)
{
	mod_spandsp_fax_event_handler(event);
}

SWITCH_STANDARD_APP(t38_gateway_function)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    time_t timeout = switch_epoch_time_now(NULL) + 20;
    const char *var;

    if (zstr(data) || strcasecmp(data, "self")) {
        data = "peer";
    }

    switch_channel_set_variable(channel, "t38_leg", data);

    if ((var = switch_channel_get_variable(channel, "t38_gateway_detect_timeout"))) {
        long to = atol(var);
        if (to > -1) {
            timeout = (time_t) (switch_epoch_time_now(NULL) + to);
        } else {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s invalid timeout value.\n", switch_channel_get_name(channel));
        }
    }
    
	switch_ivr_tone_detect_session(session, "t38", "1100.0", "rw", timeout, 1, data, NULL, t38_gateway_start);
}

/**
 * Start tone detector application
 *
 * @param data the command string
 */
SWITCH_STANDARD_APP(start_tone_detect_app)
{
	switch_channel_t *channel;
	if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No session\n");
		return;
	}
	channel = switch_core_session_get_channel(session);
	if (zstr(data)) {
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "-ERR missing descriptor name");
	} else if (callprogress_detector_start(session, data) != SWITCH_STATUS_SUCCESS) {
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "-ERR failed to start tone detector");
	} else {
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "+OK started");
	}
}

/**
 * Start tone detector API
 */
SWITCH_STANDARD_API(start_tone_detect_api)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR missing descriptor name\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (!session) {
		stream->write_function(stream, "-ERR no session\n");
		return SWITCH_STATUS_SUCCESS;
	}

	status = callprogress_detector_start(session, cmd);
	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK started\n");
	} else {
		stream->write_function(stream, "-ERR failed to start tone detector\n");
	}

	return status;
}

/**
 * Stop tone detector application
 *
 * @param data the command string
 */
SWITCH_STANDARD_APP(stop_tone_detect_app)
{
	switch_channel_t *channel;
	if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No session\n");
		return;
	}
	channel = switch_core_session_get_channel(session);
	callprogress_detector_stop(session);
	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "+OK stopped");
}

/**
 * Stop tone detector API
 */
SWITCH_STANDARD_API(stop_tone_detect_api)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	if (!session) {
		stream->write_function(stream, "-ERR no session\n");
		return SWITCH_STATUS_SUCCESS;
	}
	callprogress_detector_stop(session);
	stream->write_function(stream, "+OK stopped\n");
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_spandsp_init)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "t38_gateway", "Convert to T38 Gateway if tones are heard", "Convert to T38 Gateway if tones are heard", 
                   t38_gateway_function, "", SAF_MEDIA_TAP);

	SWITCH_ADD_APP(app_interface, "rxfax", "FAX Receive Application", "FAX Receive Application", spanfax_rx_function, SPANFAX_RX_USAGE,
				   SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "txfax", "FAX Transmit Application", "FAX Transmit Application", spanfax_tx_function, SPANFAX_TX_USAGE,
				   SAF_SUPPORT_NOMEDIA);

	SWITCH_ADD_APP(app_interface, "spandsp_stop_dtmf", "stop inband dtmf", "Stop detecting inband dtmf.", stop_dtmf_session_function, "", SAF_NONE);
	SWITCH_ADD_APP(app_interface, "spandsp_start_dtmf", "Detect dtmf", "Detect inband dtmf on the session", dtmf_session_function, "", SAF_MEDIA_TAP);

	mod_spandsp_fax_load(pool);
    mod_spandsp_codecs_load(module_interface, pool);

	if (mod_spandsp_dsp_load(module_interface, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load spandsp.conf, not adding tone_detect applications\n");
    } else {
        SWITCH_ADD_APP(app_interface, "start_tone_detect", "Start background tone detection with cadence", "", start_tone_detect_app, "[name]", SAF_NONE);
        SWITCH_ADD_APP(app_interface, "stop_tone_detect", "Stop background tone detection with cadence", "", stop_tone_detect_app, "", SAF_NONE);
        SWITCH_ADD_API(api_interface, "start_tone_detect", "Start background tone detection with cadence", start_tone_detect_api, "[name]");
        SWITCH_ADD_API(api_interface, "stop_tone_detect", "Stop background tone detection with cadence", stop_tone_detect_api, "");
	}

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind our reloadxml handler!\n");
		/* Not such severe to prevent loading */
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "mod_spandsp loaded, using spandsp library version [%s]\n", SPANDSP_RELEASE_DATETIME_STRING);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_spandsp_shutdown)
{
	switch_event_unbind(&NODE);

	mod_spandsp_fax_shutdown();
	mod_spandsp_dsp_shutdown();

	return SWITCH_STATUS_UNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
