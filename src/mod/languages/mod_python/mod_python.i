%module freeswitch
%include "cstring.i"

%cstring_bounded_mutable(char *dtmf_buf, 128);
%cstring_bounded_mutable(char *terminator, 8);

%{
#include "switch_cpp.h"
#include "freeswitch_python.h"
%}

%include switch_cpp.h
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
   char *resultStr;
   char *funcargs;
   struct input_callback_state *cb_state;	
   switch_file_handle_t *fh = NULL;	
   PyThreadState *threadState = NULL;	
 
   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "PythonDTMFCallback\n");	

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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "cb_state->function is NOT null\n");	
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "restoring threadstate: %p\n", threadState);	
   }

   PyEval_RestoreThread(threadState);  // nasty stuff happens when py interp has no thread state

   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "restored threadstate, calling python function: %p\n", func);
	
   result = PyEval_CallObject(func, arglist);    
   
   threadState = PyEval_SaveThread();  

   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "called python function\n");

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


switch_status_t process_callback_result(char *ret, 
					struct input_callback_state *cb_state,
					switch_core_session_t *session) 
{
	
    switch_file_handle_t *fh = NULL;	   
    fh = (switch_file_handle_t *) cb_state->extra;    

    if (!fh) {
	return SWITCH_STATUS_FALSE;	
    }

    if (!ret) {
	return SWITCH_STATUS_FALSE;	
    }

    if (!strncasecmp(ret, "speed", 4)) {
	char *p;

	if ((p = strchr(ret, ':'))) {
	    p++;
	    if (*p == '+' || *p == '-') {
		int step;
		if (!(step = atoi(p))) {
		    step = 1;
		}
		fh->speed += step;
	    } else {
		int speed = atoi(p);
		fh->speed = speed;
	    }
	    return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

    } else if (!strcasecmp(ret, "pause")) {
	if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
	    switch_clear_flag(fh, SWITCH_FILE_PAUSE);
	} else {
	    switch_set_flag(fh, SWITCH_FILE_PAUSE);
	}
	return SWITCH_STATUS_SUCCESS;
    } else if (!strcasecmp(ret, "stop")) {
	return SWITCH_STATUS_FALSE;
    } else if (!strcasecmp(ret, "restart")) {
	unsigned int pos = 0;
	fh->speed = 0;
	switch_core_file_seek(fh, &pos, 0, SEEK_SET);
	return SWITCH_STATUS_SUCCESS;
    } else if (!strncasecmp(ret, "seek", 4)) {
	switch_codec_t *codec;
	unsigned int samps = 0;
	unsigned int pos = 0;
	char *p;
	codec = switch_core_session_get_read_codec(session);

	if ((p = strchr(ret, ':'))) {
	    p++;
	    if (*p == '+' || *p == '-') {
		int step;
		if (!(step = atoi(p))) {
		    step = 1000;
		}
		if (step > 0) {
		    samps = step * (codec->implementation->samples_per_second / 1000);
		    switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
		} else {
		    samps = step * (codec->implementation->samples_per_second / 1000);
		    switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
		}
	    } else {
		samps = atoi(p) * (codec->implementation->samples_per_second / 1000);
		switch_core_file_seek(fh, &pos, samps, SEEK_SET);
	    }
	}

	return SWITCH_STATUS_SUCCESS;
    }

    if (!strcmp(ret, "true") || !strcmp(ret, "undefined")) {
	return SWITCH_STATUS_SUCCESS;
    }

    return SWITCH_STATUS_FALSE;


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

