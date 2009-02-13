# Microsoft Developer Studio Project File - Name="libspeexdsp_dynamic" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libspeexdsp_dynamic - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libspeexdsp_dynamic.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libspeexdsp_dynamic.mak" CFG="libspeexdsp_dynamic - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libspeexdsp_dynamic - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libspeexdsp_dynamic - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libspeexdsp_dynamic - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libspeexdsp_dynamic___Win32_Release"
# PROP BASE Intermediate_Dir "libspeexdsp_dynamic___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Dynamic_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBSPEEX_DYNAMIC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../include" /I "../" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"..\..\bin\libspeexdsp.dll" /implib:"..\..\lib\libspeexdsp.lib"

!ELSEIF  "$(CFG)" == "libspeexdsp_dynamic - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libspeexdsp_dynamic___Win32_Debug"
# PROP BASE Intermediate_Dir "libspeexdsp_dynamic___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Dynamic_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBSPEEX_DYNAMIC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /I "../" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "HAVE_CONFIG_H" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"..\..\bin\libspeexdsp.dll" /implib:"..\..\lib\libspeexdsp.lib" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "libspeexdsp_dynamic - Win32 Release"
# Name "libspeexdsp_dynamic - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\libspeex\buffer.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\fftwrap.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\filterbank.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\jitter.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\kiss_fft.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\kiss_fftr.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\mdf.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\preprocess.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\resample.c
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\smallft.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\libspeex\_kiss_fft_guts.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\arch.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\fftwrap.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\filterbank.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\fixed_debug.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\fixed_generic.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\kiss_fft.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\kiss_fftr.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\math_approx.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\os_support.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\pseudofloat.h
# End Source File
# Begin Source File

SOURCE=..\..\libspeex\smallft.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_buffer.h
# End Source File
# End Group
# Begin Group "Public Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\speex\speex.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_bits.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_echo.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_jitter.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_preprocess.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_resampler.h
# End Source File
# Begin Source File

SOURCE=..\..\include\speex\speex_types.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\config.h
# End Source File
# Begin Source File

SOURCE=..\libspeexdsp.def
# End Source File
# End Target
# End Project
