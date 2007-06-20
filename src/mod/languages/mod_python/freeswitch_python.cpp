#include "freeswitch_python.h"

#define sanity_check(x) do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)
#define init_vars() do { caller_profile.source = "mod_python"; } while(0)

PySession::PySession() : CoreSession()
{
	init_vars();
}

PySession::PySession(char *uuid) : CoreSession(uuid)
{
	init_vars();
}

PySession::PySession(switch_core_session_t *new_session) : CoreSession(new_session)
{
	init_vars();
}


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

    // swap out threadstate and store in instance variable 
    threadState = (void *) PyEval_SaveThread();
    cb_state.threadState = threadState;
    args.buf = &cb_state;     
    ap = &args;
}

void PySession::end_allow_threads(void) { 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PySession::end_allow_threads() called\n");
    // swap in threadstate from instance variable saved earlier
    PyEval_RestoreThread(((PyThreadState *)threadState));
}

PySession::~PySession() {
    // Should we do any cleanup here?
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PySession::~PySession desctructor\n");
}


/* ----- functions not bound to PySession instance ------ */


switch_status_t PythonDTMFCallback(switch_core_session_t *session, 
                                        void *input, 
                                        switch_input_type_t itype, 
                                        void *buf,  
                                        unsigned int buflen)
{
   PyObject *func, *arglist;
   PyObject *result;
   char *resultStr;
   char *funcargs;
   input_callback_state_t *cb_state;	
   switch_file_handle_t *fh = NULL;	
   PyThreadState *threadState = NULL;	
 
   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PythonDTMFCallback\n");	

   if (!buf) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "buf pointer is null");	
	return SWITCH_STATUS_FALSE;
   }	
   
   cb_state = (input_callback_state *) buf;   

   func = (PyObject *) cb_state->function;
   if (!func) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "cb_state->function is null\n");	
	return SWITCH_STATUS_FALSE;
   }
   else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cb_state->function is NOT null\n");	
   }
   if (!PyCallable_Check(func)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "function not callable\n");	
	return SWITCH_STATUS_FALSE;
   }

   funcargs = (char *) cb_state->funcargs;

   arglist = Py_BuildValue("(sis)", input, itype, funcargs);
   if (!arglist) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error building arglist");	
	return SWITCH_STATUS_FALSE;
   }

   threadState = (PyThreadState *) cb_state->threadState;
   if (!threadState) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error, invalid threadstate\n");	
	return SWITCH_STATUS_FALSE;
   }
   else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "restoring threadstate: %p\n", threadState);	
   }

   PyEval_RestoreThread(threadState);  // nasty stuff happens when py interp has no thread state

   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "restored threadstate, calling python function: %p\n", func);
	
   result = PyEval_CallObject(func, arglist);    
   
   threadState = PyEval_SaveThread();  

   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "called python function\n");

   Py_DECREF(arglist);                           // Trash arglist
   if (result && result != Py_None) {                       
       resultStr = (char *) PyString_AsString(result);
       Py_XDECREF(result);
       return process_callback_result(resultStr, cb_state, session);
   }
   else {
       return SWITCH_STATUS_FALSE;	
   }


}







