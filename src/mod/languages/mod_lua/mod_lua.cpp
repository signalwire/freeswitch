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
 * mod_lua.c -- Lua
 *
 */



#include <switch.h>
#include <switch_event.h>
SWITCH_BEGIN_EXTERN_C
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lua_shutdown);

SWITCH_MODULE_DEFINITION_EX(mod_lua, mod_lua_load, mod_lua_shutdown, NULL, SMODF_GLOBAL_SYMBOLS);
static struct {
	switch_memory_pool_t *pool;
	char *xml_handler;
} globals;

int luaopen_freeswitch(lua_State * L);
int lua_thread(const char *text);

static int panic(lua_State * L)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));

	return 0;
}

static void lua_uninit(lua_State * L)
{
	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_close(L);
}

static int traceback(lua_State * L)
{
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
	lua_pushvalue(L, 1);		/* pass error message */
	lua_pushinteger(L, 2);		/* skip this function and traceback */
	lua_call(L, 2, 1);			/* call debug.traceback */
	return 1;
}

int docall(lua_State * L, int narg, int nresults, int perror)
{
	int status;
	int base = lua_gettop(L) - narg;	/* function index */

	lua_pushcfunction(L, traceback);	/* push traceback function */
	lua_insert(L, base);		/* put it under chunk and args */

	status = lua_pcall(L, narg, nresults, base);

	lua_remove(L, base);		/* remove traceback function */
	/* force a complete garbage collection in case of errors */
	if (status != 0) {
		lua_gc(L, LUA_GCCOLLECT, 0);
	}

	if (status && perror) {
		const char *err = lua_tostring(L, -1);
		if (!zstr(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		}
		//lua_pop(L, 1); /* pop error message from the stack */
		// pass error up to top
		lua_error(L);
	}

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
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 0, 0);
	}
	return L;
}


static int lua_parse_and_execute(lua_State * L, char *input_code)
{
	int error = 0;

	if (zstr(input_code)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No code to execute!\n");
		return 1;
	}

	while(input_code && (*input_code == ' ' || *input_code == '\n' || *input_code == '\r')) input_code++;
	
	if (*input_code == '~') {
		char *buff = input_code + 1;
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 0, 0);	//lua_pcall(L, 0, 0, 0);
	} else if (!strncasecmp(input_code, "#!/lua", 6)) {
		char *buff = input_code + 6;
		error = luaL_loadbuffer(L, buff, strlen(buff), "line") || docall(L, 0, 0, 0);	//lua_pcall(L, 0, 0, 0);
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

				stream.write_function(&stream, " argv = {[0]='%y', ", input_code);
				for (x = 0; x < argc; x++) {
					stream.write_function(&stream, "'%y'%s", argv[x], x == argc - 1 ? "" : ", ");
				}
				stream.write_function(&stream, " };");
				code = (char *) stream.data;
			} else {
				code = switch_mprintf("argv = {[0]='%s'};", input_code);
			}

			if (code) {
				error = luaL_loadbuffer(L, code, strlen(code), "line") || docall(L, 0, 0, 0);
				switch_safe_free(code);
			}
		} else {
			// Force empty argv table
			char *code = NULL;
			code = switch_mprintf("argv = {[0]='%s'};", input_code);
			error = luaL_loadbuffer(L, code, strlen(code), "line") || docall(L, 0, 0, 0);
			switch_safe_free(code);
		}

		if (!error) {
			char *file = input_code, *fdup = NULL;

			if (!switch_is_file_path(file)) {
				fdup = switch_mprintf("%s/%s", SWITCH_GLOBAL_dirs.script_dir, file);
				switch_assert(fdup);
				file = fdup;
			}
			error = luaL_loadfile(L, file) || docall(L, 0, 0, 0);
			switch_safe_free(fdup);
		}
	}

	if (error) {
		const char *err = lua_tostring(L, -1);
		if (!zstr(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
		}
		lua_pop(L, 1);			/* pop error message from the stack */
	}

	return error;
}

struct lua_thread_helper {
	switch_memory_pool_t *pool;
	char *input_code;
};

static void *SWITCH_THREAD_FUNC lua_thread_run(switch_thread_t *thread, void *obj)
{
	struct lua_thread_helper *lth = (struct lua_thread_helper *) obj;
	switch_memory_pool_t *pool = lth->pool;
	lua_State *L = lua_init();	/* opens Lua */

	lua_parse_and_execute(L, lth->input_code);

	lth = NULL;

	switch_core_destroy_memory_pool(&pool);

	lua_uninit(L);

	return NULL;
}


static switch_xml_t lua_fetch(const char *section,
							  const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params, void *user_data)
{

	switch_xml_t xml = NULL;

	if (!zstr(globals.xml_handler)) {
		lua_State *L = lua_init();
		char *mycmd = strdup(globals.xml_handler);
		const char *str;
		int error;

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

		if((error = lua_parse_and_execute(L, mycmd))){
		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "LUA script parse/execute error!\n");
		    return NULL;
		}

		lua_getfield(L, LUA_GLOBALSINDEX, "XML_STRING");
		str = lua_tostring(L, 1);
		
		if (str) {
			if (zstr(str)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Result\n");
			} else if (!(xml = switch_xml_parse_str_dynamic((char *)str, SWITCH_TRUE))) { 
				/* const char -> char conversion was OK because switch_xml_parse_str_dynamic makes a duplicate of str 
				   and saves this duplcate as root->m which is freed when switch_xml_free is issued
				*/
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Parsing XML Result!\n");
			}
		}

		lua_uninit(L);
		free(mycmd);
	}

	return xml;
}


static void lua_event_handler(switch_event_t *event);

static switch_status_t do_config(void)
{
	const char *cf = "lua.conf";
	switch_xml_t cfg, xml, settings, param, hook;
	switch_stream_handle_t path_stream = {0};
	switch_stream_handle_t cpath_stream = {0};
	
    if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	SWITCH_STANDARD_STREAM(path_stream);
	SWITCH_STANDARD_STREAM(cpath_stream);
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "xml-handler-script")) {
				globals.xml_handler = switch_core_strdup(globals.pool, val);
			} else if (!strcmp(var, "xml-handler-bindings")) {
				if (!zstr(globals.xml_handler)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "binding '%s' to '%s'\n", globals.xml_handler, val);
					switch_xml_bind_search_function(lua_fetch, switch_xml_parse_section_string(val), NULL);
				}
			} else if (!strcmp(var, "module-directory") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: appending module directory: '%s'\n", val);
				if (cpath_stream.data_len) {
					cpath_stream.write_function(&cpath_stream, ";");
				}
				cpath_stream.write_function(&cpath_stream, "%s", val);
			} else if (!strcmp(var, "script-directory") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: appending script directory: '%s'\n", val);
				if (path_stream.data_len) {
					path_stream.write_function(&path_stream, ";");
				}
				path_stream.write_function(&path_stream, "%s", val);
			}
		}

		for (hook = switch_xml_child(settings, "hook"); hook; hook = hook->next) {
			char *event = (char *) switch_xml_attr_soft(hook, "event");
			char *subclass = (char *) switch_xml_attr_soft(hook, "subclass");
			//char *script = strdup( (char *) switch_xml_attr_soft(hook, "script"));
			char *script = (char *) switch_xml_attr_soft(hook, "script");
			switch_event_types_t evtype;

			if (!zstr(script)) {
				script = switch_core_strdup(globals.pool, script);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "hook params: '%s' | '%s' | '%s'\n", event, subclass, script);

			if (switch_name_event(event,&evtype) == SWITCH_STATUS_SUCCESS) {
				if (!zstr(script)) {
					if (switch_event_bind(modname, evtype, !zstr(subclass) ? subclass : SWITCH_EVENT_SUBCLASS_ANY,
							lua_event_handler, script) == SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "event handler for '%s' set to '%s'\n", switch_event_name(evtype), script);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: unsuccessful bind\n");
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: no script name for event type '%s'\n", event);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cannot set event handler: unknown event type '%s'\n", event);
			}
		}
	}

	if (cpath_stream.data_len) {
		char *lua_cpath = NULL;
		if ((lua_cpath = getenv("LUA_CPATH"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: appending LUA_CPATH: '%s'\n", lua_cpath);
			cpath_stream.write_function(&cpath_stream, ";%s", lua_cpath);
		}
#ifdef WIN32
		if (_putenv_s("LUA_CPATH", (char *)cpath_stream.data) != 0) {
#else
		if (setenv("LUA_CPATH", (char *)cpath_stream.data, 1) == ENOMEM) {
#endif
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: LUA_CPATH unable to be set, out of memory: '%s'\n", (char *)cpath_stream.data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: LUA_CPATH set to: '%s'\n", (char *)cpath_stream.data);
		}
	}
	switch_safe_free(cpath_stream.data);

	if (path_stream.data_len) {
		char *lua_path = NULL;
		if ((lua_path = getenv("LUA_PATH"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: appending LUA_PATH: '%s'\n", lua_path);
			path_stream.write_function(&path_stream, ";%s", lua_path);
		}
#ifdef WIN32
		if (_putenv_s("LUA_PATH", (char *)path_stream.data) != 0) {
#else
		if (setenv("LUA_PATH", (char *)path_stream.data, 1) == ENOMEM) {
#endif
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: LUA_PATH unable to be set, out of memory: '%s'\n", (char *)path_stream.data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "lua: LUA_PATH set to: '%s'\n", (char *)path_stream.data);
		}
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcmp(var, "startup-script")) {
				if (val) {
					lua_thread(val);
					/* wait 10ms to avoid lua init issues */
					switch_yield(10000);
				}
			}
		}
	}

	switch_safe_free(path_stream.data);
    
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

int lua_thread(const char *text)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	lua_thread_helper *lth;

	switch_core_new_memory_pool(&pool);
	lth = (lua_thread_helper *) switch_core_alloc(pool, sizeof(*lth));
	lth->pool = pool;
	lth->input_code = switch_core_strdup(lth->pool, text);

	switch_threadattr_create(&thd_attr, lth->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, lua_thread_run, lth, lth->pool);

	return 0;
}

static void lua_event_handler(switch_event_t *event)
{
	lua_State *L = lua_init();
	char *script = NULL;

	if (event->bind_user_data) {
		script = strdup((char *)event->bind_user_data);
	}

	mod_lua_conjure_event(L, event, "event", 1);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "lua event hook: execute '%s'\n", (char *)script);
	lua_parse_and_execute(L, (char *)script);
	lua_uninit(L);

	switch_safe_free(script);
}

SWITCH_STANDARD_APP(lua_function)
{
	lua_State *L = lua_init();
	char *mycmd;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no args specified!\n");
		return;
	}

	mod_lua_conjure_session(L, session, "session", 1);

	mycmd = strdup((char *) data);
	switch_assert(mycmd);

	lua_parse_and_execute(L, mycmd);
	lua_uninit(L);
	free(mycmd);

}

SWITCH_STANDARD_API(luarun_api_function)
{

	if (zstr(cmd)) {
		stream->write_function(stream, "-ERR no args specified!\n");
	} else {
		lua_thread(cmd);
		stream->write_function(stream, "+OK\n");
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_CHAT_APP(lua_chat_function)
{
	lua_State *L = lua_init();
	char *dup = NULL;

	if (data) {
		dup = strdup(data);
	}

	mod_lua_conjure_event(L, message, "message", 1);
	lua_parse_and_execute(L, (char *)dup);
	lua_uninit(L);

	switch_safe_free(dup);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_STANDARD_API(lua_api_function)
{

	lua_State *L = lua_init();
	char *mycmd;
	int error;

	if (zstr(cmd)) {
		stream->write_function(stream, "");
	} else {

		mycmd = strdup(cmd);
		switch_assert(mycmd);

		if (session) {
			mod_lua_conjure_session(L, session, "session", 1);
		}

		mod_lua_conjure_stream(L, stream, "stream", 1);

		if (stream->param_event) {
			mod_lua_conjure_event(L, stream->param_event, "env", 1);
		}

		if ((error = lua_parse_and_execute(L, mycmd))) {
			char * http = switch_event_get_header(stream->param_event, "http-uri");
			if (http && (!strncasecmp(http, "/api/", 5) || !strncasecmp(http, "/webapi/", 8))) {
					/* api -> fs api streams the Content-Type e.g. text/html or text/xml               */
					/* api -> default Content-Type is text/plain 				                       */
					/* webapi, txtapi -> Content-Type defined in mod_xmlrpc	text/html resp. text/plain */
					stream->write_function(stream, "<H2>Error Executing Script</H2>");
			} else {
				stream->write_function(stream, "-ERR Cannot execute script\n");
			}
		}
		lua_uninit(L);
		free(mycmd);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_DIALPLAN(lua_dialplan_hunt)
{
	lua_State *L = lua_init();
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *cmd = NULL;

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error Obtaining Profile!\n");
			goto done;
		}
	}

	if (!caller_profile->context) {
		caller_profile->context = "lua/dialplan.lua";
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Processing %s->%s in context/script %s\n",
	          caller_profile->caller_id_name, caller_profile->destination_number, caller_profile->context);

	if ((extension = switch_caller_extension_new(session, "_anon_", caller_profile->destination_number)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
		goto done;
	}

	cmd = strdup(caller_profile->context);
	switch_assert(cmd);

	mod_lua_conjure_session(L, session, "session", 1);
	lua_parse_and_execute(L, cmd);

	/* expecting ACTIONS = { {"app1", "app_data1"}, { "app2" }, "app3" } -- each of three is valid */
	lua_getfield(L, LUA_GLOBALSINDEX, "ACTIONS");
	if (!lua_istable(L, 1)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
			"Global variable ACTIONS may only be a table\n");
		goto done;
	}

	lua_pushnil(L); /* STACK = tab | nil */

	while (lua_next(L, 1) != 0) { /* STACK = tab | k1 .. kn | vn */
		char *application = NULL, *app_data = NULL;

		if (lua_isstring(L, -1)) {
			application = strdup(lua_tostring(L, -1));
			app_data = strdup("");

		} else if (lua_istable(L, -1)) {
			int i = lua_gettop(L);

			lua_pushnil(L); /* STACK = tab1 | k1 .. kn | tab2 | nil */

			if (lua_next(L, i) != 0) { /* STACK = tab1 | k1 .. kn | tab2 | k | v */

				if (!lua_isstring(L, -1)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
						"First element in each table in the ACTIONS table may only be a string - application name\n");
					goto rollback;
				}

				application = strdup(lua_tostring(L, -1));

				lua_pop(L, 1);

				if (lua_next(L, i) == 0) { /* STACK = tab1 | k1 .. kn | tab2 | k | k | v */
					app_data = strdup("");

				} else {
					if (!lua_isstring(L, -1)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
							"Second (optional) element in each table in the ACTIONS table may only be a string - app_data\n");
						free(application);
						goto rollback;
					}
					app_data = strdup(lua_tostring(L, -1));
				}

			}

			lua_settop(L, i); /* STACK = tab1 | k1 .. kn | tab2 */

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
				"ACTIONS table may only contain strings or tables\n");
			goto rollback;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
			"Dialplan: %s Action %s(%s)\n",
			switch_channel_get_name(channel), application, app_data);

		switch_caller_extension_add_application(session, extension, application, app_data);
		free(app_data);
		free(application);

		lua_pop(L, 1);
	}

	/* all went fine */
	goto done;

 rollback:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG,
		"Rollback, all applications previously added to this extension in current context/script are discarded\n");

	/* extension was created on session's memory pool, so just make a new, empty one here */
	if ((extension = switch_caller_extension_new(session, "_anon_", caller_profile->destination_number)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Memory Error!\n");
	}

 done:
	switch_safe_free(cmd);
	lua_uninit(L);
	return extension;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lua_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;
	switch_chat_application_interface_t *chat_app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "luarun", "run a script", luarun_api_function, "<script>");
	SWITCH_ADD_API(api_interface, "lua", "run a script as an api function", lua_api_function, "<script>");
	SWITCH_ADD_APP(app_interface, "lua", "Launch LUA ivr", "Run a lua ivr on a channel", lua_function, "<script>", 
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);
	SWITCH_ADD_DIALPLAN(dp_interface, "LUA", lua_dialplan_hunt);

	SWITCH_ADD_CHAT_APP(chat_app_interface, "lua", "execute a lua script", "execute a lua script", lua_chat_function, "<script>", SCAF_NONE);


	globals.pool = pool;
	do_config();

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lua_shutdown)
{
	switch_event_unbind_callback(lua_event_handler);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
