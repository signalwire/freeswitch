cd %1 
ping 127.0.0.1 -n 5 -w 1000 > nul
NET START wampmysqld
wamp\bin\mysql\mysql5.1.36\bin\mysql --user=root <create_freepbx.sql