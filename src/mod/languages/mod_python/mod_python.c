/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Brian Fertig <brian.fertig@convergencetek.com>
 * Johny Kadarisman <jkr888@gmail.com>
 * Traun Leyden <tleyden@branchcut.com>
 *
 * mod_python.c -- Python Module
 *
 */

#include <Python.h>

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <switch.h>


PyThreadState *mainThreadState = NULL;

void init_freeswitch(void);
static switch_api_interface_t python_run_interface;


SWITCH_MODULE_LOAD_FUNCTION(mod_python_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown);
SWITCH_MODULE_DEFINITION(mod_python, mod_python_load, mod_python_shutdown, NULL);

static void eval_some_python(char *uuid, char *args, switch_core_session_t *session)
{
	PyThreadState *tstate = NULL;
	char *dupargs = NULL;
	char *argv[128] = {0};
	int argc;
	int lead = 0;
	char *script = NULL;
	PyObject *module = NULL;
	PyObject *function = NULL;
	PyObject *arg = NULL;
	PyObject *result = NULL;

	if (args) {
	    dupargs = strdup(args);
	} else {
	    return;
	}

	assert(dupargs != NULL);
	
	if (!(argc = switch_separate_string(dupargs, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No module name specified!\n");
	    goto done;
	}

	script = argv[0];
	lead = 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Invoking py module: %s\n", script);

	tstate = PyThreadState_New(mainThreadState->interp);
	if (!tstate) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error acquiring tstate\n");
	    goto done;
	}

	// swap in thread state
	PyEval_AcquireThread(tstate);
	if (session) {
	    // record the fact that thread state is swapped in
	    switch_channel_t *channel = switch_core_session_get_channel(session);
	    switch_channel_set_private(channel, "SwapInThreadState", NULL);
	}
	init_freeswitch(); 

	// import the module
	module = PyImport_ImportModule( (char *) script);
	if (!module) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error importing module\n");
	    PyErr_Print();
	    PyErr_Clear();
	    goto done_swap_out;
	}	        

	// reload the module
	module = PyImport_ReloadModule(module);
	if (!module) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error reloading module\n");
	    PyErr_Print();
	    PyErr_Clear();
	    goto done_swap_out;
	}	        

	// get the handler function to be called
	function = PyObject_GetAttrString(module, "handler");
	if (!function) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Module does not define handler(uuid)\n");
	    PyErr_Print();
	    PyErr_Clear();
	    goto done_swap_out;
	}	        

	if (uuid) {
	    // build a tuple to pass the args, the uuid of session
	    arg = Py_BuildValue("(s)", uuid);
	    if (!arg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error building args\n");
		PyErr_Print();
		PyErr_Clear();
		goto done_swap_out;
	    }
	}
	else {
	    arg = PyTuple_New(1);
	    PyObject *nada = Py_BuildValue("");
	    PyTuple_SetItem(arg, 0, nada);
	}

	// invoke the handler 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Call python script \n");
	result = PyEval_CallObjectWithKeywords(function, arg, (PyObject *)NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Finished calling python script \n");

	// check the result and print out any errors
	if (!result) {
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error calling python script\n");
	    PyErr_Print();
	    PyErr_Clear();
	}

	goto done_swap_out;

 done:
	switch_safe_free(dupargs);

 done_swap_out:
	// decrement ref counts 
	Py_XDECREF(module);
	Py_XDECREF(function);
	Py_XDECREF(arg);
	Py_XDECREF(result);

	// swap out thread state
	if (session) {
	    // record the fact that thread state is swapped in
	    switch_channel_t *channel = switch_core_session_get_channel(session);
	    PyThreadState *swapin_tstate = (PyThreadState *) switch_channel_get_private(channel, "SwapInThreadState");
	    // so lets assume nothing in the python script swapped any thread state in
            // or out .. thread state will currently be swapped in, and the SwapInThreadState 
	    // will be null
	    if (swapin_tstate == NULL) {
		// swap it out
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Threadstate mod_python.c swap-out! \n");
		// PyEval_ReleaseThread(cur_tstate);
		swapin_tstate = (void *) PyEval_SaveThread();
		switch_channel_set_private(channel, "SwapInThreadState", (void *) swapin_tstate);
	    }
	    else {
		// thread state is already swapped out, so, nothing for us to do
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "according to chan priv data, already swapped out \n");
	    }
 	}
	else {
	    // they ran python script from cmd line, behave a bit differently (untested)
	    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Threadstate mod_python.c swap-out! \n");
	    PyEval_ReleaseThread(tstate);
	}

	switch_safe_free(dupargs);


}

SWITCH_STANDARD_APP(python_function)
{
	eval_some_python(switch_core_session_get_uuid(session), (char *)data, session);
	
}

struct switch_py_thread {
	char *args;
	switch_memory_pool_t *pool;
};

static void *SWITCH_THREAD_FUNC py_thread_run(switch_thread_t *thread, void *obj)
{
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt = (struct switch_py_thread *) obj;

	eval_some_python(NULL, strdup(pt->args), NULL);

	pool = pt->pool;
	switch_core_destroy_memory_pool(&pool);

	return NULL;
}

SWITCH_STANDARD_API(launch_python)
{
	switch_thread_t *thread;
    switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	struct switch_py_thread *pt;

	if (switch_strlen_zero(cmd)) {
		stream->write_function(stream, "USAGE: %s\n", python_run_interface.syntax);
		return SWITCH_STATUS_SUCCESS;
	}
	
	switch_core_new_memory_pool(&pool);
	assert(pool != NULL);

	pt = switch_core_alloc(pool, sizeof(*pt));
	assert(pt != NULL);

	pt->pool = pool;
	pt->args = switch_core_strdup(pt->pool, cmd);
	
    switch_threadattr_create(&thd_attr, pt->pool);
    switch_threadattr_detach_set(thd_attr, 1);
    switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, thd_attr, py_thread_run, pt, pt->pool);

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_application_interface_t python_application_interface = {
	/*.interface_name */ "python",
	/*.application_function */ python_function,
	NULL, NULL, NULL,
	/* flags */ SAF_NONE,
	/* should we support no media mode here?  If so, we need to detect the mode, and either disable the media functions or indicate media if/when we need */
	/*.next */ NULL
};

static switch_api_interface_t python_run_interface = {
	/*.interface_name */ "python",
	/*.desc */ "run a python script",
	/*.function */ launch_python,
	/*.syntax */ "python </path/to/script>",
	/*.next */ NULL
};

static switch_loadable_module_interface_t python_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &python_application_interface,
	/*.api_interface */ &python_run_interface,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MODULE_LOAD_FUNCTION(mod_python_load)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &python_module_interface;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");
	
	if (!Py_IsInitialized()) {

	    // initialize python system
	    Py_Initialize();

	    // create GIL and a threadstate
	    PyEval_InitThreads();

	    // save threadstate since it's interp field will be needed
	    // to create new threadstates, and will be needed for shutdown
	    mainThreadState = PyThreadState_Get();
	    
	    // swap out threadstate since the call threads will create
	    // their own and swap in their threadstate
	    PyThreadState_Swap(NULL); 

	    // release GIL
	    PyEval_ReleaseLock();	
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_python_shutdown)
{
    PyInterpreterState *mainInterpreterState;
    PyThreadState *myThreadState;

    PyEval_AcquireLock();
    mainInterpreterState = mainThreadState->interp;
    myThreadState = PyThreadState_New(mainInterpreterState);
    PyThreadState_Swap(myThreadState);
    PyEval_ReleaseLock();

    Py_Finalize();
    PyEval_ReleaseLock();
    return SWITCH_STATUS_SUCCESS;

}

/* Return the number of arguments of the application command line */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
