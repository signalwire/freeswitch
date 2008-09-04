\dev\swig\swig.exe -I..\..\..\include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_mono freeswitch.i 
del swig.csx
for %%X in (*.cs) do type %%X >> swig.csx
move swig.csx ..\mod_mono_managed\swig.cs
del *.cs
