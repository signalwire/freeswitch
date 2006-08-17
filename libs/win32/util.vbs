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
quote=Chr(34)
ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))
UtilsDir=Showpath(ScriptDir)
ToolsBase="http://svn.freeswitch.org/downloads/win32/"


GetCompressionTools UtilsDir


If objArgs.Count >=3 Then
	Select Case objArgs(0)
		Case "Get"		
			Wget objArgs(1), Showpath(objArgs(2))
		Case "GetUnzip"		
			WgetUnCompress objArgs(1), Showpath(objArgs(2))
	End Select
End If


' *******************
' Utility Subroutines
' *******************


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
	MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Archive & quote & " -y -o" & quote & DestFolder & quote )
	MyFile.Close
	Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
	Do
		WScript.Echo OExec.StdOut.ReadLine()
	Loop While Not OExec.StdOut.atEndOfStream
	If FSO.FileExists(Left(Archive, Len(Archive)-3))Then  
		Set MyFile = fso.CreateTextFile(UtilsDir & "tmpcmd.Bat", True)
		MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Left(Archive, Len(Archive)-3) & quote & " -y -o" & quote & DestFolder & quote )
		MyFile.Close
		Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
		Do
			WScript.Echo OExec.StdOut.ReadLine()
		Loop While Not OExec.StdOut.atEndOfStream
		WScript.Sleep(500)
		FSO.DeleteFile Left(Archive, Len(Archive)-3) ,true 
	End If
	If FSO.FileExists(Left(Archive, Len(Archive)-3) & "tar")Then  
		Set MyFile = fso.CreateTextFile(UtilsDir & "tmpcmd.Bat", True)
		MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Left(Archive, Len(Archive)-3) & "tar" & quote & " -y -o" & quote & DestFolder & quote )
		MyFile.Close
		Set oExec = WshShell.Exec(UtilsDir & "tmpcmd.Bat")
		Do
			WScript.Echo OExec.StdOut.ReadLine()
		Loop While Not OExec.StdOut.atEndOfStream
		WScript.Sleep(500)
		FSO.DeleteFile Left(Archive, Len(Archive)-3) & "tar",true 
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
