#ifndef FREESWITCH_PYTHON_H
#define FREESWITCH_PYTHON_H

#include <switch_cpp.h>

void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);


class Session : public CoreSession {
 private:
 public:
    Session();
    Session(char *uuid);
    Session(switch_core_session_t *session);
    ~Session();        

	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	virtual void check_hangup_hook();
	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);

};

#endif
