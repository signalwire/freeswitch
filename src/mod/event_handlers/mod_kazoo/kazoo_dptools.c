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
 * Karl Anderson <karl@2600hz.com>
 * Darren Schreiber <darren@2600hz.com>
 *
 *
 * kazoo_dptools.c -- clones of mod_dptools commands slightly modified for kazoo
 *
 */
#include "mod_kazoo.h"

#define SET_SHORT_DESC "Set a channel variable"
#define SET_LONG_DESC "Set a channel variable for the channel calling the application."
#define SET_SYNTAX "<varname>=<value>"

#define MULTISET_SHORT_DESC "Set many channel variables"
#define MULTISET_LONG_DESC "Set many channel variables for the channel calling the application"
#define MULTISET_SYNTAX "[^^<delim>]<varname>=<value> <var2>=<val2>"

#define UNSET_SHORT_DESC "Unset a channel variable"
#define UNSET_LONG_DESC "Unset a channel variable for the channel calling the application."
#define UNSET_SYNTAX "<varname>"

#define MULTIUNSET_SHORT_DESC "Unset many channel variables"
#define MULTIUNSET_LONG_DESC "Unset many channel variables for the channel calling the application."
#define MULTIUNSET_SYNTAX "[^^<delim>]<varname> <var2> <var3>"

#define EXPORT_SHORT_DESC "Export many channel variables"
#define EXPORT_LONG_DESC "Export many channel variables for the channel calling the application"
#define EXPORT_SYNTAX "[^^<delim>]<varname>=<value> <var2>=<val2>"

static void base_set (switch_core_session_t *session, const char *data, switch_stack_t stack) {
	char *var, *val = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *expanded = NULL;

		var = switch_core_session_strdup(session, data);

		if (!(val = strchr(var, '='))) {
			val = strchr(var, ',');
		}

		if (val) {
			*val++ = '\0';
			if (zstr(val)) {
				val = NULL;
			}
		}

		if (val) {
			expanded = switch_channel_expand_variables(channel, val);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s SET [%s]=[%s]\n", switch_channel_get_name(channel), var,
						  expanded ? expanded : "UNDEF");
		switch_channel_add_variable_var_check(channel, var, expanded, SWITCH_FALSE, stack);

		if (expanded && expanded != val) {
			switch_safe_free(expanded);
		}
	}
}

static void base_export (switch_core_session_t *session, const char *data, switch_stack_t stack) {
        char *var, *val = NULL;

        if (zstr(data)) {
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
        } else {
                switch_channel_t *channel = switch_core_session_get_channel(session);
                char *expanded = NULL;

                var = switch_core_session_strdup(session, data);

                if (!(val = strchr(var, '='))) {
                        val = strchr(var, ',');
                }

                if (val) {
                        *val++ = '\0';
                        if (zstr(val)) {
                                val = NULL;
                        }
                }

                if (val) {
                        expanded = switch_channel_expand_variables(channel, val);
                }

                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s EXPORT [%s]=[%s]\n", switch_channel_get_name(channel), var,
                                                  expanded ? expanded : "UNDEF");
		switch_channel_export_variable_var_check(channel, var, expanded, SWITCH_EXPORT_VARS_VARIABLE, SWITCH_FALSE);

                if (expanded && expanded != val) {
                        switch_safe_free(expanded);
                }
        }
}

SWITCH_STANDARD_APP(multiset_function) {
	char delim = ' ';
	char *arg = (char *) data;
	switch_event_t *event;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if (arg) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		char *array[256] = {0};
		int i, argc;

		arg = switch_core_session_strdup(session, arg);
		argc = switch_split(arg, delim, array);

		for(i = 0; i < argc; i++) {
			base_set(session, array[i], SWITCH_STACK_BOTTOM);
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}

	} else {
		base_set(session, data, SWITCH_STACK_BOTTOM);
	}
}

SWITCH_STANDARD_APP(set_function) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;

	base_set(session, data, SWITCH_STACK_BOTTOM);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_APP(unset_function) {
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *event;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "UNSET [%s]\n", (char *) data);
		switch_channel_set_variable(switch_core_session_get_channel(session), data, NULL);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}
}

SWITCH_STANDARD_APP(multiunset_function) {
	char delim = ' ';
	char *arg = (char *) data;

	if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
		arg += 2;
		delim = *arg++;
	}

	if (arg) {
		char *array[256] = {0};
		int i, argc;

		arg = switch_core_session_strdup(session, arg);
		argc = switch_split(arg, delim, array);

		for(i = 0; i < argc; i++) {
			switch_channel_set_variable(switch_core_session_get_channel(session), array[i], NULL);
		}

	} else {
		switch_channel_set_variable(switch_core_session_get_channel(session), arg, NULL);
	}
}

SWITCH_STANDARD_APP(export_function) {
        char delim = ' ';
        char *arg = (char *) data;

        if (!zstr(arg) && *arg == '^' && *(arg+1) == '^') {
                arg += 2;
                delim = *arg++;
        }

        if (arg) {
                char *array[256] = {0};
                int i, argc;

                arg = switch_core_session_strdup(session, arg);
                argc = switch_split(arg, delim, array);

                for(i = 0; i < argc; i++) {
                        base_export(session, array[i], SWITCH_STACK_BOTTOM);
                }
        } else {
                base_export(session, data, SWITCH_STACK_BOTTOM);
        }
}

void add_kz_dptools(switch_loadable_module_interface_t **module_interface, switch_application_interface_t *app_interface) {
	SWITCH_ADD_APP(app_interface, "kz_set", SET_SHORT_DESC, SET_LONG_DESC, set_function, SET_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_multiset", MULTISET_SHORT_DESC, MULTISET_LONG_DESC, multiset_function, MULTISET_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_unset", UNSET_SHORT_DESC, UNSET_LONG_DESC, unset_function, UNSET_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_APP(app_interface, "kz_multiunset", MULTISET_SHORT_DESC, MULTISET_LONG_DESC, multiunset_function, MULTIUNSET_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
        SWITCH_ADD_APP(app_interface, "kz_export", EXPORT_SHORT_DESC, EXPORT_LONG_DESC, export_function, EXPORT_SYNTAX,
                                   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
}
