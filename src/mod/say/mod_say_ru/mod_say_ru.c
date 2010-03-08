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
 * Boris Buklov (BBV) <buklov@mail.ru>
 *
 * mod_say_ru.c -- Say for Russian
 *
 */

#include <switch.h>
#include "mod_say_ru.h"

/* инициализируем массив вариантов произношения цифр описано в define mod_say_ru.h */
struct say_t matrix[7][8] = { {m_00, m_01, m_02, m_03, m_04, m_05, m_06, m_07},
{m_10, m_11, m_12, m_13, m_14, m_15, m_16, m_17},
{m_20, m_21, m_22, m_23, m_24, m_25, m_26, m_27},
{m_30, m_31, m_32, m_33, m_34, m_35, m_36, m_37},
{m_40, m_41, m_42, m_43, m_44, m_45, m_46, m_47},
{m_50, m_51, m_52, m_53, m_54, m_55, m_56, m_57},
{m_60, m_61, m_62, m_63, m_64, m_65, m_66, m_67}
};

SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load);
SWITCH_MODULE_DEFINITION(mod_say_ru, mod_say_ru_load, NULL, NULL);

#define say_file(...) {\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);\
		if ((tstatus = switch_ivr_play_file(session, NULL, tmp, args)) != SWITCH_STATUS_SUCCESS){ \
			return tstatus;\
		}\
		if (!switch_channel_ready(switch_core_session_get_channel(session))) {\
			return SWITCH_STATUS_FALSE;\
		}}

static switch_status_t play_group(say_type_t say_type, casus_t casus, int a, int b, int c,
								  unit_t what, switch_core_session_t *session, switch_input_args_t *args)
{
	if (a) {
		if (((b == 0) && (c == 0)) || (matrix[casus][say_type].all == 1)) {	//если b и с равны 0 то сказать шестьсот, сестисотый, шестисотая
			if (what == million) {	//префикс           число          окончание
				say_file("digits/%s%d00%s.wav", matrix[casus][say_type].million[12], a, matrix[casus][say_type].million[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].million[11]);
			} else if (what == thousand) {
				say_file("digits/%s%d00%s.wav", matrix[casus][say_type].thousand[12], a, matrix[casus][say_type].thousand[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].thousand[11]);
			} else {
				say_file("digits/%s%d00%s.wav", matrix[casus][say_type].num[6], a, matrix[casus][say_type].num[7]);
			}
		} else {				//если дальше есть цифры то тысячи и миллионы не прозносить пока
			say_file("digits/%d00.wav", a);
		}
	}

	if (b) {
		if (b > 1) {			//если 20 и больше
			if ((c == 0) || (matrix[casus][say_type].all == 1)) {	//если с равны 0 то сказать 20, двадцати, двадцатая
				if (what == million) {	//префикс            число          окончание
					say_file("digits/%s%d0%s.wav", matrix[casus][say_type].million[12], b, matrix[casus][say_type].million[13]);
					say_file("digits/%s.wav", matrix[casus][say_type].million[11]);
				} else if (what == thousand) {
					say_file("digits/%s%d0%s.wav", matrix[casus][say_type].thousand[12], b, matrix[casus][say_type].thousand[13]);
					say_file("digits/%s.wav", matrix[casus][say_type].thousand[11]);
				} else {
					say_file("digits/%s%d0%s.wav", matrix[casus][say_type].num[6], b, matrix[casus][say_type].num[7]);
				}
			} else {			//если есть дальше цифры
				say_file("digits/%d0.wav", b);
			}
		} else {				//от 10 до 19
			if (what == million) {
				say_file("digits/%s%d%d%s.wav", matrix[casus][say_type].million[12], b, c, matrix[casus][say_type].million[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].million[11]);
			} else if (what == thousand) {
				say_file("digits/%s%d%d%s.wav", matrix[casus][say_type].thousand[12], b, c, matrix[casus][say_type].thousand[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].thousand[11]);
			} else {			//просто произнести цифры с префиксом и окончанием
				say_file("digits/%s%d%d%s.wav", matrix[casus][say_type].num[6], b, c, matrix[casus][say_type].num[7]);
			}
			c = 0;
		}
	}

	if (c || what == zero) {
		if (c <= 5) {
			if (what == million) {
				if ((strlen(matrix[casus][say_type].million[c * 2])) > 0) {	// не произносить если не заданно например 1 миллион а просто миллион
					say_file("digits/%s.wav", matrix[casus][say_type].million[c * 2])
				}
				say_file("digits/%s.wav", matrix[casus][say_type].million[c * 2 + 1]);
			} else if (what == thousand) {
				if ((strlen(matrix[casus][say_type].thousand[c * 2])) > 0) {	// не произносить если не заданно например одна тысячас  а просто тысяча
					say_file("digits/%s.wav", matrix[casus][say_type].thousand[c * 2])
				}
				say_file("digits/%s.wav", matrix[casus][say_type].thousand[c * 2 + 1]);
			} else {			//просто произнести цифры с префиксом и окончанием
				say_file("digits/%s.wav", matrix[casus][say_type].num[c]);
			}
		} else {				/* больше 5 */

			if (what == million) {
				say_file("digits/%s%d%s.wav", matrix[casus][say_type].million[12], c, matrix[casus][say_type].million[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].million[11]);
			} else if (what == thousand) {
				say_file("digits/%s%d%s.wav", matrix[casus][say_type].thousand[12], c, matrix[casus][say_type].thousand[13]);
				say_file("digits/%s.wav", matrix[casus][say_type].thousand[11]);
			} else {			//просто произнести цифры с префиксом и окончанием
				say_file("digits/%s%d%s.wav", matrix[casus][say_type].num[6], c, matrix[casus][say_type].num[7]);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t ru_say_count(switch_core_session_t *session, char *tosay, say_type_t say_type, casus_t casus, switch_input_args_t *args)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	int in_;

	switch_status_t status;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say_count %s!\n", tosay);

	if (!(tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);
	in_ = in;

	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}

		//миллионы      
		if (places[8] || places[7] || places[6]) {
			if ((in_ % 1000000 > 0) && (matrix[casus][say_type].all != 1)) {	// если поле миллионов  есть цифры поизнести как числительое именительного падежа
				if ((status = play_group(male_c, nominativus, places[8], places[7], places[6], million, session, args)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d million! status=%d\n", places[8], places[7], places[6],
									  status);
					return status;
				}
			} else {			// иначе произнести в нужном падеже
				if ((status = play_group(say_type, casus, places[8], places[7], places[6], million, session, args)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d million! status=%d\n", places[8], places[7], places[6],
									  status);
					return status;
				}
			}
		}
		//тысячи      
		if (places[5] || places[4] || places[3]) {
			if ((in_ % 1000 > 0) && (matrix[casus][say_type].all != 1)) {	// если поле миллионов  есть цифры поизнести как числительое именительного падежа
				if ((status = play_group(male_c, nominativus, places[5], places[4], places[3], thousand, session, args)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3],
									  status);
					return status;
				}
			} else {			// иначе произнести в нужном падеже
				if ((status = play_group(say_type, casus, places[5], places[4], places[3], thousand, session, args)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3],
									  status);
					return status;
				}
			}
		}
		// сотни
		if ((status = play_group(say_type, casus, places[2], places[1], places[0], empty, session, args)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3], status);
			return status;
		}
	} else {
		if ((status = play_group(say_type, casus, places[2], places[1], places[0], zero, session, args)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d other!\n", places[2], places[1], places[0]);
			return status;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

//дописать
static switch_status_t ru_say_general_count(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	switch_status_t status;
	casus_t casus;				//падеж
	say_type_t say_type;		//тип произношения

	switch (say_args->type) {
	case SST_MESSAGES:
		say_type = it_c;
		casus = nominativus;
		break;
	default:
		say_type = male_c;
		casus = nominativus;
		break;
	};
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " type=%d   casus=%d\n", say_type, casus);
	status = ru_say_count(session, tosay, say_type, casus, args);
	return status;
}

static switch_status_t ru_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char sbuf[16] = "";
	char *rubles = NULL;
	char *kopecks = NULL;
	int irubles = 0;
	int iruble = 0;
	int ikopecks = 0;
	int ikopeck = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_money %s\n", tosay);

	if (strlen(tosay) > 15 || !(tosay = switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	rubles = sbuf;
	if ((kopecks = strchr(sbuf, '.'))) {
		*kopecks++ = '\0';
		if (strlen(kopecks) > 2) {
			kopecks[2] = '\0';
		}
	}

	if (sbuf[0] == '+') {
		rubles++;
	}

	if (sbuf[0] == '-') {
		say_file("currency/minus.wav");
		rubles++;
	}

	ru_say_count(session, rubles, male_c, nominativus, args);

	if (rubles) {
		irubles = atoi(rubles) % 100;
		iruble = atoi(rubles) % 10;
	}

	if (irubles == 1 || (irubles > 20 && iruble == 1)) {	/* рубль */
		say_file("currency/ruble.wav");
	} else if ((irubles > 1 && irubles < 5) || (irubles > 20 && iruble > 1 && iruble < 5)) {	/*рубля */
		say_file("currency/ruble-a.wav");
	} else {					/*рублей */
		say_file("currency/rubles.wav");
	}

	/* Say kopecks */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " %s\n", kopecks);
	ru_say_count(session, kopecks, female_c, nominativus, args);

	if (kopecks) {
		ikopecks = atoi(kopecks) % 100;
		ikopeck = atoi(kopecks) % 10;
	}

	if (ikopecks == 1 || (ikopecks > 20 && ikopeck == 1)) {
		/* копейка */
		say_file("currency/kopeck.wav");
	} else if ((ikopecks > 1 && ikopecks < 5) || (ikopecks > 20 && ikopeck > 1 && ikopeck < 5)) {
		/* копейки */
		say_file("currency/kopeck-i.wav");
	} else {
		/* копеек */
		say_file("currency/kopecks.wav");
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ru_say_time(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int32_t t;
	char buf[80];
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *tz = switch_channel_get_variable(channel, "timezone");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_time %s  type=%d method=%d\n", tosay, say_args->type, say_args->method);

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
			if ((seconds = atol(tosay)) <= 0) {
				seconds = (int64_t) switch_epoch_time_now(NULL);
			}

			if (seconds >= 60) {
				minutes = seconds / 60;
				r = seconds % 60;
			}

			if (minutes >= 60) {
				hours = minutes / 60;
				r = minutes % 60;
				minutes = r;
			}
		}

		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) hours);
		ru_say_count(session, buf, male_c, nominativus, args);

		if (((hours % 10) == 1) && (hours != 11)) {
			/* час */
			say_file("time/hour.wav");
		} else if (((hours % 10 > 1) && (hours % 10 < 5)) && ((hours < 12) || (hours > 14))) {
			say_file("time/hours-a.wav");	/* часа */
		} else {
			say_file("time/hours.wav");	/* часов */
		}

		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) minutes);	//перевести минуты в *char
		ru_say_count(session, buf, female_c, nominativus, args);

		if (((minutes % 10) == 1) && (minutes != 11)) {
			say_file("time/minute.wav");	//минута
		} else if (((minutes % 10 > 1) && (minutes % 10 < 5)) && ((minutes < 12) || (minutes > 14))) {
			say_file("time/minutes-i.wav");	// минуты
		} else {
			say_file("time/minutes.wav");	//минут
		}

		if (seconds != 0) {
			switch_snprintf(buf, sizeof(buf), "%u", (unsigned) seconds);
			ru_say_count(session, buf, female_c, nominativus, args);
			if (((seconds % 10) == 1) && (seconds != 11)) {
				say_file("time/second.wav");	// секунда
			} else if (((seconds % 10 > 1) && (seconds % 10 < 5)) && ((seconds < 12) || (seconds > 14))) {
				say_file("time/seconds-i.wav");	// секуны
			} else {
				say_file("time/seconds.wav");	//секунд
			}
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
		say_file("time/today.wav");
	}
	if (say_yesterday) {
		say_file("time/yesterday.wav");
	}
	if (say_dow) {
		say_file("time/day-%d.wav", tm.tm_wday);
	}
	if (say_date) {
		say_year = say_month = say_day = say_dow = 1;
		say_today = say_yesterday = 0;
	}
	if (say_day) {
		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) tm.tm_mday);
		ru_say_count(session, buf, male_h, genitivus, args);
	}
	if (say_month) {
		say_file("time/mon-%d.wav", tm.tm_mon);
	}
	if (say_year) {
		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) (tm.tm_year + 1900));
		ru_say_count(session, buf, male_h, genitivus, args);
		say_file("time/h-year.wav");
	}
	if (say_month || say_year || say_date || say_dow) {
		say_file("time/at.wav");
	}
	if (say_time) {
		switch_snprintf(buf, sizeof(buf), "%d:%d:%d", tm.tm_hour + 1, tm.tm_min, tm.tm_sec);
		say_args->type = SST_TIME_MEASUREMENT;
		ru_say_time(session, buf, say_args, args);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ru_ip(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char *a, *b, *c, *d;
	if (!(a = switch_core_session_strdup(session, tosay))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(b = strchr(a, '.'))) {
		return SWITCH_STATUS_FALSE;
	}

	*b++ = '\0';

	if (!(c = strchr(b, '.'))) {
		return SWITCH_STATUS_FALSE;
	}

	*c++ = '\0';

	if (!(d = strchr(c, '.'))) {
		return SWITCH_STATUS_FALSE;
	}

	*d++ = '\0';

	ru_say_count(session, a, male_c, nominativus, args);
	say_file("digits/dot.wav");

	ru_say_count(session, b, male_c, nominativus, args);
	say_file("digits/dot.wav");

	ru_say_count(session, c, male_c, nominativus, args);
	say_file("digits/dot.wav");

	ru_say_count(session, d, male_c, nominativus, args);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ru_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	switch_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = ru_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
		say_cb = ru_say_time;
		break;

	case SST_CURRENT_DATE:
		say_cb = ru_say_time;
		break;

	case SST_CURRENT_TIME:
		say_cb = ru_say_time;
		break;

	case SST_CURRENT_DATE_TIME:
		say_cb = ru_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = ru_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		return switch_ivr_say_spell(session, tosay, say_args, args);
		break;
	case SST_CURRENCY:
		say_cb = ru_say_money;
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

SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "ru";
	say_interface->say_function = ru_say;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
