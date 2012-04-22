REM would you believe there is no sleep() in standard windows batchfiles?
@ping 127.0.0.1 -n 2 -w 1000 > nul
@ping 127.0.0.1 -n %1% -w 1000> nul

