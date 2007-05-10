#include "freeswitch_python.h"

#define sanity_check(x) do { if (!session) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)

SessionContainer::SessionContainer(char *nuuid)
{
    uuid = nuuid;
    dtmfCallbackFunction = NULL;
    tts_name = NULL;
    voice_name = NULL;

       if (session = switch_core_session_locate(uuid)) {
	        channel = switch_core_session_get_channel(session);
    }
}

SessionContainer::~SessionContainer()
{

	if (session) {
		switch_core_session_rwunlock(session);
	}
}

int SessionContainer::answer()
{
    switch_status_t status;

	sanity_check(-1);
    status = switch_channel_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::pre_answer()
{
    switch_status_t status;
	sanity_check(-1);
    switch_channel_pre_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::hangup(char *cause)
{
	sanity_check();
    switch_channel_hangup(channel, switch_channel_str2cause(cause));
}

void SessionContainer::set_variable(char *var, char *val)
{
	sanity_check();
    switch_channel_set_variable(channel, var, val);
}

void SessionContainer::get_variable(char *var, char *val)
{
	sanity_check();
    switch_channel_get_variable(channel, var);
}

void SessionContainer::execute(char *app, char *data)
{
	const switch_application_interface_t *application_interface;
	sanity_check();

	if ((application_interface = switch_loadable_module_get_application_interface(app))) {
		switch_core_session_exec(session, application_interface, data);
	}
}

int SessionContainer::play_file(char *file, char *timer_name)
{
    switch_status_t status;
    switch_input_args_t args = { 0 }, *ap = NULL;
	sanity_check(-1);

    if (switch_strlen_zero(timer_name)) {
        timer_name = NULL;
    }

    if (dtmfCallbackFunction) {
		args.buf = dtmfCallbackFunction;
        args.input_callback = PythonDTMFCallback;
		ap = &args;
    }

    Py_BEGIN_ALLOW_THREADS
	status = switch_ivr_play_file(session, NULL, file, ap);
    Py_END_ALLOW_THREADS

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::set_dtmf_callback(PyObject *pyfunc)
{
	sanity_check();
    if (!PyCallable_Check(pyfunc)) {
        dtmfCallbackFunction = NULL;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "DTMF function is not a python function.");
    }       
    else {
        dtmfCallbackFunction = pyfunc;
    }
}

int SessionContainer::speak_text(char *text)
{
    switch_status_t status;
    switch_codec_t *codec;
    switch_input_args_t args = { 0 }, *ap = NULL;

	sanity_check(-1);

    codec = switch_core_session_get_read_codec(session);
    if (dtmfCallbackFunction) {
		args.buf = dtmfCallbackFunction;
        args.input_callback = PythonDTMFCallback;
		ap = &args;
    }

    Py_BEGIN_ALLOW_THREADS
	status = switch_ivr_speak_text(session, tts_name, voice_name, codec->implementation->samples_per_second, text, ap);
    Py_END_ALLOW_THREADS

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::set_tts_parms(char *tts_name_p, char *voice_name_p)
{
	sanity_check();
    tts_name = tts_name_p;
    voice_name = voice_name_p;
}

int SessionContainer::get_digits(char *dtmf_buf, int len, char *terminators, char *terminator, int timeout)
{
    switch_status_t status;
	sanity_check(-1);

    Py_BEGIN_ALLOW_THREADS
    status = switch_ivr_collect_digits_count(session, dtmf_buf,(uint32_t) len,(uint32_t) len, terminators, terminator, (uint32_t) timeout);
    Py_END_ALLOW_THREADS

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::transfer(char *extension, char *dialplan, char *context)
{
    switch_status_t status;
	sanity_check(-1);

    Py_BEGIN_ALLOW_THREADS
    status = switch_ivr_session_transfer(session, extension, dialplan, context);
    Py_END_ALLOW_THREADS

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::play_and_get_digits(int min_digits, int max_digits, int max_tries, int timeout, char *terminators, 
                char *audio_files, char *bad_input_audio_files, char *dtmf_buf, char *digits_regex)
{
    switch_status_t status;
	sanity_check(-1);

    Py_BEGIN_ALLOW_THREADS
    status = switch_play_and_get_digits( session, (uint32_t) min_digits,(uint32_t) max_digits,
            (uint32_t) max_tries, (uint32_t) timeout, 
            terminators, audio_files, bad_input_audio_files, dtmf_buf, 128, digits_regex);
    Py_END_ALLOW_THREADS

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}


