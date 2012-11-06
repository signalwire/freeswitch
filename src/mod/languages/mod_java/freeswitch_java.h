#ifndef FREESWITCH_JAVA_H
#define FREESWITCH_JAVA_H

#include <switch_cpp.h>
#include <jni.h>

extern JavaVM *javaVM;

#ifdef __cplusplus
extern "C" {
#endif

SWITCH_DECLARE(void) setOriginateStateHandler(jobject stateHandler);

#ifdef __cplusplus
}
#endif

class JavaSession:public CoreSession {
  public:
	JavaSession();
	JavaSession(char *uuid);
	     JavaSession(switch_core_session_t *session);
	                      virtual ~ JavaSession();

	virtual bool begin_allow_threads();
	virtual bool end_allow_threads();
	void setDTMFCallback(jobject dtmfCallback, char *funcargs);
	void setHangupHook(jobject hangupHook);
	virtual void check_hangup_hook();
	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype);
	int originate(JavaSession* aleg, char* destination, int timeout);
};

#endif
