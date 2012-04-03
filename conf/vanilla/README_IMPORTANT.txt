        -= PLEASE READ THIS BEFORE YOU PUT A FreeSWITCH BOX INTO PRODUCTION =-

This configuration, generally known as the "default configuration" for FreeSWITCH, is *NOT* designed to be put into a production environment without some important modifications. Please keep in mind that the default configuration is designed to demonstrate what FreeSWITCH *can* do, not what it *should* do in your specific scenario. 

*** SECURING YOUR SERVER ***

By default, FreeSWITCH starts up and does a NATPMP and UPnP request to your router. If your router supports either of these protocols then FreeSWITCH does two things:
#1 - It gets the external IP address, which it uses for SIP communications
#2 - It causes there to be a "pinhole" opened up in the router allowing inbound communications to your FreeSWITCH server

Please re-read #2. Now, please re-read #2 again. If you do not want a pinhole coming through your router then DO NOT USE the "auto-nat" tools. The way to disable the auto-nat (that is, UPnP/NATPMP) checking is to start FreeSWITCH with the "-nonat" flag. Easy enough.

If you are planning on putting a system into production then you had better pay attention to security in other areas as well. If you are behind a firewall then make sure your firewall is actually protecting you. If you have your server on a public-facing Internet connection then we recommend a few things:
#1 - Consider using iptables (Linux/Unix)
#2 - Consider using fail2ban (see http://wiki.freeswitch.org/wiki/Fail2ban)

*** SECURING YOUR USERS ***

By default, the static XML files have 20 "directory users" in conf/directory/10xx.xml, numbered 1000-1019. Also, the default dialplan has routing for calls to those same extension numbers. (NOTE: the directory and the dialplan are 100% separate concepts. Check out chapters 3-5 of the awesome FreeSWITCH book for details.) 

The default users all have *very* simple passwords for SIP credentials and voicemail. If you put those into a production system then you are either brave, ignorant, or stupid. Please don't be any of those three things! You have a few choices for handling your users:

#1 - Delete the static XML files and use mod_xml_curl for dynamic users from a back-end database
#2 - Manually edit the static XML user directory files and modify the passwords
#3 - Run the handy randomize-passwords.pl script found in scripts/perl/ subdirectory under the main FreeSWITCH source directory

*** GETTING HELP ***

FreeSWITCH has a thriving on-line community - we welcome you to join us!
IRC: #freeswitch on irc.freenode.net
Mailing List: freeswitch-users on lists.freeswitch.org

You can also get professional FreeSWITCH assistance by visiting http://www.freeswitchsolutions.com or sending an email to consulting@freeswitch.org.

Happy FreeSWITCHing!
