%{
#include "esl.h"
#include "esl_oop.h"
%}

%newobject ESLconnection::sendRecv;
%newobject ESLconnection::api;
%newobject ESLconnection::bgapi;
%newobject ESLconnection::getInfo;
%newobject ESLconnection::filter;
%newobject ESLconnection::recvEvent;
%newobject ESLconnection::recvEventTimed;

%include "esl_oop.h"
