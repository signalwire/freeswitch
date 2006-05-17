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


static const char modname[] = "mod_ivrtest";


/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, char *dtmf, void *buf, unsigned int buflen)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);
	
	switch_copy_string((char *)buf, dtmf, buflen);
	return SWITCH_STATUS_BREAK;

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
							  "%s, %s: %s\n", switch_xml_child(driver, "name")->txt, teamname,
							  switch_xml_child(driver, "points")->txt);
		}
	}
	switch_xml_free(f1);
}


static void dirtest_function(switch_core_session_t *session, char *data)
{
	char *var, *val;
	switch_channel_t *channel;
	switch_directory_handle_t dh;

	channel = switch_core_session_get_channel(session);
    assert(channel != NULL);


	if (switch_core_directory_open(&dh, 
								   "ldap",
								   "ldap.freeswitch.org",
								   "cn=Manager,dc=freeswitch,dc=org",
								   "test",
								   NULL) != SWITCH_STATUS_SUCCESS) {
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

static switch_status_t show_dtmf(switch_core_session_t *session, char *dtmf, void *buf, unsigned int buflen)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Digits %s\n", dtmf);
	
	switch_copy_string((char *)buf, dtmf, buflen);
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

	if(!(mydata = switch_core_session_strdup(session, (char *) data))) {
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

	switch_ivr_speak_text(session, tts_name, voice_name, NULL, codec->implementation->samples_per_second, show_dtmf, text, buf, sizeof(buf));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Done\n");
}

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
			/* you could have passed NULL instead of on_dtmf to get this exact behaviour (copy the digits to buf and stop playing)
			   but you may want to pass the function if you have something cooler to do...
			*/
			status = switch_ivr_play_file(session, NULL, data, NULL, on_dtmf, buf, sizeof(buf));
			
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				break;
			}
		}

		if (switch_ivr_collect_digits_count(session, buf, sizeof(buf), 10, "#*", &term, 10000, 1) != SWITCH_STATUS_SUCCESS) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			break;
		}

		if (term && term == '*') {
			break;
		}
		snprintf(say, sizeof(say), "You Dialed [%s]\n", buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, say);
		switch_ivr_speak_text(session, "cepstral", "david", NULL, codec->implementation->samples_per_second, NULL, say, NULL, 0);
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

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};


static const switch_application_interface_t xml_application_interface = {
	/*.interface_name */ "xml",
	/*.application_function */ xml_function,
	NULL, NULL, NULL,
	/*.next*/ NULL
};

static const switch_application_interface_t disast_application_interface = {
	/*.interface_name */ "disast",
	/*.application_function */ disast_function,
	NULL, NULL, NULL,
	/*.next*/ &xml_application_interface
};

static const switch_application_interface_t tts_application_interface = {
	/*.interface_name */ "tts",
	/*.application_function */ tts_function,
	NULL, NULL, NULL,
	/*.next*/ &disast_application_interface
};

static const switch_application_interface_t dirtest_application_interface = {
	/*.interface_name */ "dirtest",
	/*.application_function */ dirtest_function,
	NULL, NULL, NULL,
	/*.next*/ &tts_application_interface
};

static const switch_application_interface_t ivrtest_application_interface = {
	/*.interface_name */ "ivrtest",
	/*.application_function */ ivrtest_function,
	NULL, NULL, NULL,
	/*.next*/ &dirtest_application_interface
};

static const switch_loadable_module_interface_t mod_ivrtest_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ NULL,
	/*.codec_interface = */ NULL,
	/*.application_interface */ &ivrtest_application_interface
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_ivrtest_module_interface;

	/* test global state handlers */
	switch_core_add_state_handler(&state_handlers);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* 'switch_module_runtime' will start up in a thread by itself just by having it exist 
   if it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
*/


//switch_status_t switch_module_runtime(void)
