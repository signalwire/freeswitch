REM @echo off
ECHO ****************************************************************
ECHO **************          VARIABLE SETUP         *****************
ECHO ****************************************************************

for /f %%i in ('cscript /Nologo .\tools\Fulldir.vbs .\..\..\..\..') DO SET BASEDIR=%%i
set UTILSDIR=%BASEDIR%\src\freeswitch\platform\vsnet\Tools
set INCLUDEDIR=%BASEDIR%\include
set LIBSRCDIR=%BASEDIR%\src
set DEBUGLIBBINDIR=%BASEDIR%\debuglib
set WGET=cscript %UTILSDIR%\wget.vbs
set PATCHURL=http://www.jerris.com/
SET PATCHTAR=patch.tar.gz
set TAR=%UTILSDIR%\tar.exe
set TARURL=http://users.pandora.be/larc/download/windows_management/tar.exe
set GUNZIP=%UTILSDIR%\gunzip.exe
set GUNZIPURL=http://users.pandora.be/larc/download/windows_management/gunzip.exe
set UNIX2DOS=%UTILSDIR%\unix2dos.vbs
set PATCH=%UTILSDIR%\PATCH.exe
set APRDIR=apr-1.2.2
set APRTAR=%APRDIR%.tar.gz
set APRURL=ftp://ftp.wayne.edu/apache/apr/
set APRDESTDIR=apr
set CCRTPDIR=ccrtp-1.3.5
set CCRTPTAR=%CCRTPDIR%.tar.gz
set CCRTPURL=ftp://ftp.gnu.org/pub/gnu/ccrtp/
set CCRTPDESTDIR=ccrtp
set CCPPDIR=commoncpp2-1.3.21
set CCPPTAR=%CCPPDIR%.tar.gz
set CCPPURL=ftp://ftp.gnu.org/gnu/commoncpp/
set CCPPDESTDIR=commoncpp2
set EXOSIPDIR=libeXosip-0.9.0
set EXOSIPTAR=%EXOSIPDIR%.tar.gz
set EXOSIPURL=http://www.antisip.com/download/
set EXOSIPDESTDIR=eXosip
set OSIPDIR=libosip2-2.2.1
set OSIPTAR=%OSIPDIR%.tar.gz
set OSIPURL=http://www.antisip.com/download/
set OSIPDESTDIR=libosip2-2.2.0

IF NOT EXIST %INCLUDEDIR% md %INCLUDEDIR%
IF NOT EXIST %DEBUGLIBBINDIR% md %DEBUGLIBBINDIR%
IF NOT EXIST %LIBSRCDIR% md %LIBSRCDIR%
cd %LIBSRCDIR%

ECHO ****************************************************************
ECHO **************             DOWNLOADS           *****************
ECHO ****************************************************************

IF NOT EXIST %TAR% %WGET% %TARURL% %UTILSDIR%
IF NOT EXIST %GUNZIP% %WGET% %GUNZIPURL% %UTILSDIR%
cd %UTILSDIR%
IF NOT EXIST %UTILSDIR%\%PATCHTAR% IF NOT EXIST %PATCH% %WGET% %PATCHURL%%PATCHTAR% %UTILSDIR% & %GUNZIP% < %UTILSDIR%\%PATCHTAR% | %TAR% xvf - & del %PATCHTAR%
cd %LIBSRCDIR%

IF NOT EXIST %APRTAR% IF NOT EXIST %APRDESTDIR% %WGET% %APRURL%%APRTAR% & %GUNZIP% < %APRTAR% | %TAR% xvf - & ren %APRDIR% %APRDESTDIR% & del %APRTAR%
IF NOT EXIST %CCRTPTAR% IF NOT EXIST %CCRTPDESTDIR% %WGET% %CCRTPURL%%CCRTPTAR% & %GUNZIP% < %CCRTPTAR% | %TAR% xvf - & ren %CCRTPDIR% %CCRTPDESTDIR% & del %CCRTPTAR%
IF NOT EXIST %CCPPTAR% IF NOT EXIST %CCPPDESTDIR% %WGET% %CCPPURL%%CCPPTAR% & %GUNZIP% < %CCPPTAR% | %TAR% xvf - & ren %CCPPDIR% %CCPPDESTDIR% & del %CCPPTAR%
IF NOT EXIST %EXOSIPTAR% IF NOT EXIST %EXOSIPDESTDIR% %WGET% %EXOSIPURL%%EXOSIPTAR% & %GUNZIP% < %EXOSIPTAR% | %TAR% xvf - & ren %EXOSIPDIR% %EXOSIPDESTDIR% & del %EXOSIPTAR%
IF NOT EXIST %OSIPTAR% IF NOT EXIST %OSIPDESTDIR% %WGET% %OSIPURL%%OSIPTAR% & %GUNZIP% < %OSIPTAR% | %TAR% xvf - & ren %OSIPDIR% %OSIPDESTDIR% & del %OSIPTAR%


ECHO ****************************************************************
ECHO **************      VS Version Detection       *****************
ECHO ****************************************************************

IF EXIST "%VS80COMNTOOLS%..\IDE\devenv.exe" GOTO VS8
IF EXIST "%VS71COMNTOOLS%..\IDE\devenv.exe" GOTO VS7
echo no Visual Studio .net 2003 or greater found.  I don't know how to autobuild projects.  Please manually build libs.
GOTO END

:VS8
set DEVENV="%VS80COMNTOOLS%..\IDE\devenv" 
IF NOT EXIST %UTILSDIR%\upgrade.vbs copy %UTILSDIR%\upgrade8.vbs %UTILSDIR%\upgrade.vbs
call "%VS80COMNTOOLS%vsvars32.bat"
GOTO NEXT

:VS7
set DEVENV="%VS71COMNTOOLS%..\IDE\devenv"
IF NOT EXIST %UTILSDIR%\upgrade.vbs copy %UTILSDIR%\upgrade7.vbs %UTILSDIR%\upgrade.vbs
call "%VS71COMNTOOLS%vsvars32.bat"

:NEXT
ECHO ****************************************************************
ECHO **************             APR BUILD           *****************
ECHO ****************************************************************

cd %APRDESTDIR%
IF NOT EXIST %INCLUDEDIR%\apr.h copy %LIBSRCDIR%\%APRDESTDIR%\include\*.h %INCLUDEDIR%
IF NOT EXIST %INCLUDEDIR%\apr.h copy %LIBSRCDIR%\%APRDESTDIR%\include\apr.hw %INCLUDEDIR%\apr.h
IF NOT EXIST libapr.vcproj %UNIX2DOS% libapr.dsp
IF NOT EXIST libapr.vcproj cscript %UTILSDIR%\upgrade.vbs libapr.dsp libapr.vcproj
%DEVENV% libapr.vcproj /build Debug
REM %DEVENV% libapr.vcproj /build Release
copy %LIBSRCDIR%\%APRDESTDIR%\debug\*.lib %DEBUGLIBBINDIR%
copy %LIBSRCDIR%\%APRDESTDIR%\debug\*.dll %DEBUGLIBBINDIR%


ECHO ****************************************************************
ECHO **************           CCRTP BUILD           *****************
ECHO ****************************************************************

cd %LIBSRCDIR%\%CCPPDESTDIR%
cd src
IF NOT EXIST patched.tag copy %UTILSDIR%\commoncpp2005.diff
IF EXIST commoncpp2005.diff %PATCH% -u -i commoncpp2005.diff
IF EXIST commoncpp2005.diff ren commoncpp2005.diff patched.tag
IF NOT EXIST "%INCLUDEDIR%\cc++" copy "%LIBSRCDIR%\%CCPPDESTDIR%\src\*.h" "%INCLUDEDIR%"
IF NOT EXIST "%INCLUDEDIR%\ccrtp" copy "%LIBSRCDIR%\ccrtp4c\src\*.h" "%INCLUDEDIR%"
IF NOT EXIST "%INCLUDEDIR%\cc++" md "%INCLUDEDIR%\cc++"
IF NOT EXIST "%INCLUDEDIR%\ccrtp" md "%INCLUDEDIR%\ccrtp"
IF NOT EXIST "%INCLUDEDIR%\ccrtp\base.h" copy "%LIBSRCDIR%\%CCRTPDESTDIR%\src\ccrtp\*.h" "%INCLUDEDIR%\ccrtp"
IF NOT EXIST "%INCLUDEDIR%\cc++\pointer.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\src\template\*.h" "%INCLUDEDIR%\cc++"
IF NOT EXIST "%INCLUDEDIR%\cc++\unix.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\src\include\cc++\*.h" "%INCLUDEDIR%\cc++"
IF NOT EXIST "%INCLUDEDIR%\cc++\config.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\src\w32\cc++\*.h" "%INCLUDEDIR%\cc++"

IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++" md "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include" & md "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++"
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++\unix.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\w32\cc++\*.h" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++"
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++\unix.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\w32\cc++\*.h" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include"
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++\unix.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\include\cc++\*.h" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\include\cc++\"

IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\template\cc++" md "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\template" & md "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\template\cc++"
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\template\cc++\pointer.h" copy "%LIBSRCDIR%\%CCPPDESTDIR%\template\*.h" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\template\cc++\"

IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\src" md "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\src"
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\src\unix.cpp" copy "%LIBSRCDIR%\%CCPPDESTDIR%\src\*.*" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\src\"
del "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\*.dsp"
copy "%LIBSRCDIR%\%CCPPDESTDIR%\w32\*.dsp" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\"
copy "%UTILSDIR%\ccrtp1.sln" "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\"

IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccgnu2.vcproj" cscript %UTILSDIR%\upgrade.vbs %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccgnu2.dsp %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccgnu2.vcproj
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccext2.vcproj" cscript %UTILSDIR%\upgrade.vbs %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccext2.dsp %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\ccext2.vcproj
IF NOT EXIST "%LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\ccrtp1.vcproj" cscript %UTILSDIR%\upgrade.vbs %LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\ccrtp1.dsp %LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\ccrtp1.vcproj

cd %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common
%DEVENV% ccgnu2.vcproj /build Debug /project ccgnu2
REM %DEVENV% ccgnu2.vcproj /build Release /project ccgnu2
%DEVENV% ccext2.vcproj /build Debug /project ccext2
REM %DEVENV% ccext2.vcproj /build Release /project ccext2
cd %LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp
%DEVENV% ccrtp1.vcproj /build Debug /project ccrtp1
REM %DEVENV% ccrtp1.vcproj /build Release /project ccrtp1
cd %LIBSRCDIR%

copy %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\debug\*.lib %DEBUGLIBBINDIR%
copy %LIBSRCDIR%\%CCRTPDESTDIR%\w32\common\debug\*.dll %DEBUGLIBBINDIR%
copy %LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\debug\*.lib %DEBUGLIBBINDIR%
copy %LIBSRCDIR%\%CCRTPDESTDIR%\w32\msvcpp\debug\*.dll %DEBUGLIBBINDIR%


ECHO ****************************************************************
ECHO **************            OSIP BUILD           *****************
ECHO ****************************************************************

IF NOT EXIST %INCLUDEDIR%\osip2 md %INCLUDEDIR%\osip2
IF NOT EXIST %INCLUDEDIR%\osipparser2 md %INCLUDEDIR%\osipparser2
IF NOT EXIST %INCLUDEDIR%\osip2\osip.h copy %LIBSRCDIR%\%OSIPDESTDIR%\include\osip2\*.h %INCLUDEDIR%\osip2
IF NOT EXIST %INCLUDEDIR%\osipparser2\osip_parser.h copy %LIBSRCDIR%\%OSIPDESTDIR%\include\osipparser2\*.h %INCLUDEDIR%\osipparser2
%DEVENV% %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osip.sln /Upgrade
%DEVENV% %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osip.sln /build Debug
REM %DEVENV% %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osip.sln /build Release
copy %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\debug\*.lib %DEBUGLIBBINDIR%


ECHO ****************************************************************
ECHO **************          EXOSIP BUILD           *****************
ECHO ****************************************************************

IF NOT EXIST "%INCLUDEDIR%\eXosip" md "%INCLUDEDIR%\eXosip"
IF NOT EXIST %INCLUDEDIR%\eXosip copy "%LIBSRCDIR%\%EXOSIPDESTDIR%\include\eXosip\*.h" "%INCLUDEDIR%\eXosip"
%UNIX2DOS% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\windows\eXosip.vcproj
%DEVENV% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\windows\eXosip.vcproj /Upgrade
%DEVENV% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\windows\eXosip.vcproj /build Debug
REM %DEVENV% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\windows\eXosip.vcproj /build Release
copy %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\windows\debug\*.lib %DEBUGLIBBINDIR%


:END
cd %BASEDIR%


