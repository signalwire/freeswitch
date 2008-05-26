#include "freeswitch_java.h"

JavaSession::JavaSession() : CoreSession()
{
}

JavaSession::JavaSession(char *uuid) : CoreSession(uuid)
{
}

JavaSession::JavaSession(switch_core_session_t *session) : CoreSession(session)
{
}

JavaSession::~JavaSession()
{
    JNIEnv *env;
    jint res;

    res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (res == JNI_OK)
    {
        if (cb_state.function)
        {
            env->DeleteGlobalRef((jobject)cb_state.function);
            cb_state.function = NULL;
            if (cb_state.funcargs)
            {
                env->DeleteGlobalRef((jobject)cb_state.funcargs);
                cb_state.funcargs = NULL;
            }
        }
        if (on_hangup)
            env->DeleteGlobalRef((jobject)on_hangup);
    }
    else
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error getting JNIEnv, memory leaked!\n");
}

bool JavaSession::begin_allow_threads()
{
    // Java uses kernel-threads
    return true;
}

bool JavaSession::end_allow_threads()
{
    // Java uses kernel-threads
    return true;
}

void JavaSession::setDTMFCallback(jobject dtmfCallback, char *funcargs)
{
    JNIEnv *env;
    jobject globalDTMFCallback = NULL;
    jstring args = NULL;
    jobject globalArgs = NULL;
    jint res;

    res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (res != JNI_OK)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error getting JNIEnv!\n");
        return;
    }

    globalDTMFCallback = env->NewGlobalRef(dtmfCallback);
    if (globalDTMFCallback == NULL)
        goto fail;

    args = env->NewStringUTF(funcargs);
    if (args == NULL)
        goto fail;
    globalArgs = env->NewGlobalRef(args);
    env->DeleteLocalRef(args);
    if (globalArgs == NULL)
        goto fail;

    // Free previous callback
    if (cb_state.function != NULL)
    {
        env->DeleteGlobalRef((jobject)cb_state.function);
        cb_state.function = NULL;
        if (cb_state.funcargs != NULL)
        {
            env->DeleteGlobalRef((jobject)cb_state.funcargs);
            cb_state.funcargs = NULL;
        }
    }

    CoreSession::setDTMFCallback(globalDTMFCallback, (char*) globalArgs);
    return;

fail:
    if (globalDTMFCallback)
        env->DeleteGlobalRef(globalDTMFCallback);
    if (globalArgs)
        env->DeleteGlobalRef(globalArgs);
}

void JavaSession::setHangupHook(jobject hangupHook)
{
    JNIEnv *env;
    jobject globalHangupHook;
    jint res;

    res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (res != JNI_OK)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error getting JNIEnv!\n");
        return;
    }

    globalHangupHook = env->NewGlobalRef(hangupHook);
    if (globalHangupHook == NULL)
        return;

    // Free previous hook
    if (on_hangup != NULL)
    {
        env->DeleteGlobalRef((jobject)on_hangup);
        on_hangup = NULL;
    }

    CoreSession::setHangupHook(globalHangupHook);
}

void JavaSession::check_hangup_hook()
{
    JNIEnv *env;
    jint res;
    bool needDetach = false;

    if (!session)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No valid session\n");
        return;
    }

    if (on_hangup == NULL)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "on_hangup is null\n");
        return;
    }

    res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (res == JNI_EDETACHED)
    {
        res = javaVM->AttachCurrentThread((void**)&env, NULL);
        if (res == JNI_OK)
            needDetach = true;
    }
    if (res == JNI_OK)
    {
        jclass klass = env->GetObjectClass((jobject)on_hangup);
        if (klass != NULL)
        {
            jmethodID onHangup = env->GetMethodID(klass, "onHangup", "()V");
            if (onHangup != NULL)
                env->CallVoidMethod((jobject)on_hangup, onHangup);

            env->DeleteLocalRef(klass);
        }
        if (needDetach)
            javaVM->DetachCurrentThread();
    }
    else
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                          "attaching thread to JVM failed, error %d\n", res);
}

switch_status_t JavaSession::run_dtmf_callback(void *input, switch_input_type_t itype)
{
    JNIEnv *env;
    jclass klass = NULL;
    jmethodID onDTMF;
    jstring digits = NULL;
    jint res;
    jstring callbackResult = NULL;
    switch_status_t status;

    if (cb_state.function == NULL)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "cb_state->function is null\n");
        return SWITCH_STATUS_FALSE;
    }

    res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (res != JNI_OK)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error getting JNIEnv!\n");
        return SWITCH_STATUS_FALSE;
    }

    klass = env->GetObjectClass((jobject)cb_state.function);
    if (klass == NULL)
        return SWITCH_STATUS_FALSE;

    onDTMF = env->GetMethodID(klass, "onDTMF", "(Ljava/lang/Object;ILjava/lang/String;)Ljava/lang/String;");
    if (onDTMF == NULL)
    {
        status = SWITCH_STATUS_FALSE;
        goto done;
    }

    if (itype == SWITCH_INPUT_TYPE_DTMF)
    {
        digits = env->NewStringUTF((char*)input);
        if (digits == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto done;
        }
        callbackResult = (jstring) env->CallObjectMethod((jobject)cb_state.function, onDTMF, digits, itype, (jstring)cb_state.funcargs);
        const char *callbackResultUTF = env->GetStringUTFChars(callbackResult, NULL);
        if (callbackResultUTF)
        {
            status = process_callback_result((char*) callbackResultUTF);
            env->ReleaseStringUTFChars(callbackResult, callbackResultUTF);
        }
        else
            status = SWITCH_STATUS_FALSE;
    }
    else if (itype == SWITCH_INPUT_TYPE_EVENT)
    {
        // :-)
        switch_event_t *switch_event = (switch_event_t*) input;
        switch_event_header *header;
        jclass Event = NULL;
        jobject event = NULL;
        jstring body = NULL;
        jmethodID constructor, setBody, addHeader;
        const char *callbackResultUTF;

        env->FindClass("org/freeswitch/Event");
        if (Event == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }
        constructor = env->GetMethodID(Event, "<init>", "()V");
        if (constructor == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }
        event = env->CallStaticObjectMethod(Event, constructor);
        if (event == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }

        setBody = env->GetMethodID(Event, "setBody", "(Ljava/lang/String;)V");
        if (setBody == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }
        body = env->NewStringUTF(switch_event->body);
        if (body == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }
        env->CallVoidMethod(event, setBody, body);
        if (env->ExceptionOccurred())
            goto cleanup;

        addHeader = env->GetMethodID(Event, "addHeader", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (addHeader == NULL)
        {
            status = SWITCH_STATUS_FALSE;
            goto cleanup;
        }
        for (header = switch_event->headers; header; header = header->next)
        {
            jstring name = NULL;
            jstring value = NULL;

            name = env->NewStringUTF(header->name);
            if (name == NULL)
                goto endloop;
            value = env->NewStringUTF(header->value);
            if (value == NULL)
                goto endloop;

            env->CallVoidMethod(event, addHeader, name, value);

        endloop:
            if (name != NULL)
                env->DeleteLocalRef(name);
            if (value != NULL)
                env->DeleteLocalRef(value);
            if (env->ExceptionOccurred())
            {
                status = SWITCH_STATUS_FALSE;
                goto cleanup;
            }
        }

        callbackResult = (jstring) env->CallObjectMethod((jobject)cb_state.function, onDTMF, event, itype, (jstring)cb_state.funcargs);
        callbackResultUTF = env->GetStringUTFChars(callbackResult, NULL);
        if (callbackResultUTF)
        {
            status = process_callback_result((char*) callbackResultUTF);
            env->ReleaseStringUTFChars(callbackResult, callbackResultUTF);
        }
        else
            status = SWITCH_STATUS_FALSE;

    cleanup:
        if (Event != NULL)
            env->DeleteLocalRef(Event);
        if (event != NULL)
            env->DeleteLocalRef(event);
        if (body != NULL)
            env->DeleteLocalRef(body);
    }

done:
    if (klass != NULL)
        env->DeleteLocalRef(klass);
    if (digits != NULL)
        env->DeleteLocalRef(digits);
    if (callbackResult != NULL)
        env->DeleteLocalRef(callbackResult);
    return status;
}

