# remember to add here the removing of all the installed snd-* modules, so you're sure only the snd-dummy driver will be around
rmmod snd_hda_intel

# you need three dummy soundcard for 20 Skype client instances, because each dummy soundcard can handle a max of 8 Skype instances
# the enable= module parameter tells how many cards to start. For each additional card, add a comma and a 1
# manually configure the first 8 Skype client instances to use the hw:Dummy_0, the next 8 instances to use hw:Dummy_1, etc for all three devices (Play, Capture, Ring)
modprobe snd-dummy enable=1,1,1
sleep 3

#start the fake X server on a given port
/usr/bin/Xvfb :101 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
sleep 3

# start a Skype client instance that will connect to the X server above, and will login to the Skype network using the "username password" you send to it on stdin. Here xxx would be the password and skypiax1 the username
echo "skypiax1 xxx"| DISPLAY=:101 /usr/bin/skype --pipelogin &

sleep 7
/usr/bin/Xvfb :102 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
sleep 3

echo "skypiax2 xxx"| DISPLAY=:102 /usr/bin/skype --pipelogin &

sleep 7

#################################################################
# Following X server Skype client instances are commented out
#################################################################
###/usr/bin/Xvfb :103 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax3 xxx"| DISPLAY=:103 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :104 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax4 xxx"| DISPLAY=:104 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :105 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax5 xxx"| DISPLAY=:105 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :106 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax6 xxx"| DISPLAY=:106 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :107 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax7 xxx"| DISPLAY=:107 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :108 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax8 xxx"| DISPLAY=:108 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :109 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax9 xxx"| DISPLAY=:109 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :110 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax10 xxx"| DISPLAY=:110 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :111 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax11 xxx"| DISPLAY=:111 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :112 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax12 xxx"| DISPLAY=:112 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :113 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax13 xxx"| DISPLAY=:113 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :114 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax14 xxx"| DISPLAY=:114 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :115 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax15 xxx"| DISPLAY=:115 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :116 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax16 xxx"| DISPLAY=:116 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :117 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax17 xxx"| DISPLAY=:117 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :118 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax18 xxx"| DISPLAY=:118 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :119 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax19 xxx"| DISPLAY=:119 /usr/bin/skype --pipelogin &
###
###sleep 7
###/usr/bin/Xvfb :120 -auth /usr/local/freeswitch/conf/autoload_configs/skypiax.X.conf &
###sleep 3
###
###echo "skypiax20 xxx"| DISPLAY=:120 /usr/bin/skype --pipelogin &
###
###sleep 7
###
