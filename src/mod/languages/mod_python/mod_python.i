%module freeswitch
%include "cstring.i"

/** insert the following includes into generated code so it compiles */
%{
#include "switch_cpp.h"
#include "freeswitch_python.h"
%}

/**
 * tell swig to grok everything defined in these header files and
 * build all sorts of c wrappers and python shadows of the c wrappers.
 */
%include switch_cpp.h
%include freeswitch_python.h

/** hmm .. dunno why this is here */
%cstring_bounded_mutable(char *dtmf_buf, 128);
%cstring_bounded_mutable(char *terminator, 8);


