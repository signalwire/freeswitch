/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_playback.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

static const char modname[] = "mod_playback";

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{

	
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF: {
		char *dtmf = (char *) input;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);
		
		if (*dtmf == '*') {
			return SWITCH_STATUS_BREAK;
		}
	}
		break;
	default:
		break;
	}
	
	return SWITCH_STATUS_SUCCESS;
}


static void speak_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	char buf[10];
	char *argv[4] = {0};
	int argc;
	char *engine = NULL;
	char *voice = NULL;
	char *text = NULL;
	char *timer_name = NULL;
	char *mydata = NULL;
	switch_codec_t *codec;

	codec = switch_core_session_get_read_codec(session);
	assert(codec != NULL);

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	mydata = switch_core_session_strdup(session, data);
	argc = switch_separate_string(mydata, '|', argv, sizeof(argv)/sizeof(argv[0]));

	engine = argv[0];
	voice = argv[1];
	text = argv[2];
	timer_name = argv[3];
	
	if (!(engine && voice && text)) {
		if (!engine) {
			engine = "NULL";
		}
		if (!voice) {
			voice = "NULL";
		}
		if (!text) {
			text = "NULL";
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Params! [%s][%s][%s]\n", engine, voice, text);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

	switch_channel_pre_answer(channel);
	switch_ivr_speak_text(session, engine, voice, timer_name, codec->implementation->samples_per_second, on_dtmf, text, buf, sizeof(buf));
	
}

static void playback_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	char *timer_name = NULL;
	char *file_name = NULL;

	file_name = switch_core_session_strdup(session, data);

	if ((timer_name = strchr(file_name, ' ')) != 0) {
		*timer_name++ = '\0';
	}

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	switch_channel_pre_answer(channel);

	switch_ivr_play_file(session, NULL, file_name, timer_name, on_dtmf, NULL, 0);

}


static void record_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	switch_status_t status;
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	status = switch_ivr_record_file(session, NULL, data, on_dtmf, NULL, 0);

	if (!switch_channel_ready(channel) || (status != SWITCH_STATUS_SUCCESS && !SWITCH_STATUS_IS_BREAK(status))) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
}


static void record_session_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	switch_ivr_record_session(session, data, NULL);	
}


static void stop_record_session_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);

	switch_ivr_stop_record_session(session, data);	
}


static const switch_application_interface_t speak_application_interface = {
	/*.interface_name */ "speak",
	/*.application_function */ speak_function
};

static const switch_application_interface_t record_application_interface = {
	/*.interface_name */ "record",
	/*.application_function */ record_function,
	NULL,NULL,NULL,
	&speak_application_interface
};


static const switch_application_interface_t record_session_application_interface = {
	/*.interface_name */ "record_session",
	/*.application_function */ record_session_function,
	NULL,NULL,NULL,
	&record_application_interface
};


static const switch_application_interface_t stop_record_session_application_interface = {
	/*.interface_name */ "stop_record_session",
	/*.application_function */ stop_record_session_function,
	NULL,NULL,NULL,
	&record_session_application_interface
};

static const switch_application_interface_t playback_application_interface = {
	/*.interface_name */ "playback",
	/*.application_function */ playback_function,
	NULL,NULL,NULL,
	/*.next*/				  &stop_record_session_application_interface
};

static const switch_loadable_module_interface_t mod_playback_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &playback_application_interface
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_playback_module_interface;


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
*/


//switch_status_t switch_module_runtime(void)

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
