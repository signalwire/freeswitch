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
 *
 * mod_console.c -- Console Logger
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_console_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_console_shutdown);
SWITCH_MODULE_DEFINITION(mod_console, mod_console_load, mod_console_shutdown, NULL);

static int RUNNING = 0;

static int COLORIZE = 0;
#ifdef WIN32
static HANDLE hStdout;
static WORD wOldColorAttrs;
static CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

static WORD
#else
static const char *
#endif
 
	
	
	
	COLORS[] =
	{ SWITCH_SEQ_DEFAULT_COLOR, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FMAGEN, SWITCH_SEQ_FCYAN, SWITCH_SEQ_FGREEN,
SWITCH_SEQ_FYELLOW };


static switch_memory_pool_t *module_pool = NULL;
static switch_hash_t *log_hash = NULL;
static uint32_t all_level = 0;
static int32_t hard_log_level = SWITCH_LOG_DEBUG;
static switch_bool_t log_uuid = SWITCH_FALSE;
//static int32_t failed_write = 0;
static void del_mapping(char *var)
{
	switch_core_hash_insert(log_hash, var, NULL);
}

static void add_mapping(char *var, char *val, int cumlative)
{
	uint32_t m = 0;

	if (cumlative) {
		uint32_t l = switch_log_str2level(val);
		uint32_t i;

		if (l < 10) {
			for (i = 0; i <= l; i++) {
				m |= (1 << i);
			}
		}
	} else {
		m = switch_log_str2mask(val);
	}

	if (!strcasecmp(var, "all")) {
		all_level = m | switch_log_str2mask("console");
		return;
	}

	del_mapping(var);
	switch_core_hash_insert(log_hash, var, (void *) (intptr_t) m);
}

static switch_status_t config_logger(void)
{
	char *cf = "console.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (log_hash) {
		switch_core_hash_destroy(&log_hash);
	}

	switch_core_hash_init(&log_hash, module_pool);

	if ((settings = switch_xml_child(cfg, "mappings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			add_mapping(var, val, 1);
		}
		for (param = switch_xml_child(settings, "map"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			add_mapping(var, val, 0);
		}
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "colorize") && switch_true(val)) {
#ifdef WIN32
				hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
				if (switch_core_get_console() == stdout && hStdout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hStdout, &csbiInfo)) {
					wOldColorAttrs = csbiInfo.wAttributes;
					COLORIZE = 1;
				}
#else
				COLORIZE = 1;
#endif
			} else if (!strcasecmp(var, "loglevel") && !zstr(val)) {
				hard_log_level = switch_log_str2level(val);
			} else if (!strcasecmp(var, "uuid") && switch_true(val)) {
				log_uuid = SWITCH_TRUE;
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static int can_write(FILE * handle, int ms)
{
#ifndef WIN32
	int aok = 1;
	fd_set can_write;
	int fd;
	struct timeval to;
	int sec, usec;

	sec = ms / 1000;
	usec = ms % 1000;

	fd = fileno(handle);
	memset(&to, 0, sizeof(to));
	FD_ZERO(&can_write);
	FD_SET(fd, &can_write);
	to.tv_sec = sec;
	to.tv_usec = usec;
	if (select(fd + 1, NULL, &can_write, NULL, &to) > 0) {
		aok = FD_ISSET(fd, &can_write);
	} else {
		aok = 0;
	}

	return aok;
#else
	return 1;
#endif
}


static switch_status_t switch_console_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	FILE *handle;

	if (!RUNNING) {
		return SWITCH_STATUS_SUCCESS;
	}
#if 0
	if (failed_write) {
		if ((handle = switch_core_data_channel(SWITCH_CHANNEL_ID_LOG))) {
			int aok = can_write(handle, 100);
			if (aok) {
				const char *msg = "Failed to write to the console! Logging disabled! RE-enable with the 'console loglevel' command\n";
#ifdef WIN32
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s", msg);
#else
				if (COLORIZE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s%s%s", COLORS[1], msg, SWITCH_SEQ_DEFAULT_COLOR);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s", msg);
				}
#endif
				failed_write = 0;
			}
		}
	}
#endif

	if (level > hard_log_level && (node->slevel == SWITCH_LOG_UNINIT || level > node->slevel)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((handle = switch_core_data_channel(SWITCH_CHANNEL_ID_LOG))) {
		size_t mask = 0;
		size_t ok = 0;

		ok = switch_log_check_mask(all_level, level);

		if (log_hash) {
			if (!ok) {
				mask = (size_t) switch_core_hash_find(log_hash, node->file);
				ok = switch_log_check_mask(mask, level);
			}

			if (!ok) {
				mask = (size_t) switch_core_hash_find(log_hash, node->func);
				ok = switch_log_check_mask(mask, level);
			}
		}

		if (ok) {
#ifndef WIN32
			int aok = can_write(handle, 10000);

			if (!aok) {
				//hard_log_level = 0;
				//failed_write++;
				return SWITCH_STATUS_SUCCESS;
			}
#endif

			if (COLORIZE) {
#ifdef WIN32
				DWORD len = (DWORD) strlen(node->data);
				DWORD outbytes = 0;
				SetConsoleTextAttribute(hStdout, COLORS[node->level]);
				if (log_uuid && !zstr(node->userdata)) {
					WriteFile(hStdout, node->userdata, strlen(node->userdata), &outbytes, NULL);
					WriteFile(hStdout, " ", strlen(" "), &outbytes, NULL);
				}
				WriteFile(hStdout, node->data, len, &outbytes, NULL);
				SetConsoleTextAttribute(hStdout, wOldColorAttrs);
#else
				if (log_uuid && !zstr(node->userdata)) {
					fprintf(handle, "%s%s %s%s", COLORS[node->level], node->userdata, node->data, SWITCH_SEQ_DEFAULT_COLOR);
				} else {
					fprintf(handle, "%s%s%s", COLORS[node->level], node->data, SWITCH_SEQ_DEFAULT_COLOR);
				}
#endif
			} else if (log_uuid && !zstr(node->userdata)) {
				fprintf(handle, "%s %s", node->userdata, node->data);
			} else {
				fprintf(handle, "%s", node->data);
			}
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(console_api_function)
{
	int argc;
	char *mycmd = NULL, *argv[3] = { 0 };
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"console help\n"
		"console loglevel [[0-7] | <loglevel_string>]\n"
		"console colorize [on|off|toggle]\n" "--------------------------------------------------------------------------------\n";
	const char *loglevel_usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"console loglevel [[0-7] | <loglevel_string>]\n"
		"\n"
		"Set the logging verbosity of the console from 0 (least verbose) to\n"
		"7 (debugging), or specify the loglevel as a string:\n"
		"\n"
		"  0 console\n"
		"  1 alert\n"
		"  2 crit\n"
		"  3 err\n"
		"  4 warning\n" "  5 notice\n" "  6 info\n" "  7 debug\n" "--------------------------------------------------------------------------------\n";
	const char *colorize_usage_string = "USAGE:\n"
		"--------------------------------------------------------------------------------\n"
		"console colorize [on|off|toggle]\n"
		"\n" "Enable, disable, or toggle console coloring.\n" "--------------------------------------------------------------------------------\n";

	if (session)
		return SWITCH_STATUS_FALSE;

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!(mycmd = strdup(cmd))) {
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", usage_string);
		goto done;
	}

	if (!strcasecmp(argv[0], "loglevel")) {
		int level = hard_log_level;

		if (argc > 1) {
			if (!strcasecmp(argv[1], "help")) {
				stream->write_function(stream, "%s", loglevel_usage_string);
				goto done;
			} else if (*argv[1] > 47 && *argv[1] < 58) {
				level = atoi(argv[1]);
			} else {
				level = switch_log_str2level(argv[1]);
			}
		}
		if (level == SWITCH_LOG_INVALID) {
			stream->write_function(stream, "-ERR Invalid console loglevel (%s)!\n\n", argc > 1 ? argv[1] : "");
		} else {
			hard_log_level = level;
			stream->write_function(stream, "+OK console log level set to %s\n", switch_log_level2str(hard_log_level));
		}

	} else if (!strcasecmp(argv[0], "colorize")) {
		if (argc > 1) {
			if (!strcasecmp(argv[1], "help")) {
				stream->write_function(stream, "%s", colorize_usage_string);
				goto done;
			} else if (!strcasecmp(argv[1], "toggle")) {
				COLORIZE ^= 1;
			} else {
				COLORIZE = switch_true(argv[1]);
			}
		}
		stream->write_function(stream, "+OK console color %s\n", COLORIZE ? "enabled" : "disabled");

	} else {					/* if (!strcasecmp(argv[0], "help")) { */
		stream->write_function(stream, "%s", usage_string);
	}

  done:
	switch_safe_free(mycmd);
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_console_load)
{
	switch_api_interface_t *api_interface;


	module_pool = pool;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "console", "Console", console_api_function, "loglevel [level]|colorize [on|toggle|off]");
	switch_console_set_complete("add console help");
	switch_console_set_complete("add console loglevel");
	switch_console_set_complete("add console loglevel help");
	switch_console_set_complete("add console loglevel console");
	switch_console_set_complete("add console loglevel alert");
	switch_console_set_complete("add console loglevel crit");
	switch_console_set_complete("add console loglevel err");
	switch_console_set_complete("add console loglevel warning");
	switch_console_set_complete("add console loglevel notice");
	switch_console_set_complete("add console loglevel info");
	switch_console_set_complete("add console loglevel debug");
	switch_console_set_complete("add console colorize");
	switch_console_set_complete("add console colorize help");
	switch_console_set_complete("add console colorize on");
	switch_console_set_complete("add console colorize off");
	switch_console_set_complete("add console colorize toggle");

	/* setup my logger function */
	switch_log_bind_logger(switch_console_logger, SWITCH_LOG_DEBUG, SWITCH_TRUE);

	config_logger();
	RUNNING = 1;
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_console_shutdown)
{

	switch_log_unbind_logger(switch_console_logger);
	if (log_hash) {
		switch_core_hash_destroy(&log_hash);
	}

	RUNNING = 0;
	return SWITCH_STATUS_UNLOAD;
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
