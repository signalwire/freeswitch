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

	switch_core_session_t *session;
	switch_channel_t *channel;
	unsigned int flags;
	int allocated;
	input_callback_state cb_state; // callback state, always pointed to by the buf
                                   // field in this->args
	switch_channel_state_t hook_state; // store hookstate for on_hangup callback

#if 0

	int answer();
	int preAnswer();
	virtual void hangup(char *cause);
	void setVariable(char *var, char *val);
	const char *getVariable(char *var);
	int recordFile(char *file_name, int max_len=0, int silence_threshold=0, int silence_secs=0);
	void setCallerData(char *var, char *val);
	int originate(CoreSession *a_leg_session, char *dest, int timeout=60);
	void setDTMFCallback(void *cbfunc, char *funcargs);
	int speak(char *text);
	void set_tts_parms(char *tts_name, char *voice_name);
	int collectDigits(int timeout);
	int getDigits(char *dtmf_buf, 
				  switch_size_t buflen, 
				  switch_size_t maxdigits, 
				  char *terminators, 
				  char *terminator, 
				  int timeout);

	int transfer(char *extensions, char *dialplan, char *context);
	int playAndGetDigits(int min_digits, 
						 int max_digits, 
						 int max_tries, 
						 int timeout, 
						 char *terminators,
						 char *audio_files, 
						 char *bad_input_audio_files, 
						 char *dtmf_buf, 
						 char *digits_regex);

	int streamFile(char *file, int starting_sample_count=0);
	int flushEvents();
	int flushDigits();
	int setAutoHangup(bool val);
	void setHangupHook(void *hangup_func);
	bool ready();
	void execute(char *app, char *data);
	char* get_uuid();
	const switch_input_args_t& get_cb_args();
#endif

};

#endif
