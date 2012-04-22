%module freeswitch
%include "cstring.i"
%include ../../../../swig_common.i
/** 
 * tell swig to treat these variables as mutable so they
 * can be used to return values.
 * See http://www.swig.org/Doc1.3/Library.html
 */
%cstring_bounded_mutable(char *dtmf_buf, 128);
%cstring_bounded_mutable(char *terminator, 8);

%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;

/** insert the following includes into generated code so it compiles */
%{
#include "switch_cpp.h"
#include "freeswitch_python.h"
%}


%ignore SwitchToMempool;   

/**
 * tell swig to grok everything defined in these header files and
 * build all sorts of c wrappers and python shadows of the c wrappers.
 */
%include switch_swigable_cpp.h
%include freeswitch_python.h

