Const OpenAsASCII = 0  ' Opens the file as ASCII (TristateFalse) 
Const OpenAsUnicode = -1  ' Opens the file as Unicode (TristateTrue) 
Const OpenAsDefault = -2  ' Opens the file using the system default 

Const OverwriteIfExist = -1 
Const FailIfNotExist   =  0 
Const ForReading       =  1 
Set objArgs = WScript.Arguments

' path to original log file 
sFileName = objargs(0) 

Set oFSO = CreateObject("Scripting.FileSystemObject")
Set fOrgFile = oFSO.OpenTextFile(sFileName, ForReading, FailIfNotExist, OpenAsASCII)
sText = fOrgFile.ReadAll
fOrgFile.Close
sText = Replace(sText, vbLf, vbCrLf)
Set fNewFile = oFSO.CreateTextFile(sFileName, OverwriteIfExist, OpenAsASCII)
fNewFile.WriteLine sText
fNewFile.Close
