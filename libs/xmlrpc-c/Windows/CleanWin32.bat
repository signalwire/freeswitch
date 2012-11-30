@if NOT EXIST ..\include\xmlrpc-c\config.h goto DN1
del ..\include\xmlrpc-c\config.h > nul
@set TEMP1=%TEMP1% ..\include\xmlrpc-c\config.h
:DN1
@if NOT EXIST ..\xmlrpc_config.h goto DN2
del ..\xmlrpc_config.h > nul
@set TEMP1=%TEMP1% ..\xmlrpc_config.h
:DN2
@if NOT EXIST ..\transport_config.h goto DN3
del ..\transport_config.h > nul
@set TEMP1=%TEMP1% ..\transport_config.h
:DN3
@if NOT EXIST ..\version.h goto DN4
del ..\version.h > nul
@set TEMP1=%TEMP1% ..\version.h
:DN4
@if NOT EXIST ..\examples\config.h goto DN5
del ..\examples\config.h > nul
@set TEMP1=%TEMP1% ..\examples\config.h
:DN5
@if "%TEMP1%." == "." goto ALLDN
@echo DELETED win32 header files.
@echo %TEMP1%
@goto END

:ALLDN
@echo NOne to DELETE ...
@goto END

:END
