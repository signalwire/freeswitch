# Microsoft Developer Studio Project File - Name="apriconv_ccs_modules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=apriconv_ccs_modules - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "apriconv_ccs_modules.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "apriconv_ccs_modules.mak" CFG="apriconv_ccs_modules - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "apriconv_ccs_modules - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "apriconv_ccs_modules - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "apriconv_ccs_modules - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\Release\iconv"
# PROP BASE Intermediate_Dir "..\Release\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=release BIND_MODE=static"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "apriconv_ccs_modules.exe"
# PROP BASE Bsc_Name "apriconv_ccs_modules.bsc"
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

!ELSEIF  "$(CFG)" == "apriconv_ccs_modules - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\Debug\iconv"
# PROP BASE Intermediate_Dir "..\Debug\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=debug BIND_MODE=static"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "apriconv_ccs_modules.exe"
# PROP BASE Bsc_Name "apriconv_ccs_modules.bsc"
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

# Name "apriconv_ccs_modules - Win32 Release"
# Name "apriconv_ccs_modules - Win32 Debug"

!IF  "$(CFG)" == "apriconv_ccs_modules - Win32 Release"

!ELSEIF  "$(CFG)" == "apriconv_ccs_modules - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=".\adobe-stdenc.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\adobe-symbol.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\adobe-zdingbats.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\big5.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane1.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane14.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp037.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp038.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10000.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10006.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10007.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10029.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp1006.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10079.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp10081.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp1026.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp273.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp274.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp275.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp277.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp278.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp280.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp281.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp284.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp285.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp290.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp297.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp420.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp423.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp424.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp437.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp500.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp737.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp775.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp850.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp851.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp852.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp855.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp856.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp857.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp860.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp861.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp862.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp863.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp864.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp865.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp866.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp868.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp869.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp870.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp871.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp874.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp875.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp880.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp891.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp903.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp904.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp905.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp918.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp932.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp936.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp949.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\cp950.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\dec-mcs.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-at-de-a.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-at-de.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-ca-fr.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-dk-no-a.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-dk-no.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es-a.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es-s.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fi-se-a.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fi-se.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fr.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-it.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-pt.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-uk.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\ebcdic-us.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\gb12345.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\gb_2312-80.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\hp-roman8.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-1.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-10.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-13.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-14.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-15.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-3.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-4.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-5.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-6.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-7.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-8.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-8859-9.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-10.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-102.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-103.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-11.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-111.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-121.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-122.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-123.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-128.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-13.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-139.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-14.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-141.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-142.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-143.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-146.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-147.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-15.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-150.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-151.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-152.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-153.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-154.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-155.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-158.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-16.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-17.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-18.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-19.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-21.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-25.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-27.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-37.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-4.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-47.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-49.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-50.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-51.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-54.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-55.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-57.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-60.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-61.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-69.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-70.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-8-1.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-8-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-84.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-85.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-86.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-88.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-89.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-9-1.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-9-2.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-90.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-91.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-92.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-93.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-94.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-95.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-96.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-98.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso-ir-99.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso646-dk.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\iso646-kr.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\jis_x0201.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\jis_x0208-1983.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\jis_x0212-1990.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\johab.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\koi8-r.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\koi8-ru.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\koi8-u.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\ksx1001.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-ce.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-croatian.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-cyrillic.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-dingbats.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-greek.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-iceland.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-japan.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-roman.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-romania.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-thai.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-turkish.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\mac-ukraine.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\macintosh.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\shift_jis.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\us-ascii.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1250.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1251.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1252.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1253.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1254.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1255.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1256.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1257.c"
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=".\windows-1258.c"
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
