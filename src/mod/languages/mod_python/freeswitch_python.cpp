
#include <switch.h>
#include "freeswitch_python.h"
using namespace PYTHON;

#define py_init_vars() cb_function = cb_arg = hangup_func = hangup_func_arg = NULL; hh = mark = 0; TS = NULL

Session::Session():CoreSession()
{
	py_init_vars();
}

Session::Session(char *nuuid, CoreSession *a_leg):CoreSession(nuuid, a_leg)
{
	py_init_vars();
}

Session::Session(switch_core_session_t *new_session):CoreSession(new_session)
{
	py_init_vars();
}
static switch_status_t python_hanguphook(switch_core_session_t *session_hungup);

void Session::destroy(void)
{
	
	if (!allocated) {
		return;
	}

	if (session) {
		if (!channel) {
			channel = switch_core_session_get_channel(session);
		}
		switch_channel_set_private(channel, "CoreSession", NULL);
		switch_core_event_hook_remove_state_change(session, python_hanguphook);
	}

	if (hangup_func) {
		Py_DECREF(hangup_func);
        hangup_func = NULL;
    }
	
	if (hangup_func_arg) {
		Py_DECREF(hangup_func_arg);
		hangup_func_arg = NULL;
	}

	if (cb_function) {
		Py_DECREF(cb_function);
		cb_function = NULL;
	}

	if (cb_arg) {
		Py_DECREF(cb_arg);
		cb_arg = NULL;
	}

	CoreSession::destroy();
}

Session::~Session()
{
	destroy();
}

bool Session::begin_allow_threads()
{

	do_hangup_hook();

	if (!TS) {
		TS = PyEval_SaveThread();
		if (channel) {
			switch_channel_set_private(channel, "SwapInThreadState", TS);
		}
		return true;
	}

	return false;
}

bool Session::end_allow_threads()
{

	if (!TS) {
		return false;
	}

	PyEval_RestoreThread(TS);
	TS = NULL;

	if (channel) {
		switch_channel_set_private(channel, "SwapInThreadState", NULL);
	}

	do_hangup_hook();

	return true;
}

void Session::setPython(PyObject *state)
{
	Py = state;
}

void Session::setSelf(PyObject *state)
{
	Self = state;
}

PyObject *Session::getPython()
{
	return Py;
}


bool Session::ready()
{
	bool r;

	sanity_check(false);
	r = switch_channel_ready(channel) != 0;

	/*! this is called every time ready is called as a workaround to
	  make it threadsafe.  it sets a flag, and all the places where it
	  comes in and out of threadswap, check it.  so the end result is 
	  you still get the hangup hook executed pretty soon after you
	  hangup.  */
	do_hangup_hook();

	return r;
}

void Session::check_hangup_hook()
{
	if (hangup_func && (hook_state == CS_HANGUP || hook_state == CS_ROUTING)) {
		hh++;
	}
}

void Session::do_hangup_hook()
{
	PyObject *result, *arglist;
	const char *what = hook_state == CS_HANGUP ? "hangup" : "transfer";

	if (hh && !mark) {
		mark++;

		if (hangup_func) {
			
			if (!PyCallable_Check(hangup_func)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "function not callable\n");
				return;
			}

			if (!Self) {
				mod_python_conjure_session(NULL, session);
			}
			
			if (hangup_func_arg) {
				arglist = Py_BuildValue("(OsO)", Self, what, hangup_func_arg);
			} else {
				arglist = Py_BuildValue("(Os)", Self, what);
			}

			if (!(result = PyEval_CallObject(hangup_func, arglist))) {
				PyErr_Print();
			}
			
			Py_XDECREF(arglist);
			Py_XDECREF(hangup_func_arg);
		}
	}

}

static switch_status_t python_hanguphook(switch_core_session_t *session_hungup)
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


void Session::setHangupHook(PyObject *pyfunc, PyObject *arg)
{

	if (!PyCallable_Check(pyfunc)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Hangup hook is not a python function.\n");
		return;
	}

	if (hangup_func) {
		if (session) {
			switch_core_event_hook_remove_state_change(session, python_hanguphook);
		}
		Py_XDECREF(hangup_func);
		hangup_func = NULL;
	}

	if (hangup_func_arg) {
		Py_XDECREF(hangup_func_arg);
		hangup_func_arg = NULL;
	}
	
	hangup_func = pyfunc;
	hangup_func_arg = arg;

	Py_XINCREF(hangup_func);

	if (hangup_func_arg) {
		Py_XINCREF(hangup_func_arg);
	}

	switch_channel_set_private(channel, "CoreSession", this);
	hook_state = switch_channel_get_state(channel);
	switch_core_event_hook_add_state_change(session, python_hanguphook);

}

void Session::unsetInputCallback(void)
{
	if (cb_function) {
        Py_XDECREF(cb_function);
        cb_function = NULL;
    }

    if (cb_arg) {
        Py_XDECREF(cb_arg);
        cb_arg = NULL;
    }
	
	switch_channel_set_private(channel, "CoreSession", NULL);
	args.input_callback = NULL;
	ap = NULL;
	
}

void Session::setInputCallback(PyObject *cbfunc, PyObject *funcargs)
{

	if (!PyCallable_Check(cbfunc)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Hangup hook is not a python function.\n");
		return;
	}

	if (cb_function) {
		Py_XDECREF(cb_function);
		cb_function = NULL;
	}

	if (cb_arg) {
		Py_XDECREF(cb_arg);
		cb_arg = NULL;
	}

	cb_function = cbfunc;
	cb_arg = funcargs;
	args.buf = this;
    switch_channel_set_private(channel, "CoreSession", this);

	Py_XINCREF(cb_function);

	if (cb_arg) {
		Py_XINCREF(cb_arg);
	}

    args.input_callback = dtmf_callback;
    ap = &args;

}

switch_status_t Session::run_dtmf_callback(void *input, switch_input_type_t itype)
{

	PyObject *pyresult, *arglist, *io = NULL;
	int ts = 0;
	char *str = NULL, *what = "";

	if (TS) {
		ts++;
		end_allow_threads();
	}

	if (!PyCallable_Check(cb_function)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "function not callable\n");
		return SWITCH_STATUS_FALSE;
	}
	
	if (itype == SWITCH_INPUT_TYPE_DTMF) {
		switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
		io = mod_python_conjure_DTMF(dtmf->digit, dtmf->duration);
		what = "dtmf";
	} else if (itype == SWITCH_INPUT_TYPE_EVENT){
		what = "event";
		io = mod_python_conjure_event((switch_event_t *) input);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported type!\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!Self) {
		mod_python_conjure_session(NULL, session);
	}
	
	if (cb_arg) {
		arglist = Py_BuildValue("(OsOO)", Self, what, io, cb_arg);
	} else {
		arglist = Py_BuildValue("(OsO)", Self, what, io);
	}

	if ((pyresult = PyEval_CallObject(cb_function, arglist))) {
		str = (char *) PyString_AsString(pyresult);
	} else {
		PyErr_Print();
	}

	Py_XDECREF(arglist);
	Py_XDECREF(io);

	if (ts) {
		begin_allow_threads();
	}

	if (str) {
		return process_callback_result(str);
	}

	return SWITCH_STATUS_FALSE;
}
