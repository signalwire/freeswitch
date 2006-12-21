#include "freeswitch_python.h"

void *globalDTMFCallbackFunction;

SessionContainer::SessionContainer(char *nuuid)
{
    uuid = nuuid;
    dtmfCallbackFunction = NULL;
    tts_name = NULL;
    voice_name = NULL;
    if ((session = switch_core_session_locate(uuid))) {
        switch_core_session_rwunlock(session);
        channel = switch_core_session_get_channel(session);
    }
}

SessionContainer::~SessionContainer()
{

}

void SessionContainer::console_log(char *level_str, char *msg)
{
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    if (level_str) {
        level = switch_log_str2level(level_str);
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, level, msg);
}

void SessionContainer::console_clean_log(char *msg)
{
    switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN,SWITCH_LOG_DEBUG, msg);
}

int SessionContainer::answer()
{
    switch_status_t status;
    
    status = switch_channel_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::pre_answer()
{
    switch_status_t status;

    switch_channel_pre_answer(channel);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::hangup(char *cause)
{
    switch_channel_hangup(channel, switch_channel_str2cause(cause));
}

void SessionContainer::set_variable(char *var, char *val)
{
    switch_channel_set_variable(channel, var, val);
}

void SessionContainer::get_variable(char *var, char *val)
{
    switch_channel_get_variable(channel, var);
}

void SessionContainer::set_state(char *state)
{
    switch_channel_state_t current_state = switch_channel_get_state(channel);

    if ((current_state = switch_channel_name_state(state)) < CS_HANGUP) {
        switch_channel_set_state(channel, current_state);
    }
}

int SessionContainer::play_file(char *file, char *timer_name)
{
    switch_status_t status;

    if (switch_strlen_zero(timer_name)) {
        timer_name = NULL;
    }

    if (!dtmfCallbackFunction) {
        status = switch_ivr_play_file(session, NULL, file, timer_name, NULL, NULL, 0);
    } 
    else {
        globalDTMFCallbackFunction = dtmfCallbackFunction;
        status = switch_ivr_play_file(session, NULL, file, timer_name, PythonDTMFCallback, NULL, 0);
    }

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::set_dtmf_callback(PyObject *pyfunc)
{
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

    if (!dtmfCallbackFunction) {
        status = switch_ivr_speak_text(session, tts_name, voice_name, NULL, 0, NULL, text, NULL, 0);
    }
    else {
        globalDTMFCallbackFunction = dtmfCallbackFunction;
        status = switch_ivr_speak_text(session, tts_name, voice_name, NULL, 0, PythonDTMFCallback, text, NULL, 0);
    }

    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

void SessionContainer::set_tts_parms(char *tts_name_p, char *voice_name_p)
{
    tts_name = tts_name_p;
    voice_name = voice_name_p;
}

int SessionContainer::get_digits(char *dtmf_buf, int len, char *terminators, char *terminator, int timeout)
{
    switch_status_t status;

    status = switch_ivr_collect_digits_count(session, dtmf_buf,(uint32_t) len,(uint32_t) len, terminators, terminator, (uint32_t) timeout);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::transfer(char *extension, char *dialplan, char *context)
{
    switch_status_t status;

    status = switch_ivr_session_transfer(session, extension, dialplan, context);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

int SessionContainer::play_and_get_digits(int min_digits, int max_digits, int max_tries, int timeout, char *terminators, 
                char *audio_files, char *bad_input_audio_files, char *dtmf_buf, char *digits_regex)
{
    switch_status_t status;

    status = switch_play_and_get_digits( session, (uint32_t) min_digits,(uint32_t) max_digits,
            (uint32_t) max_tries, (uint32_t) timeout, 
            terminators, audio_files, bad_input_audio_files, dtmf_buf, 128, digits_regex);
    return status == SWITCH_STATUS_SUCCESS ? 1 : 0;
}

