#ifndef FREESWITCH_LUA_H
#define FREESWITCH_LUA_H

extern "C" {
#include "lua.h"
#include <lauxlib.h>
#include <lualib.h>
#include "mod_lua_extra.h"
}
#include <switch_cpp.h>
#include <string>

#ifndef lua_pushglobaltable
#define lua_pushglobaltable(L) lua_pushvalue(L,LUA_GLOBALSINDEX)
#endif

typedef struct{
  lua_State* L;
  int idx;
}SWIGLUA_FN;

#define SWIGLUA_FN_GET(fn) {lua_pushvalue(fn.L,fn.idx);}

typedef struct{
  lua_State* L;
  int idx;
}SWIGLUA_TABLE;

#define SWIGLUA_TABLE_GET(fn) {lua_pushvalue(fn.L,fn.idx);}


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
		  bool _encode_empty_table_as_object;
		  bool _return_unformatted_json;
    public:
      JSON();
      ~JSON();
      cJSON *decode(const char *);
      std::string encode(SWIGLUA_TABLE table);
      cJSON *execute(const char *);
      cJSON *execute(SWIGLUA_TABLE table);
      std::string execute2(const char *);
      std::string execute2(SWIGLUA_TABLE table);
      void encode_empty_table_as_object(bool flag);
      void return_unformatted_json(bool flag);
      static int cJSON2LuaTable(lua_State *L, cJSON *json);
      void LuaTable2cJSON(lua_State *L, int index, cJSON **json);
  };

}
#endif
