# Microsoft Developer Studio Project File - Name="libapriconv_ccs_modules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=libapriconv_ccs_modules - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libapriconv_ccs_modules.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libapriconv_ccs_modules.mak" CFG="libapriconv_ccs_modules - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libapriconv_ccs_modules - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "libapriconv_ccs_modules - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "libapriconv_ccs_modules - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\Release\iconv"
# PROP BASE Intermediate_Dir "..\Release\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=release BIND_MODE=shared"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libapriconv_ccs_modules.exe"
# PROP BASE Bsc_Name "libapriconv_ccs_modules.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Release\iconv"
# PROP Intermediate_Dir "..\Release\iconv"
# PROP Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=release BIND_MODE=shared"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Release"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "libapriconv_ccs_modules - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\Debug\iconv"
# PROP BASE Intermediate_Dir "..\Debug\iconv"
# PROP BASE Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=debug BIND_MODE=shared"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libapriconv_ccs_modules.exe"
# PROP BASE Bsc_Name "libapriconv_ccs_modules.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug\iconv"
# PROP Intermediate_Dir "..\Debug\iconv"
# PROP Cmd_Line "NMAKE /nologo /f Makefile.win BUILD_MODE=debug BIND_MODE=shared"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Debug"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "libapriconv_ccs_modules - Win32 Release"
# Name "libapriconv_ccs_modules - Win32 Debug"

!IF  "$(CFG)" == "libapriconv_ccs_modules - Win32 Release"

!ELSEIF  "$(CFG)" == "libapriconv_ccs_modules - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=".\adobe-stdenc.c"
# End Source File
# Begin Source File

SOURCE=".\adobe-symbol.c"
# End Source File
# Begin Source File

SOURCE=".\adobe-zdingbats.c"
# End Source File
# Begin Source File

SOURCE=.\big5.c
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane1.c"
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane14.c"
# End Source File
# Begin Source File

SOURCE=".\cns11643-plane2.c"
# End Source File
# Begin Source File

SOURCE=.\cp037.c
# End Source File
# Begin Source File

SOURCE=.\cp038.c
# End Source File
# Begin Source File

SOURCE=.\cp10000.c
# End Source File
# Begin Source File

SOURCE=.\cp10006.c
# End Source File
# Begin Source File

SOURCE=.\cp10007.c
# End Source File
# Begin Source File

SOURCE=.\cp10029.c
# End Source File
# Begin Source File

SOURCE=.\cp1006.c
# End Source File
# Begin Source File

SOURCE=.\cp10079.c
# End Source File
# Begin Source File

SOURCE=.\cp10081.c
# End Source File
# Begin Source File

SOURCE=.\cp1026.c
# End Source File
# Begin Source File

SOURCE=.\cp273.c
# End Source File
# Begin Source File

SOURCE=.\cp274.c
# End Source File
# Begin Source File

SOURCE=.\cp275.c
# End Source File
# Begin Source File

SOURCE=.\cp277.c
# End Source File
# Begin Source File

SOURCE=.\cp278.c
# End Source File
# Begin Source File

SOURCE=.\cp280.c
# End Source File
# Begin Source File

SOURCE=.\cp281.c
# End Source File
# Begin Source File

SOURCE=.\cp284.c
# End Source File
# Begin Source File

SOURCE=.\cp285.c
# End Source File
# Begin Source File

SOURCE=.\cp290.c
# End Source File
# Begin Source File

SOURCE=.\cp297.c
# End Source File
# Begin Source File

SOURCE=.\cp420.c
# End Source File
# Begin Source File

SOURCE=.\cp423.c
# End Source File
# Begin Source File

SOURCE=.\cp424.c
# End Source File
# Begin Source File

SOURCE=.\cp437.c
# End Source File
# Begin Source File

SOURCE=.\cp500.c
# End Source File
# Begin Source File

SOURCE=.\cp737.c
# End Source File
# Begin Source File

SOURCE=.\cp775.c
# End Source File
# Begin Source File

SOURCE=.\cp850.c
# End Source File
# Begin Source File

SOURCE=.\cp851.c
# End Source File
# Begin Source File

SOURCE=.\cp852.c
# End Source File
# Begin Source File

SOURCE=.\cp855.c
# End Source File
# Begin Source File

SOURCE=.\cp856.c
# End Source File
# Begin Source File

SOURCE=.\cp857.c
# End Source File
# Begin Source File

SOURCE=.\cp860.c
# End Source File
# Begin Source File

SOURCE=.\cp861.c
# End Source File
# Begin Source File

SOURCE=.\cp862.c
# End Source File
# Begin Source File

SOURCE=.\cp863.c
# End Source File
# Begin Source File

SOURCE=.\cp864.c
# End Source File
# Begin Source File

SOURCE=.\cp865.c
# End Source File
# Begin Source File

SOURCE=.\cp866.c
# End Source File
# Begin Source File

SOURCE=.\cp868.c
# End Source File
# Begin Source File

SOURCE=.\cp869.c
# End Source File
# Begin Source File

SOURCE=.\cp870.c
# End Source File
# Begin Source File

SOURCE=.\cp871.c
# End Source File
# Begin Source File

SOURCE=.\cp874.c
# End Source File
# Begin Source File

SOURCE=.\cp875.c
# End Source File
# Begin Source File

SOURCE=.\cp880.c
# End Source File
# Begin Source File

SOURCE=.\cp891.c
# End Source File
# Begin Source File

SOURCE=.\cp903.c
# End Source File
# Begin Source File

SOURCE=.\cp904.c
# End Source File
# Begin Source File

SOURCE=.\cp905.c
# End Source File
# Begin Source File

SOURCE=.\cp918.c
# End Source File
# Begin Source File

SOURCE=.\cp932.c
# End Source File
# Begin Source File

SOURCE=.\cp936.c
# End Source File
# Begin Source File

SOURCE=.\cp949.c
# End Source File
# Begin Source File

SOURCE=.\cp950.c
# End Source File
# Begin Source File

SOURCE=".\dec-mcs.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-at-de-a.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-at-de.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-ca-fr.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-dk-no-a.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-dk-no.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es-a.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es-s.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-es.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fi-se-a.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fi-se.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-fr.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-it.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-pt.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-uk.c"
# End Source File
# Begin Source File

SOURCE=".\ebcdic-us.c"
# End Source File
# Begin Source File

SOURCE=.\gb12345.c
# End Source File
# Begin Source File

SOURCE=".\gb_2312-80.c"
# End Source File
# Begin Source File

SOURCE=".\hp-roman8.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-1.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-10.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-13.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-14.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-15.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-2.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-3.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-4.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-5.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-6.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-7.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-8.c"
# End Source File
# Begin Source File

SOURCE=".\iso-8859-9.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-10.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-102.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-103.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-11.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-111.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-121.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-122.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-123.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-128.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-13.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-139.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-14.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-141.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-142.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-143.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-146.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-147.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-15.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-150.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-151.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-152.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-153.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-154.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-155.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-158.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-16.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-17.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-18.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-19.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-2.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-21.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-25.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-27.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-37.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-4.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-47.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-49.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-50.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-51.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-54.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-55.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-57.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-60.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-61.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-69.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-70.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-8-1.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-8-2.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-84.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-85.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-86.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-88.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-89.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-9-1.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-9-2.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-90.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-91.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-92.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-93.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-94.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-95.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-96.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-98.c"
# End Source File
# Begin Source File

SOURCE=".\iso-ir-99.c"
# End Source File
# Begin Source File

SOURCE=".\iso646-dk.c"
# End Source File
# Begin Source File

SOURCE=".\iso646-kr.c"
# End Source File
# Begin Source File

SOURCE=.\jis_x0201.c
# End Source File
# Begin Source File

SOURCE=".\jis_x0208-1983.c"
# End Source File
# Begin Source File

SOURCE=".\jis_x0212-1990.c"
# End Source File
# Begin Source File

SOURCE=.\johab.c
# End Source File
# Begin Source File

SOURCE=".\koi8-r.c"
# End Source File
# Begin Source File

SOURCE=".\koi8-ru.c"
# End Source File
# Begin Source File

SOURCE=".\koi8-u.c"
# End Source File
# Begin Source File

SOURCE=.\ksx1001.c
# End Source File
# Begin Source File

SOURCE=".\mac-ce.c"
# End Source File
# Begin Source File

SOURCE=".\mac-croatian.c"
# End Source File
# Begin Source File

SOURCE=".\mac-cyrillic.c"
# End Source File
# Begin Source File

SOURCE=".\mac-dingbats.c"
# End Source File
# Begin Source File

SOURCE=".\mac-greek.c"
# End Source File
# Begin Source File

SOURCE=".\mac-iceland.c"
# End Source File
# Begin Source File

SOURCE=".\mac-japan.c"
# End Source File
# Begin Source File

SOURCE=".\mac-roman.c"
# End Source File
# Begin Source File

SOURCE=".\mac-romania.c"
# End Source File
# Begin Source File

SOURCE=".\mac-thai.c"
# End Source File
# Begin Source File

SOURCE=".\mac-turkish.c"
# End Source File
# Begin Source File

SOURCE=".\mac-ukraine.c"
# End Source File
# Begin Source File

SOURCE=.\macintosh.c
# End Source File
# Begin Source File

SOURCE=.\shift_jis.c
# End Source File
# Begin Source File

SOURCE=".\us-ascii.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1250.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1251.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1252.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1253.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1254.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1255.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1256.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1257.c"
# End Source File
# Begin Source File

SOURCE=".\windows-1258.c"
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
