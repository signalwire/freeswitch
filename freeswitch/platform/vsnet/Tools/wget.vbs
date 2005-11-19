Set WshShell = WScript.CreateObject("WScript.Shell")
Set objArgs = WScript.Arguments

StartPos = InstrRev(objargs(0), "/", -1, 1)   
strlength = Len(objargs(0))

If objArgs.Count > 1 Then
	Path= Showpath(objargs(1))
Else
	Path= Showpath(".")
End If
Wget objargs(0), Path & "\", Right(objargs(0),strlength-StartPos)


Sub Wget(URL, DestFolder, Imagefile)

Set xml = CreateObject("Microsoft.XMLHTTP")
xml.Open "GET", URL, False
xml.Send

set oStream = createobject("Adodb.Stream")
Const adTypeBinary = 1
Const adSaveCreateOverWrite = 2
Const adSaveCreateNotExist = 1 

oStream.type = adTypeBinary
oStream.open
oStream.write xml.responseBody

' Do not overwrite an existing file
oStream.savetofile DestFolder & ImageFile, adSaveCreateNotExist

' Use this form to overwrite a file if it already exists
' oStream.savetofile DestFolder & ImageFile, adSaveCreateOverWrite

oStream.close

set oStream = nothing
Set xml = Nothing
End Sub

Function Showpath(folderspec)
   Dim fso, f
   Set fso = CreateObject("Scripting.FileSystemObject")
   Set f = fso.GetFolder(folderspec)
   showpath = f.path
End Function