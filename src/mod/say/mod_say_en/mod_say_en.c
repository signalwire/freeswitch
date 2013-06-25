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
 * Michael B. Murdock <mike@mmurdock.org>
 *
 * mod_say_en.c -- Say for English
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_say_en_load);
SWITCH_MODULE_DEFINITION(mod_say_en, mod_say_en_load, NULL, NULL);


#define say_num(_sh, num, meth) {										\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = meth;			\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus =													\
			 en_say_general_count(_sh, tmp, say_args))					\
			!= SWITCH_STATUS_SUCCESS) {									\
			return tstatus;												\
		}																\
		say_args->method = smeth; say_args->type = stype;				\
	}																	\



static switch_status_t play_group(switch_say_method_t method, int a, int b, int c, char *what, switch_say_file_handle_t *sh)
{

	if (a) {
		switch_say_file(sh, "digits/%d", a);
		switch_say_file(sh, "digits/hundred");
	}

	if (b) {
		if (b > 1) {
			if ((c == 0) && (method == SSM_COUNTED)) {
				switch_say_file(sh, "digits/h-%d0", b);
			} else {
				switch_say_file(sh, "digits/%d0", b);
			}
		} else {
			switch_say_file(sh, "digits/%d%d", b, c);
			c = 0;
		}
	}

	if (c) {
		if (method == SSM_COUNTED) {
			switch_say_file(sh, "digits/h-%d", c);
		} else {
			switch_say_file(sh, "digits/%d", c);
		}
	}

	if (what && (a || b || c)) {
		switch_say_file(sh, what);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t en_say_general_count(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[128] = "";
	switch_status_t status;

	if (say_args->method == SSM_ITERATED) {
		if ((tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)-1))) {
			char *p;
			for (p = tosay; p && *p; p++) {
				switch_say_file(sh, "digits/%c", *p);
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
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}
		

		switch (say_args->method) {
		case SSM_PRONOUNCED_YEAR:
			{
				int num = atoi(tosay);
				int a = num / 100;
				int b = num % 100;

				if (!b || !(a % 10)) {
					say_num(sh, num, SSM_PRONOUNCED);
					return SWITCH_STATUS_SUCCESS;
				}

				say_num(sh, a, SSM_PRONOUNCED);
				say_num(sh, b, SSM_PRONOUNCED);

				return SWITCH_STATUS_SUCCESS;
			}
			break;
		case SSM_COUNTED:
		case SSM_PRONOUNCED:
			if ((status = play_group(SSM_PRONOUNCED, places[8], places[7], places[6], "digits/million", sh)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(SSM_PRONOUNCED, places[5], places[4], places[3], "digits/thousand", sh)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(say_args->method, places[2], places[1], places[0], NULL, sh)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			break;
		default:
			break;
		}
	} else {
		switch_say_file(sh, "digits/0");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t en_say_time(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
{
	int32_t t;
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	const char *tz = NULL;

	tz = switch_say_file_handle_get_variable(sh, "timezone");		

	if (say_args->type == SST_TIME_MEASUREMENT) {
		int64_t hours = 0;
		int64_t minutes = 0;
		int64_t seconds = 0;
		int64_t r = 0;

		if (strchr(tosay, ':')) {
			char *tme = strdup(tosay);
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
			free(tme);
		} else {
			if ((seconds = atol(tosay)) <= 0) {
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
			say_num(sh, hours, SSM_PRONOUNCED);
			if (hours == 1) {
				switch_say_file(sh, "time/hour");
			} else {
				switch_say_file(sh, "time/hours");
			}
		} else {
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "time/hours");
		}

		if (minutes) {
			say_num(sh, minutes, SSM_PRONOUNCED);
			if (minutes == 1) {
				switch_say_file(sh, "time/minute");
			} else {
				switch_say_file(sh, "time/minutes");
			}
		} else {
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "time/minutes");
		}

		if (seconds) {
			say_num(sh, seconds, SSM_PRONOUNCED);
			if (seconds == 1) {
				switch_say_file(sh, "time/second");
			} else {
				switch_say_file(sh, "time/seconds");
			}
		} else {
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "time/seconds");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if ((t = atol(tosay)) > 0) {
		target = switch_time_make(t, 0);
		target_now = switch_micro_time_now();
	} else {
		target = switch_micro_time_now();
		target_now = switch_micro_time_now();
	}

	if (tz) {
		int check = atoi(tz);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timezone is [%s]\n", tz);
		if (check) {
			switch_time_exp_tz(&tm, target, check);
			switch_time_exp_tz(&tm_now, target_now, check);
		} else {
			switch_time_exp_tz_name(tz, &tm, target);
			switch_time_exp_tz_name(tz, &tm_now, target_now);
		}
	} else {
		switch_time_exp_lt(&tm, target);
		switch_time_exp_lt(&tm_now, target_now);
	}

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
	case SST_SHORT_DATE_TIME:
		say_time = 1;
		if (tm.tm_year != tm_now.tm_year) {
			say_date = 1;
			break;
		}
		if (tm.tm_yday == tm_now.tm_yday) {
			say_today = 1;
			break;
		}
		if (tm.tm_yday == tm_now.tm_yday - 1) {
			say_yesterday = 1;
			break;
		}
		if (tm.tm_yday >= tm_now.tm_yday - 5) {
			say_dow = 1;
			break;
		}
		if (tm.tm_mon != tm_now.tm_mon) {
			say_month = say_day = say_dow = 1;
			break;
		}

		say_month = say_day = say_dow = 1;

		break;
	default:
		break;
	}

	if (say_today) {
		switch_say_file(sh, "time/today");
	}
	if (say_yesterday) {
		switch_say_file(sh, "time/yesterday");
	}
	if (say_dow) {
		switch_say_file(sh, "time/day-%d", tm.tm_wday);
	}

	if (say_date) {
		say_year = say_month = say_day = say_dow = 1;
		say_today = say_yesterday = 0;
	}

	if (say_month) {
		switch_say_file(sh, "time/mon-%d", tm.tm_mon);
	}
	if (say_day) {
		say_num(sh, tm.tm_mday, SSM_COUNTED);
	}
	if (say_year) {
		say_num(sh, tm.tm_year + 1900, SSM_PRONOUNCED_YEAR);
	}

	if (say_time) {
		int32_t hour = tm.tm_hour, pm = 0;

		if (say_date || say_today || say_yesterday || say_dow) {
			switch_say_file(sh, "time/at");
		}

		if (hour > 12) {
			hour -= 12;
			pm = 1;
		} else if (hour == 12) {
			pm = 1;
		} else if (hour == 0) {
			hour = 12;
			pm = 0;
		}

		say_num(sh, hour, SSM_PRONOUNCED);

		if (tm.tm_min > 9) {
			say_num(sh, tm.tm_min, SSM_PRONOUNCED);
		} else if (tm.tm_min) {
			switch_say_file(sh, "time/oh");
			say_num(sh, tm.tm_min, SSM_PRONOUNCED);
		} else {
			switch_say_file(sh, "time/oclock");
		}

		switch_say_file(sh, "time/%s", pm ? "p-m" : "a-m");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t en_say_money(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
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
		switch_say_file(sh, "currency/negative");
		dollars++;
	}

	/* Say dollar amount */
	en_say_general_count(sh, dollars, say_args);
	if (atoi(dollars) == 1) {
		switch_say_file(sh, "currency/dollar");
	} else {
		switch_say_file(sh, "currency/dollars");
	}

	/* Say cents */
	if (cents) {
		/* Say "and" */
		switch_say_file(sh, "currency/and");

		en_say_general_count(sh, cents, say_args);
		if (atoi(cents) == 1) {
			switch_say_file(sh, "currency/cent");
		} else {
			switch_say_file(sh, "currency/cents");
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t say_ip(switch_say_file_handle_t *sh,
							  char *tosay,
							  switch_say_args_t *say_args)
	
{
	char *a, *b, *c, *d;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	if (!(a = strdup(tosay))) {
		abort();
	}

	if (!(b = strchr(a, '.'))) {
		goto end;
	}

	*b++ = '\0';

	if (!(c = strchr(b, '.'))) {
		goto end;
	}

	*c++ = '\0';

	if (!(d = strchr(c, '.'))) {
		goto end;
	}

	*d++ = '\0';

	say_num(sh, atoi(a), say_args->method);
	switch_say_file(sh, "digits/dot");
	say_num(sh, atoi(b), say_args->method);
	switch_say_file(sh, "digits/dot");
	say_num(sh, atoi(c), say_args->method);
	switch_say_file(sh, "digits/dot");
	say_num(sh, atoi(d), say_args->method);

 end:
	
	free(a);

	return status;
}


static switch_status_t say_spell(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (a >= '0' && a <= '9') {
			switch_say_file(sh, "digits/%c", a);
		} else {
			if (say_args->type == SST_NAME_SPELLED) {
				switch_say_file(sh, "ascii/%d", a);
			} else if (say_args->type == SST_NAME_PHONETIC) {
				switch_say_file(sh, "phonetic-ascii/%d", a);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_new_say_callback_t choose_callback(switch_say_args_t *say_args)
{
	switch_new_say_callback_t say_cb = NULL;

	switch (say_args->type) {
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
	case SST_SHORT_DATE_TIME:
		say_cb = en_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = say_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = say_spell;
		break;
	case SST_CURRENCY:
		say_cb = en_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", say_args->type);
		break;
	}

	return say_cb;
}


static switch_status_t run_callback(switch_new_say_callback_t say_cb, char *tosay, switch_say_args_t *say_args, switch_core_session_t *session, char **rstr)
{
	switch_say_file_handle_t *sh;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *var_event = NULL;
	
	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_get_variables(channel, &var_event);
	}

	switch_say_file_handle_create(&sh, say_args->ext, &var_event);
	
	status = say_cb(sh, tosay, say_args);

	if ((*rstr = switch_say_file_handle_detach_path(sh))) {
		status = SWITCH_STATUS_SUCCESS;
	}

	switch_say_file_handle_destroy(&sh);

	return status;
}


static switch_status_t en_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{

	switch_new_say_callback_t say_cb = NULL;
	char *string = NULL;

	switch_status_t status = SWITCH_STATUS_FALSE;

	say_cb = choose_callback(say_args);

	if (say_cb) {
		status = run_callback(say_cb, tosay, say_args, session, &string);
		if (session && string) {
			status = switch_ivr_play_file(session, NULL, string, args);
		}
		
		switch_safe_free(string);
	}

	return status;
}


static switch_status_t en_say_string(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, char **rstr)
{

	switch_new_say_callback_t say_cb = NULL;
	char *string = NULL;

	switch_status_t status = SWITCH_STATUS_FALSE;

	say_cb = choose_callback(say_args);

	if (say_cb) {
		status = run_callback(say_cb, tosay, say_args, session, &string);
		if (string) {
			status = SWITCH_STATUS_SUCCESS;
			*rstr = string;
		}
	}

	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_say_en_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "en";
	say_interface->say_function = en_say;
	say_interface->say_string_function = en_say_string;
	
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
