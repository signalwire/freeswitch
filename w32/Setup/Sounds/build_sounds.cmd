@ECHO OFF
SETLOCAL

CALL ..\..\..\msbuild.cmd
if exist %msbuild% (

for /F "tokens=*" %%A in (%cd%\..\..\..\build\sounds_upgradecode.txt) do (
  for /F "tokens=1 delims= " %%a in ("%%A") do (
      CALL :Build %%a
  )
)

) ELSE (
    echo "echo ERROR: Cannot find msbuild. You need Microsoft Visual Studio to compile this solution."
    EXIT /B 1
)


REM CALL :Build music
REM CALL :Build en-us-callie

:: force execution to quit at the end of the "main" logic
EXIT /B %ERRORLEVEL%

:Build
set SoundPrimaryName=%1
set SoundQuality=8000
cmd /C %msbuild% %solution% /p:SoundPrimaryName=%SoundPrimaryName% /p:SoundQuality=%SoundQuality% /p:Configuration=Release /p:Platform=x64 /t:Build /verbosity:normal /fl /flp:logfile=..\..\..\x64\sound_logs\sounds_%SoundPrimaryName%_%SoundQuality%.log;verbosity=normal
set SoundQuality=16000
cmd /C %msbuild% %solution% /p:SoundPrimaryName=%SoundPrimaryName% /p:SoundQuality=%SoundQuality% /p:Configuration=Release /p:Platform=x64 /t:Build /verbosity:normal /fl /flp:logfile=..\..\..\x64\sound_logs\sounds_%SoundPrimaryName%_%SoundQuality%.log;verbosity=normal
set SoundQuality=32000
cmd /C %msbuild% %solution% /p:SoundPrimaryName=%SoundPrimaryName% /p:SoundQuality=%SoundQuality% /p:Configuration=Release /p:Platform=x64 /t:Build /verbosity:normal /fl /flp:logfile=..\..\..\x64\sound_logs\sounds_%SoundPrimaryName%_%SoundQuality%.log;verbosity=normal
set SoundQuality=48000
cmd /C %msbuild% %solution% /p:SoundPrimaryName=%SoundPrimaryName% /p:SoundQuality=%SoundQuality% /p:Configuration=Release /p:Platform=x64 /t:Build /verbosity:normal /fl /flp:logfile=..\..\..\x64\sound_logs\sounds_%SoundPrimaryName%_%SoundQuality%.log;verbosity=normal
EXIT /B 0
