#!/bin/bash
# remember to add here the removing of all the installed snd-* modules, so you're sure only the snd-dummy driver will be around
rmmod snd_hda_intel
rmmod snd-dummy # enable=1,1,1

# you need three dummy soundcard for 20 Skype client instances, because each dummy soundcard can handle a max of 8 Skype instances
# the enable= module parameter tells how many cards to start. For each additional card, add a comma and a 1
# manually configure the first 8 Skype client instances to use the hw:Dummy_0, the next 8 instances to use hw:Dummy_1, etc for all three devices (Play, Capture, Ring)
modprobe snd-dummy 
#modprobe snd-aloop enable=1,1,1
sleep 3

#start the fake X server on a given port
 /usr/bin/Xvfb :101 -screen 0 800x600x16 -nolisten tcp -ac &
sleep 3

# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the 'username password' you send to it on stdin. Here passwd5 would be the password and user5 the username
su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:101  /usr/bin/skype --dbpath=/root/multi/skypopen101 --pipelogin &"


sleep 7


 /usr/bin/Xvfb :102 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:102  /usr/bin/skype --dbpath=/root/multi/skypopen102 --pipelogin &"

sleep 7

exit 0

#################################################################
# Following X server and Skype client instances are commented out
#################################################################
 /usr/bin/Xvfb :103 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:103  /usr/bin/skype --dbpath=/root/multi/skypopen103 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :104 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:104  /usr/bin/skype --dbpath=/root/multi/skypopen104 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :105 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:105  /usr/bin/skype --dbpath=/root/multi/skypopen105 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :106 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:106  /usr/bin/skype --dbpath=/root/multi/skypopen106 --pipelogin &"


sleep 7
 /usr/bin/Xvfb :107 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:107  /usr/bin/skype --dbpath=/root/multi/skypopen107 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :108 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:108  /usr/bin/skype --dbpath=/root/multi/skypopen108 --pipelogin &"

sleep 7


 /usr/bin/Xvfb :109 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:109  /usr/bin/skype --dbpath=/root/multi/skypopen109 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :110 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:110  /usr/bin/skype --dbpath=/root/multi/skypopen110 --pipelogin &"

sleep 7


 /usr/bin/Xvfb :111 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:111  /usr/bin/skype --dbpath=/root/multi/skypopen111 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :112 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:112  /usr/bin/skype --dbpath=/root/multi/skypopen112 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :113 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:113  /usr/bin/skype --dbpath=/root/multi/skypopen113 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :114 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:114  /usr/bin/skype --dbpath=/root/multi/skypopen114 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :115 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:115  /usr/bin/skype --dbpath=/root/multi/skypopen115 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :116 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:116  /usr/bin/skype --dbpath=/root/multi/skypopen116 --pipelogin &"

sleep 7


 /usr/bin/Xvfb :117 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:117  /usr/bin/skype --dbpath=/root/multi/skypopen117 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :118 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:118  /usr/bin/skype --dbpath=/root/multi/skypopen118 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :119 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:119  /usr/bin/skype --dbpath=/root/multi/skypopen119 --pipelogin &"

sleep 7
 /usr/bin/Xvfb :120 -screen 0 800x600x16 -nolisten tcp -ac & 
sleep 3

su root -c "/bin/echo 'user5 passwd5'| DISPLAY=:120  /usr/bin/skype --dbpath=/root/multi/skypopen120 --pipelogin &"

sleep 7

exit 0
