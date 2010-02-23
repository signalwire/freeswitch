#!/bin/sh
#
# freeswitch.sh - startup script for freeswitch on FreeBSD
#
# This goes in /usr/local/etc/rc.d and gets run at boot-time.

case "$1" in

    start)
    if [ -x /usr/local/freeswitch/bin/freeswitch ] ; then
        echo -n " freeswitch"
        /usr/local/freeswitch/bin/freeswitch -nc &
    fi
    ;;

    stop)
    if [ -x /usr/local/freeswitch/bin/freeswitch ] ; then
        echo -n " freeswitch"
        /usr/local/freeswitch/bin/freeswitch -stop &
    fi
    ;;

    *)
    echo "usage: $0 { start | stop }" >&2
    exit 1
    ;;

esac 