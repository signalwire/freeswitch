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

#define CONFIG_FILE "/etc/miniphone.conf"
#define USER_FILE "%s/.miniphone-conf"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

char regpeer[256];
char regsecret[256];
char server[256];
int refresh = 60;
char context[256];
char language[256];

#define TYPE_STRING 0
#define TYPE_INT    1

struct opt {
	char *name;
	void *where;
	int len;
	int type;
};

static struct opt opts[] =  {
	{ "regpeer", regpeer, sizeof(regpeer), TYPE_STRING },
	{ "regsecret", regsecret, sizeof(regsecret), TYPE_STRING },
	{ "server", server, sizeof(server), TYPE_STRING },
	{ "context", context, sizeof(context), TYPE_STRING },
	{ "language", language, sizeof(language), TYPE_STRING },
	{ "refresh", &refresh, sizeof(refresh), TYPE_INT },
};

static int __load_options(char *filename)
{
	FILE *f;
	int lineno = 0;
	char buf[256];
	char *var, *value;
	int x;
	char *c;
	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Failed to open '%s': %s\n", filename, strerror(errno));
		return -1;
	}
	while(!feof(f)) {
		fgets(buf, sizeof(buf), f);
		if (!feof(f)) {
			/* Ditch comments */
			if ((c = strchr(buf, '#')))
				*c = 0;
			lineno++;
			/* Strip CR */
			buf[strlen(buf)-1] = '\0';
			if (strlen(buf)) {
				var = strtok(buf, "=");
				value = strtok(NULL, "=");
				if (!var || !value) {
					fprintf(stderr, "Syntax error line %d\n", lineno);
					continue;
				}
				for (x=0;x<sizeof(opts) / sizeof(opts[0]); x++) {
					if (!strcasecmp(var, opts[x].name)) {
						switch(opts[x].type) {
						case TYPE_STRING:
							strncpy((char *)opts[x].where, value, opts[x].len);
							break;
						case TYPE_INT:
							if (sscanf(value, "%i", (int *)opts[x].where) != 1) {
								fprintf(stderr, "Not a number at line %d\n", lineno);
							}
							break;
						default:
							fprintf(stderr, "Don't know what to do about type %d\n", opts[x].type);
						}
						break;
					}
				}
				if (!(x < sizeof(opts) / sizeof(opts[0]))) {
					fprintf(stderr, "Dunno keyword '%s'\n", var);
					continue;
				}
			}
		}
	}
	fclose(f);
	return 0;
}

int load_options(void)
{
	char fn[256];
	__load_options(CONFIG_FILE);
	if (getenv("HOME")) {
		snprintf(fn, sizeof(fn), USER_FILE, getenv("HOME"));
		__load_options(fn);
	}
	return 0;
}

static int __save_options(char *filename)
{
	FILE *f;
	f = fopen(filename, "w");
	if (!f) {
		fprintf(stderr, "Failed to open '%s': %s\n", filename, strerror(errno));
		return -1;
	}
	fclose(f);
	return 0;
}

int save_options(void)
{
	char fn[256];
	if (getenv("HOME")) {
		snprintf(fn, sizeof(fn), USER_FILE, getenv("HOME"));
		return __save_options(fn);
	} else
		return __save_options(CONFIG_FILE);
	
}
