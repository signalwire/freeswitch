/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * mod_say_fr.c -- Say for English
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_say_fr_load);
SWITCH_MODULE_DEFINITION(mod_say_fr, mod_say_fr_load, NULL, NULL);

#define say_num(num, t) {							\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);				\
	if ((tstatus = fr_say_general_count(session, tmp, SST_ITEMS, t, args)) != SWITCH_STATUS_SUCCESS) {\
		return tstatus;\
	}}\

#define say_file(...) {\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);\
		if ((tstatus = switch_ivr_play_file(session, NULL, tmp, args)) != SWITCH_STATUS_SUCCESS){ \
			return tstatus;\
		}\
		if (!switch_channel_ready(switch_core_session_get_channel(session))) {\
			return SWITCH_STATUS_FALSE;\
		}}\


static switch_status_t fr_spell(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (a >= 48 && a <= 57) {
			say_file("digits/%d.wav", a - 48);
		} else {
			if (type == SST_NAME_SPELLED) {
				say_file("ascii/%d.wav", a);
			} else if (type == SST_NAME_PHONETIC) {
				say_file("phonetic-ascii/%d.wav", a);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t play_group(switch_say_method_t method, int a, int b, int c, char *what, switch_core_session_t *session, switch_input_args_t *args)
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

	for (; p && *p; p++) {
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
	for (; p && *p; p++) {
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

static switch_status_t fr_say_general_count(switch_core_session_t *session,
											char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	switch_status_t status;

	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);

	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t)x] = in / num)) {
				in -= places[(uint32_t)x] * num;
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
					say_file("digits/%c.wav", *p);
				}
			}
			break;
		default:
			break;
		}
	} else {
		say_file("digits/0.wav");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t fr_ip(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
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


static switch_status_t fr_say_time(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method,
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
				if ((p = strchr(tme, ':'))) {
					*p++ = '\0';
					minutes = atoi(p);
					if (tme) {
						hours = atoi(tme);
					}
				} else {
					minutes = atoi(tme);
				}
			}
		} else {
			if ((seconds = atoi(tosay)) <= 0) {
				seconds = (int64_t) switch_timestamp(NULL);
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
			if (hours == 1) {
				say_file("time/hour.wav");
			} else {
				say_file("time/hours.wav");
			}
		} else {
			say_file("digits/0.wav");
			say_file("time/hours.wav");
		}

		if (minutes) {
			say_num(minutes, SSM_PRONOUNCED);
			if (minutes == 1) {
				say_file("time/minute.wav");
			} else {
				say_file("time/minutes.wav");
			}
		} else {
			say_file("digits/0.wav");
			say_file("time/minutes.wav");
		}

		if (seconds) {
			say_num(seconds, SSM_PRONOUNCED);
			if (seconds == 1) {
				say_file("time/second.wav");
			} else {
				say_file("time/seconds.wav");
			}
		} else {
			say_file("digits/0.wav");
			say_file("time/seconds.wav");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if ((t = atoi(tosay)) > 0) {
		target = switch_time_make(t, 0);
	} else {
		target = switch_timestamp_now();
	}
	switch_time_exp_lt(&tm, target);

	switch (type) {
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
		say_file("time/day-%d.wav", tm.tm_wday);
		say_file("time/mon-%d.wav", tm.tm_mon);
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
			say_file("time/oh.wav");
			say_num(tm.tm_min, SSM_PRONOUNCED);
		} else {
			say_file("time/oclock.wav");
		}

		say_file("time/%s.wav", pm ? "p-m" : "a-m");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t fr_say_money(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method,
									switch_input_args_t *args)
{
	char sbuf[16] = "";			/* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *dollars = NULL;
	char *cents = NULL;

	if (strlen(tosay) > 15 || !(tosay = strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	dollars = sbuf;

	if ((cents = strchr(sbuf, '.'))) {
		*cents++ = '\0';
		if (strlen(cents) > 2) {
			cents[2] = '\0';
		}
	}

	/* If positive sign - skip over" */
	if (sbuf[0] == '+') {
		dollars++;
	}

	/* If negative say "negative" */
	if (sbuf[0] == '-') {
		say_file("currency/negative.wav");
		dollars++;
	}

	/* Say dollar amount */
	fr_say_general_count(session, dollars, type, method, args);
	if (atoi(dollars) == 1) {
		say_file("currency/dollar.wav");
	} else {
		say_file("currency/dollars.wav");
	}

	/* Say "and" */
	say_file("currency/and.wav");

	/* Say cents */
	if (cents) {
		fr_say_general_count(session, cents, type, method, args);
		if (atoi(cents) == 1) {
			say_file("currency/cent.wav");
		} else {
			say_file("currency/cents.wav");
		}
	} else {
		say_file("digits/0.wav");
		say_file("currency/cents.wav");
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t fr_say(switch_core_session_t *session, char *tosay, switch_say_type_t type, switch_say_method_t method, switch_input_args_t *args)
{

	switch_say_callback_t say_cb = NULL;

	switch (type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = fr_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
		say_cb = fr_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = fr_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = fr_spell;
		break;
	case SST_CURRENCY:
		say_cb = fr_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", type);
		break;
	}

	if (say_cb) {
		return say_cb(session, tosay, type, method, args);
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_say_fr_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "fr";
	say_interface->say_function = fr_say;

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
