#ifndef FREESWITCH_LUA_H
#define FREESWITCH_LUA_H

extern "C" {
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
}

#include <switch_cpp.h>

namespace LUA {
class Session : public CoreSession {
 private:
	virtual void do_hangup_hook();
	lua_State *getLUA();
	lua_State *L;
	int hh;
	int mark;
 public:
    Session();
    Session(char *uuid);
    Session(switch_core_session_t *session);
    ~Session();        
	
	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	virtual void check_hangup_hook();

	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
	void setInputCallback(char *cbfunc, char *funcargs);
	void setHangupHook(char *func, char *arg = NULL);
	bool ready();
	
	char *cb_function;
	char *cb_arg;
	char *hangup_func_str;
	char *hangup_func_arg;
	void setLUA(lua_State *state);
};
}
#endif
