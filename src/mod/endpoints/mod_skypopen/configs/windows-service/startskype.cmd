echo off

REM
REM you MUST use the new Skype (4.x) for Windows, older versions (3.x) cannot be started this way
REM
REM you have to adjust PATH to where the Skype executable is
set PATH=%PATH%;C:\Program Files\Skype\Phone\

REM echo %PATH%

REM start a Skype client instance that will login to the Skype network using the "username password" you give to it. 
REM Here xxxx would be the password and user20 the username

start Skype.exe /secondary /username:user20 /password:xxxx
call C:\wait.cmd 20

start Skype.exe /secondary /username:user19 /password:xxxx
call C:\wait.cmd 5

REM 
REM Following Skype client instances are commented out
REM

REM start Skype.exe /secondary /username:user18 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user17 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user16 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user15 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user14 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user13 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user12 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user11 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user10 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user9 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user8 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user7 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user6 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user5 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user4 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user3 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user2 /password:xxxx
REM call C:\wait.cmd 5
REM start Skype.exe /secondary /username:user1 /password:xxxx
call C:\wait.cmd 120
NET start AICCU2
pause 
