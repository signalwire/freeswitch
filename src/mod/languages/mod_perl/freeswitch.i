%module freeswitch
%include ../../../../swig_common.i
//%include "cstring.i"

/** 
 * tell swig to treat these variables as mutable so they
 * can be used to return values.
 * See http://www.swig.org/Doc1.3/Library.html
 */
//%cstring_bounded_mutable(char *dtmf_buf, 128);
//%cstring_bounded_mutable(char *terminator, 8);

%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;
%newobject API::execute;
%newobject API::executeString;

/** insert the following includes into generated code so it compiles */
%{
#include "switch_cpp.h"
#include "freeswitch_perl.h"
%}


%ignore SwitchToMempool;   

/**
 * tell swig to grok everything defined in these header files and
 * build all sorts of c wrappers and lua shadows of the c wrappers.
 */
%include switch_swigable_cpp.h
%include freeswitch_perl.h



