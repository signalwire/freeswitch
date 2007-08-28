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
 *
 *
 * mod_ivrtest.c -- Raw Audio File Streaming Application Module
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_ivrtest_load);
SWITCH_MODULE_DEFINITION(mod_ivrtest, mod_ivrtest_load, NULL, NULL);

/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:{
			char *dtmf = (char *) input;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);

			switch_copy_string((char *) buf, dtmf, buflen);
			return SWITCH_STATUS_BREAK;
		}
		break;
	default:
		break;
	}
	return SWITCH_STATUS_SUCCESS;
}

static void disast_function(switch_core_session_t *session, char *data)
{
	void *x = NULL;
	memset((void *) x, 0, 1000);
	//printf("%s WOOHOO\n", (char *) 42);
}


static void xml_function(switch_core_session_t *session, char *data)
{
	switch_xml_t f1 = switch_xml_parse_file("/root/formula1.xml"), team, driver;
	const char *teamname;

	for (team = switch_xml_child(f1, "team"); team; team = team->next) {
		teamname = switch_xml_attr_soft(team, "name");
		for (driver = switch_xml_child(team, "driver"); driver; driver = driver->next) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "%s, %s: %s\n", switch_xml_child(driver, "name")->txt, teamname, switch_xml_child(driver, "points")->txt);
		}
	}
	switch_xml_free(f1);
}


static void ivr_application_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *params = switch_core_session_strdup(session, data);

	if (channel != NULL && params != NULL) {
		switch_ivr_menu_t *menu = NULL, *sub_menu = NULL;
		switch_status_t status;

		// answer the channel if need be
		switch_channel_answer(channel);

		status = switch_ivr_menu_init(&menu,
									  NULL,
									  "main",
									  "please enter some numbers so i can figure out if I have any bugs or not",
									  "enter some numbers", NULL, "I have no idea what that is", "cepstral", "david", NULL, 15000, 10, NULL);


		status = switch_ivr_menu_init(&sub_menu,
									  menu, "sub", "/ram/congrats.wav", "/ram/extension.wav", NULL, "/ram/invalid.wav", NULL, NULL, NULL, 15000, 10, NULL);

		if (status == SWITCH_STATUS_SUCCESS) {
			// build the menu
			switch_ivr_menu_bind_action(menu, SWITCH_IVR_ACTION_PLAYSOUND, "/ram/swimp.raw", "1");
			switch_ivr_menu_bind_action(menu, SWITCH_IVR_ACTION_PLAYSOUND, "/ram/congrats.wav", "2");
			switch_ivr_menu_bind_action(menu, SWITCH_IVR_ACTION_EXECMENU, "sub", "3");
			//switch_ivr_menu_bind_action(menu, SWITCH_IVR_ACTION_PLAYSOUND, "/usr/local/freeswitch/sounds/3.wav", "3");


			switch_ivr_menu_bind_action(sub_menu, SWITCH_IVR_ACTION_PLAYSOUND, "/ram/swimp.raw", "1");
			switch_ivr_menu_bind_action(sub_menu, SWITCH_IVR_ACTION_BACK, NULL, "2");


			// start the menu
			status = switch_ivr_menu_execute(session, menu, "main", NULL);

			// cleaup the menu
			switch_ivr_menu_stack_free(menu);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unable to build menu %s\n", params);
		}


		switch_ivr_play_file(session, NULL, "/ram/goodbye.wav", NULL);
	}
}

static void dirtest_function(switch_core_session_t *session, char *data)
{
	char *var, *val;
	switch_channel_t *channel;
	switch_directory_handle_t dh;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);


	if (switch_core_directory_open(&dh, "ldap", "ldap.freeswitch.org", "cn=Manager,dc=freeswitch,dc=org", "test", NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't connect\n");
		return;
	}


	switch_core_directory_query(&dh, "ou=dialplan,dc=freeswitch,dc=org", "(objectClass=*)");
	while (switch_core_directory_next(&dh) == SWITCH_STATUS_SUCCESS) {
		while (switch_core_directory_next_pair(&dh, &var, &val) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "VALUE [%s]=[%s]\n", var, val);
		}
	}

	switch_core_directory_close(&dh);
	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

}

static switch_status_t show_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:{
			char *dtmf = (char *) input;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);

			switch_copy_string((char *) buf, dtmf, buflen);
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static void tts_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	switch_codec_t *codec;
	char *mydata, *text = NULL, *voice_name = NULL, *tts_name = NULL;
	char buf[10] = "";
	char *argv[3];
	int argc;
	switch_input_args_t args = { 0 };

	if (!(mydata = switch_core_session_strdup(session, (char *) data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ':', argv, sizeof(argv) / sizeof(argv[0]))) > 1 && argc) {
		tts_name = argv[0];
		voice_name = argv[1];
		text = argv[2];
	}

	if (voice_name && !text) {
		text = argv[1];
		voice_name = NULL;
	}

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_answer(channel);

	codec = switch_core_session_get_read_codec(session);
	args.input_callback = show_dtmf;
	args.buf = buf;
	args.buflen = sizeof(buf);
	switch_ivr_speak_text(session, tts_name, voice_name, text, &args);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Done\n");
}

#ifdef BUGTEST
static switch_bool_t bug_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_frame_t *frame;

	switch (type) {
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		frame = switch_core_media_bug_get_replace_frame(bug);
		switch_core_media_bug_set_replace_frame(bug, frame);
		printf("W00t\n");
		break;
	default:
		break;
	}

	return SWITCH_TRUE;
}
#endif

static void bugtest_function(switch_core_session_t *session, char *data)
{
#ifdef BUGTEST
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;

	if ((status = switch_core_media_bug_add(session, bug_callback, NULL, SMBF_WRITE_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return;
	}
#endif
	//switch_ivr_schedule_broadcast(time(NULL) + 10, switch_core_session_get_uuid(session), "/Users/anthm/sr8k.wav", SMF_ECHO_ALEG);
	//switch_ivr_schedule_transfer(time(NULL) + 10, switch_core_session_get_uuid(session), "2000", NULL, NULL);
	//switch_ivr_schedule_hangup(time(NULL) + 10, switch_core_session_get_uuid(session), SWITCH_CAUSE_ALLOTTED_TIMEOUT);

	switch_ivr_play_file(session, NULL, data, NULL);
}

#if 1
static void asrtest_function(switch_core_session_t *session, char *data)
{
	switch_ivr_detect_speech(session, "lumenvox", "demo", data, "127.0.0.1", NULL);
}

#else
static void asrtest_function(switch_core_session_t *session, char *data)
{
	switch_asr_handle_t ah = { 0 };
	switch_asr_flag_t flags = SWITCH_ASR_FLAG_NONE;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *codec_name = "L16";
	switch_codec_t codec = { 0 }, *read_codec;
	switch_frame_t write_frame = { 0 }, *write_frame_p = NULL;
	char xdata[1024] = "";

	read_codec = switch_core_session_get_read_codec(session);
	assert(read_codec != NULL);


	if (switch_core_asr_open(&ah, "lumenvox",
							 read_codec->implementation->iananame, 8000, "127.0.0.1", &flags,
							 switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		if (strcmp(ah.codec, read_codec->implementation->iananame)) {
			if (switch_core_codec_init(&codec,
									   ah.codec,
									   NULL,
									   ah.rate,
									   read_codec->implementation->microseconds_per_frame / 1000,
									   read_codec->implementation->number_of_channels,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
				switch_core_session_set_read_codec(session, &codec);
				write_frame.data = xdata;
				write_frame.buflen = sizeof(xdata);
				write_frame.codec = &codec;
				write_frame_p = &write_frame;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "Codec Activation Failed %s@%uhz %u channels %dms\n", codec_name,
								  read_codec->implementation->samples_per_second,
								  read_codec->implementation->number_of_channels, read_codec->implementation->microseconds_per_frame / 1000);
				switch_core_session_reset(session);
				return;
			}
		}


		if (switch_core_asr_load_grammar(&ah, "demo", "/opt/lumenvox/engine_7.0/Lang/BuiltinGrammars/ABNFPhone.gram") != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error loading Grammar\n");
			goto end;
		}

		while (switch_channel_ready(channel)) {
			switch_frame_t *read_frame;
			switch_status_t status = switch_core_session_read_frame(session, &read_frame, -1, 0);
			char *xmlstr = NULL;
			switch_xml_t xml = NULL, result;

			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}

			if (switch_test_flag(read_frame, SFF_CNG)) {
				continue;
			}

			if (switch_core_asr_feed(&ah, read_frame->data, read_frame->datalen, &flags) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error Feeding Data\n");
				break;
			}

			if (switch_core_asr_check_results(&ah, &flags) == SWITCH_STATUS_SUCCESS) {
				if (switch_core_asr_get_results(&ah, &xmlstr, &flags) != SWITCH_STATUS_SUCCESS) {
					break;
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RAW XML\n========\n%s\n", xmlstr);

				if ((xml = switch_xml_parse_str(xmlstr, strlen(xmlstr))) && (result = switch_xml_child(xml, "result"))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Results [%s]\n", result->txt);
					switch_xml_free(xml);
				}
				switch_safe_free(xmlstr);
			}

			if (write_frame_p) {
				write_frame.datalen = read_frame->datalen;
				switch_core_session_write_frame(session, write_frame_p, -1, 0);
			} else {
				memset(read_frame->data, 0, read_frame->datalen);
				switch_core_session_write_frame(session, read_frame, -1, 0);
			}
		}

	  end:
		if (write_frame_p) {
			switch_core_session_set_read_codec(session, read_codec);
			switch_core_codec_destroy(&codec);
		}

		switch_core_asr_close(&ah, &flags);
		switch_core_session_reset(session);
	}

}

#endif

static void ivrtest_function(switch_core_session_t *session, char *data)
{
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_codec_t *codec;
	char buf[10] = "";
	char term;
	char say[128] = "";

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_channel_answer(channel);

	codec = switch_core_session_get_read_codec(session);

	while (switch_channel_get_state(channel) == CS_EXECUTE) {
		memset(buf, 0, sizeof(buf));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Enter up to 10 digits, press # to terminate, * to hangup\n");

		if (data) {
			switch_input_args_t args = { 0 };
			/* you could have passed NULL instead of on_dtmf to get this exact behaviour (copy the digits to buf and stop playing)
			   but you may want to pass the function if you have something cooler to do...
			 */
			args.input_callback = on_dtmf;
			args.buf = buf;
			args.buflen = sizeof(buf);
			status = switch_ivr_play_file(session, NULL, data, &args);

			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				break;
			}
		}

		if (switch_ivr_collect_digits_count(session, buf, sizeof(buf), 10, "#*", &term, 10000) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			break;
		}

		if (term && term == '*') {
			break;
		}
		snprintf(say, sizeof(say), "You Dialed [%s]\n", buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, say);
		switch_ivr_speak_text(session, "cepstral", "david", say, NULL);
	}

}


static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "I globally hooked to [%s] on the hangup event\n", switch_channel_get_name(channel));
	return SWITCH_STATUS_SUCCESS;

}

static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};



static switch_application_interface_t bug_application_interface = {
	/*.interface_name */ "bugtest",
	/*.application_function */ bugtest_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ NULL
};

static switch_application_interface_t ivr_application_interface = {
	/*.interface_name */ "ivrmenu",
	/*.application_function */ ivr_application_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ &bug_application_interface
};

static switch_application_interface_t xml_application_interface = {
	/*.interface_name */ "xml",
	/*.application_function */ xml_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ &ivr_application_interface
};

static switch_application_interface_t disast_application_interface = {
	/*.interface_name */ "disast",
	/*.application_function */ disast_function,
	NULL, NULL, NULL,
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &xml_application_interface
};

static switch_application_interface_t tts_application_interface = {
	/*.interface_name */ "tts",
	/*.application_function */ tts_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ &disast_application_interface
};

static switch_application_interface_t dirtest_application_interface = {
	/*.interface_name */ "dirtest",
	/*.application_function */ dirtest_function,
	NULL, NULL, NULL,
	/* flags */ SAF_SUPPORT_NOMEDIA,
	/*.next */ &tts_application_interface
};

static switch_application_interface_t ivrtest_application_interface = {
	/*.interface_name */ "ivrtest",
	/*.application_function */ ivrtest_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ &dirtest_application_interface
};

static switch_application_interface_t asrtest_application_interface = {
	/*.interface_name */ "asrtest",
	/*.application_function */ asrtest_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/*.next */ &ivrtest_application_interface
};

static switch_loadable_module_interface_t ivrtest_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &asrtest_application_interface
};

SWITCH_MODULE_LOAD_FUNCTION(mod_ivrtest_load)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &ivrtest_module_interface;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
