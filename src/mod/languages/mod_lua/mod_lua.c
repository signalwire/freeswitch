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
 * mod_lua.c -- Framework Demo Module
 *
 */
#include <switch.h>
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
//#include <tolua.h>
//#include "mod_lua_wrap.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load);
SWITCH_MODULE_DEFINITION(mod_lua, mod_lua_load, NULL, NULL);

static struct {
	switch_memory_pool_t *pool;
} globals;


int luaopen_freeswitch(lua_State* L);

static lua_State *lua_init(void) 
{
	lua_State *L = lua_open();
	if (L) {
		lua_gc(L, LUA_GCSTOP, 0);
		luaL_openlibs(L);
		luaopen_freeswitch(L);
		lua_gc(L, LUA_GCRESTART, 0);
 	}
	return L;
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


static void lua_parse_and_execute(lua_State *L, char *input_code)
{
	int error = 0;

	if (*input_code == '~') {
		char *buff = input_code + 1;
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 1); //lua_pcall(L, 0, 0, 0);
	} else {
		char *args = strchr(input_code, ' ');
		if (args) {
			char *code = NULL;
			*args++ = '\0';
			int x, argc;
			char *argv[128] = { 0 };

			if ((argc = switch_separate_string(args, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
				switch_stream_handle_t stream = { 0 };
				SWITCH_STANDARD_STREAM(stream);
				
				stream.write_function(&stream, " argv = { ");
				for (x = 0; x < argc; x++) {
					stream.write_function(&stream, "'%s'%s", argv[x], x == argc-1 ? "" : ", ");
				}
				stream.write_function(&stream, " };");
				code = stream.data;
			} else {
				code = switch_mprintf("argv = {};");
			}

			if (code) {
				error = luaL_loadbuffer(L, code, strlen(code), "line") || docall(L, 0, 1);
				switch_safe_free(code);
			}
		}
		if (!error) {
			error = luaL_loadfile(L, input_code) || docall(L, 0, 1);
		}
	}

	if (error) {
		const char *err = lua_tostring(L, -1);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", switch_str_nil(err));
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

SWITCH_STANDARD_APP(lua_function) {

	lua_State *L = lua_init();
	char code[1024] = "";
	snprintf(code, sizeof(code), "~session = LUASession:new(\"%s\");", switch_core_session_get_uuid(session));
	lua_parse_and_execute(L, code);
	lua_parse_and_execute(L, (char *) data);
	snprintf(code, sizeof(code), "~session:delete()");
	lua_parse_and_execute(L, code);
	lua_uninit(L);
}

SWITCH_STANDARD_API(lua_api_function) {
	lua_thread(cmd);
	stream->write_function(stream, "+OK\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "luarun", "run a script", lua_api_function, "<script>");
	SWITCH_ADD_APP(app_interface, "lua", "Launch LUA ivr", "Run a lua ivr on a channel", lua_function, "<script>", SAF_SUPPORT_NOMEDIA);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	globals.pool = pool;

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
