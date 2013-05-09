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
 * switch_ivr_menu.c -- IVR Library (menu code)
 *
 */

#include <switch.h>

struct switch_ivr_menu_action;

struct switch_ivr_menu {
	char *name;
	char *greeting_sound;
	char *short_greeting_sound;
	char *invalid_sound;
	char *exit_sound;
	char *buf;
	char *ptr;
	char *confirm_macro;
	char *confirm_key;
	char *tts_engine;
	char *tts_voice;
	int confirm_attempts;
	int digit_len;
	int max_failures;
	int max_timeouts;
	int timeout;
	int inter_timeout;
	char *exec_on_max_fail;
	char *exec_on_max_timeout;
	switch_size_t inlen;
	uint32_t flags;
	struct switch_ivr_menu_action *actions;
	struct switch_ivr_menu *next;
	switch_memory_pool_t *pool;
	int stack_count;
	char *pin;
	char *prompt_pin_file;
	char *bad_pin_file;
};

struct switch_ivr_menu_action {
	switch_ivr_menu_action_function_t *function;
	switch_ivr_action_t ivr_action;
	char *arg;
	char *bind;
	int re;
	struct switch_ivr_menu_action *next;
};

static switch_ivr_menu_t *switch_ivr_menu_find(switch_ivr_menu_t *stack, const char *name)
{
	switch_ivr_menu_t *ret;
	for (ret = stack; ret; ret = ret->next) {
		if (!name || !strcmp(ret->name, name))
			break;
	}
	return ret;
}

static void switch_ivr_menu_stack_add(switch_ivr_menu_t ** top, switch_ivr_menu_t *bottom)
{
	switch_ivr_menu_t *ptr;

	for (ptr = *top; ptr && ptr->next; ptr = ptr->next);

	if (ptr) {
		ptr->next = bottom;
	} else {
		*top = bottom;
	}

}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_init(switch_ivr_menu_t ** new_menu,
													 switch_ivr_menu_t *main,
													 const char *name,
													 const char *greeting_sound,
													 const char *short_greeting_sound,
													 const char *invalid_sound,
													 const char *exit_sound,
													 const char *confirm_macro,
													 const char *confirm_key,
													 const char *tts_engine,
													 const char *tts_voice,
													 int confirm_attempts,
													 int inter_timeout,
													 int digit_len, int timeout, int max_failures, int max_timeouts, switch_memory_pool_t *pool)
{
	switch_ivr_menu_t *menu;
	uint8_t newpool = 0;

	if (!pool) {
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
			return SWITCH_STATUS_MEMERR;
		}
		newpool = 1;
	}

	if (!(menu = switch_core_alloc(pool, sizeof(*menu)))) {
		if (newpool) {
			switch_core_destroy_memory_pool(&pool);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			return SWITCH_STATUS_MEMERR;
		}
	}

	menu->pool = pool;

	if (!confirm_attempts) {
		confirm_attempts = 3;
	}

	if (!inter_timeout) {
		inter_timeout = timeout / 2;
	}

	if (!zstr(name)) {
		menu->name = switch_core_strdup(menu->pool, name);
	}

	if (!zstr(greeting_sound)) {
		menu->greeting_sound = switch_core_strdup(menu->pool, greeting_sound);
	}

	if (!zstr(short_greeting_sound)) {
		menu->short_greeting_sound = switch_core_strdup(menu->pool, short_greeting_sound);
	}

	if (!zstr(invalid_sound)) {
		menu->invalid_sound = switch_core_strdup(menu->pool, invalid_sound);
	}

	if (!zstr(exit_sound)) {
		menu->exit_sound = switch_core_strdup(menu->pool, exit_sound);
	}

	if (!zstr(confirm_key)) {
		menu->confirm_key = switch_core_strdup(menu->pool, confirm_key);
	}

	if (!zstr(confirm_macro)) {
		menu->confirm_macro = switch_core_strdup(menu->pool, confirm_macro);
	}

	if (!zstr(tts_engine)) {
		menu->tts_engine = switch_core_strdup(menu->pool, tts_engine);
	}

	if (!zstr(tts_voice)) {
		menu->tts_voice = switch_core_strdup(menu->pool, tts_voice);
	}

	menu->confirm_attempts = confirm_attempts;

	menu->inlen = digit_len;

	if (max_failures > 0) {
		menu->max_failures = max_failures;
	} else {
		menu->max_failures = 3;
	}

	if (max_timeouts > 0) {
		menu->max_timeouts = max_timeouts;
	} else {
		menu->max_timeouts = 3;
	}

	menu->timeout = timeout;

	menu->inter_timeout = inter_timeout;

	menu->actions = NULL;

	if (newpool) {
		switch_set_flag(menu, SWITCH_IVR_MENU_FLAG_FREEPOOL);
	}

	if (menu->timeout <= 0) {
		menu->timeout = 10000;
	}

	if (main) {
		switch_ivr_menu_stack_add(&main, menu);
	} else {
		switch_set_flag(menu, SWITCH_IVR_MENU_FLAG_STACK);
	}

	menu->buf = switch_core_alloc(menu->pool, 1024);

	*new_menu = menu;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_action(switch_ivr_menu_t *menu, switch_ivr_action_t ivr_action, const char *arg, const char *bind)
{
	switch_ivr_menu_action_t *action, *ap;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = switch_core_strdup(menu->pool, bind);
		action->arg = switch_core_strdup(menu->pool, arg);
		if (*action->bind == '/') {
			action->re = 1;
		} else {
			len = (uint32_t) strlen(action->bind);
			if (len > menu->inlen) {
				menu->inlen = len;
			}
		}
		action->ivr_action = ivr_action;

		if (menu->actions) {
			for(ap = menu->actions; ap && ap->next; ap = ap->next);
			ap->next = action;
		} else {
		menu->actions = action;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_function(switch_ivr_menu_t *menu,
															  switch_ivr_menu_action_function_t *function, const char *arg, const char *bind)
{
	switch_ivr_menu_action_t *action, *ap;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = switch_core_strdup(menu->pool, bind);
		action->arg = switch_core_strdup(menu->pool, arg);

		if (*action->bind == '/') {
			action->re = 1;
		} else {
			len = (uint32_t) strlen(action->bind);
			if (len > menu->inlen) {
				menu->inlen = len;
			}
		}

		action->function = function;
		
		if (menu->actions) {
			for(ap = menu->actions; ap && ap->next; ap = ap->next);
			ap->next = action;
		} else {
		menu->actions = action;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_free(switch_ivr_menu_t *stack)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (stack != NULL && stack->pool != NULL) {
		if (switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_STACK)
			&& switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_FREEPOOL)) {
			switch_memory_pool_t *pool = stack->pool;
			status = switch_core_destroy_memory_pool(&pool);
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

static switch_status_t play_and_collect(switch_core_session_t *session, switch_ivr_menu_t *menu, char *sound, switch_size_t need)
{
	char terminator;
	uint32_t len;
	char *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_input_args_t args = { 0 };
	switch_channel_t *channel;
	char *sound_expanded = sound;
	switch_size_t menu_buf_len = 0;
	const char *terminator_str = "#";

	if (!session || !menu || zstr(sound)) {
		return status;
	}

	if ((channel = switch_core_session_get_channel(session))) {
		const char *tmp;
		sound_expanded = switch_channel_expand_variables(channel, sound);
		if ((tmp = switch_channel_get_variable(channel, "ivr_menu_terminator")) && !zstr(tmp)) {
			terminator_str = tmp;
		}
	}

	memset(menu->buf, 0, menu->inlen + 1);
	menu->ptr = menu->buf;

	if (!need) {
		len = 1;
		ptr = NULL;
	} else {
		len = (uint32_t) menu->inlen + 1;
		ptr = menu->ptr;
	}
	args.buf = ptr;
	args.buflen = len;

	status = switch_ivr_play_file(session, NULL, sound_expanded, &args);

	if (sound_expanded != sound) {
		switch_safe_free(sound_expanded);
	}

	if (!need) {
		return status;
	}

	menu_buf_len = strlen(menu->buf);

	menu->ptr += menu_buf_len;
	if (menu_buf_len < need) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "waiting for %u/%u digits t/o %d\n",
						  (uint32_t) (menu->inlen - strlen(menu->buf)), (uint32_t) need, menu->inter_timeout);
		status = switch_ivr_collect_digits_count(session, menu->ptr, menu->inlen - strlen(menu->buf),
												 need, terminator_str, &terminator, menu_buf_len ? menu->inter_timeout : menu->timeout,
												 menu->inter_timeout, menu->timeout);
	}

	if (menu->confirm_macro && status == SWITCH_STATUS_SUCCESS && *menu->buf != '\0') {
		switch_input_args_t confirm_args = { 0 }, *ap = NULL;
		char buf[10] = "";
		char terminator_key;
		int att = menu->confirm_attempts;

		while (att) {
			confirm_args.buf = buf;
			confirm_args.buflen = sizeof(buf);
			memset(buf, 0, confirm_args.buflen);

			if (menu->confirm_key) {
				ap = &confirm_args;
			}

			switch_ivr_phrase_macro(session, menu->confirm_macro, menu->buf, NULL, ap);

			if (menu->confirm_key && *buf == '\0') {
				switch_ivr_collect_digits_count(session, buf, sizeof(buf), 1, terminator_str, &terminator_key, menu->timeout, 0, 0);
			}

			if (menu->confirm_key && *buf != '\0') {
				if (*menu->confirm_key == *buf) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "approving digits '%s' via confirm key %s\n", menu->buf, menu->confirm_key);
					break;
				} else {
					att = 0;
					break;
				}
			}
			att--;
		}
		if (!att) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rejecting digits '%s' via confirm key %s\n", menu->buf,
							  menu->confirm_key);
			*menu->buf = '\0';
		}
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "digits '%s'\n", menu->buf);

	return status;
}

static void exec_app(switch_core_session_t *session, char *app_str)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *app = switch_core_session_strdup(session, app_str);
	char *data = strchr(app, ' ');
	char *expanded = NULL;
	
	if (data) {
		*data++ = '\0';
	}
	
	expanded = switch_channel_expand_variables(channel, data);
	
	switch_core_session_execute_application(session, app, expanded);
	
	if (expanded && expanded != data) {
		free(expanded);
	}

}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_execute(switch_core_session_t *session, switch_ivr_menu_t *stack, char *name, void *obj)
{
	int reps = 0, errs = 0, timeouts = 0, match = 0, running = 1;
	char *greeting_sound = NULL, *aptr = NULL;
	char arg[512];
	switch_ivr_action_t todo = SWITCH_IVR_ACTION_DIE;
	switch_ivr_menu_action_t *ap;
	switch_ivr_menu_t *menu;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (++stack->stack_count > 12) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Too many levels of recursion.\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (!session || !stack || zstr(name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid menu context\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	channel = switch_core_session_get_channel(session);

	if (!(menu = switch_ivr_menu_find(stack, name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Menu!\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (!zstr(menu->tts_engine) && !zstr(menu->tts_voice)) {
		switch_channel_set_variable(channel, "tts_engine", menu->tts_engine);
		switch_channel_set_variable(channel, "tts_voice", menu->tts_voice);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Executing IVR menu %s\n", menu->name);
	switch_channel_set_variable(channel, "ivr_menu_status", "success");

	if (!zstr(menu->pin)) {
		char digit_buffer[128] = "";
		char *digits_regex = switch_core_session_sprintf(session, "^%s$", menu->pin);

		switch_play_and_get_digits(session, strlen(menu->pin), strlen(menu->pin), 3, 3000, "#",
								   menu->prompt_pin_file, menu->bad_pin_file, NULL, digit_buffer, sizeof(digit_buffer), 
								   digits_regex, 10000, NULL);		
	}


	for (reps = 0; running && status == SWITCH_STATUS_SUCCESS; reps++) {
		if (!switch_channel_ready(channel)) {
			break;
		}
		if (errs == menu->max_failures) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Maximum failures\n");
			switch_channel_set_variable(channel, "ivr_menu_status", "failure");
			if (!zstr(menu->exec_on_max_fail)) {
				exec_app(session, menu->exec_on_max_fail);
			}
			break;
		}
		if (timeouts == menu->max_timeouts) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Maximum timeouts\n");
			switch_channel_set_variable(channel, "ivr_menu_status", "timeout");
			if (!zstr(menu->exec_on_max_timeout)) {
				exec_app(session, menu->exec_on_max_timeout);
			}
			break;
		}

		if (reps > 0 && menu->short_greeting_sound) {
			greeting_sound = menu->short_greeting_sound;
		} else {
			greeting_sound = menu->greeting_sound;
		}

		match = 0;
		aptr = NULL;

		memset(arg, 0, sizeof(arg));

		memset(menu->buf, 0, menu->inlen + 1);

		if (play_and_collect(session, menu, greeting_sound, menu->inlen) == SWITCH_STATUS_TIMEOUT && *menu->buf == '\0') {
			timeouts++;
			continue;
		}

		if (*menu->buf != '\0') {

			for (ap = menu->actions; ap; ap = ap->next) {
				int ok = 0;
				char substituted[1024];
				char *use_arg = ap->arg;

				if (!zstr(menu->tts_engine) && !zstr(menu->tts_voice)) {
					switch_channel_set_variable(channel, "tts_engine", menu->tts_engine);
					switch_channel_set_variable(channel, "tts_voice", menu->tts_voice);
				}

				if (ap->re) {
					switch_regex_t *re = NULL;
					int ovector[30];

					if ((ok = switch_regex_perform(menu->buf, ap->bind, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
						switch_perform_substitution(re, ok, ap->arg, menu->buf, substituted, sizeof(substituted), ovector);
						use_arg = substituted;
					}
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "action regex [%s] [%s] [%d]\n", menu->buf, ap->bind, ok);

					switch_regex_safe_free(re);
				} else {
					ok = !strcmp(menu->buf, ap->bind);
				}

				if (ok) {
					match++;
					errs = 0;
					if (ap->function) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "IVR function on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf, use_arg);
						todo = ap->function(menu, use_arg, arg, sizeof(arg), obj);
						aptr = arg;
					} else {
						todo = ap->ivr_action;
						aptr = use_arg;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
										  "IVR action on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf, aptr);
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "switch_ivr_menu_execute todo=[%d]\n", todo);

					switch (todo) {
					case SWITCH_IVR_ACTION_DIE:
						status = SWITCH_STATUS_FALSE;
						break;
					case SWITCH_IVR_ACTION_PLAYSOUND:
						status = switch_ivr_play_file(session, NULL, aptr, NULL);
						break;
					case SWITCH_IVR_ACTION_EXECMENU:
						if (!strcmp(aptr, menu->name)) {
							status = SWITCH_STATUS_SUCCESS;
						} else {
							reps = -1;
							status = switch_ivr_menu_execute(session, stack, aptr, obj);
						}
						break;
					case SWITCH_IVR_ACTION_EXECAPP:
						{
							switch_application_interface_t *application_interface;
							char *app_name;
							char *app_arg = NULL;

							status = SWITCH_STATUS_FALSE;

							if (!zstr(aptr)) {
								app_name = switch_core_session_strdup(session, aptr);
								if ((app_arg = strchr(app_name, ' '))) {
									*app_arg++ = '\0';
								}

								if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
									switch_core_session_exec(session, application_interface, app_arg);
									UNPROTECT_INTERFACE(application_interface);
									status = SWITCH_STATUS_SUCCESS;
								}
							}
						}
						break;
					case SWITCH_IVR_ACTION_BACK:
						running = 0;
						status = SWITCH_STATUS_SUCCESS;
						break;
					case SWITCH_IVR_ACTION_TOMAIN:
						switch_set_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN);
						status = SWITCH_STATUS_BREAK;
						break;
					case SWITCH_IVR_ACTION_NOOP:
						status = SWITCH_STATUS_SUCCESS;
						break;
					default:
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Invalid TODO!\n");
						break;
					}
				}
			}

			if (switch_test_flag(menu, SWITCH_IVR_MENU_FLAG_STACK)) {	/* top level */
				if (switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN)) {	/* catch the fallback and recover */
					switch_clear_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN);
					status = SWITCH_STATUS_SUCCESS;
					running = 1;
					continue;
				}
			}
		}
		if (!match) {
			if (*menu->buf) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "IVR menu '%s' caught invalid input '%s'\n", menu->name,
								  menu->buf);
				if (menu->invalid_sound) {
					play_and_collect(session, menu, menu->invalid_sound, 0);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "IVR menu '%s' no input detected\n", menu->name);
			}
			errs++;

			/* breaks are ok too */
			if (SWITCH_STATUS_IS_BREAK(status)) {
				status = SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (stack->stack_count == 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "exit-sound '%s'\n", menu->exit_sound);
		if (!zstr(menu->exit_sound)) {
			status = play_and_collect(session, menu, menu->exit_sound, 0);
		}
	}

  end:

	stack->stack_count--;

	return status;
}

/******************************************************************************************************/

typedef struct switch_ivr_menu_xml_map {
	char *name;
	switch_ivr_action_t action;
	switch_ivr_menu_action_function_t *function;
	struct switch_ivr_menu_xml_map *next;
} switch_ivr_menu_xml_map_t;

struct switch_ivr_menu_xml_ctx {
	switch_memory_pool_t *pool;
	struct switch_ivr_menu_xml_map *map;
	int autocreated;
};

static switch_ivr_menu_xml_map_t *switch_ivr_menu_stack_xml_find(switch_ivr_menu_xml_ctx_t *xml_ctx, const char *name)
{
	switch_ivr_menu_xml_map_t *map = (xml_ctx != NULL ? xml_ctx->map : NULL);
	int rc = -1;

	while (map != NULL && (rc = strcasecmp(map->name, name)) != 0) {
		map = map->next;
	}

	return (rc == 0 ? map : NULL);
}

static switch_status_t switch_ivr_menu_stack_xml_add(switch_ivr_menu_xml_ctx_t *xml_ctx, const char *name, int action,
													 switch_ivr_menu_action_function_t *function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	/* if this action/function does not exist yet */
	if (xml_ctx != NULL && name != NULL && xml_ctx->pool != NULL && switch_ivr_menu_stack_xml_find(xml_ctx, name) == NULL) {
		switch_ivr_menu_xml_map_t *map = switch_core_alloc(xml_ctx->pool, sizeof(switch_ivr_menu_xml_map_t));

		if (map != NULL) {
			map->name = switch_core_strdup(xml_ctx->pool, name);
			map->action = action;
			map->function = function;

			if (map->name != NULL) {
				/* insert map item at top of list */
				map->next = xml_ctx->map;
				xml_ctx->map = map;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				status = SWITCH_STATUS_MEMERR;
			}
		} else {
			status = SWITCH_STATUS_MEMERR;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "switch_ivr_menu_stack_xml_add binding '%s'\n", name);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to add binding %s\n", name);
	}

	return status;
}

static struct iam_s {
	const char *name;
	switch_ivr_action_t action;
} iam[] = {
	{
	"menu-exit", SWITCH_IVR_ACTION_DIE}, {
	"menu-sub", SWITCH_IVR_ACTION_EXECMENU}, {
	"menu-exec-app", SWITCH_IVR_ACTION_EXECAPP}, {
	"menu-play-sound", SWITCH_IVR_ACTION_PLAYSOUND}, {
	"menu-back", SWITCH_IVR_ACTION_BACK}, {
	"menu-top", SWITCH_IVR_ACTION_TOMAIN}, {
	NULL, 0}
};

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_str2action(const char *action_name, switch_ivr_action_t *action)
{
	int i;

	if (!zstr(action_name)) {
		for (i = 0;; i++) {
			if (!iam[i].name) {
				break;
			}

			if (!strcasecmp(iam[i].name, action_name)) {
				*action = iam[i].action;
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	return SWITCH_STATUS_FALSE;
}

static switch_bool_t is_valid_action(const char *action)
{
	int i;

	if (!zstr(action)) {
		for (i = 0;; i++) {
			if (!iam[i].name) {
				break;
			}

			if (!strcmp(iam[i].name, action)) {
				return SWITCH_TRUE;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Action [%s]\n", switch_str_nil(action));
	return SWITCH_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_init(switch_ivr_menu_xml_ctx_t ** xml_menu_ctx, switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int autocreated = 0;

	/* build a memory pool ? */
	if (pool == NULL) {
		status = switch_core_new_memory_pool(&pool);
		autocreated = 1;
	}
	/* allocate the xml context */
	if (xml_menu_ctx != NULL && pool != NULL) {
		*xml_menu_ctx = switch_core_alloc(pool, sizeof(switch_ivr_menu_xml_ctx_t));
		if (*xml_menu_ctx != NULL) {
			(*xml_menu_ctx)->pool = pool;
			(*xml_menu_ctx)->autocreated = autocreated;
			(*xml_menu_ctx)->map = NULL;
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to alloc xml_ctx\n");
			status = SWITCH_STATUS_FALSE;
		}
	}
	/* build the standard/default xml menu handler mappings */
	if (status == SWITCH_STATUS_SUCCESS && xml_menu_ctx != NULL && *xml_menu_ctx != NULL) {
		int i;

		for (i = 0; iam[i].name && status == SWITCH_STATUS_SUCCESS; i++) {
			status = switch_ivr_menu_stack_xml_add(*xml_menu_ctx, iam[i].name, iam[i].action, NULL);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_add_custom(switch_ivr_menu_xml_ctx_t *xml_menu_ctx,
																	 const char *name, switch_ivr_menu_action_function_t *function)
{
	return switch_ivr_menu_stack_xml_add(xml_menu_ctx, name, -1, function);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_build(switch_ivr_menu_xml_ctx_t *xml_menu_ctx,
																switch_ivr_menu_t ** menu_stack, switch_xml_t xml_menus, switch_xml_t xml_menu)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (xml_menu_ctx != NULL && menu_stack != NULL && xml_menu != NULL) {
		const char *menu_name = switch_xml_attr_soft(xml_menu, "name");	/* if the attr doesn't exist, return "" */
		const char *greet_long = switch_xml_attr(xml_menu, "greet-long");	/* if the attr doesn't exist, return NULL */
		const char *greet_short = switch_xml_attr(xml_menu, "greet-short");	/* if the attr doesn't exist, return NULL */
		const char *invalid_sound = switch_xml_attr(xml_menu, "invalid-sound");	/* if the attr doesn't exist, return NULL */
		const char *exit_sound = switch_xml_attr(xml_menu, "exit-sound");	/* if the attr doesn't exist, return NULL */
		const char *timeout = switch_xml_attr_soft(xml_menu, "timeout");	/* if the attr doesn't exist, return "" */
		const char *max_failures = switch_xml_attr_soft(xml_menu, "max-failures");	/* if the attr doesn't exist, return "" */
		const char *max_timeouts = switch_xml_attr_soft(xml_menu, "max-timeouts");
		const char *exec_on_max_fail = switch_xml_attr(xml_menu, "exec-on-max-failures");
		const char *exec_on_max_timeout = switch_xml_attr(xml_menu, "exec-on-max-timeouts");
		const char *confirm_macro = switch_xml_attr(xml_menu, "confirm-macro");
		const char *confirm_key = switch_xml_attr(xml_menu, "confirm-key");
		const char *tts_engine = switch_xml_attr(xml_menu, "tts-engine");
		const char *tts_voice = switch_xml_attr(xml_menu, "tts-voice");
		const char *confirm_attempts = switch_xml_attr_soft(xml_menu, "confirm-attempts");
		const char *digit_len = switch_xml_attr_soft(xml_menu, "digit-len");
		const char *inter_timeout = switch_xml_attr_soft(xml_menu, "inter-digit-timeout");
		const char *pin = switch_xml_attr_soft(xml_menu, "pin");
		const char *prompt_pin_file = switch_xml_attr_soft(xml_menu, "pin-file");
		const char *bad_pin_file = switch_xml_attr_soft(xml_menu, "bad-pin-file");
		
		switch_ivr_menu_t *menu = NULL;

		if (zstr(max_timeouts)) {
			max_timeouts = max_failures;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "building menu '%s'\n", menu_name);

		status = switch_ivr_menu_init(&menu,
									  *menu_stack,
									  menu_name,
									  greet_long,
									  greet_short,
									  invalid_sound,
									  exit_sound,
									  confirm_macro,
									  confirm_key,
									  tts_engine,
									  tts_voice,
									  atoi(confirm_attempts),
									  atoi(inter_timeout),
									  atoi(digit_len),
									  atoi(timeout),
									  strlen(max_failures) ? atoi(max_failures) : 0, strlen(max_timeouts) ? atoi(max_timeouts) : 0, xml_menu_ctx->pool);


		if (!zstr(exec_on_max_fail)) {
			menu->exec_on_max_fail = switch_core_strdup(menu->pool, exec_on_max_fail);
		}

		if (!zstr(exec_on_max_timeout)) {
			menu->exec_on_max_timeout = switch_core_strdup(menu->pool, exec_on_max_timeout);
		}

		if (!zstr(pin)) {
			if (zstr(prompt_pin_file)) {
				prompt_pin_file = "ivr/ivr-please_enter_pin_followed_by_pound.wav";
			}
			if (zstr(bad_pin_file)) {
				prompt_pin_file = "ivr/ivr-pin_or_extension_is-invalid.wav";
			}
			menu->pin = switch_core_strdup(menu->pool, pin);
			menu->prompt_pin_file = switch_core_strdup(menu->pool, prompt_pin_file);
			menu->bad_pin_file = switch_core_strdup(menu->pool, bad_pin_file);
		}

		/* set the menu_stack for the caller */
		if (status == SWITCH_STATUS_SUCCESS && *menu_stack == NULL) {
			*menu_stack = menu;

			if (xml_menu_ctx->autocreated) {
				switch_set_flag(menu, SWITCH_IVR_MENU_FLAG_FREEPOOL);
			}
		}

		if (status == SWITCH_STATUS_SUCCESS && menu != NULL) {
			switch_xml_t xml_kvp;

			/* build menu entries */
			for (xml_kvp = switch_xml_child(xml_menu, "entry"); xml_kvp != NULL && status == SWITCH_STATUS_SUCCESS; xml_kvp = xml_kvp->next) {
				const char *action = switch_xml_attr(xml_kvp, "action");
				const char *digits = switch_xml_attr(xml_kvp, "digits");
				const char *param = switch_xml_attr_soft(xml_kvp, "param");

				if (is_valid_action(action) && !zstr(digits)) {
					switch_ivr_menu_xml_map_t *xml_map = xml_menu_ctx->map;
					int found = 0;

					/* find and appropriate xml handler */
					while (xml_map != NULL && !found) {
						if (!(found = (strcasecmp(xml_map->name, action) == 0))) {
							xml_map = xml_map->next;
						}
					}

					if (found && xml_map != NULL) {
						/* do we need to build a new sub-menu ? */
						if (xml_map->action == SWITCH_IVR_ACTION_EXECMENU && switch_ivr_menu_find(*menu_stack, param) == NULL) {
							if ((xml_menu = switch_xml_find_child(xml_menus, "menu", "name", param)) != NULL) {
								status = switch_ivr_menu_stack_xml_build(xml_menu_ctx, menu_stack, xml_menus, xml_menu);
							}
						}
						/* finally bind the menu entry */
						if (status == SWITCH_STATUS_SUCCESS) {
							if (xml_map->function != NULL) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "binding menu caller control '%s'/'%s' to '%s'\n", xml_map->name, param, digits);
								status = switch_ivr_menu_bind_function(menu, xml_map->function, param, digits);
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "binding menu action '%s' to '%s'\n", xml_map->name, digits);
								status = switch_ivr_menu_bind_action(menu, xml_map->action, param, digits);
							}
						}
					}
				} else {
					status = SWITCH_STATUS_FALSE;
				}
			}
		}
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to build xml menu\n");
	}

	return status;
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
