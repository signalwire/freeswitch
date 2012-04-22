move freeswitch_wrap.cxx freeswitch_wrap.bak
\dev\swig20\swig.exe -I..\..\..\include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed -DSWIG_CSHARP_NO_STRING_HELPER freeswitch.i 
del swig.csx
move freeswitch_wrap.cxx freeswitch_wrap.2010.cxx
move freeswitch_wrap.bak freeswitch_wrap.cxx
@ECHO OFF
for %%X in (*.cs) do type %%X >> swig.csx
@ECHO ON
move swig.csx managed\swig.2010.cs
del *.cs
