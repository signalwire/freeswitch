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
static WORD COLORS[] = { FOREGROUND_RED | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_INTENSITY,
	FOREGROUND_RED | FOREGROUND_INTENSITY,
	FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	FOREGROUND_BLUE | FOREGROUND_INTENSITY,
	FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	FOREGROUND_GREEN | FOREGROUND_INTENSITY,
	FOREGROUND_GREEN | FOREGROUND_INTENSITY
};
#else
static const char *COLORS[] = { SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FRED, SWITCH_SEQ_FMAGEN, SWITCH_SEQ_FCYAN,
	SWITCH_SEQ_FGREEN, SWITCH_SEQ_FYELLOW, ""
};
#endif

static switch_memory_pool_t *module_pool = NULL;
static switch_hash_t *log_hash = NULL;
static uint32_t all_level = 0;
static int32_t hard_log_level = SWITCH_LOG_DEBUG;

static void del_mapping(char *var)
{
	switch_core_hash_insert(log_hash, var, NULL);
}

static void add_mapping(char *var, char *val, int cumlative)
{
	uint32_t m = 0;

	if (cumlative) {
		uint32_t l = switch_log_str2level(val);
		int i;

		assert(l < 10);
		
		for (i = 0; i <= l; i++) {
			m |= (1 << i);
		}
	} else {
		m = switch_log_str2mask(val);
	}
	
	if (!strcasecmp(var, "all")) {
		all_level |= m;
		return;
	}

	del_mapping(var);
	switch_core_hash_insert(log_hash, var, (void *)(intptr_t) m);
}

static switch_status_t config_logger(void)
{
	char *cf = "console.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
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
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_console_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	FILE *handle;
	
	if (!RUNNING) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (level > hard_log_level) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((handle = switch_core_data_channel(SWITCH_CHANNEL_ID_LOG))) {
        size_t mask = 0;
        int ok = 0;
    
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
			if (COLORIZE) {
#ifdef WIN32
				SetConsoleTextAttribute(hStdout, COLORS[node->level]);
				WriteFile(hStdout, node->data, (DWORD) strlen(node->data), NULL, NULL);
				SetConsoleTextAttribute(hStdout, wOldColorAttrs);
#else
				fprintf(handle, "%s%s%s", COLORS[node->level], node->data, SWITCH_SEQ_DEFAULT_COLOR);
#endif
			} else {
				fprintf(handle, "%s", node->data);
			}
		}
	} else {
		fprintf(stderr, "HELP I HAVE NO CONSOLE TO LOG TO!\n");
		fflush(stderr);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(console_api_function)
{
	int argc;
    char *mydata = NULL, *argv[3];
	const char *err = NULL;

	if (!cmd) {
		err = "bad args";
        goto end;
    }

    mydata = strdup(cmd);
    assert(mydata);
	
    argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc > 0) {

		if (argc < 2) {
			err = "missing arg";
			goto end;
		}

		if (!strcasecmp(argv[0], "loglevel")) {
			int level;
			
			if (*argv[1] > 47 && *argv[1] < 58) {
				level = atoi(argv[1]);
			} else {
				level = switch_log_str2level(argv[1]);
			}

			hard_log_level = level;
			stream->write_function(stream,  "+OK console log level set to %s\n", switch_log_level2str(hard_log_level));
			goto end;
		} else if (!strcasecmp(argv[0], "colorize")) {
			COLORIZE = switch_true(argv[1]);
			stream->write_function(stream,  "+OK console color %s\n", COLORIZE ? "enabled" : "disabled");
			goto end;
		}
		
		err = "invalid command";

	}


 end:
	
	if (err) {
		stream->write_function(stream,  "-Error %s\n", err);
	}


	free(mydata);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_LOAD_FUNCTION(mod_console_load)
{
	switch_api_interface_t *api_interface;
	

	module_pool = pool;
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "console", "Console", console_api_function, "");


	/* setup my logger function */
	switch_log_bind_logger(switch_console_logger, SWITCH_LOG_DEBUG);

	config_logger();
	RUNNING = 1;
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_console_shutdown)
{
	//switch_core_hash_destroy(&log_hash);
	//switch_core_hash_destroy(&name_hash);
	RUNNING = 0;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
