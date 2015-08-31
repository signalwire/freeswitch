@REM this script builds freeswitch using VS2015
@REM only one platform/configuration will be built
@REM runs (probably only) from the commandline
@REM usage: Freeswitch.2015.sln  [[[.*]ebug] [[.*]elease] [[.*]64] [[.*]32]]
@REM e.g. Freeswitch.2015.sln Debug x64
@REM      Freeswitch.2015.sln x64
@REM 	  Freeswitch.2015.sln Debug
@REM 	  Freeswitch.2015.sln

@setlocal
@echo on

@REM default build
@REM change these variables if you want to build differently by default
@set configuration=Release
@set platform=Win32


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

@REM check and set VS2015 environment
@REM vcvars32.bat calls exit and will also exit whilie running this bat file ...
@REM so you have to run it again if the VS2015 env is not yet set
@if "%VS110COMNTOOLS%"=="" (
	goto :error_no_VS110COMNTOOLSDIR
)

@if "%VSINSTALLDIR%"=="" (
	goto :setvcvars
)

:build

msbuild Freeswitch.2015.sln /m:%procs% /verbosity:normal /property:Configuration=%configuration% /property:Platform=%platform% /fl /flp:logfile=vs2015%platform%%configuration%.log;verbosity=normal
@goto :end

@REM -----------------------------------------------------------------------
:setvcvars
	@endlocal
	@echo Now setting Visual Studio 2015 Environment
	@call "%VS110COMNTOOLS%vsvars32"
	@REM in order to prevent running vsvars32 multiple times and at the same time not
	@REM cluttering up the environment variables proc/configuration/platform
	@REM it is necessary to start the script again
	@echo Run the script %0 again (and/or open a command prompt)
	@goto :end

:error_no_VS110COMNTOOLSDIR
	@echo ERROR: Cannot determine the location of the VS2015 Common Tools folder.
	@echo ERROR: Note this script will not work in a git bash environment
	@goto :end

:end
@pause

@REM ------ terminate :end with LF otherwise the label is not recognized by the command processor -----



