all: Debug Debug/mod_managed_lib.dll

clean:
       rm -fr Debug

Debug:
       mkdir Debug

Debug/mod_managed_lib.dll: Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs
       gmcs -target:library -out:Debug/mod_managed_lib.dll -d:DEBUG Loader.cs ManagedSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs

install: Debug/mod_managed_lib.dll
       install Debug/mod_managed_lib.dll /usr/local/freeswitch/mod
