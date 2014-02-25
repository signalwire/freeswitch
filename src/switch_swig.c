/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Brian Fertig <brian.fertig@convergencetek.com>
 *
 * php_freeswitch.c -- PHP Module Framework
 *
 */
#include <switch.h>
#ifdef __ICC
#pragma warning (disable:1418)
#endif

#ifdef _MSC_VER
#include <php.h>
#pragma comment(lib, PHP_LIB)
#endif

void fs_core_set_globals(void)
{
	switch_core_set_globals();
}

int fs_core_init(char *path)
{
	switch_status_t status;
	const char *err = NULL;

	if (zstr(path)) {
		path = NULL;
	}

	status = switch_core_init(path, SCF_NONE, &err);

	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_core_destroy(void)
{
	switch_status_t status;

	status = switch_core_destroy();

	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_loadable_module_init(void)
{
	return switch_loadable_module_init() == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_loadable_module_shutdown(void)
{
	switch_loadable_module_shutdown();
	return 1;
}

int fs_console_loop(void)
{
	switch_console_loop();
	return 0;
}

void fs_consol_log(char *level_str, char *msg)
{
	switch_log_level_t level = SWITCH_LOG_DEBUG;
	if (level_str) {
		level = switch_log_str2level(level_str);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, level, msg);
}

void fs_consol_clean(char *msg)
{
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, msg);
}

switch_core_session_t *fs_core_session_locate(char *uuid)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(uuid))) {
		switch_core_session_rwunlock(session);
	}

	return session;
}

void fs_channel_answer(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_answer(channel);
}

void fs_channel_pre_answer(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_pre_answer(channel);
}

void fs_channel_hangup(switch_core_session_t *session, char *cause)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_hangup(channel, switch_channel_str2cause(cause));
}

void fs_channel_set_variable(switch_core_session_t *session, char *var, char *val)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_set_variable(channel, var, val);
}

void fs_channel_get_variable(switch_core_session_t *session, char *var)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_get_variable(channel, var);
}

void fs_channel_set_state(switch_core_session_t *session, char *state)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t fs_state = switch_channel_get_state(channel);

	if ((fs_state = switch_channel_name_state(state)) < CS_HANGUP) {
		switch_channel_set_state(channel, fs_state);
	}
}

/*
  IVR Routines!  You can do IVR in PHP NOW!
*/

int fs_ivr_play_file(switch_core_session_t *session, switch_file_handle_t *fh, char *file, switch_input_args_t *args)
{
	switch_status_t status;

	status = switch_ivr_play_file(session, fh, file, args);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_switch_ivr_record_file(switch_core_session_t *session, switch_file_handle_t *fh, char *file, switch_input_args_t *args, unsigned int limit)
{
	switch_status_t status;

	status = switch_ivr_record_file(session, fh, file, args, limit);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_switch_ivr_sleep(switch_core_session_t *session, uint32_t ms)
{
	switch_status_t status;
	status = switch_ivr_sleep(session, ms);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_ivr_play_file2(switch_core_session_t *session, char *file)
{
	switch_status_t status;

	status = switch_ivr_play_file(session, NULL, file, NULL);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}


int fs_switch_ivr_collect_digits_callback(switch_core_session_t *session, switch_input_args_t *args, unsigned int timeout)
{
	switch_status_t status;

	status = switch_ivr_collect_digits_callback(session, args, timeout);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_switch_ivr_collect_digits_count(switch_core_session_t *session,
									   char *buf, unsigned int buflen, unsigned int maxdigits, const char *terminators, char *terminator,
									   unsigned int timeout)
{
	switch_status_t status;

	status = switch_ivr_collect_digits_count(session, buf, buflen, maxdigits, terminators, terminator, timeout);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

/*int fs_switch_ivr_multi_threaded_bridge   (switch_core_session_t *session,
  switch_core_session_t *peer_session,
  switch_input_callback_function_t dtmf_callback,
  void *session_data,
  void *peer_session_data)  
  {
  switch_status_t status;

  status = switch_ivr_multi_threaded_bridge(session, peer_session, dtmf_callback, session_data, peer_session_data);
  return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
  }
*/

int fs_switch_ivr_originate(switch_core_session_t *session, switch_core_session_t **bleg, char *bridgeto, uint32_t timelimit_sec)
/*const switch_state_handler_table_t *table,
  char *  	cid_name_override,
  char *  	cid_num_override,
  switch_caller_profile_t *caller_profile_override)  */
{

	switch_channel_t *caller_channel;
	switch_core_session_t *peer_session;
	unsigned int timelimit = 60;
	char *var;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	if ((var = switch_channel_get_variable(caller_channel, "call_timeout"))) {
		timelimit = atoi(var);
	}

	if (switch_ivr_originate(session, &peer_session, &cause, bridgeto, timelimit, NULL, NULL, NULL, NULL, SOF_NONE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Cannot Create Outgoing Channel!\n");
		switch_channel_hangup(caller_channel, SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
		return;
	} else {
		switch_ivr_multi_threaded_bridge(session, peer_session, NULL, NULL, NULL);
		switch_core_session_rwunlock(peer_session);
	}
}

int fs_switch_ivr_session_transfer(switch_core_session_t *session, char *extension, char *dialplan, char *context)
{
	switch_status_t status;

	status = switch_ivr_session_transfer(session, extension, dialplan, context);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int fs_switch_ivr_speak_text(switch_core_session_t *session, char *tts_name, char *voice_name, char *text)
{
	switch_status_t status;

	status = switch_ivr_speak_text(session, tts_name, voice_name, text, NULL);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

/*

*******  CHANNEL STUFF  *******

*/

char *fs_switch_channel_get_variable(switch_channel_t *channel, char *varname)
{
	return switch_channel_get_variable(channel, varname);
}

int fs_switch_channel_set_variable(switch_channel_t *channel, char *varname, char *value)
{
	switch_status_t status;

	status = switch_channel_set_variable(channel, varname, value);
	return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
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
