include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_managed
MANAGED_DIR=$(switch_srcdir)/src/mod/languages/mod_managed/managed

mod_LTLIBRARIES = mod_managed.la
mod_managed_la_SOURCES  = mod_managed.cpp freeswitch_managed.cpp freeswitch_wrap.cpp
mod_managed_la_CFLAGS   = $(AM_CFLAGS)
mod_managed_la_CPPFLAGS = `/usr/bin/pkg-config mono-2 --cflags` -I$(switch_srcdir)/libs/libteletone/src/ 
mod_managed_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_managed_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `/usr/bin/pkg-config mono-2 --libs`

BUILT_SOURCES=FreeSWITCH.Managed.dll
CS_SRC=managed/AssemblyInfo.cs managed/Extensions.cs managed/Loader.cs managed/Log.cs managed/ManagedSession.cs managed/PluginInterfaces.cs
CS_SRC+=managed/PluginManager.cs managed/ScriptPluginManager.cs managed/ChannelVariables.cs managed/Util.cs
CS_SRC+=managed/swig.cs managed/XmlSearchBinding.cs

freeswitch_managed.o: freeswitch_managed.h freeswitch_managed.cpp

freeswitch_wrap.o: freeswitch_wrap.cpp

freeswitch_wrap.cpp: freeswitch_wrap.cxx
	cp freeswitch_wrap.cxx freeswitch_wrap.cpp

reswig: swigclean freeswitch_wrap.cxx

FreeSWITCH.Managed.dll: $(CS_SRC)
	dmcs -target:library -out:FreeSWITCH.Managed.dll $(CS_SRC)

install-data-local: FreeSWITCH.Managed.dll
	mkdir -p $(DESTDIR)$(modulesdir)/managed
	$(INSTALL) FreeSWITCH.Managed.dll $(DESTDIR)$(modulesdir)

uninstall:
	rm -fr $(DESTDIR)$(modulesdir)/mod_managed.so
	cd $(MANAGED_DIR) && $(MAKE) UNINSTALL="$(LTUNINSTALL)" MODINSTDIR=$(modulesdir) uninstall
	$(UNINSTALL) $(MODINSTDIR)/FreeSWITCH.Managed.dll

#clean:
#	rm -fr FreeSWITCH.Managed.dll

swigclean: clean
	rm -f freeswitch_wrap.cxx freeswitch_wrap.cpp managed/swig.cs

freeswitch_wrap.cxx:
	swig2.0 -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs
