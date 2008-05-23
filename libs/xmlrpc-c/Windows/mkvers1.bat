@if "%1." == "." goto ERR2
@if "%TEMP1%." == "." goto ERR1
@if "%TEMP1%" == "1" goto SET1
@if "%TEMP1%" == "2" goto SET2
@if "%TEMP1%" == "3" goto SET3
@echo environment variable has an invalid value %TEMP1% ...
@goto ERR2

:SET1
@set TEMPX1=%1
@set TEMP1=2
@goto END

:SET2
@set TEMPX2=%1
@set TEMP1=3
@goto END

:SET3
@set TEMPX3=%1
@set TEMP1=4
@goto END


:ERR1
@echo Environment variable TEMP1 not set ...
:ERR2
@echo This batch is only intended to be run from within UPDVERS.BAT ...
@pause
@goto END

:END
