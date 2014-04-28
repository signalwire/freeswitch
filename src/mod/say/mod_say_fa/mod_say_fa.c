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
 * Mahdi Moradi <m.moradi@sepanta.net>
 * Babak Yakhchali <b.yakhchali@sepanta.net>
 *
 * mod_say_fa.c -- Say for Persian
 *
 */

#include <switch.h>
#include <math.h>
#include <ctype.h>

void gregorian_to_jalali(/* output */ int *j_y, int *j_m, int *j_d,
                         /*  input */ int  g_y, int  g_m, int  g_d);
void jalali_to_gregorian(/* output */ int *g_y, int *g_m, int *g_d,
                         /*  input */ int  j_y, int  j_m, int  j_d);

SWITCH_MODULE_LOAD_FUNCTION(mod_say_fa_load);
SWITCH_MODULE_DEFINITION(mod_say_fa, mod_say_fa_load, NULL, NULL);

#define say_num(num, meth) {											\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = meth;			\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus =													\
			 fa_say_general_count(session, tmp, say_args, args))		\
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

static switch_status_t play_group(switch_say_method_t method, int a, int b, int c, char *what, switch_core_session_t *session, switch_input_args_t *args)
{
	if (a) {
		if ( !b && !c )
		{
			if( method == SSM_COUNTED )
			{
				say_file("digits/%d00om.wav", a);
			}
			else
				say_file("digits/%d00.wav", a);
			b = c = 0;
		}
		else
			say_file("digits/%d00+.wav", a);
	}

	if (b) {
		if ( !c || b == 1 )
		{
			if( method == SSM_COUNTED )
			{
				say_file("digits/%d%dom.wav", b, c);
			}
			else
				say_file("digits/%d%d.wav", b, c);
			c = 0;
		}
		else
			say_file("digits/%d0+.wav", b);
	}

	if (c) {
		if (method == SSM_COUNTED) {
			say_file("digits/%dom.wav", c);
		} else {
			say_file("digits/%d.wav", c);
		}
	}

	if (what && (a || b || c)) {
		say_file(what);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t fa_say_general_count(switch_core_session_t *session,	char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int in;
	int x = 0;
	int sum = 0;
	char what_file[50] = "";
	int places[9] = { 0 };
	char sbuf[128] = "";
	switch_status_t status;

	if (say_args->method == SSM_ITERATED) {
		if ((tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf)))) {
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

	if (!(tosay = switch_strip_commas(tosay, sbuf, sizeof(sbuf))) || strlen(tosay) > 9) {
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
		case SSM_COUNTED:
		case SSM_PRONOUNCED:
			sum = places[5] + places[4] + places[3] + places[2] + places[1] + places[0];
			if( !sum )
			{
				if( say_args->method == SSM_COUNTED )
				{
					strcpy(what_file,"digits/1000000om.wav");
				}
				else
					strcpy(what_file,"digits/1000000.wav");
			}
			else
				strcpy(what_file,"digits/1000000+.wav");
			if ((status = play_group(SSM_PRONOUNCED, places[8], places[7], places[6], what_file, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			sum = places[2] + places[1] + places[0];
			if( !sum )
			{	if( say_args->method == SSM_COUNTED )
				{
					strcpy(what_file,"digits/1000om.wav");
				}
				else
					strcpy(what_file,"digits/1000.wav");
			}
			else
				strcpy(what_file,"digits/1000+.wav");
			if ((status = play_group(SSM_PRONOUNCED, places[5], places[4], places[3], what_file, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
			}
			if ((status = play_group(say_args->method, places[2], places[1], places[0], NULL, session, args)) != SWITCH_STATUS_SUCCESS) {
				return status;
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

static switch_status_t fa_say_time(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	int32_t t;
	switch_time_t target = 0, target_now = 0;
	switch_time_exp_t tm, tm_now;
	uint8_t say_date = 0, say_time = 0, say_year = 0, say_month = 0, say_dow = 0, say_day = 0, say_yesterday = 0, say_today = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *tz = switch_channel_get_variable(channel, "timezone");
	int jalali_year = 1, jalali_month = 0, jalali_day = 0;

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
					hours = atoi(tme);
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

		if (hours) {
			say_num(hours, SSM_PRONOUNCED);
			if(minutes || seconds)
			{
				say_file("time/hour+.wav");
			}
			else
				say_file("time/hour.wav");
		}

		if (minutes) {
			say_num(minutes, SSM_PRONOUNCED);
			if( seconds )
			{
				say_file("time/minutes+.wav");
			}
			else
				say_file("time/minutes.wav");
		}

		if (seconds) {
			say_num(seconds, SSM_PRONOUNCED);
			say_file("time/seconds.wav");
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

	gregorian_to_jalali(&jalali_year,&jalali_month,&jalali_day,tm.tm_year + 1900,tm.tm_mon + 1,tm.tm_mday);

	if (say_day) {
		if(jalali_day > 20 && jalali_day != 30)
		{
			say_file("digits/%d+.wav", (jalali_day - jalali_day % 10));
			say_file("digits/%de.wav", jalali_day % 10);
		}
		else
			say_file("digits/%de.wav", jalali_day);
	}

	if (say_month) {
		say_file("time/mon-%d.wav", jalali_month - 1);
	}

	if (say_year) {
		say_num(jalali_year, SSM_PRONOUNCED);
	}

	if (say_time) {
		int32_t hour = tm.tm_hour, pm = 0;

		if (say_date || say_today || say_yesterday || say_dow) {
			say_file("time/at.wav");
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

		say_file("time/hour-e.wav");
		say_file("digits/%do.wav",hour);
		play_group(SSM_PRONOUNCED, 0, (tm.tm_min - tm.tm_min % 10) / 10, tm.tm_min % 10, "time/minutes-e.wav", session, args);
		say_file("time/%s.wav", pm ? "p-m" : "a-m");
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t fa_say_money(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char sbuf[16] = "";			/* enough for 999,999,999,999.99 (w/o the commas or leading $) */
	char *rials = NULL;
	char *cents = NULL;

	if (strlen(tosay) > 15 || !(tosay = switch_strip_nonnumerics(tosay, sbuf, sizeof(sbuf)))) {
		/* valid characters are 0 - 9, period (.), minus (-), and plus (+) - remove all others */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		return SWITCH_STATUS_GENERR;
	}

	rials = sbuf;

	if ((cents = strchr(sbuf, '.'))) {
		*cents++ = '\0';
		if (strlen(cents) > 2) {
			cents[2] = '\0';
		}
	}

	/* If positive sign - skip over" */
	if (sbuf[0] == '+') {
		say_file("currency/positive-e.wav");
		rials++;
	}

	/* If negative say "negative" */
	if (sbuf[0] == '-') {
		say_file("currency/negative-e.wav");
		rials++;
	}

	/* Say rial amount */
	fa_say_general_count(session, rials, say_args, args);
	say_file("currency/rials.wav");

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t fa_say_telephone(switch_core_session_t *session,	char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char *phone_number;
	int tosay_length = 0;

	if( !tosay || !strlen(tosay) )
	{
		return SWITCH_STATUS_SUCCESS;
	}

	if ((phone_number = strchr(tosay,'-')))
	{
		*phone_number++ = '\0';
		fa_say_telephone(session,tosay,say_args,args);
		fa_say_telephone(session,phone_number,say_args,args);
		return SWITCH_STATUS_SUCCESS;
	}

	tosay_length = strlen(tosay);
	if( tosay_length == 1 )
	{
		say_file("digits/%d.wav",tosay[0] - 48);
	}
	else if ( tosay[0] == '0' )
	{
		if( tosay[1] == '0' )
		{
			say_file("digits/00.wav");
			fa_say_telephone(session,tosay + 2,say_args,args);
		}
		else
		{
			say_file("digits/0.wav");
			fa_say_telephone(session,tosay + 1,say_args,args);
		}
	}
	else if ( tosay_length % 2 )
	{
		play_group(SSM_PRONOUNCED,tosay[0] - 48,tosay[1] - 48,tosay[2] - 48,NULL,session,args);
		fa_say_telephone(session,tosay + 3,say_args,args);
	}
	else
	{
		if ( tosay_length == 10 && (tosay[0] != '2' || tosay[1] != '1') )
		{
			play_group(SSM_PRONOUNCED,tosay[0] - 48,tosay[1] - 48,tosay[2] - 48,NULL,session,args);
			fa_say_telephone(session,tosay + 3,say_args,args);
		}
		else
		{
			play_group(SSM_PRONOUNCED,0,tosay[0] - 48,tosay[1] - 48,NULL,session,args);
			fa_say_telephone(session,tosay + 2,say_args,args);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t fa_say(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	switch_say_callback_t say_cb = NULL;

	switch (say_args->type) {
	case SST_NUMBER:
	case SST_ITEMS:
	case SST_PERSONS:
	case SST_MESSAGES:
		say_cb = fa_say_general_count;
		break;
	case SST_TIME_MEASUREMENT:
	case SST_CURRENT_DATE:
	case SST_CURRENT_TIME:
	case SST_CURRENT_DATE_TIME:
	case SST_SHORT_DATE_TIME:
		say_cb = fa_say_time;
		break;
	case SST_IP_ADDRESS:
		return switch_ivr_say_ip(session, tosay, fa_say_general_count, say_args, args);
		break;
	case SST_NAME_SPELLED:
	case SST_NAME_PHONETIC:
		return switch_ivr_say_spell(session, tosay, say_args, args);
		break;
	case SST_CURRENCY:
		say_cb = fa_say_money;
		break;
	case SST_TELEPHONE_NUMBER:
		say_cb = fa_say_telephone;
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

SWITCH_MODULE_LOAD_FUNCTION(mod_say_fa_load)
{
	switch_say_interface_t *say_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	say_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SAY_INTERFACE);
	say_interface->interface_name = "fa";
	say_interface->say_function = fa_say;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* Utility functions */

int g_days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int j_days_in_month[12] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29};

void gregorian_to_jalali(int *j_y, int *j_m, int *j_d,
						 int  g_y, int  g_m, int  g_d)
{
   int gy, gm, gd;
   int jy, jm, jd;
   long g_day_no, j_day_no;
   int j_np;
 
   int i;

   gy = g_y-1600;
   gm = g_m-1;
   gd = g_d-1;
 
   g_day_no = 365*gy+(gy+3)/4-(gy+99)/100+(gy+399)/400;
   for (i=0;i<gm;++i)
      g_day_no += g_days_in_month[i];
   if (gm>1 && ((gy%4==0 && gy%100!=0) || (gy%400==0)))
      /* leap and after Feb */
      ++g_day_no;
   g_day_no += gd;
 
   j_day_no = g_day_no-79;
 
   j_np = j_day_no / 12053;
   j_day_no %= 12053;
 
   jy = 979+33*j_np+4*(j_day_no/1461);
   j_day_no %= 1461;
 
   if (j_day_no >= 366) {
      jy += (j_day_no-1)/365;
      j_day_no = (j_day_no-1)%365;
   }
 
   for (i = 0; i < 11 && j_day_no >= j_days_in_month[i]; ++i) {
      j_day_no -= j_days_in_month[i];
   }
   jm = i+1;
   jd = j_day_no+1;
   *j_y = jy;
   *j_m = jm;
   *j_d = jd;
}

void jalali_to_gregorian(int *g_y, int *g_m, int *g_d,
						 int  j_y, int  j_m, int  j_d)
{
   int gy, gm, gd;
   int jy, jm, jd;
   long g_day_no, j_day_no;
   int leap;

   int i;

   jy = j_y-979;
   jm = j_m-1;
   jd = j_d-1;

   j_day_no = 365*jy + (jy/33)*8 + (jy%33+3)/4;
   for (i=0; i < jm; ++i)
      j_day_no += j_days_in_month[i];

   j_day_no += jd;

   g_day_no = j_day_no+79;

   gy = 1600 + 400*(g_day_no/146097); /* 146097 = 365*400 + 400/4 - 400/100 + 400/400 */
   g_day_no = g_day_no % 146097;

   leap = 1;
   if (g_day_no >= 36525) /* 36525 = 365*100 + 100/4 */
   {
      g_day_no--;
      gy += 100*(g_day_no/36524); /* 36524 = 365*100 + 100/4 - 100/100 */
      g_day_no = g_day_no % 36524;
      
      if (g_day_no >= 365)
         g_day_no++;
      else
         leap = 0;
   }

   gy += 4*(g_day_no/1461); /* 1461 = 365*4 + 4/4 */
   g_day_no %= 1461;

   if (g_day_no >= 366) {
      leap = 0;

      g_day_no--;
      gy += g_day_no/365;
      g_day_no = g_day_no % 365;
   }

   for (i = 0; g_day_no >= g_days_in_month[i] + (i == 1 && leap); i++)
      g_day_no -= g_days_in_month[i] + (i == 1 && leap);
   gm = i+1;
   gd = g_day_no+1;

   *g_y = gy;
   *g_m = gm;
   *g_d = gd;
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
