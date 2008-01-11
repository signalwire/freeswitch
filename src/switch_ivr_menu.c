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
	char *tts_engine;
	char *tts_voice;
	char *phrase_lang;
	char *buf;
	char *ptr;
	int max_failures;
	int timeout;
	switch_size_t inlen;
	uint32_t flags;
	struct switch_ivr_menu_action *actions;
	struct switch_ivr_menu *next;
	switch_memory_pool_t *pool;
};

struct switch_ivr_menu_action {
	switch_ivr_menu_action_function_t *function;
	switch_ivr_action_t ivr_action;
	char *arg;
	char *bind;
	struct switch_ivr_menu_action *next;
};

static switch_ivr_menu_t *switch_ivr_menu_find(switch_ivr_menu_t * stack, const char *name)
{
	switch_ivr_menu_t *ret;
	for (ret = stack; ret; ret = ret->next) {
		if (!name || !strcmp(ret->name, name))
			break;
	}
	return ret;
}

static void switch_ivr_menu_stack_add(switch_ivr_menu_t ** top, switch_ivr_menu_t * bottom)
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
													 switch_ivr_menu_t * main,
													 const char *name,
													 const char *greeting_sound,
													 const char *short_greeting_sound,
													 const char *invalid_sound,
													 const char *exit_sound,
													 const char *tts_engine,
													 const char *tts_voice, const char *phrase_lang, int timeout, int max_failures,
													 switch_memory_pool_t *pool)
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

	if (!switch_strlen_zero(name)) {
		menu->name = switch_core_strdup(menu->pool, name);
	}

	if (!switch_strlen_zero(greeting_sound)) {
		menu->greeting_sound = switch_core_strdup(menu->pool, greeting_sound);
	}

	if (!switch_strlen_zero(short_greeting_sound)) {
		menu->short_greeting_sound = switch_core_strdup(menu->pool, short_greeting_sound);
	}

	if (!switch_strlen_zero(invalid_sound)) {
		menu->invalid_sound = switch_core_strdup(menu->pool, invalid_sound);
	}

	if (!switch_strlen_zero(exit_sound)) {
		menu->exit_sound = switch_core_strdup(menu->pool, exit_sound);
	}

	if (!switch_strlen_zero(tts_engine)) {
		menu->tts_engine = switch_core_strdup(menu->pool, tts_engine);
	}

	if (!switch_strlen_zero(tts_voice)) {
		menu->tts_voice = switch_core_strdup(menu->pool, tts_voice);
	}

	if (!switch_strlen_zero(phrase_lang)) {
		menu->phrase_lang = switch_core_strdup(menu->pool, phrase_lang);
	}

	menu->max_failures = max_failures;

	menu->timeout = timeout;

	menu->actions = NULL;

	if (newpool) {
		menu->flags |= SWITCH_IVR_MENU_FLAG_FREEPOOL;
	}

	if (menu->timeout <= 0) {
		menu->timeout = 10000;
	}

	if (main) {
		switch_ivr_menu_stack_add(&main, menu);
	} else {
		menu->flags |= SWITCH_IVR_MENU_FLAG_STACK;
	}

	*new_menu = menu;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_action(switch_ivr_menu_t * menu, switch_ivr_action_t ivr_action, const char *arg, const char *bind)
{
	switch_ivr_menu_action_t *action;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = switch_core_strdup(menu->pool, bind);
		action->next = menu->actions;
		action->arg = switch_core_strdup(menu->pool, arg);
		len = (uint32_t) strlen(action->bind) + 1;
		if (len > menu->inlen) {
			menu->inlen = len;
		}
		action->ivr_action = ivr_action;
		menu->actions = action;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_bind_function(switch_ivr_menu_t * menu,
															  switch_ivr_menu_action_function_t * function, const char *arg, const char *bind)
{
	switch_ivr_menu_action_t *action;
	uint32_t len;

	if ((action = switch_core_alloc(menu->pool, sizeof(*action)))) {
		action->bind = switch_core_strdup(menu->pool, bind);
		action->next = menu->actions;
		action->arg = switch_core_strdup(menu->pool, arg);
		len = (uint32_t) strlen(action->bind) + 1;
		if (len > menu->inlen) {
			menu->inlen = len;
		}
		action->function = function;
		menu->actions = action;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_free(switch_ivr_menu_t * stack)
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

static switch_status_t play_or_say(switch_core_session_t *session, switch_ivr_menu_t * menu, char *sound, switch_size_t need)
{
	char terminator;
	uint32_t len;
	char *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_input_args_t args = { 0 };

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "play_or_say sound=[%s]\n", sound);

	if (session != NULL && menu != NULL && !switch_strlen_zero(sound)) {
		memset(menu->buf, 0, menu->inlen);
		menu->ptr = menu->buf;

		if (!need) {
			len = 1;
			ptr = NULL;
		} else {
			len = (uint32_t) menu->inlen;
			ptr = menu->ptr;
		}
		args.buf = ptr;
		args.buflen = len;

		if (strlen(sound) > 4 && strncasecmp(sound, "say:", 4) == 0) {
			if (menu->tts_engine && menu->tts_voice) {
				status = switch_ivr_speak_text(session, menu->tts_engine, menu->tts_voice, sound + 4, &args);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No TTS engine to play sound\n");
			}
		} else if (strlen(sound) > 7 && strncasecmp(sound, "phrase:", 7) == 0) {
			status = switch_ivr_phrase_macro(session, sound + 7, "", menu->phrase_lang, &args);
		} else {
			status = switch_ivr_play_file(session, NULL, sound, &args);
		}
		
		if (need) {
			menu->ptr += strlen(menu->buf);
			if (strlen(menu->buf) < need) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "waiting for %u digits\n", (uint32_t)need);
				status = switch_ivr_collect_digits_count(session, menu->ptr, menu->inlen - strlen(menu->buf), need, "#", &terminator, menu->timeout, 0, 0);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "digits '%s'\n", menu->buf);
		}
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "play_or_say returning [%d]\n", status);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_execute(switch_core_session_t *session, switch_ivr_menu_t * stack, char *name, void *obj)
{
	int reps = 0, errs = 0, match = 0, running = 1;
	char *greeting_sound = NULL, *aptr = NULL;
	char arg[512];
	switch_ivr_action_t todo = SWITCH_IVR_ACTION_DIE;
	switch_ivr_menu_action_t *ap;
	switch_ivr_menu_t *menu;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (session == NULL || stack == NULL || switch_strlen_zero(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid menu context\n");
		return SWITCH_STATUS_FALSE;
	}

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!(menu = switch_ivr_menu_find(stack, name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Menu!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!(menu->buf = malloc(menu->inlen))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Memory!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Executing IVR menu %s\n", menu->name);

	for (reps = 0; (running && status == SWITCH_STATUS_SUCCESS && errs < menu->max_failures); reps++) {
		if (!switch_channel_ready(channel)) {
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

		memset(menu->buf, 0, menu->inlen);
		status = play_or_say(session, menu, greeting_sound, menu->inlen - 1);

		if (!switch_strlen_zero(menu->buf)) {
			for (ap = menu->actions; ap; ap = ap->next) {
				if (!strcmp(menu->buf, ap->bind)) {
					match++;
					errs = 0;
					if (ap->function) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
										  "IVR function on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf, ap->arg);
						todo = ap->function(menu, ap->arg, arg, sizeof(arg), obj);
						aptr = arg;
					} else {
						todo = ap->ivr_action;
						aptr = ap->arg;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
										  "IVR action on menu '%s' matched '%s' param '%s'\n", menu->name, menu->buf, aptr);
					}

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "switch_ivr_menu_execute todo=[%d]\n", todo);

					switch (todo) {
					case SWITCH_IVR_ACTION_DIE:
						status = SWITCH_STATUS_FALSE;
						break;
					case SWITCH_IVR_ACTION_PLAYSOUND:
						status = switch_ivr_play_file(session, NULL, aptr, NULL);
						break;
					case SWITCH_IVR_ACTION_SAYTEXT:
						status = switch_ivr_speak_text(session, menu->tts_engine, menu->tts_voice, aptr, NULL);
						break;
					case SWITCH_IVR_ACTION_SAYPHRASE:
						status = switch_ivr_phrase_macro(session, aptr, "", menu->phrase_lang, NULL);
						break;
					case SWITCH_IVR_ACTION_TRANSFER:
						switch_ivr_session_transfer(session, aptr, NULL, NULL);
						running = 0;
						break;
					case SWITCH_IVR_ACTION_EXECMENU:
						reps = -1;
						status = switch_ivr_menu_execute(session, stack, aptr, obj);
						break;
					case SWITCH_IVR_ACTION_EXECAPP:
						{
							const switch_application_interface_t *application_interface;
							char *app_name;
							char *app_arg = NULL;

							status = SWITCH_STATUS_FALSE;

							if (!switch_strlen_zero(aptr)) {
								app_name = switch_core_session_strdup(session, aptr);
								if ((app_arg = strchr(app_name, ' '))) {
									*app_arg++ = '\0';
								}

								if ((application_interface = switch_loadable_module_get_application_interface(app_name))) {
									switch_core_session_exec(session, application_interface, app_arg);
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
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid TODO!\n");
						break;
					}
				}
			}


			if (switch_test_flag(menu, SWITCH_IVR_MENU_FLAG_STACK)) {	// top level
				if (switch_test_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN)) {	// catch the fallback and recover
					switch_clear_flag(stack, SWITCH_IVR_MENU_FLAG_FALLTOMAIN);
					status = SWITCH_STATUS_SUCCESS;
					running = 1;
					continue;
				}
			}
		}
		if (!match) {
			if (*menu->buf) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IVR menu '%s' caught invalid input '%s'\n", menu->name, menu->buf);
				if (menu->invalid_sound) {
					play_or_say(session, menu, menu->invalid_sound, 0);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IVR menu '%s' no input detected\n", menu->name);
			}
			errs++;
			if (status == SWITCH_STATUS_SUCCESS) {
				status = switch_ivr_sleep(session, 1000);
			}
			/* breaks are ok too */
			if (SWITCH_STATUS_IS_BREAK(status)) {
				status = SWITCH_STATUS_SUCCESS;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "exit-sound '%s'\n", menu->exit_sound);
	if (!switch_strlen_zero(menu->exit_sound)) {
		play_or_say(session, menu, menu->exit_sound, 0);
	}

	switch_safe_free(menu->buf);

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

static switch_ivr_menu_xml_map_t *switch_ivr_menu_stack_xml_find(switch_ivr_menu_xml_ctx_t * xml_ctx, char *name)
{
	switch_ivr_menu_xml_map_t *map = (xml_ctx != NULL ? xml_ctx->map : NULL);
	int rc = -1;

	while (map != NULL && (rc = strcasecmp(map->name, name)) != 0) {
		map = map->next;
	}

	return (rc == 0 ? map : NULL);
}

static switch_status_t switch_ivr_menu_stack_xml_add(switch_ivr_menu_xml_ctx_t * xml_ctx, char *name, int action,
													 switch_ivr_menu_action_function_t * function)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	// if this action/function does not exist yet
	if (xml_ctx != NULL && name != NULL && xml_ctx->pool != NULL && switch_ivr_menu_stack_xml_find(xml_ctx, name) == NULL) {
		switch_ivr_menu_xml_map_t *map = switch_core_alloc(xml_ctx->pool, sizeof(switch_ivr_menu_xml_map_t));

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "switch_ivr_menu_stack_xml_add binding '%s'\n", name);
		// and we have memory
		if (map != NULL) {
			map->name = switch_core_strdup(xml_ctx->pool, name);
			map->action = action;
			map->function = function;

			if (map->name != NULL) {
				// insert map item at top of list
				map->next = xml_ctx->map;
				xml_ctx->map = map;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				status = SWITCH_STATUS_MEMERR;
			}
		} else {
			status = SWITCH_STATUS_MEMERR;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_init(switch_ivr_menu_xml_ctx_t ** xml_menu_ctx, switch_memory_pool_t *pool)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int autocreated = 0;

	// build a memory pool ?
	if (pool == NULL) {
		status = switch_core_new_memory_pool(&pool);
		autocreated = 1;
	}
	// allocate the xml context
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
	// build the standard/default xml menu handler mappings
	if (status == SWITCH_STATUS_SUCCESS && xml_menu_ctx != NULL && *xml_menu_ctx != NULL) {
		struct iam_s {
			char *name;
			switch_ivr_action_t action;
		} iam[] = {
			{
			"menu-exit", SWITCH_IVR_ACTION_DIE}, {
			"menu-sub", SWITCH_IVR_ACTION_EXECMENU}, {
			"menu-exec-api", SWITCH_IVR_ACTION_EXECAPP}, {
			"menu-play-sound", SWITCH_IVR_ACTION_PLAYSOUND}, {
			"menu-say-text", SWITCH_IVR_ACTION_SAYTEXT}, {
			"menu-say-phrase", SWITCH_IVR_ACTION_SAYPHRASE}, {
			"menu-back", SWITCH_IVR_ACTION_BACK}, {
			"menu-top", SWITCH_IVR_ACTION_TOMAIN}, {
		"menu-call-transfer", SWITCH_IVR_ACTION_TRANSFER},};
		int iam_qty = (sizeof(iam) / sizeof(iam[0]));
		int i;

		for (i = 0; i < iam_qty && status == SWITCH_STATUS_SUCCESS; i++) {
			status = switch_ivr_menu_stack_xml_add(*xml_menu_ctx, iam[i].name, iam[i].action, NULL);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_add_custom(switch_ivr_menu_xml_ctx_t * xml_menu_ctx,
																	 char *name, switch_ivr_menu_action_function_t * function)
{
	return switch_ivr_menu_stack_xml_add(xml_menu_ctx, name, -1, function);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_menu_stack_xml_build(switch_ivr_menu_xml_ctx_t * xml_menu_ctx,
																switch_ivr_menu_t ** menu_stack, switch_xml_t xml_menus, switch_xml_t xml_menu)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (xml_menu_ctx != NULL && menu_stack != NULL && xml_menu != NULL) {
		const char *menu_name = switch_xml_attr_soft(xml_menu, "name");	// if the attr doesn't exist, return ""
		const char *greet_long = switch_xml_attr(xml_menu, "greet-long");	// if the attr doesn't exist, return NULL
		const char *greet_short = switch_xml_attr(xml_menu, "greet-short");	// if the attr doesn't exist, return NULL
		const char *invalid_sound = switch_xml_attr(xml_menu, "invalid-sound");	// if the attr doesn't exist, return NULL
		const char *exit_sound = switch_xml_attr(xml_menu, "exit-sound");	// if the attr doesn't exist, return NULL
		const char *tts_engine = switch_xml_attr(xml_menu, "tts-engine");	// if the attr doesn't exist, return NULL
		const char *tts_voice = switch_xml_attr(xml_menu, "tts-voice");	// if the attr doesn't exist, return NULL
		const char *phrase_lang = switch_xml_attr(xml_menu, "phrase-lang");	// if the attr doesn't exist, return NULL
		const char *timeout = switch_xml_attr_soft(xml_menu, "timeout");	// if the attr doesn't exist, return ""
		const char *max_failures = switch_xml_attr_soft(xml_menu, "max-failures");	// if the attr doesn't exist, return ""
		switch_ivr_menu_t *menu = NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "building menu '%s'\n", menu_name);
		status = switch_ivr_menu_init(&menu,
									  *menu_stack,
									  menu_name,
									  greet_long,
									  greet_short,
									  invalid_sound,
									  exit_sound, tts_engine, tts_voice, phrase_lang, atoi(timeout) * 1000, atoi(max_failures), xml_menu_ctx->pool);
		// set the menu_stack for the caller
		if (status == SWITCH_STATUS_SUCCESS && *menu_stack == NULL) {
			*menu_stack = menu;
		}

		if (status == SWITCH_STATUS_SUCCESS && menu != NULL) {
			switch_xml_t xml_kvp;

			// build menu entries
			for (xml_kvp = switch_xml_child(xml_menu, "entry"); xml_kvp != NULL && status == SWITCH_STATUS_SUCCESS; xml_kvp = xml_kvp->next) {
				const char *action = switch_xml_attr(xml_kvp, "action");
				const char *digits = switch_xml_attr(xml_kvp, "digits");
				const char *param = switch_xml_attr_soft(xml_kvp, "param");

				if (!switch_strlen_zero(action) && !switch_strlen_zero(digits)) {
					switch_ivr_menu_xml_map_t *xml_map = xml_menu_ctx->map;
					int found = 0;

					// find and appropriate xml handler
					while (xml_map != NULL && !found) {
						if (!(found = (strcasecmp(xml_map->name, action) == 0))) {
							xml_map = xml_map->next;
						}
					}

					if (found && xml_map != NULL) {
						// do we need to build a new sub-menu ?
						if (xml_map->action == SWITCH_IVR_ACTION_EXECMENU && switch_ivr_menu_find(*menu_stack, param) == NULL) {
							if ((xml_menu = switch_xml_find_child(xml_menus, "menu", "name", param)) != NULL) {
								status = switch_ivr_menu_stack_xml_build(xml_menu_ctx, menu_stack, xml_menus, xml_menu);
							}
						}
						// finally bind the menu entry
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
