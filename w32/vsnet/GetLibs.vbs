'On Error Resume Next
' **************
' Initialization
' **************

Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")
Set WshSysEnv = WshShell.Environment("SYSTEM")
Set xml = CreateObject("Microsoft.XMLHTTP")
Set oStream = CreateObject("Adodb.Stream")
Set objArgs = WScript.Arguments
Dim vcver, DevEnv, VCBuild
BuildRelease=False
BuildDebug=False
BuildCore=False
BuildVersion=False
BuildModExosip=False
BuildModIaxChan=False
BuildModPortAudio=False
BuildModSpeexCodec=False
BuildModCodecG729=False
BuildModCodecGSM=False
BuildModXMPPEvent=False
BuildModsndfile=False
BuildModrawaudio=False
BuildSpiderMonkey=False
quote=Chr(34)
ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))

ToolsBase="http://www.freeswitch.org/downloads/win32/"
LibsBase="http://www.freeswitch.org/downloads/libs/"
LibDestDir=Showpath(ScriptDir & "..\..\libs")
FreeswitchDir=Showpath(ScriptDir & "..\..")
UtilsDir=Showpath(ScriptDir & "Tools")
If objArgs(0) <> "Version" Then
	GetTarGZObjects UtilsDir
	GetVCBuild
	Wscript.echo "Detected VCBuild: " & VCBuild
End If

' **************
' Option Parsing
' **************

If objArgs.Count >=2 Then
	Select Case objArgs(1)
		Case "Release"		
			BuildRelease=True
		Case "Debug"   
			BuildDebug=True
		Case "All"   
			BuildRelease=True
			BuildDebug=True
	End Select
End If

If objArgs.Count >=1 Then
	Select Case objArgs(0)
		Case "Core"		
			BuildCore=True
		Case "Version"		
			BuildVersion=True
		Case "Mod_Exosip"   
			BuildModExosip=True
		Case "Mod_IaxChan"   
			BuildModIaxChan=True
		Case "Mod_PortAudio"
			BuildModPortAudio=True		
		Case "Mod_SpeexCodec"
			BuildModSpeexCodec=True
		Case "Mod_CodecG729"
			BuildModCodecG729=True
		Case "Mod_CodecGSM"
			BuildModCodecGSM=True
		Case "Mod_XMPPEvent"
			BuildModXMPPEvent=True
		Case "Mod_sndfile"
			BuildModsndfile=True
		Case "Mod_rawaudio"
			BuildModrawaudio=True
		Case Else
			BuildCore=True
			BuildModExosip=True
			BuildModIaxChan=True
			BuildModPortAudio=True		
			BuildModSpeexCodec=True
			BuildModCodecG729=True
			BuildModXMPPEvent=True
			BuildModsndfile=True
			BuildModrawaudio=True
			BuildVersion=True
	End Select
Else
	BuildCore=True
	BuildModExosip=True
	BuildModIaxChan=True
	BuildModPortAudio=True		
	BuildModSpeexCodec=True
	BuildModCodecG729=True
	BuildModXMPPEvent=True
	BuildModsndfile=True
	BuildModrawaudio=True
	BuildVersion=True
End If

' ******************
' Process lib builds
' ******************

If BuildVersion Then
	CreateSwitchVersion
End If

If BuildCore Then
	CreateSwitchVersion

	If Not FSO.FolderExists(LibDestDir & "include") Then
		FSO.CreateFolder(LibDestDir & "include")
	End If
	BuildLibs_apr BuildDebug, BuildRelease
	FSO.CopyFile LibDestDir & "apr\include\*.h", LibDestDir & "include"
	BuildLibs_apriconv BuildDebug, BuildRelease
	FSO.CopyFile LibDestDir & "apr-iconv\include\*.h", LibDestDir & "include"
	BuildLibs_aprutil BuildDebug, BuildRelease
	FSO.CopyFile LibDestDir & "apr-util\include\*.h", LibDestDir & "include"
	BuildLibs_libresample BuildDebug, BuildRelease
	FSO.CopyFile LibDestDir & "libresample\include\*.h", LibDestDir & "include"
	BuildLibs_sqlite BuildDebug, BuildRelease	
	FSO.CopyFile LibDestDir & "sqlite\*.h", LibDestDir & "include"
End If

If BuildModExosip Then
	BuildLibs_libosip2 BuildDebug, BuildRelease
	BuildLibs_exosip BuildDebug, BuildRelease
	BuildLibs_jrtplib BuildDebug, BuildRelease
End If

If BuildModIaxChan Then
	BuildLibs_libiax2 BuildDebug, BuildRelease
End If

If BuildModPortAudio Then
	BuildLibs_portaudio BuildDebug, BuildRelease
End If

If BuildModSpeexCodec Then
	BuildLibs_SpeexCodec BuildDebug, BuildRelease
End If

If BuildModCodecG729 Then
	BuildLibs_libg729 BuildDebug, BuildRelease
End If

If BuildModCodecGSM Then
	BuildLibs_libgsm BuildDebug, BuildRelease
End If

If BuildModXMPPEvent Then
	BuildLibs_iksemel BuildDebug, BuildRelease
End If

If BuildModsndfile Then
	BuildLibs_libsndfile BuildDebug, BuildRelease
End If

If BuildModrawaudio Then
	BuildLibs_libresample BuildDebug, BuildRelease
End If

If BuildSpiderMonkey Then
	BuildLibs_SpiderMonkey BuildDebug, BuildRelease
End If

WScript.Echo "Complete"

'  ******************
'  Lib Build Sectiton
'  ******************
Sub BuildLibs_aprutil(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr-util") Then 
		WgetUnTarGz "ftp://ftp.wayne.edu/apache/apr/apr-util-1.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-util-1.2.2") Then
			Wscript.echo "Unable to get apr-util from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "apr-util-1.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-util-1.2.2", "apr-util"
		FSO.CopyFile Utilsdir & "apr\xml.vcproj", LibDestDir & "apr-util\xml\expat\lib\", True
		FSO.CopyFile Utilsdir & "apr\gen_uri_delims.vcproj", LibDestDir & "apr-util\uri\", True
		FSO.CopyFile Utilsdir & "apr\aprutil.vcproj", LibDestDir & "apr-util\", True
	End If 
	If FSO.FolderExists(LibDestDir & "apr-util") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr-util\uri\uri_delims.h") Then 
				BuildViaVCBuild LibDestDir & "apr-util\uri\gen_uri_delims.vcproj", "Debug"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\xml\expat\lib\LibD\xml.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\xml\expat\lib\xml.vcproj", "Debug"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\LibD\aprutil-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\aprutil.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr-util\uri\uri_delims.h") Then 
				BuildViaVCBuild LibDestDir & "apr-util\uri\gen_uri_delims.vcproj", "Release"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\xml\expat\lib\LibR\xml.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\xml\expat\lib\xml.vcproj", "Release"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\LibR\aprutil-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\aprutil.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download apr-util"
	End If 	
End Sub

Sub BuildLibs_apriconv(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr-iconv") Then 
		WgetUnTarGz "ftp://ftp.wayne.edu/apache/apr/apr-iconv-1.1.1.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-iconv-1.1.1") Then
			Wscript.echo "Unable to get apr-iconv from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "apr-iconv-1.1.1.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-iconv-1.1.1", "apr-iconv"
		FSO.CopyFile Utilsdir & "apr\apriconv.vcproj", LibDestDir & "apr-iconv\", True
	End If 
	If FSO.FolderExists(LibDestDir & "apr-iconv") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr-iconv\LibD\apriconv-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-iconv\apriconv.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr-iconv\LibR\apriconv-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-iconv\apriconv.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download apr-iconv"
	End If 
End Sub

Sub BuildLibs_apr(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr") Then 
		WgetUnTarGz "ftp://ftp.wayne.edu/apache/apr/apr-1.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-1.2.2") Then
			Wscript.echo "Unable to get apr from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "apr-1.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-1.2.2", "apr"
		FSO.CopyFile Utilsdir & "apr\apr.vcproj", LibDestDir & "apr\", True
		FindReplaceInFile LibDestDir & "apr\file_io\unix\fullrw.c", "int i;", "unsigned int i;"
	End If 
	If FSO.FolderExists(LibDestDir & "apr") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr\LibD\apr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\apr.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr\LibR\apr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\apr.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download APR"
	End If 
End Sub

Sub BuildLibs_exosip(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libeXosip2") Then 
		WgetUnTarGz "http://www.antisip.com/download/libeXosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libeXosip2-2.2.2") Then
			Wscript.echo "Unable to get eXosip from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "libeXosip2-2.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "libeXosip2-2.2.2", "libeXosip2"
		FindReplaceInFile LibDestDir & "libeXosip2\platform\vsnet\eXosip.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	If FSO.FolderExists(LibDestDir & "libeXosip2") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libeXosip2\platform\vsnet\Debug\exosip.lib") Then 
				BuildViaVCBuild LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libeXosip2\platform\vsnet\Release\exosip.lib") Then 
				BuildViaVCBuild LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download exosip"
	End If 
End Sub

Sub BuildLibs_libosip2(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "osip") Then
		WgetUnTarGz "http://www.antisip.com/download/libosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libosip2-2.2.2") Then
			Wscript.echo "Unable to get osip from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "libosip2-2.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "libosip2-2.2.2", "osip"
		FindReplaceInFile LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
		FindReplaceInFile LibDestDir & "osip\platform\vsnet\osip2.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	If FSO.FolderExists(LibDestDir & "osip") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "osip\platform\vsnet\Debug\osip2.lib") Then 
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osip2.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "osip\platform\vsnet\Release\osip2.lib") Then 
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osip2.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download Osip"
	End If 
End Sub

Sub BuildLibs_jrtplib(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then 
		WgetUnTarGz "http://research.edm.luc.ac.be/jori/jthread/jthread-1.1.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then
			Wscript.echo "Unable to get JThread from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "jthread-1.1.2.tar.gz", LibDestDir
		End If
		FindReplaceInFile LibDestDir & "jthread-1.1.2\jthread.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	
	If Not FSO.FolderExists(LibDestDir & "jrtplib") Then 
		WgetUnTarGz "http://research.edm.luc.ac.be/jori/jrtplib/jrtplib-3.3.0.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "jrtplib-3.3.0") Then
			Wscript.echo "Unable to get JRTPLib from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "jrtplib-3.3.0.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "jrtplib-3.3.0", "jrtplib"
		FindReplaceInFile LibDestDir & "jrtplib\jrtplib.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
		FindReplaceInFile LibDestDir & "jrtplib\jrtplib.vcproj", "WarningLevel=" & quote & "3" & quote, "WarningLevel=" & quote & "0" & quote
	End If

	If FSO.FolderExists(LibDestDir & "jrtplib") And FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then 
		If BuildDebug Then
			If (Not FSO.FileExists(LibDestDir & "jrtplib\Debug\jrtplib.lib")) Or (Not FSO.FileExists(LibDestDir & "jthread-1.1.2\Debug\jthread.lib")) Then 
				BuildViaVCBuild LibDestDir & "jthread-1.1.2\jthread.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "jrtplib\jrtplib.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If (Not FSO.FileExists(LibDestDir & "jrtplib\Release\jrtplib.lib")) Or (Not FSO.FileExists(LibDestDir & "jthread-1.1.2\Release\jthread.lib")) Then 
				BuildViaVCBuild LibDestDir & "jthread-1.1.2\jthread.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "jrtplib\jrtplib.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download JRtplib"
	End If 
End Sub

Sub BuildLibs_sqlite(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "sqlite") Then 
		WgetUnZip "http://www.sqlite.org/sqlite-source-3_2_7.zip", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "sqlite-source-3_2_7") Then
			Wscript.echo "Unable to get SQLite from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "sqlite-source-3_2_7.zip", LibDestDir
		End If
		RenameFolder LibDestDir & "sqlite-source-3_2_7", "sqlite"
		FSO.CopyFile Utilsdir & "sqlite.vcproj", LibDestDir & "sqlite\", True
		FindReplaceInFile LibDestDir & "sqlite\sqlite.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	If FSO.FolderExists(LibDestDir & "sqlite") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "sqlite\Debug\sqlite.lib") Then 
				BuildViaVCBuild LibDestDir & "sqlite\sqlite.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "sqlite\Release\sqlite.lib") Then 
				BuildViaVCBuild LibDestDir & "sqlite\sqlite.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download SQLite"
	End If 
End Sub

Sub BuildLibs_iksemel(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "iksemel") Then 
		WgetUnTarGz "http://jabberstudio.2nw.net/iksemel/iksemel-1.2.tar.gz", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "iksemel-1.2") Then
			Wscript.echo "Unable to get iksemel from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "iksemel-1.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "iksemel-1.2", "iksemel"
		FSO.CopyFile Utilsdir & "iksemel\iksemel.vcproj", LibDestDir & "iksemel\", True
		FSO.CopyFile Utilsdir & "iksemel\config.h", LibDestDir & "iksemel\include\", True
	End If
	If FSO.FolderExists(LibDestDir & "iksemel") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "iksemel\Debug\iksemel.lib") Then 
				BuildViaVCBuild LibDestDir & "iksemel\iksemel.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "iksemel\Release\iksemel.lib") Then 
				BuildViaVCBuild LibDestDir & "iksemel\iksemel.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download iksemel"
	End If 
End Sub

Sub BuildLibs_libiax2(BuildDebug, BuildRelease)
	If FSO.FolderExists(LibDestDir & "iax") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "iax\Debug\libiax2.lib") Then 
				BuildViaVCBuild LibDestDir & "iax\libiax2.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "iax\Release\libiax2.lib") Then 
				BuildViaVCBuild LibDestDir & "iax\libiax2.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download libIAX2"
	End If 
End Sub

Sub BuildLibs_portaudio(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "PortAudio") Then 
		WgetUnZip LibsBase & "portaudio_v18_1.zip", LibDestDir
		RenameFolder LibDestDir & "portaudio_v18_1", "PortAudio"
	End If 
	If FSO.FolderExists(LibDestDir & "PortAudio") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "PortAudio\winvc\Lib\PAStaticWMMED.lib") Then 
				BuildViaVCBuild LibDestDir & "PortAudio\winvc\PAStaticWMME\PAStaticWMME.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "PortAudio\winvc\Lib\PAStaticWMME.lib") Then 
				BuildViaVCBuild LibDestDir & "PortAudio\winvc\PAStaticWMME\PAStaticWMME.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download PortAudio"
	End If
End Sub

Sub BuildLibs_libg729(BuildDebug, BuildRelease)
	If FSO.FolderExists(LibDestDir & "codec\libg729") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "codec\libg729\Debug\libg729.lib") Then 
				BuildViaVCBuild LibDestDir & "codec\libg729\libg729.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "codec\libg729\Release\libg729.lib") Then 
				BuildViaVCBuild LibDestDir & "codec\libg729\libg729.vcproj", "Release"
			End If
		End If
	End If 
End Sub

Sub BuildLibs_libgsm(BuildDebug, BuildRelease)
	If FSO.FolderExists(LibDestDir & "codec\gsm") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "codec\gsm\Debug\libgsm.lib") Then 
				BuildViaVCBuild LibDestDir & "codec\gsm\libgsm.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "codec\gsm\Release\libgsm.lib") Then 
				BuildViaVCBuild LibDestDir & "codec\gsm\libgsm.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download libgsm"
	End If 
End Sub

Sub BuildLibs_SpeexCodec(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "speex") Then 
		WgetUnTarGz "http://downloads.us.xiph.org/releases/speex/speex-1.1.11.1.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "speex-1.1.11.1") Then
			Wscript.echo "Unable to get libspeex from default download location, Trying backup location:"
			WgetUnTarGz LibsBase & "speex-1.1.11.1.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "speex-1.1.11.1", "speex"
		FSO.CopyFile Utilsdir & "libspeex.vcproj", LibDestDir & "speex\win32\libspeex\", True
	End If 
	If FSO.FolderExists(LibDestDir & "speex") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "speex\win32\libspeex\Debug\libspeex.lib") Then 
				BuildViaVCBuild LibDestDir & "speex\win32\libspeex\libspeex.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "speex\win32\libspeex\Release\libspeex.lib") Then 
				BuildViaVCBuild LibDestDir & "speex\win32\libspeex\libspeex.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download libspeex"
	End If 
End Sub

Sub BuildLibs_libsndfile(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libsndfile") Then 
		WgetUnTarGz LibsBase & "libsndfile-1.0.12.tar.gz", LibDestDir
		RenameFolder LibDestDir & "libsndfile-1.0.12", "libsndfile"
		FSO.CopyFile Utilsdir & "libsndfile.vcproj", LibDestDir & "libsndfile\Win32\", True
	End If 
	If FSO.FolderExists(LibDestDir & "libsndfile") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libsndfile\Win32\Debug\libsndfile.lib") Then 
				BuildViaVCBuild LibDestDir & "libsndfile\Win32\libsndfile.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libsndfile\Win32\Release\libsndfile.lib") Then 
				BuildViaVCBuild LibDestDir & "libsndfile\Win32\libsndfile.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download libsndfile"
	End If 
End Sub

Sub BuildLibs_libresample(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libresample") Then 
		WgetUnZip LibsBase & "libresample-0.1.3.zip", LibDestDir
		RenameFolder LibDestDir & "libresample-0.1.3", "libresample"
	End If 
	If FSO.FolderExists(LibDestDir & "libresample") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libresample\win\libresampled.lib") Then 
				BuildViaVCBuild LibDestDir & "libresample\win\libresample.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libresample\win\libresample.lib") Then 
				BuildViaVCBuild LibDestDir & "libresample\win\libresample.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download libresample"
	End If 
End Sub

Sub BuildLibs_SpiderMonkey(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "js") Then 
		WgetUnZip LibsBase & "js20051231.zip", LibDestDir
		RenameFolder LibDestDir & "js20051231", "js"
		WgetUnZip LibsBase & "nspr-4.6.1.winnt5.debug.zip", LibDestDir & "js"
		WgetUnZip LibsBase & "nspr-4.6.1.winnt5.release.zip", LibDestDir & "js"
		FSO.CreateFolder LibDestDir & "js\nspr\"
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.debug\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
	End If 
	If FSO.FolderExists(LibDestDir & "js") Then 
		If BuildDebug Then
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.debug\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
			If Not FSO.FileExists(LibDestDir & "js\src\Debug\js32.dll") Then 
				BuildViaVCBuild LibDestDir & "js\src\fdlibm\fdlibm.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "js\src\js.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.release\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
			If Not FSO.FileExists(LibDestDir & "js\src\Release\js32.dll") Then 
				BuildViaVCBuild LibDestDir & "js\src\fdlibm\fdlibm.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "js\src\js.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download spidermonkey"
	End If 
End Sub

' *******************
' Utility Subroutines
' *******************

Sub CreateSwitchVersion()
	Dim sLastFile
	Const OverwriteIfExist = -1 
	Const ForReading       =  1 
 	If Not FSO.FolderExists(UtilsDir & "svnversion") Then
		FSO.CreateFolder(UtilsDir & "svnversion")
	End If
	VersionCmd="svnversion " & quote & FreeswitchDir & "." & quote &  " -n"
	Set MyFile = fso.CreateTextFile(UtilsDir & "svnversion\tmpVersion.Bat", True)
	MyFile.WriteLine("@" & "cd " & UtilsDir & "svnversion")
	MyFile.WriteLine("@" & VersionCmd)
	MyFile.Close
	Set oExec = WshShell.Exec(UtilsDir & "svnversion\tmpVersion.Bat")
	Do
		strFromProc = OExec.StdOut.ReadLine()
		VERSION=strFromProc
	Loop While Not OExec.StdOut.atEndOfStream
	If VERSION = "" Then
		WgetUnZip ToolsBase & "svnversion.zip", UtilsDir 
		Set oExec = WshShell.Exec(UtilsDir & "svnversion\tmpVersion.Bat")
		Do
			strFromProc = OExec.StdOut.ReadLine()
			VERSION=strFromProc
		Loop While Not OExec.StdOut.atEndOfStream
	End If
	sLastVersion = ""
	Set sLastFile = FSO.OpenTextFile(UtilsDir & "lastversion", ForReading, true, OpenAsASCII)
	If Not sLastFile.atEndOfStream Then
		sLastVersion = sLastFile.ReadLine()
	End If
	sLastFile.Close
	
	If VERSION <> sLastVersion Then
		Set MyFile = fso.CreateTextFile(UtilsDir & "lastversion", True)
		MyFile.WriteLine(VERSION)
		MyFile.Close
	
		FSO.CopyFile FreeswitchDir & "src\include\switch_version.h.in", FreeswitchDir & "src\include\switch_version.h", true
		FindReplaceInFile FreeswitchDir & "src\include\switch_version.h", "@SVN_VERSION@", VERSION
	End If
End Sub

Sub UpgradeViaDevEnv(ProjectFile)
	Set oExec = WshShell.Exec(quote & DevEnv & quote & " " & quote & ProjectFile & quote & " /Upgrade ")
	Do While oExec.Status <> 1
	WScript.Sleep 100
	Loop
End Sub

Sub BuildViaDevEnv(ProjectFile, BuildType)
	Wscript.echo "Building : " & ProjectFile & " Config type: " & BuildType
	BuildCmd=quote & DevEnv & quote & " " & quote & ProjectFile & quote & " /Build " & BuildType
	Set MyFile = fso.CreateTextFile(UtilsDir & "tmpBuild.Bat", True)
	MyFile.WriteLine("@" & BuildCmd)
	MyFile.Close

	Set oExec = WshShell.Exec(UtilsDir & "tmpBuild.Bat")
	Do
		strFromProc = OExec.StdOut.ReadLine()
		WScript.Echo  strFromProc
	Loop While Not OExec.StdOut.atEndOfStream
End Sub

Sub BuildViaVCBuild(ProjectFile, BuildType)
	Wscript.echo "Building : " & ProjectFile & " Config type: " & BuildType
	BuildCmd=quote & VCBuild & quote & " /nologo /nocolor " & quote & ProjectFile & quote & " " & BuildType
	Set MyFile = fso.CreateTextFile(UtilsDir & "tmpBuild.Bat", True)
	MyFile.WriteLine("@" & BuildCmd)
	MyFile.Close

	Set oExec = WshShell.Exec(UtilsDir & "tmpBuild.Bat")
	Do
		strFromProc = OExec.StdOut.ReadLine()
		WScript.Echo  strFromProc
	Loop While Not OExec.StdOut.atEndOfStream
End Sub

Sub GetVCBuild()
	If WshSysEnv("VS80COMNTOOLS")<> "" Then 
		vcver = "8"
		VCBuild=Showpath(WshSysEnv("VS80COMNTOOLS")&"..\..\VC\vcpackages\") & "vcbuild.exe"
	Else If WshSysEnv("VS71COMNTOOLS")<> "" Then
		vcver = "7"
		VCBuild=Showpath(WshSysEnv("VS71COMNTOOLS")&"..\..\VC\vcpackages\") & "vcbuild.exe"
	Else
		Wscript.Echo("Did not find any Visual Studio .net 2003 or 2005 on your machine")
		WScript.Quit(1)
	End If
	End If
End Sub

Sub GetDevEnv()
	If WshSysEnv("VS80COMNTOOLS")<> "" Then 
		vcver = "8"
		DevEnv=Showpath(WshSysEnv("VS80COMNTOOLS")&"..\IDE\") & "devenv"
	Else If WshSysEnv("VS71COMNTOOLS")<> "" Then
		vcver = "7"
		DevEnv=Showpath(WshSysEnv("VS71COMNTOOLS")&"..\IDE\") & "devenv"
	Else
		Wscript.Echo("Did not find any Visual Studio .net 2003 or 2005 on your machine")
		WScript.Quit(1)
	End If
	End If
End Sub


Sub RenameFolder(FolderName, NewFolderName)
	Set Folder=FSO.GetFolder(FolderName)
	Folder.Name = NewFolderName
End Sub

Sub Upgrade(OldFileName, NewFileName)
	If WshSysEnv("VS80COMNTOOLS")<> "" Then 
		Wscript.echo "8.0"
		Set vcProj = CreateObject("VisualStudio.VCProjectEngine.8.0")
		
	Else If WshSysEnv("VS71COMNTOOLS")<> "" Then
		Wscript.echo "7.1"
		Set vcProj = CreateObject("VisualStudio.VCProjectEngine.7.1")
	Else
		Wscript.Echo("Did not find any Visual Studio .net 2003 or 2005 on your machine")
		WScript.Quit(1)
	End If
	End If
		
	Set vcProject = vcProj.LoadProject(OldFileName)
	If Not FSO.FileExists(vcProject.ProjectFile) Then
		'   // specify name and location of new project file
		vcProject.ProjectFile = NewFileName
	'   // call the project engine to save this off. 
	'   // when no name is shown, it will create one with the .vcproj name
	vcProject.Save()
	End If
'	WScript.Echo("New Project Name: "+vcProject.ProjectFile+"")
'On Error GoTo 0
End Sub

Sub Unix2dos(FileName)
	Const OpenAsASCII = 0  ' Opens the file as ASCII (TristateFalse) 
	Const OpenAsUnicode = -1  ' Opens the file as Unicode (TristateTrue) 
	Const OpenAsDefault = -2  ' Opens the file using the system default 
	
	Const OverwriteIfExist = -1 
	Const FailIfNotExist   =  0 
	Const ForReading       =  1 
	
	Set fOrgFile = FSO.OpenTextFile(FileName, ForReading, FailIfNotExist, OpenAsASCII)
	sText = fOrgFile.ReadAll
	fOrgFile.Close
	sText = Replace(sText, vbLf, vbCrLf)
	Set fNewFile = FSO.CreateTextFile(FileName, OverwriteIfExist, OpenAsASCII)
	fNewFile.WriteLine sText
	fNewFile.Close
End Sub

Sub FindReplaceInFile(FileName, sFind, sReplace)
	Const OpenAsASCII = 0  ' Opens the file as ASCII (TristateFalse) 
	Const OpenAsUnicode = -1  ' Opens the file as Unicode (TristateTrue) 
	Const OpenAsDefault = -2  ' Opens the file using the system default 
	
	Const OverwriteIfExist = -1 
	Const FailIfNotExist   =  0 
	Const ForReading       =  1 
	
	Set fOrgFile = FSO.OpenTextFile(FileName, ForReading, FailIfNotExist, OpenAsASCII)
	sText = fOrgFile.ReadAll
	fOrgFile.Close
	sText = Replace(sText, sFind, sReplace)
	Set fNewFile = FSO.CreateTextFile(FileName, OverwriteIfExist, OpenAsASCII)
	fNewFile.WriteLine sText
	fNewFile.Close
End Sub
	
Sub WgetUnTarGZ(URL, DestFolder)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	StartPos = InstrRev(URL, "/", -1, 1)   
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	Wget URL, DestFolder
	UnTarGZ Destfolder & filename, DestFolder
End Sub

Sub WgetUnZip(URL, DestFolder)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	StartPos = InstrRev(URL, "/", -1, 1) 
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	NameEnd = InstrRev(filename, ".",-1, 1)
	filestrlength = Len(filename)
	filebase = Left(filename,NameEnd)
	Wget URL, DestFolder
	UnZip Destfolder & filename, DestFolder & filebase
End Sub

Sub GetTarGZObjects(DestFolder)
	Dim oExec

	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If

	If Not FSO.FileExists(DestFolder & "XTar.dll") Then 
		Wget ToolsBase & "XTar.dll", DestFolder
	End If

	If Not FSO.FileExists(DestFolder & "XGZip.dll") Then 
		Wget ToolsBase & "XGZip.dll", DestFolder
	End If
	
	If Not FSO.FileExists(DestFolder & "XZip.dll") Then 
		Wget ToolsBase & "XZip.dll", DestFolder
	End If
	
	WshShell.Run "regsvr32 /s " & DestFolder & "XTar.dll", 6, True
	
	WshShell.Run "regsvr32 /s " & DestFolder & "XGZip.dll", 6, True

	WshShell.Run "regsvr32 /s " & DestFolder & "XZip.dll", 6, True
	
End Sub

Sub UnTarGZ(TGZfile, DestFolder)

	Set objTAR = WScript.CreateObject("XStandard.TAR")
	Set objGZip = WScript.CreateObject("XStandard.GZip")
	wscript.echo("Extracting: " & TGZfile)
	objGZip.Decompress TGZfile, Destfolder
	objTAR.UnPack Left(TGZfile, Len(TGZfile)-3), Destfolder
	
	Set objTAR = Nothing
	Set objGZip = Nothing
End Sub


Sub UnZip(Zipfile, DestFolder)
	Dim objZip
	Set objZip = WScript.CreateObject("XStandard.Zip")
	wscript.echo("Extracting: " & Zipfile)
	objZip.UnPack Zipfile, DestFolder
	Set objZip = Nothing
End Sub


Sub Wget(URL, DestFolder)

	StartPos = InstrRev(URL, "/", -1, 1)   
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If

	Wscript.echo("Downloading: " & URL)
	xml.Open "GET", URL, False
	xml.Send
	
	Const adTypeBinary = 1
	Const adSaveCreateOverWrite = 2
	Const adSaveCreateNotExist = 1 
	
	oStream.type = adTypeBinary
	oStream.open
	oStream.write xml.responseBody
	
	' Do not overwrite an existing file
	'oStream.savetofile DestFolder & filename, adSaveCreateNotExist
	
	' Use this form to overwrite a file if it already exists
	 oStream.savetofile DestFolder & filename, adSaveCreateOverWrite
	
	oStream.close
	
End Sub

Function Showpath(folderspec)
	Set f = FSO.GetFolder(folderspec)
	showpath = f.path & "\"
End Function
