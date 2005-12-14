/*
 * Snomphone: IAX software for SNOM 100 Phone
 *
 * IAX Support for talking to Asterisk and other Gnophone clients
 *
 * Copyright (C) 1999, Linux Support Services, Inc.
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

extern char regpeer[256];
extern char regsecret[256];
extern char regpeer[256];
extern char server[256];
extern int refresh;
extern char context[256];
extern char language[256];

int save_options(void);
int load_options(void);
