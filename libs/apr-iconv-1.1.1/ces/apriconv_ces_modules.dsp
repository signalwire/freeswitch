# Microsoft Developer Studio Project File - Name="apriconv_ces_modules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=apriconv_ces_modules - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "apriconv_ces_modules.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "apriconv_ces_modules.mak" CFG="apriconv_ces_modules - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "apriconv_ces_modules - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "apriconv_ces_modules - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "apriconv_ces_modules - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\Release\iconv"
# PROP BASE Intermediate_Dir "..\Release\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=release BIND_MODE=static"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "apriconv_ces_modules.exe"
# PROP BASE Bsc_Name "apriconv_ces_modules.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Release\iconv"
# PROP Intermediate_Dir "..\Release\iconv"
# PROP Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=release BIND_MODE=static"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Release"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "apriconv_ces_modules - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\Debug\iconv"
# PROP BASE Intermediate_Dir "..\Debug\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=debug BIND_MODE=static"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "apriconv_ces_modules.exe"
# PROP BASE Bsc_Name "apriconv_ces_modules.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug\iconv"
# PROP Intermediate_Dir "..\Debug\iconv"
# PROP Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=debug BIND_MODE=static"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Debug"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "apriconv_ces_modules - Win32 Release"
# Name "apriconv_ces_modules - Win32 Debug"

!IF  "$(CFG)" == "apriconv_ces_modules - Win32 Release"

!ELSEIF  "$(CFG)" == "apriconv_ces_modules - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\_tbl_simple.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\euc-jp.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\euc-kr.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\euc-tw.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\gb2312.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-10646-ucs-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-10646-ucs-4.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-2022-cn.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-2022-jp-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-2022-jp.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-2022-kr.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ucs2-internal.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ucs4-internal.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\unicode-1-1-utf-7.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\utf-16.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\utf-8.c"
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\lib\iconv.h
# End Source File
# End Group
# End Target
# End Project
