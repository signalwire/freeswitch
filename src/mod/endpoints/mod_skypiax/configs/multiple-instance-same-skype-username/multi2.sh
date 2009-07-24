
#start the fake X server on a given port
 /usr/bin/Xvfb :121 -ac &
sleep 3

# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the 'username password' you send to it on stdin. Here xxxx would be the password and skypiax2 the username
su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:121  /usr/bin/skype --dbpath=/root/multi/skypiax121 --pipelogin &"


sleep 7


 /usr/bin/Xvfb :122 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:122  /usr/bin/skype --dbpath=/root/multi/skypiax122 --pipelogin &"

sleep 7


#################################################################
# Following X server Skype client instances are commented out
#################################################################
 /usr/bin/Xvfb :123 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:123  /usr/bin/skype --dbpath=/root/multi/skypiax123 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :124 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:124  /usr/bin/skype --dbpath=/root/multi/skypiax124 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :125 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:125  /usr/bin/skype --dbpath=/root/multi/skypiax125 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :126 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:126  /usr/bin/skype --dbpath=/root/multi/skypiax126 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :127 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:127  /usr/bin/skype --dbpath=/root/multi/skypiax127 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :128 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:128  /usr/bin/skype --dbpath=/root/multi/skypiax128 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :129 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:129  /usr/bin/skype --dbpath=/root/multi/skypiax129 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :130 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:130  /usr/bin/skype --dbpath=/root/multi/skypiax130 --pipelogin &"

sleep 7

exit 0

 /usr/bin/Xvfb :131 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:131  /usr/bin/skype --dbpath=/root/multi/skypiax131 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :132 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:132  /usr/bin/skype --dbpath=/root/multi/skypiax132 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :133 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:133  /usr/bin/skype --dbpath=/root/multi/skypiax133 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :134 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:134  /usr/bin/skype --dbpath=/root/multi/skypiax134 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :135 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:135  /usr/bin/skype --dbpath=/root/multi/skypiax135 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :136 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:136  /usr/bin/skype --dbpath=/root/multi/skypiax136 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :137 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:137  /usr/bin/skype --dbpath=/root/multi/skypiax137 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :138 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:138  /usr/bin/skype --dbpath=/root/multi/skypiax138 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :139 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:139  /usr/bin/skype --dbpath=/root/multi/skypiax139 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :140 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax2 xxxx'| DISPLAY=:140  /usr/bin/skype --dbpath=/root/multi/skypiax140 --pipelogin &"

sleep 7

