/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * PeteDao <petekay@gmail.com>
 * Steve Underwood 0.0.1 <steveu@coppice.org>
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_say_zh.c -- Say for Mandarin, Cantonese, and probably any other Chinese
 *                 dialect.
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

/*  SSM_COUNTED means ordinal number
	SSM_PRONOUNCED
    SSM_ITERATED */

SWITCH_MODULE_LOAD_FUNCTION(mod_say_zh_load);
SWITCH_MODULE_DEFINITION(mod_say_zh, mod_say_zh_load, NULL, NULL);

#define say_num(num, meth) {											\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = meth;			\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus =													\
			 zh_say_general_count(session, tmp, say_args, args))		\
			!= SWITCH_STATUS_SUCCESS) {									\
			return tstatus;												\
		}																\
		say_args->method = smeth; say_args->type = stype;				\
	}																	\

#define say_file(...) {													\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);					\
		if ((tstatus =													\
			 switch_ivr_play_file(session, NULL, tmp, args))			\
			!= SWITCH_STATUS_SUCCESS){									\
			return tstatus;												\
		}																\
		if (!switch_channel_ready(switch_core_session_get_channel(session))) { \
			return SWITCH_STATUS_FALSE;									\
		}}																\

static switch_status_t zh_say_general_count(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int in;
	char sbuf[128] = "";
	char digits[11];
	int i;

	if (say_args->method == SSM_ITERATED) {
		if ((tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)-1))) {
			char *p;
			for (p = tosay; p && *p; p++) {
				say_file("digits/%c.wav", *p);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
			return SWITCH_STATUS_GENERR;
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)-1)) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);

	if (in != 0) {
		snprintf(digits, sizeof(digits), "%10.10d", in);
		switch (say_args->method) {
		case SSM_COUNTED:
			say_file("digits/ordinal.wav");
			/* Fall through */
		case SSM_PRONOUNCED:
			for (i = 0; i <= 9; i++) {
				switch (i) {
				case 0:
					/* Billions column */
					if (digits[0] != '0') {
						if (digits[0] != '1')
							say_file("digits/%c.wav", digits[i]);
						say_file("digits/10.wav");
					}
					break;
				case 1:
					/* Hundred millions column */
					if (digits[i] != '0')
						say_file("digits/%c.wav", digits[i]);
					if (memcmp(digits, "00", 2) != 0)
						say_file("digits/100000000.wav");
					break;
				case 5:
					/* Ten thousands column */
					if (digits[i] != '0')
						say_file("digits/%c.wav", digits[i]);
					if (memcmp(digits + 2, "0000", 4) != 0)
						say_file("digits/10000.wav");
					break;
				case 2:
				case 6:
					/* Ten millions or thousands column */
					if (digits[i] != '0') {
						say_file("digits/%c.wav", digits[i]);
						say_file("digits/1000.wav");
					}
					break;
				case 3:
				case 7:
					/* Millions or hundreds column */
					if (digits[i] != '0') {
						say_file("digits/%c.wav", digits[i]);
						say_file("digits/100.wav");
					} else {
						if (digits[i + 1] != '0'  &&  memcmp(digits, "00000000", i) != 0)
							say_file("digits/0.wav");
					}
					break;
				case 4:
				case 8:
					/* Hundred thousands or tens column */
					if (digits[i] != '0') {
						if (digits[i] != '1')
							say_file("digits/%c.wav", digits[i]);
						say_file("digits/10.wav");
					} else {
						if (digits[i + 1] != '0'  &&  memcmp(digits, "00000000", i) != 0)
							say_file("digits/0.wav");
					}
					break;
				case 9:
					/* Units column */
					if (digits[9] != '0')
						say_file("digits/%c.wav", digits[9]);
					break;
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

static switch_status_t zh_say_time(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int32_t t;
	switch_time_t target = 0;
	switch_time_exp_t tm;
#if 0
	switch_time_t this_morning;
	switch_time_exp_t tm2;
#endif
	uint8_t say_date = 0;
	uint8_t say_time = 0;

	if (say_args->type == SST_TIME_MEASUREMENT) {
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
				seconds = (int64_t) switch_epoch_time_now(NULL);
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
			if (hours == 2) {
				say_file("time/2s.wav");
			} else {
				say_num(hours, SSM_PRONOUNCED);
			}
			say_file("time/hours.wav");
		}
		if (minutes) {
			if (minutes == 2) {
				say_file("time/2s.wav");
			} else {
				say_num(minutes, SSM_PRONOUNCED);
			}
			say_file("time/minute.wav");
		} else {
			if (hours) {
				say_file("digits/0.wav");
				say_file("time/minute.wav");
			}
		}

		if (seconds) {
			if (seconds == 2) {
				say_file("time/2s.wav");
			} else {
				say_num(hours, SSM_PRONOUNCED);
			}
			say_file("time/seconds.wav");
		} else {
			if (hours || minutes) {
				say_file("digits/0.wav");
				say_file("time/second.wav");
			}
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if ((t = atoi(tosay)) > 0)
		target = switch_time_make(t, 0);
	else
		target = switch_micro_time_now();
	switch_time_exp_lt(&tm, target);

	switch (say_args->type) {
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
		say_num(tm.tm_year + 1900, SSM_ITERATED);
		say_file("time/year.wav");
		say_num(tm.tm_mon + 1, SSM_PRONOUNCED);
		say_file("time/month.wav");
		say_num(tm.tm_mday, SSM_PRONOUNCED);
		say_file("time/day.wav");
		say_file("time/day-%d.wav", tm.tm_wday);

#if 0
		tm = *localtime(&then);

		this_morning = switch_micro_time_now();
		switch_time_exp_lt(&tm2, this_morning);
		tm2->tm_hour = 0;
		tm2->tm_min = 0;
		tm2->tm_sec = 0;
		this_morning = mktime(tm2);

		if (this_morning <= then && then < (this_morning + 86400L)) {
			say_file("time/today.wav");
		} else if ((this_morning - 86400L) <= then && then < this_morning) {
			say_file("time/yesterday.wav");
		} else if ((this_morning + 86400L) <= then && then < (this_morning + 2 * 86400L)) {
			say_file("time/tomorrow.wav");
		} else if ((this_morning - 7 * 86400L) <= then && then < this_morning) {
			say_file("time/day-%d.wav", tm.tm_wday);
		} else {
			if (tm2->tm_year != tm.tm_year) {
				say_num(tm.tm_year + 1900, SSM_ITERATED);
				say_file("time/year.wav");
			}
			/*endif */
			if (tm2->tm_year != tm.tm_year || tm2->tm_mon != tm.tm_mon) {
				say_num(tm.tm_mon + 1, SSM_PRONOUNCED);
				say_file("time/month.wav");
			}
			/*endif */
			/* Always say the day and the day of the week */
			say_num(tm.tm_mday, SSM_PRONOUNCED);
			say_file("time/day.wav");
			say_file("time/day-%d.wav", tm.tm_wday);
		}
#endif
	}

	if (say_time) {
		int32_t hour = tm.tm_hour;

		if (hour < 6) {
			say_file("time/morning.wav");
		} else if (hour < 12) {
			say_file("time/am.wav");
		} else if (tm.tm_hour < 18) {
			say_file("time/pm.wav");
		} else {
			say_file("time/evening.wav");
		}
		if (hour > 12)
			hour -= 12;
		if (hour == 0)
			hour = 12;

		if (hour == 2) {
			say_file("time/2s.wav");
		} else {
			say_num(hour, SSM_PRONOUNCED);
		}
		say_file("time/hour.wav");
		if (tm.tm_min == 2) {
			say_file("time/2s.wav");
		} else {
			say_num(tm.tm_min, SSM_PRONOUNCED);
		}
		say_file("time/minute.wav");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t zh_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char sbuf[16] = "";			/* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *dollars = NULL;
	char *cents = NULL;

	if (strlen(tosay) > 15 || !(tosay = switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)-1))) {
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
	zh_say_general_count(session, dollars, say_args, args);
	say_file("currency/dollar.wav");

	/* Say cents */
	if (cents) {
		zh_say_general_count(session, cents, say_args, args);
	} else {
		say_file("digits/0.wav");
	}
	say_file("currency/cent.wav");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t zh_CN_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char sbuf[16] = "";			/* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char dbuf[16] = "";			/* enough for digits/x.wav */
	char *yuan = NULL;
	char *rest = NULL;
	int i;

	if (strlen(tosay) > 15 || !(tosay = switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)-1))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	yuan = sbuf;

	if ((rest = strchr(sbuf, '.'))) {
		*rest++ = '\0';
	}

	/* If positive sign - skip over" */
	if (sbuf[0] == '+') {
		yuan++;
	}

	/* If negative say "negative" */
	if (sbuf[0] == '-') {
		say_file("currency/negative.wav");
		yuan++;
	}

	/* Say dollar amount */
	zh_say_general_count(session, yuan, say_args, args);
	say_file("currency/yuan.wav");

	if (!rest) return SWITCH_STATUS_SUCCESS;

	/* Say cents */
	for (i=0; *rest; i++, rest++) {
	sprintf(dbuf, "digits/%c.wav", *rest);
		say_file(dbuf);
		if (i == 0) {
			say_file("currency/jiao.wav");
		} else if (i == 1) {
			say_file("currency/fen.wav");
		} else if (i == 2) {
			say_file("currency/li.wav");
		}  /* else just say the rest of digits */
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t zh_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	switch_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = zh_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
	case SST_SHORT_DATE_TIME:
		say_cb = zh_say_time;
		break;
	case SST_IP_ADDRESS:
		return switch_ivr_say_ip(session, tosay, zh_say_general_count, say_args, args);
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		return switch_ivr_say_spell(session, tosay, say_args, args);
		break;
	case SST_CURRENCY:
		say_cb = zh_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", say_args->type);
		break;
	}

	if (say_cb) {
		return say_cb(session, tosay, say_args, args);
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t zh_CN_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	switch_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = zh_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
		say_cb = zh_say_time;
		break;
	case SST_IP_ADDRESS:
		return switch_ivr_say_ip(session, tosay, zh_say_general_count, say_args, args);
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		return switch_ivr_say_spell(session, tosay, say_args, args);
		break;
	case SST_CURRENCY:
		say_cb = zh_CN_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", say_args->type);
		break;
	}

	if (say_cb) {
		return say_cb(session, tosay, say_args, args);
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_say_zh_load)
{
	switch_say_interface_t *say_interface;
	switch_say_interface_t *say_zh_CN_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "zh";
	say_interface->say_function = zh_say;

	say_zh_CN_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_zh_CN_interface->interface_name = "zh_CN";
	say_zh_CN_interface->say_function = zh_CN_say;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
