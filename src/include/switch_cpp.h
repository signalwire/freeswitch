#ifndef SWITCH_CPP_H
#define SWITCH_CPP_H

SWITCH_BEGIN_EXTERN_C
#include <switch.h>

void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);

class CoreSession {
 private:
	char *uuid;
	char *tts_name;
	char *voice_name;
	switch_input_args_t args;
	switch_input_args_t *ap;
 public:
	CoreSession(char *uuid);
	CoreSession(switch_core_session_t *session);
	~CoreSession();
	switch_core_session_t *session;
	switch_channel_t *channel;
	int answer();
	int preAnswer();
	void hangup(char *cause);
	void setVariable(char *var, char *val);
	char *getVariable(char *var);
	int playFile(char *file, char *timer_name);
	void setDTMFCallback(switch_input_callback_function_t cb, void *buf, uint32_t buflen);
	int speakText(char *text);
	void set_tts_parms(char *tts_name, char *voice_name);
	int getDigits(char *dtmf_buf, int len, char *terminators, char *terminator, int timeout);
	int transfer(char *extensions, char *dialplan, char *context);
	int playAndgetDigits(int min_digits, int max_digits, int max_tries, int timeout, char *terminators,
							char *audio_files, char *bad_input_audio_files, char *dtmf_buf, char *digits_regex);
	void execute(char *app, char *data);
 protected:
};


SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

