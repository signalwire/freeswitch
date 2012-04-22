# Microsoft Developer Studio Project File - Name="aprtest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=aprtest - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "aprtest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "aprtest.mak" CFG="aprtest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "aprtest - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "aprtest - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "aprtest - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f Makefile"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "aprtest.exe"
# PROP BASE Bsc_Name "aprtest.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f aprtest.win"
# PROP Rebuild_Opt "/a"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "aprtest - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f aprtest.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "aprtest.exe"
# PROP BASE Bsc_Name "aprtest.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f aprtest.win"
# PROP Rebuild_Opt "/a"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "aprtest - Win32 Release"
# Name "aprtest - Win32 Debug"

!IF  "$(CFG)" == "aprtest - Win32 Release"

!ELSEIF  "$(CFG)" == "aprtest - Win32 Debug"

!ENDIF 

# Begin Group "Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\abc.c
# End Source File
# Begin Source File

SOURCE=.\client.c
# End Source File
# Begin Source File

SOURCE=.\mod_test.c
# End Source File
# Begin Source File

SOURCE=.\occhild.c
# End Source File
# Begin Source File

SOURCE=.\sendfile.c
# End Source File
# Begin Source File

SOURCE=.\server.c
# End Source File
# Begin Source File

SOURCE=.\testargs.c
# End Source File
# Begin Source File

SOURCE=.\testcontext.c
# End Source File
# Begin Source File

SOURCE=.\testdso.c
# End Source File
# Begin Source File

SOURCE=.\testfile.c
# End Source File
# Begin Source File

SOURCE=.\testflock.c
# End Source File
# Begin Source File

SOURCE=.\testlock.c
# End Source File
# Begin Source File

SOURCE=.\testmmap.c
# End Source File
# Begin Source File

SOURCE=.\testnames.c
# End Source File
# Begin Source File

SOURCE=.\testoc.c
# End Source File
# Begin Source File

SOURCE=.\testpath.c
# End Source File
# Begin Source File

SOURCE=.\testpipe.c
# End Source File
# Begin Source File

SOURCE=.\testproc.c
# End Source File
# Begin Source File

SOURCE=.\testshm.c
# End Source File
# Begin Source File

SOURCE=.\testsock.c
# End Source File
# Begin Source File

SOURCE=.\testthread.c
# End Source File
# Begin Source File

SOURCE=.\testtime.c
# End Source File
# Begin Source File

SOURCE=.\testucs.c
# End Source File
# Begin Source File

SOURCE=.\testuser.c
# End Source File
# Begin Source File

SOURCE=.\testuuid.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\aprtest.win
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\Makefile.in
# End Source File
# End Target
# End Project
