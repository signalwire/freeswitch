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

# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the 'username password' you send to it on stdin. Here xxxx would be the password and user1 the username
su root -c "/bin/echo 'user1 xxxx'| DISPLAY=:101  /usr/bin/skype --pipelogin &"


sleep 7


 /usr/bin/Xvfb :102 -ac & 
sleep 3

su root -c "/bin/echo 'user2 xxxx'| DISPLAY=:102  /usr/bin/skype --pipelogin &"

sleep 7

exit 0

##################################################################################
# Following X server and Skype client instances are NOT LAUNCHED (see line before)
##################################################################################

 /usr/bin/Xvfb :103 -ac & 
sleep 3

su root -c "/bin/echo 'user3 xxxx'| DISPLAY=:103  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :104 -ac & 
sleep 3

su root -c "/bin/echo 'user4 xxxx'| DISPLAY=:104  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :105 -ac & 
sleep 3

su root -c "/bin/echo 'user5 xxxx'| DISPLAY=:105  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :106 -ac & 
sleep 3

su root -c "/bin/echo 'user6 xxxx'| DISPLAY=:106  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :107 -ac & 
sleep 3

su root -c "/bin/echo 'user7 xxxx'| DISPLAY=:107  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :108 -ac & 
sleep 3

su root -c "/bin/echo 'user8 xxxx'| DISPLAY=:108  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :109 -ac & 
sleep 3

su root -c "/bin/echo 'user9 xxxx'| DISPLAY=:109  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :110 -ac & 
sleep 3

su root -c "/bin/echo 'user10 xxxx'| DISPLAY=:110  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :111 -ac & 
sleep 3

su root -c "/bin/echo 'user11 xxxx'| DISPLAY=:111  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :112 -ac & 
sleep 3

su root -c "/bin/echo 'user12 xxxx'| DISPLAY=:112  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :113 -ac & 
sleep 3

su root -c "/bin/echo 'user13 xxxx'| DISPLAY=:113  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :114 -ac & 
sleep 3

su root -c "/bin/echo 'user14 xxxx'| DISPLAY=:114  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :115 -ac & 
sleep 3

su root -c "/bin/echo 'user15 xxxx'| DISPLAY=:115  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :116 -ac & 
sleep 3

su root -c "/bin/echo 'user16 xxxx'| DISPLAY=:116  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :117 -ac & 
sleep 3

su root -c "/bin/echo 'user17 xxxx'| DISPLAY=:117  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :118 -ac & 
sleep 3

su root -c "/bin/echo 'user18 xxxx'| DISPLAY=:118  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :119 -ac & 
sleep 3

su root -c "/bin/echo 'user19 xxxx'| DISPLAY=:119  /usr/bin/skype --pipelogin &"

sleep 7
 /usr/bin/Xvfb :120 -ac & 
sleep 3

su root -c "/bin/echo 'user20 xxxx'| DISPLAY=:120  /usr/bin/skype --pipelogin &"

sleep 7

