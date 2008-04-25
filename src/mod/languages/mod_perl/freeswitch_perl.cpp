#include "freeswitch_perl.h"

Session::Session() : CoreSession()
{
	
}

Session::Session(char *uuid) : CoreSession(uuid)
{
	
}

Session::Session(switch_core_session_t *new_session) : CoreSession(new_session)
{

}

Session::~Session() 
{
	
}


bool Session::begin_allow_threads() 
{
	return true;
}

bool Session::end_allow_threads() 
{
	return true;
}

void Session::check_hangup_hook() 
{
}

switch_status_t Session::run_dtmf_callback(void *input, switch_input_type_t itype) 
{
	return SWITCH_STATUS_FALSE;
}


#if 0
int Session::answer() {}
int Session::preAnswer() {}
void Session::hangup(char *cause) {}
void Session::setVariable(char *var, char *val) {}
const char *Session::getVariable(char *var) {}
int Session::recordFile(char *file_name, int max_len, int silence_threshold, int silence_secs) {}
void Session::setCallerData(char *var, char *val) {}
int Session::originate(CoreSession *a_leg_session, char *dest, int timeout) {}
void Session::setDTMFCallback(void *cbfunc, char *funcargs) {}
int Session::speak(char *text) {}
void Session::set_tts_parms(char *tts_name, char *voice_name) {}
int Session::collectDigits(int timeout) {}
int Session::getDigits(char *dtmf_buf, 
			  switch_size_t buflen, 
			  switch_size_t maxdigits, 
			  char *terminators, 
			  char *terminator, 
			  int timeout) {}

int Session::transfer(char *extensions, char *dialplan, char *context) {}
int Session::playAndGetDigits(int min_digits, 
					 int max_digits, 
					 int max_tries, 
					 int timeout, 
					 char *terminators,
					 char *audio_files, 
					 char *bad_input_audio_files, 
					 char *dtmf_buf, 
					 char *digits_regex) {}

int Session::streamFile(char *file, int starting_sample_count) {}
int Session::flushEvents() {}
int Session::flushDigits() {}
int Session::setAutoHangup(bool val) {}
void Session::setHangupHook(void *hangup_func) {}
bool Session::ready() {}
void Session::execute(char *app, char *data) {}
char* Session::get_uuid() {}
const switch_input_args_t& Session::get_cb_args() {}

#endif
