@echo This batch file requires a powerful XDELETE program. One
@echo that will REMOVE whole directories recursively ...
@echo If you do NOT have such a program, then abort now, and
@echo adjust the line below ...
@set TEMPX=xdelete -dfrm
@echo set TEMPX=%TEMPX%
@pause
@echo #####################################################
@echo ARE YOU SURE YOU WANT TO DO THIS? Ctrl+C to abort ...
@echo #####################################################
@pause
@echo CleanAll: Last chance ... ctrl+c to abort ...
@pause
@echo CleanAll: Cleaning the headers ...
call CleanWin32
@echo CleanAll: and removing the SOLUTION files ...
call delsln
@echo CleanAll: Cleaning the gennmtab generated header ...
@if EXIST ..\lib\expat\xmltok\nametab.h del ..\lib\expat\xmltok\nametab.h > nul
@echo CleanAll: Cleaning all built binaries ...
@if EXIST ..\bin\*.exe del ..\bin\*.exe > nul
@if EXIST ..\bin\*.exp del ..\bin\*.exp > nul
@if EXIST ..\bin\*.ilk del ..\bin\*.ilk > nul
@if EXIST ..\bin\*.lib del ..\bin\*.lib > nul
@if EXIST ..\lib\*.lib del ..\lib\*.lib > nul
@if EXIST ..\lib\*.dll del ..\lib\*.dll > nul
@echo CleanAll: Cleaning test data files ...
@if EXIST ..\bin\data\*.xml del ..\bin\data\*.xml > nul
@if EXIST ..\bin\data\. rd ..\bin\data > nul
@if EXIST ..\bin\. rd ..\bin > nul
@echo CleanAll: Cleaning old residual built binaries ... but none should exist ...
@if EXIST ..\lib\expat\gennmtab\Debug\. %TEMPX% ..\lib\expat\gennmtab\Debug
@if EXIST ..\lib\expat\gennmtab\Release\. %TEMPX% ..\lib\expat\gennmtab\Release
@if EXIST ..\lib\expat\xmlparse\Debug\. %TEMPX% ..\lib\expat\xmlparse\Debug
@if EXIST ..\lib\expat\xmlparse\DebugDLL\. %TEMPX% ..\lib\expat\xmlparse\DebugDLL
@if EXIST ..\lib\expat\xmlparse\Release\. %TEMPX% ..\lib\expat\xmlparse\Release
@if EXIST ..\lib\expat\xmlparse\ReleaseDLL\. %TEMPX% ..\lib\expat\xmlparse\ReleaseDLL
@if EXIST ..\lib\expat\xmlparse\ReleaseMinSizeDLL\. %TEMPX% ..\lib\expat\xmlparse\ReleaseMinSizeDLL
@if EXIST ..\lib\expat\xmltok\Debug\. %TEMPX% ..\lib\expat\xmltok\Debug
@if EXIST ..\lib\expat\xmltok\DebugDLL\. %TEMPX% ..\lib\expat\xmltok\DebugDLL
@if EXIST ..\lib\expat\xmltok\Release\. %TEMPX% ..\lib\expat\xmltok\Release
@if EXIST ..\lib\expat\xmltok\ReleaseDLL\. %TEMPX% ..\lib\expat\xmltok\ReleaseDLL
@echo CleanAll: Finally, cleaning the main intermediate directories ...
@if EXIST Debug\. %TEMPX% Debug
@if EXIST Release\. %TEMPX% Release
@echo .
@echo CleanAll: Phew ... all done ...
@echo .
