#!/bin/bash
swig -I../../../include -v -O -c++ -csharp -namespace FreeSWITCH.Native -dllimport mod_managed freeswitch.i
rm -f ./managed/swig.cs
cat *.cs > ./managed/swig.cs
rm -f *.cs
