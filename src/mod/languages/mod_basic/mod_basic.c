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
 * mod_basic.c -- BASIC Module
 *
 */
#include <switch.h>
#include "my_basic.h"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_basic_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_basic_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_basic_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_basic, mod_basic_load, mod_basic_shutdown, NULL);

static struct {
	int integer;
} globals;



static switch_status_t do_config(switch_bool_t reload)
{
	memset(&globals, 0, sizeof(globals));

	return SWITCH_STATUS_SUCCESS;
}

static void _on_error(mb_interpreter_t* s, mb_error_e e, char* m, int p, unsigned short row, unsigned short col, int abort_code) {
	mb_unrefvar(s);
	if(SE_NO_ERR != e) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
						  "Error:\n    [POS] %d, [ROW] %d, [COL] %d,\n    [CODE] %d, [MESSAGE] %s, [ABORT CODE] %d\n", p, row, col, e, m, abort_code);
	}
}

typedef struct fs_data {
	switch_core_session_t *session;
	int argc;
	char *argv[128];
	switch_event_t *vars;
} fs_data_t;


static int fun_execute(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t app;
	mb_value_t arg;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &app)) != MB_FUNC_OK) {
		goto end;
	}

	if ((result = mb_pop_value(s, l, &arg)) != MB_FUNC_OK) {
		goto end;
	}

	if (app.type == MB_DT_STRING && arg.type == MB_DT_STRING && fsdata->session) {
		switch_core_session_execute_application(fsdata->session, app.value.string, arg.value.string);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad args or no fsdata->session\n");
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}            


static int fun_setvar(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t var;
	mb_value_t val;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &var)) != MB_FUNC_OK) {
		goto end;
	}

	if ((result = mb_pop_value(s, l, &val)) != MB_FUNC_OK) {
		goto end;
	}

	if (var.type == MB_DT_STRING && val.type == MB_DT_STRING && fsdata->session) {
		switch_channel_t *channel = switch_core_session_get_channel(fsdata->session);
		switch_channel_set_variable(channel, var.value.string, val.value.string);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad args or no session\n");
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}            

static int fun_getarg(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t idx;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &idx)) != MB_FUNC_OK) {
		goto end;
	}

	if (idx.type == MB_DT_INT && fsdata->argc) {
		if (idx.value.integer < fsdata->argc) {
			mb_push_string(s, l, strdup(fsdata->argv[idx.value.integer]));
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad args or no session\n");
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}

static int fun_getvar(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t var;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &var)) != MB_FUNC_OK) {
		goto end;
	}

	if (var.type == MB_DT_STRING && fsdata->session) {
		switch_channel_t *channel = switch_core_session_get_channel(fsdata->session);
		const char *value = switch_channel_get_variable(channel, var.value.string);
		
		mb_push_string(s, l, strdup(value));

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad args or no session\n");
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}            

static int fun_api(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t app;
	mb_value_t arg;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &app)) != MB_FUNC_OK) {
		goto end;
	}

	if ((result = mb_pop_value(s, l, &arg)) != MB_FUNC_OK) {
		goto end;
	}

	if (app.type == MB_DT_STRING && arg.type == MB_DT_STRING) {
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);

		switch_api_execute(app.value.string, arg.value.string, fsdata->session, &stream);
		mb_push_string(s, l, (char *) stream.data);
		//switch_safe_free(stream.data);
	} else {
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}            


static int fun_log(mb_interpreter_t* s, void** l) 
{
	int result = MB_FUNC_OK;
	fs_data_t *fsdata = (fs_data_t *) mb_get_user_data(s);
	mb_value_t level;
	mb_value_t data;

	mb_assert(s && l);

	mb_check(mb_attempt_func_begin(s, l));

	if ((result = mb_pop_value(s, l, &level)) != MB_FUNC_OK) {
		goto end;
	}

	if ((result = mb_pop_value(s, l, &data)) != MB_FUNC_OK) {
		goto end;
	}

	if (level.type == MB_DT_STRING && data.type == MB_DT_STRING) {
		switch_log_level_t fslevel = SWITCH_LOG_DEBUG;
		
		fslevel = switch_log_str2level(level.value.string);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(fsdata->session), fslevel, "%s\n", data.value.string);
	} else {
		result = MB_FUNC_WARNING;
	}

	mb_check(mb_attempt_func_end(s, l));

 end:

	return result;
}            


static int bprint(const char *fmt, ...) 
{
	char *data = NULL;
	va_list ap;
	int ret = 0;
	
	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);
	
	if (data) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "%s\n", data);
	}

	switch_safe_free(data);	
	
	return ret;

}


SWITCH_STANDARD_APP(basic_function)
{
	const char *file;
	char *fdup = NULL;
	mb_interpreter_t *bi = 0;
	fs_data_t fsdata = { 0 };
	char *mydata = NULL;

	if (data) {
		mydata = strdup((char *) data);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing data\n");
		return;
	}

	fsdata.argc = switch_split(mydata, ' ', fsdata.argv);

	file = fsdata.argv[0];

	if (zstr(file)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing file\n");
	}

	if (!switch_is_file_path(file)) {
		fdup = switch_mprintf("%s/%s", SWITCH_GLOBAL_dirs.script_dir, file);
		switch_assert(fdup);
		file = fdup;
	}

	mb_open(&bi);
	mb_set_error_handler(bi, _on_error); 
	mb_set_printer(bi, bprint);
	fsdata.session = session;
	mb_set_user_data(bi, (void *) &fsdata);

	mb_register_func(bi, "FS_EXECUTE", fun_execute);
	mb_register_func(bi, "FS_GETARG", fun_getarg);
	mb_register_func(bi, "FS_GETVAR", fun_getvar);
	mb_register_func(bi, "FS_SETVAR", fun_setvar);
	mb_register_func(bi, "FS_API", fun_api);
	mb_register_func(bi, "FS_LOG", fun_log);
	
	if (mb_load_file(bi, file) == MB_FUNC_OK) {
		mb_run(bi);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error executing file\n");
	}

	mb_close(&bi);
	
	switch_safe_free(fdup);
	switch_safe_free(mydata);
}

SWITCH_STANDARD_API(basic_api_function)
{

	basic_function(session, cmd);
	
	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_basic_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_basic_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	do_config(SWITCH_FALSE);

	SWITCH_ADD_API(api_interface, "basic", "Basic API", basic_api_function, "syntax");
	SWITCH_ADD_APP(app_interface, "basic", "", "", basic_function, "<file>", SAF_NONE);

	mb_init();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_basic_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_basic_shutdown)
{
	/* Cleanup dynamically allocated config settings */
	mb_dispose();

	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_basic_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_basic_runtime)
{
	while(looping)
	{
		switch_cond_next();
	}
	return SWITCH_STATUS_TERM;
}
*/

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
