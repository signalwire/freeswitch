
#include <switch.h>
#include "freeswitch_lua.h"
using namespace LUA;

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


void Session::destroy(void)
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

	CoreSession::destroy();
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
		lua_getfield(L, LUA_GLOBALSINDEX, uuid);
	}

}

int Session::originate(CoreSession *a_leg_session, char *dest, int timeout)
{
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
		const char *err = NULL;
		int arg_count = 2;
		mark++;

		if (!getLUA()) {
			return;
		}

		lua_getfield(L, LUA_GLOBALSINDEX, (char *) hangup_func_str);
		lua_getfield(L, LUA_GLOBALSINDEX, uuid);

		lua_pushstring(L, hook_state == CS_HANGUP ? "hangup" : "transfer");

		if (hangup_func_arg) {
			lua_getfield(L, LUA_GLOBALSINDEX, (char *) hangup_func_arg);
			arg_count++;
		}

		lua_call(L, arg_count, 1);
		err = lua_tostring(L, -1);

		if (!zstr(err)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", err);
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

			lua_getfield(L, LUA_GLOBALSINDEX, (char *) cb_function);
			lua_getfield(L, LUA_GLOBALSINDEX, uuid);

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
				lua_getfield(L, LUA_GLOBALSINDEX, (char *) cb_arg);
				arg_count++;
			}

			lua_call(L, arg_count, 1);
			ret = lua_tostring(L, -1);
			lua_pop(L, 1);

			return process_callback_result((char *) ret);
		}
		break;
	case SWITCH_INPUT_TYPE_EVENT:
		{
			switch_event_t *event = (switch_event_t *) input;
			int arg_count = 3;


			lua_getfield(L, LUA_GLOBALSINDEX, (char *) cb_function);
			lua_getfield(L, LUA_GLOBALSINDEX, uuid);
			lua_pushstring(L, "event");
			mod_lua_conjure_event(L, event, "__Input_Event__", 1);
			lua_getfield(L, LUA_GLOBALSINDEX, "__Input_Event__");

			if (!zstr(cb_arg)) {
				lua_getfield(L, LUA_GLOBALSINDEX, (char *) cb_arg);
				arg_count++;
			}

			lua_call(L, arg_count, 1);
			ret = lua_tostring(L, -1);
			lua_pop(L, 1);

			return process_callback_result((char *) ret);
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}
