@echo off
set Path=%PATH%;c:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE;c:\Program Files\Microsoft Visual Studio 9.0\VC\BIN;c:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools;c:\WINDOWS\Microsoft.NET\Framework\v3.5;c:\WINDOWS\Microsoft.NET\Framework\v2.0.50727;c:\Program Files\Microsoft Visual Studio 9.0\VC\VCPackages;C:\Program Files\Microsoft SDKs\Windows\v6.0A\bin;
msbuild Freeswitch.2008.sln /verbosity:detailed /property:Configuration=Release /logger:FileLogger,Microsoft.Build.Engine;logfile=Freeswitch.2008.sln.release.log
pause