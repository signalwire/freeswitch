@echo off
REM Usage: wslrun.cmd <bash command>
REM Runs the given command in the Windows Subsystem for Linux bash shell.

REM Locate bash.exe
REM 32/64 bits causes issues here because it actually redirects the System32 dir
set BASH=bash.exe
if exist C:\Windows\System32\bash.exe set BASH=C:\Windows\System32\bash.exe
if exist C:\Windows\Sysnative\bash.exe set BASH=C:\Windows\Sysnative\bash.exe

%BASH% -c "%*"
