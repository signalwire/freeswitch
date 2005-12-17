# Microsoft Developer Studio Project File - Name="jrtplib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=jrtplib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "jrtplib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "jrtplib.mak" CFG="jrtplib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "jrtplib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "jrtplib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "jrtplib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\jthread-1.1.2\src" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x813 /d "NDEBUG"
# ADD RSC /l 0x813 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "jrtplib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\jthread-1.1.2\src" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x813 /d "_DEBUG"
# ADD RSC /l 0x813 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "jrtplib - Win32 Release"
# Name "jrtplib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\rtcpapppacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpbyepacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpcompoundpacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpcompoundpacketbuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcppacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcppacketbuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcprrpacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpscheduler.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsdesinfo.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsdespacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsrpacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpcollisionlist.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpdebug.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtperrors.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpinternalsourcedata.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv4address.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv6address.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtplibraryversion.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtppacket.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtppacketbuilder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtppollthread.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtprandom.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpsession.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpsessionparams.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpsessionsources.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpsourcedata.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpsources.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtptimeutilities.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpudpv4transmitter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\jrtp4c.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\jmutex.cpp
# End Source File
# Begin Source File

SOURCE=.\src\win32\jthread.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rtpudpv6transmitter.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\rtcpapppacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpbyepacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpcompoundpacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpcompoundpacketbuilder.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcppacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcppacketbuilder.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcprrpacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpscheduler.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsdesinfo.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsdespacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpsrpacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtcpunknownpacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpaddress.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpcollisionlist.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpconfig.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpconfig_win.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpdebug.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpdefines.h
# End Source File
# Begin Source File

SOURCE=.\src\rtperrors.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpeventhandler.h
# End Source File
# Begin Source File

SOURCE=.\src\rtphashtable.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpinternalsourcedata.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv4address.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv4destination.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv6address.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpipv6destination.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpkeyhashtable.h
# End Source File
# Begin Source File

SOURCE=.\src\rtplibraryversion.h
# End Source File
# Begin Source File

SOURCE=.\src\rtppacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtppacketbuilder.h
# End Source File
# Begin Source File

SOURCE=.\src\rtppollthread.h
# End Source File
# Begin Source File

SOURCE=.\src\rtprandom.h
# End Source File
# Begin Source File

SOURCE=.\src\rtprawpacket.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpsession.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpsessionparams.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpsessionsources.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpsourcedata.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpsources.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpstructs.h
# End Source File
# Begin Source File

SOURCE=.\src\rtptimeutilities.h
# End Source File
# Begin Source File

SOURCE=.\src\rtptransmitter.h
# End Source File
# Begin Source File

SOURCE=.\src\rtptypes.h
# End Source File
# Begin Source File

SOURCE=.\src\rtptypes_win.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpudpv4transmitter.h
# End Source File
# Begin Source File

SOURCE=.\src\rtpudpv6transmitter.h
# End Source File
# End Group
# End Target
# End Project
