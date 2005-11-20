@echo off
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
set APRDESTDIR=apr
set JRTPDIR=jrtplib-3.3.0
set JRTPTAR=%JRTPDIR%.tar.gz
set JRTPURL=http://research.edm.luc.ac.be/jori/jrtplib/
set JRTPDESTDIR=jrtplib
set JTHREADDIR=jthread-1.1.2
set JTHREADTAR=%JTHREADDIR%.tar.gz
set JTHREADURL=http://research.edm.luc.ac.be/jori/jthread/
set JTHREADDESTDIR=%JTHREADDIR%
set EXOSIPDIR=libeXosip2-1.9.1-pre17
set EXOSIPTAR=%EXOSIPDIR%.tar.gz
set EXOSIPURL=http://www.antisip.com/download/
set EXOSIPDESTDIR=libeXosip2
set OSIPDIR=libosip2-2.2.1
set OSIPTAR=%OSIPDIR%.tar.gz
set OSIPURL=http://www.antisip.com/download/
set OSIPDESTDIR=osip

IF NOT EXIST %LIBSRCDIR% md %LIBSRCDIR%


ECHO ****************************************************************
ECHO **************             DOWNLOADS           *****************
ECHO ****************************************************************

IF NOT EXIST %TAR% %WGET% %TARURL% %UTILSDIR%
IF NOT EXIST %TAR% %WGET% %TARURL2% %UTILSDIR%
IF NOT EXIST %GUNZIP% %WGET% %GUNZIPURL% %UTILSDIR%
IF NOT EXIST %GUNZIP% %WGET% %GUNZIPURL2% %UTILSDIR%

cd %LIBSRCDIR%
IF NOT EXIST %LIBSRCDIR%\%APRTAR% IF NOT EXIST %LIBSRCDIR%\%APRDESTDIR% %WGET% %APRURL%%APRTAR% %LIBSRCDIR% & %GUNZIP% < %LIBSRCDIR%\%APRTAR% | %TAR% xvf - & ren %APRDIR% %APRDESTDIR% & del %APRTAR%
IF NOT EXIST %LIBSRCDIR%\%EXOSIPTAR% IF NOT EXIST %LIBSRCDIR%\%EXOSIPDESTDIR% %WGET% %EXOSIPURL%%EXOSIPTAR% %LIBSRCDIR% & %GUNZIP% < %LIBSRCDIR%\%EXOSIPTAR% | %TAR% xvf - & ren %EXOSIPDIR% %EXOSIPDESTDIR% & del %EXOSIPTAR%
IF NOT EXIST %LIBSRCDIR%\%OSIPTAR% IF NOT EXIST %LIBSRCDIR%\%OSIPDESTDIR% %WGET% %OSIPURL%%OSIPTAR% %LIBSRCDIR% & %GUNZIP% < %LIBSRCDIR%\%OSIPTAR% | %TAR% xvf - & ren %OSIPDIR% %OSIPDESTDIR% & del %OSIPTAR%
IF NOT EXIST %LIBSRCDIR%\%JTHREADTAR% IF NOT EXIST %LIBSRCDIR%\%JTHREADDESTDIR% %WGET% %JTHREADURL%%JTHREADTAR% %LIBSRCDIR% & %GUNZIP% < %LIBSRCDIR%\%JTHREADTAR% | %TAR% xvf - & ren %JTHREADDIR% %JTHREADDESTDIR% & del %JTHREADTAR%
IF NOT EXIST %LIBSRCDIR%\%JRTPTAR% IF NOT EXIST %LIBSRCDIR%\%JRTPDESTDIR% %WGET% %JRTPURL%%JRTPTAR% %LIBSRCDIR% & %GUNZIP% < %LIBSRCDIR%\%JRTPTAR% | %TAR% xvf - & ren %JRTPDIR% %JRTPDESTDIR% & del %JRTPTAR%
cd 

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

IF NOT EXIST %LIBSRCDIR%\%APRDESTDIR%\libapr.vcproj %UNIX2DOS% %LIBSRCDIR%\%APRDESTDIR%\libapr.dsp
IF NOT EXIST %LIBSRCDIR%\%APRDESTDIR%\libapr.vcproj cscript %UTILSDIR%\upgrade.vbs %LIBSRCDIR%\%APRDESTDIR%\libapr.dsp %LIBSRCDIR%\%APRDESTDIR%\libapr.vcproj
REM %DEVENV% %LIBSRCDIR%\%APRDESTDIR%\libapr.vcproj /build Debug


ECHO ****************************************************************
ECHO **************            OSIP BUILD           *****************
ECHO ****************************************************************

del %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osipparser2.vcproj
copy %UTILSDIR%\osipparser2.vcproj %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\
%DEVENV% %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osip.sln /Upgrade
REM %DEVENV% %LIBSRCDIR%\%OSIPDESTDIR%\platform\vsnet\osip.sln /build Debug


ECHO ****************************************************************
ECHO **************          EXOSIP BUILD           *****************
ECHO ****************************************************************

%DEVENV% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\vsnet\eXosip.vcproj /Upgrade
REM %DEVENV% %LIBSRCDIR%\%EXOSIPDESTDIR%\platform\vsnet\eXosip.vcproj /build Debug

ECHO ****************************************************************
ECHO **************           JRTP BUILD            *****************
ECHO ****************************************************************

REM %DEVENV% %LIBSRCDIR%\jrtp4c\w32\jrtp4c.sln /build Debug

:END
cd %UTILSDIR%\..


