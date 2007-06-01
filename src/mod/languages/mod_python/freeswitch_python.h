#ifndef FREESWITCH_PYTHON_H
#define FREESWITCH_PYTHON_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef DOH
}
#endif

#include <switch_cpp.h>



// declaration for function that is defined in mod_python.i
extern switch_status_t PythonDTMFCallback(switch_core_session *session, 
					  void *input, 
					  switch_input_type_t itype, 
					  void *buf, 
					  unsigned int buflen);

void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);

struct input_callback_state {
    PyObject *function;
    PyThreadState *threadState;
    void *extra;
    char *funcargs; 
};

class PySession : public CoreSession {
 private:
	PyObject *dtmfCallbackFunction;
	PyThreadState *threadState;
 public:
	PySession(char *uuid) : CoreSession(uuid) {};
	PySession(switch_core_session_t *session) : CoreSession(session) {};
	~PySession();        
	int streamfile(char *file, PyObject *pyfunc, char *funcargs, int starting_sample_count);
	void begin_allow_threads();
	void end_allow_threads();

 protected:
};

#ifdef __cplusplus
}
#endif
#endif
