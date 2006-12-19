# Microsoft Developer Studio Project File - Name="testall" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=testall - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "testall.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "testall.mak" CFG="testall - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testall - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "testall - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "testall - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win all"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "testall.exe"
# PROP BASE Bsc_Name "testall.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win all"
# PROP Rebuild_Opt "/a"
# PROP Target_File "testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testall - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win all"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "testall.exe"
# PROP BASE Bsc_Name "testall.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win all"
# PROP Rebuild_Opt "/a"
# PROP Target_File "testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "testall - Win32 Release"
# Name "testall - Win32 Debug"

!IF  "$(CFG)" == "testall - Win32 Release"

!ELSEIF  "$(CFG)" == "testall - Win32 Debug"

!ENDIF 

# Begin Source File

SOURCE=.\abts.c
# End Source File
# Begin Source File

SOURCE=.\globalmutexchild.c
# End Source File
# Begin Source File

SOURCE=.\Makefile.win
# End Source File
# Begin Source File

SOURCE=.\mod_test.c
# End Source File
# Begin Source File

SOURCE=.\nw_misc.c
# End Source File
# Begin Source File

SOURCE=.\occhild.c
# End Source File
# Begin Source File

SOURCE=.\proc_child.c
# End Source File
# Begin Source File

SOURCE=.\readchild.c
# End Source File
# Begin Source File

SOURCE=.\sendfile.c
# End Source File
# Begin Source File

SOURCE=.\sockchild.c
# End Source File
# Begin Source File

SOURCE=.\testapp.c
# End Source File
# Begin Source File

SOURCE=.\testargs.c
# End Source File
# Begin Source File

SOURCE=.\testatomic.c
# End Source File
# Begin Source File

SOURCE=.\testdir.c
# End Source File
# Begin Source File

SOURCE=.\testdso.c
# End Source File
# Begin Source File

SOURCE=.\testdup.c
# End Source File
# Begin Source File

SOURCE=.\testenv.c
# End Source File
# Begin Source File

SOURCE=.\testfile.c
# End Source File
# Begin Source File

SOURCE=.\testfilecopy.c
# End Source File
# Begin Source File

SOURCE=.\testfileinfo.c
# End Source File
# Begin Source File

SOURCE=.\testflock.c
# End Source File
# Begin Source File

SOURCE=.\testfmt.c
# End Source File
# Begin Source File

SOURCE=.\testfnmatch.c
# End Source File
# Begin Source File

SOURCE=.\testglobalmutex.c
# End Source File
# Begin Source File

SOURCE=.\testhash.c
# End Source File
# Begin Source File

SOURCE=.\testipsub.c
# End Source File
# Begin Source File

SOURCE=.\testlfs.c
# End Source File
# Begin Source File

SOURCE=.\testlock.c
# End Source File
# Begin Source File

SOURCE=.\testlockperf.c
# End Source File
# Begin Source File

SOURCE=.\testmmap.c
# End Source File
# Begin Source File

SOURCE=.\testmutexscope.c
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

SOURCE=.\testpoll.c
# End Source File
# Begin Source File

SOURCE=.\testpools.c
# End Source File
# Begin Source File

SOURCE=.\testproc.c
# End Source File
# Begin Source File

SOURCE=.\testprocmutex.c
# End Source File
# Begin Source File

SOURCE=.\testrand.c
# End Source File
# Begin Source File

SOURCE=.\testrand2.c
# End Source File
# Begin Source File

SOURCE=.\testshm.c
# End Source File
# Begin Source File

SOURCE=.\testshmconsumer.c
# End Source File
# Begin Source File

SOURCE=.\testshmproducer.c
# End Source File
# Begin Source File

SOURCE=.\testsleep.c
# End Source File
# Begin Source File

SOURCE=.\testsock.c
# End Source File
# Begin Source File

SOURCE=.\testsockets.c
# End Source File
# Begin Source File

SOURCE=.\testsockopt.c
# End Source File
# Begin Source File

SOURCE=.\teststr.c
# End Source File
# Begin Source File

SOURCE=.\teststrnatcmp.c
# End Source File
# Begin Source File

SOURCE=.\testtable.c
# End Source File
# Begin Source File

SOURCE=.\testtemp.c
# End Source File
# Begin Source File

SOURCE=.\testthread.c
# End Source File
# Begin Source File

SOURCE=.\testtime.c
# End Source File
# Begin Source File

SOURCE=.\testud.c
# End Source File
# Begin Source File

SOURCE=.\testuser.c
# End Source File
# Begin Source File

SOURCE=.\testutil.c
# End Source File
# Begin Source File

SOURCE=.\testvsn.c
# End Source File
# Begin Source File

SOURCE=.\tryread.c
# End Source File
# End Target
# End Project
