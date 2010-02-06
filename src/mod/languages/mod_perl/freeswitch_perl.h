#ifndef FREESWITCH_LUA_H
#define FREESWITCH_LUA_H

extern "C" {
#ifdef __ICC
#pragma warning (disable:1419)
#endif
#ifdef _MSC_VER
#include <perlibs.h>
#pragma comment(lib, PERL_LIB)
#endif

#include <EXTERN.h>
#include <perl.h>
#include <switch.h>
}
#include <switch_cpp.h>
namespace PERL {
	class Session:public CoreSession {
	  private:
		virtual void do_hangup_hook();
		PerlInterpreter *getPERL();
		PerlInterpreter *my_perl;
		int hh;
		int mark;
		SV *me;
	  public:
		   Session();
		   Session(char *uuid, CoreSession * a_leg = NULL);
		     Session(switch_core_session_t *session);
		                     ~Session();

		virtual void destroy(void);
		virtual bool begin_allow_threads();
		virtual bool end_allow_threads();
		virtual void check_hangup_hook();

		virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
		void setME(SV * p);
		void setInputCallback(char *cbfunc = "on_input", char *funcargs = NULL);
		void unsetInputCallback(void);
		void setHangupHook(char *func, char *arg = NULL);
		bool ready();
		char *suuid;
		char *cb_function;
		char *cb_arg;
		char *hangup_func_str;
		char *hangup_func_arg;
		void setPERL(PerlInterpreter * pi);
	};
}
#endif
