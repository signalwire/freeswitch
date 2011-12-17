%typemap(newfree) char * "free($1);";
%newobject getGlobalVariable;
