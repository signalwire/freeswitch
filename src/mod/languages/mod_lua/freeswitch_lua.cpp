#include "freeswitch_lua.h"

SWITCH_BEGIN_EXTERN_C
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
SWITCH_END_EXTERN_C


Session::Session() : CoreSession()
{
	cb_function = cb_arg = hangup_func_str = NULL;
}

Session::Session(char *uuid) : CoreSession(uuid)
{
	cb_function = cb_arg = hangup_func_str = NULL;
}

Session::Session(switch_core_session_t *new_session) : CoreSession(new_session)
{
	cb_function = cb_arg = hangup_func_str = NULL;
}
static switch_status_t lua_hanguphook(switch_core_session_t *session_hungup);
Session::~Session()
{
	switch_safe_free(cb_function);
	switch_safe_free(cb_arg);
	switch_safe_free(hangup_func_str);
	switch_core_event_hook_remove_state_change(session, lua_hanguphook);
}


bool Session::begin_allow_threads() 
{
	return true;
}

bool Session::end_allow_threads() 
{
	return true;
}

void Session::check_hangup_hook() 
{
	if (hangup_func_str && (hook_state == CS_HANGUP || hook_state == CS_RING)) {
		lua_State *L;
		L = (lua_State *) getPrivate("__L");

		if (!L) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh!\n");
			return;
		}
		
		lua_getfield(L, LUA_GLOBALSINDEX, (char *)hangup_func_str);
		lua_pushstring(L, hook_state == CS_HANGUP ? "hangup" : "transfer");
		lua_call(L, 1, 0);
	}
}

static switch_status_t lua_hanguphook(switch_core_session_t *session_hungup) 
{
	switch_channel_t *channel = switch_core_session_get_channel(session_hungup);
	CoreSession *coresession = NULL;
	switch_channel_state_t state = switch_channel_get_state(channel);

	if ((coresession = (CoreSession *) switch_channel_get_private(channel, "CoreSession"))) {
		if (coresession->hook_state != state) {
			coresession->hook_state = state;
			coresession->check_hangup_hook();
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


void Session::setHangupHook(char *func) {

	sanity_check_noreturn;

	switch_safe_free(hangup_func_str);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Currently Available\n");
	func = NULL;

	if (func) {
		hangup_func_str = strdup(func);
		switch_channel_set_private(channel, "CoreSession", this);
		hook_state = switch_channel_get_state(channel);
		switch_core_event_hook_add_state_change(session, lua_hanguphook);
	}
}

void Session::setInputCallback(char *cbfunc, char *funcargs) {

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
	lua_State *L;
	const char *ret;
	
	L = (lua_State *) getPrivate("__L");
	if (!L) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Doh!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch (itype) {
    case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			char str[2] = "";
			int arg_count = 2;

			lua_getfield(L, LUA_GLOBALSINDEX, (char *)cb_function);
			lua_pushstring(L, "dtmf");			
			
			lua_newtable(L);		
			lua_pushstring(L, "digit");
			str[0] = dtmf->digit;
			lua_pushstring(L, str);
			lua_rawset(L, -3);
			
			lua_pushstring(L, "duration");
			lua_pushnumber(L, dtmf->duration);
			lua_rawset(L, -3);

			if (cb_arg) {
				lua_getfield(L, LUA_GLOBALSINDEX, (char *)cb_arg);
				arg_count++;
			}

			lua_call(L, arg_count, 1);
			ret = lua_tostring(L, -1);
			lua_pop(L, 1);

			return process_callback_result((char *)ret);
		}
		break;
    case SWITCH_INPUT_TYPE_EVENT:
		{
			switch_event_t *event = (switch_event_t *) input;
			int arg_count = 2;

			lua_getfield(L, LUA_GLOBALSINDEX, (char *)cb_function);
			lua_pushstring(L, "event");
			mod_lua_conjure_event(L, event, "__Input_Event__", 1);
			lua_getfield(L, LUA_GLOBALSINDEX, "__Input_Event__");

			if (cb_arg) {
				lua_getfield(L, LUA_GLOBALSINDEX, (char *)cb_arg);
				arg_count++;
			}

			lua_call(L, 2, 1);

            ret = lua_tostring(L, -1);
			lua_pop(L, 1);
			
            return process_callback_result((char *)ret);			
		}
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


#if 0
int Session::answer() {}
int Session::preAnswer() {}
void Session::hangup(char *cause) {}
void Session::setVariable(char *var, char *val) {}
const char *Session::getVariable(char *var) {}
int Session::recordFile(char *file_name, int max_len, int silence_threshold, int silence_secs) {}
void Session::setCallerData(char *var, char *val) {}
int Session::originate(CoreSession *a_leg_session, char *dest, int timeout) {}
void Session::setDTMFCallback(void *cbfunc, char *funcargs) {}
int Session::speak(char *text) {}
void Session::set_tts_parms(char *tts_name, char *voice_name) {}
int Session::collectDigits(int timeout) {}
int Session::getDigits(char *dtmf_buf, 
			  switch_size_t buflen, 
			  switch_size_t maxdigits, 
			  char *terminators, 
			  char *terminator, 
			  int timeout) {}

int Session::transfer(char *extensions, char *dialplan, char *context) {}
int Session::playAndGetDigits(int min_digits, 
					 int max_digits, 
					 int max_tries, 
					 int timeout, 
					 char *terminators,
					 char *audio_files, 
					 char *bad_input_audio_files, 
					 char *dtmf_buf, 
					 char *digits_regex) {}

int Session::streamFile(char *file, int starting_sample_count) {}
int Session::flushEvents() {}
int Session::flushDigits() {}
int Session::setAutoHangup(bool val) {}
void Session::setHangupHook(void *hangup_func) {}
bool Session::ready() {}
void Session::execute(char *app, char *data) {}
char* Session::get_uuid() {}
const switch_input_args_t& Session::get_cb_args() {}

#endif
