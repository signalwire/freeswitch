include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_managedcore
MANAGED_DIR=$(switch_srcdir)/src/mod/languages/mod_managedcore/managed

# Make these come from pkg-config or autoconf dependancy detection later
COREHOST_CFLAGS = -I/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64/5.0.10/runtimes/linux-x64/native
COREHOST_LDFLAGS = -L/usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64/5.0.10/runtimes/linux-x64/native -lnethost

mod_LTLIBRARIES = mod_managedcore.la
mod_managedcore_la_SOURCES  = mod_managedcore.cpp freeswitch_managedcore.cpp freeswitch_wrap.cxx
mod_managedcore_la_CFLAGS   = $(AM_CFLAGS) $(COREHOST_CFLAGS)
mod_managedcore_la_CXXFLAGS = $(AM_CXXFLAGS) $(COREHOST_CFLAGS)
mod_managedcore_la_CPPFLAGS = $(SWITCH_AM_CPPFLAGS)
mod_managedcore_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_managedcore_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(COREHOST_LDFLAGS)

MANAGED_SRCDIR=$(abs_srcdir)/managed
BUILT_SOURCES=$(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.dll
CS_SRC=$(MANAGED_SRCDIR)/AssemblyInfo.cs $(MANAGED_SRCDIR)/Extensions.cs $(MANAGED_SRCDIR)/Loader.cs $(MANAGED_SRCDIR)/Log.cs
CS_SRC+=$(MANAGED_SRCDIR)/ManagedSession.cs $(MANAGED_SRCDIR)/PluginInterfaces.cs
CS_SRC+=$(MANAGED_SRCDIR)/PluginManager.cs $(MANAGED_SRCDIR)/ChannelVariables.cs $(MANAGED_SRCDIR)/Util.cs
CS_SRC+=$(MANAGED_SRCDIR)/swig.cs $(MANAGED_SRCDIR)/XmlSearchBinding.cs
CS_PROJ=$(MANAGED_SRCDIR)/FreeSWITCH.ManagedCore.2019.csproj

freeswitch_managedcore.o: freeswitch_managedcore.h freeswitch_managedcore.cpp

reswig: swigclean freeswitch_wrap.cxx

$(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.dll: $(CS_SRC)
	dotnet build -c Release $(CS_PROJ)

install-data-local: $(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.dll
	mkdir -p $(DESTDIR)$(modulesdir)/managedcore
	$(INSTALL) $(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.dll $(DESTDIR)$(modulesdir)
	$(INSTALL) $(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.runtimeconfig.json $(DESTDIR)$(modulesdir)
	$(INSTALL) $(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.deps.json $(DESTDIR)$(modulesdir)

uninstall:
	rm -fr $(DESTDIR)$(modulesdir)/mod_managedcore.so
	rm -fr $(DESTDIR)$(modulesdir)/FreeSWITCH.ManagedCore.dll
	rm -fr $(DESTDIR)$(modulesdir)/FreeSWITCH.ManagedCore.runtimeconfig.json
	rm -fr $(DESTDIR)$(modulesdir)/FreeSWITCH.ManagedCore.deps.json

#clean:
#	rm -fr $(top_srcdir)/x64/Release/mod/FreeSWITCH.ManagedCore.dll

swigclean: clean
	rm -f freeswitch_wrap.cxx managed/swig.cs

freeswitch_wrap.cxx:
	swig3.0 -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managedcore -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i
	rm -f ./managed/swig.cs
	cat *.cs > ./managed/swig.cs
	rm -f *.cs
