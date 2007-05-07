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
   
   func = (PyObject *) buf;               // Get Python function
   arglist = Py_BuildValue("(si)", input, itype);             // Build argument list
   result = PyEval_CallObject(func, arglist);     // Call Python
   Py_DECREF(arglist);                           // Trash arglist
   if (result) {                                 // If no errors, return double
     dres = (switch_status_t) PyInt_AsLong(result);
   }
   Py_XDECREF(result);
   return dres;
}


void console_log(char *level_str, char *msg)
{
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    if (level_str) {
        level = switch_log_str2level(level_str);
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, level, msg);
}

void console_clean_log(char *msg)
{
    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN,SWITCH_LOG_DEBUG, msg);
}


 char *api_execute(char *cmd, char *arg)
 {
	 switch_stream_handle_t stream = { 0 };
	 SWITCH_STANDARD_STREAM(stream);
	 switch_api_execute(cmd, arg, NULL, &stream);
	 return (char *) stream.data;
 }

 void api_reply_delete(char *reply)
 {
	 if (!switch_strlen_zero(reply)) {
		 free(reply);
	 }
 }


%}

