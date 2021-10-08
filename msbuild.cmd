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
