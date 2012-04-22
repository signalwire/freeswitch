echo off
rem example call: expsound {tmp} {app} freeswitch-sounds-en-us-callie-8000-1.0.11.tar.gz

IF EXIST %1\%3 %1\7za x -r -y -o%1 %1\%3
IF EXIST %1\%~n3 %1\7za x -r -y -o%2\sounds %1\%~n3
