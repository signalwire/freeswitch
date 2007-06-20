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



switch_status_t PythonDTMFCallback(switch_core_session *session, 
				   void *input, 
				   switch_input_type_t itype, 
				   void *buf, 
				   unsigned int buflen);


void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);

class PySession : public CoreSession {
 private:
    void *threadState;
 public:
    PySession();
    PySession(char *uuid);
    PySession(switch_core_session_t *session);
    ~PySession();        
    void setDTMFCallback(PyObject *pyfunc, char *funcargs);
    void begin_allow_threads();
    void end_allow_threads();

};


#ifdef __cplusplus
}
#endif
#endif
