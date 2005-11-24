On Error Resume Next
Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")
Set WshSysEnv = WshShell.Environment("SYSTEM")
Set xml = CreateObject("Microsoft.XMLHTTP")
Set oStream = createobject("Adodb.Stream")

ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))

LibDestDir=Showpath(ScriptDir & "..\..\libs")
UtilsDir=Showpath(ScriptDir & "Tools")

GetTarGZObjects UtilsDir
If Not FSO.FolderExists(LibDestDir & "osip") Then
	WgetUnTarGz "http://www.antisip.com/download/libosip2-2.2.1.tar.gz", LibDestDir
	RenameFolder LibDestDir & "libosip2-2.2.1", "osip"
'	FSO.CopyFile Utilsdir & "osipparser2.vcproj", LibDestDir & "osip\platform\vsnet\", True
	Upgrade Utilsdir & "osipparser2.vcproj", LibDestDir & "osip\platform\vsnet\osipparser2.vcproj"
	Upgrade LibDestDir & "osip\platform\vsnet\osip2.vcproj", LibDestDir & "osip\platform\vsnet\osip2.vcproj"
End If

If Not FSO.FolderExists(LibDestDir & "libeXosip2") Then 
	WgetUnTarGz "http://www.antisip.com/download/libeXosip2-1.9.1-pre17.tar.gz", LibDestDir
	RenameFolder LibDestDir & "libeXosip2-1.9.1-pre17", "libeXosip2"
	Upgrade Utilsdir & "eXosip.vcproj", LibDestDir & "libeXosip2\platform\vsnet\eXosip.vcproj"
End If

If Not FSO.FolderExists(LibDestDir & "jthread-1.1.2") Then 
	WgetUnTarGz "http://research.edm.luc.ac.be/jori/jthread/jthread-1.1.2.tar.gz", LibDestDir
End If

If Not FSO.FolderExists(LibDestDir & "jrtplib") Then 
	WgetUnTarGz "http://research.edm.luc.ac.be/jori/jrtplib/jrtplib-3.3.0.tar.gz", LibDestDir
	RenameFolder LibDestDir & "jrtplib-3.3.0", "jrtplib"
End If

If Not FSO.FolderExists(LibDestDir & "apr") Then 
	WgetUnTarGz "ftp://ftp.wayne.edu/apache/apr/apr-1.2.2.tar.gz", LibDestDir
	RenameFolder LibDestDir & "apr-1.2.2", "apr"
	Unix2dos LibDestDir & "apr\libapr.dsp"
	Upgrade LibDestDir & "apr\libapr.dsp", LibDestDir & "apr\libapr.vcproj"
End If 

If Not FSO.FolderExists(LibDestDir & "sqlite") Then 
	WgetUnZip "http://www.sqlite.org/sqlite-source-3_2_7.zip", LibDestDir 
	RenameFolder LibDestDir & "sqlite-source-3_2_7", "sqlite"
	Upgrade Utilsdir & "sqlite.vcproj", LibDestDir & "sqlite\sqlite.vcproj"
End If

WScript.Echo "Download Complete"

Sub RenameFolder(FolderName, NewFolderName)
	Set Folder=FSO.GetFolder(FolderName)
	Folder.Name = NewFolderName
End Sub

Sub Upgrade(OldFileName, NewFileName)

	If WshSysEnv("VS80COMNTOOLS")<> "" Then 
		Set vcProj = CreateObject("VisualStudio.VCProjectEngine.8.0")
	Else If WshSysEnv("VS71COMNTOOLS")<> "" Then
		Set vcProj = CreateObject("VisualStudio.VCProjectEngine.7.1")
	Else
		Wscript.Echo("Did not find any Visual Studio .net 2003 or 2005 on your machine")
		WScript.Quit(1)
	End If
	End If
	
	
'	WScript.Echo("Converting: "+ OldFileName)
	
	Set vcProject = vcProj.LoadProject(OldFileName)
'	If Not FSO.FileExists(vcProject.ProjectFile) Then
		'   // specify name and location of new project file
		vcProject.ProjectFile = NewFileName
'	End If
	'   // call the project engine to save this off. 
	'   // when no name is shown, it will create one with the .vcproj name
	vcProject.Save()
'	WScript.Echo("New Project Name: "+vcProject.ProjectFile+"")

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
'	wscript.echo("Extracting: " & TGZfile)
	objGZip.Decompress TGZfile, Destfolder
	objTAR.UnPack Left(TGZfile, Len(TGZfile)-3), Destfolder
	
	Set objTAR = Nothing
	Set objGZip = Nothing
End Sub


Sub UnZip(Zipfile, DestFolder)
Dim objZip
Set objZip = WScript.CreateObject("XStandard.Zip")
objZip.UnPack Zipfile, DestFolder
Set objZip = Nothing
End Sub


Sub Wget(URL, DestFolder)

	StartPos = InstrRev(URL, "/", -1, 1)   
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If

'	Wscript.echo("Downloading: " & URL)
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