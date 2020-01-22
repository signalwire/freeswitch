%module freeswitch
%include ../../../../swig_common.i
//%include "cstring.i"
%include std_string.i

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


%typemap(in, checkfn="lua_istable") SWIGLUA_TABLE {
  $1.L = L;
  $1.idx = $input;
}

%typemap(typecheck) SWIGLUA_TABLE {
  $1 = lua_istable(L, $input);
}

%typemap(out) cJSON * {
  SWIG_arg += LUA::JSON::cJSON2LuaTable(L, result);
  cJSON_Delete(result);
}

/* Lua function typemap */
%typemap(in, checkfn = "lua_isfunction") SWIGLUA_FN {
  $1.L = L;
  $1.idx = $input;
}

%typemap(default) SWIGLUA_FN {
  SWIGLUA_FN default_swiglua_fn = { 0 };
  $1 = default_swiglua_fn;
}

%ignore SwitchToMempool;   
%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;
%newobject Dbh;
%newobject API::execute;
%newobject API::executeString;
%newobject CoreSession::playAndDetectSpeech;
%newobject JSON;
%newobject JSON::decode;

%include "typemaps.i"
%apply int *OUTPUT { int *len };

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
	virtual void destroy(const char *err = NULL);
	
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

class Dbh {
  private:
    switch_cache_db_handle_t *dbh;
    char *err;
    bool m_connected;
    static int query_callback(void *pArg, int argc, char **argv, char **cargv);
  public:
    Dbh(char *dsn, char *user = NULL, char *pass = NULL);
    ~Dbh();
    bool release();
    bool connected();
    bool test_reactive(char *test_sql, char *drop_sql = NULL, char *reactive_sql = NULL);
    bool query(char *sql, SWIGLUA_FN lua_fun);
    int affected_rows();
    char *last_error();
    void clear_error();
    int load_extension(const char *extension);
};

class JSON {
  private:
  public:
    JSON();
    ~JSON();
    cJSON *decode(const char *str);
    std::string encode(SWIGLUA_TABLE lua_table);
    cJSON *execute(const char *);
    cJSON *execute(SWIGLUA_TABLE table);
    std::string execute2(const char *);
    std::string execute2(SWIGLUA_TABLE table);
    void encode_empty_table_as_object(bool flag);
    void return_unformatted_json(bool flag);
};

}

