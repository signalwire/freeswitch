#ifndef FREESWITCH_LUA_H
#define FREESWITCH_LUA_H

extern "C" {
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
}
#include <switch_cpp.h>

#ifndef lua_pushglobaltable
#define lua_pushglobaltable(L) lua_pushvalue(L,LUA_GLOBALSINDEX)
#endif

typedef struct{
  lua_State* L;
  int idx;
}SWIGLUA_FN;

#define SWIGLUA_FN_GET(fn) {lua_pushvalue(fn.L,fn.idx);}


namespace LUA {
	class Session:public CoreSession {
	  private:
		virtual void do_hangup_hook();
		lua_State *getLUA();
		lua_State *L;
		int hh;
		int mark;
	  public:
		    Session();
		    Session(char *uuid, CoreSession * a_leg = NULL);
		     Session(switch_core_session_t *session);
		                     ~Session();
		                      SWITCH_MOD_DECLARE(virtual void) destroy(const char *err = NULL);

		virtual bool begin_allow_threads();
		virtual bool end_allow_threads();
		virtual void check_hangup_hook();

		virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
		void unsetInputCallback(void);
		void setInputCallback(char *cbfunc, char *funcargs = NULL);
		void setHangupHook(char *func, char *arg = NULL);
		bool ready();
		int originate(CoreSession * a_leg_session, char *dest, int timeout);

		char *cb_function;
		char *cb_arg;
		char *hangup_func_str;
		char *hangup_func_arg;
		void setLUA(lua_State * state);

	};

  class Dbh {
    protected:
      switch_cache_db_handle_t *dbh;
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
      int load_extension(const char *extension);
  };
}
#endif
