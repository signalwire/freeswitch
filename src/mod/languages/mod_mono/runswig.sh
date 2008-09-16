#!/bin/bash
swig -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_mono freeswitch.i
rm -f ../mod_mono_managed/swig.cs
cat *.cs > ../mod_mono_managed/swig.cs
rm -f *.cs
