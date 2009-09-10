%module freeswitch
//%include "cstring.i"

/** 
 * tell swig to treat these variables as mutable so they
 * can be used to return values.
 * See http://www.swig.org/Doc1.3/Library.html
 */
//%cstring_bounded_mutable(char *dtmf_buf, 128);
//%cstring_bounded_mutable(char *terminator, 8);


/** insert the following includes into generated code so it compiles */
%{
#include "switch.h"
#include "switch_cpp.h"
#include "freeswitch_lua.h"
%}



%ignore SwitchToMempool;   
%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;

/**
 * tell swig to grok everything defined in these header files and
 * build all sorts of c wrappers and lua shadows of the c wrappers.
 */
%include switch_swigable_cpp.h


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
    Session(char *uuid, CoreSession *a_leg = NULL);
    Session(switch_core_session_t *session);
    ~Session();        
	virtual void destroy(void);
	
	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	virtual void check_hangup_hook();

	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
	void unsetInputCallback(void);
	void setInputCallback(char *cbfunc, char *funcargs = NULL);
	void setHangupHook(char *func, char *arg = NULL);
	bool ready();
	int originate(CoreSession *a_leg_session, char *dest, int timeout);
	
	char *cb_function;
	char *cb_arg;
	char *hangup_func_str;
	char *hangup_func_arg;
	void setLUA(lua_State *state);

};
}





