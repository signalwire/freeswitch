# Microsoft Developer Studio Project File - Name="libsofia_sip_ua" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libsofia_sip_ua - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libsofia_sip_ua.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libsofia_sip_ua.mak" CFG="libsofia_sip_ua - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libsofia_sip_ua - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libsofia_sip_ua - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libsofia_sip_ua - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /WX /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /D "_USRDLL" /D "LIBSOFIA_SIP_UA_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /WX /GX /O2 /I ".." /I "..\..\libsofia-sip-ua\su" /I "..\..\libsofia-sip-ua\ipt" /I "..\..\libsofia-sip-ua\sresolv" /I "..\..\libsofia-sip-ua\bnf" /I "..\..\libsofia-sip-ua\url" /I "..\..\libsofia-sip-ua\msg" /I "..\..\libsofia-sip-ua\sip" /I "..\..\libsofia-sip-ua\nta" /I "..\..\libsofia-sip-ua\nua" /I "..\..\libsofia-sip-ua\iptsec" /I "..\..\libsofia-sip-ua\http" /I "..\..\libsofia-sip-ua\nth" /I "..\..\libsofia-sip-ua\nea" /I "..\..\libsofia-sip-ua\sdp" /I "..\..\libsofia-sip-ua\soa" /I "..\..\libsofia-sip-ua\stun" /I "..\..\libsofia-sip-ua\tport" /I "..\..\libsofia-sip-ua\features" /I "..\pthread" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBSOFIA_SIP_UA_EXPORTS" /D IN_LIBSOFIA_SIP_UA=1 /D IN_LIBSOFIA_SRES=1 /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "NDEBUG"
# ADD RSC /l 0x40b /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /dll /machine:I386

!ELSEIF  "$(CFG)" == "libsofia_sip_ua - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /WX /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /D "_USRDLL" /D "LIBSOFIA_SIP_UA_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /WX /Gm /GX /ZI /Od /I ".." /I "..\..\libsofia-sip-ua\su" /I "..\..\libsofia-sip-ua\ipt" /I "..\..\libsofia-sip-ua\sresolv" /I "..\..\libsofia-sip-ua\bnf" /I "..\..\libsofia-sip-ua\url" /I "..\..\libsofia-sip-ua\msg" /I "..\..\libsofia-sip-ua\sip" /I "..\..\libsofia-sip-ua\nta" /I "..\..\libsofia-sip-ua\nua" /I "..\..\libsofia-sip-ua\iptsec" /I "..\..\libsofia-sip-ua\http" /I "..\..\libsofia-sip-ua\nth" /I "..\..\libsofia-sip-ua\nea" /I "..\..\libsofia-sip-ua\sdp" /I "..\..\libsofia-sip-ua\soa" /I "..\..\libsofia-sip-ua\stun" /I "..\..\libsofia-sip-ua\tport" /I "..\..\libsofia-sip-ua\features" /I "..\pthread" /I "." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBSOFIA_SIP_UA_EXPORTS" /D IN_LIBSOFIA_SIP_UA=1 /D IN_LIBSOFIA_SRES=1 /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40b /d "_DEBUG"
# ADD RSC /l 0x40b /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib ws2_32.lib advapi32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copy dll to win32 directory
PostBuild_Cmds=copy Debug\libsofia_sip_ua.dll ..
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "libsofia_sip_ua - Win32 Release"
# Name "libsofia_sip_ua - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "su"

# PROP Default_Filter "su*.c"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\inet_ntop.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\inet_pton.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\smoothsort.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\string0.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_addrinfo.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_alloc.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_alloc_lock.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_base_port.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_bm.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_default_log.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_errno.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_global_log.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_localinfo.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_log.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_md5.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_os_nw.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_port.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_pthread_port.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_root.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_socket_port.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_sprintf.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_strdup.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_strlst.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_tag_io.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_taglist.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_time.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_time0.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_timer.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_uniqueid.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_vector.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_wait.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_win32_port.c"
# End Source File
# End Group
# Begin Group "ipt"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\base64.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\rc4.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\token64.c"
# End Source File
# End Group
# Begin Group "url"

# PROP Default_Filter "url*.c"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\url.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\url_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\url_tag_ref.c"
# End Source File
# End Group
# Begin Group "features"

# PROP Default_Filter "features*.c"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\features\features.c"
# End Source File
# End Group
# Begin Group "bnf"

# PROP Default_Filter "bnf*.c"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\bnf\bnf.c"
# End Source File
# End Group
# Begin Group "msg"

# PROP Default_Filter "msg*.c"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_auth.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_basic.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_date.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_generic.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_header_copy.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_header_make.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_mclass.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_mime.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_mime_table.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_parser.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_parser_util.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_tag.c"
# End Source File
# End Group
# Begin Group "clib replacement"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\memcspn.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\memmem.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\memspn.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\strcasestr.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\strtoull.c"
# End Source File
# End Group
# Begin Group "sip"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_basic.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_caller_prefs.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_event.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_extra.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_feature.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_header.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_mime.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_parser.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_parser_table.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_prack.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_pref_util.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_reason.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_refer.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_security.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_session.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_status.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_tag_class.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_tag_ref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_time.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_util.c"
# End Source File
# End Group
# Begin Group "http"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_basic.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_extra.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_header.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_parser.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_parser_table.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_status.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_tag_class.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\http_tag_ref.c"
# End Source File
# End Group
# Begin Group "nth"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\nth_client.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\nth_server.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\nth_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\nth_tag_ref.c"
# End Source File
# End Group
# Begin Group "sresolv"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sres.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sres_blocking.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sres_cache.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sresolv.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sres_sip.c"
# End Source File
# End Group
# Begin Group "nea"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_debug.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_event.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_server.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_tag_ref.c"
# End Source File
# End Group
# Begin Group "iptsec"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_client.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_common.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_digest.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_module.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_module_http.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_module_sip.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_plugin.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_plugin_delayed.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\auth_tag_ref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\iptsec_debug.c"
# End Source File
# End Group
# Begin Group "stun"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_common.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_dns.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_mini.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\stun_tag_ref.c"
# End Source File
# End Group
# Begin Group "nua"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_common.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_dialog.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_dialog.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_event_server.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_extension.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_message.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_notifier.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_options.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_params.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_params.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_publish.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_register.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_registrar.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_session.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_stack.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_stack.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_client.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_client.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_server.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_server.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_subnotref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_tag_ref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\nua_types.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\outbound.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\outbound.h"
# End Source File
# End Group
# Begin Group "nta"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\nta.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\nta_check.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\nta_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\nta_tag_ref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sl_read_payload.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sl_utils_log.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sl_utils_print.c"
# End Source File
# End Group
# Begin Group "tport"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_logging.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_stub_sigcomp.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_stub_stun.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_tag_ref.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_type_connect.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_type_tcp.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_type_udp.c"
# End Source File
# End Group
# Begin Group "sdp"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sdp.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sdp_parse.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sdp_print.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sdp_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sdp_tag_ref.c"
# End Source File
# End Group
# Begin Group "soa"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\soa.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\soa_static.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\soa_tag.c"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\soa_tag_ref.c"
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "su headers"

# PROP Default_Filter "su*.h"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\heap.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\htable.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\htable2.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\rbtree.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_addrinfo.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_alloc.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_alloc_stat.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_bm.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_config.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_debug.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_errno.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_localinfo.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_log.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_md5.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_module_debug.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_os_nw.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\su_port.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_source.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_strlst.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_tag_class.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_tag_inline.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_tag_io.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_tagarg.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_time.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_types.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_uniqueid.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_vector.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\su_wait.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\su\sofia-sip\tstdef.h"
# End Source File
# End Group
# Begin Group "win32 headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\config.h
# End Source File
# Begin Source File

SOURCE="..\sofia-sip\su_configure.h"
# End Source File
# Begin Source File

SOURCE=..\unistd.h
# End Source File
# End Group
# Begin Group "ipt headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\sofia-sip\base64.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\sofia-sip\rc4.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\ipt\sofia-sip\token64.h"
# End Source File
# End Group
# Begin Group "url headers"

# PROP Default_Filter "url*.h"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\sofia-sip\url.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\sofia-sip\url_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\url\sofia-sip\url_tag_class.h"
# End Source File
# End Group
# Begin Group "features headers"

# PROP Default_Filter "features*.h"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\features\sofia-sip\sofia_features.h"
# End Source File
# End Group
# Begin Group "bnf headers"

# PROP Default_Filter "bnf*.h"
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\bnf\sofia-sip\bnf.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\bnf\sofia-sip\hostdomain.h"
# End Source File
# End Group
# Begin Group "msg headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_addr.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_bnf.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_buffer.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_date.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_header.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\msg_internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_mclass.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_mclass_hash.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_mime.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_mime_protos.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_parser.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_protos.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_tag_class.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\msg\sofia-sip\msg_types.h"
# End Source File
# End Group
# Begin Group "sip headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_extensions.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_hclasses.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_header.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sip_internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_parser.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_protos.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_status.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_tag_class.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sip\sofia-sip\sip_util.h"
# End Source File
# End Group
# Begin Group "http headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_hclasses.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_header.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_parser.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_protos.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_status.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\http\sofia-sip\http_tag_class.h"
# End Source File
# End Group
# Begin Group "nth headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\sofia-sip\nth.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nth\sofia-sip\nth_tag.h"
# End Source File
# End Group
# Begin Group "sresolv headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-resolv\sres.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-resolv\sres_async.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-resolv\sres_cache.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-resolv\sres_config.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-resolv\sres_record.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-sip\sresolv.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sresolv\sofia-sip\sres_sip.h"
# End Source File
# End Group
# Begin Group "nea headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\sofia-sip\nea.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\nea_debug.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nea\sofia-sip\nea_tag.h"
# End Source File
# End Group
# Begin Group "iptsec headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_client.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_client_plugin.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_common.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_digest.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_module.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_ntlm.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\sofia-sip\auth_plugin.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\iptsec\iptsec_debug.h"
# End Source File
# End Group
# Begin Group "stun headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\sofia-sip\stun.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\sofia-sip\stun_common.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\stun\sofia-sip\stun_tag.h"
# End Source File
# End Group
# Begin Group "nua headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\sofia-sip\nua.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nua\sofia-sip\nua_tag.h"
# End Source File
# End Group
# Begin Group "nta headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sofia-sip\nta.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\nta_internal.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sofia-sip\nta_stateless.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sofia-sip\nta_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sofia-sip\nta_tport.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\nta\sofia-sip\sl_utils.h"
# End Source File
# End Group
# Begin Group "tport headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\sofia-sip\tport.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\sofia-sip\tport_plugins.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\sofia-sip\tport_tag.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\tport\tport_tls.h"
# End Source File
# End Group
# Begin Group "sdp headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sofia-sip\sdp.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\sdp\sofia-sip\sdp_tag.h"
# End Source File
# End Group
# Begin Group "soa headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\sofia-sip\soa.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\sofia-sip\soa_add.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\sofia-sip\soa_session.h"
# End Source File
# Begin Source File

SOURCE="..\..\libsofia-sip-ua\soa\sofia-sip\soa_tag.h"
# End Source File
# End Group
# End Group
# Begin Source File

SOURCE=..\pthread\pthreadVC2.lib
# End Source File
# End Target
# End Project
