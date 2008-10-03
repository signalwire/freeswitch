#MOD_CFLAGS=`pkg-config --cflags --libs mono`
MOD_CFLAGS=-D_REENTRANT -pthread -I/opt/mono-1.9/lib/pkgconfig/../../include/mono-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -lmono
#LDFLAGS=`pkg-config --libs mono`
BASE=../../../..
VERBOSE=1
include $(BASE)/build/modmake.rules
LOCAL_OBJS=freeswitch_managed.o freeswitch_wrap.o
local_depend: $(LOCAL_OBJS)

freeswitch_managed.o: freeswitch_managed.h freeswitch_managed.cpp

freeswitch_wrap.o: freeswitch_wrap.cpp

freeswitch_wrap.cpp: freeswitch_wrap.cxx
	cp freeswitch_wrap.cxx freeswitch_wrap.cpp

reswig: swigclean freeswitch_wrap.cxx

swigclean: clean
	rm -f freeswitch_wrap.cxx freeswitch_wrap.cpp managed/swig.cs

freeswitch_wrap.cxx:
	swig -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs
