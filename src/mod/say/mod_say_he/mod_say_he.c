/*
 * Copyright (c) 2011-2012, Shahar Hadas
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
 * The Initial Developers of the Original Code are
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael B. Murdock <mike@mmurdock.org>
 * Marc O. Chouinard <mochouinard@moctel.com>
 * Yehavi Bourvine <yehavi@savion.huji.ac.il>
 * Eli Hayun <elihay@savion.huji.ac.il>
 * Portions created by the Initial Developers are Copyright (C)
 * the Initial Developers. All Rights Reserved.
 *
 * Contributor(s):
 *
 * mod_say_he.c -- Say for Hebrew
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_say_he_load);
SWITCH_MODULE_DEFINITION(mod_say_he, mod_say_he_load, NULL, NULL);

#define say_num(_sh, num, meth) {											\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = meth;			\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus =													\
			 he_say_general_count(_sh, tmp, say_args))		\
			!= SWITCH_STATUS_SUCCESS) {									\
			return tstatus;												\
		}																\
		say_args->method = smeth; say_args->type = stype;				\
	}																	\

#define say_num_goto_status(_sh, num, meth, tag) {						\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_args_t tsay_args = *say_args;						\
		tsay_args.type = SST_ITEMS;										\
		tsay_args.method = meth;										\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus = he_say_general_count(_sh, tmp, &tsay_args)) !=	\
			SWITCH_STATUS_SUCCESS) {									\
			switch_goto_status(tstatus, tag);							\
		}																\
	}


typedef enum {
	PGR_HUNDREDS = 1000,
	PGR_THOUSANDS = 100000,
	PGR_MILLIONS = 100000000,
} play_group_range_t;

static switch_status_t play_group(switch_say_method_t method, switch_say_gender_t gender, int total, play_group_range_t range, int a, int b, int c, char *what, switch_say_file_handle_t *sh)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "** total=[%d]  range=[%d]  a=[%d]  b=[%d]  c=[%d]  gender=[%d]  method=[%d]  what=[%s]\n", total, range, a, b, c, gender, method, what);

	/* Check for special cases of thousands */
	if (range == PGR_THOUSANDS && a == 0) {
		/* Check for special cases of 1000, 2000 ... 8000, 9000 */
		if (b == 0 && c != 0) {
			switch_say_file(sh, "digits/%d000", c);
			return SWITCH_STATUS_SUCCESS;
		}

		/* Check for special case of 10,000 */
		if (b == 1 && c == 0) {
			switch_say_file(sh, "digits/10000");
			return SWITCH_STATUS_SUCCESS;
		}
	}

	/* Check for special case of million */
	if (range == PGR_MILLIONS && a == 0 && b == 0 && c == 1) {
		switch_say_file(sh, "digits/million");
		return SWITCH_STATUS_SUCCESS;
	}

	/* Check for Hebrew SSM_COUNTED special case. Anything above 10 needs to said differently */
	if (method == SSM_COUNTED && range == PGR_HUNDREDS && total <= 10) {
		if (b) {
			switch_say_file(sh, "digits/h-10%s", gender == SSG_MASCULINE ? "_m" : "");
		} else {
			switch_say_file(sh, "digits/h-%d%s", c, gender == SSG_MASCULINE ? "_m" : "");
		}
		return SWITCH_STATUS_SUCCESS;
	}

	/* In Hebrew, hundreds can be said as "<number> hundreds", but the more correct
	 * form of pronunciation required it to be recorded separately.
	 * Note that hundreds are always pronounced in SSG_FEMININE form in hebrew, and were recorded as such.
	 */
	if (a) {
		switch_say_file(sh, "digits/%d00", a);
	}

	if (b) {
		/* Check for two digits playback (10 to 19) */
		if (b > 1) {
			switch_say_file(sh, "digits/%d0", b);
		} else {
			if (range != PGR_HUNDREDS || gender == SSG_MASCULINE) {
				if ((range == PGR_MILLIONS && a) || (range != PGR_MILLIONS && total > 9)){ /* Check if need to say "and" */
					switch (c) {
					case 0:
					case 5:
						switch_say_file(sh, "digits/va");
						break;

					case 2:
					case 3:
					case 8:
					case 9:
						switch_say_file(sh, "digits/uu");
						break;

					case 1:
					case 4:
					case 6:
					case 7:
						switch_say_file(sh, "digits/ve");
						break;

					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "play_group unkonwn digit (%d) Error! (SSG_MASCULINE)\n", c);
						break;
					}
				}
				switch_say_file(sh, "digits/%d%d_m", b, c);
			} else {
				if ((range == PGR_MILLIONS && a) || (range != PGR_MILLIONS && total > 9)){ /* Check if need to say "and" */
					switch (c) {
					case 2:
					case 3:
					case 7:
					case 8:
					case 9:
						switch_say_file(sh, "digits/uu");
						break;

					case 0:
					case 1:
					case 4:
					case 5:
					case 6:
						switch_say_file(sh, "digits/ve");
						break;

					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "play_group unkonwn digit (%d) Error! (SSG_FEMININE)\n", c);
						break;
					}
				}
				switch_say_file(sh, "digits/%d%d", b, c);
			}
			c = 0;	/* Skip the pronunciation on the c value */
		}
	}

	if (c) {
		if (range != PGR_HUNDREDS || gender == SSG_MASCULINE) {
			if ((range == PGR_MILLIONS && (b || a)) || (range != PGR_MILLIONS && total > 9)){ /* Check if need to say "and" */
				switch (c) {
				case 5:
					switch_say_file(sh, "digits/va");
					break;

				case 2:
				case 3:
				case 8:
					switch_say_file(sh, "digits/uu");
					break;

				case 1:
				case 4:
				case 6:
				case 7:
				case 9:
					switch_say_file(sh, "digits/ve");
					break;

				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "play_group unkonwn digit (%d) Error! (SSG_MASCULINE)\n", c);
					break;
				}
			}
			switch_say_file(sh, "digits/%d_m", c);
		} else {
			if ((range == PGR_MILLIONS && (b || a)) || (range != PGR_MILLIONS && total > 9)){ /* Check if need to say "and" */
				switch (c) {
				case 2:
				case 8:
					switch_say_file(sh, "digits/uu");
					break;

				case 1:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 9:
					switch_say_file(sh, "digits/ve");
					break;

				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "play_group unkonwn digit (%d) Error! (SSG_FEMININE)\n", c);
					break;
				}
			}
			switch_say_file(sh, "digits/%d", c);
		}
	}

	if (what && (a || b || c)) {
		switch_say_file(sh, what);
	}

	/* Incase of SSM_COUNTED, in the case of total > 10, after the number was said, we need to say "be'mispar" (in number).
	 * Although we shouldn't be here if total <= 10, I still preferred to have the criteria tested again. */
	if (method == SSM_COUNTED && range == PGR_HUNDREDS && total > 10) {
		switch_say_file(sh, "digits/in_number", c);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t he_say_general_count(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
{
	int in, inCopy;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[128] = "";
	switch_status_t status;

	if (say_args->method == SSM_ITERATED) {
		if ((tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)-1))) {
			char *p;
			for (p = tosay; p && *p; p++) {
				if (*p != '0' && say_args->gender == SSG_MASCULINE) {
					switch_say_file(sh, "digits/%c_m", *p);
				} else {
					switch_say_file(sh, "digits/%c", *p);
				}
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

	in = inCopy = atoi(tosay);

	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}
		switch (say_args->method) {
		case SSM_COUNTED:
			/* TODO add 'ha' (the)? */
		case SSM_PRONOUNCED:
			if ((status = play_group(SSM_PRONOUNCED, say_args->gender, inCopy, PGR_MILLIONS, places[8], places[7], places[6], "digits/million", sh)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(SSM_PRONOUNCED, say_args->gender, inCopy, PGR_THOUSANDS, places[5], places[4], places[3], "digits/thousand", sh)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(say_args->method, say_args->gender, inCopy, PGR_HUNDREDS, places[2], places[1], places[0], NULL, sh)) != SWITCH_STATUS_SUCCESS) {
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

static switch_status_t he_say_time(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
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
					hours = atoi(tme);
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

		say_args->gender = SSG_FEMININE;

		if (hours) {
			switch_say_file(sh, "time/hour");
			say_num(sh, hours, SSM_PRONOUNCED);
		} else {
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "time/hours");
		}

		if (minutes) {
			if (minutes == 1) {
				switch_say_file(sh, "time/minute");
				say_num(sh, minutes, SSM_PRONOUNCED);
			} else {
				say_num(sh, minutes, SSM_PRONOUNCED);
				switch_say_file(sh, "time/minutes");
			}
		} else {
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "time/minutes");
		}

		if (seconds) {
			if (seconds == 1) {
				switch_say_file(sh, "time/second");
				say_num(sh, seconds, SSM_PRONOUNCED);
			} else {
				say_num(sh, seconds, SSM_PRONOUNCED);
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

	/* In Hebrew it's costumed to speak the day before the month. */
	if (say_day) {
		say_args->gender = SSG_MASCULINE;
		say_num(sh, tm.tm_mday, SSM_PRONOUNCED);
	}
	if (say_month) {
		switch_say_file(sh, "time/at");
		switch_say_file(sh, "time/mon-%d", tm.tm_mon);
	}
	if (say_year) {
		say_args->gender = SSG_FEMININE;
		say_num(sh, tm.tm_year + 1900, SSM_PRONOUNCED);
	}

	if (say_time) {
		int32_t hour = tm.tm_hour, pm = 0;

		if (say_date || say_today || say_yesterday || say_dow) {
			switch_say_file(sh, "time/at-hour");
		}
		else {
			switch_say_file(sh, "time/hour");
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

		say_args->gender = SSG_FEMININE;
		say_num(sh, hour, SSM_PRONOUNCED);

		if (tm.tm_min) {
			switch (tm.tm_min) {
			case 2:
			case 8:
			case 12:
			case 13:
			case 17:
			case 18:
			case 19:
			case 30:
				switch_say_file(sh, "digits/uu");
				break;

			case 50:
				switch_say_file(sh, "digits/va");
				break;

			default:
				switch_say_file(sh, "digits/ve");
				break;
			}

			if (tm.tm_min == 1) {
				switch_say_file(sh, "time/minute");
				switch_say_file(sh, "digits/1");
			} else {
				say_num(sh, tm.tm_min, SSM_PRONOUNCED);
				switch_say_file(sh, "time/minutes");
			}
		}

		switch_say_file(sh, "time/%s", pm ? "p-m" : "a-m");
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t he_say_money(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args)
{
	char sbuf[16] = "";			/* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *currency = NULL;
	char *cents = NULL;
	int icents = 0;

	if (strlen(tosay) > 15 || !switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)-1)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	currency = sbuf;

	if ((cents = strchr(sbuf, '.'))) {
		*cents++ = '\0';
		if (strlen(cents) > 2) {
			cents[2] = '\0';
		}
	}

	/* If positive sign - skip over" */
	if (sbuf[0] == '+') {
		++currency;
	}

	/* If negative say "negative" */
	if (sbuf[0] == '-') {
		switch_say_file(sh, "currency/negative");
		++currency;
	}

	/* Say shekel amount (Israel currency) */
	switch (atoi(currency)) {
	case 1:
		switch_say_file(sh, "currency/shekel");
		switch_say_file(sh, "digits/1_m");
		break;

	case 2:
		/* In the case of 2, because currency, we need a special case of 2 pronunciation */
		switch_say_file(sh, "digits/shney");
		switch_say_file(sh, "currency/shkalim");
		break;

	default:
		say_args->gender = SSG_MASCULINE;
		he_say_general_count(sh, currency, say_args);
		switch_say_file(sh, "currency/shkalim");
		break;
	}

	if (cents) {
		/* We need to use the value twice, so speed it up by atoi only once */
		icents = atoi(cents);

		/* Say "and" */
		switch (icents) {
		case 2:
		case 8:
		case 12:
		case 13:
		case 17:
		case 18:
		case 19:
		case 30:
		case 80:
			switch_say_file(sh, "digits/uu");
			break;

		case 50:
			switch_say_file(sh, "digits/va");
			break;

		default:
			switch_say_file(sh, "digits/ve");
			break;
		}

		/* Say agorot (Israel currency equivalent for "cents") */
		switch (icents) {
		case 0:
			switch_say_file(sh, "digits/0");
			switch_say_file(sh, "currency/agorot");
			break;

		case 1:
			switch_say_file(sh, "currency/agora");
			switch_say_file(sh, "digits/1");
			break;

		case 2:
			/* In the case of 2, because currency, we need a special case of 2 pronunciation */
			switch_say_file(sh, "digits/shtey");
			switch_say_file(sh, "currency/agorot");
			break;

		default:
			say_args->gender = SSG_FEMININE;
			he_say_general_count(sh, cents, say_args);
			switch_say_file(sh, "currency/agorot");
			break;
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

	say_num_goto_status(sh, atoi(a), say_args->method, end);
	switch_say_file(sh, "digits/dot");
	say_num_goto_status(sh, atoi(b), say_args->method, end);
	switch_say_file(sh, "digits/dot");
	say_num_goto_status(sh, atoi(c), say_args->method, end);
	switch_say_file(sh, "digits/dot");
	say_num_goto_status(sh, atoi(d), say_args->method, end);

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
			if (a != '0' && say_args->gender == SSG_MASCULINE) {
				switch_say_file(sh, "digits/%c_m", a);
			} else {
				switch_say_file(sh, "digits/%c", a);
			}
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
		say_cb = he_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
	case SST_SHORT_DATE_TIME:
		say_cb = he_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = say_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = say_spell;
		break;
	case SST_CURRENCY:
		say_cb = he_say_money;
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


static switch_status_t he_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{

	switch_new_say_callback_t say_cb = NULL;
	char *string = NULL;

	switch_status_t status = SWITCH_STATUS_FALSE;

	say_cb = choose_callback(say_args);

	if (say_cb) {
		status = run_callback(say_cb, tosay, say_args, session, &string);
		if (session && string) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "** he_say [%s]\n", string);
			status = switch_ivr_play_file(session, NULL, string, args);
		}

		switch_safe_free(string);
	}

	return status;
}


static switch_status_t he_say_string(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, char **rstr)
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


SWITCH_MODULE_LOAD_FUNCTION(mod_say_he_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "he";
	say_interface->say_function = he_say;
	say_interface->say_string_function = he_say_string;

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
