echo off
REM
REM you MUST use the new Skype BETA (4.x) for Windows, older versions (3.x) cannot be started this way
REM
REM you have to adjust PATH to where the Skype executable is
set PATH=%PATH%;C:\Program Files\Skype\Phone\

echo %PATH%

REM start a Skype client instance that will login to the Skype network using the "username password" you give to it. Here xxx would be the password and skypiax1 the username
start Skype.exe /secondary /username:skypiax1 /password:xxx
call wait 7
start Skype.exe /secondary /username:skypiax2 /password:xxx
call wait 7
REM 
REM Following Skype client instances are commented out
REM
REM start Skype.exe /secondary /username:skypiax3 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax4 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax5 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax6 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax7 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax8 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax9 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax10 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax11 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax12 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax13 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax14 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax15 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax16 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax17 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax18 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax19 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:skypiax20 /password:xxx
