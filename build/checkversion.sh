#!/bin/sh

if [ -f .noversion ] ; then
    exit
fi

if eval test x${1} = xforce ; then
	rm -f .version
fi

force=0
version=`svnversion . -n || echo hacked`
oldversion=`cat .version 2>/dev/null || echo "0"`
grep "@SVN_VERSION@" src/include/switch_version.h && force=1

if [ $oldversion != $version ] || [ $force = 1 ] ; then
    cat src/include/switch_version.h.in | sed "s/@SVN_VERSION@/$version/g" > src/include/switch_version.h
    echo $version > .version
fi
