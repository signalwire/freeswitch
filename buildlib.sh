#!/bin/bash

root=$1
shift

if [ -f $root/.nodepends ] ; then
    echo "***depends disabled*** use make yesdepends to re-enable"
    exit 0
fi



install=
base=http://www.sofaswitch.com/mikej

if [ ! -z $1 ] && [ $1 = install ] ; then
    install=1
    shift
fi

tar=$1
shift

cd $root/libs/.
CFLAGS=
LDFLAGS=
MAKEFLAGS=

if [ -d $tar ] ; then
    uncompressed=$tar
    tar=
else
    uncompressed=`echo $tar | sed "s/\.tar\.gz//g"`
    if [ ! -f $tar ] ; then
	rm -fr $uncompressed
	wget $base/$tar
	if [ ! -f $tar ] ; then
	    echo cannot find $tar
	    exit
	fi
	tar -zxvf $tar
    fi
fi

if [ -f $uncompressed/.complete ] ; then
    echo $uncompressed already installed
    exit 0
fi

cd $uncompressed
make clean 2>&1
sh ./configure --prefix=/usr/local $@

if [ $? == 0 ] ; then
    make
else 
    echo ERROR
    exit 1
fi

if [ ! -z $install ] ; then
    make install
    ldconfig 2>&1
fi

if [ $? == 0 ] ; then
    touch .complete
else 
    echo ERROR
    exit 1
fi

exit 0
