echo off
REM
REM you MUST use the new Skype (4.x) for Windows, older versions (3.x) cannot be started this way
REM
REM you have to adjust PATH to where the Skype executable is
set PATH=%PATH%;C:\Program Files\Skype\Phone\

echo %PATH%

REM start a Skype client instance that will login to the Skype network using the "username password" you give to it. Here xxx would be the password and user1 the username
start Skype.exe /secondary /username:user1 /password:xxx
call wait 7
start Skype.exe /secondary /username:user2 /password:xxx
call wait 7
REM 
REM Following Skype client instances are commented out
REM
REM start Skype.exe /secondary /username:user3 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user4 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user5 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user6 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user7 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user8 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user9 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user10 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user11 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user12 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user13 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user14 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user15 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user16 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user17 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user18 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user19 /password:xxx
REM call wait 7
REM start Skype.exe /secondary /username:user20 /password:xxx
