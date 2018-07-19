@REM this script builds freeswitch using VS2017
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

@REM check and set VS2017 environment
rem VS2017U2 contains vswhere.exe
if "%VSWHERE%"=="" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

rem Use %ProgramFiles% in a 32-bit program prior to Windows 10)
If Not Exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

If Not Exist "%VSWHERE%" (
    echo "WARNING: Can't find vswhere.exe. It is a part of VS 2017 version 15.2 or later. Trying known path..."
    set "InstallDir=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community"
) ELSE (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
       set InstallDir=%%i
    )
)

echo Install dir is "%InstallDir%"
if exist "%InstallDir%\MSBuild\15.0\Bin\MSBuild.exe" (
    set msbuild="%InstallDir%\MSBuild\15.0\Bin\MSBuild.exe"
)

if exist %msbuild% (
%msbuild% Freeswitch.2017.sln /m:%procs% /verbosity:normal /property:Configuration=%configuration% /property:Platform=%platform% /fl /flp:logfile=vs2017%platform%%configuration%.log;verbosity=normal
) ELSE (
    echo "echo ERROR: Cannot find msbuild. You need Visual Studio 2017 to compile this solution."
)
@pause

@REM ------ terminate :end with LF otherwise the label is not recognized by the command processor -----



