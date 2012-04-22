%module fs_elmoscript
%typemap(newfree) char * "free($1);";
%newobject getGlobalVariable;
%{
#include "switch.h"
%}

%include "/usr/local/freeswitch/include/switch.h"

