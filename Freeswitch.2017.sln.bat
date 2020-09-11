@REM this script builds freeswitch using the latest found Microsoft Visual Studio
@REM only one platform/configuration will be built
@REM runs (probably only) from the commandline
@REM usage: Freeswitch.2017.sln  [[[.*]ebug] [[.*]elease] [[.*]64] [[.*]32]]
@REM e.g. Freeswitch.2017.sln Debug x64
@REM      Freeswitch.2017.sln x64
@REM 	  Freeswitch.2017.sln Debug
@REM 	  Freeswitch.2017.sln

@setlocal
@echo on

@REM default build
@REM change these variables if you want to build differently by default
@set configuration=Release
@set platform=x64


@REM if commandline parameters contain "ebug" and/or "64 and/or 32"
@REM set the configuration/platform to Debug and/or x64 and/or 32
@if "%1"=="" (
	@goto :paramsset
)

@set params=%*
@set xparams=x%params: =%
@if not y%xparams:ebug=%==y%xparams% (
	set configuration=Debug
)

@if not x%xparams:64=%==x%xparams% (
	set platform=x64
)

@if not x%xparams:32=%==x%xparams% (
	set platform=Win32
)

@if not y%xparams:elease=%==y%xparams% (
	set configuration=Debug
)

:paramsset

@REM use all processors minus 1 when building
@REM hmm, this doesn't seem to work as I expected as all my procs are used during the build
@set procs=%NUMBER_OF_PROCESSORS%
@set /a procs -= 1

@REM check and set Visual Studio environment
CALL msbuild.cmd

if exist %msbuild% (
%msbuild% Freeswitch.2017.sln /m:%procs% /verbosity:normal /property:Configuration=%configuration% /property:Platform=%platform% /fl /flp:logfile=vs%platform%%configuration%.log;verbosity=normal
) ELSE (
    echo "echo ERROR: Cannot find msbuild. You need Microsoft Visual Studio to compile this solution."
)
@pause

@REM ------ terminate :end with LF otherwise the label is not recognized by the command processor -----



