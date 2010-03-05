%{
#include "esl.h"
#include "esl_oop.h"
%}

#ifdef SWIGPHP
%newobject ESLconnection::sendRecv();
%newobject ESLconnection::api();
%newobject ESLconnection::bgapi();
%newobject ::getInfo();
%newobject ESLconnection::filter();
%newobject ::recvEvent();
%newobject ::sendEvent();
%newobject ESLconnection::recvEventTimed();
#else
%newobject ESLconnection::sendRecv;
%newobject ESLconnection::api;
%newobject ESLconnection::bgapi;
%newobject ESLconnection::getInfo;
%newobject ESLconnection::filter;
%newobject ESLconnection::sendEvent;
%newobject ESLconnection::recvEvent;
%newobject ESLconnection::recvEventTimed;
%newobject ESLconnection::execute;
%newobject ESLconnection::executeAsync;
#endif


#ifdef SWIGCSHARP
//fix C# keyword event
%rename (Event) ESLevent::event;  
// Rename some things to make them more .NET-like
%rename (SerializedString) ESLevent::serialized_string;
%rename (Mine) ESLevent::mine;
%rename (Serialize) ESLevent::serialize;
%rename (SetPriority) ESLevent::setPriority;
%rename (GetHeader) ESLevent::getHeader;
%rename (GetBody) ESLevent::getBody;
%rename (AddBody) ESLevent::addBody;
%rename (AddHeader) ESLevent::addHeader;
%rename (DelHeader) ESLevent::delHeader;
%rename (FirstHeader) ESLevent::firstHeader;
%rename (NextHeader) ESLevent::nextHeader;
%rename (SocketDescriptor) ESLconnection::socketDescriptor;
%rename (Connected) ESLconnection::connected;
%rename (GetInfo) ESLconnection::getInfo;
%rename (Send) ESLconnection::send;
%rename (SendRecv) ESLconnection::sendRecv;
%rename (Api) ESLconnection::api;
%rename (Bgapi) ESLconnection::bgapi;
%rename (SendEvent) ESLconnection::sendEvent;
%rename (RecvEvent) ESLconnection::recvEvent;
%rename (RecvEventTimed) ESLconnection::recvEventTimed;
%rename (Filter) ESLconnection::filter;
%rename (Events) ESLconnection::events;
%rename (Execute) ESLconnection::execute;
%rename (ExecuteAsync) ESLconnection::executeAsync;
%rename (SetAsyncExecute) ESLconnection::setAsyncExecute;
%rename (SetEventLock) ESLconnection::setEventLock;
%rename (Disconnect) ESLconnection::disconnect;
#endif

%include "esl_oop.h"
