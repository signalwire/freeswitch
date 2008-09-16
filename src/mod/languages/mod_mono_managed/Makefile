all: Debug Debug/mod_mono_managed.dll

clean:
       rm -fr Debug

Debug:
       mkdir Debug

Debug/mod_mono_managed.dll: Loader.cs MonoSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs
       gmcs -target:library -out:Debug/mod_mono_managed.dll -d:DEBUG Loader.cs MonoSession.cs ApiFunction.cs AppFunction.cs Extensions.cs Log.cs Demo.cs swig.cs

install: Debug/mod_mono_managed.dll
       install Debug/mod_mono_managed.dll /usr/local/freeswitch/mod
