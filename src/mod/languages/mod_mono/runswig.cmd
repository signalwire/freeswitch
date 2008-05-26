\dev\swig\swig.exe -I..\..\..\include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_mono freeswitch.i 
move *.cs ..\mod_mono_managed\swig\ 
..\mod_mono_managed\swigStringFix ..\mod_mono_managed\swig\freeswitchPINVOKE.cs > ..\mod_mono_managed\swig\freeswitchPINVOKE_fixed.cs