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
AutoBuild=False
BuildVersion=False
BuildModExosip=False
BuildModIaxChan=False
BuildModPortAudio=False
BuildModSpeexCodec=False
BuildModCodecG729=False
BuildModCodecGSM=False
BuildModXMPPEvent=False
BuildModsndfile=False
BuildModpcre=False
BuildModldap=False
BuildModzeroconf=False
BuildModSpiderMonkey=False
BuildModDingaling=False
quote=Chr(34)
ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))

ToolsBase="http://svn.freeswitch.org/downloads/win32/"
LibsBase="http://svn.freeswitch.org/downloads/libs/"
LibDestDir=Showpath(ScriptDir & "..\..\libs")
FreeswitchDir=Showpath(ScriptDir & "..\..")
UtilsDir=Showpath(ScriptDir & "Tools")
If objArgs.Count >=1 Then
	If objArgs(0) <> "Version" Then
		GetCompressionTools UtilsDir
		GetVCBuild
		Wscript.echo "Detected VCBuild: " & VCBuild
	End If
Else
	GetCompressionTools UtilsDir
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
		Case "Build"		
			AutoBuild=True
		Case "Core"		
			BuildCore=True
		Case "Version"		
			BuildVersion=True
		Case "Mod_Exosip"   
			BuildModExosip=True
		Case "Mod_iax"   
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
		Case "Mod_pcre"
			BuildModpcre=True
		Case "Mod_ldap"
			BuildModldap=True
		Case "Mod_zeroconf"
			BuildModzeroconf=True
		Case "Mod_SpiderMonkey"
			BuildModSpiderMonkey=True
		Case "Mod_Dingaling"
			BuildModDingaling=True
		Case Else
			BuildCore=True
			BuildModExosip=True
			BuildModIaxChan=True
			BuildModPortAudio=True		
			BuildModSpeexCodec=True
			BuildModCodecG729=True
			BuildModXMPPEvent=True
			BuildModsndfile=True
			BuildVersion=True
			BuildModpcre=True
			BuildModldap=True
			BuildModzeroconf=True
			BuildModSpiderMonkey=True
			BuildModDingaling=True
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
	BuildVersion=True
	BuildModldap=True
	BuildModpcre=True
	BuildModzeroconf=True
	BuildModSpiderMonkey=True
	BuildModDingaling=True
End If

' ******************
' Process lib builds
' ******************

If BuildVersion Then
	CreateSwitchVersion
End If

If AutoBuild Then
	If BuildDebug Then
		BuildViaVCBuild ScriptDir & "Freeswitch.sln", "DEBUG|WIN32"
	End If
	If BuildRelease Then
		BuildViaVCBuild ScriptDir & "Freeswitch.sln", "RELEASE|WIN32"
	End If
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
	BuildLibs_srtp BuildDebug, BuildRelease
	FSO.CopyFile LibDestDir & "srtp\include\*.h", LibDestDir & "include"
	FSO.CopyFile LibDestDir & "srtp\crypto\include\*.h", LibDestDir & "include"
End If

If BuildModzeroconf Then
	BuildLibs_howl BuildDebug, BuildRelease	
End If

If BuildModExosip Then
	BuildLibs_libosip2 BuildDebug, BuildRelease
	BuildLibs_exosip BuildDebug, BuildRelease
End If

If BuildModDingaling Then
	BuildLibs_iksemel BuildDebug, BuildRelease
	BuildLibs_libdingaling BuildDebug, BuildRelease
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

If BuildModpcre Then
	BuildLibs_pcre BuildDebug, BuildRelease
End If

If BuildModldap Then
	BuildLibs_ldap BuildDebug, BuildRelease
End If

If BuildModSpiderMonkey Then
	BuildLibs_SpiderMonkey BuildDebug, BuildRelease
	BuildLibs_curl BuildDebug, BuildRelease
End If

WScript.Echo "Complete"

'  ******************
'  Lib Build Sectiton
'  ******************
Sub BuildLibs_aprutil(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr-util") Then 
		WgetUnCompress "ftp://ftp.wayne.edu/apache/apr/apr-util-1.2.6.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-util-1.2.6") Then
			Wscript.echo "Unable to get apr-util from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "apr-util-1.2.6.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-util-1.2.6", "apr-util"
		FSO.CopyFile Utilsdir & "apr\xml.vcproj", LibDestDir & "apr-util\xml\expat\lib\", True
		FSO.CopyFile Utilsdir & "apr\libaprutil.vcproj", LibDestDir & "apr-util\", True
	End If 
	If FSO.FolderExists(LibDestDir & "apr-util") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr-util\xml\expat\lib\LibD\xml.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\xml\expat\lib\xml.vcproj", "Debug"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\Debug\libaprutil-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\libaprutil.vcproj", "Debug"
				FSO.CopyFile LibDestDir & "apr-util\Debug\libaprutil-1.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "apr-util\Debug\libaprutil-1.lib", ScriptDir & "Debug\", True
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr-util\xml\expat\lib\LibR\xml.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\xml\expat\lib\xml.vcproj", "Release"
			End If
			If Not FSO.FileExists(LibDestDir & "apr-util\Release\libaprutil-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-util\libaprutil.vcproj", "Release"
				FSO.CopyFile LibDestDir & "apr-util\Release\libaprutil-1.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "apr-util\Release\libaprutil-1.lib", ScriptDir & "Release\", True
			End If
		End If
	Else
		Wscript.echo "Unable to download apr-util"
	End If 	
End Sub

Sub BuildLibs_apriconv(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr-iconv") Then 
		WgetUnCompress "ftp://ftp.wayne.edu/apache/apr/apr-iconv-1.1.1.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-iconv-1.1.1") Then
			Wscript.echo "Unable to get apr-iconv from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "apr-iconv-1.1.1.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-iconv-1.1.1", "apr-iconv"
		FSO.CopyFile Utilsdir & "apr\libapriconv.vcproj", LibDestDir & "apr-iconv\", True
	End If 
	If FSO.FolderExists(LibDestDir & "apr-iconv") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr-iconv\Debug\libapriconv-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-iconv\libapriconv.vcproj", "Debug"
				FSO.CopyFile LibDestDir & "apr-iconv\Debug\libapriconv-1.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "apr-iconv\Debug\libapriconv-1.lib", ScriptDir & "Debug\", True
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr-iconv\Release\libapriconv-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr-iconv\libapriconv.vcproj", "Release"
				FSO.CopyFile LibDestDir & "apr-iconv\Release\libapriconv-1.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "apr-iconv\Release\libapriconv-1.lib", ScriptDir & "Release\", True
			End If
		End If
	Else
		Wscript.echo "Unable to download apr-iconv"
	End If 
End Sub

Sub BuildLibs_apr(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr") Then 
		WgetUnCompress "ftp://ftp.wayne.edu/apache/apr/apr-1.2.6.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-1.2.6") Then
			Wscript.echo "Unable to get apr from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "apr-1.2.6.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-1.2.6", "apr"
		FSO.CopyFile Utilsdir & "apr\libapr.vcproj", LibDestDir & "apr\", True
		FSO.CopyFile Utilsdir & "apr\apr.hw", LibDestDir & "apr\include\", True
	End If 
	If FSO.FolderExists(LibDestDir & "apr") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr\Debug\libapr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\libapr.vcproj", "Debug"
				FSO.CopyFile LibDestDir & "apr\Debug\libapr-1.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "apr\Debug\libapr-1.lib", ScriptDir & "Debug\", True
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr\Release\libapr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\libapr.vcproj", "Release"
				FSO.CopyFile LibDestDir & "apr\Release\libapr-1.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "apr\Release\libapr-1.lib", ScriptDir & "Release\", True
			End If
		End If
	Else
		Wscript.echo "Unable to download APR"
	End If 
End Sub

Sub BuildLibs_exosip(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libeXosip2") Then 
		WgetUnCompress "http://www.antisip.com/download/libeXosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libeXosip2-2.2.2") Then
			Wscript.echo "Unable to get eXosip from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "libeXosip2-2.2.2.tar.gz", LibDestDir
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
		WgetUnCompress "http://www.antisip.com/download/libosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libosip2-2.2.2") Then
			Wscript.echo "Unable to get osip from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "libosip2-2.2.2.tar.gz", LibDestDir
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

Sub BuildLibs_srtp(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "srtp") Then 
		WgetUnCompress LibsBase & "srtp.zip", LibDestDir
	End If 
	If FSO.FolderExists(LibDestDir & "srtp") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "srtp\Debug\srtp.lib") Then 
				BuildViaVCBuild LibDestDir & "srtp\srtp.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "srtp\Release\srtp.lib") Then 
				BuildViaVCBuild LibDestDir & "srtp\srtp.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download srtp"
	End If
End Sub

Sub BuildLibs_sqlite(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "sqlite") Then 
		WgetUnCompress "http://www.sqlite.org/sqlite-source-3_2_7.zip", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "sqlite-source-3_2_7") Then
			Wscript.echo "Unable to get SQLite from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "sqlite-source-3_2_7.zip", LibDestDir
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
		WgetUnCompress "http://jabberstudio.2nw.net/iksemel/iksemel-1.2.tar.gz", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "iksemel-1.2") Then
			Wscript.echo "Unable to get iksemel from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "iksemel-1.2.tar.gz", LibDestDir
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
		WgetUnCompress LibsBase & "portaudio_v18_1.zip", LibDestDir
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

Sub BuildLibs_libxml2(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libxml2") Then 
		WgetUnCompress "http://xmlsoft.org/sources/libxml2-sources-2.6.23.tar.gz", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "libxml2-2.6.23") Then
			Wscript.echo "Unable to get libxml2 from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "libxml2-sources-2.6.23.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "libxml2-2.6.23", "libxml2"
	End If
	If FSO.FolderExists(LibDestDir & "libxml2") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libxml2\win32\bin.msvc\libxml2_a.lib") Then 
				Exec "cscript configure.js compiler=msvc iconv=no debug=yes", Showpath(LibDestDir & "libxml2\win32\" & "\")
				Exec "nmake /f Makefile.msvc  all  CRUNTIME=" & quote & "/MD /D _CRT_SECURE_NO_DEPRECATE /D _CRT_NONSTDC_NO_DEPRECATE" & quote, Showpath(LibDestDir & "libxml2\win32\" & "\")
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libxml2\win32\bin.msvc\libxml2_a.lib") Then 
				Exec "cscript configure.js compiler=msvc iconv=no debug=no", Showpath(LibDestDir & "libxml2\win32\" & "\")
				Exec "nmake /f Makefile.msvc  all  CRUNTIME=" & quote & "/MD /D _CRT_SECURE_NO_DEPRECATE /D _CRT_NONSTDC_NO_DEPRECATE" & quote, Showpath(LibDestDir & "libxml2\win32\" & "\")
			End If
		End If
	Else
		Wscript.echo "Unable to download libxml2"
	End If
End Sub

Sub BuildLibs_libdingaling(BuildDebug, BuildRelease)
	If FSO.FolderExists(LibDestDir & "libdingaling") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libdingaling\Debug\libdingaling.lib") Then 
				BuildViaVCBuild LibDestDir & "libdingaling\libdingaling.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libdingaling\Release\libdingaling.lib") Then 
				BuildViaVCBuild LibDestDir & "libdingaling\libdingaling.vcproj", "Release"
			End If
		End If
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
		WgetUnCompress "http://downloads.us.xiph.org/releases/speex/speex-1.1.11.1.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "speex-1.1.11.1") Then
			Wscript.echo "Unable to get libspeex from default download location, Trying backup location:"
			WgetUnCompress LibsBase & "speex-1.1.11.1.tar.gz", LibDestDir
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
		WgetUnCompress LibsBase & "libsndfile-1.0.12.tar.gz", LibDestDir
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

Sub BuildLibs_howl(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "howl") Then 
		WgetUnCompress LibsBase & "howl-1.0.0.tar.gz", LibDestDir
		RenameFolder LibDestDir & "howl-1.0.0", "howl"
		FSO.CopyFile Utilsdir & "howl\libhowl.vcproj", LibDestDir & "howl\src\lib\howl\Win32\", True
		FSO.CopyFile Utilsdir & "howl\libmDNSResponder.vcproj", LibDestDir & "howl\src\lib\mDNSResponder\Win32\", True
	End If 
	If FSO.FolderExists(LibDestDir & "howl") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "howl\src\lib\howl\Win32\Debug\libhowld.lib") Then 
				BuildViaVCBuild LibDestDir & "howl\src\lib\howl\Win32\libhowl.vcproj", "Debug"
			End If
			If Not FSO.FileExists(LibDestDir & "howl\src\lib\mDNSResponder\Win32\Debug\libmDNSResponderd.lib") Then 
				BuildViaVCBuild LibDestDir & "howl\src\lib\mDNSResponder\Win32\libmDNSResponder.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "howl\src\lib\howl\Win32\Release\libhowl.lib") Then 
				BuildViaVCBuild LibDestDir & "howl\src\lib\howl\Win32\libhowl.vcproj", "Release"
			End If
			If Not FSO.FileExists(LibDestDir & "howl\src\lib\mDNSResponder\Win32\Release\libmDNSResponder.lib") Then 
				BuildViaVCBuild LibDestDir & "howl\src\lib\mDNSResponder\Win32\libmDNSResponder.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download howl"
	End If 
End Sub

Sub BuildLibs_libresample(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "libresample") Then 
		WgetUnCompress LibsBase & "libresample-0.1.3.zip", LibDestDir
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

Sub BuildLibs_pcre(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "pcre") Then 
		WgetUnCompress LibsBase & "pcre-6.4.tar.gz", LibDestDir
		RenameFolder LibDestDir & "pcre-6.4", "pcre"
		If Not FSO.FolderExists(LibDestDir & "pcre\win32") Then
			FSO.CreateFolder(LibDestDir & "pcre\win32")
		End If
		FSO.CopyFile Utilsdir & "pcre\libpcre.vcproj", LibDestDir & "pcre\win32\", True
		FSO.CopyFile Utilsdir & "pcre\pcre_chartables.c.vcproj", LibDestDir & "pcre\win32\", True
		FSO.CopyFile Utilsdir & "pcre\pcre.h", LibDestDir & "pcre\win32\", True
		FSO.CopyFile Utilsdir & "pcre\config.h", LibDestDir & "pcre\win32\", True
	End If 
	If FSO.FolderExists(LibDestDir & "pcre") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "pcre\win32\Debug\libpcre.lib") Then 
				BuildViaVCBuild LibDestDir & "pcre\win32\pcre_chartables.c.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "pcre\win32\libpcre.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "pcre\win32\Release\libpcre.lib") Then 
				BuildViaVCBuild LibDestDir & "pcre\win32\pcre_chartables.c.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "pcre\win32\libpcre.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download pcre"
	End If 
End Sub

Sub BuildLibs_curl(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "curl") Then 
		WgetUnCompress LibsBase & "curl-7.15.2.tar.gz", LibDestDir
		RenameFolder LibDestDir & "curl-7.15.2", "curl"
		FSO.CopyFile Utilsdir & "curl\curllib.vcproj", LibDestDir & "curl\lib\", True
	End If 
	If FSO.FolderExists(LibDestDir & "curl") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "curl\lib\Debug\curllib.lib") Then 
				BuildViaVCBuild LibDestDir & "curl\lib\curllib.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "curl\lib\Release\curllib.lib") Then 
				BuildViaVCBuild LibDestDir & "curl\lib\curllib.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download curl"
	End If 
End Sub

Sub	BuildLibs_ldap(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "openldap") Then 
		WgetUnCompress LibsBase & "openldap-2.3.19.tar.gz", LibDestDir
		RenameFolder LibDestDir & "openldap-2.3.19", "openldap"
		FSO.CopyFile Utilsdir & "openldap\lber_types.h", LibDestDir & "openldap\include\", True
		FSO.CopyFile Utilsdir & "openldap\ldap_config.h", LibDestDir & "openldap\include\", True
		FSO.CopyFile Utilsdir & "openldap\ldap_features.h", LibDestDir & "openldap\include\", True
		FSO.CopyFile Utilsdir & "openldap\portable.h", LibDestDir & "openldap\include\", True
		FSO.CopyFile Utilsdir & "openldap\liblber.vcproj", LibDestDir & "openldap\libraries\liblber\", True
		FSO.CopyFile Utilsdir & "openldap\libldap.vcproj", LibDestDir & "openldap\libraries\libldap\", True
		FSO.CopyFile Utilsdir & "openldap\libldap_r.vcproj", LibDestDir & "openldap\libraries\libldap_r\", True
	End If 
	If FSO.FolderExists(LibDestDir & "openldap") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "openldap\Debug\oldap_r.lib") Then 
				BuildViaVCBuild LibDestDir & "openldap\libraries\liblber\liblber.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "openldap\libraries\libldap_r\libldap_r.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "openldap\Release\oldap_r.lib") Then 
				BuildViaVCBuild LibDestDir & "openldap\libraries\liblber\liblber.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "openldap\libraries\libldap_r\libldap_r.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download openldap"
	End If 
End Sub


Sub BuildLibs_SpiderMonkey(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "js") Then 
		WgetUnCompress LibsBase & "js20051231.zip", LibDestDir
		RenameFolder LibDestDir & "js20051231", "js"
		WgetUnCompress LibsBase & "nspr-4.6.1.winnt5.debug.zip", LibDestDir & "js"
		WgetUnCompress LibsBase & "nspr-4.6.1.winnt5.release.zip", LibDestDir & "js"
		FSO.CreateFolder LibDestDir & "js\nspr\"
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.debug\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
	End If 
	If FSO.FolderExists(LibDestDir & "js") Then 
		If BuildDebug Then
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.debug\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
			If Not FSO.FileExists(LibDestDir & "js\src\Debug\js32.dll") Then 
				BuildViaVCBuild LibDestDir & "js\src\fdlibm\fdlibm.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "js\src\js.vcproj", "Debug"
				FSO.CopyFile LibDestDir & "js\src\Debug\js32.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libnspr4.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libplc4.dll", ScriptDir & "Debug\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libplds4.dll", ScriptDir & "Debug\", True
			End If
		End If
		If BuildRelease Then
		FSO.CopyFolder LibDestDir & "js\nspr-4.6.1.winnt5.release\nspr-4.6.1\*", LibDestDir & "js\nspr\",true
			If Not FSO.FileExists(LibDestDir & "js\src\Release\js32.dll") Then 
				BuildViaVCBuild LibDestDir & "js\src\fdlibm\fdlibm.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "js\src\js.vcproj", "Release"
				FSO.CopyFile LibDestDir & "js\src\Release\js32.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libnspr4.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libplc4.dll", ScriptDir & "Release\", True
				FSO.CopyFile LibDestDir & "js\nspr\lib\libplds4.dll", ScriptDir & "Release\", True
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
		WgetUnCompress ToolsBase & "svnversion.zip", UtilsDir 
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

Sub BuildViaVCBuild(ProjectFile, BuildType)
	Wscript.echo "Building : " & ProjectFile & " Config type: " & BuildType
	BuildCmd=quote & VCBuild & quote & " /nologo /nocolor " & quote & ProjectFile & quote & " " & quote & BuildType & quote
	Set MyFile = fso.CreateTextFile(UtilsDir & "tmpBuild.Bat", True)
	MyFile.WriteLine("@" & BuildCmd)
	MyFile.Close

	Set oExec = WshShell.Exec(UtilsDir & "tmpBuild.Bat")
	Do
		strFromProc = OExec.StdOut.ReadLine()
		WScript.Echo  strFromProc
	Loop While Not OExec.StdOut.atEndOfStream
End Sub

Sub Exec(cmdline, strpath)
	If WshSysEnv("VS80COMNTOOLS")<> "" Then 
		vcver = "8"
		Vsvars="call " & quote & Showpath(WshSysEnv("VS80COMNTOOLS")&"\") & "vsvars32.bat" & quote
	Else If WshSysEnv("VS71COMNTOOLS")<> "" Then
		vcver = "7"
		Vsvars="call " & quote & Showpath(WshSysEnv("VS71COMNTOOLS")&"\") & "vsvars32.bat" & quote
	Else
		Wscript.Echo("Did not find any Visual Studio .net 2003 or 2005 on your machine")
		WScript.Quit(1)
	End If
	End If
	Wscript.echo "Executing : " & cmdline
	Set MyFile = fso.CreateTextFile(UtilsDir & "tmpcmd.Bat", True)
	MyFile.WriteLine("@" & "cd " & strpath)
	MyFile.WriteLine("@" & Vsvars)
	MyFile.WriteLine("@" & cmdline)
	MyFile.Close

	Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
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

Sub RenameFolder(FolderName, NewFolderName)
	Set Folder=FSO.GetFolder(FolderName)
	Folder.Name = NewFolderName
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

Sub WgetUnCompress(URL, DestFolder)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	StartPos = InstrRev(URL, "/", -1, 1) 
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	NameEnd = InstrRev(filename, ".",-1, 1)
	filestrlength = Len(filename)
	filebase = Left(filename,NameEnd)
	fileext = Right(filename, Len(filename) - NameEnd)
	Wget URL, DestFolder
	If fileext = "zip" Then
		UnCompress Destfolder & filename, DestFolder & filebase
	Else
		UnCompress Destfolder & filename, DestFolder	
	End If
End Sub

Sub GetCompressionTools(DestFolder)
	Dim oExec
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	If Not FSO.FileExists(DestFolder & "7za.exe") Then 
		Wget ToolsBase & "7za.exe", DestFolder
	End If	
End Sub

Sub UnCompress(Archive, DestFolder)
	wscript.echo("Extracting: " & Archive)
	Set MyFile = fso.CreateTextFile(UtilsDir & "tmpcmd.Bat", True)
	MyFile.WriteLine("@" & UtilsDir & "7za.exe x " & Archive & " -y -o" & DestFolder)
	MyFile.Close
	Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
	Do
		WScript.Echo OExec.StdOut.ReadLine()
	Loop While Not OExec.StdOut.atEndOfStream
	If FSO.FileExists(Left(Archive, Len(Archive)-3)) Then  
		Set MyFile = fso.CreateTextFile(UtilsDir & "tmpcmd.Bat", True)
		MyFile.WriteLine("@" & UtilsDir & "7za.exe x " & Left(Archive, Len(Archive)-3) & " -y -o" & DestFolder)
		MyFile.Close
		Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
		Do
			WScript.Echo OExec.StdOut.ReadLine()
		Loop While Not OExec.StdOut.atEndOfStream
		WScript.Sleep(500)
		FSO.DeleteFile Left(Archive, Len(Archive)-3) ,true 
	End If
	WScript.Sleep(500)
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
	oStream.savetofile DestFolder & filename, adSaveCreateOverWrite
	
	oStream.close
End Sub

Function Showpath(folderspec)
	Set f = FSO.GetFolder(folderspec)
	showpath = f.path & "\"
End Function
