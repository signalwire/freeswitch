Set WshShell = WScript.CreateObject("WScript.Shell")
Set objArgs = WScript.Arguments

'system, user, or process
wscript.echo Showpath(objargs(0))
 
Function Showpath(folderspec)
   Dim fso, f
   Set fso = CreateObject("Scripting.FileSystemObject")
   Set f = fso.GetFolder(folderspec)
   showpath = f.path
End Function