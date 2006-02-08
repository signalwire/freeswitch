#!/bin/sh

if [ ! -z $1 ] ; then
    if [ $1 = force ] ; then
	rm -f .version
    fi
fi

force=0
version=`svnversion . -n || echo hacked`
oldversion=`cat .version 2>/dev/null || echo "0"`
grep "@SVN_VERSION@" src/include/switch_version.h && force=1

if [ $oldversion != $version ] || [ $force = 1 ] ; then
    cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/$version/g" > src/include/switch_version.h
    echo $version > .version
    make modclean
fi



