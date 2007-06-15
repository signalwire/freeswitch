#include <switch.h>
#include <switch_cpp.h>

#ifdef _MSC_VER
#pragma warning(disable:4127 4003)
#endif

#define sanity_check(x) do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)
#define sanity_check_noreturn do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return;}} while(0)
#define init_vars() do { session = NULL; channel = NULL; uuid = NULL; tts_name = NULL; voice_name = NULL; memset(&args, 0, sizeof(args)); ap = NULL;} while(0)

CoreSession::CoreSession(char *nuuid)
{
	init_vars();
    uuid = strdup(nuuid);
	if (session = switch_core_session_locate(uuid)) {
		channel = switch_core_session_get_channel(session);
    }
}

CoreSession::CoreSession(switch_core_session_t *new_session)
{
	init_vars();
	session = new_session;
	channel = switch_core_session_get_channel(session);
	switch_core_session_read_lock(session);
}

CoreSession::~CoreSession()
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::~CoreSession desctructor");

	if (session) {
		switch_core_session_rwunlock(session);
	}

	switch_safe_free(uuid);	
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
}

int CoreSession::answer()
{
    switch_status_t status;

	sanity_check(-1);
    status = switch_channel_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int CoreSession::preAnswer()
{
    switch_status_t status;
	sanity_check(-1);
    status = switch_channel_pre_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void CoreSession::hangup(char *cause)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::hangup\n");
	sanity_check_noreturn;
    switch_channel_hangup(channel, switch_channel_str2cause(cause));
}

void CoreSession::setVariable(char *var, char *val)
{
	sanity_check_noreturn;
    switch_channel_set_variable(channel, var, val);
}

char *CoreSession::getVariable(char *var)
{
	sanity_check(NULL);
    return switch_channel_get_variable(channel, var);
}

void CoreSession::execute(char *app, char *data)
{
	const switch_application_interface_t *application_interface;
	sanity_check_noreturn;

	if ((application_interface = switch_loadable_module_get_application_interface(app))) {
		begin_allow_threads();
		switch_core_session_exec(session, application_interface, data);
		end_allow_threads();
	}
}

int CoreSession::playFile(char *file, char *timer_name)
{
    switch_status_t status;
    switch_file_handle_t fh = { 0 };
	sanity_check(-1);
    if (switch_strlen_zero(timer_name)) {
        timer_name = NULL;
    }
	store_file_handle(&fh);
	begin_allow_threads();
	status = switch_ivr_play_file(session, &fh, file, ap);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

void CoreSession::setDTMFCallback(switch_input_callback_function_t cb, 
								  void *buf, 
								  uint32_t buflen)
{
	sanity_check_noreturn;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::setDTMFCallback.");
	if (cb) {
		args.buf = buf;
		args.buflen = buflen;
		args.input_callback = cb;
		ap = &args;
	} else {
		memset(&args, 0, sizeof(args));
		ap = NULL;
	}
}


int CoreSession::speak(char *text)
{
    switch_status_t status;
    switch_codec_t *codec;

	sanity_check(-1);
	if (!tts_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No TTS engine specified");
		return SWITCH_STATUS_FALSE;
	}

	if (!voice_name) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No TTS voice specified");
		return SWITCH_STATUS_FALSE;
	}

    codec = switch_core_session_get_read_codec(session);
	begin_allow_threads();
	status = switch_ivr_speak_text(session, tts_name, voice_name, codec->implementation->samples_per_second, text, ap);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void CoreSession::set_tts_parms(char *tts_name_p, char *voice_name_p)
{
	sanity_check_noreturn;
	switch_safe_free(tts_name);
	switch_safe_free(voice_name);
    tts_name = strdup(tts_name_p);
    voice_name = strdup(voice_name_p);
}

int CoreSession::getDigits(char *dtmf_buf, 
						   int len, 
						   char *terminators, 
						   char *terminator, 
						   int timeout)
{
    switch_status_t status;
	sanity_check(-1);
	begin_allow_threads();
    status = switch_ivr_collect_digits_count(session, 
											 dtmf_buf,
											 (uint32_t) len,
											 (uint32_t) len, 
											 terminators, 
											 terminator, 
											 (uint32_t) timeout);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int CoreSession::transfer(char *extension, char *dialplan, char *context)
{
    switch_status_t status;
	sanity_check(-1);
    status = switch_ivr_session_transfer(session, extension, dialplan, context);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int CoreSession::playAndGetDigits(int min_digits, 
								  int max_digits, 
								  int max_tries, 
								  int timeout, 
								  char *terminators, 
								  char *audio_files, 
								  char *bad_input_audio_files, 
								  char *dtmf_buf, 
								  char *digits_regex)
{
    switch_status_t status;
	sanity_check(-1);
	begin_allow_threads();
    status = switch_play_and_get_digits( session, 
										 (uint32_t) min_digits,
										 (uint32_t) max_digits,
										 (uint32_t) max_tries, 
										 (uint32_t) timeout, 
										 terminators, 
										 audio_files, 
										 bad_input_audio_files, 
										 dtmf_buf, 128, 
										 digits_regex);
	end_allow_threads();
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int CoreSession::streamfile(char *file, int starting_sample_count) {

    switch_status_t status;
    switch_file_handle_t fh = { 0 };
    unsigned int samps;
    unsigned int pos = 0;
	char *prebuf;

    sanity_check(-1);
    fh.samples = starting_sample_count;
	store_file_handle(&fh);

    begin_allow_threads();
    status = switch_ivr_play_file(session, &fh, file, ap);
    end_allow_threads();

	if ((prebuf = switch_channel_get_variable(this->channel, "stream_prebuffer"))) {
        int maybe = atoi(prebuf);
        if (maybe > 0) {
            fh.prebuf = maybe;
        }
	}

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;

}

bool CoreSession::ready() {

	switch_channel_t *channel;

	if (!session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "You must call the session.originate method before calling this method!\n");
		return false;
	}

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	
	return switch_channel_ready(channel) != 0;


}

void CoreSession::begin_allow_threads() { 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::begin_allow_threads() called and does nothing\n");
}

void CoreSession::end_allow_threads() { 
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CoreSession::end_allow_threads() called and does nothing\n");
}

/** \brief Store a file handle in the callback args
 * 
 * In a few of the methods like playFile and streamfile,
 * an empty switch_file_handle_t is created and passed
 * to core, and stored in callback args so that the callback
 * handler can retrieve it for pausing, ff, rewinding file ptr. 
 * 
 * \param fh - a switch_file_handle_t
 */
void CoreSession::store_file_handle(switch_file_handle_t *fh) {
    cb_state.extra = fh;  // set a file handle so callback handler can pause
    args.buf = &cb_state;     
    ap = &args;
}


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
