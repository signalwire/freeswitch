all: FreeSWITCH.Managed.dll

clean:
	rm -fr FreeSWITCH.Managed.dll

FreeSWITCH.Managed.dll: Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs
	gmcs -target:library -out:FreeSWITCH.Managed.dll -d:DEBUG Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs

install: FreeSWITCH.Managed.dll
	$(INSTALL) FreeSWITCH.Managed.dll $(DESTDIR)$(MODINSTDIR)

uninstall:
	$(UNINSTALL) $(MODINSTDIR)/FreeSWITCH.Managed.dll

