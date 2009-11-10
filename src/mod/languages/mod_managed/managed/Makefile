all: FreeSWITCH.Managed.dll

clean:
	rm -fr FreeSWITCH.Managed.dll

FreeSWITCH.Managed.dll: AssemblyInfo.cs Extensions.cs Loader.cs Log.cs ManagedSession.cs PluginInterfaces.cs PluginManager.cs ScriptPluginManager.cs swig.cs
	gmcs -target:library -out:FreeSWITCH.Managed.dll AssemblyInfo.cs Extensions.cs Loader.cs Log.cs ManagedSession.cs PluginInterfaces.cs PluginManager.cs ScriptPluginManager.cs ChannelVariables.cs Util.cs swig.cs XmlSearchBinding.cs

install: FreeSWITCH.Managed.dll
	$(INSTALL) FreeSWITCH.Managed.dll $(DESTDIR)$(MODINSTDIR)

uninstall:
	$(UNINSTALL) $(MODINSTDIR)/FreeSWITCH.Managed.dll

