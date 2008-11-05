all: mod_managed_lib.dll

clean:
	rm -fr mod_managed_lib.dll

mod_managed_lib.dll: Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs
	gmcs -target:library -out:mod_managed_lib.dll -d:DEBUG Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs

install: mod_managed_lib.dll
	$(INSTALL) mod_managed_lib.dll $(DESTDIR)$(MODINSTDIR)

uninstall:
	$(UNINSTALL) $(MODINSTDIR)/mod_managed_lib.dll

