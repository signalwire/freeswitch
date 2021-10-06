@REM check and set Visual Studio environment
rem There is vswhere.exe starting VS2017U2
if "%VSWHERE%"=="" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

rem Use %ProgramFiles% in a 32-bit program prior to Windows 10)
If Not Exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

If Not Exist "%VSWHERE%" (
    echo "WARNING: Can't find vswhere.exe. It is a part of VS 2017 version 15.2 or later. Trying known path..."
    set "InstallDir=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
) ELSE (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
       set InstallDir=%%i
    )
)

echo Install dir is "%InstallDir%"
if exist "%InstallDir%\MSBuild\Current\Bin\MSBuild.exe" (
    set msbuild="%InstallDir%\MSBuild\Current\Bin\MSBuild.exe"
)
