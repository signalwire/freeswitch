# Microsoft Developer Studio Project File - Name="gsmlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=gsmlib - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "gsmlib.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "gsmlib.mak" CFG="gsmlib - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "gsmlib - Win32 Release" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE "gsmlib - Win32 Debug" (basierend auf  "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gsmlib - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GR /GX /O2 /I "../vcproject" /I ".." /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /FR /YX /FD /TP /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "gsmlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GR /GX /ZI /Od /I ".." /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /YX /FD /GZ /TP /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "gsmlib - Win32 Release"
# Name "gsmlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;cc;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\gsmlib\gsm_at.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_error.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_cb.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_event.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_me_ta.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_nls.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_parser.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_phonebook.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms_codec.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms_store.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_phonebook.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_phonebook_base.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_sms_store.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_util.cc
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_win32_serial.cc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\gsmlib\gsm_at.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_cb.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_error.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_event.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_map_key.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_me_ta.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_nls.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_parser.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_phonebook.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_port.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms_codec.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sms_store.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_phonebook.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_phonebook_base.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sorted_sms_store.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_sysdep.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_util.h
# End Source File
# Begin Source File

SOURCE=..\gsmlib\gsm_win32_serial.h
# End Source File
# End Group
# End Target
# End Project
