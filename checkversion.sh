#!/bin/bash

if [ ! -z $1 ] ; then
    if [ $1 = force ] ; then
	rm -f .version
    fi
fi

version=`svnversion . -n || echo hacked`
oldversion=`cat .version 2>/dev/null || echo "0"`

if [ $oldversion != $version ] ; then
    cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/$version/g" > src/include/switch_version.h
    echo $version > .version
fi



