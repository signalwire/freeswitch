# remember to add here the removing of all the installed snd-* modules, so you're sure only the snd-dummy driver will be around
rmmod snd_hda_intel
rmmod snd-dummy 

# if you DO NOT USE the custom ALSA device we provide, you need three "standard" dummy soundcard for 20 Skype client instances, because each "standard" dummy soundcard can handle a max of 8 Skype instances
# the enable= module parameter tells how many cards to start. For each additional card, add a comma and a 1
# manually configure the first 8 Skype client instances to use the hw:Dummy_0, the next 8 instances to use hw:Dummy_1, etc for all three devices (Play, Capture, Ring)
modprobe snd-dummy # enable=1,1,1
sleep 3

#start the fake X server on a given port
 /usr/bin/Xvfb :101 -ac &
sleep 3

# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the 'username password' you send to it on stdin. Here xxxx would be the password and skypiax1 the username
su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:101  /usr/bin/skype --dbpath=/root/multi/skypiax101 --pipelogin &"


sleep 7


 /usr/bin/Xvfb :102 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:102  /usr/bin/skype --dbpath=/root/multi/skypiax102 --pipelogin &"

sleep 7


#################################################################
# Following X server Skype client instances are commented out
#################################################################
 /usr/bin/Xvfb :103 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:103  /usr/bin/skype --dbpath=/root/multi/skypiax103 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :104 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:104  /usr/bin/skype --dbpath=/root/multi/skypiax104 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :105 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:105  /usr/bin/skype --dbpath=/root/multi/skypiax105 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :106 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:106  /usr/bin/skype --dbpath=/root/multi/skypiax106 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :107 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:107  /usr/bin/skype --dbpath=/root/multi/skypiax107 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :108 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:108  /usr/bin/skype --dbpath=/root/multi/skypiax108 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :109 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:109  /usr/bin/skype --dbpath=/root/multi/skypiax109 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :110 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:110  /usr/bin/skype --dbpath=/root/multi/skypiax110 --pipelogin &"

sleep 7

exit 0

 /usr/bin/Xvfb :111 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:111  /usr/bin/skype --dbpath=/root/multi/skypiax111 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :112 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:112  /usr/bin/skype --dbpath=/root/multi/skypiax112 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :113 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:113  /usr/bin/skype --dbpath=/root/multi/skypiax113 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :114 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:114  /usr/bin/skype --dbpath=/root/multi/skypiax114 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :115 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:115  /usr/bin/skype --dbpath=/root/multi/skypiax115 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :116 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:116  /usr/bin/skype --dbpath=/root/multi/skypiax116 --pipelogin &"

sleep 7

 /usr/bin/Xvfb :117 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:117  /usr/bin/skype --dbpath=/root/multi/skypiax117 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :118 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:118  /usr/bin/skype --dbpath=/root/multi/skypiax118 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :119 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:119  /usr/bin/skype --dbpath=/root/multi/skypiax119 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :120 -ac & 
sleep 3

su root -c "/bin/echo 'skypiax1 xxxx'| DISPLAY=:120  /usr/bin/skype --dbpath=/root/multi/skypiax120 --pipelogin &"

sleep 7

