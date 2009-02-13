# Microsoft Developer Studio Project File - Name="libspeexdsp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libspeexdsp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libspeexdsp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libspeexdsp.mak" CFG="libspeexdsp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libspeexdsp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libspeexdsp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libspeexdsp - Win32 Release"

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
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W1 /GX- /O2 /I "../../include" /I "../" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libspeexdsp.lib"

!ELSEIF  "$(CFG)" == "libspeexdsp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libspeexdsp___Win32_Debug"
# PROP BASE Intermediate_Dir "libspeexdsp___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "libspeexdsp___Win32_Debug"
# PROP Intermediate_Dir "libspeexdsp___Win32_Debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ  /c
# ADD CPP /nologo /MDd /W3 /Gm- /GX- /Zi /Od /I "../../include" /I "../" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FD /GZ  /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libspeexdsp.lib"

!ENDIF 

# Begin Target

# Name "libspeexdsp - Win32 Release"
# Name "libspeexdsp - Win32 Debug"
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

SOURCE=..\..\include\speex\speex_buffer.h
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
# End Target
# End Project
