\dev\swigwin-2.0.12\swig.exe -I..\..\..\include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i 
del swig.csx
@ECHO OFF
for %%X in (*.cs) do type %%X >> swig.csx
@ECHO ON
move swig.csx managed\swig.cs
del *.cs
\tools\dos2unix\bin\dos2unix managed\swig.cs
\tools\dos2unix\bin\dos2unix freeswitch_wrap.cxx
