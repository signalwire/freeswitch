include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_managed
MANAGED_DIR=$(switch_srcdir)/src/mod/languages/mod_managed/managed

mod_LTLIBRARIES = mod_managed.la
mod_managed_la_SOURCES  = mod_managed.cpp freeswitch_managed.cpp freeswitch_wrap.cxx
mod_managed_la_CFLAGS   = $(AM_CFLAGS)
mod_managed_la_CXXFLAGS = $(AM_CXXFLAGS)
mod_managed_la_CPPFLAGS = `/usr/bin/pkg-config mono-2 --cflags` $(SWITCH_AM_CPPFLAGS)
mod_managed_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_managed_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `/usr/bin/pkg-config mono-2 --libs`

MANAGED_SRCDIR=$(abs_srcdir)/managed
BUILT_SOURCES=FreeSWITCH.Managed.dll
CS_SRC=$(MANAGED_SRCDIR)/AssemblyInfo.cs $(MANAGED_SRCDIR)/Extensions.cs $(MANAGED_SRCDIR)/Loader.cs $(MANAGED_SRCDIR)/Log.cs
CS_SRC+=$(MANAGED_SRCDIR)/ManagedSession.cs $(MANAGED_SRCDIR)/PluginInterfaces.cs
CS_SRC+=$(MANAGED_SRCDIR)/PluginManager.cs $(MANAGED_SRCDIR)/ScriptPluginManager.cs $(MANAGED_SRCDIR)/ChannelVariables.cs $(MANAGED_SRCDIR)/Util.cs
CS_SRC+=$(MANAGED_SRCDIR)/swig.cs $(MANAGED_SRCDIR)/XmlSearchBinding.cs

freeswitch_managed.o: freeswitch_managed.h freeswitch_managed.cpp

reswig: swigclean freeswitch_wrap.cxx

FreeSWITCH.Managed.dll: $(CS_SRC)
	dmcs -target:library -out:FreeSWITCH.Managed.dll $(CS_SRC)

install-data-local: FreeSWITCH.Managed.dll
	mkdir -p $(DESTDIR)$(modulesdir)/managed
	$(INSTALL) FreeSWITCH.Managed.dll $(DESTDIR)$(modulesdir)

uninstall:
	rm -fr $(DESTDIR)$(modulesdir)/mod_managed.so
	rm -fr $(DESTDIR)$(modulesdir)/FreeSWITCH.Managed.dll

#clean:
#	rm -fr FreeSWITCH.Managed.dll

swigclean: clean
	rm -f freeswitch_wrap.cxx managed/swig.cs

freeswitch_wrap.cxx:
	swig2.0 -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs
