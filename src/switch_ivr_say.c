/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Michael Jerris <mike@jerris.com>
 *
 * switch_ivr_say.c -- IVR Library (functions to say audio in languages)
 *
 */

#include <switch.h>

static char *SAY_METHOD_NAMES[] = {
	"N/A",
	"PRONOUNCED",
	"ITERATED",
	"COUNTED",
	NULL
};

static char *SAY_TYPE_NAMES[] = {
	"NUMBER",
	"ITEMS",
	"PERSONS",
	"MESSAGES",
	"CURRENCY",
	"TIME_MEASUREMENT",
	"CURRENT_DATE",
	"CURRENT_TIME",
	"CURRENT_DATE_TIME",
	"TELEPHONE_NUMBER",
	"TELEPHONE_EXTENSION",
	"URL",
	"IP_ADDRESS",
	"EMAIL_ADDRESS",
	"POSTAL_ADDRESS",
	"ACCOUNT_NUMBER",
	"NAME_SPELLED",
	"NAME_PHONETIC",
	"SHORT_DATE_TIME",
	NULL
};

static char *SAY_GENDER_NAMES[] = {
	"MASCULINE",
	"FEMININE",
	"NEUTER",
	NULL
};

SWITCH_DECLARE(switch_say_gender_t) switch_ivr_get_say_gender_by_name(const char *name)
{
	int x = 0;

	if (!name) return (switch_say_gender_t)0;

	for (x = 0; SAY_GENDER_NAMES[x]; x++) {
		if (!strcasecmp(SAY_GENDER_NAMES[x], name)) {
			break;
		}
	}

	return (switch_say_gender_t) x;
}

SWITCH_DECLARE(switch_say_method_t) switch_ivr_get_say_method_by_name(const char *name)
{
	int x = 0;

	if (!name) return (switch_say_method_t)0;

	for (x = 0; SAY_METHOD_NAMES[x]; x++) {
		if (!strcasecmp(SAY_METHOD_NAMES[x], name)) {
			break;
		}
	}

	return (switch_say_method_t) x;
}

SWITCH_DECLARE(switch_say_type_t) switch_ivr_get_say_type_by_name(const char *name)
{
	int x = 0;

	if (!name) return (switch_say_type_t)0;

	for (x = 0; SAY_TYPE_NAMES[x]; x++) {
		if (!strcasecmp(SAY_TYPE_NAMES[x], name)) {
			break;
		}
	}

	return (switch_say_type_t) x;
}

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


SWITCH_DECLARE(switch_status_t) switch_ivr_say_spell(switch_core_session_t *session, char *tosay, switch_say_args_t *say_args, switch_input_args_t *args)
{
	char *p;

	for (p = tosay; p && *p; p++) {
		int a = tolower((int) *p);
		if (a >= '0' && a <= '9') {
			say_file("digits/%d.wav", a - '0');
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

#define say_num(num, meth) {											\
		char tmp[80];													\
		switch_status_t tstatus;										\
		switch_say_method_t smeth = say_args->method;					\
		switch_say_type_t stype = say_args->type;						\
		say_args->type = SST_ITEMS; say_args->method = meth;			\
		switch_snprintf(tmp, sizeof(tmp), "%u", (unsigned)num);			\
		if ((tstatus =													\
			 number_func(session, tmp, say_args, args))					\
			!= SWITCH_STATUS_SUCCESS) {									\
			return tstatus;												\
		}																\
		say_args->method = smeth; say_args->type = stype;				\
	}																	\

SWITCH_DECLARE(switch_status_t) switch_ivr_say_ip(switch_core_session_t *session,
												  char *tosay,
												  switch_say_callback_t number_func,
												  switch_say_args_t *say_args,
												  switch_input_args_t *args)
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

	say_num(atoi(a), say_args->method);
	say_file("digits/dot.wav");
	say_num(atoi(b), say_args->method);
	say_file("digits/dot.wav");
	say_num(atoi(c), say_args->method);
	say_file("digits/dot.wav");
	say_num(atoi(d), say_args->method);

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
