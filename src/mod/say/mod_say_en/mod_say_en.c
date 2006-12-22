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

typedef struct {
	switch_core_session_t *session;
	switch_input_callback_function_t input_callback;
	void *buf;
	uint32_t buflen;
} common_args_t;

static void play_group(int a, int b, int c, char *what, common_args_t *args)
{
	char tmp[80] = "";

	if (a) {
		snprintf(tmp, sizeof(tmp), "digits/%d.wav", a);
		switch_ivr_play_file(args->session, NULL, tmp, NULL, args->input_callback, args->buf, args->buflen);
		switch_ivr_play_file(args->session, NULL, "digits/hundred.wav", NULL, args->input_callback, args->buf, args->buflen);
	}

	if (b) {
		if (b > 1) {
			snprintf(tmp, sizeof(tmp), "digits/%d0.wav", b);
			switch_ivr_play_file(args->session, NULL, tmp, NULL, args->input_callback, args->buf, args->buflen);
		} else {
			snprintf(tmp, sizeof(tmp), "digits/%d%d.wav", b, c);
			switch_ivr_play_file(args->session, NULL, tmp, NULL, args->input_callback, args->buf, args->buflen);
			c = 0;
		}
	}

	if (c) {
		snprintf(tmp, sizeof(tmp), "digits/%d.wav", c);
		switch_ivr_play_file(args->session, NULL, tmp, NULL, args->input_callback, args->buf, args->buflen);
	}

	if (what && (a || b || c)) {
		switch_ivr_play_file(args->session, NULL, what, NULL, args->input_callback, args->buf, args->buflen);
	}

}
					   
static char *strip_commas(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;
	
	for(;p && *p; p++) {
		if ((*p > 47 && *p < 58)) {
			*q++ = *p;
		} else if (*p != ',') {
			ret = NULL;
			break;
		}

		if (++x > len) {
			ret = NULL;
			break;
		}
	}

	return ret;
}

static switch_status_t en_say_general_count(switch_core_session_t *session,
											char *tosay,
											switch_say_type_t type,
											switch_say_method_t method,
											switch_input_callback_function_t input_callback,
											void *buf,
											uint32_t buflen)
{
	switch_channel_t *channel;
	int in;
	int x = 0, places[9] = {0};
	char tmp[80] = "";
	char sbuf[13] = "";
	common_args_t args = {0};

	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	args.session = session;
	args.input_callback = input_callback;
	args.buf = buf;
	args.buflen = buflen;
	
	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);
	
	for(x = 8; x >= 0; x--) {
		int num = (int)pow(10, x);
		if ((places[x] = in / num)) {
			in -= places[x] * num;
		}
	}

	switch (method) {
	case SSM_PRONOUNCED:
		play_group(places[8], places[7], places[6], "digits/million.wav", &args);
		play_group(places[5], places[4], places[3], "digits/thousand.wav", &args);
		play_group(places[2], places[1], places[0], NULL, &args);
		break;
	case SSM_ITERATED:
		for(x = 8; x >= 0; x--) {
			if (places[x]) {
				snprintf(tmp, sizeof(tmp), "digits/%d.wav", places[x]);
				switch_ivr_play_file(session, NULL, tmp, NULL, input_callback, buf, buflen);
			}
		}
		break;
	default:
		break;
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t en_say(switch_core_session_t *session,
							  char *tosay,
							  switch_say_type_t type,
							  switch_say_method_t method,
							  switch_input_callback_function_t input_callback,
							  void *buf,
							  uint32_t buflen)
{
	
	switch_say_callback_t say_cb = NULL;

	switch(type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = en_say_general_count;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Finish ME!\n");
		break;
	}
	
	if (say_cb) {
		say_cb(session, tosay, type, method, input_callback, buf, buflen);
	} 

	return SWITCH_STATUS_SUCCESS;
}

static const switch_say_interface_t en_say_interface= {
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

