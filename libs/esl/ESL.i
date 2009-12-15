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
%newobject ESLconnection::recvEventTimed();
#else
%newobject ESLconnection::sendRecv;
%newobject ESLconnection::api;
%newobject ESLconnection::bgapi;
%newobject ESLconnection::getInfo;
%newobject ESLconnection::filter;
%newobject ESLconnection::recvEvent;
%newobject ESLconnection::recvEventTimed;
%newobject ESLconnection::execute;
%newobject ESLconnection::executeAsync;
#endif

%include "esl_oop.h"
