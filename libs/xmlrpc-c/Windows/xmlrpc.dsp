# Microsoft Developer Studio Project File - Name="xmlrpc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=xmlrpc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc.mak" CFG="xmlrpc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xmlrpc - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "xmlrpc - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\xmlrpc"
# PROP Intermediate_Dir "Release\xmlrpc"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../lib" /I "../lib/util/include" /I "../include" /I ".." /I "../lib/expat/xmlparse" /I "../lib/abyss/src" /I "../lib/wininet_transport" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /D "CURL_STATICLIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\xmlrpc.lib"

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\xmlrpc"
# PROP Intermediate_Dir "Debug\xmlrpc"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../lib" /I "../lib/util/include" /I "../include" /I ".." /I "../lib/expat/xmlparse" /I "../lib/abyss/src" /I "../lib/wininet_transport" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /D "CURL_STATICLIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\xmlrpcD.lib"

!ENDIF 

# Begin Target

# Name "xmlrpc - Win32 Release"
# Name "xmlrpc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;cc"
# Begin Source File

SOURCE=..\lib\libutil\asprintf.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\base64.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\error.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\make_printable.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\memblock.c
# End Source File
# Begin Source File

SOURCE=..\src\method.c
# End Source File
# Begin Source File

SOURCE=..\lib\util\pthreadx_win32.c
# End Source File
# Begin Source File

SOURCE=..\src\parse_datetime.c
# End Source File
# Begin Source File

SOURCE=..\src\parse_value.c
# End Source File
# Begin Source File

SOURCE=..\src\registry.c
# End Source File
# Begin Source File

SOURCE=..\src\resource.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\select.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\sleep.c
# End Source File
# Begin Source File

SOURCE=..\src\system_method.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\time.c
# End Source File
# Begin Source File

SOURCE=..\src\trace.c
# End Source File
# Begin Source File

SOURCE=..\src\version.c
# End Source File
# Begin Source File

SOURCE=..\lib\libutil\utf8.c
# End Source File
# Begin Source File

SOURCE=..\src\double.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_array.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_authcookie.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_base64.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_build.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_client.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_client_global.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_server_info.c
# End Source File
# Begin Source File

SOURCE=..\lib\curl_transport\xmlrpc_curl_transport.c

!IF  "$(CFG)" == "xmlrpc - Win32 Release"

# ADD CPP /I "." /I "..\..\curl\include"
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "xmlrpc - Win32 Debug"

# ADD CPP /I "." /I "..\..\curl\include"
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_data.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_datetime.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_decompose.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_expat.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_libxml2.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_parse.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_serialize.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_server_abyss.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_server_w32httpsys.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_string.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlrpc_struct.c
# End Source File
# Begin Source File

SOURCE=..\lib\wininet_transport\xmlrpc_wininet_transport.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\lib\abyss\src\http.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\abyss.h"
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\abyss_info.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\abyss_winsock.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\base.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\base_int.h"
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\bool.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\c_util.h"
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\c_util.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\casprintf.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\channel.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\chanswitch.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\client.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\client_global.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\client_int.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\config.h"
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\conn.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\date.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\file.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\girmath.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\handler.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\inline.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\linklist.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\mallocvar.h
# End Source File
# Begin Source File

SOURCE=..\src\double.h
# End Source File
# Begin Source File

SOURCE=..\src\method.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\pthreadx.h
# End Source File
# Begin Source File

SOURCE=..\src\parse_datetime.h
# End Source File
# Begin Source File

SOURCE=..\src\parse_value.h
# End Source File
# Begin Source File

SOURCE=..\src\registry.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\server.h"
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\server.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\server_abyss.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\server_cgi.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\server_w32httpsys.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\sleep_int.h"
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket_win.h
# End Source File
# Begin Source File

SOURCE=..\lib\util\include\stdargx.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\string_int.h"
# End Source File
# Begin Source File

SOURCE=..\src\system_method.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\thread.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\time_int.h"
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\token.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\transport.h"
# End Source File
# Begin Source File

SOURCE=..\transport_config.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\transport_int.h"
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\util_int.h"
# End Source File
# Begin Source File

SOURCE=..\xml_rpc_alloc.h
# End Source File
# Begin Source File

SOURCE=..\lib\expat\xmlparse\xmlparse.h
# End Source File
# Begin Source File

SOURCE="..\include\xmlrpc-c\xmlparser.h"
# End Source File
# Begin Source File

SOURCE=..\xmlrpc_config.h
# End Source File
# Begin Source File

SOURCE=..\lib\curl_transport\xmlrpc_curl_transport.h
# End Source File
# Begin Source File

SOURCE=..\lib\wininet_transport\xmlrpc_wininet_transport.h
# End Source File
# End Group
# End Target
# End Project
