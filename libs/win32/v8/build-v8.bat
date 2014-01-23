@ECHO OFF

IF "%1" == "" GOTO Fail
IF "%2" == "" GOTO Fail
IF "%3" == "" GOTO Fail

cd "%3"

SET /P LAST_BUILD_INFO=<last_build

IF "%1-%2" == "%LAST_BUILD_INFO%" (
	ECHO V8 is already built!
	SET COPY_FILES_ONLY=1
)

SET LIB_DEST_DIR=

IF "%1" == "x64" (
	IF NOT "%COPY_FILES_ONLY%" == "1" .\third_party\python_26\python.exe build\gyp_v8 -Dtarget_arch=x64 -Dcomponent=shared_library -Dv8_use_snapshot=false
	IF NOT ERRORLEVEL 0 GOTO Fail
	SET LIB_DEST_DIR=..\..\x64\%2\
)

IF "%1" == "x86" (
	IF NOT "%COPY_FILES_ONLY%" == "1" .\third_party\python_26\python.exe build\gyp_v8 -Dcomponent=shared_library
	IF NOT ERRORLEVEL 0 GOTO Fail
	SET LIB_DEST_DIR=..\..\Win32\%2\
)

IF "%LIB_DEST_DIR%" == "" GOTO Fail

IF "%COPY_FILES_ONLY%" == "1" GOTO CopyFiles

devenv.com /clean %2 tools\gyp\v8.sln
IF NOT ERRORLEVEL 0 GOTO Fail

devenv.com /build %2 tools\gyp\v8.sln
IF NOT ERRORLEVEL 0 GOTO Fail

:CopyFiles

REM xcopy /C /F /R /Y .\build\%2\icudt.dll %LIB_DEST_DIR%
REM IF NOT ERRORLEVEL 0 GOTO Fail

xcopy /C /F /R /Y .\build\%2\icui18n.dll %LIB_DEST_DIR%
IF NOT ERRORLEVEL 0 GOTO Fail

xcopy /C /F /R /Y .\build\%2\icuuc.dll %LIB_DEST_DIR%
IF NOT ERRORLEVEL 0 GOTO Fail

xcopy /C /F /R /Y .\build\%2\v8.dll %LIB_DEST_DIR%
IF NOT ERRORLEVEL 0 GOTO Fail

ECHO %1-%2> last_build

exit

:Fail
exit /b 1
