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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Mehmet Yıldırım <meyildirim@innova.com.tr>
 *
 * mod_say_tr.c -- Say for Turkish
 *
 */

#include <ctype.h>
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_say_tr_load);
SWITCH_MODULE_DEFINITION(mod_say_tr, mod_say_tr_load, NULL, NULL);

#define say_num(num, meth)                                                                                             \
	{                                                                                                                  \
		char tmp[80];                                                                                                  \
		switch_status_t tstatus;                                                                                       \
		switch_say_method_t smeth = say_args->method;                                                                  \
		switch_say_type_t stype = say_args->type;                                                                      \
		say_args->type = SST_ITEMS;                                                                                    \
		say_args->method = meth;                                                                                       \
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);                                                        \
		if ((tstatus = tr_say_general_count(session, tmp, say_args, args)) != SWITCH_STATUS_SUCCESS) {                 \
			return tstatus;                                                                                            \
		}                                                                                                              \
		say_args->method = smeth;                                                                                      \
		say_args->type = stype;                                                                                        \
	}

#define say_file(...)                                                                                                  \
	{                                                                                                                  \
		char tmp[80];                                                                                                  \
		switch_status_t tstatus;                                                                                       \
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);                                                                \
		if ((tstatus = switch_ivr_play_file(session, NULL, tmp, args)) != SWITCH_STATUS_SUCCESS) { return tstatus; }   \
		if (!switch_channel_ready(switch_core_session_get_channel(session))) { return SWITCH_STATUS_FALSE; }           \
	}

static switch_status_t play_group(switch_say_method_t method, int a, int b, int c, char *what,
								  switch_core_session_t *session, switch_input_args_t *args)
{
	if (a) {
		/**
		 * 100 = Yüz
		 * 200 = İkiyüz
		 * 300 = Üçyüz
		 * 400 = Dörtyüz
		 * 500 = Beşyüz
		 * 600 = Altıyüz
		 * 700 = Yediyüz
		 * 800 = Sekizyüz
		 * 900 = Dokuzyüz
		 */
		if (a > 1) { say_file("digits/%d", a); }

		if (method == SSM_COUNTED && !b && !c) {
			/**
			 * In Turkish for 100th, "yüzüncü" suffix is used.
			 */
			say_file("digits/yuzuncu", a);
		} else {
			say_file("digits/yuz", a);
		}
	}

	if (b) {
		if (method == SSM_COUNTED && !c) {
			/**
			 * 10. Onuncu
			 * 20. Yirmiinci
			 * 30. Otuzuncu
			 * 40. Kırkıncı
			 * 50. Ellinci
			 * 60. Altmışıncı
			 * 70. Yetmişinci
			 * 80. Sekizinci
			 * 90. Dokuzuncu
			 */
			say_file("digits/%d0-nth", b);
		} else {
			/**
			 * 10 = On
			 * 20 = Yirmi
			 * 30 = Otuz
			 * 40 = Kırk
			 * 50 = Elli
			 * 60 = Altmış
			 * 70 = Yetmiş
			 * 80 = Seksen
			 * 90 = Doksan
			 */
			say_file("digits/%d0", b);
		}
	}

	if (c) {
		/* In Turkish, "One Thousand" is just "Bin", not "Bir Bin" */
		if (what && !strncmp(what, "digits/bin", 10) && !a && !b && c == 1) {
			/* Skip saying "Bir" for Bin and Bininci */
		} else {
			if (method == SSM_COUNTED) {
				/**
				 * 1. Birinci
				 * 2. İkinci
				 * 3. Üçüncü
				 * 4. Dördüncü
				 * 5. Beşinci
				 * 6. Altıncı
				 * 7. Yedinci
				 * 8. Sekizinci
				 * 9. Dokuzuncu
				 */
				say_file("digits/%d-nth", c);
			} else {
				/**
				 * 1 = Bir
				 * 2 = İki
				 * 3 = Üç
				 * 4 = Dört
				 * 5 = Beş
				 * 6 = Altı
				 * 7 = Yedi
				 * 8 = Sekiz
				 * 9 = Dokuz
				 */
				say_file("digits/%d", c);
			}
		}
	}

	if (what && (a || b || c)) { say_file(what); }

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_general_count(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
											switch_input_args_t *args)
{
	int64_t in;
	char sbuf[128] = "";
	char digits[32];
	int i;
	switch_status_t status;

	if (say_args->method == SSM_ITERATED) {
		if ((tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf) - 1))) {
			char *p;
			for (p = tosay; p && *p; p++) { say_file("digits/%c", *p); }
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
			return SWITCH_STATUS_GENERR;
		}
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf) - 1)) || strlen(tosay) > 15) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoll(tosay);
	if (in != 0) {
		int places[15] = {0};
		int64_t temp_in = in;

		// Populate places manually to avoid pow precision/cast issues
		// 10^14
		for (i = 0; i < 15; i++) {
			places[i] = temp_in % 10;
			temp_in /= 10;
		}
		// Note: places[0] is units, places[1] is tens.
		// play_group expects: (hundreds, tens, units).
		// My places array is reversed compared to mod_say_en's logic which used pow(10, x) from top down.
		// mod_say_en: places[8] = 100M, places[0] = units.
		// My loop above: places[0] = units.
		// So indices match!
		// places[14] is 100T. places[12] is T units.

		snprintf(digits, sizeof(digits), "%lld", in);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Say: %s \n", digits);

		switch (say_args->method) {
		case SSM_COUNTED:
		case SSM_PRONOUNCED: {
			char *trilyon_file = "digits/trilyon";
			char *milyar_file = "digits/milyar";
			char *milyon_file = "digits/milyon";
			char *bin_file = "digits/bin";

			if (say_args->method == SSM_COUNTED) {
				// Check if lower magnitudes are empty to apply ordinal suffix to the lowest non-empty magnitude

				int has_trilyon = places[14] || places[13] || places[12];
				int has_milyar = places[11] || places[10] || places[9];
				int has_milyon = places[8] || places[7] || places[6];
				int has_bin = places[5] || places[4] || places[3];
				int has_units = places[2] || places[1] || places[0];

				if (!has_units && !has_bin && !has_milyon && !has_milyar && has_trilyon) {
					trilyon_file = "digits/trilyon-nth";
				}
				if (!has_units && !has_bin && !has_milyon && has_milyar) { milyar_file = "digits/milyar-nth"; }
				if (!has_units && !has_bin && has_milyon) { milyon_file = "digits/milyon-nth"; }
				if (!has_units && has_bin) { bin_file = "digits/bin-nth"; }
			}

			// 1.000.000.000.000 = Bir Trilyon
			if ((status = play_group(SSM_PRONOUNCED, places[14], places[13], places[12], trilyon_file, session,
									 args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			// 1.000.000.000 = Bir Milyar
			if ((status = play_group(SSM_PRONOUNCED, places[11], places[10], places[9], milyar_file, session, args)) !=
				SWITCH_STATUS_SUCCESS) {
				return status;
			}
			// 1.000.000 = Bir Milyon
			if ((status = play_group(SSM_PRONOUNCED, places[8], places[7], places[6], milyon_file, session, args)) !=
				SWITCH_STATUS_SUCCESS) {
				return status;
			}
			// 1.000 = Bin
			if ((status = play_group(SSM_PRONOUNCED, places[5], places[4], places[3], bin_file, session, args)) !=
				SWITCH_STATUS_SUCCESS) {
				return status;
			}
			// 1 = Bir
			if ((status = play_group(say_args->method, places[2], places[1], places[0], NULL, session, args)) !=
				SWITCH_STATUS_SUCCESS) {
				return status;
			}
			break;
		}
		default:
			break;
		}
	} else {
		say_file("digits/0");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_time(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
								   switch_input_args_t *args)
{
	int64_t t = 0;
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0,
			say_today = 0;
	const char *tz = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	tz = switch_channel_get_variable(channel, "timezone");

	if (say_args->type == SST_TIME_MEASUREMENT) {
		int64_t hours = 0;
		int64_t minutes = 0;
		int64_t seconds = 0;
		int64_t r = 0;

		if (strchr(tosay, ':')) {
			char *tme = strdup(tosay);
			char *p;
			switch_assert(tme);
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
			if ((seconds = atol(tosay)) <= 0) { seconds = (int64_t)switch_epoch_time_now(NULL); }

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
			if (minutes || seconds) {
				say_file("time/saat+");
			} else {
				say_file("time/saat");
			}
		}
		if (minutes) {
			say_num(minutes, SSM_PRONOUNCED);
			if (seconds) {
				say_file("time/dakika+");
			} else {
				say_file("time/dakika");
			}
		}

		if (seconds) {
			say_num(seconds, SSM_PRONOUNCED);
			say_file("time/saniye");
		}

		return SWITCH_STATUS_SUCCESS;
	}

	if (strchr(tosay, ':')) {
		switch_time_t tme = switch_str_time(tosay);
		t = (int64_t)((tme) / (int64_t)(1000000));

		target = switch_time_make(t, 0);
		target_now = switch_micro_time_now();
	}

	if (!t) {
		if ((t = atol(tosay)) > 0) {
			target = switch_time_make(t, 0);
			target_now = switch_micro_time_now();
		} else {
			target = switch_micro_time_now();
			target_now = switch_micro_time_now();
		}
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
		// Time is in the future
		if ((tm.tm_year > tm_now.tm_year) || (tm.tm_year == tm_now.tm_year && tm.tm_mon > tm_now.tm_mon) ||
			(tm.tm_year == tm_now.tm_year && tm.tm_mon == tm_now.tm_mon && tm.tm_mday > tm_now.tm_mday)) {
			say_date = 1;
			break;
		}
		// Time is today or earlier
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

		say_month = say_day = say_dow = 1;

		break;
	default:
		break;
	}

	if (say_date) {
		say_year = say_month = say_day = say_dow = 1;
		say_today = say_yesterday = 0;
	}

	if (say_today) { say_file("time/bugun"); }
	if (say_yesterday) { say_file("time/dun"); }
	if (say_dow) { say_file("time/day-%d", tm.tm_wday); }
	if (say_day) {
		say_num(tm.tm_mday, SSM_PRONOUNCED); // 1. 2. 3. (Birinci, Ikinci...) for day of month?
		// In Turkish, dates are usually cardinal numbers (1 Ekim, 25 Kasim), not ordinal (Birinci Ekim is rare/wrong).
		// It should be SSM_PRONOUNCED.
		// Wait, earlier I saw SSM_COUNTED for ordinals (Birinci).
		// Standard Turkish date reading: "25 October" -> "Yirmi Beş Ekim".
		// So SSM_PRONOUNCED is correct.
		// Modifying to SSM_PRONOUNCED.
	}
	if (say_month) { say_file("time/mon-%d", tm.tm_mon); }
	if (say_year) { say_num(tm.tm_year + 1900, SSM_PRONOUNCED); }

	if (say_time) {
		int32_t hour = tm.tm_hour;

		if (say_date || say_today || say_yesterday || say_dow) {
			say_file("time/at"); // "saat" or "de/da"
		}
		// In Turkish, "at 5" is "saat 5'te".
		// "time/at" might be "saat" word.
		// I will use "time/saat" if not already spoken.
		// But "time/saat" is used inside the block below.

		say_file("time/saat");
		say_num(hour, SSM_PRONOUNCED);
		if (tm.tm_min) { say_num(tm.tm_min, SSM_PRONOUNCED); }
		// No default "hundred" or "oclock" needed for Turkish usually if minute is 0.
		// "Saat On" is fine.
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
									switch_input_args_t *args)
{
	char sbuf[16] = ""; /* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *dollars = NULL;
	char *cents = NULL;

	if (strlen(tosay) > 15 || !switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf) - 1)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	dollars = sbuf;

	if ((cents = strchr(sbuf, '.'))) {
		*cents++ = '\0';
		if (strlen(cents) > 2) { cents[2] = '\0'; }
	}

	say_args->type = SST_ITEMS;
	say_args->method = SSM_PRONOUNCED;
	tr_say_general_count(session, dollars, say_args, args);
	say_file("money/lira");

	if (cents) {
		tr_say_general_count(session, cents, say_args, args);
		say_file("money/kurus");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_telephone_number(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
											   switch_input_args_t *args)
{
	int silence = 0;
	char *p;

	for (p = tosay; !zstr(p); p++) {
		int a = tolower((int)*p);
		if (a >= '0' && a <= '9') {
			say_file("digits/%c", a);
			silence = 0;
		} else if (a == '+' || (a >= 'a' && a <= 'z')) {
			switch_say_file(sh, "ascii/%d", a);
			silence = 0;
		} else if (!silence) {
			switch_say_file(sh, "silence_stream://100");
			silence = 1;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_spell(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
									switch_input_args_t *args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int)*p);
		if (a >= '0' && a <= '9') {
			say_file("digits/%c", a);
		} else {
			if (say_args->type == SST_NAME_SPELLED) {
				say_file("ascii/%d", a);
			} else if (say_args->type == SST_NAME_PHONETIC) {
				say_file("phonetic-ascii/%d", a);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t tr_say_ip(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
								 switch_input_args_t *args)
{
	char *a, *b, *c, *d;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!(a = strdup(tosay))) { abort(); }

	if (!(b = strchr(a, '.'))) { goto end; }

	*b++ = '\0';

	if (!(c = strchr(b, '.'))) { goto end; }

	*c++ = '\0';

	if (!(d = strchr(c, '.'))) { goto end; }

	*d++ = '\0';

	say_num_goto_status(atoi(a), say_args->method, end);
	say_file("digits/nokta");
	say_num_goto_status(atoi(b), say_args->method, end);
	say_file("digits/nokta");
	say_num_goto_status(atoi(c), say_args->method, end);
	say_file("digits/nokta");
	say_num_goto_status(atoi(d), say_args->method, end);

end:

	free(a);

	return status;
}

static switch_status_t tr_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
							  switch_input_args_t *args)
{
	switch_new_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = tr_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
	case SST_SHORT_DATE_TIME:
		say_cb = tr_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = tr_say_ip;
		break;
	case SST_TELEPHONE_NUMBER:
		say_cb = tr_say_telephone_number;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = tr_say_spell;
		break;
	case SST_CURRENCY:
		say_cb = tr_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", say_args->type);
		break;
	}

	if (say_cb) { return say_cb(session, tosay, say_args, args); }

	return SWITCH_STATUS_FALSE;
}

static switch_status_t tr_say_string(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args,
									 char **rstr)
{
	switch_new_say_callback_t say_cb = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *string = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = tr_say_general_count;
		break;
	}

	if (say_cb) {
		status = say_cb(session, tosay, say_args, &string);
		if (string) {
			status = SWITCH_STATUS_SUCCESS;
			*rstr = string;
		}
	}
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_say_tr_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "tr";
	say_interface->say_function = tr_say;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}