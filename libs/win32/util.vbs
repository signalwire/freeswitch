'
' Contributor(s):
' Michael Jerris <mike@jerris.com>
' David A. Horner http://dave.thehorners.com
'----------------------------------------------

' **************
' Initialization
' **************

Set WshShell = CreateObject("WScript.Shell")
Set FSO = CreateObject("Scripting.FileSystemObject")
Set WshSysEnv = WshShell.Environment("SYSTEM")
Set xml = CreateObject("Microsoft.XMLHTTP")
Dim UseWgetEXE

On Error Resume Next
Set oStream = CreateObject("Adodb.Stream")
On Error Goto 0

If Not IsObject(oStream)  Then
	wscript.echo("Failed to create Adodb.Stream, using alternative download method.")
	UseWgetEXE=true
Else
	UseWgetEXE=false
End If
Randomize
Set objArgs = WScript.Arguments
quote=Chr(34)
ScriptDir=Left(WScript.ScriptFullName,Len(WScript.ScriptFullName)-Len(WScript.ScriptName))
UtilsDir=Showpath(ScriptDir)
ToolsBase="http://files.freeswitch.org/downloads/win32/"

If UseWgetEXE Then
	GetWgetEXE UtilsDir
End If

If objArgs.Count >=3 Then
	Select Case objArgs(0)
		Case "Get"
			Wget objArgs(1), Showpath(objArgs(2))
		Case "GetUnzip"
			WgetUnCompress objArgs(1), Showpath(objArgs(2))
		Case "GetUnzipSounds"
			WgetSounds objArgs(1), objArgs(2), Showpath(objArgs(3)), objArgs(4)
		Case "Version"
			'CreateVersion(tmpFolder, VersionDir, includebase, includedest)
			CreateVersion Showpath(objArgs(1)), Showpath(objArgs(2)), objArgs(3), objArgs(4)
	End Select
End If


' *******************
' Utility Subroutines
' *******************

Sub WgetSounds(PrimaryName, Freq, DestFolder, VersionFile)
	BaseURL = "http://files.freeswitch.org/freeswitch-sounds"
	Set objFSO = CreateObject("Scripting.FileSystemObject")
	Set objTextFile = objFSO.OpenTextFile(VersionFile,1)
	Do Until objTextFile.AtEndOfStream
		strLine = objTextFile.Readline
		if Len(strLine) > 2 then
			versionPos = InstrRev(strLine, " ", -1, 1)
			name = Left(strLine, versionPos-1)
			if name = PrimaryName Then
				version = Right(strLine, Len(strLine) - versionPos)
				Wscript.Echo "Sound name: " & name & " Version " & version
				URL = BaseURL & "-" & name & "-" & Freq &"-" & version & ".tar.gz"
				Wscript.Echo "URL: " & URL
				WgetUnCompress URL, Showpath(DestFolder)
			End If
		End if
	Loop
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
	Dim tries
	Dim max_tries
	Dim MyFile

	tries = 0
	max_tries = 10
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	If Not FSO.FileExists(DestFolder & "7za.exe") Then
		On Error Resume Next
		Set MyFile = FSO.CreateTextFile(DestFolder & "7za.tag", False)
		If Err <> 0 Then Wscript.echo("Downloading 7za: " & DestFolder & "7za.tag - " & Err.Description) End If
		On Error Goto 0
		If Not IsEmpty(MyFile) Then
			MyFile.WriteLine("This file marks a pending download for 7za.exe so we don't download it twice at the same time")
			MyFile.Close
			Wget ToolsBase & "7za.exe", DestFolder
			FSO.DeleteFile DestFolder & "7za.tag", true
			Wscript.echo("Downloaded 7za.exe")
		End If
		Set MyFile = Nothing
		WScript.Sleep(500)
	End If
	Do While FSO.FileExists(DestFolder & "7za.tag") And tries < max_tries
		Wscript.echo("Waiting for 7za.exe to be downloaded")
		WScript.Sleep(1000)
		tries = tries + 1
	Loop
	If tries = max_tries Then
		Wscript.echo("ERROR: Download of 7za.exe takes too much time")
		Wscript.quit 99
	End If
End Sub

Sub GetWgetEXE(DestFolder)
	Dim oExec
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If
	If Not FSO.FileExists(DestFolder & "wget.exe") Then
		Slow_Wget ToolsBase & "wget.exe", DestFolder
	End If
End Sub

Function Strip(Str)
	Set oRE = New Regexp
	oRE.Pattern = "[\W_]"
	oRE.Global = True
	Strip = Right(oRE.Replace(Str, ""), 20)
End Function

Sub ExecPrintOutput(Str)
	Dim Process

	Set Process = WshShell.Exec(Str)

	Do
		If Not Process.StdOut.atEndOfStream Then
			WScript.Echo Process.StdOut.ReadLine()
		End If
	Loop Until (Process.Status <> 0) And (Process.StdOut.atEndOfStream)

	Process.Terminate
	WScript.Sleep(500)
End Sub

Sub UnCompress(Archive, DestFolder)
	Dim Fn
	Dim batname
	Dim MyFile

	GetCompressionTools UtilsDir

	batname = "tmp" & Strip(Archive) & CStr(Int(10000*Rnd)) & ".bat"
	wscript.echo("Extracting: " & Archive & " - using: " & batname)
	Set MyFile = FSO.CreateTextFile(UtilsDir & batname, True)
	MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Archive & quote & " -y -o" & quote & DestFolder & quote )
	MyFile.Close
	Set MyFile = Nothing
	ExecPrintOutput(UtilsDir & batname)
	wscript.echo("Ready extracting: " & Archive)
	Fn = Left(Archive, Len(Archive)-3)
	If FSO.FileExists(Fn) Then
		Set MyFile = FSO.CreateTextFile(UtilsDir & batname, True)
		MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Fn & quote & " -y -o" & quote & DestFolder & quote )
		MyFile.Close
		Set MyFile = Nothing
		ExecPrintOutput(UtilsDir & batname)
		wscript.echo("Ready extracting: " & Fn)
		wscript.echo("Deleting: " & Fn)
		FSO.DeleteFile Fn,true
	End If
	Fn = Fn & tar
	If FSO.FileExists(Fn) Then
		Set MyFile = FSO.CreateTextFile(UtilsDir & batname, True)
		MyFile.WriteLine("@" & quote & UtilsDir & "7za.exe" & quote & " x " & quote & Fn & quote & " -y -o" & quote & DestFolder & quote )
		MyFile.Close
		Set MyFile = Nothing
		ExecPrintOutput(UtilsDir & batname)
		wscript.echo("Ready extracting: " & Fn )
		wscript.echo("Deleting: " & Fn)
		FSO.DeleteFile Fn,true
	End If

	WScript.Sleep(500)
	If FSO.FileExists(UtilsDir & batname) Then
		FSO.DeleteFile UtilsDir & batname, True
	End If
End Sub

Sub Wget(URL, DestFolder)
	Dim MyFile

	StartPos = InstrRev(URL, "/", -1, 1)
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If

	If UseWgetEXE Then
		Wscript.echo("wget: " & URL)
		batname = "tmp" & CStr(Int(10000*Rnd)) & ".bat"
		Set MyFile = FSO.CreateTextFile(UtilsDir & batname, True)
		MyFile.WriteLine("@cd " & quote & DestFolder & quote)
		MyFile.WriteLine("@" & quote & UtilsDir & "wget.exe" & quote & " " & URL)
		MyFile.Close
		Set MyFile = Nothing
		ExecPrintOutput(UtilsDir & batname)
	Else
		Wscript.echo("XMLHTTP GET: " & URL)

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
	End If

End Sub

Sub Slow_Wget(URL, DestFolder)
	Dim MyFile

	StartPos = InstrRev(URL, "/", -1, 1)
	strlength = Len(URL)
	filename=Right(URL,strlength-StartPos)
	If Right(DestFolder, 1) <> "\" Then DestFolder = DestFolder & "\" End If

	Wscript.echo("Downloading: " & URL)
	xml.Open "GET", URL, False
	xml.Send

	const ForReading = 1 , ForWriting = 2 , ForAppending = 8
	Set MyFile = FSO.OpenTextFile(DestFolder & filename ,ForWriting, True)
	For i = 1 to lenb(xml.responseBody)
		MyFile.write Chr(Ascb(midb(xml.responseBody,i,1)))
	Next
	MyFile.Close()
	Set MyFile = Nothing

End Sub

Function Showpath(folderspec)
	Set f = FSO.GetFolder(folderspec)
	showpath = f.path & "\"
End Function


Function FindVersionStringInConfigure(strConfigFile, strVersionString)

	Set objRegEx = CreateObject("VBScript.RegExp")
	objRegEx.Pattern = "[^#]AC_SUBST\(" & strVersionString & ".*\[([^\[]*)\]"

	Set objFSO = CreateObject("Scripting.FileSystemObject")
	Set objFile = objFSO.OpenTextFile(strConfigFile, 1)
	strSearchString = objFile.ReadAll
	objFile.Close

	Set colMatches = objRegEx.Execute(strSearchString)

	strResult = ""
	If colMatches.Count > 0 Then
		For Each strMatch in colMatches
			strResult = objRegEx.Replace(strMatch.Value, "$1")
		Next
	End If

	FindVersionStringInConfigure = strResult

End Function

Sub FindReplaceInFile(FileName, sFind, sReplace)
	Const OpenAsASCII = 0  ' Opens the file as ASCII (TristateFalse)
	Const OpenAsUnicode = -1  ' Opens the file as Unicode (TristateTrue)
	Const OpenAsDefault = -2  ' Opens the file using the system default

	Const OverwriteIfExist = -1
	Const FailIfNotExist   =  0
	Const ForReading       =  1

	Set fOrgFile = FSO.OpenTextFile(FileName, ForReading, FailIfNotExist, OpenAsASCII)
'	Wscript.echo("FindReplaceInFile: " & FileName & " s/" & sFind & "/" & sReplace)
	sText = fOrgFile.ReadAll
	fOrgFile.Close
	sText = Replace(sText, sFind, sReplace)
	Set fNewFile = FSO.CreateTextFile(FileName, OverwriteIfExist, OpenAsASCII)
	fNewFile.WriteLine sText
	fNewFile.Close
End Sub

Function ExecAndGetResult(tmpFolder, VersionDir, execStr)
	Dim MyFile

	Set MyFile = FSO.CreateTextFile(tmpFolder & "tmpExec.Bat", True)
	MyFile.WriteLine("@" & "cd " & quote & VersionDir & quote)
	MyFile.WriteLine("@" & execStr)
	MyFile.Close
	Set MyFile = Nothing

	Set oExec = WshShell.Exec("cmd /C " & quote & tmpFolder & "tmpExec.Bat" & quote)

	ExecAndGetResult = Trim(OExec.StdOut.ReadLine())

	Do
	Loop Until (oExec.Status <> 0) And (oExec.StdOut.atEndOfStream)

	oExec.Terminate
	WScript.Sleep(500)

	FSO.DeleteFile(tmpFolder & "tmpExec.Bat")

End Function

Function ExecAndGetExitCode(tmpFolder, VersionDir, execStr)
	Dim MyFile

	Set MyFile = FSO.CreateTextFile(tmpFolder & "tmpExec.Bat", True)
	MyFile.WriteLine("@" & "cd " & quote & VersionDir & quote)
	MyFile.WriteLine("@" & execStr)
	MyFile.WriteLine("@exit %ERRORLEVEL%")
	MyFile.Close
	Set MyFile = Nothing

	ExecAndGetExitCode = WshShell.Run("cmd /C " & quote & tmpFolder & "tmpExec.Bat" & quote, 0, True)

	FSO.DeleteFile(tmpFolder & "tmpExec.Bat")

End Function

Function pd(n, totalDigits)
	If totalDigits > len(n) then
		pd = String(totalDigits-len(n),"0") & n
	Else
		pd = n
	End If
End Function

Function GetTimeUTC()

	iOffset = WshShell.RegRead("HKLM\System\CurrentControlSet\Control\TimeZoneInformation\ActiveTimeBias")

	If IsNumeric(iOffset) Then
		GetTimeUTC = DateAdd("n", iOffset, Now())
	Else
		GetTimeUTC = Now()
	End If

End Function

Sub CreateVersion(tmpFolder, VersionDir, includebase, includedest)
	Dim oExec

	Wscript.echo("Checking if we're building a newer git version")

	strVerMajor = FindVersionStringInConfigure(VersionDir & "configure.in", "SWITCH_VERSION_MAJOR")
	strVerMinor = FindVersionStringInConfigure(VersionDir & "configure.in", "SWITCH_VERSION_MINOR")
	strVerMicro = FindVersionStringInConfigure(VersionDir & "configure.in", "SWITCH_VERSION_MICRO")
	strVerRev   = FindVersionStringInConfigure(VersionDir & "configure.in", "SWITCH_VERSION_REVISION")
	strVerHuman = FindVersionStringInConfigure(VersionDir & "configure.in", "SWITCH_VERSION_REVISION_HUMAN")

	'Set version to the one reported by configure.in
	If strVerRev <> "" Then
		VERSION = strVerRev
	End If

	Dim sLastFile
	Const ForReading = 1
	Const ShowUnclean = False 'Don't show unclean state for now

	'Try To read revision from git
	If FSO.FolderExists(VersionDir & ".git") Then
		'Get timestamp for last commit
		strFromProc = ExecAndGetResult(tmpFolder, VersionDir, "git log -n1 --format=" & quote & "%%ct" & quote & " HEAD")
		If IsNumeric(strFromProc) Then
			lastChangedDateTime = DateAdd("s", strFromProc, "01/01/1970 00:00:00")
			strLastCommit = YEAR(lastChangedDateTime) & Pd(Month(lastChangedDateTime), 2) & Pd(DAY(lastChangedDateTime), 2) & "T" & Pd(Hour(lastChangedDateTime), 2) & Pd(Minute(lastChangedDateTime), 2) & Pd(Second(lastChangedDateTime), 2) & "Z"
			strLastCommitHuman = YEAR(lastChangedDateTime) & "-" & Pd(Month(lastChangedDateTime), 2) & "-" & Pd(DAY(lastChangedDateTime), 2) & " " & Pd(Hour(lastChangedDateTime), 2) & ":" & Pd(Minute(lastChangedDateTime), 2) & ":" & Pd(Second(lastChangedDateTime), 2) & "Z"
		Else
			strLastCommit = ""
			strLastCommitHuman = ""
		End If

		'Get revision hash
		strRevision = ExecAndGetResult(tmpFolder, VersionDir, "git rev-list -n1 --abbrev=10 --abbrev-commit HEAD")
		strRevisionHuman = ExecAndGetResult(tmpFolder, VersionDir, "git rev-list -n1 --abbrev=7 --abbrev-commit HEAD")

		If strLastCommit <> "" And strLastCommitHuman <> "" And strRevision <> "" And strRevisionHuman <> "" Then
			'Build version string
			strGitVer = "+git~" & strLastCommit & "~" & strRevision
			strVerHuman = "git " & strRevisionHuman & " " & strLastCommitHuman

			'Check for local changes, if found, append to git revision string
			If ShowUnclean Then
				If ExecAndGetExitCode(tmpFolder, VersionDir, "git diff-index --quiet HEAD") <> 0 Then
					lastChangedDateTime = GetTimeUTC()
					strGitVer = strGitVer & "+unclean~" & YEAR(lastChangedDateTime) & Pd(Month(lastChangedDateTime), 2) & Pd(DAY(lastChangedDateTime), 2) & "T" & Pd(Hour(lastChangedDateTime), 2) & Pd(Minute(lastChangedDateTime), 2) & Pd(Second(lastChangedDateTime), 2) & "Z"
					strVerHuman = strVerHuman & " unclean " & YEAR(lastChangedDateTime) & "-" & Pd(Month(lastChangedDateTime), 2) & "-" & Pd(DAY(lastChangedDateTime), 2) & " " & Pd(Hour(lastChangedDateTime), 2) & ":" & Pd(Minute(lastChangedDateTime), 2) & ":" & Pd(Second(lastChangedDateTime), 2) & "Z"
				End If
			End If
		Else
			strGitVer = ""
			strVerHuman = ""
		End If

		VERSION=VERSION & strGitVer

		sLastVersion = ""
		Set sLastFile = FSO.OpenTextFile(tmpFolder & "lastversion", ForReading, True, OpenAsASCII)
		If Not sLastFile.atEndOfStream Then
			sLastVersion = sLastFile.ReadLine()
		End If
		sLastFile.Close
		Set sLastFile = Nothing
	End If

	Source = "source code"
	If FSO.GetExtensionName(includedest) <> "inc" Then
		Dim IncFn

		IncFn = VersionDir & "\w32\Library\switch_version.inc"
		If FSO.FileExists(IncFn) Then
			Wscript.echo("CreateVersion: deleting - " & IncFn)
			FSO.DeleteFile IncFn
		End If
	Else
		Source = "resource files"
	End If

	sNewVersion = VERSION & " " & strVerHuman
	If sNewVersion <> sLastVersion Or not FSO.FileExists(includedest) Then
		Dim MyFile

		Set MyFile = FSO.CreateTextFile(tmpFolder & "lastversion", True)
		MyFile.WriteLine(sNewVersion)
		MyFile.Close
		Set MyFile = Nothing

		Wscript.echo("Updating " & Source & " from " & sLastVersion & " to " & sNewVersion)

		FSO.CopyFile includebase, includedest, true
		FindReplaceInFile includedest, "@SWITCH_VERSION_REVISION@", VERSION
		FindReplaceInFile includedest, "@SWITCH_VERSION_MAJOR@", strVerMajor
		FindReplaceInFile includedest, "@SWITCH_VERSION_MINOR@", strVerMinor
		FindReplaceInFile includedest, "@SWITCH_VERSION_MICRO@", strVerMicro
		FindReplaceInFile includedest, "@SWITCH_VERSION_REVISION_HUMAN@", strVerHuman
		FindReplaceInFile includedest, "@SWITCH_VERSION_YEAR@", YEAR(lastChangedDateTime)
	End If

End Sub

