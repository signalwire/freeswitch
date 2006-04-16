/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * mod_pcre.c -- Regex Dialplan Module
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcre.h>

static const char modname[] = "mod_pcre";

#define cleanre()	if (re) {\
				pcre_free(re);\
				re = NULL;\
			}

static switch_caller_extension *dialplan_hunt(switch_core_session *session)
{
	switch_caller_profile *caller_profile;
	switch_caller_extension *extension = NULL;
	switch_channel *channel;
	char *cf = "regextensions.conf";
	switch_config cfg;
	char *var, *val;
	char app[1024] = "";
	int catno = -1;
	char *exten_name = NULL;
	pcre *re = NULL;
	int match_count = 0;
	int ovector[30];
	int skip = 0;

	channel = switch_core_session_get_channel(session);
	caller_profile = switch_channel_get_caller_profile(channel);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Processing %s->%s!\n", caller_profile->caller_id_name,
						  caller_profile->destination_number);

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		switch_channel_hangup(channel);
		return NULL;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (cfg.catno != catno) {	/* new category */
			catno = cfg.catno;
			exten_name = cfg.category;
			cleanre();
			match_count = 0;
			skip = 0;

			if (!strcasecmp(exten_name, "outbound") && !switch_channel_test_flag(channel, CF_OUTBOUND)) {
				skip = 1;
			} else if (!strcasecmp(exten_name, "inbound") && switch_channel_test_flag(channel, CF_OUTBOUND)) {
				skip = 1;
			}
		}

		if (skip) {
			continue;
		}

		if (!strcasecmp(var, "regex")) {
			const char *error = NULL;
			int erroffset = 0;

			cleanre();
			re = pcre_compile(val,	/* the pattern */
							  0,	/* default options */
							  &error,	/* for error message */
							  &erroffset,	/* for error offset */
							  NULL);	/* use default character tables */
			if (error) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "COMPILE ERROR: %d [%s]\n", erroffset, error);
				cleanre();
				switch_channel_hangup(channel);
				return NULL;
			}

			match_count = pcre_exec(re,	/* result of pcre_compile() */
									NULL,	/* we didn't study the pattern */
									caller_profile->destination_number,	/* the subject string */
									(int) strlen(caller_profile->destination_number),	/* the length of the subject string */
									0,	/* start at offset 0 in the subject */
									0,	/* default options */
									ovector,	/* vector of integers for substring information */
									sizeof(ovector) / sizeof(ovector[0]));	/* number of elements (NOT size in bytes) */
		} else if (match_count > 0 && !strcasecmp(var, "match")) {
			if (!re) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: match without regex in %s line %d\n", cfg.path,
									  cfg.lineno);
				continue;
			} else {
				char newval[1024] = "";
				char index[10] = "";
				char replace[128] = "";
				unsigned int x, y = 0, z = 0, num = 0;
				char *data;

				for (x = 0; x < sizeof(newval) && x < strlen(val);) {
					if (val[x] == '$') {
						x++;

						while (val[x] > 47 && val[x] < 58) {
							index[z++] = val[x];
							x++;
						}
						index[z++] = '\0';
						z = 0;
						num = atoi(index);

						if (pcre_copy_substring
							(caller_profile->destination_number, ovector, match_count, num, replace,
							 sizeof(replace)) > 0) {
							unsigned int r;
							for (r = 0; r < strlen(replace); r++) {
								newval[y++] = replace[r];
							}
						}
					} else {
						newval[y++] = val[x];
						x++;
					}
				}
				newval[y++] = '\0';

				memset(app, 0, sizeof(app));
				switch_copy_string(app, newval, sizeof(app));

				if ((data = strchr(app, ' ')) != 0) {
					*data = '\0';
					data++;
				} 

				if (!extension) {
					if ((extension =
						 switch_caller_extension_new(session, exten_name, caller_profile->destination_number)) == 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
						break;
					}
				}

				switch_caller_extension_add_application(session, extension, app, data);
			}
		}
	}

	switch_config_close_file(&cfg);

	if (extension) {
		switch_channel_set_state(channel, CS_EXECUTE);
	} else {
		switch_channel_hangup(channel);
	}

	cleanre();
	return extension;
}


static const switch_dialplan_interface dialplan_interface = {
	/*.interface_name = */ "pcre",
	/*.hunt_function = */ dialplan_hunt
		/*.next = NULL */
};

static const switch_loadable_module_interface dialplan_module_interface = {
	/*.module_name = */ modname,
	/*.endpoint_interface = */ NULL,
	/*.timer_interface = */ NULL,
	/*.dialplan_interface = */ &dialplan_interface,
	/*.codec_interface = */ NULL,
	/*.application_interface = */ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &dialplan_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
