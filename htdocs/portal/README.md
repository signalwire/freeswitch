# The FreeSWITCH Portal Project

The FreeSWITCH Portal Project is designed to show an intuitive view of the FreeSWITCH internals.

It can be used by FreeSWITCH funs, administrators, developers etc.

It does not aims to replace GUIs such as fusionPBX or blue.box.

It would be very easy to use and super helpful for new FreeSWITCH users.

## Philosophy

To provide a GUI out of the box without depends on external resources like PHP or a webserver such as Apache or Nginx.

Mainly developed with static html and Javascripts, and perhaps some lua scripts can help do some more magic things later.

## Install

Assume you installed FreeSWITCH in the default place - /usr/local/freeswitch, you can do

    cd /usr/local/freeswitch/htdocs
    git clone https://github.com/seven1240/FreeSWITCH-Portal.git portal

In FreeSWITCH you need to

    load mod_xml_rpc

Open your browser (Only Chrome is tested) and go to

    http://localhost:8080/portal/index.html

If you it asking for username and password you can find them in /usr/local/freeswitch/conf/autoload_configs/xml\_rpc.conf.xml. For more information see <http://wiki.freeswitch.org/wiki/Mod_xml_rpc> .

## Todo

* Websocket: by add websocket support in FreeSWITCH we can see channel changes lively, I have some working code as a patch to mod\_event\_socket.

* Modify users: A raw idea to add a new user would be something like below and reloadxml.

    sed -e 's/1000/new-user/g' 1000.xml > new-user.xml

* Modify dialplan and/or other XMLs: possible to use some online XML editor and can save the XML with some lua or C code at the backend, although there are security concerns.

* Store information in DB: I guess the Dbh handle in lua should can do something like this.

* Web terminal: With terminal.js like things and websocket we can really build a web version of fs_cli

* rtmp web client support to make and receive calls

* WebRTC?

* Logging, Event Debugging or SIP tracing: Yeah, more magic

* i18n

## Security

The primary goal is to help new users learn and use FreeSWITCH. Please DON'T put this on your production server as I haven't think anything about security.

## Development

I started this project to learn how to use [bootstrap](twitter.github.com/bootstrap/index.html) and [ember.js](twitter.github.com/bootstrap/index.html), the latter said it is a framework for creating **ambitious** web applications.

Contributions and patches are welcome.
