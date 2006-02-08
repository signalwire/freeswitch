#!/bin/sh

make=`which gmake 2>/dev/null || which make 2>/dev/null || make`
check=`($make --version 2>/dev/null | grep GNU) || echo bad`

if [ "$check" = "bad" ] ; then
    echo "Sorry, you need GNU make to build this application"
    exit 1
fi

if [ -z $1 ] ; then
    echo "Usage: $0 <build | configure> [--prefix=/prefix/dir>]"
    echo
    echo "The 'build' option will fully install the application to the given --prefix"
    echo "The 'configure' option will reconfigure and exit."
    echo
    echo "The default --prefix is /usr/local/freeswitch"
    exit 1
fi

if [ $1 = "configure" ] ; then
    shift
    ./configure $@
    exit 0
fi

if [ ! -f ./Makefile ] ; then
    ./configure $@
fi

$make installall

