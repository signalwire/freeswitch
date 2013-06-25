/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_cluechoo.c -- Framework Demo Module
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cluechoo_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_cluechoo_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_cluechoo_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_cluechoo, mod_cluechoo_load, mod_cluechoo_shutdown, NULL);

int add_D51(int x);
int add_sl(int x);
int add_man(int y, int x);
int add_smoke(int y, int x);
int go(int i);
int vgo(int i, switch_core_session_t *session);

SWITCH_STANDARD_APP(cluechoo_app)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_channel_answer(channel);
	switch_ivr_sleep(session, 1000, SWITCH_FALSE, NULL);

	while (switch_channel_ready(channel)) {
		if (vgo(0, session) < 0) {
			break;
		}
	}
}

SWITCH_STANDARD_API(cluechoo_function)
{
	//stream->write_function(stream, "+OK Reloading\n");

	go(0);
	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_cluechoo_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_cluechoo_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	SWITCH_ADD_API(api_interface, "cluechoo", "Cluechoo API", cluechoo_function, "syntax");

	SWITCH_ADD_APP(app_interface, "cluechoo", "cluechoo", "cluechoo", cluechoo_app, "", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_cluechoo_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cluechoo_shutdown)
{

	return SWITCH_STATUS_SUCCESS;
}


/*========================================
 *    sl.c:
 *	Copyright 1993,1998 Toyoda Masashi 
 *		(toyoda@is.titech.ac.jp)
 *	Last Modified: 1998/ 7/22
 *========================================
 */
/* sl version 3.03 : add usleep(20000)                                       */
/*                                              by Toyoda Masashi 1998/ 7/22 */
/* sl version 3.02 : D51 flies! Change options.                              */
/*                                              by Toyoda Masashi 1993/ 1/19 */
/* sl version 3.01 : Wheel turns smoother                                    */
/*                                              by Toyoda Masashi 1992/12/25 */
/* sl version 3.00 : Add d(D51) option                                       */
/*                                              by Toyoda Masashi 1992/12/24 */
/* sl version 2.02 : Bug fixed.(dust remains in screen)                      */
/*                                              by Toyoda Masashi 1992/12/17 */
/* sl version 2.01 : Smoke run and disappear.                                */
/*                   Change '-a' to accident option.			     */
/*                                              by Toyoda Masashi 1992/12/16 */
/* sl version 2.00 : Add a(all),l(long),F(Fly!) options.                     */
/* 						by Toyoda Masashi 1992/12/15 */
/* sl version 1.02 : Add turning wheel.                                      */
/*					        by Toyoda Masashi 1992/12/14 */
/* sl version 1.01 : Add more complex smoke.                                 */
/*                                              by Toyoda Masashi 1992/12/14 */
/* sl version 1.00 : SL runs vomitting out smoke.                            */
/*						by Toyoda Masashi 1992/12/11 */

#include <curses.h>
#include <signal.h>
#include <unistd.h>
#include "sl.h"

int ACCIDENT = 0;
int LOGO = 0;
int FLY = 0;

int my_mvaddstr(int y, int x, char *str)
{
	for (; x < 0; ++x, ++str)
		if (*str == '\0')
			return ERR;
	for (; *str != '\0'; ++str, ++x)
		if (mvaddch(y, x, *str) == ERR)
			return ERR;
	return OK;
}

void option(char *str)
{
	extern int ACCIDENT, FLY;

	while (*str != '\0') {
		switch (*str++) {
		case 'a':
			ACCIDENT = 1;
			break;
		case 'F':
			FLY = 1;
			break;
		case 'l':
			LOGO = 1;
			break;
		default:
			break;
		}
	}
}

int go(int i)
{
	int x;
	int sleep_len = 40000;

	if (i > 0) {
		sleep_len = i;
	}

	/*
	   for (i = 1; i < argc; ++i) {
	   if (*argv[i] == '-') {
	   option(argv[i] + 1);
	   }
	   }
	 */
	initscr();
	signal(SIGINT, SIG_IGN);
	noecho();
	leaveok(stdscr, TRUE);
	scrollok(stdscr, FALSE);

	for (x = COLS - 1;; --x) {
		if (LOGO == 0) {
			if (add_D51(x) == ERR)
				break;
		} else {
			if (add_sl(x) == ERR)
				break;
		}
		refresh();
		if (x == COLS / 4) {
			sleep(2);
		} else {
			usleep(sleep_len);
		}
	}
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();

	return 0;
}


int vgo(int i, switch_core_session_t *session)
{
	int x;
	//int sleep_len = 40000;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t *read_frame;
	switch_status_t status;
	switch_codec_implementation_t read_impl = { 0 };
	switch_codec_t codec = { 0 };
	int hangover_hits = 0, hangunder_hits = 0;
	int diff_level = 400;
	int hangover = 40, hangunder = 15;
	int talking = 0;
	int energy_level = 500;
	int done = 0;
	switch_core_session_get_read_impl(session, &read_impl);

	printf("%s", SWITCH_SEQ_CLEARSCR);

	//if (i > 0) {
		//sleep_len = i;
	//}

	initscr();
	signal(SIGINT, SIG_IGN);
	noecho();
	leaveok(stdscr, TRUE);
	scrollok(stdscr, FALSE);


	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL, (int) read_impl.samples_per_second, read_impl.microseconds_per_packet / 1000,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
							   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}

	switch_core_session_set_read_codec(session, &codec);

	for (x = COLS - 1;; --x) {

		if (!done && !switch_channel_ready(channel)) {
			done = 1;
		}

		if (!done) {
			status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				done = 1;
			}
		}

		if (!done) {
			int16_t *fdata = (int16_t *) read_frame->data;
			uint32_t samples = read_frame->datalen / sizeof(*fdata);
			uint32_t score, count = 0, j = 0;
			double energy = 0;
			int divisor = 0;


			for (count = 0; count < samples; count++) {
				energy += abs(fdata[j]);
				j += read_impl.number_of_channels;
			}

			if (!(divisor = read_impl.actual_samples_per_second / 8000)) {
				divisor = 1;
			}

			score = (uint32_t) (energy / (samples / divisor));

			if (score > energy_level) {
				uint32_t diff = score - energy_level;
				if (hangover_hits) {
					hangover_hits--;
				}

				if (diff >= diff_level || ++hangunder_hits >= hangunder) {
					hangover_hits = hangunder_hits = 0;

					if (!talking) {
						talking = 1;
					}
				}
			} else {
				if (hangunder_hits) {
					hangunder_hits--;
				}
				if (talking) {
					if (++hangover_hits >= hangover) {
						hangover_hits = hangunder_hits = 0;
						talking = 0;
					}
				}
			}

			if (!talking) {
				x++;
				continue;
			}
		} else {
			usleep(20000);
		}

		if (LOGO == 0) {
			if (add_D51(x) == ERR)
				break;
		} else {
			if (add_sl(x) == ERR)
				break;
		}
		refresh();

		/*
		   if (x == COLS / 4) {
		   sleep(2);
		   } else {
		   usleep(sleep_len);
		   }
		 */
	}
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();

	switch_core_session_set_read_codec(session, NULL);
	switch_core_codec_destroy(&codec);
	return 0;
}


int add_sl(int x)
{
	static char *sl[LOGOPATTERNS][LOGOHIGHT + 1]
	= { {LOGO1, LOGO2, LOGO3, LOGO4, LWHL11, LWHL12, DELLN},
	{LOGO1, LOGO2, LOGO3, LOGO4, LWHL21, LWHL22, DELLN},
	{LOGO1, LOGO2, LOGO3, LOGO4, LWHL31, LWHL32, DELLN},
	{LOGO1, LOGO2, LOGO3, LOGO4, LWHL41, LWHL42, DELLN},
	{LOGO1, LOGO2, LOGO3, LOGO4, LWHL51, LWHL52, DELLN},
	{LOGO1, LOGO2, LOGO3, LOGO4, LWHL61, LWHL62, DELLN}
	};

	static char *coal[LOGOHIGHT + 1]
	= { LCOAL1, LCOAL2, LCOAL3, LCOAL4, LCOAL5, LCOAL6, DELLN };

	static char *car[LOGOHIGHT + 1]
	= { LCAR1, LCAR2, LCAR3, LCAR4, LCAR5, LCAR6, DELLN };

	int i, y, py1 = 0, py2 = 0, py3 = 0;

	if (x < -LOGOLENGTH)
		return ERR;
	y = LINES / 2 - 3;

	if (FLY == 1) {
		y = (x / 6) + LINES - (COLS / 6) - LOGOHIGHT;
		py1 = 2;
		py2 = 4;
		py3 = 6;
	}

	for (i = 0; i <= LOGOHIGHT; ++i) {
		my_mvaddstr(y + i, x, sl[(LOGOLENGTH + x) / 3 % LOGOPATTERNS][i]);
		my_mvaddstr(y + i + py1, x + 21, coal[i]);
		my_mvaddstr(y + i + py2, x + 42, car[i]);
		my_mvaddstr(y + i + py3, x + 63, car[i]);
	}
	if (ACCIDENT == 1) {
		add_man(y + 1, x + 14);
		add_man(y + 1 + py2, x + 45);
		add_man(y + 1 + py2, x + 53);
		add_man(y + 1 + py3, x + 66);
		add_man(y + 1 + py3, x + 74);
	}
	add_smoke(y - 1, x + LOGOFUNNEL);
	return OK;
}

static int loops = 0;

int add_D51(int x)
{
	static char *d51[D51PATTERNS][D51HIGHT + 1]
		= { {D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
			 D51WHL11, D51WHL12, D51WHL13, D51DEL},
	{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
	 D51WHL21, D51WHL22, D51WHL23, D51DEL},
	{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
	 D51WHL31, D51WHL32, D51WHL33, D51DEL},
	{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
	 D51WHL41, D51WHL42, D51WHL43, D51DEL},
	{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
	 D51WHL51, D51WHL52, D51WHL53, D51DEL},
	{D51STR1, D51STR2, D51STR3, D51STR4, D51STR5, D51STR6, D51STR7,
	 D51WHL61, D51WHL62, D51WHL63, D51DEL}
	};
	static char *coal[D51HIGHT + 1]
		= { COAL01, COAL02, COAL03, COAL04, COAL05,
		COAL06, COAL07, COAL08, COAL09, COAL10, COALDEL
	};

	static char *acoal[D51HIGHT + 1]
		= { COAL01, COAL02, COAL03, COAL04, COAL5A,
		COAL06, COAL07, COAL08, COAL09, COAL10, COALDEL
	};

	int y, i, dy = 0;

	if (x < -D51LENGTH)
		return ERR;
	y = LINES / 2 - 5;

	if (FLY == 1) {
		y = (x / 7) + LINES - (COLS / 7) - D51HIGHT;
		dy = 1;
	}

	for (i = 0; i <= D51HIGHT; ++i) {
		my_mvaddstr(y + i, x, d51[(D51LENGTH + x) % D51PATTERNS][i]);
		my_mvaddstr(y + i + dy, x + 53, loops > 60 ? coal[i] : acoal[i]);
		loops++;
		if (loops == 500)
			loops = -100;
	}
	if (ACCIDENT == 1) {
		add_man(y + 2, x + 43);
		add_man(y + 2, x + 47);
	}
	add_smoke(y - 1, x + D51FUNNEL);
	return OK;
}


int add_man(int y, int x)
{
	static char *man[2][2] = { {"", "(O)"}, {"Help!", "\\O/"} };
	int i;

	for (i = 0; i < 2; ++i) {
		my_mvaddstr(y + i, x, man[(LOGOLENGTH + x) / 12 % 2][i]);
	}

	return 0;
}


int add_smoke(int y, int x)
#define SMOKEPTNS	16
{
	static struct smokes {
		int y, x;
		int ptrn, kind;
	} S[1000];
	static int sum = 0;
	static char *Smoke[2][SMOKEPTNS]
		= { {"(   )", "(    )", "(    )", "(   )", "(  )",
			 "(  )", "( )", "( )", "()", "()",
			 "O", "O", "O", "O", "O",
			 " "},
	{"(@@@)", "(@@@@)", "(@@@@)", "(@@@)", "(@@)",
	 "(@@)", "(@)", "(@)", "@@", "@@",
	 "@", "@", "@", "@", "@",
	 " "}
	};
	static char *Eraser[SMOKEPTNS]
		= { "     ", "      ", "      ", "     ", "    ",
		"    ", "   ", "   ", "  ", "  ",
		" ", " ", " ", " ", " ",
		" "
	};
	static int dy[SMOKEPTNS] = { 2, 1, 1, 1, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0
	};
	static int dx[SMOKEPTNS] = { -2, -1, 0, 1, 1, 1, 1, 1, 2, 2,
		2, 2, 2, 3, 3, 3
	};
	int i;

	if (x % 4 == 0) {
		for (i = 0; i < sum; ++i) {
			my_mvaddstr(S[i].y, S[i].x, Eraser[S[i].ptrn]);
			S[i].y -= dy[S[i].ptrn];
			S[i].x += dx[S[i].ptrn];
			S[i].ptrn += (S[i].ptrn < SMOKEPTNS - 1) ? 1 : 0;
			my_mvaddstr(S[i].y, S[i].x, Smoke[S[i].kind][S[i].ptrn]);
		}
		my_mvaddstr(y, x, Smoke[sum % 2][0]);
		S[sum].y = y;
		S[sum].x = x;
		S[sum].ptrn = 0;
		S[sum].kind = sum % 2;
		sum++;
	}

	return 0;
}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
