# Microsoft Developer Studio Project File - Name="spandsp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=spandsp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "spandsp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "spandsp.mak" CFG="spandsp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "spandsp - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "spandsp - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "spandsp - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D HAVE_TGMATH_H /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O2 /I "." /I "..\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D HAVE_TGMATH_H /D "_WINDLL" /FR /FD /c
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
# ADD LINK32 kernel32.lib ws2_32.lib winmm.lib /nologo /dll /map /debug /machine:I386 /out:"Release/libspandsp.dll"

!ELSEIF  "$(CFG)" == "spandsp - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D HAVE_TGMATH_H /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "." /I "..\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D HAVE_TGMATH_H /FR /FD /GZ /c
# SUBTRACT CPP /WX /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib ws2_32.lib winmm.lib /nologo /dll /incremental:no /map /debug /machine:I386 /out:"Debug/libspandsp.dll" /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "spandsp - Win32 Release"
# Name "spandsp - Win32 Debug"
# Begin Group "Source Files"
# Begin Source File

SOURCE=.\adsi.c
# End Source File
# Begin Source File

SOURCE=.\async.c
# End Source File
# Begin Source File

SOURCE=.\at_interpreter.c
# End Source File
# Begin Source File

SOURCE=.\awgn.c
# End Source File
# Begin Source File

SOURCE=.\bell_r2_mf.c
# End Source File
# Begin Source File

SOURCE=.\bert.c
# End Source File
# Begin Source File

SOURCE=.\bit_operations.c
# End Source File
# Begin Source File

SOURCE=.\bitstream.c
# End Source File
# Begin Source File

SOURCE=.\complex_filters.c
# End Source File
# Begin Source File

SOURCE=.\complex_vector_float.c
# End Source File
# Begin Source File

SOURCE=.\complex_vector_int.c
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\dds_float.c
# End Source File
# Begin Source File

SOURCE=.\dds_int.c
# End Source File
# Begin Source File

SOURCE=.\dtmf.c
# End Source File
# Begin Source File

SOURCE=.\echo.c
# End Source File
# Begin Source File

SOURCE=.\fax.c
# End Source File
# Begin Source File

SOURCE=.\fax_modems.c
# End Source File
# Begin Source File

SOURCE=.\fsk.c
# End Source File
# Begin Source File

SOURCE=.\g711.c
# End Source File
# Begin Source File

SOURCE=.\g722.c
# End Source File
# Begin Source File

SOURCE=.\g726.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_decode.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_encode.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_long_term.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_lpc.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_preprocess.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_rpe.c
# End Source File
# Begin Source File

SOURCE=.\gsm0610_short_term.c
# End Source File
# Begin Source File

SOURCE=.\hdlc.c
# End Source File
# Begin Source File

SOURCE=.\ima_adpcm.c
# End Source File
# Begin Source File

SOURCE=.\logging.c
# End Source File
# Begin Source File

SOURCE=.\lpc10_analyse.c
# End Source File
# Begin Source File

SOURCE=.\lpc10_decode.c
# End Source File
# Begin Source File

SOURCE=.\lpc10_encode.c
# End Source File
# Begin Source File

SOURCE=.\lpc10_placev.c
# End Source File
# Begin Source File

SOURCE=.\lpc10_voicing.c
# End Source File
# Begin Source File

SOURCE=.\modem_echo.c
# End Source File
# Begin Source File

SOURCE=.\modem_connect_tones.c
# End Source File
# Begin Source File

SOURCE=.\noise.c
# End Source File
# Begin Source File

SOURCE=.\oki_adpcm.c
# End Source File
# Begin Source File

SOURCE=.\playout.c
# End Source File
# Begin Source File

SOURCE=.\plc.c
# End Source File
# Begin Source File

SOURCE=.\power_meter.c
# End Source File
# Begin Source File

SOURCE=.\queue.c
# End Source File
# Begin Source File

SOURCE=.\schedule.c
# End Source File
# Begin Source File

SOURCE=.\sig_tone.c
# End Source File
# Begin Source File

SOURCE=.\silence_gen.c
# End Source File
# Begin Source File

SOURCE=.\super_tone_rx.c
# End Source File
# Begin Source File

SOURCE=.\super_tone_tx.c
# End Source File
# Begin Source File

SOURCE=.\swept_tone.c
# End Source File
# Begin Source File

SOURCE=.\t4_rx.c
# End Source File
# Begin Source File

SOURCE=.\t4_tx.c
# End Source File
# Begin Source File

SOURCE=.\t30.c
# End Source File
# Begin Source File

SOURCE=.\t30_api.c
# End Source File
# Begin Source File

SOURCE=.\t30_logging.c
# End Source File
# Begin Source File

SOURCE=.\t31.c
# End Source File
# Begin Source File

SOURCE=.\t35.c
# End Source File
# Begin Source File

SOURCE=.\t38_core.c
# End Source File
# Begin Source File

SOURCE=.\t38_gateway.c
# End Source File
# Begin Source File

SOURCE=.\t38_non_ecm_buffer.c
# End Source File
# Begin Source File

SOURCE=.\t38_terminal.c
# End Source File
# Begin Source File

SOURCE=.\testcpuid.c
# End Source File
# Begin Source File

SOURCE=.\time_scale.c
# End Source File
# Begin Source File

SOURCE=.\tone_detect.c
# End Source File
# Begin Source File

SOURCE=.\tone_generate.c
# End Source File
# Begin Source File

SOURCE=.\v17rx.c
# End Source File
# Begin Source File

SOURCE=.\v17tx.c
# End Source File
# Begin Source File

SOURCE=.\v18.c
# End Source File
# Begin Source File

SOURCE=.\v22bis_rx.c
# End Source File
# Begin Source File

SOURCE=.\v22bis_tx.c
# End Source File
# Begin Source File

SOURCE=.\v27ter_rx.c
# End Source File
# Begin Source File

SOURCE=.\v27ter_tx.c
# End Source File
# Begin Source File

SOURCE=.\v29rx.c
# End Source File
# Begin Source File

SOURCE=.\v29tx.c
# End Source File
# Begin Source File

SOURCE=.\v42.c
# End Source File
# Begin Source File

SOURCE=.\v42bis.c
# End Source File
# Begin Source File

SOURCE=.\v8.c
# End Source File
# Begin Source File

SOURCE=.\vector_float.c
# End Source File
# Begin Source File

SOURCE=.\vector_int.c
# End Source File
# Begin Source File

SOURCE=.\.\msvc\gettimeofday.c
# End Source File
# End Group
# Begin Group "Header Files"
# Begin Source File

SOURCE=.\spandsp/adsi.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/async.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/arctan2.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/at_interpreter.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/awgn.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/bell_r2_mf.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/bert.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/biquad.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/bit_operations.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/bitstream.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/crc.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/complex.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/complex_filters.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/complex_vector_float.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/complex_vector_int.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/dc_restore.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/dds.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/dtmf.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/echo.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/fast_convert.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/fax.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/fax_modems.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/fir.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/fsk.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/g168models.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/g711.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/g722.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/g726.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/gsm0610.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/hdlc.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/ima_adpcm.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/logging.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/lpc10.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/modem_echo.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/modem_connect_tones.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/noise.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/oki_adpcm.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/playout.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/plc.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/power_meter.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/queue.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/saturated.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/schedule.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/sig_tone.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/silence_gen.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/super_tone_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/super_tone_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/swept_tone.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t4_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t4_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t30.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t30_api.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t30_fcf.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t30_logging.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t31.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t35.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t38_core.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t38_gateway.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t38_non_ecm_buffer.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/t38_terminal.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/telephony.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/time_scale.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/timing.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/tone_detect.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/tone_generate.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v17rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v17tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v18.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v22bis.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v27ter_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v27ter_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v29rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v29tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v42.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v42bis.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/v8.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/vector_float.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/vector_int.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/version.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/adsi.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/async.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/at_interpreter.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/awgn.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/bell_r2_mf.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/bert.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/bitstream.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/dtmf.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/echo.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/fax.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/fax_modems.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/fsk.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/g711.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/g722.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/g726.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/gsm0610.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/hdlc.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/ima_adpcm.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/logging.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/lpc10.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/modem_connect_tones.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/modem_echo.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/noise.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/oki_adpcm.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/queue.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/schedule.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/sig_tone.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/silence_gen.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/super_tone_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/super_tone_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/swept_tone.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t30.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t30_dis_dtc_dcs_bits.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t31.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t38_core.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t38_gateway.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t38_non_ecm_buffer.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t38_terminal.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t4_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/t4_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/time_scale.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/tone_detect.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/tone_generate.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v17rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v17tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v18.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v22bis.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v27ter_rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v27ter_tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v29rx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v29tx.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v42.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v42bis.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/private/v8.h
# End Source File
# Begin Source File

SOURCE=.\spandsp/expose.h
# End Source File
# Begin Source File

SOURCE=.\spandsp.h
# End Source File
# End Group

# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
