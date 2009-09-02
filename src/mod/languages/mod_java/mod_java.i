%module freeswitch

/** insert the following includes into generated code so it compiles */
%{
#include "switch_cpp.h"
#include "freeswitch_java.h"
%}

%ignore SwitchToMempool;   
%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;


// I thought we were using swig because it's easier than the alternatives :-)

%typemap(jtype) jobject dtmfCallback "org.freeswitch.DTMFCallback"
%typemap(jstype) jobject dtmfCallback "org.freeswitch.DTMFCallback"

%typemap(jtype) jobject hangupHook "org.freeswitch.HangupHook"
%typemap(jstype) jobject hangupHook "org.freeswitch.HangupHook"

// Taken from various.i definitions for BYTE
%typemap(jni) char *dtmf_buf "jbyteArray"
%typemap(jtype) char *dtmf_buf "byte[]"
%typemap(jstype) char *dtmf_buf "byte[]"
%typemap(in) char *dtmf_buf
{
    $1 = (char*) JCALL2(GetByteArrayElements, jenv, $input, 0);
    if (!$1) return 0;
}
%typemap(argout) char *dtmf_buf
{
    JCALL3(ReleaseByteArrayElements, jenv, $input, (jbyte*) $1, 0);
}
%typemap(javain) char *dtmf_buf "$javainput"
%typemap(freearg) char *dtmf_buf ""

%typemap(jni) char *terminator "jbyteArray"
%typemap(jtype) char *terminator "byte[]"
%typemap(jstype) char *terminator "byte[]"
%typemap(in) char *terminator
{
    $1 = (char*) JCALL2(GetByteArrayElements, jenv, $input, 0);
    if (!$1) return 0;
}
%typemap(argout) char *terminator
{
    JCALL3(ReleaseByteArrayElements, jenv, $input, (jbyte*) $1, 0);
}
%typemap(javain) char *terminator "$javainput"
%typemap(freearg) char *terminator ""



%include "enums.swg"
%include switch_swigable_cpp.h
%include freeswitch_java.h

