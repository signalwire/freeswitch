include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_managed

mod_LTLIBRARIES = mod_managed.la
mod_managed_la_SOURCES  = mod_managed.cpp freeswitch_managed.cpp freeswitch_wrap.cpp
mod_managed_la_CFLAGS   = $(AM_CFLAGS)
mod_managed_la_CPPFLAGS = `/usr/bin/pkg-config mono-2 --cflags` -I$(switch_srcdir)/libs/stfu/ -I$(switch_srcdir)/libs/libteletone/src/ 
mod_managed_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_managed_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `/usr/bin/pkg-config mono-2 --libs`

freeswitch_managed.o: freeswitch_managed.h freeswitch_managed.cpp

freeswitch_wrap.o: freeswitch_wrap.cpp

freeswitch_wrap.cpp: freeswitch_wrap.cxx
	cp freeswitch_wrap.cxx freeswitch_wrap.cpp

reswig: swigclean freeswitch_wrap.cxx

local_depend:
	cd managed && $(MAKE)

install: $(DESTDIR)$(modulesdir)/mod_managed.$(LIBTOOL_LIB_EXTEN)
	mkdir -p $(DESTDIR)$(modulesdir)/managed
	cd managed && $(MAKE) INSTALL="$(LTINSTALL)" MODINSTDIR=$(modulesdir) DESTDIR=$(DESTDIR) install

uninstall:
	rm -fr $(DESTDIR)$(modulesdir)/mod_managed.so
	cd managed && $(MAKE) UNINSTALL="$(LTUNINSTALL)" MODINSTDIR=$(modulesdir) uninstall

clean:
	cd managed && $(MAKE) clean

swigclean: clean
	rm -f freeswitch_wrap.cxx freeswitch_wrap.cpp managed/swig.cs

freeswitch_wrap.cxx:
	swig2.0 -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs
