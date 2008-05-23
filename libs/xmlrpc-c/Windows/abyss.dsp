# Microsoft Developer Studio Project File - Name="abyss" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=abyss - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "abyss.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "abyss.mak" CFG="abyss - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "abyss - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "abyss - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "abyss - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\Abyss"
# PROP Intermediate_Dir "Release\Abyss"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\\" /I "..\include" /I "..\lib\util\include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /D "_THREAD" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\abyss.lib"

!ELSEIF  "$(CFG)" == "abyss - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\abyss"
# PROP Intermediate_Dir "Debug\abyss"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\\" /I "..\include" /I "..\lib\util\include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "ABYSS_WIN32" /D "_THREAD" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib\abyssD.lib"

!ENDIF 

# Begin Target

# Name "abyss - Win32 Release"
# Name "abyss - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\lib\abyss\src\channel.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\chanswitch.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\conf.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\conn.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\data.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\date.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\file.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\handler.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\http.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\init.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\response.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\server.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\session.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket_openssl.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket_unix.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket_win.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\thread_fork.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\thread_pthread.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\thread_windows.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\token.c
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\trace.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\lib\abyss\src\abyss_info.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\channel.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\chanswitch.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\conn.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\data.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\date.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\file.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\handler.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\http.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\server.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\session.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\socket_win.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\thread.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\token.h
# End Source File
# Begin Source File

SOURCE=..\lib\abyss\src\trace.h
# End Source File
# End Group
# End Target
# End Project
