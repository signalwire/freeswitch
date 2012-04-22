@echo off

if not "%NovellLibC%" == "" goto CheckNDK
set NovellLibC=\novell\ndk\libc
@echo Could not find the NovellLibC environment variable
@echo Setting NovellLibC = %NovellLibC%
@echo ---------------------  

:CheckNDK
if exist %NovellLibC%\include\netware.h goto NDKOK
@echo The path to the NDK "%NovellLibC%" is invalid.
@echo Please set then NovellLibC environment variable to the location of the NDK
@echo ---------------------  
goto Done

:NDKOK
@echo # As part of the pre-build process, the utility GenURI.NLM
@echo #  (Gen URI Delims) must be built, copied to a NetWare server 
@echo #  and run using the following command:
@echo #
@echo # "sys:\genuri >sys:\uri_delims.h"
@echo #
@echo #  The file "sys:\uri_delims.h" must then be copied to
@echo #  "apr-util\uri\uri_delims.h" on the build machine.

@echo Fixing up the APR headers
copy ..\include\apr.hnw ..\include\apr.h

@echo Fixing up the APR-Util headers
copy ..\..\apr-util\include\apu.hnw ..\..\apr-util\include\apu.h
copy ..\..\apr-util\include\apu_want.hnw ..\..\apr-util\include\apu_want.h
copy ..\..\apr-util\include\apr_ldap.hnw ..\..\apr-util\include\apr_ldap.h
copy ..\..\apr-util\include\private\apu_config.hw ..\..\apr-util\include\private\apu_config.h
copy ..\..\apr-util\xml\expat\lib\expat.h.in ..\..\apr-util\xml\expat\lib\expat.h
copy ..\..\apr-util\xml\expat\lib\config.hnw ..\..\apr-util\xml\expat\lib\config.h
copy ..\..\apr-util\include\private\apu_select_dbm.hw ..\..\apr-util\include\private\apu_select_dbm.h

@echo Fixing up the pcre headers
copy ..\..\pcre\config.hw ..\..\pcre\config.h
copy ..\..\pcre\pcre.hw ..\..\pcre\pcre.h

@echo Generating the import list...
set MWCIncludes=..\include;..\include\arch\netware;..\include\arch\unix;..\..\apr-util\include;+%NovellLibC%
mwccnlm -P nw_export.inc -d NETWARE -EP
awk -f make_nw_export.awk nw_export.i |sort >..\aprlib.imp

:Done
pause
