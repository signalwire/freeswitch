%typemap(newfree) char * "free($1);";
%newobject getGlobalVariable;
%rename(msleep) switch_msleep;




