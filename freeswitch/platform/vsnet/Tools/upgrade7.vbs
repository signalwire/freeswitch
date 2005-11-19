Set vcProj = CreateObject("VisualStudio.VCProjectEngine.7.1")
Set objFile = Createobject("Scripting.FileSystemObject")
Set objArgs = WScript.Arguments

'// check the arguments to be sure it's right
if (objArgs.Count() < 2) Then
   WScript.Echo("VC6 or 5 DSP Project File Conversion")
   WScript.Echo("Opens specified .dsp and converts to VC7.1 Format.")
   WScript.Echo("Will create project file with .vcproj extension")
   WScript.Echo("usage: <full path\project.dsp> <full path\project.vcproj>")
   WScript.Quit(1)
End If
WScript.Echo("Converting: "+ objArgs.Item(0))
'// If there is a file name of the .vcproj extension, do not convert
Set vcProject = vcProj.LoadProject(objArgs.Item(0))
If Not objFile.FileExists(vcProject.ProjectFile) Then
'   // specify name and location of new project file
vcProject.ProjectFile = objArgs.Item(1)

'   // call the project engine to save this off. 
'   // when no name is shown, it will create one with the .vcproj name
vcProject.Save()
WScript.Echo("New Project Name: "+vcProject.ProjectFile+"")

else

   WScript.Echo("ERROR!: "+vcProject.ProjectFile+" already exists!")
End If
