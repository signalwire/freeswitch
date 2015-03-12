@ECHO OFF

REM First argument is the target architecture
REM Second argument is "Debug" or "Release" mode
REM Third argument is the V8 root directory path
REM Fourth argument is the version of Visual Studio (optional)

IF "%1" == "" GOTO Fail
IF "%2" == "" GOTO Fail
IF "%3" == "" GOTO Fail

REM Go into the V8 lib directory
cd "%3"

REM Check the last build info, so we know if we're supposed to build again or not
SET /P LAST_BUILD_INFO=<last_build
IF "%1-%2" == "%LAST_BUILD_INFO%" (
	IF EXIST ".\build\%2\v8.dll" (
		ECHO V8 is already built!
		SET COPY_FILES_ONLY=1
	)
)

SET LIB_DEST_DIR=

IF "%VisualStudioVersion%" == "11.0" (
	SET VS_VERSION=-Gmsvs_version=2012
	ECHO Forcing build to use Visual Studio 2012
) ELSE IF "%VisualStudioVersion%" == "12.0" (
	SET VS_VERSION=-Gmsvs_version=2013
	ECHO Forcing build to use Visual Studio 2013
) ELSE IF NOT "%4" == "" (
	SET VS_VERSION=-Gmsvs_version=%4
	ECHO Forcing build to use Visual Studio %4
)

IF "%1" == "x64" (
	REM If this is a 32-bit system (but we target x64), we must disable the snapshot feature to get it to build.
	IF NOT EXIST "%PROGRAMFILES(X86)%" (
		SET SKIP_V8_SNAPSHOT=-Dv8_use_snapshot=false
	)
)

IF "%1" == "x64" (
	IF NOT "%SKIP_V8_SNAPSHOT%" == "" ECHO Targeting x64 platform on a x86 system, disabling V8 snapshout feature to make this work [%SKIP_V8_SNAPSHOT%]
	IF NOT "%COPY_FILES_ONLY%" == "1" .\third_party\python_26\python.exe build\gyp_v8 -Dtarget_arch=x64 -Dcomponent=shared_library %SKIP_V8_SNAPSHOT% %VS_VERSION%
	IF ERRORLEVEL 1 GOTO Fail
	SET LIB_DEST_DIR=..\..\x64\%2\
)

IF "%1" == "x86" (
	IF NOT "%COPY_FILES_ONLY%" == "1" .\third_party\python_26\python.exe build\gyp_v8 -Dcomponent=shared_library %VS_VERSION%
	IF ERRORLEVEL 1 GOTO Fail
	SET LIB_DEST_DIR=..\..\Win32\%2\
)

IF "%LIB_DEST_DIR%" == "" GOTO Fail

IF "%COPY_FILES_ONLY%" == "1" GOTO CopyFiles

REM Clean build before we continue
REM First try to clean using the solution path (works for most VS versions)
msbuild "tools\gyp\v8.sln" /t:"_tools_\_gyp_\v8:Clean" /p:Configuration=%2 /clp:WarningsOnly
IF NOT ERRORLEVEL 1 GOTO CleanDone
REM If clean using solution path didn't work, try to build without the path (works for some VS versions...)
msbuild "tools\gyp\v8.sln" /t:v8:Clean /p:Configuration=%2
IF ERRORLEVEL 1 GOTO Fail
:CleanDone

REM Just to make sure that everything is cleaned up
rmdir /S /Q .\build\%2

REM Build the V8 library
REM First try to build using the solution path (works for most VS versions)
msbuild "tools\gyp\v8.sln" /t:"_tools_\_gyp_\v8:Rebuild" /p:Configuration=%2 /clp:WarningsOnly
IF NOT ERRORLEVEL 1 GOTO CopyFiles
REM If build using solution path didn't work, try to build without the path (works for some VS versions...)
msbuild "tools\gyp\v8.sln" /t:v8:Rebuild /p:Configuration=%2
IF ERRORLEVEL 1 GOTO Fail

:CopyFiles

xcopy /C /F /R /Y .\build\%2\icui18n.dll %LIB_DEST_DIR%
IF ERRORLEVEL 1 GOTO Fail

xcopy /C /F /R /Y .\build\%2\icuuc.dll %LIB_DEST_DIR%
IF ERRORLEVEL 1 GOTO Fail

xcopy /C /F /R /Y .\build\%2\v8.dll %LIB_DEST_DIR%
IF ERRORLEVEL 1 GOTO Fail

ECHO %1-%2> last_build

exit /b 0

:Fail
REM Delete the last_build info if this build failed!
@del /Q last_build
exit /b 1
