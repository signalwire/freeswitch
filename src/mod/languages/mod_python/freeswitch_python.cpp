#include "freeswitch_python.h"

#define sanity_check(x) do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)


int PySession::streamfile(char *file, PyObject *pyfunc, char *funcargs, int starting_sample_count)
{
    switch_status_t status;
    switch_input_args_t args = { 0 }, *ap = NULL;
    struct input_callback_state cb_state = { 0 };
    switch_file_handle_t fh = { 0 };

    sanity_check(-1);
    cb_state.funcargs = funcargs;
    fh.samples = starting_sample_count;

    if (!PyCallable_Check(pyfunc)) {
        dtmfCallbackFunction = NULL;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "DTMF function is not a python function.");
    }       
    else {
        dtmfCallbackFunction = pyfunc;
    }

    if (dtmfCallbackFunction) {
	cb_state.function = dtmfCallbackFunction;
	cb_state.extra = &fh;
	args.buf = &cb_state; 
	args.buflen = sizeof(cb_state);  // not sure what this is used for, copy mod_spidermonkey
        args.input_callback = PythonDTMFCallback;  // defined in mod_python.i, will use ptrs in cb_state
	ap = &args;
    }


    this->begin_allow_threads();
    cb_state.threadState = threadState;  // pass threadState so the dtmfhandler can pick it up
    status = switch_ivr_play_file(session, &fh, file, ap);
    this->end_allow_threads();

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}


void PySession::begin_allow_threads(void) { 
    threadState = PyEval_SaveThread();
}

void PySession::end_allow_threads(void) { 
    PyEval_RestoreThread(threadState);
}

PySession::~PySession() {
    // Should we do any cleanup here?
}

