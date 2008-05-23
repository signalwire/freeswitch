# Microsoft Developer Studio Project File - Name="xmlrpc_cpp_proxy" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=xmlrpc_cpp_proxy - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc_cpp_proxy.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xmlrpc_cpp_proxy.mak" CFG="xmlrpc_cpp_proxy - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xmlrpc_cpp_proxy - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "xmlrpc_cpp_proxy - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xmlrpc_cpp_proxy - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\xmlrpc_cpp_proxy"
# PROP Intermediate_Dir "Release\xmlrpc_cpp_proxy"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I ".." /I "../include" /I "../lib/util/include" /I "../.." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "ABYSS_WIN32" /D "CURL_STATICLIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ..\lib\xmlrpccpp.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib Wininet.lib /nologo /subsystem:console /machine:I386 /out:"..\bin\xmlrpc_cpp_proxy.exe"

!ELSEIF  "$(CFG)" == "xmlrpc_cpp_proxy - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\xmlrpc_cpp_proxy"
# PROP Intermediate_Dir "Debug\xmlrpc_cpp_proxy"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../.." /I ".." /I "../include" /I "../lib/util/include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "ABYSS_WIN32" /D "CURL_STATICLIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\lib\xmlrpccppD.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib Wininet.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\bin\xmlrpc_cpp_proxyD.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "xmlrpc_cpp_proxy - Win32 Release"
# Name "xmlrpc_cpp_proxy - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\proxyClass.cpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\systemProxy.cpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\xmlrpcMethod.cpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\xmlrpcType.cpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\xmlrpc_cpp_proxy.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\proxyClass.hpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\systemProxy.hpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\xmlrpcMethod.hpp
# End Source File
# Begin Source File

SOURCE=..\tools\xmlrpc_cpp_proxy\xmlrpcType.hpp
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
