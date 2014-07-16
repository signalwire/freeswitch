%module freeswitch

%include ../../../../swig_common.i

/** String fix - copied from csharphead.swg with fix for multiple appdomains **/
/* Must pass -DSWIG_CSHARP_NO_STRING_HELPER to swig */

#if defined(SWIG_CSHARP_NO_STRING_HELPER)
%insert(runtime) %{

/* Callback for returning strings to C# without leaking memory */
#ifndef _MANAGED
#include <mono/jit/jit.h>
#include <mono/metadata/environment.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/threads.h>
#include <mono/metadata/debug-helpers.h>
#endif

typedef char * (SWIGSTDCALL* SWIG_CSharpStringHelperCallback)(const char *);
static SWIG_CSharpStringHelperCallback SWIG_csharp_string_callback_real = NULL;
%}

%pragma(csharp) imclasscode=%{

  protected class SWIGStringHelper {

    public delegate string SWIGStringDelegate(string message);
    static SWIGStringDelegate stringDelegate = new SWIGStringDelegate(CreateString);

    [DllImport("$dllimport", EntryPoint="SWIGRegisterStringCallback_$module")]
    public static extern void SWIGRegisterStringCallback_$module(SWIGStringDelegate stringDelegate);

    static string CreateString(string cString) {
      return cString;
    }

    static SWIGStringHelper() {
      SWIGRegisterStringCallback_$module(stringDelegate);
    }
  }

  static protected SWIGStringHelper swigStringHelper = new SWIGStringHelper();
%}

%insert(runtime) %{
#ifdef __cplusplus
extern "C" 
#endif
SWIGEXPORT void SWIGSTDCALL SWIGRegisterStringCallback_freeswitch(SWIG_CSharpStringHelperCallback callback) {
	/* Set this only once, in the main appdomain */
	if (SWIG_csharp_string_callback_real == NULL) SWIG_csharp_string_callback_real = callback;
}
char * SWIG_csharp_string_callback(const char * str) {
#ifndef _MANAGED
	// Mono won't transition appdomains properly after the callback, so we force it
	MonoDomain* dom = mono_domain_get();
	char* res = SWIG_csharp_string_callback_real(str);
	mono_domain_set(dom, true);
	return res;
#else
	return SWIG_csharp_string_callback_real(str);
#endif
}
%}
#endif // SWIG_CSHARP_NO_STRING_HELPER



/** insert the following includes into generated code so it compiles */
%{
#include "switch.h"
#include "switch_cpp.h"
#include "freeswitch_managed.h"
%}

%typemap(csclassmodifiers) ManagedSession "public partial class"
%typemap(csclassmodifiers) Event "public partial class"
%typemap(csclassmodifiers) Stream "public partial class"
%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;
%newobject API::execute;
%newobject API::executeString;

// Allow bitwise compare on flag fields
%typemap(csclassmodifiers) session_flag_t "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_application_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_asr_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_bind_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_caller_profile_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_channel_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_codec_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_core_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_core_session_message_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_directory_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_eavesdrop_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_file_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_frame_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_io_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_media_bug_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_media_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_originate_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_port_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_rtp_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_scheduler_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_speech_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_timer_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_unicast_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_vad_flag_enum_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_xml_flag_t  "[System.Flags] public enum"
%typemap(csclassmodifiers) switch_xml_section_enum_t  "[System.Flags] public enum"

// Some things we dont want exposed to managed users directly, since 
// we're gonna handle them with our own internalcall methods
%ignore dtmfDelegate;
%ignore hangupDelegate;
%ignore setHangupHook;
%ignore beginAllowThreads;
%ignore endAllowThreads;
%ignore process_callback_result;
%ignore run_dtmf_callback;
%ignore setDTMFCallback;

// These methods need a bit of wrapping help
%csmethodmodifiers CoreSession::originate "protected";

// Rename some things to make them more .NET-like
%rename (Answer) CoreSession::answer;
%rename (Hangup) CoreSession::hangup;
%rename (Ready) CoreSession::ready;
%rename (Transfer) CoreSession::transfer;
%rename (SetVariable) CoreSession::setVariable;
%rename (GetVariable) CoreSession::getVariable;
%rename (SetPrivate) CoreSession::setPrivate;
%rename (GetPrivate) CoreSession::getPrivate;
%rename (Say) CoreSession::say;
%rename (SayPhrase) CoreSession::sayPhrase;
%rename (RecordFile) CoreSession::recordFile;
%rename (SetCallerData) CoreSession::setCallerData;
%rename (CollectDigits) CoreSession::collectDigits;
%rename (GetDigits) CoreSession::getDigits;
%rename (PlayAndGetDigits) CoreSession::playAndGetDigits;
%rename (StreamFile) CoreSession::streamFile;
%rename (Execute) CoreSession::execute;
%rename (GetUuid) CoreSession::get_uuid;
%rename (HookState) CoreSession::hook_state;
%rename (InternalSession) CoreSession::session;
%rename (Speak) CoreSession::speak;
%rename (SetTtsParameters) CoreSession::set_tts_parms;
%rename (SetAutoHangup) CoreSession::setAutoHangup;


%rename (Serialize) Event::serialize;
%rename (SetPriority) Event::setPriority;
%rename (GetHeader) Event::getHeader;
%rename (GetBody) Event::getBody;
%rename (GetEventType) Event::getType;
%rename (AddBody) Event::addBody;
%rename (AddHeader) Event::addHeader;
%rename (DeleteHeader) Event::delHeader;
%rename (Fire) Event::fire;
%rename (InternalEvent) Event::event;

%rename (Write) Stream::write;
%rename (GetData) Stream::getData;

%rename (Api) API;
%rename (Execute) API::execute;
%rename (ExecuteString) API::executeString;

%rename (IvrMenu) IVRMenu;
%rename (Execute) IVRMenu::execute;
%rename (ExecuteString) API::executeString;

// Causes C2564, todo
%ignore switch_ivr_menu_action_function_t;
// todo, other errors
%ignore switch_core_session_get_event_hooks;
%ignore switch_inet_pton;
%ignore switch_xml_idx;
%ignore switch_xml_pi;

// GCC complains "ISO C++ forbids assignment of arrays"
%ignore switch_vmprintf;

// Real header includes now
%import switch_platform.i // This will give us all the macros we need to compile the other stuff

%include switch.h
%include switch_types.h
//%include switch_apr.h

%include switch_core_db.h
%include switch_regex.h
%include switch_core.h
%ignore switch_module_runtime;
%ignore switch_module_load;
%ignore switch_module_shutdown;
%include switch_loadable_module.h // note: Above three ignore lines sort out some linking issues 
%include switch_console.h // Has unsupported varargs functions
%include switch_utils.h
%include switch_caller.h
%include switch_frame.h
%include switch_module_interfaces.h
%include switch_channel.h
%include switch_buffer.h
%include switch_event.h // Varargs omitted
%include switch_resample.h
%include switch_ivr.h
%include switch_rtp.h
%include switch_log.h // switch_log_printf is omitted (varargs)
%include switch_xml.h
%include switch_core_event_hook.h
%include switch_scheduler.h
%include switch_config.h
%include switch_cpp.h
%include freeswitch_managed.h
