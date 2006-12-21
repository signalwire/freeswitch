%module freeswitch
%include "cstring.i"

%cstring_bounded_mutable(char *dtmf_buf, 128);
%cstring_bounded_mutable(char *terminator, 8);

%{
#include "freeswitch_python.h"
%}

%include freeswitch_python.h

%{

switch_status_t PythonDTMFCallback(switch_core_session_t *session, 
                                        void *input, 
                                        switch_input_type_t itype, 
                                        void *buf, 
                                        unsigned int buflen)
{
   PyObject *func, *arglist;
   PyObject *result;
   switch_status_t dres = SWITCH_STATUS_FALSE;
   
   func = (PyObject *) globalDTMFCallbackFunction;               // Get Python function
   arglist = Py_BuildValue("(sisi)",input,itype,buf,buflen);             // Build argument list
   result = PyEval_CallObject(func,arglist);     // Call Python
   Py_DECREF(arglist);                           // Trash arglist
   if (result) {                                 // If no errors, return double
     dres = (switch_status_t) PyInt_AsLong(result);
   }
   Py_XDECREF(result);
   return dres;
}

%}

