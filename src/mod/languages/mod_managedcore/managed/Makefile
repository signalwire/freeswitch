all: FreeSWITCH.ManagedCore.dll

clean:
	rm -fr FreeSWITCH.ManagedCore.dll

FreeSWITCH.ManagedCore.dll: AssemblyInfo.cs Extensions.cs Loader.cs Log.cs ManagedSession.cs PluginInterfaces.cs PluginManager.cs ChannelVariables.cs Util.cs swig.cs XmlSearchBinding.cs
	dotnet build ./managed/FreeSWITCH.ManagedCore.2019.csproj

install: FreeSWITCH.ManagedCore.dll
	$(INSTALL) FreeSWITCH.ManagedCore.dll $(DESTDIR)$(MODINSTDIR)

uninstall:
	$(UNINSTALL) $(MODINSTDIR)/FreeSWITCH.ManagedCore.dll

