#include "freeswitch_java.h"

jobject originate_state_handler;

SWITCH_DECLARE(void) setOriginateStateHandler(jobject stateHandler)
{
        JNIEnv *env = NULL;
        jint envStatus = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
        if ( envStatus != JNI_OK ) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error getting JNIEnv!\n");
                return;
        }

        if ( stateHandler != NULL && originate_state_handler != NULL ) {
                const char* errorMessage = "Originate state handler is already registered";
                jclass exceptionClass = env->FindClass("java/util/TooManyListenersException");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, errorMessage);
                env->ThrowNew(exceptionClass, errorMessage);
        } else if ( stateHandler == NULL && originate_state_handler != NULL ) {
                env->DeleteGlobalRef(originate_state_handler);
                originate_state_handler = NULL;
        } else {
                originate_state_handler = env->NewGlobalRef(stateHandler);
                if ( originate_state_handler == NULL ) {
                        const char* errorMessage = "Unable to create global reference for state handler";
                        jclass exceptionClass = env->FindClass("java/lang/OutOfMemoryError");
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, errorMessage);
                        env->ThrowNew(exceptionClass, errorMessage);
                }
        }
}

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

switch_status_t originate_handler_method(switch_core_session_t *session, const char* method) {
	if ( originate_state_handler != NULL ) {
		JNIEnv *env = NULL;
		bool needDetach = false;

		jint envStatus = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
		if ( envStatus == JNI_EDETACHED ) {
			envStatus = javaVM->AttachCurrentThread((void**)&env, NULL);
			if ( envStatus == JNI_OK ) needDetach = true;
		}

		if ( envStatus != JNI_OK ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error getting JNIEnv!\n");
			return SWITCH_STATUS_FALSE;
		}

		jclass handlerClass = env->GetObjectClass(originate_state_handler);
		if ( handlerClass == NULL ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error getting handler class!\n");
			if ( needDetach ) javaVM->DetachCurrentThread();
			return SWITCH_STATUS_FALSE;
		}

		jint result = SWITCH_STATUS_FALSE;
		jmethodID handlerMethod = env->GetMethodID(handlerClass, method, "(Ljava/lang/String;)I");
		if ( handlerMethod != NULL ) {
			char *uuid = switch_core_session_get_uuid(session);
			jstring javaUuid = env->NewStringUTF(uuid);
			result = env->CallIntMethod(originate_state_handler, handlerMethod, javaUuid);
			env->DeleteLocalRef(javaUuid);
		}

		env->DeleteLocalRef(handlerClass);
		if ( needDetach ) javaVM->DetachCurrentThread();
		return (switch_status_t)result;
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t originate_on_init(switch_core_session_t *session) {
	return originate_handler_method(session, "onInit");
}

switch_status_t originate_on_routing(switch_core_session_t *session) {
	return originate_handler_method(session, "onRouting");
}

switch_status_t originate_on_execute(switch_core_session_t *session) {
	return originate_handler_method(session, "onExecute");
}

switch_status_t originate_on_hangup(switch_core_session_t *session) {
	if ( originate_state_handler != NULL ) {
		JNIEnv *env = NULL;
		bool needDetach = false;

		jint envStatus = javaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
		if ( envStatus == JNI_EDETACHED ) {
			envStatus = javaVM->AttachCurrentThread((void**)&env, NULL);
			if ( envStatus == JNI_OK ) needDetach = true;
                }

		if ( envStatus != JNI_OK ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error getting JNIEnv!\n");
			return SWITCH_STATUS_FALSE;
		}

		jclass handlerClass = env->GetObjectClass(originate_state_handler);
		if ( handlerClass == NULL ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error getting handler class!\n");
			if ( needDetach ) javaVM->DetachCurrentThread();
			return SWITCH_STATUS_FALSE;
		}

		jint result = SWITCH_STATUS_FALSE;
		jmethodID handlerMethod = env->GetMethodID(handlerClass, "onHangup", "(Ljava/lang/String;Ljava/lang/String;)I");
		if ( handlerMethod != NULL ) {
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *uuid = switch_core_session_get_uuid(session);
			const char *cause = switch_channel_cause2str(switch_channel_get_cause(channel));
			jstring javaUuid = env->NewStringUTF(uuid);
			jstring javaCause = env->NewStringUTF(cause);
			result = env->CallIntMethod(originate_state_handler, handlerMethod, javaUuid, javaCause);
			env->DeleteLocalRef(javaUuid);
			env->DeleteLocalRef(javaCause);
		}

		env->DeleteLocalRef(handlerClass);
		if ( needDetach ) javaVM->DetachCurrentThread();
		return (switch_status_t)result;
	}

	return SWITCH_STATUS_FALSE;
}

switch_status_t originate_on_exchange_media(switch_core_session_t *session) {
	return originate_handler_method(session, "onExchangeMedia");
}

switch_status_t originate_on_soft_execute(switch_core_session_t *session) {
	return originate_handler_method(session, "onSoftExecute");
}

switch_status_t originate_on_consume_media(switch_core_session_t *session) {
	return originate_handler_method(session, "onConsumeMedia");
}

switch_status_t originate_on_hibernate(switch_core_session_t *session) {
	return originate_handler_method(session, "onHibernate");
}

switch_status_t originate_on_reset(switch_core_session_t *session) {
	return originate_handler_method(session, "onReset");
}

switch_status_t originate_on_park(switch_core_session_t *session) {
	return originate_handler_method(session, "onPark");
}

switch_status_t originate_on_reporting(switch_core_session_t *session) {
	return originate_handler_method(session, "onReporting");
}

switch_status_t originate_on_destroy(switch_core_session_t *session) {
	return originate_handler_method(session, "onDestroy");
}

switch_state_handler_table_t originate_state_handlers = {
	/*.on_init */ &originate_on_init,
	/*.on_routing */ &originate_on_routing,
	/*.on_execute */ &originate_on_execute,
	/*.on_hangup */ &originate_on_hangup,
	/*.on_exchange_media */ &originate_on_exchange_media,
	/*.on_soft_execute */ &originate_on_soft_execute,
	/*.on_consume_media */ &originate_on_consume_media,
	/*.on_hibernate */ &originate_on_hibernate,
	/*.on_reset */ &originate_on_reset,
	/*.on_park */ &originate_on_park,
	/*.on_reporting */ &originate_on_reporting,
	/*.on_destroy */ &originate_on_destroy
};

int JavaSession::originate(JavaSession* aleg, char* destination, int timeout) {
	switch_state_handler_table_t *stateHandlers = NULL;
	if ( originate_state_handler != NULL ) stateHandlers = &originate_state_handlers;
	return CoreSession::originate(aleg, destination, timeout, stateHandlers);
}

