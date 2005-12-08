'On Error Resume Next
Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")
Set WshSysEnv = WshShell.Environment("SYSTEM")
Set xml = CreateObject("Microsoft.XMLHTTP")
Set oStream = createobject("Adodb.Stream")
Set objArgs = WScript.Arguments
Dim vcver, DevEnv, VCBuild
BuildRelease=False
BuildDebug=False
BuildCore=False
BuildModExosip=False
BuildModIaxChan=False
BuildModPortAudio=False
quote=Chr(34)
ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))

LibDestDir=Showpath(ScriptDir & "..\..\libs")
UtilsDir=Showpath(ScriptDir & "Tools")
GetTarGZObjects UtilsDir
GetVCBuild
Wscript.echo "Detected VCBuild: " & VCBuild

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
		Case "Mod_Exosip"   
			BuildModExosip=True
		Case "Mod_IaxChan"   
			BuildModIaxChan=True
		Case "Mod_PortAudio"
			BuildModPortAudio=True		
		Case Else
			BuildCore=True
			BuildModExosip=True
			BuildModIaxChan=True
			BuildModPortAudio=True		
	End Select
Else
	BuildCore=True
	BuildModExosip=True
	BuildModIaxChan=True
	BuildModPortAudio=True		
End If


If BuildCore Then
	BuildLibs_Core BuildDebug, BuildRelease
End If

If BuildModExosip Then
	BuildLibs_ModExosip BuildDebug, BuildRelease
End If

If BuildModIaxChan Then
	BuildLibs_ModIaxChan BuildDebug, BuildRelease
End If

If BuildModPortAudio Then
	BuildLibs_ModPortAudio BuildDebug, BuildRelease
End If

WScript.Echo "Complete"

Sub BuildLibs_Core(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "apr") Then 
		WgetUnTarGz "ftp://ftp.wayne.edu/apache/apr/apr-1.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "apr-1.2.2") Then
			Wscript.echo "Unable to get SQLite from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/apr-1.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "apr-1.2.2", "apr"
		FSO.CopyFile Utilsdir & "libapr.vcproj", LibDestDir & "apr\", True
		FindReplaceInFile LibDestDir & "apr\libapr.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
		FindReplaceInFile LibDestDir & "apr\file_io\unix\fullrw.c", "int i;", "unsigned int i;"
'		Upgrade LibDestDir & "apr\libapr.dsp", LibDestDir & "apr\libapr.vcproj"
	End If 
	If FSO.FolderExists(LibDestDir & "apr") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "apr\Debug\libapr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\libapr.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "apr\Release\libapr-1.lib") Then 
				BuildViaVCBuild LibDestDir & "apr\libapr.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download APR"
	End If 
	
	If Not FSO.FolderExists(LibDestDir & "sqlite") Then 
		WgetUnZip "http://www.sqlite.org/sqlite-source-3_2_7.zip", LibDestDir 
		If Not FSO.FolderExists(LibDestDir & "sqlite-source-3_2_7") Then
			Wscript.echo "Unable to get SQLite from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/sqlite-source-3_2_7.zip", LibDestDir
		End If
		RenameFolder LibDestDir & "sqlite-source-3_2_7", "sqlite"
		FSO.CopyFile Utilsdir & "sqlite.vcproj", LibDestDir & "sqlite\", True
		FindReplaceInFile LibDestDir & "sqlite\sqlite.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	'	Upgrade Utilsdir & "sqlite.vcproj", LibDestDir & "sqlite\sqlite.vcproj"
	End If
	If FSO.FolderExists(LibDestDir & "sqlite") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "sqlite\Debug\sqlite.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "sqlite\sqlite.vcproj"
				BuildViaVCBuild LibDestDir & "sqlite\sqlite.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "sqlite\Release\sqlite.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "sqlite\sqlite.vcproj"
				BuildViaVCBuild LibDestDir & "sqlite\sqlite.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download SQLite"
	End If 
End Sub

Sub BuildLibs_ModExosip(BuildDebug, BuildRelease)

	If Not FSO.FolderExists(LibDestDir & "osip") Then
		WgetUnTarGz "http://www.antisip.com/download/libosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libosip2-2.2.2") Then
			Wscript.echo "Unable to get osip from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/libosip2-2.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "libosip2-2.2.2", "osip"
'		FSO.CopyFile Utilsdir & "osipparser2.vcproj", LibDestDir & "osip\platform\vsnet\", True
'		FSO.CopyFile Utilsdir & "osip2.vcproj", LibDestDir & "osip\platform\vsnet\", True
		FindReplaceInFile LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
		FindReplaceInFile LibDestDir & "osip\platform\vsnet\osip2.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	If FSO.FolderExists(LibDestDir & "osip") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "osip\platform\vsnet\Debug\osip2.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "osip\platform\vsnet\osip.sln"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osip2.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "osip\platform\vsnet\Release\osip2.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "osip\platform\vsnet\osip.sln"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osip2.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "osip\platform\vsnet\osipparser2.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download Osip"
	End If 

	If Not FSO.FolderExists(LibDestDir & "libeXosip2") Then 
		WgetUnTarGz "http://www.antisip.com/download/libeXosip2-2.2.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "libeXosip2-2.2.2") Then
			Wscript.echo "Unable to get eXosip from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/libeXosip2-2.2.2.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "libeXosip2-2.2.2", "libeXosip2"
		FindReplaceInFile LibDestDir & "libeXosip2\platform\vsnet\eXosip.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
'		FSO.CopyFile Utilsdir & "eXosip.vcproj", LibDestDir & "libeXosip2\platform\vsnet\", True
	End If
	If FSO.FolderExists(LibDestDir & "libeXosip2") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "libeXosip2\platform\vsnet\Debug\exosip.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj"
				BuildViaVCBuild LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "libeXosip2\platform\vsnet\Release\exosip.lib") Then 
'				UpgradeViaDevEnv LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj"
				BuildViaVCBuild LibDestDir & "libeXosip2\platform\vsnet\exosip.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download exosip"
	End If 

	If Not FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then 
		WgetUnTarGz "http://research.edm.luc.ac.be/jori/jthread/jthread-1.1.2.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then
			Wscript.echo "Unable to get JThread from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/jthread-1.1.2.tar.gz", LibDestDir
		End If
		FindReplaceInFile LibDestDir & "jthread-1.1.2\jthread.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
	End If
	
	If Not FSO.FolderExists(LibDestDir & "jrtplib") Then 
		WgetUnTarGz "http://research.edm.luc.ac.be/jori/jrtplib/jrtplib-3.3.0.tar.gz", LibDestDir
		If Not FSO.FolderExists(LibDestDir & "jrtplib-3.3.0") Then
			Wscript.echo "Unable to get JRTPLib from default download location, Trying backup location:"
			WgetUnTarGz "http://www.sofaswitch.org/mikej/jrtplib-3.3.0.tar.gz", LibDestDir
		End If
		RenameFolder LibDestDir & "jrtplib-3.3.0", "jrtplib"
		FindReplaceInFile LibDestDir & "jrtplib\jrtplib.vcproj", "WIN32;", "_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;WIN32;"
		FindReplaceInFile LibDestDir & "jrtplib\jrtplib.vcproj", "WarningLevel=" & quote & "3" & quote, "WarningLevel=" & quote & "0" & quote
	End If
	If FSO.FolderExists(LibDestDir & "jrtplib") And FSO.FolderExists(LibDestDir & "jthread-1.1.2") And FSO.FolderExists(LibDestDir & "jrtp4c")Then 
		If BuildDebug Then
			If (Not FSO.FileExists(LibDestDir & "jrtp4c\w32\Debug\jrtp4c.lib")) Or (Not FSO.FileExists(LibDestDir & "jrtplib\Debug\jrtplib.lib")) Or (Not FSO.FileExists(LibDestDir & "jthread-1.1.2\Debug\jthread.lib")) Then 
'				UpgradeViaDevEnv LibDestDir & "jrtp4c\w32\jrtp4c.sln"
				BuildViaVCBuild LibDestDir & "jrtp4c\w32\jrtp4c.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "jrtplib\jrtplib.vcproj", "Debug"
				BuildViaVCBuild LibDestDir & "jthread-1.1.2\jthread.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If (Not FSO.FileExists(LibDestDir & "jrtp4c\w32\Release\jrtp4c.lib")) Or (Not FSO.FileExists(LibDestDir & "jrtplib\Release\jrtplib.lib")) Or (Not FSO.FileExists(LibDestDir & "jthread-1.1.2\Release\jthread.lib")) Then 
'				UpgradeViaDevEnv LibDestDir & "jrtp4c\w32\jrtp4c.sln"
				BuildViaVCBuild LibDestDir & "jrtp4c\w32\jrtp4c.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "jrtplib\jrtplib.vcproj", "Release"
				BuildViaVCBuild LibDestDir & "jthread-1.1.2\jthread.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download JRtplib"
	End If 


End Sub

Sub BuildLibs_ModIaxChan(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "iax") Then 
		WgetUnTarGz "http://www.sofaswitch.org/mikej/iax-0.2.3.tar.gz", LibDestDir
		RenameFolder LibDestDir & "iax-0.2.3", "iax"
	End If 
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

Sub BuildLibs_ModPortAudio(BuildDebug, BuildRelease)
	If Not FSO.FolderExists(LibDestDir & "PortAudio") Then 
		WgetUnZip "http://www.sofaswitch.org/mikej/portaudio_v18_1.zip", LibDestDir
		RenameFolder LibDestDir & "portaudio_v18_1", "PortAudio"
	End If 
	If FSO.FolderExists(LibDestDir & "PortAudio") Then 
		If BuildDebug Then
			If Not FSO.FileExists(LibDestDir & "PortAudio\Lib\PAStaticWMMED.lib") Then 
				BuildViaVCBuild LibDestDir & "PortAudio\winvc\PAStaticWMME\PAStaticWMME.vcproj", "Debug"
			End If
		End If
		If BuildRelease Then
			If Not FSO.FileExists(LibDestDir & "PortAudio\Lib\PAStaticWMME.lib") Then 
				BuildViaVCBuild LibDestDir & "PortAudio\winvc\PAStaticWMME\PAStaticWMME.vcproj", "Release"
			End If
		End If
	Else
		Wscript.echo "Unable to download PortAudio"
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
'On Error Resume Next
	Set Folder=FSO.GetFolder(FolderName)
	Folder.Name = NewFolderName
'On Error GoTo 0
End Sub

Sub Upgrade(OldFileName, NewFileName)
'On Error Resume Next
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
	
	
'	WScript.Echo("Converting: "+ OldFileName)
	
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
		Wget "http://www.sofaswitch.org/mikej/XTar.dll", DestFolder
	End If

	If Not FSO.FileExists(DestFolder & "XGZip.dll") Then 
		Wget "http://www.sofaswitch.org/mikej/XGZip.dll", DestFolder
	End If
	
	If Not FSO.FileExists(DestFolder & "XZip.dll") Then 
		Wget "http://www.sofaswitch.org/mikej/XZip.dll", DestFolder
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