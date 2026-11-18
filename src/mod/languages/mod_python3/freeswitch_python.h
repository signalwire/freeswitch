#ifndef FREESWITCH_PYTHON_H
#define FREESWITCH_PYTHON_H

extern "C" {
#include <Python.h>
#include "mod_python_extra.h"
}
#include <switch_cpp.h>
namespace PYTHON {
	class Session:public CoreSession {
	  private:
		virtual void do_hangup_hook();
		PyObject *getPython();
		PyObject *Py;
		PyObject *Self;
		int hh;
		int mark;
		PyThreadState *TS;
	  public:
		              Session();
		              Session(char *nuuid, CoreSession * a_leg = NULL);
		     Session(switch_core_session_t *session);
		                      virtual ~ Session();

		virtual bool begin_allow_threads();
		virtual bool end_allow_threads();
		virtual void check_hangup_hook();
		virtual void destroy(void);

		virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
		void setInputCallback(PyObject * cbfunc, PyObject * funcargs = NULL);
		void unsetInputCallback(void);
		void setHangupHook(PyObject * pyfunc, PyObject * arg = NULL);
		bool ready();

		PyObject *cb_function;
		PyObject *cb_arg;
		PyObject *hangup_func;
		PyObject *hangup_func_arg;

		/* keep a pointer to parent module, internal use only */
		void setPython(PyObject * state);

		/* keeps the PyObject * version of the Session so it can be used as args to the callback */
		void setSelf(PyObject * state);

	};
}
#endif
