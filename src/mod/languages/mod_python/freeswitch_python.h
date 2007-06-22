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



typedef enum {
	S_SWAPPED_IN = (1 << 0),
	S_SWAPPED_OUT = (1 << 1)
} swap_state_t;

void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);

class PySession : public CoreSession {
 private:
    void *threadState;
    int swapstate;
 public:
    PySession();
    PySession(char *uuid);
    PySession(switch_core_session_t *session);
    ~PySession();        
    void setDTMFCallback(PyObject *pyfunc, char *funcargs);
    void setHangupHook(PyObject *pyfunc);
    void check_hangup_hook();
    void hangup(char *cause);
    bool begin_allow_threads();
    bool end_allow_threads();

    switch_status_t run_dtmf_callback(void *input, 
				      switch_input_type_t itype);

};


#ifdef __cplusplus
}
#endif
#endif
