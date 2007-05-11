#ifndef FREESWITCH_PYTHON_H
#define FREESWITCH_PYTHON_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef DOH
}
#endif

#include <switch.h>

extern switch_status_t PythonDTMFCallback(switch_core_session * session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);

class SessionContainer {
 private:
	switch_core_session_t *session;
	switch_channel_t *channel;
	PyObject *dtmfCallbackFunction;
	char *tts_name;
	char *voice_name;
 public:
	SessionContainer(char *uuid);
	~SessionContainer();
	char *uuid;

	int answer();
	int pre_answer();
	void hangup(char *cause);
	void set_variable(char *var, char *val);
	char *get_variable(char *var);
	int play_file(char *file, char *timer_name);
	void set_dtmf_callback(PyObject * pyfunc);
	int speak_text(char *text);
	void set_tts_parms(char *tts_name, char *voice_name);
	int get_digits(char *dtmf_buf, int len, char *terminators, char *terminator, int timeout);
	int transfer(char *extensions, char *dialplan, char *context);
	int play_and_get_digits(int min_digits, int max_digits, int max_tries, int timeout, char *terminators,
							char *audio_files, char *bad_input_audio_files, char *dtmf_buf, char *digits_regex);
	void execute(char *app, char *data);
 protected:
};

#ifdef __cplusplus
}
#endif
#endif
