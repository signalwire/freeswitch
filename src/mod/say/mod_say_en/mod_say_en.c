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
 * mod_say_en.c -- Say for English
 *
 */
#include <switch.h>
#include <math.h>

static const char modname[] = "mod_say_en";

static switch_status_t en_say(switch_core_session_t *session,
							  char *tosay,
							  switch_say_type_t type,
							  switch_say_method_t method,
							  switch_input_callback_function_t input_callback,
							  void *buf,
							  uint32_t buflen)
{
	switch_channel_t *channel;

	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	switch(type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		{
			int in;
			int x, places[7] = {0};
			char tmp[25];

			in = atoi(tosay);

			for(x = 6; x >= 0; x--) {
				int num = pow(10, x);
				if ((places[x] = in / num)) {
					in -= places[x] * num;
				}
			}

			switch (method) {
			case SSM_PRONOUNCED:
				if (places[6]) {
					snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[6]);
					switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					switch_ivr_play_file(session, NULL, "digits/million.wav", NULL, input_callback, buf, buflen);
				}

				if (places[5]) {
					snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[5]);
					switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					switch_ivr_play_file(session, NULL, "digits/hundred.wav", NULL, input_callback, buf, buflen);
				}
			
				if (places[4]) {
					if (places[4] > 1) {
						snprintf(tmp, sizeof(tmp), "digits/%d0.wav", places[4]);
						switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					} else {
						snprintf(tmp, sizeof(tmp), "digits/%d%d.wav", places[4], places[3]);
						switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
						places[3] = 0;
					}
				}
			
				if (places[3]) {
					snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[3]);
					switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
				}

				if (places[4] || places[3]) {
					switch_ivr_play_file(session, NULL, "digits/thousand.wav", NULL, input_callback, buf, buflen);
				}

				if (places[2]) {
					snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[2]);
					switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					switch_ivr_play_file(session, NULL, "digits/hundred.wav", NULL, input_callback, buf, buflen);
				}

				if (places[1]) {
					if (places[1] > 1) {
						snprintf(tmp, sizeof(tmp), "digits/%d0.wav", places[1]);
						switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					} else {
						snprintf(tmp, sizeof(tmp), "digits/%d%d.wav", places[1], places[0]);
						switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
						places[0] = 0;
					}
				}

				if (places[0]) {
					snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[0]);
					switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
				}

				break;
			case SSM_ITERATED:
				for(x = 7; x >= 0; x--) {
					if (places[x]) {
						snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[x]);
						switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Finish ME!\n");
		break;
	}



	return SWITCH_STATUS_SUCCESS;
}

static const switch_chat_interface_t en_say_interface= {
	/*.name */ "en",
	/*.say_function */ en_say,
};

static switch_loadable_module_interface_t say_en_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL,
	/*.chat_interface */ NULL,
	/*.say_inteface*/ &en_say_interface,
	/*.asr_interface*/ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &say_en_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

