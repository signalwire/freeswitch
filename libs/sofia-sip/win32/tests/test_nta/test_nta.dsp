# Microsoft Developer Studio Project File - Name="test_nta" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=test_nta - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "test_nta.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "test_nta.mak" CFG="test_nta - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "test_nta - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "test_nta - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "test_nta - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /WX /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD CPP /nologo /MD /W3 /WX /GX /O2 /I "..\.." /I "..\..\..\libsofia-sip-ua\su" /I "..\..\..\libsofia-sip-ua\ipt" /I "..\..\..\libsofia-sip-ua\sresolv" /I "..\..\..\libsofia-sip-ua\bnf" /I "..\..\..\libsofia-sip-ua\url" /I "..\..\..\libsofia-sip-ua\msg" /I "..\..\..\libsofia-sip-ua\sip" /I "..\..\..\libsofia-sip-ua\nta" /I "..\..\..\libsofia-sip-ua\nua" /I "..\..\..\libsofia-sip-ua\iptsec" /I "..\..\..\libsofia-sip-ua\http" /I "..\..\..\libsofia-sip-ua\nth" /I "..\..\..\libsofia-sip-ua\nea" /I "..\..\..\libsofia-sip-ua\sdp" /I "..\..\..\libsofia-sip-ua\soa" /I "..\..\..\libsofia-sip-ua\stun" /I "..\..\..\libsofia-sip-ua\tport" /I "..\..\..\libsofia-sip-ua\features" /I "include" /I "." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "test_nta - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /WX /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /WX /Gm /GX /ZI /Od /I "..\.." /I "..\..\..\libsofia-sip-ua\su" /I "..\..\..\libsofia-sip-ua\ipt" /I "..\..\..\libsofia-sip-ua\sresolv" /I "..\..\..\libsofia-sip-ua\bnf" /I "..\..\..\libsofia-sip-ua\url" /I "..\..\..\libsofia-sip-ua\msg" /I "..\..\..\libsofia-sip-ua\sip" /I "..\..\..\libsofia-sip-ua\nta" /I "..\..\..\libsofia-sip-ua\nua" /I "..\..\..\libsofia-sip-ua\iptsec" /I "..\..\..\libsofia-sip-ua\http" /I "..\..\..\libsofia-sip-ua\nth" /I "..\..\..\libsofia-sip-ua\nea" /I "..\..\..\libsofia-sip-ua\sdp" /I "..\..\..\libsofia-sip-ua\soa" /I "..\..\..\libsofia-sip-ua\stun" /I "..\..\..\libsofia-sip-ua\tport" /I "..\..\..\libsofia-sip-ua\features" /I "include" /I "." /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "test_nta - Win32 Release"
# Name "test_nta - Win32 Debug"
# Begin Source File

SOURCE="..\..\..\libsofia-sip-ua\nta\test_nta.c"
# End Source File
# Begin Source File

SOURCE=..\..\pthread\pthreadVC2.lib
# End Source File
# End Target
# End Project
