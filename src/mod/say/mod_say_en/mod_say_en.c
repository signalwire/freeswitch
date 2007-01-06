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
 * Michael B. Murdock <mike@mmurdock.org>
 *
 * mod_say_en.c -- Say for English
 *
 */
#include <switch.h>
#include <math.h>
#include <ctype.h>

static const char modname[] = "mod_say_en";


#define say_num(num, t) {							\
		char tmp[80];\
		switch_status_t status;\
		snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);				\
	if ((status = en_say_general_count(session, tmp, SST_ITEMS, t, args)) != SWITCH_STATUS_SUCCESS) {\
		return status;\
	}}\

#define say_file(...) {\
		char tmp[80];\
		snprintf(tmp, sizeof(tmp), __VA_ARGS__);\
		switch_ivr_play_file(session, NULL, tmp, args); \
		if (!switch_channel_ready(switch_core_session_get_channel(session))) {\
			return SWITCH_STATUS_FALSE;\
		}}\


static switch_status_t en_spell(switch_core_session_t *session,
								char *tosay,
								switch_say_type_t type,
								switch_say_method_t method,
								switch_input_args_t *args)
{
	char *p;

	for(p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (type == SST_NAME_SPELLED) {
			say_file("ascii/%d.wav", a);
		} else if (type == SST_NAME_PHONETIC) {
			say_file("phonetic-ascii/%d.wav", a);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t play_group(switch_say_method_t method,
								  int a,
								  int b,
								  int c,
								  char *what,
								  switch_core_session_t *session,
								  switch_input_args_t *args)
{

	if (a) {
		say_file("digits/%d.wav", a);
		say_file("digits/hundred.wav");
	}

	if (b) {
		if (b > 1) {
			say_file("digits/%d0.wav", b);
		} else {
			say_file("digits/%d%d.wav", b, c);
			c = 0;
		}
	}

	if (c) {
		if (method == SSM_COUNTED) {
			say_file("digits/h-%d.wav", c);
		} else {
			say_file("digits/%d.wav", c);
		}
	}

	if (what && (a || b || c)) {
		say_file(what);
	}

	return SWITCH_STATUS_SUCCESS;
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

static char *strip_nonnumerics(char *in, char *out, switch_size_t len)
{
	char *p = in, *q = out;
	char *ret = out;
	switch_size_t x = 0;
	// valid are 0 - 9, period (.), minus (-), and plus (+) - remove all others
	for(;p && *p; p++) {
		if ((*p > 47 && *p < 58) || *p == '.' || *p == '-' || *p == '+') {
			*q++ = *p;
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
											switch_input_args_t *args)
{
	switch_channel_t *channel;
	int in;
	int x = 0, places[9] = {0};
	char sbuf[13] = "";
	switch_status_t status;
	
	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);

	if (in != 0) {
		for(x = 8; x >= 0; x--) {
			int num = (int)pow(10, x);
			if ((places[x] = in / num)) {
				in -= places[x] * num;
			}
		}
	
		switch (method) {
		case SSM_COUNTED:
		case SSM_PRONOUNCED:
			if ((status = play_group(SSM_PRONOUNCED, places[8], places[7], places[6], "digits/million.wav", session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(SSM_PRONOUNCED, places[5], places[4], places[3], "digits/thousand.wav", session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(method, places[2], places[1], places[0], NULL, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			break;
		case SSM_ITERATED:
			{
				char *p;
				for (p = tosay; p && *p; p++) {
					if (places[x] > -1) {
						say_file("digits/%c.wav", *p);
					}
				}
			}
			break;
		default:
			break;
		}
	}
	else {
		say_file("digits/0.wav");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t en_ip(switch_core_session_t *session,
								char *tosay,
								switch_say_type_t type,
								switch_say_method_t method,
								switch_input_args_t *args)
{
	char *a, *b, *c, *d;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	if (!(a = strdup(tosay))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(b = strchr(a, '.'))) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	*b++ = '\0';

	if (!(c = strchr(b, '.'))) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	*c++ = '\0';

	if (!(d = strchr(c, '.'))) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	*d++ = '\0';

	say_num(atoi(a), method);
	say_file("digits/dot.wav");
	say_num(atoi(b), method);
	say_file("digits/dot.wav");
	say_num(atoi(c), method);
	say_file("digits/dot.wav");
	say_num(atoi(d), method);

 done:
	switch_safe_free(a);
	return status;
}


static switch_status_t en_say_time(switch_core_session_t *session,
								   char *tosay,
								   switch_say_type_t type,
								   switch_say_method_t method,
								   switch_input_args_t *args)
{
	int32_t t;
	switch_time_t target = 0;
	switch_time_exp_t tm;
	uint8_t say_date = 0, say_time = 0;
	
	if (type == SST_TIME_MEASUREMENT) {
		int64_t hours = 0;
		int64_t minutes = 0;
		int64_t seconds = 0;
		int64_t r = 0;

		if (strchr(tosay, ':')) {
			char *tme = switch_core_session_strdup(session, tosay);
			char *p;
			
			if ((p = strrchr(tme, ':'))) {
				*p++ = '\0';
				seconds = atoi(p);
				if ((p = strrchr(tme, ':'))) {
					*p++ = '\0';
					minutes = atoi(p);
					if (tme) {
						hours = atoi(tme);
					}
				}
			}
		} else {
			if ((seconds = atoi(tosay)) <= 0) {
				seconds = (int64_t) time(NULL);
			}

			if (seconds >= 60) {
				minutes = seconds / 60;
				r = seconds % 60;
				seconds = r;
			}
			
			if (minutes >= 60) {
				hours = minutes / 60;
				r = minutes % 60;
				minutes = r;
			}
		}

		if (hours) {
			say_num(hours, SSM_PRONOUNCED);
			say_file("digits/hours.wav");
		}

		if (minutes) {
			say_num(minutes, SSM_PRONOUNCED);
			say_file("digits/minutes.wav");
		}

		if (seconds) {
			say_num(seconds, SSM_PRONOUNCED);
			say_file("digits/seconds.wav");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if ((t = atoi(tosay)) > 0) {
		target = switch_time_make(t, 0);
	} else {
		target = switch_time_now();
	}
	switch_time_exp_lt(&tm, target);
	
	switch(type) {
	case SST_CURRENT_DATE_TIME:
		say_date = say_time = 1;
		break;
	case SST_CURRENT_DATE:
		say_date = 1;
		break;
	case SST_CURRENT_TIME:
		say_time = 1;
		break;
	default:
		break;
	}

	if (say_date) {
		say_file("digits/day-%d.wav", tm.tm_wday);
		say_file("digits/mon-%d.wav", tm.tm_mon);
		say_num(tm.tm_mday, SSM_COUNTED);
		say_num(tm.tm_year + 1900, SSM_PRONOUNCED);
	}

	if (say_time) {
		int32_t hour = tm.tm_hour, pm = 0;
		
		if (hour > 12) {
			hour -= 12;
            pm = 1;
		} else if (hour == 12) {
			pm = 1;
		} else if (hour == 0) {
			hour = 12;
			pm = 0;
		}

		say_num(hour, SSM_PRONOUNCED);

		if (tm.tm_min > 9) {
			say_num(tm.tm_min, SSM_PRONOUNCED);
		} else if (tm.tm_min) {
			say_file("digits/oh.wav");
			say_num(tm.tm_min, SSM_PRONOUNCED);
		} else {
			say_file("digits/oclock.wav");
		}

		say_file("digits/%s.wav", pm ? "p-m" : "a-m");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t en_say_money(switch_core_session_t *session,
											char *tosay,
											switch_say_type_t type,
											switch_say_method_t method,
											switch_input_args_t *args)
{
	switch_channel_t *channel;
		
	char sbuf[16] = ""; /* enuough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *dollars = NULL;
	char *cents = NULL;
		
	assert(session != NULL);
	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
			
	if (strlen(tosay) > 15 || !(tosay = strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	dollars = sbuf;

	if ((cents = strchr(sbuf, '.'))) {
		*cents++ = '\0';
	}

    /* If positive sign - skip over" */
    if (sbuf[0] == '+') {
        dollars++;
    }

	/* If negative say "negative" */
	if (sbuf[0] == '-') {
		say_file("negative.wav");
		dollars++;
	}
			
	/* Say dollar amount */
	en_say_general_count(session, dollars, type, method, args);
	if (atoi(dollars) == 1) {
		say_file("dollar.wav");
	}
	else {
		say_file("dollars.wav");
	}
		
	/* Say "and" */
	say_file("and.wav");
	
    /* Say cents */
    if (cents) {
        en_say_general_count(session, cents, type, method, args);
        if (atoi(cents) == 1) {
            say_file("cent.wav");
        }
        else {
            say_file("cents.wav");
        }
    }
    else {
        say_file("digits/0.wav");
        say_file("cents.wav");
    }
	
	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t en_say(switch_core_session_t *session,
							  char *tosay,
							  switch_say_type_t type,
							  switch_say_method_t method,
							  switch_input_args_t *args)
{
	
	switch_say_callback_t say_cb = NULL;

	switch(type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = en_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
		say_cb = en_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = en_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = en_spell;
		break;
	case SST_CURRENCY:
		say_cb = en_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", type);
		break;
	}
	
	if (say_cb) {
		say_cb(session, tosay, type, method, args);
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
