REM @echo off
ECHO ****************************************************************
ECHO **************          VARIABLE SETUP         *****************
ECHO ****************************************************************

for /f %%i in ('cscript /Nologo .\tools\Fulldir.vbs .\..\..') DO SET BASEDIR=%%i
set UTILSDIR=%BASEDIR%\w32\vsnet\Tools
set INCLUDEDIR=%BASEDIR%\include
set LIBSRCDIR=%BASEDIR%\libs
set DEBUGLIBBINDIR=%BASEDIR%\debuglib
set WGET=cscript %UTILSDIR%\wget.vbs
set TAR=%UTILSDIR%\tar.exe
set TARURL=http://users.pandora.be/larc/download/windows_management/tar.exe
set TARURL2=http://www.sofaswitch.org/mikej/tar.exe
set GUNZIP=%UTILSDIR%\gunzip.exe
set GUNZIPURL=http://users.pandora.be/larc/download/windows_management/gunzip.exe
set GUNZIPURL2=http://www.sofaswitch.org/mikej/gunzip.exe
set UNIX2DOS=%UTILSDIR%\unix2dos.vbs
set APRDIR=apr-1.2.2
set APRTAR=%APRDIR%.tar.gz
set APRURL=ftp://ftp.wayne.edu/apache/apr/
set APRDESTDIR=%APRDIR%
set JRTPDIR=jrtplib-3.3.0
set JRTPTAR=%JRTPDIR%.tar.gz
set JRTPURL=http://research.edm.luc.ac.be/jori/jrtplib/
set JRTPDESTDIR=%JRTPDIR%
set JTHREADDIR=jthread-1.1.2
set JTHREADTAR=%JTHREADDIR%.tar.gz
set JTHREADURL=http://research.edm.luc.ac.be/jori/jthread/
set JTHREADDESTDIR=%JTHREADDIR%
set EXOSIPDIR=libeXosip-0.9.0
set EXOSIPTAR=%EXOSIPDIR%.tar.gz
set EXOSIPURL=http://www.antisip.com/download/
set EXOSIPDESTDIR=%EXOSIPDIR%
set OSIPDIR=libosip2-2.2.1
set OSIPTAR=%OSIPDIR%.tar.gz
set OSIPURL=http://www.antisip.com/download/
set OSIPDESTDIR=libosip2-2.2.0

IF NOT EXIST %INCLUDEDIR% md %INCLUDEDIR%
IF NOT EXIST %DEBUGLIBBINDIR% md %DEBUGLIBBINDIR%
IF NOT EXIST %LIBSRCDIR% md %LIBSRCDIR%


ECHO ****************************************************************
ECHO **************             DOWNLOADS           *****************
ECHO ****************************************************************

IF NOT EXIST %TAR% %WGET% %TARURL% %UTILSDIR%
IF NOT EXIST %TAR% %WGET% %TARURL2% %UTILSDIR%
IF NOT EXIST %GUNZIP% %WGET% %GUNZIPURL% %UTILSDIR%
IF NOT EXIST %GUNZIP% %WGET% %GUNZIPURL2% %UTILSDIR%

cd %LIBSRCDIR%
IF NOT EXIST %APRTAR% IF NOT EXIST %APRDESTDIR% %WGET% %APRURL%%APRTAR% & %GUNZIP% < %APRTAR% | %TAR% xvf - & ren %APRDIR% %APRDESTDIR% & del %APRTAR%
IF NOT EXIST %EXOSIPTAR% IF NOT EXIST %EXOSIPDESTDIR% %WGET% %EXOSIPURL%%EXOSIPTAR% & %GUNZIP% < %EXOSIPTAR% | %TAR% xvf - & ren %EXOSIPDIR% %EXOSIPDESTDIR% & del %EXOSIPTAR%
IF NOT EXIST %OSIPTAR% IF NOT EXIST %OSIPDESTDIR% %WGET% %OSIPURL%%OSIPTAR% & %GUNZIP% < %OSIPTAR% | %TAR% xvf - & ren %OSIPDIR% %OSIPDESTDIR% & del %OSIPTAR%
IF NOT EXIST %JTHREADTAR% IF NOT EXIST %JTHREADDESTDIR% %WGET% %JTHREADURL%%JTHREADTAR% & %GUNZIP% < %JTHREADTAR% | %TAR% xvf - & ren %JTHREADDIR% %JTHREADDESTDIR% & del %JTHREADTAR%
IF NOT EXIST %JRTPTAR% IF NOT EXIST %JRTPDESTDIR% %WGET% %JRTPURL%%JRTPTAR% & %GUNZIP% < %JRTPTAR% | %TAR% xvf - & ren %JRTPDIR% %JRTPDESTDIR% & del %JRTPTAR%


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
cd %LIBSRCDIR%

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

ECHO ****************************************************************
ECHO **************           JRTP BUILD            *****************
ECHO ****************************************************************

%DEVENV% %LIBSRCDIR%\jrtp4c\jrtp4c.sln /build Debug

:END
cd %UTILSDIR%\..


