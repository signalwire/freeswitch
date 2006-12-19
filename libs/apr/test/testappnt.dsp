# Microsoft Developer Studio Project File - Name="testappnt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=testappnt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "testappnt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "testappnt.mak" CFG="testappnt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testappnt - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "testappnt - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "testappnt - Win32 Release"

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
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "WINNT" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /FD /c
# ADD CPP /nologo /MD /W3 /O2 /I "../include" /D "NDEBUG" /D "WIN32" /D "WINNT" /D "_CONSOLE" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /Fd"./testappnt" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib /nologo /subsystem:console
# ADD LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib /nologo /entry:"wmainCRTStartup" /subsystem:console

!ELSEIF  "$(CFG)" == "testappnt - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "."
# PROP BASE Intermediate_Dir "."
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "."
# PROP Intermediate_Dir "."
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /FD /c
# ADD CPP /nologo /MDd /W3 /EHsc /Zi /Od /I "../include" /D "_DEBUG" /D "WIN32" /D "WINNT" /D "_CONSOLE" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /Fd"./testappnt" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib /nologo /subsystem:console /incremental:no /debug
# ADD LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib /nologo /entry:"wmainCRTStartup" /subsystem:console /incremental:no /debug

!ENDIF 

# Begin Target

# Name "testappnt - Win32 Release"
# Name "testappnt - Win32 Debug"
# Begin Source File

SOURCE=.\testapp.c

!IF  "$(CFG)" == "testappnt - Win32 Release"

# ADD CPP /Fo"testappnt"

!ELSEIF  "$(CFG)" == "testappnt - Win32 Debug"

# ADD CPP /Fo"testappnt"

!ENDIF 

# End Source File
# End Target
# End Project
