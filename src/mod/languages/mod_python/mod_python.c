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
 *
 * mod_python.c -- Python Module
 *
 */

#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <Python.h>

#include <switch.h>

const char modname[] = "mod_python";
static int numargs=0;

static PyObject* emb_numargs(PyObject *self, PyObject *args)
{
    if(!PyArg_ParseTuple(args, ":numargs"))
        return NULL;
    return Py_BuildValue("i", numargs);
}

static PyMethodDef EmbMethods[] = {
    {"numargs", emb_numargs, METH_VARARGS,
     "Return the number of arguments received by the process."},
    {NULL, NULL, 0, NULL}
};

static void python_function(switch_core_session_t *session, char *data)
{
	char *uuid = switch_core_session_get_uuid(session);
	uint32_t ulen = strlen(uuid);
	uint32_t len = strlen((char *) data) + ulen + 2;
	char *mydata = switch_core_session_alloc(session, len);
	int argc, i;
	char *argv[5];
	char python_code[1024]; 
//	void*** tsrm_ls = NULL;

	PyObject *pName, *pModule, *pFunc;
    	PyObject *pArgs, *pValue;


	snprintf(mydata, len, "%s %s", uuid, data);

	argc = switch_separate_string(mydata, ' ',argv,(sizeof(argv) / sizeof(argv[0])));
	
	sprintf(python_code, "$uuid=\"%s\"; include(\"%s\");\n", argv[0], argv[1]);
	//python_embed_init(argc, argv, &tsrm_ls);
	//python_EMBED_START_BLOCK(argc, argv);
	//zend_eval_string(python_code, NULL, "Embedded code" TSRMLS_CC);
	//python_EMBED_END_BLOCK();
	//python_embed_shutdown(tsrm_ls);

	
    Py_Initialize();
    numargs = argc;
    Py_InitModule("emb", EmbMethods);
    pName = PyString_FromString(data);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, "main");
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
            pArgs = PyTuple_New(argc - 3);
            for (i = 0; i < argc - 3; ++i) {
                pValue = PyInt_FromLong(atoi(argv[i + 3]));
                if (!pValue) {
                    Py_DECREF(pArgs);
                    Py_DECREF(pModule);
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot convert argument\n");
                    
                }
                /* pValue reference stolen here: */
                PyTuple_SetItem(pArgs, i, pValue);
            }
            pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);
            if (pValue != NULL) {
                printf("Result of call: %ld\n", PyInt_AsLong(pValue));
                Py_DECREF(pValue);
            }
            else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Call failed\n");
                
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find function \"%s\"\n", argv[2]);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        PyErr_Print();
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load \"%s\"\n", argv[1]);
        
    }
    Py_Finalize();
    

}

static const switch_application_interface_t python_application_interface = {
	/*.interface_name */ "python",
	/*.application_function */ python_function
};

static switch_loadable_module_interface_t python_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &python_application_interface,
	/*.api_interface */ NULL,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &python_module_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Python Framework Loading...\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }
*/

/*
  If it exists, this is called in it's own thread when the module-load completes
  SWITCH_MOD_DECLARE(switch_status) switch_module_shutdown(void)
  {
  return SWITCH_STATUS_SUCCESS;
  }
*/


/* Return the number of arguments of the application command line */
