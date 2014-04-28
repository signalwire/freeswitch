/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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


struct say_cur_t matrix_currency[3] = {c_0, c_1, c_2};


SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load);
SWITCH_MODULE_DEFINITION(mod_say_ru, mod_say_ru_load, NULL, NULL);


static switch_status_t play_group(say_gender_t gender, cases_t cases, int a, int b, int c, unit_t what,	switch_say_file_handle_t *sh)
{
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play_group ! %d%d%d  gender=%d  causes=%d\n",a,b,c,gender,cases);
	if (a) {
		if (((b == 0) && (c == 0)) || (matrix[cases][gender].all == 1)) {	//если b и с равны 0 то сказать шестьсот, сестисотый, шестисотая
			if (what == million) {	//префикс           число          окончание
				switch_say_file(sh, "digits/%s%d00%s", matrix[cases][gender].million[12], a, matrix[cases][gender].million[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].million[11]);
			} else if (what == thousand) {
				switch_say_file(sh, "digits/%s%d00%s", matrix[cases][gender].thousand[12], a, matrix[cases][gender].thousand[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[11]);
			} else {
				switch_say_file(sh, "digits/%s%d00%s", matrix[cases][gender].num[6], a, matrix[cases][gender].num[7]);
			}
		} else {				//если дальше есть цифры то тысячи и миллионы не прозносить пока
			switch_say_file(sh, "digits/%d00", a);
		}
	}

	if (b) {
		if (b > 1) {			//если 20 и больше
			if ((c == 0) || (matrix[cases][gender].all == 1)) {	//если с равны 0 то сказать 20, двадцати, двадцатая
				if (what == million) {	//префикс            число          окончание
					switch_say_file(sh, "digits/%s%d0%s", matrix[cases][gender].million[12], b, matrix[cases][gender].million[13]);
					switch_say_file(sh, "digits/%s", matrix[cases][gender].million[11]);
				} else if (what == thousand) {
					switch_say_file(sh, "digits/%s%d0%s", matrix[cases][gender].thousand[12], b, matrix[cases][gender].thousand[13]);
					switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[11]);
				} else {
					switch_say_file(sh, "digits/%s%d0%s", matrix[cases][gender].num[6], b, matrix[cases][gender].num[7]);
				}
			} else {			//если есть дальше цифры
				switch_say_file(sh, "digits/%d0", b);
			}
		} else {				//от 10 до 19
			if (what == million) {
				switch_say_file(sh, "digits/%s%d%d%s", matrix[cases][gender].million[12], b, c, matrix[cases][gender].million[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].million[11]);
			} else if (what == thousand) {
				switch_say_file(sh, "digits/%s%d%d%s", matrix[cases][gender].thousand[12], b, c, matrix[cases][gender].thousand[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[11]);
			} else {			//просто произнести цифры с префиксом и окончанием
				switch_say_file(sh, "digits/%s%d%d%s", matrix[cases][gender].num[6], b, c, matrix[cases][gender].num[7]);
			}
			c = 0;
		}
	}

	if (c || what == zero) {
		if (c <= 5) {
			if (what == million) {
				if ((strlen(matrix[cases][gender].million[c * 2])) > 0) {	// не произносить если не заданно например 1 миллион а просто миллион
					switch_say_file(sh, "digits/%s", matrix[cases][gender].million[c * 2]);
				}
				switch_say_file(sh, "digits/%s", matrix[cases][gender].million[c * 2 + 1]);
			} else if (what == thousand) {
				if ((strlen(matrix[cases][gender].thousand[c * 2])) > 0) {	// не произносить если не заданно например одна тысячас  а просто тысяча
					switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[c * 2]);
				}
				switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[c * 2 + 1]);
			} else {			//просто произнести цифры с префиксом и окончанием
				switch_say_file(sh, "digits/%s", matrix[cases][gender].num[c]);
			}
		} else {				/* больше 5 */

			if (what == million) {
				switch_say_file(sh, "digits/%s%d%s", matrix[cases][gender].million[12], c, matrix[cases][gender].million[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].million[11]);
			} else if (what == thousand) {
				switch_say_file(sh, "digits/%s%d%s", matrix[cases][gender].thousand[12], c, matrix[cases][gender].thousand[13]);
				switch_say_file(sh, "digits/%s", matrix[cases][gender].thousand[11]);
			} else {			//просто произнести цифры с префиксом и окончанием
				switch_say_file(sh, "digits/%s%d%s", matrix[cases][gender].num[6], c, matrix[cases][gender].num[7]);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t ru_say_count(switch_say_file_handle_t *sh, char *tosay, say_gender_t gender, cases_t cases)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	int in_;

	switch_status_t status;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say_count %s!   gender=%d  causes=%d\n", tosay,gender,cases);

	if (!(tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)-1)) || strlen(tosay) > 9) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
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
			if ((in_ % 1000000 > 0) && (matrix[cases][gender].all != 1)) {	// если поле миллионов  есть цифры поизнести как числительое именительного падежа
				if ((status = play_group(male, nominativus, places[8], places[7], places[6], million, sh)) != SWITCH_STATUS_SUCCESS) {
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d million! status=%d\n", places[8], places[7], places[6], status);
					return status;
				}
			} else {			// иначе произнести в нужном падеже
				if ((status = play_group(gender, cases, places[8], places[7], places[6], million, sh)) != SWITCH_STATUS_SUCCESS) {
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d million! status=%d\n", places[8], places[7], places[6],  status);
					return status;
				}
			}
		}
		//тысячи      
		if (places[5] || places[4] || places[3]) {
			if ((in_ % 1000 > 0) && (matrix[cases][gender].all != 1)) {	// если поле миллионов  есть цифры поизнести как числительое именительного падежа
				if ((status = play_group(male, nominativus, places[5], places[4], places[3], thousand, sh)) != SWITCH_STATUS_SUCCESS) {
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3],  status);
					return status;
				}
			} else {			// иначе произнести в нужном падеже
				if ((status = play_group(gender, cases, places[5], places[4], places[3], thousand, sh)) != SWITCH_STATUS_SUCCESS) {
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3],  status);
					return status;
				}
			}
		}
		// сотни
		if ((status = play_group(gender, cases, places[2], places[1], places[0], empty, sh)) != SWITCH_STATUS_SUCCESS) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d thousand! status=%d\n", places[5], places[4], places[3], status);
			return status;
		}
	} else {
		if ((status = play_group(gender, cases, places[2], places[1], places[0], zero, sh)) != SWITCH_STATUS_SUCCESS) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "play group %d %d %d other!\n", places[2], places[1], places[0]);
			return status;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

//дописать
static switch_status_t ru_say_general_count(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args,say_opt_t *say_opt)
{
	switch_status_t status;
	cases_t cases;				//падеж
	say_gender_t gender;		//тип произношения
	char sbuf[128] = "";

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

	switch (say_args->type) {
	case SST_MESSAGES:
		gender = it;
		cases = nominativus;
		break;
	default:
		gender = male;
		cases = nominativus;
		if (say_opt->gender>0) {
		    gender=say_opt->gender;
//		    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " opt_gender=%d type=%d   cases=%d\n", gender, cases,say_opt->gender);
		}
		if (say_opt->cases>0) {
		    cases=say_opt->cases;
//		    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " opt_gender=%d type=%d   cases=%d\n", gender, cases,say_opt->gender);

		}
		break;
	};
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " opt_gender=%d type=%d   cases=%d\n", gender, cases,say_opt->gender);
	status = ru_say_count(sh, tosay, (say_gender_t)gender, (cases_t)cases);
	return status;
}

static switch_status_t ru_say_money(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args,say_opt_t *say_opt)
{
	char sbuf[16] = "";
	char *rubles = NULL;
	char *kopecks = NULL;
	int irubles = 0;
	int iruble = 0;
	int ikopecks = 0;
	int ikopeck = 0;
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_money %s say_opt->currency=%d\n", tosay,say_opt->currency);

	if (strlen(tosay) > 15 || !(tosay = switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)-1))) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
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
		switch_say_file(sh, "currency/minus");
		rubles++;
	}


	ru_say_count(sh, rubles, matrix_currency[say_opt->currency].first_gender,matrix_currency[say_opt->currency].first_cases);

	if (rubles) {
		irubles = atoi(rubles) % 100;
		iruble = atoi(rubles) % 10;
	}
	if (iruble<5) {
	    if ((irubles>10)&&(irubles<15)) {
		switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].first[5]);
	    }
	    else {
		switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].first[iruble]);
	    }
	}
	else {
	    switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].first[5]);
	}
	    
	    
	/* Say kopecks */
	ru_say_count(sh, kopecks, matrix_currency[say_opt->currency].second_gender,matrix_currency[say_opt->currency].second_cases);

	if (kopecks) {
		ikopecks = atoi(kopecks) % 100;
		ikopeck = atoi(kopecks) % 10;
	}

	if (ikopeck<5) {
	    if ((ikopecks>10)&&(ikopecks<15)) {
		switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].second[5]);
	    }
	    else {
    		switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].second[ikopeck]);
	    }
	}
	else {
	    switch_say_file(sh, "currency/%s",matrix_currency[say_opt->currency].second[5]);
	}



	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ru_say_time(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args,say_opt_t *say_opt)
{
	int32_t t;
	char buf[80];
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	const char *tz = NULL;
	tz = switch_say_file_handle_get_variable(sh, "timezone");    

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " ru_say_time %s  type=%d method=%d\n", tosay, say_args->type, say_args->method);

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
			}

			if (minutes >= 60) {
				hours = minutes / 60;
				r = minutes % 60;
				minutes = r;
			}
		}

		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) hours);
		ru_say_count(sh, buf, male, nominativus);

		if (((hours % 10) == 1) && (hours != 11)) {
			/* час */
			switch_say_file(sh, "time/hour");
		} else if (((hours % 10 > 1) && (hours % 10 < 5)) && ((hours < 12) || (hours > 14))) {
			switch_say_file(sh, "time/hours-a");	/* часа */
		} else {
			switch_say_file(sh, "time/hours");	/* часов */
		}

		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) minutes);	//перевести минуты в *char
		ru_say_count(sh, buf, female, nominativus);

		if (((minutes % 10) == 1) && (minutes != 11)) {
			switch_say_file(sh, "time/minute");	//минута
		} else if (((minutes % 10 > 1) && (minutes % 10 < 5)) && ((minutes < 12) || (minutes > 14))) {
			switch_say_file(sh, "time/minutes-i");	// минуты
		} else {
			switch_say_file(sh, "time/minutes");	//минут
		}

		if (seconds != 0) {
			switch_snprintf(buf, sizeof(buf), "%u", (unsigned) seconds);
			ru_say_count(sh, buf, female, nominativus);
			if (((seconds % 10) == 1) && (seconds != 11)) {
				switch_say_file(sh, "time/second");	// секунда
			} else if (((seconds % 10 > 1) && (seconds % 10 < 5)) && ((seconds < 12) || (seconds > 14))) {
				switch_say_file(sh, "time/seconds-i");	// секуны
			} else {
				switch_say_file(sh, "time/seconds");	//секунд
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
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Timezone is [%s]\n", tz);
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
		tm.tm_sec = 0; // В коротком варианте секунды не проговариваем
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
	if (say_day) {
		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) tm.tm_mday);
		ru_say_count(sh, buf, male_h, genitivus);
	}
	if (say_month) {
		switch_say_file(sh, "time/mon-%d", tm.tm_mon);
	}
	if (say_year) {
		switch_snprintf(buf, sizeof(buf), "%u", (unsigned) (tm.tm_year + 1900));
		ru_say_count(sh, buf, male_h, genitivus);
		switch_say_file(sh, "time/h-year");
	}
	if (say_time) {
		if (say_month || say_year || say_date || say_dow) {
			switch_say_file(sh, "time/at");
		}
		switch_snprintf(buf, sizeof(buf), "%d:%d:%d", tm.tm_hour, tm.tm_min, tm.tm_sec);
		say_args->type = SST_TIME_MEASUREMENT;
		ru_say_time(sh, buf, say_args,say_opt);
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ru_ip(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args,say_opt_t *say_opt)
{
	char *a, *b, *c, *d;
	switch_status_t status = SWITCH_STATUS_FALSE;
	if (!(a = strdup(tosay))) {
    		abort();
//		return SWITCH_STATUS_FALSE;
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

	ru_say_count(sh, a, male, nominativus);
	switch_say_file(sh, "digits/dot");

	ru_say_count(sh, b, male, nominativus);
	switch_say_file(sh, "digits/dot");

	ru_say_count(sh, c, male, nominativus);
	switch_say_file(sh, "digits/dot");

	ru_say_count(sh, d, male, nominativus);
end:
	free(a);
	return status;
}


static switch_status_t ru_say_spell(switch_say_file_handle_t *sh, char *tosay, switch_say_args_t *say_args,say_opt_t *say_opt)
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




static switch_new_say_callback_ru_t choose_callback(switch_say_args_t *say_args)
{

	switch_new_say_callback_ru_t say_cb = NULL;
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
	case SST_SHORT_DATE_TIME:
	case SST_CURRENT_DATE_TIME:
		say_cb = ru_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = ru_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = ru_say_spell;
		break;
	case SST_CURRENCY:
		say_cb = ru_say_money;
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown Say type=[%d]\n", say_args->type);
		break;
	}

	return say_cb;
}


                                                                                                


static switch_status_t run_callback(switch_new_say_callback_ru_t say_cb, char *tosay, switch_say_args_t *say_args, switch_core_session_t *session, char **rstr)
{
        switch_say_file_handle_t *sh;
        switch_status_t status = SWITCH_STATUS_FALSE;
        switch_event_t *var_event = NULL;
        const char *cases=NULL;
        const char *gender=NULL;
        const char *currency=NULL;
	say_opt_t say_opt;
	say_opt.cases=0;
	say_opt.gender=0;
	say_opt.currency=0;
        if (session) {
                switch_channel_t *channel = switch_core_session_get_channel(session);
                switch_channel_get_variables(channel, &var_event);
// проверяем не заданы ли канальные переменные род, падеж, валюта
                gender = switch_channel_get_variable(channel, "gender");
                cases = switch_channel_get_variable(channel, "cases");
                currency = switch_channel_get_variable(channel, "currency");
                //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say!!!  %s  %s   %s !\n",gender, cases,currency);
                if (cases) {
		    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say!!!  %s!\n", cases);
                
		    if ((strcmp(cases,"nominativus")==0) || (strcmp(cases,"именительный")==0)) {
			say_opt.cases=(cases_t)0;
		    }
		    if ((strcmp(cases,"genitivus")==0) || (strcmp(cases,"родительный")==0)) {
			say_opt.cases=(cases_t)1;
		    }
		    if ((strcmp(cases,"dativus")==0) || (strcmp(cases,"дательный")==0)) {
			say_opt.cases=(cases_t)2;
		    }
		    if ((strcmp(cases,"accusativus_a")==0) || (strcmp(cases,"винительный_о")==0)) {
			say_opt.cases=(cases_t)3;
		    }
		    if ((strcmp(cases,"accusativus_i")==0) || (strcmp(cases,"винительный_н")==0)) {
			say_opt.cases=(cases_t)4;
		    }
		    if ((strcmp(cases,"instrumentalis")==0) || (strcmp(cases,"творительный")==0)) {
			say_opt.cases=(cases_t)5;
		    }
		    if ((strcmp(cases,"prepositive")==0) || (strcmp(cases,"предложный")==0)) {
			say_opt.cases=(cases_t)6;
		    }
		}
                if (gender) {
		    //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say!!!  %s!\n", gender);
                
		    if ((strcmp(gender,"male")==0) || (strcmp(gender,"мужской")==0)) {
			say_opt.gender=(say_gender_t)0;
		    }
		    if ((strcmp(gender,"it")==0) || (strcmp(gender,"средний")==0)) {
			say_opt.gender=(say_gender_t)1;
		    }
		    if ((strcmp(gender,"female")==0) || (strcmp(gender,"женский")==0)) {
			say_opt.gender=(say_gender_t)2;
		    }
		    if ((strcmp(gender,"plural")==0) || (strcmp(gender,"множественное")==0)) {
			say_opt.gender=(say_gender_t)3;
		    }
		    if ((strcmp(gender,"male_h")==0) || (strcmp(gender,"мужской_порядковый")==0)) {
			say_opt.gender=(say_gender_t)4;
		    }
		    if ((strcmp(gender,"it_h")==0) || (strcmp(gender,"средний_порядковый")==0)) {
			say_opt.gender=(say_gender_t)5;
		    }
		    if ((strcmp(gender,"female_h")==0) || (strcmp(gender,"женский_порядковый")==0)) {
			say_opt.gender=(say_gender_t)6;
		    }
		    if ((strcmp(gender,"plural_h")==0) || (strcmp(gender,"множественное_порядковый")==0)) {
			say_opt.gender=(say_gender_t)7;
		    }
            
    		}
                if (currency) {
		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say!!!  %s!\n", currency);
                
		    if ((strcmp(currency,"ruble")==0) || (strcmp(currency,"рубль")==0)) {
			say_opt.currency=(currency_t)0;
//			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "rul!!!  \n");
			
		    }
		    if ((strcmp(currency,"dollar")==0) || (strcmp(currency,"доллар")==0)) {
//			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "dollar!!!  !\n");
			say_opt.currency=(currency_t)1;
		    }
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ru_say!!!  %s!   say_opt.gender=%d   say_opt.cases=%d\n", tosay,say_opt.gender,say_opt.cases);
                                                                                
	}
        switch_say_file_handle_create(&sh, say_args->ext, &var_event);
//запуск ru_ip,ru_say_money ...
        status = say_cb(sh, tosay, say_args,&say_opt);

        if ((*rstr = switch_say_file_handle_detach_path(sh))) {
                status = SWITCH_STATUS_SUCCESS;
        }

        switch_say_file_handle_destroy(&sh);

        return status;
}



static switch_status_t ru_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{

        switch_new_say_callback_ru_t say_cb = NULL;
        char *string = NULL;
        switch_status_t status;
        
        status = SWITCH_STATUS_FALSE;
        
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



static switch_status_t ru_say_string(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, char **rstr)
{

        switch_new_say_callback_ru_t say_cb = NULL;
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



SWITCH_MODULE_LOAD_FUNCTION(mod_say_ru_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "ru";
	say_interface->say_function = ru_say;
	say_interface->say_string_function = ru_say_string;

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
