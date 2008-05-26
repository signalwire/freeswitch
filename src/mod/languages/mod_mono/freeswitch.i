%module freeswitch

/** insert the following includes into generated code so it compiles */
%{
#include "switch.h"
#include "switch_cpp.h"
#include "freeswitch_mono.h"
%}

%typemap(csclassmodifiers) MonoSession "public partial class"
%typemap(csclassmodifiers) Event "public partial class"
%typemap(csclassmodifiers) Stream "public partial class"

// Some things we dont want exposed to managed users directly, since 
// we're gonna handle them with our own internalcall methods
%ignore dtmfDelegateHandle;
%ignore hangupDelegateHandle;
%ignore setHangupHook;
%ignore beginAllowThreads;
%ignore endAllowThreads;
%ignore process_callback_result;
%ignore run_dtmf_callback;
%ignore setDTMFCallback;

// Rename some things to make them more .NET-like
%rename (Answer) CoreSession::answer;
%rename (Hangup) CoreSession::hangup;
%rename (Ready) CoreSession::ready;
%rename (Transfer) CoreSession::transfer;
%rename (Originate) CoreSession::originate;
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

// Real header includes now
%import switch_platform.i // This will give us all the macros we need to compile the other stuff

%include switch.h
%include switch_types.h

%include switch_core_db.h
%include switch_regex.h
%include switch_core.h
//%include switch_loadable_module.h // todo: Sort out some linking issues
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
%include freeswitch_mono.h
