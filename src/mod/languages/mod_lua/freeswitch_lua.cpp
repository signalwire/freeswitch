
#include <switch.h>
#include "freeswitch_lua.h"
using namespace LUA;

extern "C" {
	int docall(lua_State * L, int narg, int nresults, int perror, int fatal);
};

Session::Session():CoreSession()
{
	cb_function = cb_arg = hangup_func_str = hangup_func_arg = NULL;
	hh = mark = 0;
}

Session::Session(char *nuuid, CoreSession *a_leg):CoreSession(nuuid, a_leg)
{
	cb_function = cb_arg = hangup_func_str = hangup_func_arg = NULL;
	hh = mark = 0;
}

Session::Session(switch_core_session_t *new_session):CoreSession(new_session)
{
	cb_function = cb_arg = hangup_func_str = hangup_func_arg = NULL;
	hh = mark = 0;
}
static switch_status_t lua_hanguphook(switch_core_session_t *session_hungup);


void Session::destroy(const char *err)
{

	if (!allocated) {
		return;
	}

	if (session) {
		if (!channel) {
			channel = switch_core_session_get_channel(session);
		}
		switch_channel_set_private(channel, "CoreSession", NULL);
		switch_core_event_hook_remove_state_change(session, lua_hanguphook);
	}

	switch_safe_free(hangup_func_str);
	switch_safe_free(hangup_func_arg);
	switch_safe_free(cb_function);
	switch_safe_free(cb_arg);

	unsetInputCallback();

	CoreSession::destroy();


	if (!zstr(err)) {
		lua_pushstring(L, err);
		lua_error(L);
	}

}


Session::~Session()
{
	destroy();
}

bool Session::begin_allow_threads()
{
	do_hangup_hook();
	return true;
}

bool Session::end_allow_threads()
{
	do_hangup_hook();
	return true;
}

void Session::setLUA(lua_State * state)
{
	L = state;

	if (session && allocated && uuid) {
		lua_setglobal(L, uuid);
		lua_getglobal(L, uuid);
	}

}

int Session::originate(CoreSession *a_leg_session, char *dest, int timeout)
{
	if (zstr(dest)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing destination.\n");
		return 0;
	}

	int x = CoreSession::originate(a_leg_session, dest, timeout);

	if (x) {
		setLUA(L);
	}

	return x;
}

lua_State *Session::getLUA()
{
	if (!L) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh!\n");
	}
	return L;
}


bool Session::ready()
{
	bool r;

	if (!session) {
		return false;
	}
	sanity_check(false);
	r = switch_channel_ready(channel) != 0;
	do_hangup_hook();

	return r;
}

void Session::check_hangup_hook()
{
	if (hangup_func_str && (hook_state == CS_HANGUP || hook_state == CS_ROUTING)) {
		hh++;
	}
}

void Session::do_hangup_hook()
{
	if (hh && !mark) {
		int arg_count = 2;
		mark++;

		if (!getLUA()) {
			return;
		}

		lua_getglobal(L, (char *) hangup_func_str);
		lua_getglobal(L, uuid);

		lua_pushstring(L, hook_state == CS_HANGUP ? "hangup" : "transfer");

		if (hangup_func_arg) {
			lua_getglobal(L, (char *) hangup_func_arg);
			arg_count++;
		}

		docall(L, arg_count, 1, 1, 0);

		const char *err = lua_tostring(L, -1);

		switch_channel_set_variable(channel, "lua_hangup_hook_return_val", err);

		if (!zstr(err)) {

			if (!strcasecmp(err, "exit") || !strcasecmp(err, "die")) {
				lua_error(L);
			} else {
				lua_pop(L, 1);
			}
		} else {
			lua_pop(L, 1);
		}


		if (channel) {
			switch_channel_set_private(channel, "CoreSession", NULL);
		}

		if (session) {
			switch_core_event_hook_remove_state_change(session, lua_hanguphook);
		}
		switch_safe_free(hangup_func_str);

	}
}

static switch_status_t lua_hanguphook(switch_core_session_t *session_hungup)
{
	switch_channel_t *channel = switch_core_session_get_channel(session_hungup);
	Session *coresession = NULL;
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (session_hungup) {

		channel = switch_core_session_get_channel(session_hungup);

		if (channel) {
			void *vs = switch_channel_get_private(channel, "CoreSession");
			if (vs) {
				coresession = (Session *) vs;
			}
		}

		if (!(coresession && coresession->hook_state)) {
			return SWITCH_STATUS_FALSE;
		}

		if (coresession && coresession->allocated && (state == CS_HANGUP || state == CS_ROUTING) && coresession->hook_state != state) {
			coresession->hook_state = state;
			coresession->check_hangup_hook();
			switch_core_event_hook_remove_state_change(session_hungup, lua_hanguphook);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


void Session::setHangupHook(char *func, char *arg)
{

	sanity_check_noreturn;

	switch_safe_free(hangup_func_str);
	switch_safe_free(hangup_func_arg);

	if (func) {
		hangup_func_str = strdup(func);
		if (!zstr(arg)) {
			hangup_func_arg = strdup(arg);
		}
		switch_channel_set_private(channel, "CoreSession", this);
		hook_state = switch_channel_get_state(channel);
		switch_core_event_hook_add_state_change(session, lua_hanguphook);
	}
}

void Session::unsetInputCallback(void)
{
	sanity_check_noreturn;
	switch_safe_free(cb_function);
	switch_safe_free(cb_arg);
	args.input_callback = NULL;
	ap = NULL;
	switch_channel_clear_flag_recursive(channel, CF_QUEUE_TEXT_EVENTS);
}

void Session::setInputCallback(char *cbfunc, char *funcargs)
{

	sanity_check_noreturn;

	switch_safe_free(cb_function);
	if (cbfunc) {
		cb_function = strdup(cbfunc);
	}

	switch_safe_free(cb_arg);
	if (funcargs) {
		cb_arg = strdup(funcargs);
	}

	args.buf = this;
	switch_channel_set_private(channel, "CoreSession", this);

	args.input_callback = dtmf_callback;
	ap = &args;

	switch_channel_set_flag_recursive(channel, CF_QUEUE_TEXT_EVENTS);

}

switch_status_t Session::run_dtmf_callback(void *input, switch_input_type_t itype)
{
	const char *ret;

	if (!getLUA()) {
		return SWITCH_STATUS_FALSE;;
	}

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			char str[3] = "";
			int arg_count = 3;
			int r;

			lua_getglobal(L, (char *) cb_function);
			lua_getglobal(L, uuid);

			lua_pushstring(L, "dtmf");

			lua_newtable(L);
			lua_pushstring(L, "digit");
			str[0] = dtmf->digit;
			lua_pushstring(L, str);
			lua_rawset(L, -3);

			lua_pushstring(L, "duration");
			lua_pushnumber(L, dtmf->duration);
			lua_rawset(L, -3);

			if (!zstr(cb_arg)) {
				lua_getglobal(L, (char *) cb_arg);
				arg_count++;
			}

			r = docall(L, arg_count, 1, 1, 0);

			if (!r) {
				ret = lua_tostring(L, -1);
				lua_pop(L, 1);
			} else {
				ret = "SCRIPT_ERROR";
			}

			return process_callback_result((char *) ret);
		}
		break;
	case SWITCH_INPUT_TYPE_EVENT:
		{
			switch_event_t *event = (switch_event_t *) input;
			int arg_count = 3;


			lua_getglobal(L, (char *) cb_function);
			lua_getglobal(L, uuid);
			lua_pushstring(L, "event");
			mod_lua_conjure_event(L, event, "__Input_Event__", 1);
			lua_getglobal(L, "__Input_Event__");

			if (!zstr(cb_arg)) {
				lua_getglobal(L, (char *) cb_arg);
				arg_count++;
			}

			if (!docall(L, arg_count, 1, 1, 0)) {
				ret = lua_tostring(L, -1);
				lua_pop(L, 1);
			} else {
				ret = "SCRIPT_ERROR";
			}

			return process_callback_result((char *) ret);
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


Dbh::Dbh(char *dsn, char *user, char *pass)
{
	dbh = NULL;
	err = NULL;
	char *tmp = NULL;

	if (!zstr(user) || !zstr(pass)) {
		tmp = switch_mprintf("%s%s%s%s%s", dsn,
							 zstr(user) ? "" : ":",
							 zstr(user) ? "" : user,
							 zstr(pass) ? "" : ":",
							 zstr(pass) ? "" : pass
							 );

		dsn = tmp;
	}

	if (!zstr(dsn) && switch_cache_db_get_db_handle_dsn(&dbh, dsn) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "DBH handle %p Connected.\n", (void *) dbh);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Connection failed.  DBH NOT Connected.\n");
	}

	switch_safe_free(tmp);

}

Dbh::~Dbh()
{
	if (dbh) release();

	clear_error();
}

void Dbh::clear_error()
{
	switch_safe_free(err);
}

char *Dbh::last_error()
{
	return err;
}

bool Dbh::release()
{
  if (dbh) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "DBH handle %p released.\n", (void *) dbh);
	  switch_cache_db_release_db_handle(&dbh);
	  return true;
  }

  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
  return false;
}

bool Dbh::connected()
{
	return dbh ? true : false;
}

bool Dbh::test_reactive(char *test_sql, char *drop_sql, char *reactive_sql)
{
  if (dbh) {
    if (!zstr(test_sql) && !zstr(reactive_sql)) {
      if (switch_cache_db_test_reactive(dbh, test_sql, drop_sql, reactive_sql) == SWITCH_TRUE) {
        return true;
      }
    } else {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing parameters.\n");
    }
  } else {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
  }
  return false;
}

int Dbh::query_callback(void *pArg, int argc, char **argv, char **cargv)
{
  SWIGLUA_FN *lua_fun = (SWIGLUA_FN *)pArg;
	int ret = 0;

  lua_pushvalue(lua_fun->L, lua_fun->idx); /* get the lua callback function onto the stack */

  lua_newtable(lua_fun->L); /* push a row (table) */

  for (int i = 0; i < argc; i++) {
    lua_pushstring(lua_fun->L, switch_str_nil(cargv[i]));
    lua_pushstring(lua_fun->L, switch_str_nil(argv[i]));
    lua_settable(lua_fun->L, -3);
  }

  if (docall(lua_fun->L, 1, 1, 1, 0)) {
	  return 1;
  }

  ret = lua_tonumber(lua_fun->L, -1);
  lua_pop(lua_fun->L, 1);

  if (ret != 0) {
	  return 1;
  }

  return 0; /* 0 to continue with next row */
}

bool Dbh::query(char *sql, SWIGLUA_FN lua_fun)
{
  clear_error();

  if (zstr(sql)) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing SQL query.\n");
    return false;
  }

  if (dbh) {
    if (lua_fun.L) {
      if (switch_cache_db_execute_sql_callback(dbh, sql, query_callback, &lua_fun, &err) == SWITCH_STATUS_SUCCESS) {
        return true;
      }
    } else { /* if no lua_fun arg is passed from Lua, an empty initialized struct will be sent - see freeswitch.i */
      if (switch_cache_db_execute_sql(dbh, sql, &err) == SWITCH_STATUS_SUCCESS) {
        return true;
      }
    }
  } else {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
  }
  return false;
}

int Dbh::affected_rows()
{
  if (dbh) {
    return switch_cache_db_affected_rows(dbh);
  }

  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
  return 0;
}

int Dbh::load_extension(const char *extension)
{
  if (zstr(extension)) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing extension name.\n");
    return 0;
  }

  if (dbh) {
    return switch_cache_db_load_extension(dbh, extension);
  }

  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "DBH NOT Connected.\n");
  return 0;
}

JSON::JSON()
{
	_encode_empty_table_as_object = true;
}

JSON::~JSON()
{
}

void JSON::encode_empty_table_as_object(bool flag)
{
	_encode_empty_table_as_object = flag;
}

void JSON::return_unformatted_json(bool flag)
{
	_return_unformatted_json = flag;
}

cJSON *JSON::decode(const char *str)
{
	cJSON *json = cJSON_Parse(str);
	return json;
}

#define ADDITEM(json, k, v) do { \
	if (return_array > 0) { cJSON_AddItemToArray(json, v);} else { cJSON_AddItemToObject(json, k, v); } \
} while (0)

void JSON::LuaTable2cJSON(lua_State *L, int index, cJSON **json)
{
	int return_array = -1;

    // Push another reference to the table on top of the stack (so we know
    // where it is, and this function can work for negative, positive and
    // pseudo indices
    lua_pushvalue(L, index);
    // stack now contains: -1 => table
    lua_pushnil(L);
    // stack now contains: -1 => nil; -2 => table
    while (lua_next(L, -2)) {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(L, -2);
        // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table

        const char *key = lua_tostring(L, -1);
        // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "key: %s\n", key);

		if (return_array < 0) {
			if (lua_isnumber(L, -1) && lua_tonumber(L, -1) == 1) {
				return_array = 1;
				*json = cJSON_CreateArray();
			} else {
				return_array = 0;
				*json = cJSON_CreateObject();
			}
		}

		switch_assert(*json);

		if (lua_isnumber(L, -2)) {
			ADDITEM(*json, key, cJSON_CreateNumber(lua_tonumber(L, -2)));
		} else if (lua_isstring(L, -2)) {
			ADDITEM(*json, key, cJSON_CreateString(lua_tostring(L, -2)));
		} else if (lua_isboolean(L, -2)) {
			ADDITEM(*json, key, cJSON_CreateBool(lua_toboolean(L, -2)));
		} else if (lua_isnil(L, -2)) {
			ADDITEM(*json, key, cJSON_CreateNull());
		} else if (lua_isnone(L, -2)) {
			// ADDITEM(*json, key, cJSON_CreateNone());
		} else if (lua_istable(L, -2)) {
			cJSON *child = NULL;
			LuaTable2cJSON(L, -2, &child);
			if (child) {
				ADDITEM(*json, key, child);
			} else { // empty table?
				ADDITEM(*json, key, _encode_empty_table_as_object ? cJSON_CreateObject() : cJSON_CreateArray());
			}
		}

        // pop value + copy of key, leaving original key
        lua_pop(L, 2);
        // stack now contains: -1 => key; -2 => table
    }

    // stack now contains: -1 => table (when lua_next returns 0 it pops the key
    // but does not push anything.)
    // Pop table
    lua_pop(L, 1);
    // Stack is now the same as it was on entry to this function
}

std::string JSON::encode(SWIGLUA_TABLE lua_table)
{
	lua_State *L = lua_table.L;
	cJSON *json = NULL;

	luaL_checktype(L, lua_table.idx, LUA_TTABLE);
	LuaTable2cJSON(L, -1, &json);

	if (!json) {
		json = _encode_empty_table_as_object ? cJSON_CreateObject() : cJSON_CreateArray();
	}

	char *s = _return_unformatted_json ? cJSON_PrintUnformatted(json) : cJSON_Print(json);
	std::string result = std::string(s);
	free(s);
	cJSON_Delete(json);
	return result;
}

int JSON::cJSON2LuaTable(lua_State *L, cJSON *json) {
	cJSON *current = NULL;

	if (!json) return 0;

	lua_newtable(L);

	if (json->type == cJSON_Object) {
		for (current = json->child; current; current = current->next) {
			// printf("type: %d %s\n", current->type, current->string);
			switch (current->type) {
				case cJSON_String:
					lua_pushstring(L, current->valuestring);
					lua_setfield(L, -2, current->string);
					break;
				case cJSON_Number:
					lua_pushnumber(L, current->valuedouble);
					lua_setfield(L, -2, current->string);
					break;
				case cJSON_True:
					lua_pushboolean(L, 1);
					lua_setfield(L, -2, current->string);
					break;
				case cJSON_False:
					lua_pushboolean(L, 0);
					lua_setfield(L, -2, current->string);
					break;
				case cJSON_Object:
					JSON::cJSON2LuaTable(L, current);
					lua_setfield(L, -2, current->string);
					break;
				case cJSON_Array:
					JSON::cJSON2LuaTable(L, current);
					lua_setfield(L, -2, current->string);
					break;
				default:
					break;
			}
		}
	} else if (json->type == cJSON_Array) {
		int i = 1;

		for (current = json->child; current; current = current->next) {
			// printf("array type: %d %s\n", current->type, current->valuestring);
			switch (current->type) {
				case cJSON_String:
					lua_pushinteger(L, i++);
					lua_pushstring(L, current->valuestring);
					lua_settable(L, -3);
					break;
				case cJSON_Number:
					lua_pushinteger(L, i++);
					lua_pushnumber(L, current->valuedouble);
					lua_settable(L, -3);
					break;
				case cJSON_True:
					lua_pushinteger(L, i++);
					lua_pushboolean(L, 1);
					lua_settable(L, -3);
					break;
				case cJSON_False:
					lua_pushinteger(L, i++);
					lua_pushboolean(L, 0);
					lua_settable(L, -3);
					break;
				case cJSON_Object:
					lua_pushinteger(L, i++);
					JSON::cJSON2LuaTable(L, current);
					lua_settable(L, -3);
					break;
				default:
					break;
			}
		}
	}

	return 1;
}

cJSON *JSON::execute(const char *str)
{
	cJSON *cmd = cJSON_Parse(str);
	cJSON *reply = NULL;

	if (cmd) {
		switch_json_api_execute(cmd, NULL, &reply);
	}

	cJSON_Delete(cmd);

	return reply;
}

cJSON *JSON::execute(SWIGLUA_TABLE table)
{
	lua_State *L = table.L;
	cJSON *json = NULL;
	cJSON *reply = NULL;

	luaL_checktype(L, table.idx, LUA_TTABLE);
	LuaTable2cJSON(L, -1, &json);

	switch_json_api_execute(json, NULL, &reply);
	cJSON_Delete(json);
	return reply;
}

std::string JSON::execute2(const char *str)
{
    cJSON *reply = execute(str);
    char *s = _return_unformatted_json ? cJSON_PrintUnformatted(reply) : cJSON_Print(reply);
    std::string result = std::string(s);
    free(s);
    cJSON_Delete(reply);
    return result;
}

std::string JSON::execute2(SWIGLUA_TABLE table)
{
    cJSON *reply = execute(table);
    char *s = _return_unformatted_json ? cJSON_PrintUnformatted(reply) : cJSON_Print(reply);
    std::string result = std::string(s);
    free(s);
    cJSON_Delete(reply);
    return result;
}
