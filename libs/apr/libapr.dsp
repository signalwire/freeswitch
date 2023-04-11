# Microsoft Developer Studio Project File - Name="libapr" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libapr - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libapr.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libapr.mak" CFG="libapr - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libapr - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libapr - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libapr - Win32 Release"

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
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /O2 /Oy- /Zi /I "./include" /I "./include/arch" /I "./include/arch/win32" /I "./include/arch/unix" /D "NDEBUG" /D "APR_DECLARE_EXPORT" /D "WIN32" /D "_WINDOWS" /Fd"Release\libfspr_src" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "APR_VERSION_ONLY" /I "./include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib ole32.lib shell32.lib rpcrt4.lib /nologo /base:"0x6EEC0000" /subsystem:windows /dll /incremental:no /debug /opt:ref
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib ole32.lib shell32.lib rpcrt4.lib /nologo /base:"0x6EEC0000" /subsystem:windows /dll /incremental:no /debug /out:"Release/libapr-1.dll" /opt:ref
# Begin Special Build Tool
OutDir=.\Release
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(OUTDIR)\libapr-1.dll.manifest mt.exe -manifest $(OUTDIR)\libapr-1.dll.manifest -outputresource:$(OUTDIR)\libapr-1.dll;2
# End Special Build Tool

!ELSEIF  "$(CFG)" == "libapr - Win32 Debug"

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
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MDd /W3 /EHsc /Zi /Od /I "./include" /I "./include/arch" /I "./include/arch/win32" /I "./include/arch/unix" /D "_DEBUG" /D "APR_DECLARE_EXPORT" /D "WIN32" /D "_WINDOWS" /Fd"Debug\libfspr_src" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "APR_VERSION_ONLY" /I "./include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib ole32.lib shell32.lib rpcrt4.lib /nologo /base:"0x6EEC0000" /subsystem:windows /dll /incremental:no /debug
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib ole32.lib shell32.lib rpcrt4.lib /nologo /base:"0x6EEC0000" /subsystem:windows /dll /incremental:no /debug /out:"Debug/libapr-1.dll"
# Begin Special Build Tool
OutDir=.\Debug
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(OUTDIR)\libapr-1.dll.manifest mt.exe -manifest $(OUTDIR)\libapr-1.dll.manifest -outputresource:$(OUTDIR)\libapr-1.dll;2
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libapr - Win32 Release"
# Name "libapr - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ".c"
# Begin Group "atomic"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\atomic\win32\fspr_atomic.c
# End Source File
# End Group
# Begin Group "dso"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dso\win32\dso.c
# End Source File
# End Group
# Begin Group "file_io"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\file_io\unix\copy.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\dir.c
# End Source File
# Begin Source File

SOURCE=.\file_io\unix\fileacc.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\filedup.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\filepath.c
# End Source File
# Begin Source File

SOURCE=.\file_io\unix\filepath_util.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\filestat.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\filesys.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\flock.c
# End Source File
# Begin Source File

SOURCE=.\file_io\unix\fullrw.c
# End Source File
# Begin Source File

SOURCE=.\file_io\unix\mktemp.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\open.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\pipe.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\readwrite.c
# End Source File
# Begin Source File

SOURCE=.\file_io\win32\seek.c
# End Source File
# Begin Source File

SOURCE=.\file_io\unix\tempdir.c
# End Source File
# End Group
# Begin Group "locks"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\locks\win32\proc_mutex.c
# End Source File
# Begin Source File

SOURCE=.\locks\win32\thread_cond.c
# End Source File
# Begin Source File

SOURCE=.\locks\win32\thread_mutex.c
# End Source File
# Begin Source File

SOURCE=.\locks\win32\thread_rwlock.c
# End Source File
# End Group
# Begin Group "memory"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\memory\unix\fspr_pools.c
# End Source File
# End Group
# Begin Group "misc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\misc\win32\fspr_app.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\misc\win32\charset.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\env.c
# End Source File
# Begin Source File

SOURCE=.\misc\unix\errorcodes.c
# End Source File
# Begin Source File

SOURCE=.\misc\unix\getopt.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\internal.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\misc.c
# End Source File
# Begin Source File

SOURCE=.\misc\unix\otherchild.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\rand.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\start.c
# End Source File
# Begin Source File

SOURCE=.\misc\win32\utf8.c
# End Source File
# Begin Source File

SOURCE=.\misc\unix\version.c
# End Source File
# End Group
# Begin Group "mmap"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\mmap\unix\common.c
# End Source File
# Begin Source File

SOURCE=.\mmap\win32\mmap.c
# End Source File
# End Group
# Begin Group "network_io"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\network_io\unix\inet_ntop.c
# End Source File
# Begin Source File

SOURCE=.\network_io\unix\inet_pton.c
# End Source File
# Begin Source File

SOURCE=.\poll\unix\select.c
# End Source File
# Begin Source File

SOURCE=.\network_io\unix\multicast.c
# End Source File
# Begin Source File

SOURCE=.\network_io\win32\sendrecv.c
# End Source File
# Begin Source File

SOURCE=.\network_io\unix\sockaddr.c
# End Source File
# Begin Source File

SOURCE=.\network_io\win32\sockets.c
# End Source File
# Begin Source File

SOURCE=.\network_io\win32\sockopt.c
# End Source File
# End Group
# Begin Group "passwd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\passwd\fspr_getpass.c
# End Source File
# End Group
# Begin Group "random"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\random\unix\fspr_random.c
# End Source File
# Begin Source File

SOURCE=.\random\unix\sha2.c
# End Source File
# Begin Source File

SOURCE=.\random\unix\sha2_glue.c
# End Source File
# End Group
# Begin Group "shmem"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\shmem\win32\shm.c
# End Source File
# End Group
# Begin Group "strings"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\strings\fspr_cpystrn.c
# End Source File
# Begin Source File

SOURCE=.\strings\fspr_fnmatch.c
# End Source File
# Begin Source File

SOURCE=.\strings\fspr_snprintf.c
# End Source File
# Begin Source File

SOURCE=.\strings\fspr_strings.c
# End Source File
# Begin Source File

SOURCE=.\strings\fspr_strnatcmp.c
# End Source File
# Begin Source File

SOURCE=.\strings\fspr_strtok.c
# End Source File
# End Group
# Begin Group "tables"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\tables\fspr_hash.c
# End Source File
# Begin Source File

SOURCE=.\tables\fspr_tables.c
# End Source File
# End Group
# Begin Group "threadproc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\threadproc\win32\proc.c
# End Source File
# Begin Source File

SOURCE=.\threadproc\win32\signals.c
# End Source File
# Begin Source File

SOURCE=.\threadproc\win32\thread.c
# End Source File
# Begin Source File

SOURCE=.\threadproc\win32\threadpriv.c
# End Source File
# End Group
# Begin Group "time"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\time\win32\access.c
# End Source File
# Begin Source File

SOURCE=.\time\win32\time.c
# End Source File
# Begin Source File

SOURCE=.\time\win32\timestr.c
# End Source File
# End Group
# Begin Group "user"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\user\win32\groupinfo.c
# End Source File
# Begin Source File

SOURCE=.\user\win32\userinfo.c
# End Source File
# End Group
# End Group
# Begin Group "Private Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_atime.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_dso.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_file_io.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_inherit.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_misc.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_networkio.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_thread_mutex.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_thread_rwlock.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_threadproc.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_arch_utf8.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\win32\fspr_private.h
# End Source File
# Begin Source File

SOURCE=.\include\arch\fspr_private_common.h
# End Source File
# End Group
# Begin Group "Public Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\fspr.h.in
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\include\fspr.hnw
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\include\fspr.hw

!IF  "$(CFG)" == "libapr - Win32 Release"

# Begin Custom Build - Creating apr.h from apr.hw
InputPath=.\include\fspr.hw

".\include\fspr.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\fspr.hw > .\include\fspr.h

# End Custom Build

!ELSEIF  "$(CFG)" == "libapr - Win32 Debug"

# Begin Custom Build - Creating apr.h from apr.hw
InputPath=.\include\fspr.hw

".\include\fspr.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\fspr.hw > .\include\fspr.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\fspr_allocator.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_atomic.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_dso.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_env.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_errno.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_file_info.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_file_io.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_fnmatch.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_general.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_getopt.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_global_mutex.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_hash.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_inherit.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_lib.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_mmap.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_network_io.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_poll.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_pools.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_portable.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_proc_mutex.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_ring.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_shm.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_signal.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_strings.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_support.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_tables.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_thread_cond.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_thread_mutex.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_thread_proc.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_thread_rwlock.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_time.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_user.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_version.h
# End Source File
# Begin Source File

SOURCE=.\include\fspr_want.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\libapr.rc
# End Source File
# End Target
# End Project
