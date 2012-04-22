LOCAL_INSERT_CFLAGS= /usr/bin/pkg-config mono-2 --cflags
LOCAL_INSERT_LDFLAGS= /usr/bin/pkg-config mono-2 --libs
#MOD_CFLAGS=-D_REENTRANT -pthread -I/usr/lib/mono -lmono
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

local_depend:
	cd managed && $(MAKE)

local_install: $(DESTDIR)$(modulesdir)/mod_managed.$(LIBTOOL_LIB_EXTEN)
	cd managed && $(MAKE) INSTALL="$(LTINSTALL)" MODINSTDIR=$(modulesdir) DESTDIR=$(DESTDIR) install

local_uninstall:
	rm -fr $(DESTDIR)$(modulesdir)/mod_managed.so
	cd managed && $(MAKE) UNINSTALL="$(LTUNINSTALL)" MODINSTDIR=$(modulesdir) uninstall

local_clean:
	cd managed && $(MAKE) clean

swigclean: clean
	rm -f freeswitch_wrap.cxx freeswitch_wrap.cpp managed/swig.cs

freeswitch_wrap.cxx:
	swig -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs

depend_install:
	mkdir -p $(DESTDIR)$(modulesdir)/managed
