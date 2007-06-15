#include "freeswitch_python.h"

#define sanity_check(x) do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)


void PySession::setDTMFCallback(PyObject *pyfunc, char *funcargs)
{
    sanity_check();

    if (!PyCallable_Check(pyfunc)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "DTMF function is not a python function.");
    }
    else {
	cb_state.funcargs = funcargs;
	cb_state.function = (void *) pyfunc;

	args.buf = &cb_state; 
	args.buflen = sizeof(cb_state);  // not sure what this is used for, copy mod_spidermonkey
        
	// we cannot set the actual callback to a python function, because
	// the callback is a function pointer with a specific signature.
	// so, set it to the following c function which will act as a proxy,
	// finding the python callback in the args callback args structure
	args.input_callback = PythonDTMFCallback;  // defined in mod_python.i
	ap = &args;

    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "dtmf callback was set, pyfunc: %p.  cb_state: %p\n", pyfunc, &cb_state);

}


void PySession::begin_allow_threads(void) { 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PySession::begin_allow_threads() called\n");

    threadState = (void *) PyEval_SaveThread();
    cb_state.threadState = threadState;
    // cb_state.extra = &fh;
    args.buf = &cb_state;     
    ap = &args;
}

void PySession::end_allow_threads(void) { 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PySession::end_allow_threads() called\n");
    PyEval_RestoreThread(((PyThreadState *)threadState));
}

PySession::~PySession() {
    // Should we do any cleanup here?
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PySession::~PySession desctructor\n");
}



