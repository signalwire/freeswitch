echo %0
md conf
xcopy -D ..\..\..\conf\*.* conf
cd mod
md temp
move *.ilk temp
cd ..
FreeSwitch.exe 
cd mod
copy .\temp\*.ilk
del .\temp\*.ilk
rd temp
pause