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
 *
 * mod_dialplan_asterisk.c -- Asterisk extensions.conf style dialplan parser.
 *
 */
#include <switch.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_asterisk_load);
SWITCH_MODULE_DEFINITION(mod_dialplan_asterisk, mod_dialplan_asterisk_load, NULL, NULL);

static switch_status_t exec_app(switch_core_session_t *session, char *app, char *arg)
{
	switch_application_interface_t *application_interface;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if ((application_interface = switch_loadable_module_get_application_interface(app))) {
		status = switch_core_session_exec(session, application_interface, arg);
		UNPROTECT_INTERFACE(application_interface);
	}

	return status;
}

SWITCH_STANDARD_APP(dial_function)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, '|', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
			goto error;
		}

		if (argc > 1) {
			switch_channel_set_variable(channel, "call_timeout", argv[1]);
		}

		switch_replace_char(argv[0], '&', ',', SWITCH_FALSE);

		if (exec_app(session, "bridge", argv[0]) != SWITCH_STATUS_SUCCESS) {
			goto error;
		}

		goto ok;
	}

  error:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");

  ok:

	return;
}

SWITCH_STANDARD_APP(avoid_function)
{
#if 0
	void *y = NULL;
#endif
	int x = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	for (x = 0; x < 5; x++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Avoiding initial deadlock on channel %s.\n", switch_channel_get_name(channel));
		switch_yield(100000);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "I should never be called!.\n");
#if 0
	memset((void *) y, 0, 1000);
#endif
}

SWITCH_STANDARD_APP(goto_function)
{
	int argc;
	char *argv[3] = { 0 };
	char *mydata;

	if (data && (mydata = switch_core_session_strdup(session, data))) {
		if ((argc = switch_separate_string(mydata, '|', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
			goto error;
		}

		switch_ivr_session_transfer(session, argv[1], "asterisk", argv[0]);
		goto ok;
	}

  error:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!\n");

  ok:

	return;
}

SWITCH_STANDARD_DIALPLAN(asterisk_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *cf = "extensions.conf";
	switch_config_t cfg;
	char *var, *val;
	const char *context = NULL;

	if (arg) {
		cf = arg;
	}

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	if (!caller_profile || zstr(caller_profile->destination_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
		return NULL;
	}

	context = caller_profile->context ? caller_profile->context : "default";

	if (!switch_config_open_file(&cfg, cf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return NULL;
	}

	while (switch_config_next_pair(&cfg, &var, &val)) {
		if (!strcasecmp(cfg.category, context)) {
			char *field_expanded = NULL;

			if (!strcasecmp(var, "include")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "param '%s' not implemented at line %d!\n", var, cfg.lineno);
			} else {
				int argc;
				char *argv[3] = { 0 };
				char *pattern = NULL;
				char *app = NULL;
				char *argument = NULL;
				char *expression = NULL, expression_buf[1024] = "";
				char substituted[2048] = "";
				const char *field_data = caller_profile->destination_number;
				int proceed = 0;
				switch_regex_t *re = NULL;
				int ovector[30] = { 0 };
				char *cid = NULL;

				expression = expression_buf;

				argc = switch_separate_string(val, ',', argv, (sizeof(argv) / sizeof(argv[0])));
				if (argc < 3) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse error line %d!\n", cfg.lineno);
					continue;
				}

				pattern = argv[0];

				if (!strcasecmp(var, "exten")) {
					char *p;
					if (pattern && (p = strchr(pattern, '/'))) {
						*p++ = '\0';
						cid = pattern;
						pattern = p;
					}
				} else {
					if (strchr(var, '$')) {
						if ((field_expanded = switch_channel_expand_variables(channel, var)) == var) {
							field_expanded = NULL;
							field_data = var;
						} else {
							field_data = field_expanded;
						}
					} else {
						field_data = switch_caller_get_field_by_name(caller_profile, var);
					}
				}

				if (pattern && (*pattern == '_' || *pattern == '~')) {
					if (*pattern == '_') {
						pattern++;
						if (switch_ast2regex(pattern, expression_buf, sizeof(expression_buf))) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Converting [%s] to real regex [%s] you should try them!\n",
											  pattern, expression_buf);
						}
					} else {
						pattern++;
						expression = pattern;
					}

					if (!field_data) {
						field_data = "";
					}

					if (!(proceed = switch_regex_perform(field_data, expression, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
						switch_regex_safe_free(re);
						switch_safe_free(field_expanded);
						continue;
					}
				} else {
					if (pattern && strcasecmp(pattern, field_data)) {
						continue;
					}
				}

				if (cid) {
					if (strcasecmp(cid, caller_profile->caller_id_number)) {
						continue;
					}
				}

				switch_channel_set_variable(channel, "EXTEN", caller_profile->destination_number);
				switch_channel_set_variable(channel, "CHANNEL", switch_channel_get_name(channel));
				switch_channel_set_variable(channel, "UNIQUEID", switch_core_session_get_uuid(session));

				//pri = argv[1];
				app = argv[2];

				if ((argument = strchr(app, '('))) {
					char *p;
					*argument++ = '\0';
					p = strrchr(argument, ')');
					if (p) {
						*p = '\0';
					}
				} else if ((argument = strchr(app, ','))) {
					*argument++ = '\0';
				}

				if (!argument) {
					argument = "";
				}

				if (!field_data) {
					field_data = "";
				}

				if (strchr(expression, '(')) {
					switch_perform_substitution(re, proceed, argument, field_data, substituted, sizeof(substituted), ovector);
					argument = substituted;
				}
				switch_regex_safe_free(re);

				if (!extension) {
					if (zstr(field_data)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "No extension!\n");
						break;
					}
					if ((extension = switch_caller_extension_new(session, field_data, field_data)) == 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
						break;
					}
				}

				switch_caller_extension_add_application(session, extension, app, argument);
			}

			switch_safe_free(field_expanded);
		}
	}

	switch_config_close_file(&cfg);

	return extension;
}

/* fake chan_sip */
switch_endpoint_interface_t *sip_endpoint_interface;
static switch_call_cause_t sip_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												switch_caller_profile_t *outbound_profile,
												switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												switch_call_cause_t *cancel_cause);
switch_io_routines_t sip_io_routines = {
	/*.outgoing_channel */ sip_outgoing_channel
};

static switch_call_cause_t sip_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												switch_caller_profile_t *outbound_profile,
												switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												switch_call_cause_t *cancel_cause)
{
	const char *profile;
	char *dup_profile = NULL;

	if (session) {
		profile = switch_channel_get_variable(switch_core_session_get_channel(session), "sip_profile");
	} else {
		dup_profile = switch_core_get_variable_dup("sip_profile");
		profile = dup_profile;
	}
	if (zstr(profile)) {
		profile = "default";
	}

	outbound_profile->destination_number = switch_core_sprintf(outbound_profile->pool, "%s/%s", profile, outbound_profile->destination_number);

	UNPROTECT_INTERFACE(sip_endpoint_interface);

	switch_safe_free(dup_profile);

	return switch_core_session_outgoing_channel(session, var_event, "sofia", outbound_profile, new_session, pool, SOF_NONE, cancel_cause);
}




/* fake chan_iax2 */
switch_endpoint_interface_t *iax2_endpoint_interface;
static switch_call_cause_t iax2_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause);
switch_io_routines_t iax2_io_routines = {
	/*.outgoing_channel */ iax2_outgoing_channel
};

static switch_call_cause_t iax2_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
												 switch_caller_profile_t *outbound_profile,
												 switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												 switch_call_cause_t *cancel_cause)
{
	UNPROTECT_INTERFACE(iax2_endpoint_interface);

	return switch_core_session_outgoing_channel(session, var_event, "iax", outbound_profile, new_session, pool, SOF_NONE, cancel_cause);
}


#define WE_DONT_NEED_NO_STINKIN_KEY "true"
static char *key()
{
	return WE_DONT_NEED_NO_STINKIN_KEY;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_dialplan_asterisk_load)
{
	switch_dialplan_interface_t *dp_interface;
	switch_application_interface_t *app_interface;
	char *mykey = NULL;
	int x = 0;

	if ((mykey = key())) {
		mykey = NULL;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	/* add a dialplan interface */
	SWITCH_ADD_DIALPLAN(dp_interface, "asterisk", asterisk_dialplan_hunt);

	/* a few fake apps for the sake of emulation */
	SWITCH_ADD_APP(app_interface, "Dial", "Dial", "Dial", dial_function, "Dial", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "Goto", "Goto", "Goto", goto_function, "Goto", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_APP(app_interface, "AvoidingDeadlock", "Avoid", "Avoid", avoid_function, "Avoid", SAF_SUPPORT_NOMEDIA);

	/* fake chan_sip facade */
	sip_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	sip_endpoint_interface->interface_name = "SIP";
	sip_endpoint_interface->io_routines = &sip_io_routines;

	/* fake chan_iax2 facade */
	iax2_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	iax2_endpoint_interface->interface_name = "IAX2";
	iax2_endpoint_interface->io_routines = &iax2_io_routines;

	if (getenv("FAITHFUL_EMULATION")) {
		for (x = 0; x < 10; x++) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Avoiding Deadlock.\n");
			switch_yield(100000);
		}
	}

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
