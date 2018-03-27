@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM Usage: wslpath.cmd -w <linux path>
REM Converts a path from the Linux /mnt/c/... format into Windows format.

REM Usage: wslpath.cmd -u <windows path>
REM Converts a path from Windows to Linux format.

REM Both usages require `wslrun.cmd` in the same directory as this file.

if exist %0\..\wslrun.cmd set WSLRUN="%0\..\wslrun.cmd"
if exist %CD%\%0\..\wslrun.cmd set WSLRUN="%CD%\%0\..\wslrun.cmd"

if "%1" == "-w" goto :towindows
if "%1" == "-u" shift /1

REM Convert path to Linux
if exist "%1\*" (pushd %1) else (pushd %~dp1)
if ERRORLEVEL 1 goto :eof
%WSLRUN% pwd
popd
goto :eof

:towindows
REM Convert path to Windows
%WSLRUN% cd "'%2'" ^&^& cmd.exe /c cd
