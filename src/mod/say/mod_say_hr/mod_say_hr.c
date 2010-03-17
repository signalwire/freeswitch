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
 * Michael B. Murdock <mike@mmurdock.org>
 * Tihomir Culjaga <tculjaga@gmail.com> 
 *
 * mod_say_hr.c -- Say for Croatian
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_say_hr_load);
SWITCH_MODULE_DEFINITION(mod_say_hr, mod_say_hr_load, NULL, NULL);

#define say_num(num, t, gen) {\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = t;	\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);				\
	if ((tstatus = hr_say_count(session, gen, tmp, say_args, args)) != SWITCH_STATUS_SUCCESS) {\
		return tstatus;\
	} \
	say_args->method = smeth; say_args->type = stype;				\
}\

#define say_file(...) {\
		char tmp[80];\
		switch_status_t tstatus;\
		switch_snprintf(tmp, sizeof(tmp), __VA_ARGS__);\
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "say_file:%s \n", tmp);\
		if ((tstatus = switch_ivr_play_file(session, NULL, tmp, args)) != SWITCH_STATUS_SUCCESS){ \
			return tstatus;\
		}\
		if (!switch_channel_ready(switch_core_session_get_channel(session))) {\
			return SWITCH_STATUS_FALSE;\
		}}\

#define nop

static switch_status_t hr_spell(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (a >= 48 && a <= 57) {
			say_file("digits/%d.wav", a - 48);
		} else {
			if (say_args->type == SST_NAME_SPELLED) {
				say_file("ascii/%d.wav", a);
			} else if (say_args->type == SST_NAME_PHONETIC) {
				say_file("phonetic-ascii/%d.wav", a);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/*static switch_status_t play_group(switch_say_method_t method, int a, int b, int c, char *what, int tosay, switch_core_session_t *session,
								  switch_input_args_t *args)
{
	if (a) {
		say_file("digits/%d.wav", a);
		say_file("digits/hundred.wav");
	}

	if (b) {
		if (b > 2) {
			if ((c == 0) && (method == SSM_COUNTED)) {
				say_file("digits/h-%d0.wav", b);
			} else {
				say_file("digits/%d0.wav", b);
			}
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
}*/

static switch_status_t play_group(switch_say_method_t method, char* gen, int a, int b, int c, char *what, int tosay, switch_core_session_t *session,
								  switch_input_args_t *args)
{
	if (a) {
		say_file("digits/%d.wav", a);
		say_file("digits/hundred.wav");
	}

	if (b) {
		if (b > 1) {
			if ((c == 0) && (method == SSM_COUNTED)) {
				say_file("digits/h-%d0.wav", b);
			} else {
				say_file("digits/%d0.wav", b);
			}
		} else {
			say_file("digits/%d%d%s.wav", b, c, gen);
			c = 0;
		}
	}

	if (c) {
		if (method == SSM_COUNTED) {
			say_file("digits/h-%d%s.wav", c, gen);
		} else {
			say_file("digits/%d%s.wav", c, gen);
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

static switch_status_t hr_say_count(switch_core_session_t *session, char* gen,
											char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	char milion[64] = "";
	char tisucu[64] = "";
	char tgen[8] = "";
	char mgen[8] = "";
	
	
	int number;
	switch_status_t status;

	strcpy(tgen, gen);
	
	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);
	number = in;

	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}

		switch (say_args->method) {
		case SSM_COUNTED:
		case SSM_PRONOUNCED:
			{
				if (places[6] == 1)
					strcpy(milion, "digits/milijun.wav");
				else
					strcpy(milion, "digits/milijuna.wav");
				
				if (places[4] == 1)	
				{
					strcpy(tisucu, "digits/thousands-a.wav");
				}
				else if (places[4] == 0)
				{
					switch(places[3])
					{
						case 1:
							strcpy(tisucu, "digits/thousands-a.wav");
							strcpy(tgen, "a");
							break;
							
						case 5:
						case 6:
						case 7:
						case 8:
						case 9:
							strcpy(tisucu, "digits/thousands-a.wav");
							break;
							
						case 2:
							strcpy(tgen, "je");
							strcpy(tisucu, "digits/thousands-e.wav");
							break;
							
						case 3:
						case 4:
							strcpy(tisucu, "digits/thousands-e.wav");
							break;
					}
				}
				else
				{
					switch(places[3])
					{
						case 1:
							strcpy(tisucu, "digits/thousands-a.wav");
							strcpy(tgen, "a");
							break;
							
						case 5:
						case 6:
						case 7:
						case 8:
						case 9:
							strcpy(tisucu, "digits/thousands-a.wav");
							break;
							
						case 2:
							strcpy(tisucu, "digits/thousands-e.wav");
							strcpy(tgen, "je");
							break;
							
						case 3:
						case 4:
							strcpy(tisucu, "digits/thousands-e.wav");
							break;
					}
				}

				
				if ((status =
					 play_group(SSM_PRONOUNCED, mgen, places[8], places[7], places[6], milion, number, session, args)) != SWITCH_STATUS_SUCCESS) {
					return status;
				}
				if ((status =
					 play_group(SSM_PRONOUNCED, tgen, places[5], places[4], places[3], tisucu, number, session, args)) != SWITCH_STATUS_SUCCESS) {
					return status;
				}
				if ((status = play_group(say_args->method, gen, places[2], places[1], places[0], NULL, number, session, args)) != SWITCH_STATUS_SUCCESS) {
					return status;
				}
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
	}
	/* else {
		say_file("digits/0.wav");
	}*/

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hr_say_general_count(switch_core_session_t *session,	char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int in;
	int x = 0;
	int places[9] = { 0 };
	char sbuf[13] = "";
	int number;
	switch_status_t status;

	if (!(tosay = strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	in = atoi(tosay);
	number = in;

	if (in != 0) {
		for (x = 8; x >= 0; x--) {
			int num = (int) pow(10, x);
			if ((places[(uint32_t) x] = in / num)) {
				in -= places[(uint32_t) x] * num;
			}
		}

		switch (say_args->method) {
		case SSM_COUNTED:
		case SSM_PRONOUNCED:
			if ((status =
				 play_group(SSM_PRONOUNCED, "", places[8], places[7], places[6], "digits/million.wav", number, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status =
				 play_group(SSM_PRONOUNCED, "", places[5], places[4], places[3], "digits/thousand.wav", number, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(say_args->method, "", places[2], places[1], places[0], NULL, number, session, args)) != SWITCH_STATUS_SUCCESS) {
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



static switch_status_t hr_ip(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
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

	say_num(atoi(a), say_args->method, "");
	say_file("digits/dot.wav");
	say_num(atoi(b), say_args->method, "");
	say_file("digits/dot.wav");
	say_num(atoi(c), say_args->method, "");
	say_file("digits/dot.wav");
	say_num(atoi(d), say_args->method, "");

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t hr_say_time(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int32_t t;
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *tz = switch_channel_get_variable(channel, "timezone");
	
	int sat_desetinka = 0;
	int sat_jedinica = 0;
	int minuta_desetinka = 0;
	int minuta_jedinica = 0;
	int sekunda_desetinka = 0;
	int sekunda_jedinica = 0;

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
				seconds = r;
			}

			if (minutes >= 60) {
				hours = minutes / 60;
				r = minutes % 60;
				minutes = r;
			}
		}

		sat_jedinica = hours % 10;
		
		if (hours > 10)
			sat_desetinka = (int) (hours / 10);
	
		if (hours) 
		{
			say_num(hours, SSM_PRONOUNCED, "");
			
			if (sat_desetinka == 0 && sat_jedinica == 0)
			{
				nop;
			}
			else if (sat_desetinka == 1)
			{
				say_file("time/sati.wav");
			}
			else
			{
				switch(sat_jedinica)
				{
					case 1:
						say_file("time/sat.wav");
						break;
						
					case 2:
					case 3:
					case 4:
						say_file("time/sata.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_file("time/sati.wav");
						break;
				}
			}
		} 

		minuta_jedinica = minutes % 10;
		
		if (minutes > 10)
			minuta_desetinka = (int) (minutes / 10);
			
		if (minutes) 
		{
			
			
			if (minuta_desetinka == 1)
			{
				say_num(minutes, SSM_PRONOUNCED, "");
				say_file("time/minuta.wav");
			}
			else
			{
				switch(minuta_jedinica)
				{
					case 2:
						say_num(minutes, SSM_PRONOUNCED, "je");
						say_file("time/minute.wav");
						break;
						
					case 3:
					case 4:
						say_num(minutes, SSM_PRONOUNCED, "");
						say_file("time/minute.wav");
						break;
					
					case 1:
							say_num(minutes, SSM_PRONOUNCED, "a");
							say_file("time/minuta.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_num(minutes, SSM_PRONOUNCED, "");
						say_file("time/minuta.wav");
						break;
				}
			}
		} 

		sekunda_jedinica = seconds % 10;
		
		if (seconds > 10)
			sekunda_desetinka = (int) (seconds / 10);
			
		if (seconds) 
		{
			if (sekunda_desetinka == 1)
			{
				say_num(seconds, SSM_PRONOUNCED, "");
				say_file("time/sekundi.wav");
			}
			else if (sekunda_desetinka == 0)
			{
				switch(sekunda_jedinica)
				{
					case 1:
						say_num(seconds, SSM_PRONOUNCED, "a");
						say_file("time/sekunda.wav");
						break;
						
					case 2:
						say_num(seconds, SSM_PRONOUNCED, "je");
						say_file("time/sekunde.wav");
						break;
						
					case 3:
					case 4:
						say_num(seconds, SSM_PRONOUNCED, "");
						say_file("time/sekunde.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_file("time/sekundi.wav");
						break;
				}
			}
			else
			{
				switch(sekunda_jedinica)
				{
					case 1:
						say_num(seconds, SSM_PRONOUNCED, "a");
						say_file("time/sekunda.wav");
						break;
						
					case 2:
						say_num(seconds, SSM_PRONOUNCED, "je");
						say_file("time/sekunde.wav");
						break;
						
					case 3:
					case 4:
						say_num(seconds, SSM_PRONOUNCED, "");
						say_file("time/sekunde.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_num(seconds, SSM_PRONOUNCED, "");
						say_file("time/sekundi.wav");
						break;
				}
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Timezone is [%s]\n", tz);
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
		say_num(tm.tm_mday, SSM_COUNTED, "");
	}
	
	if (say_month) {
		say_file("time/mon-%d.wav", tm.tm_mon);
	}
	
	if (say_year) {
		int y = tm.tm_year + 1900;
		int tis = 0;
		
		//say_num(tm.tm_year + 1900, SSM_PRONOUNCED, "");
		//1 - 99 => h-1a.wav - h-99a.wav
		if (y >= 1 && y <= 99)
		{
			say_file("digits/h-%da.wav", y);
		}
		// [1-9]00            =>     h-[1-9]00a.wav
		else if (y >= 100 && y <= 900 && y % 100 == 0)
		{
			say_file("digits/h-%da.wav", y);
		}
		//[1-9]01 - [1-9]99    =>     [1-9]00.wav  + (h-1a.wav - h-99a.wav)
		else if (y >= 100 && y <= 900 && y % 100 != 0)
		{
			say_file("digits/h-%da.wav", (int) y - (y % 100));
			say_file("digits/h-%da.wav", (int) y % 100);
		}
		// 1000 => thousand-ta.wav
		else if (y == 1000)
		{
			say_file("digits/thousand-ta.wav");
		}
		// 1001 - 1999 => thousand-u.wav + [1-9]00.wav + (h-1a.wav - h-99a.wav)
		else if (y >= 1001 && y <= 1999)
		{
			say_file("digits/thousand-u.wav");
			if (0 != (int) ((y - 1000) - ((y - 1000) % 100)))
				say_file("digits/h-%da.wav", (int) ((y - 1000) - ((y - 1000) % 100)));
				
			say_file("digits/h-%da.wav", (int) y % 100);
		}
		//2000  => 2je.wav + thousand-ta.wav
		else if (y == 2000)
		{
			say_file("digits/2je.wav");
			say_file("digits/thousand-ta.wav");
		}
		// 2001 - 2999 => 2je.wav + thousands-e.wav + [1-9]00.wav + (h-1a.wav - h-99a.wav)
		else if (y >= 2001 && y <= 2999)
		{
			say_file("digits/2je.wav");
			say_file("digits/thousands-e.wav");
			if (0 != (int) ((y - 2000) - ((y - 2000) % 100)))
				say_file("digits/h-%da.wav", (int) ((y - 2000) - ((y - 2000) % 100)));
				
			say_file("digits/h-%da.wav", (int) y % 100);
		}
		// 3000 => [3-9].wav + thousand-ta.wav
		else if (y >= 3000 && y <= 9000 && y % 1000 == 0)
		{
			say_file("digits/%d.wav", (int) (y / 1000));
			say_file("digits/thousand-ta.wav");
		}
		// [3-9]001 - [3-9]999 => [3-9].wav + thousands-e.wav + [1-9]00.wav + (h-1a.wav - h-99a.wav)
		else if (y >= 3000 && y <= 9000 && y % 1000 != 0)
		{
			say_file("digits/%d.wav", (int) (y / 1000));
			say_file("digits/thousands-e.wav");
			tis = y  - (y % 1000);
			if (0 != (int) ((y - tis) - ((y - tis) % 100)))
				say_file("digits/h-%da.wav",  (int) ((y - tis) - ((y - tis) % 100)));
				
			say_file("digits/h-%da.wav", (int) y % 100);
		}
		
		//say_num(tm.tm_year + 1900, SSM_COUNTED, "a");
	}

	
	if (say_time) 
	{
		say_num(tm.tm_hour, SSM_PRONOUNCED, "");
		
		sat_jedinica = tm.tm_hour % 10;
		
		if (tm.tm_hour >= 10)
			sat_desetinka = (int) (tm.tm_hour / 10);
			
	
		if (tm.tm_hour) 
		{
			if (sat_desetinka == 0 && sat_jedinica == 0)
			{
				nop;
			}
			else if (sat_desetinka == 1)
			{
				say_file("time/sati.wav");
			}
			else
			{
				switch(sat_jedinica)
				{
					case 1:
						say_file("time/sat.wav");
						break;
						
					case 2:
					case 3:
					case 4:
						say_file("time/sata.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_file("time/sati.wav");
						break;
					
				}
			}
		} 
		
		minuta_jedinica = tm.tm_min % 10;
		
		if (tm.tm_min >= 10)
			minuta_desetinka = (int) (tm.tm_min / 10);
			
		if (tm.tm_min) 
		{
			if (minuta_desetinka == 1)
			{
				say_num(tm.tm_min, SSM_PRONOUNCED, "");
				say_file("time/minuta.wav");
			}
			else
			{
				switch(minuta_jedinica)
				{
					case 2:
						say_num(tm.tm_min, SSM_PRONOUNCED, "je");
						say_file("time/minute.wav");
						break;
						
					case 3:
					case 4:
						say_num(tm.tm_min, SSM_PRONOUNCED, "");
						say_file("time/minute.wav");
						break;
					
					case 1:
						say_num(tm.tm_min, SSM_PRONOUNCED, "a");
						say_file("time/minuta.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_num(tm.tm_min, SSM_PRONOUNCED, "");
						say_file("time/minuta.wav");
						break;
				}
			}
		} 
		
		sekunda_jedinica = tm.tm_sec % 10;
		
		if (tm.tm_sec >= 10)
			sekunda_desetinka = (int) (tm.tm_sec / 10);
			
		if (tm.tm_sec) 
		{
			if (sekunda_desetinka == 1)
			{
				say_num(tm.tm_sec, SSM_PRONOUNCED, "");
				say_file("time/sekundi.wav");
			}
			else if (sekunda_desetinka == 0)
			{
				switch(sekunda_jedinica)
				{
					case 1:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "a");
						say_file("time/sekunda.wav");
						break;
						
					case 2:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "je");
						say_file("time/sekunde.wav");
						break;
						
					case 3:
					case 4:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "");
						say_file("time/sekunde.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_file("time/sekundi.wav");
						break;
				}
			}
			else
			{
				switch(sekunda_jedinica)
				{
					case 1:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "a");
						say_file("time/sekunda.wav");
						break;
						
					case 2:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "je");
						say_file("time/sekunde.wav");
						break;
						
					case 3:
					case 4:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "");
						say_file("time/sekunde.wav");
						break;
						
					case 0:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
						say_num(tm.tm_sec, SSM_PRONOUNCED, "");
						say_file("time/sekundi.wav");
						break;
				}
			}
		} 
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t hr_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char sbuf[16] = "";
	char *kuna = NULL;
	char *lipa = NULL;
	
	int zadnja_kuna = 0;
	int predzadnja_kuna = 0;
	int zadnja_lipa = 0;
	int predzadnja_lipa = 0;

	if (strlen(tosay) > 15 || !(tosay = strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) 
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	kuna = sbuf;
	
	if ((lipa = strchr(sbuf, '.'))) 
	{
		*lipa++ = '\0';
		if (strlen(lipa) > 2) 
		{
			lipa[2] = '\0';
		}
	}

	if (sbuf[0] == '+') 
	{
		kuna++;
	}

	if (sbuf[0] == '-') 
	{
		say_file("currency/negative.wav");
		kuna++;
	}

	if (kuna != NULL && strlen(kuna) > 0)
		zadnja_kuna = (int) ((char) *(kuna + strlen(kuna) - 1) - '0');
		
	if (kuna != NULL && strlen(kuna) > 1)
		predzadnja_kuna = (int) ((char) *(kuna + strlen(kuna) - 2) - '0');
		
		
	if (predzadnja_kuna == 1)
	{
		hr_say_count(session, "", kuna, say_args, args);
		say_file("currency/kuna.wav");
	}
	else
	{
		switch(zadnja_kuna)
		{
			case 1:
				hr_say_count(session, "a", kuna, say_args, args);
				say_file("currency/kuna.wav");
				break;

			case 0:				
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				hr_say_count(session, "", kuna, say_args, args);
				say_file("currency/kuna.wav");
				break;
				
			case 2:
				hr_say_count(session, "je", kuna, say_args, args);
				say_file("currency/kune.wav");
				break;
				
			case 3:
			case 4:
				hr_say_count(session, "", kuna, say_args, args);
				say_file("currency/kune.wav");
				break;
		}
	}
	
	if (lipa)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lipa:%s \n", lipa);
	else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lipa: NULL \n");
	
	if (lipa)
	{
		/* Say "and" */
		say_file("currency/and.wav");
	
		if (lipa != NULL && strlen(lipa) > 0)
			zadnja_lipa = (int) ((char) *(lipa + strlen(lipa) - 1) - '0');
		
		if (lipa != NULL && strlen(lipa) > 1)
			predzadnja_lipa = (int) ((char) *(lipa + strlen(lipa) - 2) - '0');
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "zadnja_lipa:%d \n", zadnja_lipa);	
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "predzadnja_lipa:%d \n", predzadnja_lipa);	
			
		if (predzadnja_lipa == 1)
		{
			hr_say_count(session, "", lipa, say_args, args);
			say_file("currency/lipa.wav");
		}
		else
		{
			switch(zadnja_lipa)
			{
				case 1:
					hr_say_count(session, "a", lipa, say_args, args);
					say_file("currency/lipa.wav");
					break;

				case 0:					
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
					hr_say_count(session, "", lipa, say_args, args);
					say_file("currency/lipa.wav");
					break;
					
				case 2:
					hr_say_count(session, "je", lipa, say_args, args);
					say_file("currency/lipa.wav");
					break;
					
				case 3:
				case 4:
					hr_say_count(session, "", lipa, say_args, args);
					say_file("currency/lipe.wav");
					break;
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}



static switch_status_t hr_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{

	switch_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = hr_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
	case SST_SHORT_DATE_TIME:
		say_cb = hr_say_time;
		break;
	case SST_IP_ADDRESS:
		say_cb = hr_ip;
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		say_cb = hr_spell;
		break;
	case SST_CURRENCY:
		say_cb = hr_say_money;
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


SWITCH_MODULE_LOAD_FUNCTION(mod_say_hr_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "hr";
	say_interface->say_function = hr_say;

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
