@mingw32-make -f Makefile.mingw %1
@if errorlevel 1 goto end
@strip upnpc-static.exe
@upx --best upnpc-static.exe
@strip upnpc-shared.exe
@upx --best upnpc-shared.exe
:end
