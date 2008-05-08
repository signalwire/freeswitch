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
 * mod_lua.c -- Lua
 *
 */

#include <switch.h>
SWITCH_BEGIN_EXTERN_C
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load);
SWITCH_MODULE_DEFINITION(mod_lua, mod_lua_load, NULL, NULL);

static struct {
	switch_memory_pool_t *pool;
	char *xml_handler;
} globals;

int luaopen_freeswitch(lua_State* L);

static int panic (lua_State *L) 
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "unprotected error in call to Lua API (%s)\n",
					  lua_tostring(L, -1));

	return 0;
}

static void lua_uninit(lua_State *L) 
{
	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_close(L);
}

static int traceback (lua_State *L) {
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);  /* pass error message */
	lua_pushinteger(L, 2);	/* skip this function and traceback */
	lua_call(L, 2, 1);	/* call debug.traceback */
	return 1;
}

static int docall (lua_State *L, int narg, int clear) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
 
	lua_pushcfunction(L, traceback);  /* push traceback function */
	lua_insert(L, base);  /* put it under chunk and args */

	status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);

	lua_remove(L, base);  /* remove traceback function */
	/* force a complete garbage collection in case of errors */
	if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
	return status;
}



static lua_State *lua_init(void) 
{
	lua_State *L = lua_open();
	int error = 0;

	if (L) {
		const char *buff = "os.exit = function() freeswitch.consoleLog(\"err\", \"Surely you jest! exiting is a bad plan....\\n\") end";
		lua_gc(L, LUA_GCSTOP, 0);
		luaL_openlibs(L);
		luaopen_freeswitch(L);
		lua_gc(L, LUA_GCRESTART, 0);
		lua_atpanic(L, panic);
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 1);
 	}
	return L;
}


static void lua_parse_and_execute(lua_State *L, char *input_code)
{
	int error = 0;

	if (switch_strlen_zero(input_code)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No code to execute!\n");
		return;
	}

	if (*input_code == '~') {
		char *buff = input_code + 1;
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 1); //lua_pcall(L, 0, 0, 0);
	} else {
		char *args = strchr(input_code, ' ');
		if (args) {
			char *code = NULL;
			int x, argc;
			char *argv[128] = { 0 };
			*args++ = '\0';

			if ((argc = switch_separate_string(args, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);
				
				stream.write_function(&stream, " argv = { ");
				for (x = 0; x < argc; x++) {
					stream.write_function(&stream, "'%s'%s", argv[x], x == argc-1 ? "" : ", ");
				}
				stream.write_function(&stream, " };");
				code = (char *)stream.data;
			} else {
				code = switch_mprintf("argv = {};");
			}

			if (code) {
				error = luaL_loadbuffer(L, code, strlen(code), "line") || docall(L, 0, 1);
				switch_safe_free(code);
			}
		}
		if (!error) {
			char *file = input_code, *fdup = NULL;
			
			if (!switch_is_file_path(file)) {
				fdup = switch_mprintf("%s/%s", SWITCH_GLOBAL_dirs.script_dir, file);
				switch_assert(fdup);
				file = fdup;
			}
			error = luaL_loadfile(L, file) || docall(L, 0, 1);
			switch_safe_free(fdup);
		}
	}

	if (error) {
		const char *err = lua_tostring(L, -1);
		if (!switch_strlen_zero(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		}
		lua_pop(L, 1);	/* pop error message from the stack */
	}
}

static void *SWITCH_THREAD_FUNC lua_thread_run(switch_thread_t *thread, void *obj)
{
	char *input_code = (char *) obj;
	lua_State *L = lua_init();	 /* opens Lua */

	lua_parse_and_execute(L, input_code);
	
	if (input_code) {
		free(input_code);
	}

	lua_uninit(L);

	return NULL;
}


static switch_xml_t lua_fetch(const char *section, 
							  const char *tag_name, 
							  const char *key_name, 
							  const char *key_value, 
							  switch_event_t *params,
							  void *user_data)
{

	switch_xml_t xml = NULL;

	if (!switch_strlen_zero(globals.xml_handler)) {
		lua_State *L = lua_init();
		char *mycmd = strdup(globals.xml_handler);
		const char *str;

		switch_assert(mycmd);

		lua_newtable(L);

		lua_pushstring(L, "section");
		lua_pushstring(L, switch_str_nil(section));
		lua_rawset(L, -3);
		lua_pushstring(L, "tag_name");
		lua_pushstring(L, switch_str_nil(tag_name));
		lua_rawset(L, -3);
		lua_pushstring(L, "key_name");
		lua_pushstring(L, switch_str_nil(key_name));
		lua_rawset(L, -3);
		lua_pushstring(L, "key_value");
		lua_pushstring(L, switch_str_nil(key_value));
		lua_rawset(L, -3);
		lua_setglobal(L, "XML_REQUEST");

		if (params) {
			mod_lua_conjure_event(L, params, "params", 1);
		}

		lua_parse_and_execute(L, mycmd);

		lua_getfield(L, LUA_GLOBALSINDEX, "XML_STRING");
		str = lua_tostring(L, 1);

		if (str) {
			if (switch_strlen_zero(str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			} else if (!(xml = switch_xml_parse_str((char *)str, strlen(str)))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
		}
		
		lua_uninit(L);
		free(mycmd);
	}

	return xml;
}


static switch_status_t do_config(void)
{
	char *cf = "lua.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!switch_strlen_zero(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, val);
					switch_xml_bind_search_function(lua_fetch, switch_xml_parse_section_string(val), NULL);
				}
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

int lua_thread(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, lua_thread_run, strdup(text), globals.pool);

	return 0;
}

SWITCH_STANDARD_APP(lua_function)
{
	lua_State *L = lua_init();
	char code[1024] = "";
	char *mycmd;
	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no args specified!\n");
        return;
    }

	snprintf(code, sizeof(code), "~session = freeswitch.Session(\"%s\");", switch_core_session_get_uuid(session));
	lua_parse_and_execute(L, code);

	mycmd = strdup((char *)data);
	switch_assert(mycmd);

	lua_parse_and_execute(L, mycmd);
	lua_uninit(L);
	free(mycmd);
}

SWITCH_STANDARD_API(luarun_api_function) {

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");
	} else {
		lua_thread(cmd);
		stream->write_function(stream, "+OK\n");
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_STANDARD_API(lua_api_function) {

	lua_State *L = lua_init();
	char *mycmd;
	
	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "");
	} else {

		mycmd = strdup(cmd);
		switch_assert(mycmd);
		mod_lua_conjure_stream(L, stream, "stream", 1);

		if (stream->event) {
			mod_lua_conjure_event(L, stream->event, "env", 1);
		}

		lua_parse_and_execute(L, mycmd);
		lua_uninit(L);
		free(mycmd);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	SWITCH_ADD_API(api_interface, "luarun", "run a script", luarun_api_function, "<script>");
	SWITCH_ADD_API(api_interface, "lua", "run a script as an api function", lua_api_function, "<script>");
	SWITCH_ADD_APP(app_interface, "lua", "Launch LUA ivr", "Run a lua ivr on a channel", lua_function, "<script>", SAF_SUPPORT_NOMEDIA);


	
	globals.pool = pool;
	do_config();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C

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
