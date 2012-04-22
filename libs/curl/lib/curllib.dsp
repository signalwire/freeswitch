# Microsoft Developer Studio Project File - Name="curllib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=curllib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "curllib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "curllib.mak" CFG="curllib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "curllib - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "curllib - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "curllib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BUILDING_LIBCURL" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O2 /I "." /I "..\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BUILDING_LIBCURL" /D "_WINDLL" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib ws2_32.lib winmm.lib /nologo /dll /map /debug /machine:I386 /out:"Release/libcurl.dll"

!ELSEIF  "$(CFG)" == "curllib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BUILDING_LIBCURL" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "." /I "..\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BUILDING_LIBCURL" /FR /FD /GZ /c
# SUBTRACT CPP /WX /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib ws2_32.lib winmm.lib /nologo /dll /incremental:no /map /debug /machine:I386 /out:"Debug/libcurl.dll" /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "curllib - Win32 Release"
# Name "curllib - Win32 Debug"
# Begin Group "Source Files"
# Begin Source File

SOURCE=.\file.c
# End Source File
# Begin Source File

SOURCE=.\timeval.c
# End Source File
# Begin Source File

SOURCE=.\base64.c
# End Source File
# Begin Source File

SOURCE=.\hostip.c
# End Source File
# Begin Source File

SOURCE=.\progress.c
# End Source File
# Begin Source File

SOURCE=.\formdata.c
# End Source File
# Begin Source File

SOURCE=.\cookie.c
# End Source File
# Begin Source File

SOURCE=.\http.c
# End Source File
# Begin Source File

SOURCE=.\sendf.c
# End Source File
# Begin Source File

SOURCE=.\ftp.c
# End Source File
# Begin Source File

SOURCE=.\url.c
# End Source File
# Begin Source File

SOURCE=.\dict.c
# End Source File
# Begin Source File

SOURCE=.\if2ip.c
# End Source File
# Begin Source File

SOURCE=.\speedcheck.c
# End Source File
# Begin Source File

SOURCE=.\ldap.c
# End Source File
# Begin Source File

SOURCE=.\ssluse.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# Begin Source File

SOURCE=.\getenv.c
# End Source File
# Begin Source File

SOURCE=.\escape.c
# End Source File
# Begin Source File

SOURCE=.\mprintf.c
# End Source File
# Begin Source File

SOURCE=.\telnet.c
# End Source File
# Begin Source File

SOURCE=.\netrc.c
# End Source File
# Begin Source File

SOURCE=.\getinfo.c
# End Source File
# Begin Source File

SOURCE=.\transfer.c
# End Source File
# Begin Source File

SOURCE=.\strequal.c
# End Source File
# Begin Source File

SOURCE=.\easy.c
# End Source File
# Begin Source File

SOURCE=.\security.c
# End Source File
# Begin Source File

SOURCE=.\krb4.c
# End Source File
# Begin Source File

SOURCE=.\memdebug.c
# End Source File
# Begin Source File

SOURCE=.\http_chunks.c
# End Source File
# Begin Source File

SOURCE=.\strtok.c
# End Source File
# Begin Source File

SOURCE=.\connect.c
# End Source File
# Begin Source File

SOURCE=.\llist.c
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\multi.c
# End Source File
# Begin Source File

SOURCE=.\content_encoding.c
# End Source File
# Begin Source File

SOURCE=.\share.c
# End Source File
# Begin Source File

SOURCE=.\http_digest.c
# End Source File
# Begin Source File

SOURCE=.\md5.c
# End Source File
# Begin Source File

SOURCE=.\http_negotiate.c
# End Source File
# Begin Source File

SOURCE=.\http_ntlm.c
# End Source File
# Begin Source File

SOURCE=.\inet_pton.c
# End Source File
# Begin Source File

SOURCE=.\strtoofft.c
# End Source File
# Begin Source File

SOURCE=.\strerror.c
# End Source File
# Begin Source File

SOURCE=.\hostares.c
# End Source File
# Begin Source File

SOURCE=.\hostasyn.c
# End Source File
# Begin Source File

SOURCE=.\hostip4.c
# End Source File
# Begin Source File

SOURCE=.\hostip6.c
# End Source File
# Begin Source File

SOURCE=.\hostsyn.c
# End Source File
# Begin Source File

SOURCE=.\hostthre.c
# End Source File
# Begin Source File

SOURCE=.\inet_ntop.c
# End Source File
# Begin Source File

SOURCE=.\parsedate.c
# End Source File
# Begin Source File

SOURCE=.\select.c
# End Source File
# Begin Source File

SOURCE=.\gtls.c
# End Source File
# Begin Source File

SOURCE=.\sslgen.c
# End Source File
# Begin Source File

SOURCE=.\tftp.c
# End Source File
# Begin Source File

SOURCE=.\splay.c
# End Source File
# Begin Source File

SOURCE=.\strdup.c
# End Source File
# Begin Source File

SOURCE=.\socks.c
# End Source File
# End Group
# Begin Group "Header Files"
# Begin Source File

SOURCE=.\arpa_telnet.h
# End Source File
# Begin Source File

SOURCE=.\netrc.h
# End Source File
# Begin Source File

SOURCE=.\file.h
# End Source File
# Begin Source File

SOURCE=.\timeval.h
# End Source File
# Begin Source File

SOURCE=.\base64.h
# End Source File
# Begin Source File

SOURCE=.\hostip.h
# End Source File
# Begin Source File

SOURCE=.\progress.h
# End Source File
# Begin Source File

SOURCE=.\formdata.h
# End Source File
# Begin Source File

SOURCE=.\cookie.h
# End Source File
# Begin Source File

SOURCE=.\http.h
# End Source File
# Begin Source File

SOURCE=.\sendf.h
# End Source File
# Begin Source File

SOURCE=.\ftp.h
# End Source File
# Begin Source File

SOURCE=.\url.h
# End Source File
# Begin Source File

SOURCE=.\dict.h
# End Source File
# Begin Source File

SOURCE=.\if2ip.h
# End Source File
# Begin Source File

SOURCE=.\speedcheck.h
# End Source File
# Begin Source File

SOURCE=.\urldata.h
# End Source File
# Begin Source File

SOURCE=.\ldap.h
# End Source File
# Begin Source File

SOURCE=.\ssluse.h
# End Source File
# Begin Source File

SOURCE=.\escape.h
# End Source File
# Begin Source File

SOURCE=.\telnet.h
# End Source File
# Begin Source File

SOURCE=.\getinfo.h
# End Source File
# Begin Source File

SOURCE=.\strequal.h
# End Source File
# Begin Source File

SOURCE=.\krb4.h
# End Source File
# Begin Source File

SOURCE=.\memdebug.h
# End Source File
# Begin Source File

SOURCE=.\inet_ntoa_r.h
# End Source File
# Begin Source File

SOURCE=.\http_chunks.h
# End Source File
# Begin Source File

SOURCE=.\strtok.h
# End Source File
# Begin Source File

SOURCE=.\connect.h
# End Source File
# Begin Source File

SOURCE=.\llist.h
# End Source File
# Begin Source File

SOURCE=.\hash.h
# End Source File
# Begin Source File

SOURCE=.\content_encoding.h
# End Source File
# Begin Source File

SOURCE=.\share.h
# End Source File
# Begin Source File

SOURCE=.\md5.h
# End Source File
# Begin Source File

SOURCE=.\http_digest.h
# End Source File
# Begin Source File

SOURCE=.\http_negotiate.h
# End Source File
# Begin Source File

SOURCE=.\http_ntlm.h
# End Source File
# Begin Source File

SOURCE=.\ca-bundle.h
# End Source File
# Begin Source File

SOURCE=.\inet_pton.h
# End Source File
# Begin Source File

SOURCE=.\strtoofft.h
# End Source File
# Begin Source File

SOURCE=.\strerror.h
# End Source File
# Begin Source File

SOURCE=.\inet_ntop.h
# End Source File
# Begin Source File

SOURCE=.\curlx.h
# End Source File
# Begin Source File

SOURCE=.\memory.h
# End Source File
# Begin Source File

SOURCE=.\setup.h
# End Source File
# Begin Source File

SOURCE=.\transfer.h
# End Source File
# Begin Source File

SOURCE=.\select.h
# End Source File
# Begin Source File

SOURCE=.\easyif.h
# End Source File
# Begin Source File

SOURCE=.\multiif.h
# End Source File
# Begin Source File

SOURCE=.\parsedate.h
# End Source File
# Begin Source File

SOURCE=.\sslgen.h
# End Source File
# Begin Source File

SOURCE=.\gtls.h
# End Source File
# Begin Source File

SOURCE=.\tftp.h
# End Source File
# Begin Source File

SOURCE=.\sockaddr.h
# End Source File
# Begin Source File

SOURCE=.\splay.h
# End Source File
# Begin Source File

SOURCE=.\strdup.h
# End Source File
# Begin Source File

SOURCE=.\setup_once.h
# End Source File
# Begin Source File

SOURCE=.\socks.h
# End Source File
# Begin Source File

SOURCE=.\config-win32.h
# End Source File
# End Group

# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
